cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(osgMap VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    # MSVC: force UTF-8 for source and execution character set
    add_compile_options(/utf-8)
else()
    # GCC / Clang
    add_compile_options(
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )
endif()

# === Zmodyfikowana sekcja dla OpenSceneGraph ===
# Definiujemy zmienną dla ścieżki do OpenSceneGraph
set(OSG_ROOT_DIR "" CACHE PATH "Specify the root directory of your OpenSceneGraph installation")

# Normalizuj ścieżkę OSG_ROOT_DIR
if(NOT OSG_ROOT_DIR STREQUAL "")
    file(TO_CMAKE_PATH "${OSG_ROOT_DIR}" OSG_ROOT_DIR)
    message(STATUS "Using OSG_ROOT_DIR provided by user: ${OSG_ROOT_DIR}")
    # Dodaj OSG_ROOT_DIR do CMAKE_PREFIX_PATH
    list(APPEND CMAKE_PREFIX_PATH "${OSG_ROOT_DIR}")
    # Dodaj potencjalne podkatalogi dla bibliotek i nagłówków
    list(APPEND CMAKE_INCLUDE_PATH "${OSG_ROOT_DIR}/include")
    list(APPEND CMAKE_LIBRARY_PATH "${OSG_ROOT_DIR}/lib" "${OSG_ROOT_DIR}/lib64" "${OSG_ROOT_DIR}/bin")
    # Debug: wyświetl wyszukiwane ścieżki
    message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
    message(STATUS "CMAKE_INCLUDE_PATH: ${CMAKE_INCLUDE_PATH}")
    message(STATUS "CMAKE_LIBRARY_PATH: ${CMAKE_LIBRARY_PATH}")
endif()

# Sprawdź zmienną środowiskową OSG_DIR jako fallback
if(DEFINED ENV{OSG_DIR} AND OSG_ROOT_DIR STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{OSG_DIR}" OSG_DIR)
    message(STATUS "Using OSG_DIR environment variable: ${OSG_DIR}")
    list(APPEND CMAKE_PREFIX_PATH "${OSG_DIR}")
    list(APPEND CMAKE_INCLUDE_PATH "${OSG_DIR}/include")
    list(APPEND CMAKE_LIBRARY_PATH "${OSG_DIR}/lib" "${OSG_DIR}/lib64" "${OSG_DIR}/bin")
    message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
    message(STATUS "CMAKE_INCLUDE_PATH: ${CMAKE_INCLUDE_PATH}")
    message(STATUS "CMAKE_LIBRARY_PATH: ${CMAKE_LIBRARY_PATH}")
endif()

# Znajdź pakiet OpenSceneGraph z wymaganymi komponentami
find_package(OpenSceneGraph REQUIRED COMPONENTS OpenThreads osg osgDB osgGA osgUtil osgViewer osgSim osgText)

# Sprawdź, czy OpenSceneGraph zostało znalezione i wyświetl informację
if(OPENSCENEGRAPH_FOUND)
    message(STATUS "OpenSceneGraph found. Version: ${OPENSCENEGRAPH_VERSION}")
    message(STATUS "OpenSceneGraph include directories: ${OPENSCENEGRAPH_INCLUDE_DIR}")
    message(STATUS "OpenSceneGraph libraries: ${OPENSCENEGRAPH_LIBRARIES}")
else()
    message(FATAL_ERROR "OpenSceneGraph not found. Please verify that OSG_ROOT_DIR or OSG_DIR points to a valid OpenSceneGraph installation. Expected structure:\n"
                        "  - include/osg (headers like osg/Vec3)\n"
                        "  - lib or bin (libraries like osg.lib, OpenThreads.lib)\n"
                        "  - Optionally: lib/cmake/OpenSceneGraph/OpenSceneGraphConfig.cmake\n"
                        "Current OSG_ROOT_DIR: ${OSG_ROOT_DIR}")
endif()

# === Koniec zmodyfikowanej sekcji ===

add_subdirectory(src)
