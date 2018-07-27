# Test source files
file(GLOB SOURCES_SC2LADDERSERVER_INTEGRATION_TESTS "*.cpp" "*.h")

# copy test data/configs
configure_file(${CMAKE_CURRENT_LIST_DIR}/integration_test_configs/TestMatch_Bot1Eliminated/LadderBots.json
    ${EXECUTABLE_OUTPUT_PATH}/integration_test_configs/TestMatch_Bot1Eliminated/LadderBots.json COPYONLY)
configure_file(${CMAKE_CURRENT_LIST_DIR}/integration_test_configs/TestMatch_Bot1Eliminated/LadderManager.conf
    ${EXECUTABLE_OUTPUT_PATH}/integration_test_configs/TestMatch_Bot1Eliminated/LadderManager.conf COPYONLY)

# Include directories
include_directories(SYSTEM
    ${PROJECT_SOURCE_DIR}/tests/integration
    ${PROJECT_SOURCE_DIR}/src/sc2laddercore
    ${PROJECT_SOURCE_DIR}/s2client-api/include
    ${PROJECT_SOURCE_DIR}/s2client-api/contrib/protobuf/src
    ${PROJECT_BINARY_DIR}/s2client-api/generated
)

# Link directories
link_directories(${PROJECT_BINARY_DIR}/s2client-api/bin)

if (ENABLE_CURL AND WIN32) # Curl library for windows
	link_directories(${PROJECT_SOURCE_DIR}/curl/lib)
endif ()

# Create the executable.
add_executable(Sc2LadderIntegrationTests ${SOURCES_SC2LADDERSERVER_INTEGRATION_TESTS})
target_link_libraries(Sc2LadderIntegrationTests
    Sc2LadderCore
)

# Set working directory as the tests binary directory
set_target_properties(Sc2LadderIntegrationTests PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}")