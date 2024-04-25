#include "APIEnvir.h"
#include "ACAPinc.h"   // Also includes APIdefs.h
#include "APICommon.h"
#include "ResourceIds.hpp"
#include <fstream>
#include <APIdefs_Elements.h>
#include <APIdefs_Base.h>
#include <set>
#include <vector>
#include <string>
#include <iomanip>
#include "AutomaticAnnotation.hpp"

// Forward declaration of functions
static GSErrCode __ACENV_CALL MenuCommandHandler(const API_MenuParams* menuParams);
void ProcessBuildingElements();
void ReportElementProperties(const API_Guid& elementGuid, API_ElemTypeID elemType, std::ofstream& outFile);
void ReportDimensionElementProperties(const API_Guid& elementGuid, API_ElemTypeID elemType, std::ofstream& outFile);
void DeleteDimensionsAndAnnotations();
void Messagebox();
void OutputAdditionalInfo(std::ofstream& outFile);


// Unique IDs for the Add-On (change these to actual unique IDs)
static const GSResID AddOnInfoID = ID_MY_ADDON_INFO;
static const Int32 AddOnNameID = 1;
static const Int32 AddOnDescriptionID = 2;

static const short AddOnMenuID = ID_MY_ADDON;
static const Int32 AddOnCommandID = 1;

// Add a new menu item ID

static const Int32 ClearAnnotationsCommandID = 3; // Adjust the ID as needed

// Output file for element information
std::ofstream outFile;
std::map<API_Guid, std::set<API_Guid>> wallDoors;
std::map<API_Guid, API_Guid> doorToWallMap; // Global declaration
std::map<API_Guid, bool> wallHasDimElems;

// Check environment function
API_AddonType __ACDLL_CALL CheckEnvironment(API_EnvirParams* envir)
{
    // Set the Add-On's name and description
    RSGetIndString(&envir->addOnInfo.name, 32000, 1, ACAPI_GetOwnResModule());
    RSGetIndString(&envir->addOnInfo.description, 32000, 2, ACAPI_GetOwnResModule());

    return APIAddon_Preload;
}


// Register interface function
GSErrCode __ACENV_CALL	RegisterInterface(void)
{
    GSErrCode	err;

    //
    // Register menus
    //
    err = ACAPI_MenuItem_RegisterMenu(32500, 0, MenuCode_UserDef, MenuFlag_SeparatorBefore);
    err = ACAPI_MenuItem_RegisterMenu(32501, 0, MenuCode_UserDef, MenuFlag_Default);
    err = ACAPI_MenuItem_RegisterMenu(32502, 0, MenuCode_UserDef, MenuFlag_Default);
    err = ACAPI_MenuItem_RegisterMenu(32503, 0, MenuCode_UserDef, MenuFlag_Default);
    err = ACAPI_MenuItem_RegisterMenu(32504, 0, MenuCode_UserDef, MenuFlag_Default);

    return err;
}		/* RegisterInterface */



// Free data function
GSErrCode __ACENV_CALL FreeData(void)
{
    // Ensure any final data gets written out before closing the file.
    OutputAdditionalInfo(outFile);
    // Close the output file
    outFile.close();
    return NoError;
}

// Function to process building elements
void ProcessBuildingElements() {
    GS::Array<API_Guid> elementList;

    // Process dimension elements first to populate wallHasDimElems
    if (ACAPI_Element_GetElemList(API_DimensionID, &elementList) == NoError && !elementList.IsEmpty()) {
        for (const API_Guid& elementGuid : elementList) {
            ReportDimensionElementProperties(elementGuid, API_DimensionID, outFile);
        }
    }

    // Clear the list before processing other types
    elementList.Clear();

    // Now, process all other element types, ensuring walls are processed after dimension elements
    API_ElemTypeID elementTypes[] = { API_WallID, API_SlabID, API_ZoneID, API_DoorID };

    for (API_ElemTypeID elemType : elementTypes) {
        if (ACAPI_Element_GetElemList(elemType, &elementList) == NoError && !elementList.IsEmpty()) {
            for (const API_Guid& elementGuid : elementList) {
                ReportElementProperties(elementGuid, elemType, outFile);
            }
        }
        // Clear the list after each type to prepare for the next
        elementList.Clear();
    }
}
// Function to report properties of an element
struct ZoneStampInfo {
    std::string guid;
    API_Box3D boundingBox;
    std::string infoString = "Zone Stamp";
};

struct DoorLabelInfo {
    std::string guid;
    API_Box3D boundingBox;
    std::string infoString = "Door Label";
};

struct DimensionNoteInfo {
    std::string guid;
    API_Box3D noteBoundingBox;
    std::string noteText;
    double textLength;
    API_Coord position;
    int globalDimElemCount;
    std::string infoString = "Dim Note";
    // Add other fields as necessary
};

std::vector<ZoneStampInfo> zoneStampInfos;
std::vector<DoorLabelInfo> doorLabelInfos;
std::vector<DimensionNoteInfo> dimensionNoteInfos;

void ReportElementProperties(const API_Guid& elementGuid, API_ElemTypeID elemType, std::ofstream& outFile)
{
    char reportStr[1024];
    API_Element element;
    BNZeroMemory(&element, sizeof(API_Element));
    element.header.guid = elementGuid;

    // Retrieve the element
    if (ACAPI_Element_Get(&element) == NoError) {
        GS::UniString elemName;
        
        // Handle Zone type specifically
        if (elemType == API_ZoneID) {
            API_ZoneType& zone = element.zone;

            // Report Zone ID first
            char zoneIdStr[256];
            sprintf(zoneIdStr, "Element Type: Zone, GUID: %s, ", APIGuidToString(element.header.guid).ToCStr().Get());
            strcpy_s(reportStr, zoneIdStr); // Start with Zone ID

            // Add Zone Stamp GUID
            GS::Guid gsGuid = APIGuid2GSGuid(zone.stampGuid);
            std::string guidStr = (const char*)gsGuid.ToUniString().ToCStr().Get();
            char stampIdStr[256];
            sprintf(stampIdStr, "Zone Stamp GUID: %s, Position: (%.2f, %.2f)", guidStr.c_str(), zone.pos.x, zone.pos.y);
            strcat_s(reportStr, stampIdStr);

            // Room/Zone Name
            if (zone.roomName[0]) {
                strcat_s(reportStr, ", Room Name: ");
                strcat_s(reportStr, GS::UniString(zone.roomName).ToCStr());
            }

            // Room Number
            if (zone.roomNoStr[0]) {
                strcat_s(reportStr, ", Room Number: ");
                strcat_s(reportStr, GS::UniString(zone.roomNoStr).ToCStr());
            }

            // Room Height
            char roomHeightStr[64];
            sprintf(roomHeightStr, ", Room Height: %.2f", zone.roomHeight);
            strcat_s(reportStr, roomHeightStr);


            // Retrieve the bounding box for the zone stamp
            API_Element zoneStampElement;
            BNZeroMemory(&zoneStampElement, sizeof(API_Element));
            zoneStampElement.header.guid = zone.stampGuid;

            API_Box3D extent3D;
            if (ACAPI_Element_CalcBounds(&zoneStampElement.header, &extent3D) == NoError) {

                ZoneStampInfo info = { APIGuidToString(zone.stampGuid).ToCStr().Get(), extent3D };
                zoneStampInfos.push_back(info);

                // Append bounding box info for the zone stamp to your report
                char boundingBoxStr[512];
                sprintf(boundingBoxStr, ", Zone Stamp Bounding Box: [(%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f)]",
                    extent3D.xMin, extent3D.yMin, extent3D.zMin,
                    extent3D.xMax, extent3D.yMax, extent3D.zMax);
                strcat_s(reportStr, boundingBoxStr);


            }
            else {
                // Handle error in retrieving the bounding box
                strcat_s(reportStr, ", Zone Stamp Bounding Box: Not available");
            }
        
        }

        else if (elemType == API_DoorID) {
            // Handle Door elements
            API_DoorType& door = element.door;
            GS::Guid markGuid = APIGuid2GSGuid(door.openingBase.markGuid);
            std::string markGuidStr = (const char*)markGuid.ToUniString().ToCStr().Get();
            sprintf(reportStr, "Element Type: Door, GUID: %s, Width: %.2f , Height: %.2f ",
                APIGuidToString(elementGuid).ToCStr().Get(),
                door.openingBase.width,
                door.openingBase.height);

            // Check if Marker GUID should be included
            if (!markGuidStr.empty() && markGuidStr != "00000000-0000-0000-0000-000000000000") {
                // Append Marker GUID to the report string
                sprintf(reportStr + strlen(reportStr), ", Marker GUID: %s", markGuidStr.c_str());
            }
            // Retrieve connected labels for the door
            GS::Array<API_Guid> connectedLabels;
            if (ACAPI_Grouping_GetConnectedElements(elementGuid, API_LabelID, &connectedLabels) == NoError) {
                for (const API_Guid& labelGuid : connectedLabels) {
                    // Retrieve label element data
                    API_Element labelElement;
                    BNZeroMemory(&labelElement, sizeof(API_Element));
                    labelElement.header.guid = labelGuid;

                    if (ACAPI_Element_Get(&labelElement) == NoError) {
                        // Get bounding box for the label
                        API_Box3D boundingBox;
                        if (ACAPI_Element_CalcBounds(&labelElement.header, &boundingBox) == NoError) {
                            
                                
                            // Capturing door label info within the existing label processing loop
                            DoorLabelInfo info = { APIGuidToString(labelGuid).ToCStr().Get(), boundingBox };
                            doorLabelInfos.push_back(info);

                            // Append label GUID and bounding box to the door report
                            sprintf(reportStr + strlen(reportStr), ", Label GUID: %s, Label Bounding Box: [(%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f)]",
                                APIGuidToString(labelGuid).ToCStr().Get(),
                                boundingBox.xMin, boundingBox.yMin, boundingBox.zMin,
                                boundingBox.xMax, boundingBox.yMax, boundingBox.zMax);

                           

                        }
                        else {
                            // Handle error in retrieving the bounding box
                            sprintf(reportStr + strlen(reportStr), ", Label GUID: %s, Bounding Box: Not available",
                                APIGuidToString(labelGuid).ToCStr().Get());
                        }
                    }
                }
            }


            // Include the wall GUID if the door is embedded in a wall
            auto wallIt = doorToWallMap.find(elementGuid);
            if (wallIt != doorToWallMap.end()) {
                API_Guid wallGuid = wallIt->second;
                sprintf(reportStr + strlen(reportStr), ", Embedded in Wall GUID: %s", APIGuidToString(wallGuid).ToCStr().Get());
            }
            else {
                strcat(reportStr, ", Not embedded in any wall");
            }
        }
        


        else {
            // Handle other types (walls, slabs, etc.)
            if (ACAPI_Element_GetElemTypeName(elemType, elemName) == NoError) {
                sprintf(reportStr, "Element Type: %s, GUID: %s", (const char*)elemName.ToCStr(), APIGuidToString(elementGuid).ToCStr().Get());
            }
            else {
                sprintf(reportStr, "Element Type: %d, GUID: %s", elemType, APIGuidToString(elementGuid).ToCStr().Get());
            }
        }

        // Handling Wall elements (Check for any Embedded Doors)

        if (elemType == API_WallID) {
            API_WallType& wall = element.wall;

            // Calculate the length of the wall in the XY-plane
            double dx = wall.begC.x - wall.endC.x;
            double dy = wall.begC.y - wall.endC.y;
            double wallLength = sqrt(dx * dx + dy * dy);

            // Use the thickness at the beginning of the wall as the reported thickness
            double wallThickness = wall.thickness;

            // Wall height relative to its bottom
            double wallHeight = wall.height;

            // Append wall length, thickness, and height to the report string
            sprintf(reportStr + strlen(reportStr), ", Length: %.2f, Width: %.2f, Height: %.2f", wallLength, wallThickness, wallHeight);

        }



        if (elemType == API_WallID) {
            API_ElementMemo memo;
            BNZeroMemory(&memo, sizeof(API_ElementMemo));
            if (ACAPI_Element_GetMemo(elementGuid, &memo) == NoError) {
                std::string doorsStr;
                if (memo.wallDoors != nullptr) {
                    // Calculate the number of doors
                    GSSize doorCount = BMGetPtrSize(reinterpret_cast<GSPtr>(memo.wallDoors)) / sizeof(API_Guid);
                    for (GSSize i = 0; i < doorCount; i++) {
                        const API_Guid& doorGuid = memo.wallDoors[i];
                        doorToWallMap[doorGuid] = elementGuid; // Map each door to this wall
                        if (!doorsStr.empty()) doorsStr += ", ";
                        doorsStr += APIGuidToString(doorGuid).ToCStr().Get();
                    }
                }
                if (!doorsStr.empty()) {
                    sprintf(reportStr + strlen(reportStr), ", Embedded Door GUID: %s", doorsStr.c_str());
                }
                ACAPI_DisposeElemMemoHdls(&memo);
            }
        }

        // Retrieve the compound info string for the element
        GS::UniString infoString;
        if (ACAPI_Element_GetElementInfoString(&elementGuid, &infoString) == NoError) {
            std::string infoStr = (const char*)infoString.ToCStr().Get();
            // Append the info string to your report
            sprintf(reportStr + strlen(reportStr), ", Info String: %s", infoStr.c_str());
        }
        else {
            // Handle error in retrieving the info string
            strcat(reportStr, ", Info String: Not available");
        }

        // Retrieve the bounding box for the element
        API_Box3D extent3D;
        if (ACAPI_Element_CalcBounds(&element.header, &extent3D) == NoError) {
            // Get the name of the element type
            GS::UniString elemName;
            char elemTypeStr[64] = { 0 };
            if (ACAPI_Element_GetElemTypeName(elemType, elemName) == NoError) {
                sprintf(elemTypeStr, "%s", (const char*)elemName.ToCStr());
            }
            else {
                sprintf(elemTypeStr, ", Type: %d", elemType);
            }

            // Append element type and bounding box info to your report
            sprintf(reportStr + strlen(reportStr), ", %s Bounding Box: [(%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f)]",
                elemTypeStr, // Element type
                extent3D.xMin, extent3D.yMin, extent3D.zMin, // Bounding Box minimum coordinates
                extent3D.xMax, extent3D.yMax, extent3D.zMax); // Bounding Box maximum coordinates
        }
        else {
            // Handle error in retrieving the bounding box
            strcat(reportStr, ", Bounding Box: Not available");
        }

        // Check for attached label (Label classification)
       
        GS::Array<API_Guid> connectedLabels;
        API_ElementMemo memo{};
        int labelType = 0;
        // Check for dimension elements associated with walls
        if (elemType == API_WallID) {
            // Check global map filled in ReportDimensionElementProperties
            labelType = wallHasDimElems.find(elementGuid) != wallHasDimElems.end() ? 1 : 0;
        }
        else if (elemType == API_ZoneID) {
            // For zones, check if the stampGuid is not null to assign a label type
            labelType = (element.zone.stampGuid != APINULLGuid) ? 4 : 0;
        }
        else if (elemType == API_DoorID) {
            // For doors, first check if a marker is present
            API_Element doorElement;
            BNZeroMemory(&doorElement, sizeof(API_Element));
            doorElement.header.guid = elementGuid;

            if (ACAPI_Element_Get(&doorElement) == NoError) {
                API_DoorType& door = doorElement.door;
                if (door.openingBase.markGuid != APINULLGuid) {
                    // If door marker is present, assign label type 3
                    labelType = 3;
                }
                else {
                    // If no marker, check for connected labels and assign label type 2 if found
                    if (ACAPI_Grouping_GetConnectedElements(elementGuid, API_LabelID, &connectedLabels) == NoError) {
                        if (!connectedLabels.IsEmpty()) {
                            labelType = 2;
                        }
                    }
                }
            }
        }
        else {
            // For other element types, check for connected labels
            if (ACAPI_Grouping_GetConnectedElements(elementGuid, API_LabelID, &connectedLabels) == NoError) {
                if (!connectedLabels.IsEmpty()) {
                    labelType = 2; // Assign label type if labels are found
                }
            }
        }

        // Append label presence info to your report
        sprintf(reportStr + strlen(reportStr), ", Label Type: %d", labelType);


        outFile << reportStr << std::endl;
        // Clear element data
        BNZeroMemory(&element, sizeof(API_Element));
    }
}



// Function to clear all dimensions ,annotations,labels and zones
void DeleteDimensionsAndAnnotations() {
    API_ElemTypeID elementTypes[] = { API_DimensionID ,API_LabelID, API_ZoneID /*, other annotation types */ };

    for (API_ElemTypeID elemType : elementTypes) {
        GS::Array<API_Guid> elementList;

        // Get the list of elements of the specified type
        if (ACAPI_Element_GetElemList(elemType, &elementList) == NoError && !elementList.IsEmpty()) {
            // Delete all elements of the current type
            GSErrCode err = ACAPI_Element_Delete(elementList);
            if (err != NoError) {
                // Handle error (e.g., log it or display a message to the user)
            }
        }
    }
}

void ReportDimensionElementProperties(const API_Guid& elementGuid, API_ElemTypeID elemType, std::ofstream& outFile) {
    char reportStr[1024];
    API_Element element;
    BNZeroMemory(&element, sizeof(API_Element));
    element.header.guid = elementGuid;
    static Int32 globalDimElemCount = 0;
    static Int32 dimElementCount = 0; // Counter for dimension elements
    GSErrCode err = ACAPI_Element_Get(&element);
    if (err == NoError && elemType == API_DimensionID) {
        API_ElementMemo memo;
        BNZeroMemory(&memo, sizeof(API_ElementMemo));
        double totalLength = 0.0; // Variable to accumulate total length
        if (ACAPI_Element_GetMemo(element.header.guid, &memo) == NoError) {
            Int32 numDimElems = BMGetHandleSize((GSHandle)memo.dimElems) / sizeof(API_DimElem);
            for (Int32 i = 0; i < numDimElems; ++i, ++globalDimElemCount) {
                API_DimElem& dimElem = (*memo.dimElems)[i];
                API_Base base = reinterpret_cast<API_Base&>(dimElem.base);
                totalLength += dimElem.dimVal; // Accumulate the length
                // Extracting Dimension Text
                GS::UniString dimText = (dimElem.note.contentUStr != nullptr) ?
                    *(dimElem.note.contentUStr) :
                    GS::UniString(dimElem.note.content);
                // If the base element is a wall, record that it has associated dimension elements
                if (base.type == API_WallID) {
                    wallHasDimElems[base.guid] = true;
                }
                // Formatting the output string for each dimension element

                 // Assuming average character width is roughly 60% of the noteSize
                
                double textWidth = 0.5;
                double textHeight = 0.5;
                API_Coord textPos = dimElem.note.pos; // Assuming this gives the bottom left position of the note
                

                // Calculate bounding box without rotation for simplicity
                double textbbXMin = textPos.x;
                double textbbYMin = textPos.y;
                double textbbXMax = textPos.x + textWidth;
                double textbbYMax = textPos.y + textHeight;
                float  textbbZMin = 0.0f; // No conversion issue here, literal 0.0f is already float
                float  textbbZMax = 0.0f; // Same as abo
                // Assuming a negligible thickness for the dimension element to simulate a bounding box
                const float thickness = 0.01f; // Arbitrarily small value to simulate a bounding box

                // Calculate the bounding box coordinates based on the dimension point
                float bbXMin = static_cast<float>(dimElem.pos.x - thickness / 2); // Slightly reduce for visual representation
                float bbYMin = static_cast<float>(dimElem.pos.y - thickness / 2);
                float bbXMax = static_cast<float>(dimElem.pos.x + thickness / 2); // Slightly increase for visual representation
                float bbYMax = static_cast<float>(dimElem.pos.y + thickness / 2);

                // Since we are not considering Z coordinate, set a default value for ZMin and ZMax if needed
                float bbZMin = 0.0f; // No conversion issue here, literal 0.0f is already float
                float bbZMax = 0.0f; // Same as above

                API_Box3D noteBoundingBox;
                noteBoundingBox.xMin = textbbXMin;
                noteBoundingBox.yMin = textbbYMin;
                noteBoundingBox.zMin = textbbZMin;
                noteBoundingBox.xMax = textbbXMax;
                noteBoundingBox.yMax = textbbYMax;
                noteBoundingBox.zMax = textbbZMax;

                DimensionNoteInfo noteInfo = {
                    APIGuidToString(element.header.guid).ToCStr().Get(), // GUID of the dimension element
                    noteBoundingBox, // The calculated or defined bounding box for the note
                    dimText.ToCStr().Get(), // Converting UniString to C-style string
                    static_cast<int>(dimElem.dimVal), // Dimension value, cast to int if necessary
                    dimElem.note.pos, // Position of the note
                    globalDimElemCount
                };

                // Add the populated instance to the collection
                dimensionNoteInfos.push_back(noteInfo);

                // Now, format the output string to include both position and bounding box information
                sprintf(reportStr,
                    "Element Type: DimNode %d, GUID: %s, Associated Element GUID: %s, Text: %s, Length: %.2f, "
                    "DimNode %d Bounding Box: [(%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f)], Position: (%.2f, %.2f), "
                    "Info String: Dim Node %d",
                    globalDimElemCount,
                    APIGuidToString(element.header.guid).ToCStr().Get(),
                    APIGuidToString(base.guid).ToCStr().Get(),
                    dimText.ToCStr().Get(),
                    dimElem.dimVal,
                    globalDimElemCount, // Used again, make sure it's an int
                    bbXMin, bbYMin, bbZMin,
                    bbXMax, bbYMax, bbZMax,
                    dimElem.pos.x, dimElem.pos.y,
                    globalDimElemCount); // Make sure the final count matches the type expected by '%d'

               

                outFile << reportStr << std::endl;
            }

            // Retrieve and report the bounding box for the entire dimension element
            API_Box3D boundingBox;
            if (ACAPI_Element_CalcBounds(&element.header, &boundingBox) == NoError) {

                ++dimElementCount;
                sprintf(reportStr, "Element Type: Dimension, GUID: %s, Dimension Bounding Box: [(%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f)], Length: %.2f, Info String: Dim %d",
                    APIGuidToString(elementGuid).ToCStr().Get(),
                    boundingBox.xMin, boundingBox.yMin, boundingBox.zMin,
                    boundingBox.xMax, boundingBox.yMax, boundingBox.zMax,
                    totalLength,
                    dimElementCount); // Include total length here
            }
            else {
                // Handle error in retrieving the bounding box
                sprintf(reportStr, "Bounding Box for Dimension Element, GUID: %s: Not available",
                    APIGuidToString(elementGuid).ToCStr().Get());
            }

            outFile << reportStr << std::endl;
            ACAPI_DisposeElemMemoHdls(&memo);
        }
        else {
            strcat(reportStr, "Error retrieving element memo");
            outFile << reportStr << std::endl;
        }
    }
    else {
        strcat(reportStr, "Error or Unsupported Element Type");
        outFile << reportStr << std::endl;
    }
}

void OutputAdditionalInfo(std::ofstream& outFile) {
    // Output Zone Stamp Info
    for (const auto& info : zoneStampInfos) {
        outFile << "Element Type: Zone Stamp, GUID: " << info.guid
            << ", Zone Stamp Bounding Box: [(" << std::fixed << std::setprecision(2) << info.boundingBox.xMin << ", " << info.boundingBox.yMin << ", " << info.boundingBox.zMin
            << "), (" << info.boundingBox.xMax << ", " << info.boundingBox.yMax << ", " << info.boundingBox.zMax << ")]"
            << ", Info String: " << info.infoString
            << "\n";
    }

    // Output Door Label Info
    for (const auto& info : doorLabelInfos) {
        outFile << "Element Type: Door Label, GUID: " << info.guid
            << ", Label Bounding Box: [(" << std::fixed << std::setprecision(2) << info.boundingBox.xMin << ", " << info.boundingBox.yMin << ", " << info.boundingBox.zMin
            << "), (" << info.boundingBox.xMax << ", " << info.boundingBox.yMax << ", " << info.boundingBox.zMax << ")]"
            << ", Info String: " << info.infoString
            << "\n";
    }
    // Output Dimension note Info
    for (const auto& info : dimensionNoteInfos) {
        outFile << "Element Type: DimText " << info.globalDimElemCount << ", GUID: " << info.guid
            << ", DimText " << info.globalDimElemCount << " Bounding Box: [("
            << std::fixed << std::setprecision(2)
            << info.noteBoundingBox.xMin << ", " << info.noteBoundingBox.yMin << ", " << info.noteBoundingBox.zMin
            << "), ("
            << info.noteBoundingBox.xMax << ", " << info.noteBoundingBox.yMax << ", " << info.noteBoundingBox.zMax << ")]"
            << ", Text: " << info.noteText
            << ", Position: ("
            << info.position.x << ", " << info.position.y << ")"
            << ", Info String: Dim Text " << info.globalDimElemCount
            << std::endl;
    }
}


void Messagebox() {

    // Define your report string with the additional properties you want to report
    GS::UniString reportStr = "Grabbing elements like a magician pulling rabbits out of a hat!by Server";

    // Use ACAPI_WriteReport to write the report string to the Report window
    ACAPI_WriteReport(reportStr, true);
}



// Menu command handler function 

GSErrCode __ACENV_CALL ProcessBuildingElements(const API_MenuParams* menuParams)
{
    ACAPI_KeepInMemory(false);

    return ACAPI_CallUndoableCommand("Element Test API Function",
        [&]() -> GSErrCode {

            switch (menuParams->menuItemRef.itemIndex) {
            case 1:		ProcessBuildingElements();							break;
            
            default:
                break;
            }

            return NoError;
        });
}		/* ProcessBuildingElements */

// Menu command handler function 
GSErrCode __ACENV_CALL DeleteDimensionsAndAnnotations(const API_MenuParams* menuParams)
{
    ACAPI_KeepInMemory(false);

    return ACAPI_CallUndoableCommand("Element Test API Function",
        [&]() -> GSErrCode {

            switch (menuParams->menuItemRef.itemIndex) {
            case 1:		DeleteDimensionsAndAnnotations();  						break;

            default:
                break;
            }

            return NoError;
        });
}		/* ClearDimensionsAndAnnotations */

// Menu command handler function 
GSErrCode __ACENV_CALL Messagebox(const API_MenuParams* menuParams)
{
    ACAPI_KeepInMemory(false);

    return ACAPI_CallUndoableCommand("Element Test API Function",
        [&]() -> GSErrCode {

            switch (menuParams->menuItemRef.itemIndex) {
            case 1:		Messagebox(); 						break;

            default:
                break;
            }

            return NoError;
        });
}		/* Messagebox */



// Menu command handler function 
GSErrCode __ACENV_CALL AutomaticAnnotation(const API_MenuParams* menuParams)
{
    ACAPI_KeepInMemory(false);

    return ACAPI_CallUndoableCommand("Element Test API Function",
        [&]() -> GSErrCode {

            switch (menuParams->menuItemRef.itemIndex) {
            case 1:		AutomaticAnnotation();  						break;

            default:
                break;
            }

            return NoError;
        });
}


GSErrCode __ACENV_CALL	Initialize(void)
{
    GSErrCode err = NoError;

    //
    // Install menu handler callbacks
    //
    err = ACAPI_MenuItem_InstallMenuHandler(32500, ProcessBuildingElements);
    err = ACAPI_MenuItem_InstallMenuHandler(32501, DeleteDimensionsAndAnnotations);
    err = ACAPI_MenuItem_InstallMenuHandler(32503, AutomaticAnnotation);
    err = ACAPI_MenuItem_InstallMenuHandler(32504, Messagebox);

    // Open the output file for writing
    outFile.open("ElementInfo.txt");

    return err;
}		/* Initialize */

// Copyright statement :
// The code produced herein is part of the master thesis conducted at the Technical University of Munichand should be used with proper citation.
// All rights reserved.
// Happy coding!by Server Çeter