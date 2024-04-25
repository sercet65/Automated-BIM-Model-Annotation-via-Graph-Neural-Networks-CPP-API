#include "APIdefs_Elements.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "ACAPinc.h" // Ensure you include the correct headers for ArchiCAD API


static void	ReplaceEmptyTextWithPredefined(API_ElementMemo& memo)
{
    const char* predefinedContent = "Door";

    if (memo.textContent == nullptr || Strlen32(*memo.textContent) < 2) {
        BMhKill(&memo.textContent);
        memo.textContent = BMhAllClear(Strlen32(predefinedContent) + 1);
        strcpy(*memo.textContent, predefinedContent);
        (*memo.paragraphs)[0].run[0].range = Strlen32(predefinedContent);
    }
}


// Function to create dimensions around wall elements
void CreateDimensionForWalls(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (std::getline(iss, token, ',')) { // Change delimiter if necessary
        tokens.push_back(token);
    }

    // Adjusted for the additional tokens, including labelType and length
    if (tokens.size() < 21) {
        std::cerr << "Not enough tokens in line: " << line << std::endl;
        return;
    }

    try {
        double labelType = std::stod(tokens[23]); // Assuming 'labelType' is at index 8
        if (labelType != 1) {
            // If labelType is not 1, skip this wall
            return;
        }

        double bbXMin = std::stod(tokens[9]);
        double bbYMin = std::stod(tokens[10]);
        double bbXMax = std::stod(tokens[12]);
        double bbYMax = std::stod(tokens[13]);
        double Width = std::stod(tokens[4]);
        //double Length= std::stod(tokens[3]);

        bool horizontalWall = (bbYMax - bbYMin) < (bbXMax - bbXMin); // Check if the wall is horizontal

        API_Element element = {};
        API_ElementMemo memo = {};
        BNZeroMemory(&element, sizeof(API_Element));
        BNZeroMemory(&memo, sizeof(API_ElementMemo));
        element.header.type = API_DimensionID;
        GSErrCode err = ACAPI_Element_GetDefaults(&element, &memo);
        if (err != NoError) {
            std::cerr << "Error getting defaults: " << err << std::endl;
            ACAPI_DisposeElemMemoHdls(&memo);
            return;
        }

        element.dimension.textWay = horizontalWall ? APIDir_Horizontal : APIDir_Vertical; // Set the text way based on wall orientation
        element.dimension.dimAppear = APIApp_Normal;
        element.dimension.textPos = APIPos_Above;
        element.dimension.nDimElem = 2; // Only two points needed for linear dimension

        memo.dimElems = reinterpret_cast<API_DimElem**>(BMAllocateHandle(element.dimension.nDimElem * sizeof(API_DimElem), ALLOCATE_CLEAR, 0));
        if (memo.dimElems == nullptr || *memo.dimElems == nullptr) {
            ACAPI_DisposeElemMemoHdls(&memo);
            return;
        }

        if (horizontalWall) {
            // For horizontal walls, set reference point and direction accordingly
            element.dimension.refC.x = bbXMin;
            element.dimension.refC.y = bbYMin;

            // Adjust y-coordinate if bbYMin is 0 or negative
            if (bbYMin <= 0) {
                // Add a negative offset to the y-coordinate
                element.dimension.refC.y -= Width; // Adjust yOffset as needed
                element.dimension.textPos = APIPos_Below;
            }
            else {
                // Add a positive offset to the y-coordinate
                element.dimension.refC.y += Width + Width; // Adjust yOffset as needed
                element.dimension.textPos = APIPos_Above;
            }

            // Set direction and other properties as before
            element.dimension.direction.x = 1.0;
            element.dimension.direction.y = 0.0;  // Horizontal direction

            (*memo.dimElems)[0].base.loc.x = bbXMin;
            (*memo.dimElems)[0].base.loc.y = bbYMin;
            (*memo.dimElems)[1].base.loc.x = bbXMax;
            (*memo.dimElems)[1].base.loc.y = bbYMin; // Same Y-coordinate for horizontal wall
             // Adjusted text position


        }
        else {
            // For vertical walls, set reference point and direction accordingly
            element.dimension.refC.x = bbXMin;
            element.dimension.refC.y = bbYMin;

            // Adjust y-coordinate if bbYMin is 0 or negative
            if (bbXMin <= 0) {
                // Add a negative offset to the y-coordinate
                element.dimension.refC.x -= Width; // Adjust yOffset as needed
                element.dimension.textPos = APIPos_Above;
            }
            else {
                // Add a positive offset to the y-coordinate
                element.dimension.refC.x += Width + Width; // Adjust yOffset as needed
                element.dimension.textPos = APIPos_Below;
            }

            element.dimension.direction.x = 0.0; // Vertical direction
            element.dimension.direction.y = 1.0;

            (*memo.dimElems)[0].base.loc.x = bbXMin;
            (*memo.dimElems)[0].base.loc.y = bbYMin;
            (*memo.dimElems)[1].base.loc.x = bbXMin; // Same X-coordinate for vertical wall
            (*memo.dimElems)[1].base.loc.y = bbYMax;
            
        }

        err = ACAPI_Element_Create(&element, &memo);
        if (err != NoError) {
            std::cerr << "Error creating element: " << err << std::endl;
        }

        ACAPI_DisposeElemMemoHdls(&memo);
    }
    catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << " for line: " << line << std::endl;
    }
}



// Function to create labels for door elements
void CreateLabelForDoors(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (std::getline(iss, token, ',')) {
        tokens.push_back(token);
    }

    // Assuming 'labelType' is at index 8 for doors
    if (tokens.size() < 21) {
        std::cerr << "Not enough tokens in line: " << line << std::endl;
        return;
    }

    try {
        double labelType = std::stod(tokens[23]);
        if (labelType != 2) {
            // If labelType is not 2, skip this element as it's not a door
            return;
        }

        // Extract position for the label based on door position, adjust indices as necessary
        API_Coord c;
        c.x = std::stod(tokens[9]) + 0.5; // Assuming 'bb_xmin' is the reference point x
        c.y = std::stod(tokens[10]) - 0.25; // Assuming 'bb_ymin' is the reference point y

        API_Element element = {};
        API_ElementMemo memo = {};
        BNZeroMemory(&element, sizeof(API_Element));
        BNZeroMemory(&memo, sizeof(API_ElementMemo));
        element.header.type = API_LabelID;

        GSErrCode err = ACAPI_Element_GetDefaults(&element, &memo);
        if (err != NoError) {
            std::cerr << "Error getting defaults: " << err << std::endl;
            return;
        }

        element.label.parent = APINULLGuid;
        element.label.begC = c;
        element.label.midC.x = c.x; // You may want to adjust this based on your needs
        element.label.midC.y = c.y; // You may want to adjust this based on your needs
        element.label.endC.x = c.x; // You may want to adjust this based on your needs
        element.label.endC.y = c.y; // You may want to adjust this based on your needs


        // Set label text directly to "Door1"
        if (element.label.labelClass == APILblClass_Text) {
            ReplaceEmptyTextWithPredefined(memo);
            element.label.u.text.nonBreaking = true;
        }

        err = ACAPI_Element_Create(&element, &memo);
        if (err != NoError) {
            std::cerr << "Error creating label: " << err << std::endl;
        }

        ACAPI_DisposeElemMemoHdls(&memo);
    }
    catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << " for line: " << line << std::endl;
    }
}


void CreateDoorMarker(const API_Coord& position) {
    API_Element element = {};
    API_ElementMemo memo = {};
    API_SubElement marker = {};

    GSErrCode err = NoError;

    element.header.type = API_DetailID;
    marker.subType = (API_SubElementType)(APISubElement_MainMarker | APISubElement_NoParams);

    err = ACAPI_Element_GetDefaultsExt(&element, &memo, 1UL, &marker);
    if (err != NoError) {
        ACAPI_DisposeElemMemoHdls(&memo);
        ACAPI_DisposeElemMemoHdls(&marker.memo);
        return;
    }

    // Set up detail element
    element.detail.pos = position;
    element.detail.poly.nCoords = 5;
    element.detail.poly.nSubPolys = 1;
    element.detail.poly.nArcs = 0;
    memo.coords = (API_Coord**)BMAllocateHandle((element.detail.poly.nCoords + 1) * sizeof(API_Coord), ALLOCATE_CLEAR, 0);
    memo.pends = (Int32**)BMAllocateHandle((element.detail.poly.nSubPolys + 1) * sizeof(Int32), ALLOCATE_CLEAR, 0);
    if (memo.coords != nullptr && memo.pends != nullptr) {
        (*memo.coords)[1].x = position.x - 1.0;
        (*memo.coords)[1].y = position.y;
        (*memo.coords)[2].x = position.x;
        (*memo.coords)[2].y = position.y - 1.0;
        (*memo.coords)[3].x = position.x + 1.0;
        (*memo.coords)[3].y = position.y;
        (*memo.coords)[4].x = position.x;
        (*memo.coords)[4].y = position.y + 1.0;
        (*memo.coords)[5].x = (*memo.coords)[1].x;
        (*memo.coords)[5].y = (*memo.coords)[1].y;

        (*memo.pends)[0] = 0;
        (*memo.pends)[1] = element.detail.poly.nCoords;
    }

    // Set up door marker
    marker.subElem.object.pen = 3; // Example pen color, adjust as needed
    marker.subElem.object.useObjPens = true;
    marker.subElem.object.pos.x = position.x + 1.5; // Example offset from detail element, adjust as needed
    marker.subElem.object.pos.y = position.y + 1.0; // Example offset from detail element, adjust as needed

    // Create door marker
    marker.subType = APISubElement_MainMarker;

    // Create detail element and door marker
    err = ACAPI_Element_CreateExt(&element, &memo, 1UL, &marker);
    if (err != NoError)
        std::cerr << "Error creating detail and door marker: " << err << std::endl;

    ACAPI_DisposeElemMemoHdls(&memo);
    ACAPI_DisposeElemMemoHdls(&marker.memo);

    return;
}

GSErrCode CreateZone(const API_Coord& pos, const GS::UniString& roomName, const GS::UniString& roomNoStr, API_Guid* newZoneGuid) {
    API_Element element = {};
    API_ElementMemo memo = {};
    GSErrCode err;

    GS::Array<API_Guid> zoneList;
    ACAPI_Element_GetElemList(API_ZoneID, &zoneList);

    element.header.type = API_ZoneID;

    err = ACAPI_Element_GetDefaults(&element, &memo);
    if (err != NoError) {
        std::cerr << "Error getting defaults: " << err << std::endl;
        return err;
    }

    element.header.type = API_ZoneID;
    element.zone.catInd = ACAPI_CreateAttributeIndex(1);
    element.zone.manual = false;

    GS::snuprintf(element.zone.roomName, sizeof(element.zone.roomName), roomName.ToUStr());
    GS::snuprintf(element.zone.roomNoStr, sizeof(element.zone.roomNoStr), roomNoStr.ToUStr());

    element.zone.pos.x = pos.x;
    element.zone.pos.y = pos.y;
    element.zone.refPos.x = pos.x;
    element.zone.refPos.y = pos.y;

    err = ACAPI_Element_Create(&element, &memo);
    if (err != NoError) {
        std::cerr << "Error creating zone: " << err << std::endl;
        *newZoneGuid = APINULLGuid;
    }
    else {
        *newZoneGuid = element.header.guid;
    }

    ACAPI_DisposeElemMemoHdls(&memo);

    return err;
}

void CreateLabelForDoor(const std::vector<std::string>& tokens, const API_Guid& doorGuid) {
    GSErrCode err;
    API_Element element = {};
    API_ElementMemo memo = {};

    // Set up label element
    element.header.type = API_LabelID;
    element.label.parentType = API_ObjectID;

    // Get default properties for the label
    err = ACAPI_Element_GetDefaults(&element, &memo);

    if (err != NoError) {
        std::cerr << "Error getting defaults: " << err << std::endl;
        return;
    }

    // Extract position for the label based on door bounding box
    double bbXMin = std::stod(tokens[9]); // Assuming 'bb_xmin' is the reference point x
    double bbYMin = std::stod(tokens[10]); // Assuming 'bb_ymin' is the reference point y
    double bbXMax = std::stod(tokens[12]); // Assuming 'bb_xmax' is the maximum x
    
    double labelX = (bbXMin + bbXMax) / 2.0;
    double labelY = bbYMin - 0.25; // Adjust Y position as necessary

    // Set label position
    element.label.begC.x = labelX;
    element.label.begC.y = labelY;

    // Set midC and endC points
    element.label.midC = element.label.begC; // Just for example, you might adjust this based on your needs
    element.label.endC = element.label.begC; // Just for example, you might adjust this based on your needs

    // Set the parent of the label to the door
    element.label.parent = doorGuid;

    // If the label is of type text, replace empty text with predefined content
    if (element.label.labelClass == APILblClass_Text) {
        ReplaceEmptyTextWithPredefined(memo);
        element.label.u.text.nonBreaking = true;
    }

    // Set textWay to APIDir_Parallel for ensuring the label is parallel to the floor
    element.label.textWay = APIDir_Parallel;


    // Create the label element
    err = ACAPI_Element_Create(&element, &memo);
    if (err != NoError)
        std::cerr << "Error creating label: " << err << std::endl;

    ACAPI_DisposeElemMemoHdls(&memo);
}
// Main function to automatically annotate elements
void AutomaticAnnotation() {
    // Path to the source file
    std::string filePath = "C:\\API Development Kit 27.3001\\Server Add on\\Extraction_V2 c\\Extraction_V2\\build\\Debug\\elements_data_68.csv";

    // Open the source file
    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        std::cerr << "Failed to open source file." << std::endl;
        return;
    }

    // Skip the header line
    std::string line;
    std::getline(inFile, line);

    // Process each line to create labels for doors, dimensions for walls, and markers for windows
    while (std::getline(inFile, line)) {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (std::getline(iss, token, ',')) { // Assuming a comma is the delimiter
            tokens.push_back(token);
        }

        // Ensure the line has enough tokens to prevent out-of-range errors
        if (tokens.size() < 21) {
            std::cerr << "Not enough tokens in line: " << line << std::endl;
            continue;
        }

        // Determine labelType and call the appropriate function
        double labelType = std::stod(tokens[23]); // Make sure index matches labelType in CSV
        if (labelType == 1) {
            CreateDimensionForWalls(line);
        }
        else if (labelType == 2) {
            //CreateLabelForDoors(line);
           
            CreateDoorMarker({ std::stod(tokens[9]) + 0.5, std::stod(tokens[10]) - 0.5 });
            API_Guid doorGuid = APINULLGuid;
            if (!tokens[2].empty()) { // Ensure token 2 is not empty
                doorGuid = APIGuidFromString(tokens[2].c_str()); // Convert string to GUID
            }
            else {
                std::cerr << "Door GUID is empty for line: " << line << std::endl;
                continue;
            }
            CreateLabelForDoor(tokens, doorGuid);
        }
        else if (labelType == 4) {
            // Extract zone information from the line
            // Adjust indices as needed based on your CSV structure
            GS::UniString roomName = tokens[18].c_str();
            GS::UniString roomNoStr = tokens[19].c_str();
            API_Coord pos;
            pos.x = std::stod(tokens[16]) + 1.0; // Example coordinate from CSV
            pos.y = std::stod(tokens[17]) - 1.0; // Example coordinate from CSV

            // Call function to create zone
            API_Guid newZoneGuid = APINULLGuid;
            GSErrCode err = CreateZone(pos, roomName, roomNoStr, &newZoneGuid);
            if (err != NoError) {
                std::cerr << "Error creating zone: " << err << std::endl;
                // Handle error if necessary
            }
        }
    }

    // Close the source file
    inFile.close();
}



int main() {
    // Call the AutomaticAnnotation function
    AutomaticAnnotation();

    return 0;
}

// Copyright statement :
// The code produced herein is part of the master thesis conducted at the Technical University of Munichand should be used with proper citation.
// All rights reserved.
// Happy coding!by Server Çeter