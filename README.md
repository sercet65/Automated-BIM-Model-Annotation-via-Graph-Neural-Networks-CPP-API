
# Extraction_V2: Archicad Data Extraction Add-On

## Reminder: Organize hard-coded file paths before running to ensure all necessary files are generated 
## and to review the final results.

## Overview
`Extraction_V2` is a C++ add-on for Archicad designed to streamline the extraction of building element information, enhancing architectural data analysis and management.

## Features
- **Data Extraction**: Retrieves detailed information on building elements such as walls, doors, zones, and dimensions.
- **Custom Reports**: Generates comprehensive reports including GUIDs, dimensions, and specific properties of each element.
- **and more coming soon**: 

## Installation and Build Instructions
### Prerequisites
- [CMake 3.27.7](https://cmake.org/download/)
- [Visual Studio 2019](https://visualstudio.microsoft.com/vs/older-downloads/)
- [Archicad 27](https://www.graphisoft.com/downloads/)
- [Archicad 27 API Development Kit (version 27.3001)](https://www.graphisoft.com/downloads/addons/interoperability/api.html)

### Steps
1. Clone or download the source code from the GitHub repository.
2. Open the project in Visual Studio 2019.
3. Configure the project with CMake 3.27.7.
4. Build the project using Visual Studio's build tools.
5. Install the compiled add-on in Archicad 27.

## Usage
After installation, access the add-on functionalities in Archicad through custom menu items:
- **Extract BE**: Extracts data from building elements.
- **Delete ADZL**: Removes dimensions and annotations.
- **Automatic Annotation**: Removes dimensions and annotations.

!!!For Automatic annotation part make sure that the debug folder (or where you specify the location) includes related csv file with predicted label types.

## Key Libraries and Headers
The add-on leverages several key libraries and headers, including:
- `APIEnvir.h`, `ACAPinc.h`, `APICommon.h`: Essential headers for Archicad API development.
- `<fstream>`: Used for file handling operations (e.g., writing reports to files).
- `APIdefs_Elements.h`, `APIdefs_Base.h`: Define API structures and constants for handling Archicad elements.
- `<set>`: Standard C++ library for managing collections of unique elements.

## Components
- `Extraction_V2Fix.grc`: Registers the developer ID.
- `Extraction_V2.grc`: Registers menu items in Archicad.

## Code Structure
- `MenuCommandHandler`:  Handles menu commands.
- `ProcessBuildingElements`: Extracts properties from building elements.
- `ReportElementProperties`: Generates reports for each building element.
- `ClearDimensionsAndAnnotations`: Clears dimensions and annotations.
- `ReportDimensionElementProperties`: Reports on properties of dimension elements.
  
## Dependencies
- Archicad C++ API
- Standard C++ libraries


