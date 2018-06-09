
// Tests.cpp
// These tests execute some basic scenarios that should be useful for regression testing.
// Run manually for now

#include "sc2lib/sc2_lib.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_score.h"
#include "sc2api/sc2_map_info.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2api/sc2_game_settings.h"
#include "sc2api/sc2_proto_interface.h"
#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_proto_to_pods.h"
#include "s2clientprotocol/sc2api.pb.h"
#include "sc2api/sc2_server.h"
#include "sc2api/sc2_connection.h"
#include "sc2api/sc2_args.h"
#include "sc2api/sc2_client.h"
#include "sc2api/sc2_proto_to_pods.h"
#include "sc2utils/sc2_arg_parser.h"
#include "civetweb.h"

#include <iostream>
#include <fstream>
#include <string>
#include <map>

#include "Types.h"
#include "LadderConfig.h"
#include "LadderManager.h"
#include "MatchupList.h"

#include "Tests.h"

bool TestMatch_Bot1Eliminated(int argc, char** argv) {
	try
	{
		// Write out the matchup file before launching LadderManager so we can dictate the bot load order
		std::ofstream myfile;
		myfile.open("./test_configs/TestMatch_Bot1Eliminated/matchuplist");
		myfile << "\"DebugBot1\" \"DebugBot2\" Ladder2017Season3/InterloperLE.SC2Map";
		myfile.close();

		// Run LadderManager
		LadderManager LadderMan(argc, argv, "./test_configs/TestMatch_Bot1Eliminated/LadderManager.conf");
		if (LadderMan.LoadSetup())
		{
			LadderMan.RunLadderManager();
		}
		return true;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception in TestMatch_Bot1Eliminated" << std::endl;
		std::cerr << e.what() << std::endl;
		return false;
	}
}

// Handy macro from: s2client-api/tests/all_tests.cc
#define TEST(X)                                                     \
    std::cout << "Running test: " << #X << std::endl;               \
    if (X(argc, argv)) {                                            \
        std::cout << "Test: " << #X << " succeeded." << std::endl;  \
    }                                                               \
    else {                                                          \
        success = false;                                            \
        std::cerr << "Test: " << #X << " failed!" << std::endl;     \
    }

int RunTests(int argc, char** argv) {
	bool success = true;

	TEST(TestMatch_Bot1Eliminated);
	//TEST(TestMatch_Bot2Eliminated);
	//TEST(sc2ai::TestMatch_Bot1Leave);
	//TEST(sc2ai::TestMatch_Bot2Leave);
	//TEST(sc2ai::TestMatch_Bot1Quit);
	//TEST(sc2ai::TestMatch_Bot2Quit);
	//TEST(sc2ai::TestMatch_BotVerseBlizzardAI);
	//TEST(sc2ai::TestMatch_BotBinaryVerseBotPython);
	// Add more tests here...

	if (success)
		std::cout << "All tests succeeded!" << std::endl;
	else
		std::cerr << "Some tests failed!" << std::endl;

	// Prevent the console from disappearing before it can be read.
	std::cout << "Hit any key to exit..." << std::endl;
	while (!sc2::PollKeyPress());

	return success ? 0 : -1;
}