#include "APIEnvir.h"
#include "ACAPinc.h"   // Also includes APIdefs.h
#include "APICommon.h"
#include "ResourceIds.hpp"
#include <fstream>
#include <APIdefs_Elements.h>
#include <APIdefs_Base.h>
#include <set>



// Forward declaration of functions
static GSErrCode __ACENV_CALL MenuCommandHandler(const API_MenuParams* menuParams);
void ProcessBuildingElements();
void ReportElementProperties(const API_Guid& elementGuid, API_ElemTypeID elemType, std::ofstream& outFile);
void ReportDimensionElementProperties(const API_Guid& elementGuid, API_ElemTypeID elemType, std::ofstream& outFile);
void ClearDimensionsAndAnnotations();


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
    

    return err;
}		/* RegisterInterface */

// Initialize function

GSErrCode __ACENV_CALL Initialize(void)
{
    GSErrCode err = ACAPI_MenuItem_InstallMenuHandler(AddOnMenuID, MenuCommandHandler);

    // Open the output file for writing
    outFile.open("ElementInfo.txt");

    return err;
}

// Free data function
GSErrCode __ACENV_CALL FreeData(void)
{
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
            sprintf(zoneIdStr, "Zone ID: %s, ", APIGuidToString(element.header.guid).ToCStr().Get());
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
            // Retrieve the bounding box for the zone stamp
            API_Element zoneStampElement;
            BNZeroMemory(&zoneStampElement, sizeof(API_Element));
            zoneStampElement.header.guid = zone.stampGuid;

            API_Box3D extent3D;
            if (ACAPI_Element_CalcBounds(&zoneStampElement.header, &extent3D) == NoError) {
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
            // Access markGuid from openingBase
            GS::Guid markGuid = APIGuid2GSGuid(door.openingBase.markGuid);
            std::string markGuidStr = (const char*)markGuid.ToUniString().ToCStr().Get();
            sprintf(reportStr, "Door Element, GUID: %s, Marker GUID: %s", APIGuidToString(elementGuid).ToCStr().Get(), markGuidStr.c_str());
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
                        if (!doorsStr.empty()) doorsStr += ", ";
                        doorsStr += APIGuidToString(doorGuid).ToCStr().Get();
                    }
                }
                if (!doorsStr.empty()) {
                    sprintf(reportStr + strlen(reportStr), ", Embedded Doors: [%s]", doorsStr.c_str());
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
            // Append bounding box info to your report
            sprintf(reportStr + strlen(reportStr), ", Bounding Box: [(%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f)]",
                extent3D.xMin, extent3D.yMin, extent3D.zMin,
                extent3D.xMax, extent3D.yMax, extent3D.zMax);
        }
        else {
            // Handle error in retrieving the bounding box
            strcat(reportStr, ", Bounding Box: Not available");
        }



        outFile << reportStr << std::endl;
        // Clear element data
        BNZeroMemory(&element, sizeof(API_Element));
    }
}


// Function to clear all dimensions and annotations
void ClearDimensionsAndAnnotations() {
    API_ElemTypeID elementTypes[] = { API_DimensionID /*, other annotation types */ };

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
        API_DimensionType& dimension = element.dimension;

        // Format the report string with extracted properties from linear dimension
        sprintf(reportStr, "Linear Dimension, GUID: %s, Line Pen: %d, Text Position: %d",
            APIGuidToString(elementGuid).ToCStr().Get(),
            dimension.linPen,
            dimension.textPos);

        // Retrieve the bounding box for the element
        API_Box3D boundingBox;
        if (ACAPI_Element_CalcBounds(&element.header, &boundingBox) == NoError) {
            // Append bounding box info to your report
            sprintf(reportStr + strlen(reportStr), ", Bounding Box: [(%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f)]",
                boundingBox.xMin, boundingBox.yMin, boundingBox.zMin,
                boundingBox.xMax, boundingBox.yMax, boundingBox.zMax);
        }
        else {
            // Handle error in retrieving the bounding box
            strcat(reportStr, ", Bounding Box: Not available");
        }

        outFile << reportStr << std::endl;


        API_ElementMemo memo;
        BNZeroMemory(&memo, sizeof(API_ElementMemo));
        if (ACAPI_Element_GetMemo(element.header.guid, &memo) == NoError) {
            Int32 numDimElems = BMGetHandleSize((GSHandle)memo.dimElems) / sizeof(API_DimElem);
            for (Int32 i = 0; i < numDimElems; ++i) {
                API_DimElem& dimElem = (*memo.dimElems)[i];
                API_Base base = reinterpret_cast<API_Base&>(dimElem.base);

                // Output the GUID of the linear dimension element and the associated base element's GUID
                outFile << "Dimension element [" << i << "] GUID: " << APIGuidToString(elementGuid).ToCStr().Get()
                    << " is associated with Element GUID: " << APIGuidToString(base.guid).ToCStr().Get() << std::endl;
            }
            ACAPI_DisposeElemMemoHdls(&memo);
        }

    }
    else {
        strcat(reportStr, "Error or Unsupported Element Type");
        outFile << reportStr << std::endl;
    }
}



// Menu command handler function 
static GSErrCode __ACENV_CALL MenuCommandHandler(const API_MenuParams* menuParams) {
    if (menuParams->menuItemRef.menuResID == ID_MY_ADDON) {
        switch (menuParams->menuItemRef.itemIndex) {
        case 1:
            ProcessBuildingElements();
            break;
        case 2:
            ClearDimensionsAndAnnotations();  
            break;
            // ...
        }
    }
    return NoError;
}