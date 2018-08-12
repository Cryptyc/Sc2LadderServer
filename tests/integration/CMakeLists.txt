# Test source files
file(GLOB SOURCES_SC2LADDERSERVER_INTEGRATION_TESTS "*.cpp" "*.h")

# This variable is used in the test configs
if (WIN32)
	set(DEBUG_BOT_FILENAME "DebugBot.exe")
else ()
	set(DEBUG_BOT_FILENAME "DebugBot")
endif ()

# copy test data/configs and replace any variables they might contain
configure_file(${CMAKE_CURRENT_LIST_DIR}/integration_test_configs/TestMatch_Bot1Eliminated/LadderBots.json
    ${EXECUTABLE_OUTPUT_PATH}/integration_test_configs/TestMatch_Bot1Eliminated/LadderBots.json)
configure_file(${CMAKE_CURRENT_LIST_DIR}/integration_test_configs/TestMatch_Bot1Eliminated/LadderManager.json
    ${EXECUTABLE_OUTPUT_PATH}/integration_test_configs/TestMatch_Bot1Eliminated/LadderManager.json)

# Include directories
include_directories(SYSTEM
    ${PROJECT_SOURCE_DIR}/tests/integration
    ${PROJECT_SOURCE_DIR}/src/sc2laddercore
    ${PROJECT_SOURCE_DIR}/s2client-api/include
    ${PROJECT_SOURCE_DIR}/s2client-api/contrib/protobuf/src
    ${PROJECT_BINARY_DIR}/s2client-api/generated
    ${PROJECT_SOURCE_DIR}/rapidjson
)

# Link directories
link_directories(${PROJECT_BINARY_DIR}/s2client-api/bin)

# Create the executable.
add_executable(Sc2LadderIntegrationTests ${SOURCES_SC2LADDERSERVER_INTEGRATION_TESTS})
target_link_libraries(Sc2LadderIntegrationTests
    Sc2LadderCore
)

# Set working directory as the tests binary directory
set_target_properties(Sc2LadderIntegrationTests PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}")