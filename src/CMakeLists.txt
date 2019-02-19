# Source files
file(GLOB SOURCES_SC2LADDERSERVER "sc2ladderserver/*.cpp" "sc2ladderserver/*.h")
file(GLOB SOURCES_SC2LADDERCORE "sc2laddercore/*.cpp" "sc2laddercore/*.h")

# Include directories
include_directories(SYSTEM
    ${PROJECT_SOURCE_DIR}/src/sc2laddercore
    ${PROJECT_SOURCE_DIR}/s2client-api/include
    ${PROJECT_SOURCE_DIR}/s2client-api/contrib/protobuf/src
    ${PROJECT_BINARY_DIR}/s2client-api/generated
    ${PROJECT_SOURCE_DIR}/rapidjson
)

if (WIN32)
    include_directories(SYSTEM
        ${PROJECT_SOURCE_DIR}/s2client-api/src/sc2utils/
    )
endif ()

# Link directories
link_directories(${PROJECT_BINARY_DIR} ${PROJECT_BINARY_DIR}/s2client-api/bin)


# Create the executable.
add_executable(Sc2LadderServer ${SOURCES_SC2LADDERSERVER})
add_library(Sc2LadderCore ${SOURCES_SC2LADDERCORE})
target_link_libraries(Sc2LadderServer
    Sc2LadderCore
)
target_link_libraries(Sc2LadderCore
    sc2api sc2lib sc2utils sc2protocol civetweb libprotobuf
)


# Set working directory as the project root
set_target_properties(Sc2LadderServer PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")