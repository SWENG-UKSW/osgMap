project(osgMap)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define the executable target
add_executable(${PROJECT_NAME} map.cpp landuse.cpp water.cpp roads.cpp buildings.cpp camera_manip.cpp post_process.cpp labels.cpp HUD.cpp HUD.h)

# Link against OpenSceneGraph libraries
# Używamy zmiennej OPENSCENEGRAPH_LIBRARIES, która zawiera pełne ścieżki lub nazwy bibliotek z find_package
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OPENSCENEGRAPH_LIBRARIES}
)

# Set include directories for OpenSceneGraph headers
# Używamy OPENSCENEGRAPH_INCLUDE_DIR (standardowa zmienna z find_package)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${OPENSCENEGRAPH_INCLUDE_DIR}
)

# Opcjonalnie: Ustaw katalogi linkowania, jeśli biblioteki nie są znajdowane automatycznie
# (nie zawsze potrzebne, bo OPENSCENEGRAPH_LIBRARIES często zawiera pełne ścieżki)
link_directories(${OPENSCENEGRAPH_LIBRARY_DIRS})

# Optionally, if you have assets, you could add a custom command to copy them
# For this map, we'll just have a README in the assets folder.
