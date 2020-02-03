#include "LadderGame.h"

#include <sys/stat.h>
#include <fcntl.h>

#include <exception>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <future>
#include <chrono>
#include <sstream>
#include <cctype>

#include "sc2lib/sc2_lib.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_score.h"
#include "sc2api/sc2_map_info.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2api/sc2_game_settings.h"
#include "sc2api/sc2_proto_interface.h"
#include "sc2api/sc2_proto_to_pods.h"
#include "s2clientprotocol/sc2api.pb.h"
#include "sc2api/sc2_server.h"
#include "sc2api/sc2_connection.h"
#include "sc2api/sc2_args.h"
#include "sc2api/sc2_client.h"
#include "civetweb.h"

#include "Types.h"
#include "Tools.h"
#include "Proxy.h"


LadderGame::LadderGame(int InCoordinatorArgc, char** InCoordinatorArgv, LadderConfig *InConfig)
    : CoordinatorArgc(InCoordinatorArgc)
    , CoordinatorArgv(InCoordinatorArgv)
    , Config(InConfig)
{
    const int maxGameTimeInt = Config->GetIntValue("MaxGameTime");
    MaxGameTime = maxGameTimeInt > 0 ? static_cast<uint32_t>(maxGameTimeInt) : 0;
    const int MaxRealGameTimeInt = Config->GetIntValue("MaxRealGameTime");
    MaxRealGameTime = MaxRealGameTimeInt > 0 ? static_cast<uint32_t>(MaxRealGameTimeInt) : 0;
    RealTime = Config->GetBoolValue("RealTimeMode");
}

void LadderGame::LogStartGame(const BotConfig &Bot1, const BotConfig &Bot2)
{
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    // Why do we log this in stderr ?
    std::string Bot1Filename = Bot1.RootPath + "/data/stderr.log";
    std::string Bot2Filename = Bot2.RootPath + "/data/stderr.log";
    std::ofstream outfile;
    outfile.open(Bot1Filename, std::ios_base::app);
    outfile << std::endl << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") << ": " << "Starting game vs " << Bot2.BotName << std::endl;
    outfile.close();
    outfile.open(Bot2Filename, std::ios_base::app);
    outfile << std::endl << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") << ": " << "Starting game vs " << Bot1.BotName << std::endl;
    outfile.close();
}

GameResult LadderGame::StartGame(const BotConfig &Agent1, const BotConfig &Agent2, const std::string &Map)
{
    LogStartGame(Agent1, Agent2);
    // Proxy init
    Proxy proxyBot1(MaxGameTime, MaxRealGameTime, Agent1);
    Proxy proxyBot2(MaxGameTime, MaxRealGameTime, Agent2);

    // Start the SC2 instances
    sc2::ProcessSettings process_settings;
    sc2::GameSettings game_settings;
    sc2::ParseSettings(CoordinatorArgc, CoordinatorArgv, process_settings, game_settings);
    constexpr int portServerBot1 = 5677;
    constexpr int portServerBot2 = 5678;
    constexpr int portClientBot1 = 5679;
    constexpr int portClientBot2 = 5680;
    PrintThread {} << "Starting the StarCraft II clients." << std::endl;
    proxyBot1.startSC2Instance(process_settings, portServerBot1, portClientBot1);
    proxyBot2.startSC2Instance(process_settings, portServerBot2, portClientBot2);
    const bool startSC2InstanceSuccessful1 = proxyBot1.ConnectToSC2Instance(process_settings, portServerBot1, portClientBot1);
    const bool startSC2InstanceSuccessful2 = proxyBot2.ConnectToSC2Instance(process_settings, portServerBot2, portClientBot2);
    if (!startSC2InstanceSuccessful1 || !startSC2InstanceSuccessful2)
    {
        PrintThread {} << "Failed to start the StarCraft II clients." << std::endl;
        return GameResult();
    }
    // Setup map
    PrintThread {} << "Creating the game on " << Map << "." << std::endl;
    const bool setupGameSuccessful1 = proxyBot1.setupGame(process_settings, Map, RealTime, Agent1.Race, Agent2.Race);
    const bool setupGameSuccessful2 = proxyBot2.setupGame(process_settings, Map, RealTime, Agent1.Race, Agent2.Race);
    if (!setupGameSuccessful1 || !setupGameSuccessful2)
    {
        PrintThread {} << "Failed to create the game." << std::endl;
        return GameResult();
    }

    // Start the bots
    PrintThread {} << "Starting the bots " << Agent1.BotName << " and " << Agent2.BotName << "." << std::endl;
    const bool startBotSuccessful1 = proxyBot1.startBot(portServerBot1, PORT_START, Agent2.PlayerId);
    const bool startBotSuccessful2 = proxyBot2.startBot(portServerBot2, PORT_START, Agent1.PlayerId);
    if (!startBotSuccessful1)
    {
        PrintThread {} << "Failed to start " << Agent1.BotName << "." << std::endl;
    }
    if (!startBotSuccessful2)
    {
        PrintThread {} << "Failed to start " << Agent2.BotName << "." << std::endl;
    }
    if (!startBotSuccessful1 || !startBotSuccessful2)
    {
        return GameResult();
    }

    // Start the match
    PrintThread {} << "Starting the match." << std::endl;
    proxyBot1.startGame();
    proxyBot2.startGame();

    // Check from time to time if the match finished
    while (!proxyBot1.gameFinished() || !proxyBot2.gameFinished())
    {
        sc2::SleepFor(1000);
    }

    std::string replayDir = Config->GetStringValue("LocalReplayDirectory");
    if (replayDir.back() != '/')
    {
        replayDir += "/";
    }
    std::string replayFile = replayDir + Agent1.BotName + "v" + Agent2.BotName + "-" + RemoveMapExtension(Map) + ".SC2Replay";
    replayFile.erase(remove_if(replayFile.begin(), replayFile.end(), isspace), replayFile.end());
    if (!(proxyBot1.saveReplay(replayFile) || proxyBot2.saveReplay(replayFile)))
    {
        PrintThread{} << "Saving replay failed." << std::endl;
    }
    ChangeBotNames(replayFile, Agent1.BotName, Agent2.BotName);

    GameResult Result;
    const auto resultBot1 = proxyBot1.getResult();
    const auto resultBot2 = proxyBot2.getResult();


    Result.Result = getEndResultFromProxyResults(resultBot1, resultBot2);
    Result.Bot1AvgFrame = proxyBot1.stats().avgLoopDuration;
    Result.Bot2AvgFrame = proxyBot2.stats().avgLoopDuration;
    Result.GameLoop = proxyBot1.stats().gameLoops;

    std::time_t t = std::time(nullptr);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") <<"UTC";
    Result.TimeStamp = oss.str();
    return Result;
}


void LadderGame::ChangeBotNames(const std::string &ReplayFile, const std::string &Bot1Name, const std::string &Bot2Name)
{
    std::string CmdLine = Config->GetStringValue("ReplayBotRenameProgram");
    if (CmdLine.size() > 0)
    {
        CmdLine = CmdLine + " " + ReplayFile + " " + FIRST_PLAYER_NAME + " " + Bot1Name + " " + SECOND_PLAYER_NAME + " " + Bot2Name;
        StartExternalProcess(CmdLine);
    }
}
