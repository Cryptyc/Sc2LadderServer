# Source files
file(GLOB SOURCES_SC2LADDERSERVER "Sc2LadderServer/*.cpp" "Sc2LadderServer/*.h")
file(GLOB SOURCES_SC2LADDERCORE "core/*.cpp" "core/*.h")

# Include directories
include_directories(SYSTEM
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/s2client-api/include
    ${PROJECT_SOURCE_DIR}/s2client-api/contrib/protobuf/src
    ${PROJECT_BINARY_DIR}/s2client-api/generated
    ${PROJECT_SOURCE_DIR}/rapidjson
	if (WIN32) # Curl include for windows
		include_directories(SYSTEM
				${PROJECT_SOURCE_DIR}/curl/include/curl
		)
	endif ()
)

# Link directories
link_directories(${PROJECT_BINARY_DIR})
link_directories(${PROJECT_BINARY_DIR}/s2client-api/bin)

if (WIN32) # Curl library for windows
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
if (WIN32) # Platform specific curl library
	target_link_libraries(Sc2LadderCore libcurl.lib)
elseif (UNIX)
	target_link_libraries(Sc2LadderCore curl)
endif ()


if (WIN32) # Curl dlls for windows
	# todo: check for debug/release and copy only relevant DLL
	# todo: currently it copies both the debug and release DLLs regardless
	add_custom_command(TARGET Sc2LadderServer POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${PROJECT_SOURCE_DIR}/curl/bin/"
			$<TARGET_FILE_DIR:Sc2LadderServer>)
endif ()

# Set working directory as the project root
set_target_properties(Sc2LadderServer PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")