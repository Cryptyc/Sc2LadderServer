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
#include "civetweb.h"
#include <exception>

#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <future>
#include <chrono>
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>
#include <cctype>

#include "Types.h"
#include "Tools.h"
#include "LadderGame.h"


bool ProcessResponse(const SC2APIProtocol::ResponseCreateGame& response)
{
    bool success = true;
    if (response.has_error()) {
        std::string errorCode = "Unknown";
        switch (response.error()) {
        case SC2APIProtocol::ResponseCreateGame::MissingMap: {
            errorCode = "Missing Map";
            break;
        }
        case SC2APIProtocol::ResponseCreateGame::InvalidMapPath: {
            errorCode = "Invalid Map Path";
            break;
        }
        case SC2APIProtocol::ResponseCreateGame::InvalidMapData: {
            errorCode = "Invalid Map Data";
            break;
        }
        case SC2APIProtocol::ResponseCreateGame::InvalidMapName: {
            errorCode = "Invalid Map Name";
            break;
        }
        case SC2APIProtocol::ResponseCreateGame::InvalidMapHandle: {
            errorCode = "Invalid Map Handle";
            break;
        }
        case SC2APIProtocol::ResponseCreateGame::MissingPlayerSetup: {
            errorCode = "Missing Player Setup";
            break;
        }
        case SC2APIProtocol::ResponseCreateGame::InvalidPlayerSetup: {
            errorCode = "Invalid Player Setup";
            break;
        }
        default: {
            break;
        }
        }

        std::cerr << "CreateGame request returned an error code: " << errorCode << std::endl;
        success = false;
    }

    if (response.has_error_details() && response.error_details().length() > 0) {
        std::cerr << "CreateGame request returned error details: " << response.error_details() << std::endl;
        success = false;
    }
    return success;

}

float_t CalculateAverage(float_t OriginalValue, long NewValue, int32_t NumValues)
{
    if (NumValues == 0 || OriginalValue == 0)
    {
        return (float_t)NewValue;
    }
    return ((OriginalValue * NumValues) + NewValue) / (1 + NumValues);
}


std::mutex m;
bool gameEnded = false;

void setGameEnded(const bool status)
{
    std::lock_guard<std::mutex> lock(m);
    gameEnded = status;
}

bool getGameEnded()
{
    std::lock_guard<std::mutex> lock(m);
    return gameEnded;
}

ExitCase GameUpdate(sc2::Connection *client, sc2::Server *server, const std::string *botName, uint32_t MaxGameTime, uint32_t MaxRealGameTime, float_t *AvgFrame, int32_t *GameLoop)
{
    ExitCase CurrentExitCase = ExitCase::InProgress;
    PrintThread{} << "Starting proxy for " << *botName << std::endl;
    setGameEnded(false);
    clock_t LastRequest = clock();
    clock_t FirstRequest = clock();
    clock_t StepTime = 0;
    uint32_t currentGameLoop = 0;
    float_t totalTime = 0;
    float_t AvgStepTime = 0;
    std::map<SC2APIProtocol::Status, std::string> status;
    status[SC2APIProtocol::Status::launched] = "launched";
    status[SC2APIProtocol::Status::init_game] = "init_game";
    status[SC2APIProtocol::Status::in_game] = "in_game";
    status[SC2APIProtocol::Status::in_replay] = "in_replay";
    status[SC2APIProtocol::Status::ended] = "ended";
    status[SC2APIProtocol::Status::quit] = "quit";
    status[SC2APIProtocol::Status::unknown] = "unknown";
    SC2APIProtocol::Status OldStatus = SC2APIProtocol::Status::unknown;
    try
    {
        bool AlreadyWarned = false;
        while (CurrentExitCase == ExitCase::InProgress && !getGameEnded()) {
            SC2APIProtocol::Status CurrentStatus;
            if (!client || !server || client->connection_ == nullptr)
            {
                PrintThread{} << botName << " Null server or client returning ClientTimeout" << std::endl;
                return ExitCase::ClientTimeout;
            }
            if (server->HasRequest())
            {
                const sc2::RequestData request = server->PeekRequest();
                if (request.second)
                {
                    if (request.second->has_quit()) //Really paranoid here...
                    {
                        // Intercept leave game and quit requests, we want to keep game alive to save replays
                        CurrentExitCase = ExitCase::ClientRequestExit;
                        break;
                    }
                    else if (request.second->has_debug() && !AlreadyWarned)
                    {
                        PrintThread{} << *botName << " IS USING DEBUG INTERFACE.  POSSIBLE CHEAT Please tell them not to" << std::endl;
                        AlreadyWarned = true;
                    }
                    else if (StepTime > 0 && request.second->has_step())
                    {
                        clock_t ThisStepTime = clock() - StepTime;
                        totalTime += static_cast<float_t>(ThisStepTime);
                        AvgStepTime = totalTime / static_cast<float_t>(currentGameLoop);
                    }
                }
                if (client->connection_ != nullptr)
                {
                    server->SendRequest(client->connection_);

                }

                // Block for sc2's response then queue it.
                SC2APIProtocol::Response* response = nullptr;
                client->Receive(response, 100000);
                if (response != nullptr)
                {
                    CurrentStatus = response->status();
                    if (OldStatus != CurrentStatus)
                    {
                        PrintThread{} << "New status of " << *botName << ": " << status.at(CurrentStatus) << std::endl;
                        OldStatus = CurrentStatus;
                    }
                    if (CurrentStatus > SC2APIProtocol::Status::in_replay)
                    {
                        CurrentExitCase = ExitCase::GameEnd;

                    }
                    if (response->has_observation())
                    {
                        const SC2APIProtocol::ResponseObservation LastObservation = response->observation();
                        const SC2APIProtocol::Observation& ActualObservation = LastObservation.observation();
                        currentGameLoop = ActualObservation.game_loop();
                        if (currentGameLoop > MaxGameTime)
                        {
                            CurrentExitCase = ExitCase::GameTimeout;
                        }
                        if (GameLoop != nullptr)
                        {
                            *GameLoop = currentGameLoop;
                        }
                    }
                    else if (response->has_step())
                    {
                        StepTime = clock();
                    }
                    if (MaxRealGameTime > 0)
                    {
                        if (clock() > (FirstRequest + (MaxRealGameTime * CLOCKS_PER_SEC)))
                        {
                            CurrentExitCase = ExitCase::GameTimeout;
                        }
                    }

                }

                // Send the response back to the client.
                if (server->connections_.size() > 0 && client->connection_ != NULL)
                {
                    server->QueueResponse(client->connection_, response);
                    server->SendResponse();
                }
                else
                {
                    CurrentExitCase = ExitCase::ClientTimeout;
                }
                LastRequest = clock();

            }
            else
            {
                if ((LastRequest + (50 * CLOCKS_PER_SEC)) < clock())
                {
                    PrintThread{} << "Client timeout (" << *botName << ")" << std::endl;
                    CurrentExitCase = ExitCase::ClientTimeout;
                }
            }
        }
        *AvgFrame = AvgStepTime;
        PrintThread{} << *botName << " Exiting with " << GetExitCaseString(CurrentExitCase) << " Average step time " << AvgStepTime << ", total time: " << totalTime << ", game loops: " << currentGameLoop << std::endl;
        setGameEnded(true);
        return CurrentExitCase;
    }
    catch (const std::exception& e)
    {
        PrintThread{} << e.what() << std::endl;
        return ExitCase::ClientTimeout;
    }
}

ExitCase OnEnd(sc2::Connection *client, sc2::Server *server, const std::string &botName)
{
    ExitCase CurrentExitCase = ExitCase::InProgress;
    PrintThread{} << "Processing last requests/responses for " << botName << std::endl;
    std::time_t LastRequest = std::time(nullptr);
    try
    {
        while (CurrentExitCase == ExitCase::InProgress)
        {
            if (!client || !server)
            {
                PrintThread{} << botName << " Null server or client returning ClientTimeout" << std::endl;
                return ExitCase::ClientTimeout;
            }
            if (client->connection_ == nullptr)
            {
                PrintThread{} << "Client disconnected (" << botName << ")" << std::endl;
                CurrentExitCase = ExitCase::ClientTimeout;
            }
            if (server->HasRequest())
            {
                if (client->connection_ != nullptr)
                {
                    PrintThread{} << "Sending request of " << botName << std::endl;
                    server->SendRequest(client->connection_);
                    LastRequest = std::time(nullptr);
                }
            }
            SC2APIProtocol::Response* response = nullptr;
            if (client->Receive(response, 1000)) // why is server->hasResponse() not working?!
            {
                // Send the response back to the client.
                if (response && server->connections_.size() > 0 && client->connection_ != NULL)
                {
                    PrintThread{} << "Sending response for " << botName << std::endl;
                    server->QueueResponse(client->connection_, response);
                    server->SendResponse();
                    LastRequest = std::time(nullptr);
                }
                else
                {
                    CurrentExitCase = ExitCase::ClientTimeout;
                }
            }
            if (difftime(std::time(nullptr), LastRequest) > 5)
            {
                PrintThread{} << "No new requests/responses for (" << botName << ")" << std::endl;
                CurrentExitCase = ExitCase::ClientTimeout;
            }
        }
        return CurrentExitCase;
    }
    catch (const std::exception& e)
    {
        PrintThread{} << e.what() << std::endl;
        return ExitCase::ClientTimeout;
    }
}

void ResolveMap(const std::string& map_name, SC2APIProtocol::RequestCreateGame* request, sc2::ProcessSettings process_settings) {
    // BattleNet map
    if (!sc2::HasExtension(map_name, ".SC2Map")) {
        request->set_battlenet_map_name(map_name);
        return;
    }

    // Absolute path
    SC2APIProtocol::LocalMap* local_map = request->mutable_local_map();
    if (sc2::DoesFileExist(map_name)) {
        local_map->set_map_path(map_name);
        return;
    }

    // Relative path - Game maps directory
    std::string game_relative = sc2::GetGameMapsDirectory(process_settings.process_path) + map_name;
    if (sc2::DoesFileExist(game_relative)) {
        local_map->set_map_path(map_name);
        return;
    }

    // Relative path - Library maps directory
    std::string library_relative = sc2::GetLibraryMapsDirectory() + map_name;
    if (sc2::DoesFileExist(library_relative)) {
        local_map->set_map_path(library_relative);
        return;
    }

    // Relative path - Remotely saved maps directory
    local_map->set_map_path(map_name);
}

sc2::GameRequestPtr CreateStartGameRequest(const std::string &MapName, std::vector<sc2::PlayerSetup> players, sc2::ProcessSettings process_settings)
{
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();

    SC2APIProtocol::RequestCreateGame* request_create_game = request->mutable_create_game();
    for (const sc2::PlayerSetup& setup : players)
    {
        SC2APIProtocol::PlayerSetup* playerSetup = request_create_game->add_player_setup();
        playerSetup->set_type(SC2APIProtocol::PlayerType(setup.type));
        playerSetup->set_race(SC2APIProtocol::Race(int(setup.race) + 1));
        playerSetup->set_difficulty(SC2APIProtocol::Difficulty(setup.difficulty));
    }
    ResolveMap(MapName, request_create_game, process_settings);

    request_create_game->set_realtime(false);
    return request;
}


void LadderGame::LogStartGame(const BotConfig &Bot1, const BotConfig &Bot2)
{
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::string Bot1Filename = Bot1.RootPath + "/stderr.log";
    std::string Bot2Filename = Bot2.RootPath + "/stderr.log";
    std::ofstream outfile;
    outfile.open(Bot1Filename, std::ios_base::app);
    outfile << std::endl << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") << ": " << "Starting game vs " << Bot2.BotName << std::endl;
    outfile.close();
    outfile.open(Bot2Filename, std::ios_base::app);
    outfile << std::endl << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") << ": " << "Starting game vs " << Bot1.BotName << std::endl;
    outfile.close();
}


bool LadderGame::SaveReplay(sc2::Connection *client, const std::string& path) {
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();
    request->mutable_save_replay();
    SendDataToConnection(client, request.get());
    SC2APIProtocol::Response* replay_response = nullptr;
    if (!client->Receive(replay_response, 10000))
    {
        //		std::cout << "Failed to receive replay response" << std::endl;
        return false;
    }

    const SC2APIProtocol::ResponseSaveReplay& response_replay = replay_response->save_replay();

    if (response_replay.data().size() == 0) {
        return false;
    }

    std::ofstream file;
    file.open(path, std::fstream::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(&response_replay.data()[0], response_replay.data().size());
    return true;
}

bool LadderGame::ProcessObservationResponse(SC2APIProtocol::ResponseObservation Response, std::vector<sc2::PlayerResult> *PlayerResults)
{
    if (Response.player_result_size())
    {
        PlayerResults->clear();
        for (const auto& player_result : Response.player_result()) {
            PlayerResults->push_back(sc2::PlayerResult(player_result.player_id(), sc2::ConvertGameResultFromProto(player_result.result())));
        }
        return true;
    }
    return false;
}

std::string LadderGame::GetBotCommandLine(const BotConfig &AgentConfig, int GamePort, int StartPort, const std::string &OpponentId, bool CompOpp, sc2::Race CompRace, sc2::Difficulty CompDifficulty)
{
    // Add bot type specific command line needs
    std::string OutCmdLine;
    switch (AgentConfig.Type)
    {
    case Python:
    {
        OutCmdLine = Config->GetValue("PythonBinary") + " " + AgentConfig.FileName;
        break;
    }
    case Wine:
    {
        OutCmdLine = "wine " + AgentConfig.FileName;
        break;
    }
    case Mono:
    {
        OutCmdLine = "mono " + AgentConfig.FileName;
        break;
    }
    case DotNetCore:
    {
        OutCmdLine = "dotnet " + AgentConfig.FileName;
        break;
    }
    case CommandCenter:
    {
        OutCmdLine = Config->GetValue("CommandCenterPath") + " --ConfigFile " + AgentConfig.FileName;
        break;
    }
    case BinaryCpp:
    {
        OutCmdLine = AgentConfig.RootPath + AgentConfig.FileName;
        break;
    }
    case Java:
    {
        OutCmdLine = "java -jar " + AgentConfig.FileName;
        break;
    }
    case NodeJS:
    {
        OutCmdLine = Config->GetValue("NodeJSBinary") + " " + AgentConfig.FileName;
        break;
    }
    case DefaultBot: {} // BlizzardAI - doesn't need any command line arguments
    }

    // Add universal arguments
    OutCmdLine += " --GamePort " + std::to_string(GamePort) + " --StartPort " + std::to_string(StartPort) + " --LadderServer 127.0.0.1 --OpponentId " + OpponentId;

    if (CompOpp)
    {
        OutCmdLine += " --ComputerOpponent 1 --ComputerRace " + GetRaceString(CompRace) + " --ComputerDifficulty " + GetDifficultyString(CompDifficulty);
    }
    if (AgentConfig.Args != "")
    {
        OutCmdLine += " " + AgentConfig.Args;
    }
    return OutCmdLine;
}


void LadderGame::ResolveMap(const std::string& map_name, SC2APIProtocol::RequestCreateGame* request, sc2::ProcessSettings process_settings) {
    // BattleNet map
    if (!sc2::HasExtension(map_name, ".SC2Map")) {
        request->set_battlenet_map_name(map_name);
        return;
    }

    // Absolute path
    SC2APIProtocol::LocalMap* local_map = request->mutable_local_map();
    if (sc2::DoesFileExist(map_name)) {
        local_map->set_map_path(map_name);
        return;
    }

    // Relative path - Game maps directory
    std::string game_relative = sc2::GetGameMapsDirectory(process_settings.process_path) + map_name;
    if (sc2::DoesFileExist(game_relative)) {
        local_map->set_map_path(map_name);
        return;
    }

    // Relative path - Library maps directory
    std::string library_relative = sc2::GetLibraryMapsDirectory() + map_name;
    if (sc2::DoesFileExist(library_relative)) {
        local_map->set_map_path(library_relative);
        return;
    }

    // Relative path - Remotely saved maps directory
    local_map->set_map_path(map_name);
}

sc2::GameRequestPtr LadderGame::CreateStartGameRequest(const std::string &MapName, std::vector<sc2::PlayerSetup> players, sc2::ProcessSettings process_settings)
{
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();

    SC2APIProtocol::RequestCreateGame* request_create_game = request->mutable_create_game();
    for (const sc2::PlayerSetup& setup : players)
    {
        SC2APIProtocol::PlayerSetup* playerSetup = request_create_game->add_player_setup();
        playerSetup->set_type(SC2APIProtocol::PlayerType(setup.type));
        playerSetup->set_race(SC2APIProtocol::Race(int(setup.race) + 1));
        playerSetup->set_difficulty(SC2APIProtocol::Difficulty(setup.difficulty));
    }
    ResolveMap(MapName, request_create_game, process_settings);

    request_create_game->set_realtime(false);
    return request;
}

sc2::GameResponsePtr LadderGame::CreateErrorResponse()
{
    const sc2::GameResponsePtr response = std::make_shared<SC2APIProtocol::Response>(SC2APIProtocol::Response());
    return response;
}

sc2::GameRequestPtr LadderGame::CreateLeaveGameRequest()
{
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();

    request->mutable_leave_game();

    return request;
}

sc2::GameRequestPtr LadderGame::CreateQuitRequest()
{
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();
    request->mutable_quit();

    return request;
}


bool LadderGame::SendDataToConnection(sc2::Connection *Connection, const SC2APIProtocol::Request *request)
{
    if (Connection->connection_ != nullptr)
    {
        Connection->Send(request);
        return true;
    }
    return false;
}


ResultType LadderGame::GetPlayerResults(sc2::Connection *client)
{
    if (client == nullptr)
    {
        return ResultType::ProcessingReplay;
    }
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr ObservationRequest = proto.MakeRequest();
    ObservationRequest->mutable_observation();
    SendDataToConnection(client, ObservationRequest.get());

    SC2APIProtocol::Response* ObservationResponse = nullptr;
    std::vector<sc2::PlayerResult> PlayerResults;
    if (client->Receive(ObservationResponse, 100000))
    {
        ProcessObservationResponse(ObservationResponse->observation(), &PlayerResults);
    }
    if (PlayerResults.size() > 1)
    {
        if (PlayerResults.back().result == sc2::GameResult::Undecided)
        {
            return ResultType::ProcessingReplay;
        }
        else if (PlayerResults.back().result == sc2::GameResult::Tie)
        {
            return ResultType::Tie;
        }
        else if (PlayerResults.back().result == sc2::GameResult::Win)
        {
            if (PlayerResults.back().player_id == 1)
            {
                return ResultType::Player1Win;
            }
            else
            {
                return ResultType::Player2Win;
            }
        }
        else if (PlayerResults.back().result == sc2::GameResult::Loss)
        {
            if (PlayerResults.back().player_id == 1)
            {
                return ResultType::Player2Win;
            }
            else
            {
                return ResultType::Player1Win;
            }

        }
    }
    return ResultType::ProcessingReplay;
}

GameResult LadderGame::StartGameVsDefault(const BotConfig &Agent1, sc2::Race CompRace, sc2::Difficulty CompDifficulty, const std::string &Map)
{
    std::string ReplayDir = Config->GetValue("LocalReplayDirectory");
    std::string ReplayFile = ReplayDir + Agent1.BotName + "v" + GetDifficultyString(CompDifficulty) + "-" + RemoveMapExtension(Map) + ".Sc2Replay";
    ReplayFile.erase(remove_if(ReplayFile.begin(), ReplayFile.end(), isspace), ReplayFile.end());
    remove(ReplayFile.c_str());
    using namespace std::chrono_literals;
    // Setup server that mimicks sc2.
    std::string Agent1Path = GetBotCommandLine(Agent1, 5677, PORT_START, "", true, sc2::Race::Random, CompDifficulty);
    if (Agent1Path == "")
    {
        return GameResult();
    }

    sc2::Server server;

    server.Listen("5677", "100000", "100000", "5");

    // Find game executable and run it.
    sc2::ProcessSettings process_settings;
    sc2::GameSettings game_settings;
    sc2::ParseSettings(CoordinatorArgc, CoordinatorArgv, process_settings, game_settings);
    uint64_t BotProcessId = sc2::StartProcess(process_settings.process_path,
        { "-listen", "127.0.0.1",
          "-port", "5679",
          "-displayMode", "0",
          "-dataVersion", process_settings.data_version }
    );

    // Connect to running sc2 process.
    sc2::Connection client;
    client.Connect("127.0.0.1", 5679, false);
    int connectionAttemptsClient = 0;
    while (!client.Connect("127.0.0.1", 5679, false))
    {
        connectionAttemptsClient++;
        sc2::SleepFor(1000);
        if (connectionAttemptsClient > 60)
        {
            PrintThread{} << "Failed to connect client 1. BotProcessID: " << BotProcessId << std::endl;
            return GameResult();
        }
    }

    std::vector<sc2::PlayerSetup> Players;
    Players.push_back(sc2::PlayerSetup(sc2::PlayerType::Participant, Agent1.Race, nullptr, sc2::Easy));
    Players.push_back(sc2::PlayerSetup(sc2::PlayerType::Computer, sc2::Race::Random, nullptr, CompDifficulty));
    sc2::GameRequestPtr Create_game_request = CreateStartGameRequest(Map, Players, process_settings);
    SendDataToConnection(&client, Create_game_request.get());

    SC2APIProtocol::Response* create_response = nullptr;
    if (client.Receive(create_response, 100000))
    {
        PrintThread{} << "Recieved create game response " << create_response->data().DebugString() << std::endl;
        if (ProcessResponse(create_response->create_game()))
        {
            PrintThread{} << "Create game successful" << std::endl << std::endl;
        }
    }
    unsigned long ProcessId;
    auto bot1ProgramThread = std::thread(StartBotProcess, Agent1, Agent1Path, &ProcessId);
    sc2::SleepFor(1000);

    PrintThread{} << "Monitoring client of: " << Agent1.BotName << std::endl;
    float_t AvgFrameTime = 0;
    int32_t GameLoop;
    auto bot1UpdateThread = std::async(GameUpdate, &client, &server, &Agent1.BotName, MaxGameTime, MaxRealGameTime, &AvgFrameTime, &GameLoop);
    sc2::SleepFor(1000);

    ResultType CurrentResult = ResultType::InitializationError;
    bool GameRunning = true;
    //sc2::ProtoInterface proto_1;
    //std::vector<sc2::PlayerResult> Player1Results;
    SleepFor(10000);
    while (GameRunning)
    {

        auto update1status = bot1UpdateThread.wait_for(1s);
        if (update1status == std::future_status::ready)
        {
            ExitCase BotExitCase = bot1UpdateThread.get();
            if (BotExitCase == ExitCase::ClientRequestExit)
            {
                // If Player 1 has requested exit, he has surrendered, and player 2 is awarded the win
                CurrentResult = ResultType::Player2Win;
            }
            else if (BotExitCase == ExitCase::ClientTimeout)
            {
                CurrentResult = ResultType::Player1Crash;
            }
            else if (BotExitCase == ExitCase::GameTimeout)
            {
                CurrentResult = ResultType::Timeout;
            }
            else
            {
                CurrentResult = ResultType::ProcessingReplay;
            }

            GameRunning = false;
            break;
        }
    }
    if (CurrentResult == ResultType::ProcessingReplay)
    {
        CurrentResult = GetPlayerResults(&client);
    }


    SaveReplay(&client, ReplayFile);
    if (!SendDataToConnection(&client, CreateLeaveGameRequest().get()))
    {
        PrintThread{} << "CreateLeaveGameRequest failed" << std::endl;
    }

    bot1ProgramThread.join();
    GameResult result;
    result.Result = CurrentResult;
    return result;
}

GameResult LadderGame::StartGame(const BotConfig &Agent1, const BotConfig &Agent2, const std::string &Map)
{

    using namespace std::chrono_literals;
    // Setup server that mimicks sc2.
    std::string Agent1Path = GetBotCommandLine(Agent1, 5677, PORT_START, Agent2.PlayerId);
    std::string Agent2Path = GetBotCommandLine(Agent2, 5678, PORT_START, Agent1.PlayerId);
    if (Agent1Path == "" || Agent2Path == "")
    {
        return GameResult();
    }
    sc2::Server server;
    sc2::Server server2;
    server.Listen("5677", "100000", "100000", "5");
    server2.Listen("5678", "100000", "100000", "5");
    // Find game executable and run it.
    sc2::ProcessSettings process_settings;
    sc2::GameSettings game_settings;
    sc2::ParseSettings(CoordinatorArgc, CoordinatorArgv, process_settings, game_settings);
    auto GameClientPid1 = sc2::StartProcess(process_settings.process_path,
        { "-listen", "127.0.0.1",
          "-port", "5679",
          "-displayMode", "0",
          "-dataVersion", process_settings.data_version }
    );
    auto GameClientPid2 = sc2::StartProcess(process_settings.process_path,
        { "-listen", "127.0.0.1",
          "-port", "5680",
          "-displayMode", "0",
          "-dataVersion", process_settings.data_version }
    );

    // Connect to running sc2 process.
    sc2::Connection client;
    int connectionAttemptsClient1 = 0;
    while (!client.Connect("127.0.0.1", 5679, false))
    {
        connectionAttemptsClient1++;
        sc2::SleepFor(1000);
        if (connectionAttemptsClient1 > 60)
        {
            PrintThread{} << "Failed to connect client 1. ClientProcessID: " << GameClientPid1 << std::endl;
            return GameResult();
        }
    }
    sc2::Connection client2;
    int connectionAttemptsClient2 = 0;
    while (!client2.Connect("127.0.0.1", 5680, false))
    {
        connectionAttemptsClient2++;
        sc2::SleepFor(1000);
        if (connectionAttemptsClient2 > 60)
        {
            PrintThread{} << "Failed to connect client 2. ClientProcessID: " << GameClientPid2 << std::endl;
            return GameResult();
        }
    }

    std::vector<sc2::PlayerSetup> Players;

    Players.push_back(sc2::PlayerSetup(sc2::PlayerType::Participant, Agent1.Race, nullptr, sc2::Easy));
    Players.push_back(sc2::PlayerSetup(sc2::PlayerType::Participant, Agent2.Race, nullptr, sc2::Easy));
    sc2::GameRequestPtr Create_game_request = CreateStartGameRequest(Map, Players, process_settings);
    client.Send(Create_game_request.get());
    SC2APIProtocol::Response* create_response = nullptr;
    if (client.Receive(create_response, 100000))
    {
        PrintThread{} << "Recieved create game response " << create_response->data().DebugString() << std::endl;
        if (ProcessResponse(create_response->create_game()))
        {
            PrintThread{} << "Create game successful" << std::endl << std::endl;
        }
    }
    unsigned long Bot1ThreadId = 0;
    unsigned long Bot2ThreadId = 0;
    LogStartGame(Agent1, Agent2);
    auto bot1ProgramThread = std::async(&StartBotProcess, Agent1, Agent1Path, &Bot1ThreadId);
    auto bot2ProgramThread = std::async(&StartBotProcess, Agent2, Agent2Path, &Bot2ThreadId);
    sc2::SleepFor(500);
    sc2::SleepFor(500);

    //toDo check here already if the bots crashed.
    float_t Bot1AvgFrame = 0;
    float_t Bot2AvgFrame = 0;
    int32_t GameLoop;
    auto bot1UpdateThread = std::async(&GameUpdate, &client, &server, &Agent1.BotName, MaxGameTime, MaxRealGameTime, &Bot1AvgFrame, &GameLoop);
    auto bot2UpdateThread = std::async(&GameUpdate, &client2, &server2, &Agent2.BotName, MaxGameTime, MaxRealGameTime, &Bot2AvgFrame, nullptr);
    sc2::SleepFor(1000);

    ResultType CurrentResult = ResultType::InitializationError;
    bool GameRunning = true;
    //sc2::ProtoInterface proto_1;
    while (GameRunning)
    {
        auto update1status = bot1UpdateThread.wait_for(1s);
        auto update2status = bot2UpdateThread.wait_for(0ms);
        auto thread1Status = bot1ProgramThread.wait_for(0ms);
        auto thread2Status = bot2ProgramThread.wait_for(0ms);
        if (update1status == std::future_status::ready)
        {
            ExitCase BotExitCase = bot1UpdateThread.get();
            if (BotExitCase == ExitCase::ClientRequestExit)
            {
                // If Player 1 has requested exit, he has surrendered, and player 2 is awarded the win
                CurrentResult = ResultType::Player2Win;
            }
            else if (BotExitCase == ExitCase::ClientTimeout)
            {
                CurrentResult = ResultType::Player1Crash;
            }
            else if (BotExitCase == ExitCase::GameTimeout)
            {
                CurrentResult = ResultType::Timeout;
            }
            else
            {
                CurrentResult = ResultType::ProcessingReplay;
            }

            GameRunning = false;
            break;
        }
        if (update2status == std::future_status::ready)
        {
            ExitCase BotExitCase = bot2UpdateThread.get();
            if (BotExitCase == ExitCase::ClientRequestExit)
            {
                // If Player 2 has requested exit, he has surrendered, and player 1 is awarded the win
                CurrentResult = ResultType::Player1Win;
            }
            else if (BotExitCase == ExitCase::ClientTimeout)
            {
                CurrentResult = ResultType::Player2Crash;
            }
            else if (BotExitCase == ExitCase::GameTimeout)
            {
                CurrentResult = ResultType::Timeout;
            }
            else
            {
                CurrentResult = ResultType::ProcessingReplay;
            }

            GameRunning = false;
            break;
        }
        if (thread1Status == std::future_status::ready)
        {
            CurrentResult = ResultType::Player1Crash;
            GameRunning = false;
        }
        if (thread2Status == std::future_status::ready)
        {
            CurrentResult = ResultType::Player2Crash;
            GameRunning = false;
        }
    }

    if (CurrentResult == ResultType::ProcessingReplay)
    {
        CurrentResult = GetPlayerResults(&client);
    }
    if (CurrentResult == ResultType::ProcessingReplay)
    {
        CurrentResult = GetPlayerResults(&client2);
    }
    sc2::SleepFor(1000);
    PrintThread{} << "Saving replay" << std::endl;
    std::string ReplayDir = Config->GetValue("LocalReplayDirectory");
    std::string ReplayFile = ReplayDir + Agent1.BotName + "v" + Agent2.BotName + "-" + RemoveMapExtension(Map) + ".SC2Replay";
    ReplayFile.erase(remove_if(ReplayFile.begin(), ReplayFile.end(), isspace), ReplayFile.end());
    if (!SaveReplay(&client, ReplayFile))
    {
        SaveReplay(&client2, ReplayFile);
    }
    sc2::SleepFor(1000);
    ChangeBotNames(ReplayFile, Agent1.BotName, Agent2.BotName);
    // Process last requests
    std::thread onEnd1(&OnEnd, &client, &server, Agent1.BotName);
    std::thread onEnd2(&OnEnd, &client2, &server2, Agent2.BotName);
    onEnd1.join();
    onEnd2.join();
    sc2::SleepFor(1000);
    if (!server.connections_.empty())
    {
        server.connections_.clear();
        PrintThread{} << Agent1.BotName << " is still connected..." << std::endl;
    }
    if (!server2.connections_.empty())
    {
        PrintThread{} << Agent2.BotName << " is still connected..." << std::endl;
    }
    std::future_status bot1ProgStatus, bot2ProgStatus;
    auto start = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds;
    while (elapsed_seconds.count() < 20)
    {
        bot1ProgStatus = bot1ProgramThread.wait_for(50ms);
        bot2ProgStatus = bot2ProgramThread.wait_for(50ms);
        if (bot1ProgStatus == std::future_status::ready && bot2ProgStatus == std::future_status::ready)
        {
            PrintThread{} << "Both bots quit properly." << std::endl;
            break;
        }
        elapsed_seconds = std::chrono::system_clock::now() - start;
    }
    if (bot1ProgStatus != std::future_status::ready)
    {
        PrintThread{} << "Failed to detect end of " << Agent1.BotName << " after 20s. Make sure the bot does not issue leaveGame or quit. Killing" << std::endl;
        KillBotProcess(Bot1ThreadId);
    }
    if (bot2ProgStatus != std::future_status::ready)
    {
        PrintThread{} << "Failed to detect end of " << Agent2.BotName << " after 20s. Make sure the bot does not issue leaveGame or quit. Killing" << std::endl;
        KillBotProcess(Bot2ThreadId);
    }
    sc2::SleepFor(1000);
    sc2::TerminateProcess(GameClientPid1);
    sc2::TerminateProcess(GameClientPid2);
    sc2::SleepFor(1000);
    GameResult Result;
    Result.Result = CurrentResult;
    Result.Bot1AvgFrame = Bot1AvgFrame;
    Result.Bot2AvgFrame = Bot2AvgFrame;
    Result.GameLoop = GameLoop;
    return Result;
}


void LadderGame::ChangeBotNames(const std::string ReplayFile, const std::string &Bot1Name, const std::string Bot2Name)
{
    std::string CmdLine = Config->GetValue("ReplayBotRenameProgram");
    if (CmdLine.size() > 0)
    {
        CmdLine = CmdLine + " " + ReplayFile + " " + FIRST_PLAYER_NAME + " " + Bot1Name + " " + SECOND_PLAYER_NAME + " " + Bot2Name;
        StartExternalProcess(CmdLine);
    }
}

LadderGame::LadderGame(int InCoordinatorArgc, char** InCoordinatorArgv, LadderConfig *InConfig)
    : CoordinatorArgc(InCoordinatorArgc)
    , CoordinatorArgv(InCoordinatorArgv)
    , Config(InConfig)
{


    std::string MaxGameTimeString = Config->GetValue("MaxGameTime");
    if (MaxGameTimeString.length() > 0)
    {
        MaxGameTime = std::stoi(MaxGameTimeString);
    }
    std::string MaxRealGameTimeString = Config->GetValue("MaxRealGameTime");
    if (MaxRealGameTimeString.length() > 0)
    {
        MaxRealGameTime = std::stoi(MaxRealGameTimeString);
    }

}
