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
    API_ElemTypeID elementTypes[] = { API_WallID, API_SlabID, API_ZoneID, API_DoorID, API_DimensionID };
    for (API_ElemTypeID elemType : elementTypes) {
        elementList.Clear();
        if (ACAPI_Element_GetElemList(elemType, &elementList) == NoError && !elementList.IsEmpty()) {
            for (const API_Guid& elementGuid : elementList) {
                if (elemType == API_DimensionID) {
                    ReportDimensionElementProperties(elementGuid, elemType, outFile);
                }
                else {
                    ReportElementProperties(elementGuid, elemType, outFile);
                }
            }
        }
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

std::vector<ZoneStampInfo> zoneStampInfos;
std::vector<DoorLabelInfo> doorLabelInfos;

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
            sprintf(reportStr, "Element Type: Door, GUID: %s, Marker GUID: %s", APIGuidToString(elementGuid).ToCStr().Get(), markGuidStr.c_str());

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
        bool hasLabel = false;
        GS::Array<API_Guid> connectedLabels;
        API_ElementMemo memo{};
        int labelType = 0;
        if (elemType == API_WallID) {
            API_ElementMemo memo{};
            GSErrCode err = ACAPI_Element_GetMemo(elementGuid, &memo); // Retrieve memo for this wall
            if (err == NoError) {
                if (memo.wallDoors != nullptr) {
                    GSSize doorCount = BMGetPtrSize(reinterpret_cast<GSPtr>(memo.wallDoors)) / sizeof(API_Guid);
                    for (GSSize i = 0; i < doorCount && !hasLabel; i++) {
                        const API_Guid& doorGuid = memo.wallDoors[i];

                        // Check for labels connected to each door
                        if (ACAPI_Grouping_GetConnectedElements(doorGuid, API_LabelID, &connectedLabels) == NoError) {
                            if (!connectedLabels.IsEmpty()) {
                                labelType = 2; // A label is found for a door in this wall
                            }
                        }
                    }
                }
                ACAPI_DisposeElemMemoHdls(&memo); // Dispose memo after use
            }
        }
        else if (elemType == API_ZoneID) {
            // For zones, check if the stampGuid is not null
            if (element.zone.stampGuid != APINULLGuid) labelType = 3;
        }
        else {
            // For other element types, check for connected labels
            if (ACAPI_Grouping_GetConnectedElements(elementGuid, API_LabelID, &connectedLabels) == NoError) {
                if (!connectedLabels.IsEmpty()) labelType = 2; // Label found for other element types
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

    GSErrCode err = ACAPI_Element_Get(&element);
    if (err == NoError && elemType == API_DimensionID) {
        API_ElementMemo memo;
        BNZeroMemory(&memo, sizeof(API_ElementMemo));
        double totalLength = 0.0; // Variable to accumulate total length
        if (ACAPI_Element_GetMemo(element.header.guid, &memo) == NoError) {
            Int32 numDimElems = BMGetHandleSize((GSHandle)memo.dimElems) / sizeof(API_DimElem);
            for (Int32 i = 0; i < numDimElems; ++i) {
                API_DimElem& dimElem = (*memo.dimElems)[i];
                API_Base base = reinterpret_cast<API_Base&>(dimElem.base);
                totalLength += dimElem.dimVal; // Accumulate the length
                // Extracting Dimension Text
                GS::UniString dimText = (dimElem.note.contentUStr != nullptr) ?
                    *(dimElem.note.contentUStr) :
                    GS::UniString(dimElem.note.content);

                // Formatting the output string for each dimension element

                 // Assuming a negligible thickness for the dimension element to simulate a bounding box
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

                // Now, format the output string to include both position and bounding box information
                sprintf(reportStr, "Element Type: Dimension [%d], GUID: %s, Associated Element GUID: %s, Text: %s, Length: %.2f mm, Dimension [%d] Bounding Box: [(%f, %f, %f), (%f, %f, %f)], Position: (%f, %f), Info String: Dim Node %d",
                    i,
                    APIGuidToString(element.header.guid).ToCStr().Get(),
                    APIGuidToString(base.guid).ToCStr().Get(),
                    dimText.ToCStr().Get(),
                    dimElem.dimVal,
                    i, // This might be duplicated or unnecessary if already specified at the start
                    bbXMin, bbYMin, bbZMin,
                    bbXMax, bbYMax, bbZMax,
                    dimElem.pos.x, dimElem.pos.y,
                    i); // Duplicated 'i' at the end might be unnecessary


                outFile << reportStr << std::endl;
            }

            // Retrieve and report the bounding box for the entire dimension element
            API_Box3D boundingBox;
            if (ACAPI_Element_CalcBounds(&element.header, &boundingBox) == NoError) {
                sprintf(reportStr, "Element Type: Dimension, GUID: %s, Dimension Bounding Box: [(%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f)], Total Length : % .2f mm, Info String: Dim",
                    APIGuidToString(elementGuid).ToCStr().Get(),
                    boundingBox.xMin, boundingBox.yMin, boundingBox.zMin,
                    boundingBox.xMax, boundingBox.yMax, boundingBox.zMax,
                    totalLength); // Include total length here
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
            << ", Zone Stamp Bounding Box: [(" << info.boundingBox.xMin << ", " << info.boundingBox.yMin << ", " << info.boundingBox.zMin
            << "), (" << info.boundingBox.xMax << ", " << info.boundingBox.yMax << ", " << info.boundingBox.zMax << ")]"
            << ", Info String: " << info.infoString // Separated info string
            << "\n";
    }

    // Output Door Label Info
    for (const auto& info : doorLabelInfos) {
        outFile << "Element Type: Door Label, GUID: " << info.guid
            << ", Label Bounding Box: [(" << info.boundingBox.xMin << ", " << info.boundingBox.yMin << ", " << info.boundingBox.zMin
            << "), (" << info.boundingBox.xMax << ", " << info.boundingBox.yMax << ", " << info.boundingBox.zMax << ")]"
            << ", Info String: " << info.infoString // Separated info string
            << "\n";
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

GSErrCode __ACENV_CALL	Initialize(void)
{
    GSErrCode err = NoError;

    //
    // Install menu handler callbacks
    //
    err = ACAPI_MenuItem_InstallMenuHandler(32500, ProcessBuildingElements);
    err = ACAPI_MenuItem_InstallMenuHandler(32501, DeleteDimensionsAndAnnotations);
    err = ACAPI_MenuItem_InstallMenuHandler(32504, Messagebox);

    // Open the output file for writing
    outFile.open("ElementInfo.txt");

    return err;
}		/* Initialize */

