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

#define RAPIDJSON_HAS_STDSTRING 1

#include "rapidjson.h"
#include "document.h"
#include "ostreamwrapper.h"
#include "writer.h"
#include "prettywriter.h"
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
#include "LadderConfig.h"
#include "LadderManager.h"
#include "MatchupList.h"
#include "Tools.h"

#ifdef ENABLE_CURL
#include "curl.h"
#endif

std::mutex PrintThread::_mutexPrint{};

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


ExitCase GameUpdate(sc2::Connection *client, sc2::Server *server, const std::string *botName, uint32_t MaxGameTime)
{
	//    std::cout << "Sending Join game request" << std::endl;
	//    sc2::GameRequestPtr Create_game_request = CreateJoinGameRequest();
	//    Client->Send(Create_game_request.get());
	ExitCase CurrentExitCase = ExitCase::InProgress;
	PrintThread{} << "Starting proxy for " << *botName << std::endl;
	clock_t LastRequest = clock();
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
		bool RequestFound = false;
		bool AlreadyWarned = false;
		while (CurrentExitCase == ExitCase::InProgress) {
			SC2APIProtocol::Status CurrentStatus;
			if (!client || !server)
			{
				return ExitCase::ClientTimeout;
			}
			if (client->connection_ == nullptr && RequestFound)
			{
				PrintThread{} << "Client disconnect (" << *botName << ")" << std::endl;
				CurrentExitCase = ExitCase::ClientTimeout;
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
						std::cout << *botName << " IS USING DEBUG INTERFACE.  POSSIBLE CHEAT Please tell them not to" << std::endl;
						AlreadyWarned = true;
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
						uint32_t currentGameLoop = ActualObservation.game_loop();
						if (currentGameLoop > MaxGameTime)
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
					PrintThread{} << "Client timeout (" << *botName <<")" << std::endl;
					CurrentExitCase = ExitCase::ClientTimeout;
				}
			}

		}
		return CurrentExitCase;
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
		return ExitCase::ClientTimeout;
	}
}

bool LadderManager::SaveReplay(sc2::Connection *client, const std::string& path) {
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

bool LadderManager::ProcessObservationResponse(SC2APIProtocol::ResponseObservation Response, std::vector<sc2::PlayerResult> *PlayerResults)
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

std::string LadderManager::GetBotCommandLine(const BotConfig &AgentConfig, int GamePort, int StartPort, const std::string &OpponentId, bool CompOpp, sc2::Race CompRace, sc2::Difficulty CompDifficulty)
{
	std::string OutCmdLine;
	switch (AgentConfig.Type)
	{
	case Python:
	{
		OutCmdLine = Config->GetValue("PythonBinary") + " " + AgentConfig.FileName;
		OutCmdLine += " --GamePort " + std::to_string(GamePort) + " --StartPort " + std::to_string(StartPort) + " --LadderServer 127.0.0.1 --OpponentId " + OpponentId;
		break;
	}
	case Wine:
	{
		OutCmdLine = "wine " + AgentConfig.FileName;
		OutCmdLine += " --GamePort " + std::to_string(GamePort) + " --StartPort " + std::to_string(StartPort) + " --LadderServer 127.0.0.1 --OpponentId " + OpponentId;
		break;
	}
	case Mono:
	{
		OutCmdLine = "mono " + AgentConfig.FileName;
		OutCmdLine += " --GamePort " + std::to_string(GamePort) + " --StartPort " + std::to_string(StartPort) + " --LadderServer 127.0.0.1 --OpponentId " + OpponentId;
		break;
	}
	case BinaryCpp:
	{
		OutCmdLine = AgentConfig.RootPath + AgentConfig.FileName;
		OutCmdLine += " --GamePort " + std::to_string(GamePort) + " --StartPort " + std::to_string(StartPort) + " --LadderServer 127.0.0.1 --OpponentId " + OpponentId;
		break;
	}
	case CommandCenter:
	{
		OutCmdLine = Config->GetValue("CommandCenterPath") + " --ConfigFile " + AgentConfig.FileName;
		OutCmdLine += " --GamePort " + std::to_string(GamePort) + " --StartPort " + std::to_string(StartPort) + " --LadderServer 127.0.0.1 --OpponentId " + OpponentId;
		break;

	}
	case DefaultBot:
	{
		OutCmdLine += " --GamePort " + std::to_string(GamePort) + " --StartPort " + std::to_string(StartPort) + " --LadderServer 127.0.0.1 --OpponentId " + OpponentId;
	}
	}

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

sc2::GameResponsePtr LadderManager::CreateErrorResponse()
{
	const sc2::GameResponsePtr response = std::make_shared<SC2APIProtocol::Response>(SC2APIProtocol::Response());
	return response;
}

sc2::GameRequestPtr LadderManager::CreateLeaveGameRequest()
{
	sc2::ProtoInterface proto;
	sc2::GameRequestPtr request = proto.MakeRequest();

	request->mutable_quit();

	return request;
}

sc2::GameRequestPtr LadderManager::CreateQuitRequest()
{
	sc2::ProtoInterface proto;
	sc2::GameRequestPtr request = proto.MakeRequest();
	request->mutable_quit();

	return request;
}

ResultType LadderManager::GetPlayerResults(sc2::Connection *client)
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

bool LadderManager::SendDataToConnection(sc2::Connection *Connection, const SC2APIProtocol::Request *request)
{
	if (Connection->connection_ != nullptr)
	{
		Connection->Send(request);
		return true;
	}
	return false;
}

ResultType LadderManager::StartGameVsDefault(const BotConfig &Agent1, sc2::Race CompRace, sc2::Difficulty CompDifficulty, const std::string &Map, int32_t &GameLoop)
{
	using namespace std::chrono_literals;
	// Setup server that mimicks sc2.
	std::string Agent1Path = GetBotCommandLine(Agent1, 5677, PORT_START, "", true, sc2::Race::Random, CompDifficulty);
	if (Agent1Path == "" )
	{
		return ResultType::InitializationError;
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
	client.Connect("127.0.0.1", 5679);
	int connectionAttemptsClient = 0;
	while (!client.Connect("127.0.0.1", 5679, false))
	{
		connectionAttemptsClient++;
		sc2::SleepFor(1000);
		if (connectionAttemptsClient > 60)
		{
			std::cout << "Failed to connect client 1. BotProcessID: " << BotProcessId << std::endl;
			return ResultType::InitializationError;
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
		std::cout << "Recieved create game response " << create_response->data().DebugString() << std::endl;
		if (ProcessResponse(create_response->create_game()))
		{
			std::cout << "Create game successful" << std::endl << std::endl;
		}
	}
	unsigned long ProcessId;
	auto bot1ProgramThread = std::thread(StartBotProcess,Agent1, Agent1Path, &ProcessId);
	sc2::SleepFor(1000);

	std::cout << "Monitoring client of: " << Agent1.BotName << std::endl;
	auto bot1UpdateThread = std::async(GameUpdate, &client, &server,&Agent1.BotName, MaxGameTime);
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

	std::string ReplayDir = Config->GetValue("LocalReplayDirectory");
	std::string ReplayFile = ReplayDir + Agent1.BotName + "v" + GetDifficultyString(CompDifficulty) + "-" + RemoveMapExtension(Map) + ".Sc2Replay";
	ReplayFile.erase(remove_if(ReplayFile.begin(), ReplayFile.end(), isspace), ReplayFile.end());

	SaveReplay(&client, ReplayFile);
	if (!SendDataToConnection(&client, CreateLeaveGameRequest().get()))
	{
		std::cout << "CreateLeaveGameRequest failed" << std::endl;
	}

	bot1ProgramThread.join();
	return CurrentResult;
}

ResultType LadderManager::StartGame(const BotConfig &Agent1, const BotConfig &Agent2, const std::string &Map, int32_t &GameLoop)
{
	
	using namespace std::chrono_literals;
	// Setup server that mimicks sc2.
	std::string Agent1Path = GetBotCommandLine(Agent1, 5677, PORT_START, Agent2.PlayerId);
	std::string Agent2Path = GetBotCommandLine(Agent2, 5678, PORT_START, Agent1.PlayerId);
	if (Agent1Path == "" || Agent2Path == "")
	{
		return ResultType::InitializationError;
	}
	sc2::Server server;
	sc2::Server server2;
	server.Listen("5677", "100000", "100000", "5");
	server2.Listen("5678", "100000", "100000", "5");
	// Find game executable and run it.
	sc2::ProcessSettings process_settings;
	sc2::GameSettings game_settings;
	sc2::ParseSettings(CoordinatorArgc, CoordinatorArgv, process_settings, game_settings);
	uint64_t Bot1ProcessId = sc2::StartProcess(process_settings.process_path,
	{ "-listen", "127.0.0.1",
		"-port", "5679",
		"-displayMode", "0",
		"-dataVersion", process_settings.data_version }
	);
	uint64_t Bot2ProcessId = sc2::StartProcess(process_settings.process_path,
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
			std::cout << "Failed to connect client 1. BotProcessID: " << Bot1ProcessId << std::endl;
			return ResultType::InitializationError;
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
			std::cout << "Failed to connect client 2. BotProcessID: " << Bot2ProcessId << std::endl;
			return ResultType::InitializationError;
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
		std::cout << "Recieved create game response " << create_response->data().DebugString() << std::endl;
		if (ProcessResponse(create_response->create_game()))
		{
			std::cout << "Create game successful" << std::endl << std::endl;
		}
	}
	unsigned long Bot1ThreadId = 0;
	unsigned long Bot2ThreadId = 0;
	auto bot1ProgramThread = std::async(&StartBotProcess, Agent1, Agent1Path, &Bot1ThreadId);
	auto bot2ProgramThread = std::async(&StartBotProcess, Agent2, Agent2Path, &Bot2ThreadId);
	sc2::SleepFor(500);
	sc2::SleepFor(500);

	//toDo check here already if the bots crashed.

	auto bot1UpdateThread = std::async(&GameUpdate, &client, &server, &Agent1.BotName, MaxGameTime);
	auto bot2UpdateThread = std::async(&GameUpdate, &client2, &server2, &Agent2.BotName, MaxGameTime);
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
			else if( BotExitCase == ExitCase::ClientTimeout)
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
		if(update2status == std::future_status::ready)
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
	std::string ReplayDir = Config->GetValue("LocalReplayDirectory");

	std::string ReplayFile = ReplayDir + Agent1.BotName + "v" + Agent2.BotName + "-" + RemoveMapExtension(Map) + ".SC2Replay";
	ReplayFile.erase(remove_if(ReplayFile.begin(), ReplayFile.end(), isspace), ReplayFile.end());
	if (!SaveReplay(&client, ReplayFile))
	{
		SaveReplay(&client2, ReplayFile);
	}
	sc2::SleepFor(1000);
	if (!SendDataToConnection(&client, CreateLeaveGameRequest().get()))
	{
		std::cout << "CreateLeaveGameRequest failed for Client 1." << std::endl;
	}
	sc2::SleepFor(1000);
	if (!SendDataToConnection(&client2, CreateLeaveGameRequest().get()))
	{
		std::cout << "CreateLeaveGameRequest failed for Client 2." << std::endl;
	}
	sc2::SleepFor(1000);
	if (server.HasRequest() && server.connections_.size() > 0)
	{
		server.SendRequest();
	}
	sc2::SleepFor(1000);
	if (server2.HasRequest() && server.connections_.size() > 0)
	{
		server2.SendRequest();
	}
	ChangeBotNames(ReplayFile, Agent1.BotName, Agent2.BotName);

	if (CurrentResult == Player1Crash || CurrentResult == Player2Crash)
	{
		sc2::SleepFor(5000);
		KillSc2Process((unsigned long)Bot1ProcessId);
		KillSc2Process((unsigned long)Bot2ProcessId);
		sc2::SleepFor(5000);
		try
		{
			bot1UpdateThread.wait();
			bot2UpdateThread.wait();

		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << std::endl <<" Unable to detect end of update thread.  Continuing" << std::endl;
			return CurrentResult;
		}

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
			break;
		}
		elapsed_seconds = std::chrono::system_clock::now() - start;
	}
	if (bot1ProgStatus != std::future_status::ready)
	{
		std::cout << "Failed to detect end of " << Agent1.BotName << " after 20s.  Killing" << std::endl;
		KillSc2Process(Bot1ThreadId);
	}
	if (bot2ProgStatus != std::future_status::ready)
	{
		std::cout << "Failed to detect end of " << Agent2.BotName << " after 20s.  Killing" << std::endl;
		KillSc2Process(Bot2ThreadId);
	}
	return CurrentResult;
}


LadderManager::LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv)

	: coordinator(nullptr)
	, CoordinatorArgc(InCoordinatorArgc)
	, CoordinatorArgv(inCoordinatorArgv)
	, MaxGameTime(0)
	, ConfigFile("LadderManager.conf")
	, EnableReplayUploads(false)
	, EnableServerLogin(false)
	, EnablePlayerIds(false)
	, Sc2Launched(false)
	, Config(nullptr)
	, PlayerIds(nullptr)
{
}

// Used for tests
LadderManager::LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv, char *InConfigFile)

	: coordinator(nullptr)
	, CoordinatorArgc(InCoordinatorArgc)
	, CoordinatorArgv(inCoordinatorArgv)
	, MaxGameTime(0)
	, ConfigFile(InConfigFile)
	, EnableReplayUploads(false)
	, EnableServerLogin(false)
	, EnablePlayerIds(false)
	, Sc2Launched(false)
	, Config(nullptr) 
	, PlayerIds(nullptr)
{
}

std::string LadderManager::GerneratePlayerId(size_t Length)
{
	static const char hexdigit[16] = { '0', '1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };
	std::string outstring;
	if (Length < 1)
	{
		return outstring;

	}
	--Length;
	for (int i = 0; i < Length; ++i)
	{
		outstring.append(1, hexdigit[rand() % sizeof hexdigit]);
	}
	return outstring;
}

bool LadderManager::LoadSetup()
{
	delete Config;
	Config = new LadderConfig(ConfigFile);
	if (!Config->ParseConfig())
	{
		std::cout << "No valid config found at " << ConfigFile << std::endl;
		return false;
	}

	std::string PlayerIdFile = Config->GetValue("PlayerIdFile");
	if (PlayerIdFile.length() > 0)
	{
		PlayerIds = new LadderConfig(PlayerIdFile);
		EnablePlayerIds = true;
	}

	std::string MaxGameTimeString = Config->GetValue("MaxGameTime");
	if (MaxGameTimeString.length() > 0)
	{
		MaxGameTime = std::stoi(MaxGameTimeString);
	}

	std::string EnableReplayUploadString = Config->GetValue("EnableReplayUpload");
	if (EnableReplayUploadString == "True")
	{
#ifndef ENABLE_CURL
		throw std::logic_error(
				"ERROR: Project was compiled without ENABLE_CURL but ladder manager is configured with EnableReplayUpload=True!"
				" Either set EnableReplayUpload to False in the ladder configuration or run CMake without ENABLE_CURL set to OFF");
#else
        EnableReplayUploads = true;
#endif
	}

	ResultsLogFile = Config->GetValue("ResultsLogFile");

	std::string EnableServerLoginString = Config->GetValue("EnableServerLogin");
	if (EnableServerLoginString == "True")
	{
#ifndef ENABLE_CURL
		throw std::logic_error(
				"ERROR: Project was compiled without ENABLE_CURL but ladder manager is configured with EnableServerLogin=True!"
				" Either set EnableServerLogin to False in the ladder configuration or run CMake with ENABLE_CURL set to ON");
#else
        EnableServerLogin = true;
		ServerLoginAddress = Config->GetValue("ServerLoginAddress");
		ServerUsername = Config->GetValue("ServerUsername");
		ServerPassword = Config->GetValue("ServerPassword");
#endif
	}

	return true;
}

void LadderManager::SaveJsonResult(const BotConfig &Bot1, const BotConfig &Bot2, const std::string  &Map, ResultType Result, int32_t GameTime)
{
	rapidjson::Document ResultsDoc;
	rapidjson::Document OriginalResults;
	rapidjson::Document::AllocatorType& alloc = ResultsDoc.GetAllocator();
	ResultsDoc.SetObject();
	rapidjson::Value ResultsArray(rapidjson::kArrayType);
	std::ifstream ifs(ResultsLogFile.c_str());
	if (ifs)
	{
		std::stringstream buffer;
		buffer << ifs.rdbuf();
		bool parsingFailed = OriginalResults.Parse(buffer.str()).HasParseError();
		if (!parsingFailed && OriginalResults.HasMember("Results"))
		{
			const rapidjson::Value & Results = OriginalResults["Results"];
			for (const auto& val : Results.GetArray())
			{
				rapidjson::Value NewVal;
				NewVal.CopyFrom(val, alloc);
				ResultsArray.PushBack(NewVal, alloc);
			}
		}
	}

	rapidjson::Value NewResult(rapidjson::kObjectType);
	NewResult.AddMember("Bot1", Bot1.BotName, ResultsDoc.GetAllocator());
	NewResult.AddMember("Bot2", Bot2.BotName, alloc);
	switch (Result)
	{
	case Player1Win:
	case Player2Crash:
		NewResult.AddMember("Winner", Bot1.BotName, alloc);
		break;
	case Player2Win:
	case Player1Crash:
		NewResult.AddMember("Winner", Bot2.BotName, alloc);
		break;
	case Tie:
	case Timeout:
		NewResult.AddMember("Winner", "Tie", alloc);
		break;
	case InitializationError:
	case Error:
	case ProcessingReplay:
		NewResult.AddMember("Winner", "Error", alloc);
		break;
	}

	NewResult.AddMember("Map", Map, alloc);
	NewResult.AddMember("Result", GetResultType(Result), alloc);
	NewResult.AddMember("GameTime", GameTime, alloc);
	ResultsArray.PushBack(NewResult, alloc);
	ResultsDoc.AddMember("Results", ResultsArray, alloc);
	std::ofstream ofs(ResultsLogFile.c_str());
	rapidjson::OStreamWrapper osw(ofs);
	rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
	ResultsDoc.Accept(writer);
}


void LadderManager::LoadAgents()
{
	std::string BotConfigFile = Config->GetValue("BotConfigFile");
	if (BotConfigFile.length() < 1)
	{
		return;
	}
	std::ifstream t(BotConfigFile);
	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string BotConfigString = buffer.str();
	rapidjson::Document doc;
	bool parsingFailed = doc.Parse(BotConfigString.c_str()).HasParseError();
	if (parsingFailed)
	{
		std::cerr << "Unable to parse bot config file: " << BotConfigFile << std::endl;
		return;
	}
	if (doc.HasMember("Bots") && doc["Bots"].IsObject())
	{
		const rapidjson::Value & Bots = doc["Bots"];
		for (auto itr = Bots.MemberBegin(); itr != Bots.MemberEnd(); ++itr)
		{
			BotConfig NewBot;
			NewBot.BotName = itr->name.GetString();
			const rapidjson::Value &val = itr->value;

			if (val.HasMember("Race") && val["Race"].IsString())
			{
				NewBot.Race = GetRaceFromString(val["Race"].GetString());
			}
			else
			{
				std::cerr << "Unable to parse race for bot " << NewBot.BotName << std::endl;
				continue;
			}
			if (val.HasMember("Type") && val["Type"].IsString())
			{
				NewBot.Type = GetTypeFromString(val["Type"].GetString());
			}
			else
			{
				std::cerr << "Unable to parse type for bot " << NewBot.BotName << std::endl;
				continue;
			}
			if (NewBot.Type != DefaultBot)
			{
				if (val.HasMember("RootPath") && val["RootPath"].IsString())
				{
					NewBot.RootPath = val["RootPath"].GetString();
					if (NewBot.RootPath.back() != '/')
					{
						NewBot.RootPath += '/';
					}
				}
				else
				{
					std::cerr << "Unable to parse root path for bot " << NewBot.BotName << std::endl;
					continue;
				}
				if (val.HasMember("FileName") && val["FileName"].IsString())
				{
					NewBot.FileName = val["FileName"].GetString();
				}
				else
				{
					std::cerr << "Unable to parse file name for bot " << NewBot.BotName << std::endl;
					continue;
				}
				if (!sc2::DoesFileExist(NewBot.RootPath + NewBot.FileName))
				{
					std::cerr << "Unable to parse bot " << NewBot.BotName << std::endl;
					std::cerr << "Is the path " << NewBot.RootPath << "correct?" << std::endl;
					continue;
				}
				if (val.HasMember("Args") && val["Args"].IsString())
				{
					NewBot.Args = val["Arg"].GetString();
				}
			}
			else
			{
				if (val.HasMember("Difficulty") && val["Difficulty"].IsString())
				{
					NewBot.Difficulty = GetDifficultyFromString(val["Difficulty"].GetString());
				}
			}
			if (EnablePlayerIds)
			{
				NewBot.PlayerId = PlayerIds->GetValue(NewBot.BotName);
				if (NewBot.PlayerId.empty())
				{
					NewBot.PlayerId = GerneratePlayerId(PLAYER_ID_LENGTH);
					PlayerIds->AddValue(NewBot.BotName, NewBot.PlayerId);
					PlayerIds->WriteConfig();
				}
			}
			BotConfigs.insert(std::make_pair(std::string(NewBot.BotName), NewBot));

		}
	}
	if (doc.HasMember("Maps") && doc["Maps"].IsArray())
	{
		const rapidjson::Value & Maps = doc["Maps"];
		for (auto itr = Maps.Begin(); itr != Maps.End(); ++itr)
		{
			MapList.push_back(itr->GetString());
		}
	}
}

std::string LadderManager::RemoveMapExtension(const std::string& filename) {
	size_t lastdot = filename.find_last_of(".");
	if (lastdot == std::string::npos) return filename;
	return filename.substr(0, lastdot);
}

void LadderManager::ChangeBotNames(const std::string ReplayFile, const std::string &Bot1Name, const std::string Bot2Name)
{
	std::string CmdLine = Config->GetValue("ReplayBotRenameProgram");
	if (CmdLine.size() > 0)
	{
		CmdLine = CmdLine + " " + ReplayFile + " " + FIRST_PLAYER_NAME + " " + Bot1Name + " " + SECOND_PLAYER_NAME + " " + Bot2Name;
		StartExternalProcess(CmdLine);
	}
}

bool LadderManager::UploadMime(ResultType result, const Matchup &ThisMatch)
{
#ifndef ENABLE_CURL
	return true;
#else
	std::string ReplayDir = Config->GetValue("LocalReplayDirectory");
	std::string UploadResultLocation = Config->GetValue("UploadResultLocation");
	std::string RawMapName = RemoveMapExtension(ThisMatch.Map);
	std::string ReplayFile;
	if (ThisMatch.Agent2.Type == BotType::DefaultBot)
	{
		ReplayFile = ThisMatch.Agent1.BotName + "v" + GetDifficultyString(ThisMatch.Agent2.Difficulty) + "-" + RawMapName + ".Sc2Replay";
	}
	else
	{
		ReplayFile = ThisMatch.Agent1.BotName + "v" + ThisMatch.Agent2.BotName + "-" + RawMapName + ".Sc2Replay";
	}
	ReplayFile.erase(remove_if(ReplayFile.begin(), ReplayFile.end(), isspace), ReplayFile.end());
	std::string ReplayLoc = ReplayDir + ReplayFile;
	CURL *curl;
	CURLcode res;



	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();
	if (curl) {
		curl_mime *form = NULL;
		curl_mimepart *field = NULL;
		struct curl_slist *headerlist = NULL;
		static const char buf[] = "Expect:";

		/* Create the form */
		form = curl_mime_init(curl);

		/* Fill in the file upload field */
		if (std::fstream(ReplayLoc.c_str()))
		{
			field = curl_mime_addpart(form);
			curl_mime_name(field, "replayfile");
			curl_mime_filedata(field, ReplayLoc.c_str());
		}
		curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "cookies.txt");
		curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "cookies.txt");
		/* Fill in the filename field */
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Bot1Name");
		curl_mime_data(field, ThisMatch.Agent1.BotName.c_str(), CURL_ZERO_TERMINATED);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Bot1Race");
		curl_mime_data(field, std::to_string((int)ThisMatch.Agent1.Race).c_str(), CURL_ZERO_TERMINATED);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Bot2Name");
		curl_mime_data(field, ThisMatch.Agent2.BotName.c_str(), CURL_ZERO_TERMINATED);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Bot2Race");
		curl_mime_data(field, std::to_string((int)ThisMatch.Agent2.Race).c_str(), CURL_ZERO_TERMINATED);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Map");
		curl_mime_data(field, RawMapName.c_str(), CURL_ZERO_TERMINATED);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Result");
		curl_mime_data(field, GetResultType(result).c_str(), CURL_ZERO_TERMINATED);
		/* initialize custom header list (stating that Expect: 100-continue is not
		wanted */
		headerlist = curl_slist_append(headerlist, buf);
		/* what URL that receives this POST */
		curl_easy_setopt(curl, CURLOPT_URL, UploadResultLocation.c_str());

		curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);
		/* Check for errors */
		if (res != CURLE_OK)
		{
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			return false;
		}

		/* always cleanup */
		curl_easy_cleanup(curl);

		/* then cleanup the form */
		curl_mime_free(form);
		/* free slist */
		curl_slist_free_all(headerlist);
		MoveReplayFile(ReplayLoc.c_str(), std::string(ReplayDir + "Uploaded/" + ReplayFile.c_str()).c_str());

		return true;
	}
	return false;
#endif
}

bool LadderManager::LoginToServer()
{
#ifndef ENABLE_CURL
	return true;
#else
	CURL *curl;
	CURLcode res;

	curl_mime *form = NULL;
	curl_mimepart *field = NULL;
	struct curl_slist *headerlist = NULL;
	static const char buf[] = "Expect:";

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();
	if (curl) {
		/* Create the form */
		form = curl_mime_init(curl);
		curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "cookies.txt");
		curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "cookies.txt");
		/* Fill in the filename field */
		field = curl_mime_addpart(form);
		curl_mime_name(field, "username");
		curl_mime_data(field, ServerUsername.c_str(), CURL_ZERO_TERMINATED);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "password");
		curl_mime_data(field, ServerPassword.c_str(), CURL_ZERO_TERMINATED);

		/* initialize custom header list (stating that Expect: 100-continue is not
		wanted */
		headerlist = curl_slist_append(headerlist, buf);
		/* what URL that receives this POST */
		curl_easy_setopt(curl, CURLOPT_URL, ServerLoginAddress.c_str());

		curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);
		/* Check for errors */
		if (res != CURLE_OK)
		{
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			return false;
		}
				

		/* always cleanup */
		curl_easy_cleanup(curl);

		/* then cleanup the form */
		curl_mime_free(form);
		/* free slist */
		curl_slist_free_all(headerlist);
		return true;
	}
	return false;
#endif
}


void LadderManager::RunLadderManager()
{

	LoadAgents();
	std::cout << "Starting with " << MapList.size() << " maps:" << std::endl;
	for (auto &map : MapList)
	{
		std::cout << "* " << map << std::endl;
	}
	std::cout << "Starting with agents: " << std::endl;
	for (auto &Agent : BotConfigs)
	{
		std::cout << Agent.second.BotName << std::endl;
	}
	std::string MatchListFile = Config->GetValue("MatchupListFile");
	MatchupList *Matchups = new MatchupList(MatchListFile);
	Matchups->GenerateMatches(BotConfigs, MapList);
	Matchup NextMatch;
	try
	{
		if (EnableServerLogin)
		{
			LoginToServer();
		}
		while (Matchups->GetNextMatchup(NextMatch))
		{
			ResultType result = ResultType::InitializationError;
			std::cout << std::endl << "Starting " << NextMatch.Agent1.BotName << " vs " << NextMatch.Agent2.BotName << " on " << NextMatch.Map << std::endl;
			int32_t CurrentGameLoop = 0;
			if (NextMatch.Agent1.Type == DefaultBot || NextMatch.Agent2.Type == DefaultBot)
			{
				if (NextMatch.Agent1.Type == DefaultBot)
				{
					// Swap so computer is always player 2
					BotConfig Temp = NextMatch.Agent1;
					NextMatch.Agent1 = NextMatch.Agent2;
					NextMatch.Agent2 = Temp;
				}
				result = StartGameVsDefault(NextMatch.Agent1, NextMatch.Agent2.Race, NextMatch.Agent2.Difficulty, NextMatch.Map, CurrentGameLoop);
			}
			else
			{
				result = StartGame(NextMatch.Agent1, NextMatch.Agent2, NextMatch.Map, CurrentGameLoop);
			}
			std::cout << std::endl << "Game finished with result: " << GetResultType(result) << std::endl;
			if (EnableReplayUploads)
			{
				UploadMime(result, NextMatch);
			}
			if (ResultsLogFile.size() > 0)
			{
				SaveJsonResult(NextMatch.Agent1, NextMatch.Agent2, NextMatch.Map, result, 0);
			}
			Matchups->SaveMatchList();
		}
	
	}
	catch (const std::exception& e)
	{
		std::cout << "Exception in game " << e.what() << std::endl;
		SaveError(NextMatch.Agent1.BotName, NextMatch.Agent2.BotName, NextMatch.Map);
	}
	
}

void LadderManager::SaveError(const std::string &Agent1, const std::string &Agent2, const std::string &Map)
{
	std::string ErrorListFile = Config->GetValue("ErrorListFile");
	if (ErrorListFile == "")
	{
		return;
	}
	std::ofstream ofs(ErrorListFile, std::ofstream::app);
	if (!ofs)
	{
		return;
	}
	ofs << "\"" + Agent1 + "\"vs\"" + Agent2 + "\" " + Map << std::endl;
	ofs.close();
}
