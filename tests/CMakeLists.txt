# Test source files
file(GLOB SOURCES_SC2LADDERSERVER_TESTS "tests/*.cpp" "tests/*.h")

# copy test data/configs
configure_file(${CMAKE_CURRENT_LIST_DIR}/tests/test_configs/TestMatch_Bot1Eliminated/LadderBots.json
    ${EXECUTABLE_OUTPUT_PATH}/test_configs/TestMatch_Bot1Eliminated/LadderBots.json COPYONLY)
configure_file(${CMAKE_CURRENT_LIST_DIR}/tests/test_configs/TestMatch_Bot1Eliminated/LadderManager.conf
    ${EXECUTABLE_OUTPUT_PATH}/test_configs/TestMatch_Bot1Eliminated/LadderManager.conf COPYONLY)

# Debug Bot source files
file(GLOB SOURCES_SC2LADDERSERVER_DEBUGBOT "debugbot/*.cpp" "debugbot/*.h")

# Include directories
include_directories(SYSTEM
    ${PROJECT_SOURCE_DIR}/tests/tests
    ${PROJECT_SOURCE_DIR}/tests/debugbot
    ${PROJECT_SOURCE_DIR}/src/sc2laddercore
    ${PROJECT_SOURCE_DIR}/s2client-api/include
    ${PROJECT_SOURCE_DIR}/s2client-api/contrib/protobuf/src
    ${PROJECT_BINARY_DIR}/s2client-api/generated
)

# Link directories
link_directories(${PROJECT_BINARY_DIR}/s2client-api/bin)

if (NOT DISABLE_CURL AND WIN32) # Curl library for windows
	link_directories(${PROJECT_SOURCE_DIR}/curl/lib)
endif ()

# Create the executables.
add_executable(Sc2LadderTests ${SOURCES_SC2LADDERSERVER_TESTS})
target_link_libraries(Sc2LadderTests
    Sc2LadderCore
)

add_executable(DebugBot ${SOURCES_SC2LADDERSERVER_DEBUGBOT})
target_link_libraries(DebugBot
    Sc2LadderCore
)

# Set working directory as the tests binary directory
set_target_properties(Sc2LadderTests PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}")