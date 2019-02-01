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

// If we mock the filesystem, we can move this into the unit tests
bool TestLadderConfig(int argc, char** argv) {

	// Use separate config instances to make sure we don't accidentally
	// succeed due to the values sticking around between file access.

	const char *configFile = "./integration_test_configs/TestConfig.json",
		*item1 = "Just a general string",
		*item2 = "./a/path\\like\\string.exe",
		*item3 = "http://127.0.0.1/web_address.php";

	// Just in case the config file already exists, we try delete it
	// otherwise it could cause a false positive
	remove(configFile);

	// Write out a config
	LadderConfig writeConfig(configFile);
	writeConfig.AddValue("Item1", item1);
	writeConfig.AddValue("Item2", item2);
	writeConfig.AddValue("Item3", item3);
	writeConfig.WriteConfig();

	// Check the config values come back as expected
	LadderConfig readConfig(configFile);
	readConfig.ParseConfig();
	return readConfig.GetStringValue("Item1") == item1
		&& readConfig.GetStringValue("Item2") == item2
		&& readConfig.GetStringValue("Item3") == item3;
}

bool TestMatch_Bot1Eliminated(int argc, char** argv) {
	try
	{
		// Write out the matchup file before launching LadderManager so we can dictate the bot load order
		std::ofstream myfile;
		myfile.open("./integration_test_configs/TestMatch_Bot1Eliminated/matchuplist");
		myfile << "\"DebugBot1\" \"DebugBot2\" InterloperLE.SC2Map";
		myfile.close();

		// Run LadderManager
		LadderManager LadderMan(argc, argv, "./integration_test_configs/TestMatch_Bot1Eliminated/LadderManager.json");
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
    std::cout << "Running integration test: " << #X << std::endl;   \
    if (X(argc, argv)) {                                            \
        std::cout << "Test: " << #X << " succeeded." << std::endl;  \
    }                                                               \
    else {                                                          \
        success = false;                                            \
        std::cerr << "Test: " << #X << " failed!" << std::endl;     \
    }

int main(int argc, char** argv) {
	bool success = true;

	TEST(TestLadderConfig);

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
		std::cout << "All integration tests succeeded!" << std::endl;
	else
		std::cerr << "Some integration tests failed!" << std::endl;

	return success ? 0 : -1;
}
