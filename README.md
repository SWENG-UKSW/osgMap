**osgMap – Foundation Project for the Graphics Libraries Course**

This repository provides a skeleton application that all student groups will use as a common starting point in the Graphics Libraries course.
The goal of the course is to collaboratively build an interactive 3D map application using OpenSceneGraph (OSG) and OpenStreetMap (OSM) data.

Each group extends this foundation with its assigned feature set.
At the end of the semester, all contributions will form one complete, integrated application.

📌 Project Overview

The base application includes:

* minimal loading of OSM-derived data

* basic project structure and CMake configuration

* initial placeholder modules for extension

* example data directory (map_data)

Students are expected to expand this skeleton, implement required rendering techniques, and integrate their work through GitHub Pull Requests.

🧩 Workflow for All Task Groups

Each project group:

1. Forks this repository:
```
https://github.com/SWENG-UKSW/osgMap
```
2. Creates a working branch based on main.

3. Opens a draft Pull Request to SWENG-UKSW/osgMap -> main.

4. Works on assigned features.

5. When ready, changes PR status to Ready for review.

Test Data

* Main test data: available in map_data.zip

* Additional OSM extracts: [https://download.geofabrik.de](https://download.geofabrik.de)

* Building/label metadata: processed using scripts from this repo

* OSM processing can be assisted with QGIS (free)

🛠 Build Instructions

Install OpenSceneGraph (OSG).
After installation, set OSG_DIR if required.

Clone this repository:
```
git clone https://github.com/SWENG-UKSW/osgMap
```
Configure and build:
```
mkdir build
cd build
cmake ..
```
or
```
cmake -DOSG_ROOT_DIR=C:/path/to/OpenSceneGraph ..
```
▶️ Running the Application

Run the executable with a specified data directory:
```
./osgMap -path ./map_data
```
