cmake_minimum_required(VERSION 3.6)

project(Sc2LadderServer)

# Use bin as the directory for all executables.
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# Windows builds subdirectories Debug/Release.
# These variables will overwrite that and put binaries in bin.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR}/bin)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR}/bin)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_BINARY_DIR}/bin)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_BINARY_DIR}/bin)

# Build with c++14 support, required by sc2api.
set(CMAKE_CXX_STANDARD 14)

# Allow creating filters for projects in visual studio.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Disable building of examples in the sc2api submodule.
set(BUILD_API_EXAMPLES OFF CACHE INTERNAL "" FORCE)

# Disable building of tests in the sc2api submodule.
set(BUILD_API_TESTS OFF CACHE INTERNAL "" FORCE)

add_subdirectory("cpp-sc2")
add_subdirectory("src")
add_subdirectory("tests")

# set Sc2LadderServer as startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Sc2LadderServer)

# set folders/subfolders
# there might be a better way to do this than
# manually setting all these, but it works...
set_target_properties(sc2api PROPERTIES FOLDER sc2api)
set_target_properties(sc2lib PROPERTIES FOLDER sc2api)
set_target_properties(sc2protocol PROPERTIES FOLDER sc2api)
set_target_properties(sc2utils PROPERTIES FOLDER sc2api)

set_target_properties(civetweb-c-library PROPERTIES FOLDER sc2api/contrib)
set_target_properties(libprotobuf PROPERTIES FOLDER sc2api/contrib)
set_target_properties(libprotobuf-lite PROPERTIES FOLDER sc2api/contrib)
set_target_properties(libprotoc PROPERTIES FOLDER sc2api/contrib)
set_target_properties(protoc PROPERTIES FOLDER sc2api/contrib)

# SC2 API doesn't build SDL and ipv6 libs on OS X.
if (NOT APPLE)
    set_target_properties(sc2renderer PROPERTIES FOLDER sc2api/utilities)
    set_target_properties(ipv6-parse PROPERTIES FOLDER sc2api/contrib)
    set_target_properties(SDL2main PROPERTIES FOLDER sc2api/contrib)
    set_target_properties(SDL2-static PROPERTIES FOLDER sc2api/contrib)
endif ()
