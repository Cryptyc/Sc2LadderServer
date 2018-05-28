# Source files
file(GLOB_RECURSE SOURCES_SC2LADDERSERVER "*.cpp" "*.h")

# Include directories
include_directories(SYSTEM
    ${PROJECT_SOURCE_DIR}/s2client-api/include
    ${PROJECT_SOURCE_DIR}/s2client-api/contrib/protobuf/src
    ${PROJECT_BINARY_DIR}/s2client-api/generated
)

# Link directories
link_directories(${PROJECT_BINARY_DIR}/s2client-api/bin)

# Create the executable.
add_executable(Sc2LadderServer ${SOURCES_SC2LADDERSERVER})
target_link_libraries(Sc2LadderServer
    sc2api sc2lib sc2utils sc2protocol civetweb libprotobuf
)
