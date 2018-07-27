# Test source files
file(GLOB SOURCES_SC2LADDERSERVER_UNIT_TESTS "*.cpp" "*.h")

# Include directories
include_directories(SYSTEM
        ${PROJECT_SOURCE_DIR}/tests/unit
        ${PROJECT_SOURCE_DIR}/src/sc2laddercore
        ${PROJECT_SOURCE_DIR}/s2client-api/include
        ${PROJECT_SOURCE_DIR}/s2client-api/contrib/protobuf/src
        ${PROJECT_BINARY_DIR}/s2client-api/generated
        )

# Link directories
link_directories(${PROJECT_BINARY_DIR}/s2client-api/bin)

# Create the executable.
add_executable(Sc2LadderUnitTests ${SOURCES_SC2LADDERSERVER_UNIT_TESTS})
target_link_libraries(Sc2LadderUnitTests
        Sc2LadderCore
        )

# Set working directory as the tests binary directory
set_target_properties(Sc2LadderUnitTests PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}")