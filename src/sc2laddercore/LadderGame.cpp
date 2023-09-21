#include "LadderGame.h"

#include <fstream>
#include <string>
#include <iostream>
#include <future>
#include <chrono>
#include <sstream>
#include <cctype>
#include <filesystem>

#include "sc2api/sc2_score.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2api/sc2_game_settings.h"
#include "sc2api/sc2_args.h"

#include "Types.h"
#include "Tools.h"
#include "Proxy.h"

static ResultType getEndResultFromProxyResults(const std::vector<ExitCase> &exitCases);

namespace fs = std::filesystem;


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

void LadderGame::LogStartGame(const std::vector<BotConfig>& Agents) {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);

    auto startTime = std::put_time(&tm, "%d-%m-%Y %H-%M-%S");

    for(const auto & Bot : Agents) {
        fs::path BotRoot(Bot.RootPath);
        BotRoot = BotRoot / "data" / ("stderr-" + Bot.PlayerId + ".log");

        std::ofstream outfile;

        outfile.open(BotRoot.string(), std::ios_base::app);
        outfile << std::endl << startTime << ": " << "Starting game vs "
                << Bot.BotName << std::endl;

        outfile.close();
    }
}

GameResult LadderGame::StartGame(const std::vector<BotConfig> &Agents, const std::string &Map) {
    if(Agents.empty()) {
        return {};
    }

    LogStartGame(Agents);

    // prepare races array
    std::vector<sc2::Race> races = std::vector<sc2::Race>();
    for (const auto & agent : Agents) {
        races.push_back(agent.Race);
    }

    sc2::ProcessSettings processSettings;
    sc2::GameSettings gameSettings;

    sc2::ParseSettings(CoordinatorArgc, CoordinatorArgv,
                       processSettings,
                       gameSettings);

    std::vector<Proxy*> proxies = std::vector<Proxy*>();

    for (const auto & Agent : Agents) {
        auto *proxyBot = new Proxy(MaxGameTime,
                                   MaxRealGameTime, Agent);
        proxies.push_back(proxyBot);
    }

    // start sc2 instances
    int agentIndex = 0;
    for (const auto proxy : proxies) {
        int portServer = PORT_START_BOT + agentIndex * 2;
        int portClient = PORT_START_BOT + agentIndex * 2 + 1;

        PrintThread{} << "Starting StarCraft II instance for bot "
                        << Agents[agentIndex].BotName << std::endl;
        proxy->startSC2Instance(processSettings, portServer, portClient);

        agentIndex++;
    }

    // connecting to sc2 instances
    agentIndex = 0;
    for (const auto proxy : proxies) {
        int portServer = PORT_START_BOT + agentIndex * 2;
        int portClient = PORT_START_BOT + agentIndex * 2 + 1;

        PrintThread{} << "Connecting " << Agents[agentIndex].PlayerId
                        << " bot to StarCraft II instance" << std::endl;

        const bool startSC2InstanceSuccessful = proxy->ConnectToSC2Instance(
                processSettings, portServer, portClient
        );

        if (!startSC2InstanceSuccessful) {
            PrintThread {} << "Failed to start the StarCraft II clients" << std::endl;
            return {};
        }

        agentIndex++;
    }

    // setup game
    agentIndex = 0;
    for (const auto proxy : proxies) {
        PrintThread{} << "Setup game for " << Agents[agentIndex].PlayerId
                        << " bot" << std::endl;
        const bool setupGameSuccessful = proxy->setupGame(processSettings, Map,
                                                          RealTime,
                                                          races);
        if (!setupGameSuccessful) {
            PrintThread {} << "Failed to create the game" << std::endl;
            return {};
        }

        agentIndex++;
    }

    agentIndex = 0;
    for (const auto proxy : proxies) {
        int portServer = PORT_START_BOT + agentIndex * 2;

        PrintThread{} << "Starting " << Agents[agentIndex].PlayerId
                        << " bot" << std::endl;
        const bool startBotSuccessful = proxy->startBot(portServer,
                                                        PORT_START,
                                                        Agents[agentIndex].PlayerId);
        if (!startBotSuccessful) {
            PrintThread {} << "Failed to start " << Agents[agentIndex].BotName
                            << std::endl;
            return {};
        }

        agentIndex++;
    }

    agentIndex = 0;
    for (const auto proxy : proxies) {
        PrintThread{} << "Starting " << Agents[agentIndex].PlayerId
                        << " bot game" << std::endl;
        proxy->startGame();

        agentIndex++;
    }

    // god, send us streams...
    bool gameFinished;
    do {
        gameFinished = proxies[0]->gameFinished();

        if (proxies.size() == 1) {
            continue;
        }

        for (int i = 1; i < proxies.size(); i++) {
            gameFinished = gameFinished && proxies[i]->gameFinished();
            if (!gameFinished) {
                break;
            }
        }

        if (!gameFinished) {
            sc2::SleepFor(1000);
        }
    } while (!gameFinished);

    std::string replayDir = Config->GetStringValue("LocalReplayDirectory");
    fs::path replay(replayDir);
    replay = replay / (RemoveMapExtension(Map) + ".SC2Replay");

    // save replay
    bool saveResult = false;
    for (const auto proxy : proxies) {
        saveResult = proxy->saveReplay(replay.string());

        if(saveResult) {
            break;
        }
    }

    if (!saveResult) {
        PrintThread{} << "Saving replay failed" << std::endl;
    }

    // save result
    GameResult Result;

    // todo unify structures - relying on order and different list sizes is not so good
    agentIndex = 0;
    std::vector<ExitCase> exitCases = std::vector<ExitCase>();
    std::vector<float> avgFrames = std::vector<float>();

    for (const auto & proxy : proxies) {
        auto result = proxy->getResult();
        exitCases.push_back(result);

        float avgFrame = proxy->stats().avgLoopDuration;
        avgFrames.push_back(avgFrame);

        if (result == ExitCase::GameEndVictory) {
            Result.Winner = Agents[agentIndex].BotName;
        }

        agentIndex++;
    }

    Result.AvgFrames = avgFrames;
    Result.Result = getEndResultFromProxyResults(exitCases);
    Result.GameLoop = proxies[0]->stats().gameLoops;

    std::time_t t = std::time(nullptr);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") << "UTC";
    Result.TimeStamp = oss.str();

    // delete proxy objects
    for (const auto proxy : proxies) {
        delete proxy;
    }

    return Result;
}

static ResultType getEndResultFromProxyResults(const std::vector<ExitCase> &exitCases) {
    for (const auto & exitCase : exitCases) {
        if (exitCase == ExitCase::BotCrashed ||
                exitCase == ExitCase::BotStepTimeout ||
                    exitCase == ExitCase::Error) {
            return ResultType::Error;
        }

        if (exitCase == ExitCase::GameTimeOver) {
            return ResultType::Timeout;
        }

        if (exitCase == ExitCase::GameEndVictory) {
            return ResultType::Win;
        }
    }

    return ResultType::Timeout;
}




