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

# Link directories
link_directories(${PROJECT_BINARY_DIR} ${PROJECT_BINARY_DIR}/s2client-api/bin)

if(ENABLE_CURL AND WIN32) # Curl library for windows
    include_directories(SYSTEM
            ${PROJECT_SOURCE_DIR}/curl/include/curl
    )
    link_directories(${PROJECT_SOURCE_DIR}/curl/lib)
endif ()

# Create the executable.
add_executable(Sc2LadderServer ${SOURCES_SC2LADDERSERVER})
add_library(Sc2LadderCore ${SOURCES_SC2LADDERCORE})
target_link_libraries(Sc2LadderServer
    Sc2LadderCore
)
target_link_libraries(Sc2LadderCore
    sc2api sc2lib sc2utils sc2protocol civetweb libprotobuf
)

if(ENABLE_CURL)
    if (WIN32) # Platform specific curl library
        target_link_libraries(Sc2LadderCore libcurl.lib)
        # todo: check for debug/release and copy only relevant DLL
        # todo: currently it copies both the debug and release DLLs regardless
        add_custom_command(TARGET Sc2LadderServer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PROJECT_SOURCE_DIR}/curl/bin/"
                $<TARGET_FILE_DIR:Sc2LadderServer>)
    elseif (UNIX)
        target_link_libraries(Sc2LadderCore curl)
    endif ()
endif ()

# Set working directory as the project root
set_target_properties(Sc2LadderServer PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")