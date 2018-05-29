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

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <Windows.h>
#include <future>
#include <chrono>
#include <curl/curl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>   
#include <cctype>
#include "Types.h"
#include "LadderConfig.h"
#include "LadderManager.h"
#include "MatchupList.h"

const static char *ConfigFile = "LadderManager.conf";


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


ExitCase GameUpdate(sc2::Connection *client, sc2::Server *server,std::string *botName)
{
	//    std::cout << "Sending Join game request" << std::endl;
	//    sc2::GameRequestPtr Create_game_request = CreateJoinGameRequest();
	//    Client->Send(Create_game_request.get());
	ExitCase CurrentExitCase = ExitCase::InProgress;
	std::cout << "Starting proxy\n" << std::endl;
	bool RequestFound = false;
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
		while (CurrentExitCase == ExitCase::InProgress) {
			SC2APIProtocol::Status CurrentStatus;
			if (!client || !server)
			{
				return ExitCase::ClientTimeout;
			}
			if (client->connection_ == nullptr && RequestFound)
			{
				std::cout << "Client disconnect" << std::endl;
				CurrentExitCase = ExitCase::ClientTimeout;
			}

			if (server->HasRequest())
			{
				const sc2::RequestData request = server->PeekRequest();
				if (request.second && request.second->has_quit()) //Really paranoid here...
				{
					// Intercept leave game and quit requests, we want to keep game alive to save replays
					CurrentExitCase = ExitCase::ClientRequestExit;
					break;
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
						std::cout << "Current status of " << *botName << ": " << status.at(CurrentStatus) << std::endl;
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
						if (currentGameLoop > MAX_GAME_TIME)
						{
							CurrentExitCase = ExitCase::GameTimeout;
						}

					}

				}

				// Send the response back to the client.
				if (server->connections_.size() > 0)
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
					std::cout << "Client timeout" << std::endl;
					CurrentExitCase = ExitCase::ClientTimeout;
				}
			}

		}
		return CurrentExitCase;
	}
	catch (const std::exception& e)
	{
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


void StartBotProcess(std::string CommandLine)
{
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Executes the given command using CreateProcess() and WaitForSingleObject().
	// Returns FALSE if the command could not be executed or if the exit code could not be determined.
	PROCESS_INFORMATION processInformation = { 0 };
	STARTUPINFO startupInfo = { 0 };
	DWORD exitCode;
	startupInfo.cb = sizeof(startupInfo);
	LPSTR cmdLine = const_cast<char *>(CommandLine.c_str());

	// Create the process

	BOOL result = CreateProcess(NULL, cmdLine,
		NULL, NULL, FALSE,
		NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE,
		NULL, NULL, &startupInfo, &processInformation);


	if (!result)
	{
		// CreateProcess() failed
		// Get the error from the system
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);


		// Free resources created by the system
		LocalFree(lpMsgBuf);

		// We failed.
		exitCode = -1;
	}
	else
	{
		// Successfully created the process.  Wait for it to finish.
		WaitForSingleObject(processInformation.hProcess, INFINITE);

		// Get the exit code.
		result = GetExitCodeProcess(processInformation.hProcess, &exitCode);

		// Close the handles.
		CloseHandle(processInformation.hProcess);
		CloseHandle(processInformation.hThread);

		if (!result)
		{
			// Could not get exit code.
			exitCode = -1;
		}


		// We succeeded.
	}
}

BOOL KillSc2Process(DWORD dwProcessId, UINT uExitCode)
{
	DWORD dwDesiredAccess = PROCESS_TERMINATE;
	BOOL  bInheritHandle = FALSE;
	HANDLE hProcess = OpenProcess(dwDesiredAccess, bInheritHandle, dwProcessId);
	if (hProcess == NULL)
		return FALSE;

	BOOL result = TerminateProcess(hProcess, uExitCode);

	CloseHandle(hProcess);

	return result;
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

std::string LadderManager::GetBotCommandLine(BotConfig AgentConfig, int GamePort, int StartPort, bool CompOpp, sc2::Race CompRace, sc2::Difficulty CompDifficulty)
{
	std::string OutCmdLine;
	switch (AgentConfig.Type)
	{
	case pysc2:
	{
		std::string race = GetRaceString(AgentConfig.Race);
		race[0] = std::tolower(race[0]);
		size_t lastdot = AgentConfig.Path.find_last_of(".");
		OutCmdLine = "python -m pysc2.bin.play_vs_agent --agent " + AgentConfig.Path.substr(0, lastdot+1) + AgentConfig.Name + " --host_port " + std::to_string(GamePort) + " --lan_port " + std::to_string(StartPort + 2) + " --map Interloper --agent_race " + race;
		if (CompOpp)
		{
			OutCmdLine += " --ComputerOpponent 1 --ComputerRace " + GetRaceString(CompRace) + " --ComputerDifficulty " + GetDifficultyString(CompDifficulty);
		}
		OutCmdLine += " " + AgentConfig.Args;
		return OutCmdLine;
	}
	case pythonSC2:
	{
		OutCmdLine = "python " + AgentConfig.Path;
		break;
	}
	case BinaryCpp:
	{
		OutCmdLine = AgentConfig.Path;
		break;
	}
	case CommandCenter:
	{
		OutCmdLine = Config->GetValue("CommandCenterPath") + " --ConfigFile " + AgentConfig.Path;
		break;

	}
	case DefaultBot:
	{

	}
	}
	OutCmdLine += " --GamePort " + std::to_string(GamePort) + " --StartPort " + std::to_string(StartPort) + " --LadderServer 127.0.0.1 ";
	if (CompOpp)
	{
		OutCmdLine += " --ComputerOpponent 1 --ComputerRace " + GetRaceString(CompRace) + " --ComputerDifficulty " + GetDifficultyString(CompDifficulty);
	}
	OutCmdLine += " " + AgentConfig.Args;
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

sc2::GameRequestPtr CreateStartGameRequest(std::string MapName, std::vector<sc2::PlayerSetup> players, sc2::ProcessSettings process_settings)
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

ResultType LadderManager::StartGameVsDefault(BotConfig Agent1, sc2::Race CompRace, sc2::Difficulty CompDifficulty, std::string Map)
{
	using namespace std::chrono_literals;
	// Setup server that mimicks sc2.
	std::string Agent1Path = GetBotCommandLine(Agent1, 5677, PORT_START, true, sc2::Race::Random, CompDifficulty);
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

	std::cout << "Starting bot: " << Agent1.Name << std::endl;
	auto bot1ProgramThread = std::thread(StartBotProcess, Agent1Path);
	sc2::SleepFor(1000);

	std::cout << "Monitoring client of: " << Agent1.Name << std::endl;
	auto bot1UpdateThread = std::async(GameUpdate, &client, &server,&Agent1.Name);
	sc2::SleepFor(1000);

	ResultType CurrentResult = ResultType::InitializationError;
	bool GameRunning = true;
	//sc2::ProtoInterface proto_1;
	//std::vector<sc2::PlayerResult> Player1Results;
	Sleep(10000);
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
	std::string ReplayFile = ReplayDir + Agent1.Name + "v" + GetDifficultyString(CompDifficulty) + "-" + RemoveMapExtension(Map) + ".Sc2Replay";
	ReplayFile.erase(remove_if(ReplayFile.begin(), ReplayFile.end(), isspace), ReplayFile.end());

	SaveReplay(&client, ReplayFile);
	if (!SendDataToConnection(&client, CreateLeaveGameRequest().get()))
	{
		std::cout << "CreateLeaveGameRequest failed" << std::endl;
	}

	bot1ProgramThread.join();
	return CurrentResult;
}

ResultType LadderManager::StartGame(BotConfig Agent1, BotConfig Agent2, std::string Map)
{
	
	using namespace std::chrono_literals;
	// Setup server that mimicks sc2.
	std::string Agent1Path = GetBotCommandLine(Agent1, 5677, PORT_START);
	std::string Agent2Path = GetBotCommandLine(Agent2, 5678, PORT_START);
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
	std::cout << "Starting bot: " << Agent1.Name << " with command:"<<std::endl;
	std::cout << Agent1Path << std::endl;
	auto bot1ProgramThread = std::async(&StartBotProcess, Agent1Path);
	sc2::SleepFor(1000);

	std::cout << "Monitoring client of: " << Agent1.Name << std::endl;
	auto bot1UpdateThread = std::async(&GameUpdate, &client, &server, &Agent1.Name);
	sc2::SleepFor(1000);

	std::cout << std::endl << "Starting bot: " << Agent2.Name << " with command:" << std::endl;
	std::cout << Agent2Path << std::endl;
	auto bot2ProgramThread = std::async(&StartBotProcess, Agent2Path);
	sc2::SleepFor(1000);

	std::cout << "Monitoring client of: " << Agent2.Name << std::endl;
	auto bot2UpdateThread = std::async(&GameUpdate, &client2, &server2, &Agent2.Name);
	sc2::SleepFor(1000);

	ResultType CurrentResult = ResultType::InitializationError;
	bool GameRunning = true;
	//sc2::ProtoInterface proto_1;
	sc2::SleepFor(5000);
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
	sc2::SleepFor(1000);
	std::string ReplayDir = Config->GetValue("LocalReplayDirectory");

	std::string ReplayFile = ReplayDir + Agent1.Name + "v" + Agent2.Name + "-" + RemoveMapExtension(Map) + ".SC2Replay";
	ReplayFile.erase(remove_if(ReplayFile.begin(), ReplayFile.end(), isspace), ReplayFile.end());
	if (!SaveReplay(&client, ReplayFile))
	{
		SaveReplay(&client2, ReplayFile);
	}
	sc2::SleepFor(1000);
	if(SendDataToConnection(&client, CreateLeaveGameRequest().get()))
	{
		std::cout << "CreateLeaveGameRequest failed for Client 1." << std::endl;
	}
	sc2::SleepFor(1000);
	if(SendDataToConnection(&client2, CreateLeaveGameRequest().get()))
	{
		std::cout << "CreateLeaveGameRequest failed for Client 2." << std::endl;
	}
	sc2::SleepFor(1000);
	if (server.HasRequest())
	{
		server.SendRequest();
	}
	sc2::SleepFor(1000);
	if (server2.HasRequest())
	{
		server2.SendRequest();
	}
	if (CurrentResult == Player1Crash || CurrentResult == Player2Crash)
	{
		sc2::SleepFor(5000);
		KillSc2Process(Bot1ProcessId, 0);
		KillSc2Process(Bot2ProcessId, 0);
		sc2::SleepFor(5000);
		try
		{
			bot1UpdateThread.wait();
			bot2UpdateThread.wait();

		}
		catch (const std::exception& e)
		{
			std::cout << "Unable to detect end of update thread.  Continuing";
			return CurrentResult;
		}

	}
	return CurrentResult;
}


LadderManager::LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv)

	: coordinator(nullptr)
	, CoordinatorArgc(InCoordinatorArgc)
	, CoordinatorArgv(inCoordinatorArgv)
	, MaxGameTime(0)
{

}

bool LadderManager::LoadSetup()
{

	Config = new LadderConfig(ConfigFile);
	if (!Config->ParseConfig())
	{
		std::cout << "No valid config found at " << ConfigFile << std::endl;
		return false;
	}

	std::string MaxGameTimeString = Config->GetValue("MaxGameTime");
	if (MaxGameTimeString.length() > 0)
	{
		MaxGameTime = std::stoi(MaxGameTimeString);
	}
	return true;
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
			NewBot.Name = itr->name.GetString();
			const rapidjson::Value &    val = itr->value;

			if (val.HasMember("Race") && val["Race"].IsString())
			{
				NewBot.Race = GetRaceFromString(val["Race"].GetString());
			}
			else
			{
				std::cerr << "Unable to parse race for bot " << NewBot.Name << std::endl;
				continue;
			}
			if (val.HasMember("Type") && val["Type"].IsString())
			{
				NewBot.Type = GetTypeFromString(val["Type"].GetString());
			}
			else
			{
				std::cerr << "Unable to parse type for bot " << NewBot.Name << std::endl;
				continue;
			}
			if (val.HasMember("Path") && val["Path"].IsString())
			{
				NewBot.Path = val["Path"].GetString();
				if (NewBot.Type != DefaultBot && !sc2::DoesFileExist(NewBot.Path))
				{
					std::cerr << "Unable to parse bot " << NewBot.Name << std::endl;
					std::cerr << "Is the path " << NewBot.Path << "correct?"<<std::endl;
					continue;
				}
			}
			else
			{
				std::cerr << "Unable to parse path for bot " << NewBot.Name << std::endl;
				continue;
			}
			if (val.HasMember("Difficulty") && val["Difficulty"].IsString())
			{
				NewBot.Difficulty = GetDifficultyFromString(val["Difficulty"].GetString());
			}
			if (val.HasMember("Args") && val["Args"].IsString())
			{
				NewBot.Args = val["Arg"].GetString();
			}
			BotConfigs.insert(std::make_pair(std::string(NewBot.Name), NewBot));

		}
	}
}

void LadderManager::GetMapList()
{
	std::string MapListFile = Config->GetValue("MapListFile");
	std::ifstream file(MapListFile);
	std::string str;
	while (std::getline(file, str))
	{
		MapList.push_back(str);
	}

}

std::string LadderManager::RemoveMapExtension(const std::string& filename) {
	size_t lastdot = filename.find_last_of(".");
	if (lastdot == std::string::npos) return filename;
	return filename.substr(0, lastdot);
}

void LadderManager::UploadMime(ResultType result, Matchup ThisMatch)
{
	std::string ReplayDir = Config->GetValue("LocalReplayDirectory");
	std::string UploadResultLocation = Config->GetValue("UploadResultLocation");
	std::string RawMapName = RemoveMapExtension(ThisMatch.Map);
	std::string ReplayFile;
	if (ThisMatch.Agent2.Type == BotType::DefaultBot)
	{
		ReplayFile = ThisMatch.Agent1.Name + "v" + GetDifficultyString(ThisMatch.Agent2.Difficulty) + "-" + RawMapName + ".Sc2Replay";
	}
	else
	{
		ReplayFile = ThisMatch.Agent1.Name + "v" + ThisMatch.Agent2.Name + "-" + RawMapName + ".Sc2Replay";
	}
	ReplayFile.erase(remove_if(ReplayFile.begin(), ReplayFile.end(), isspace), ReplayFile.end());
	std::string ReplayLoc = ReplayDir + ReplayFile;
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

		/* Fill in the file upload field */
		if (std::fstream(ReplayLoc.c_str()))
		{
			field = curl_mime_addpart(form);
			curl_mime_name(field, "replayfile");
			curl_mime_filedata(field, ReplayLoc.c_str());
		}
		/* Fill in the filename field */
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Bot1Name");
		curl_mime_data(field, ThisMatch.Agent1.Name.c_str(), CURL_ZERO_TERMINATED);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Bot1Race");
		curl_mime_data(field, std::to_string((int)ThisMatch.Agent1.Race).c_str(), CURL_ZERO_TERMINATED);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Bot2Name");
		curl_mime_data(field, ThisMatch.Agent2.Name.c_str(), CURL_ZERO_TERMINATED);
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
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));

		/* always cleanup */
		curl_easy_cleanup(curl);

		/* then cleanup the form */
		curl_mime_free(form);
		/* free slist */
		curl_slist_free_all(headerlist);
		MoveFile(ReplayLoc.c_str(), std::string(ReplayDir + "Uploaded\\" + ReplayFile.c_str()).c_str());


	}

}


void LadderManager::RunLadderManager()
{

	GetMapList();
	LoadAgents();
	std::cout << "Starting with " << MapList.size() << " maps:\r\n";
	for (auto &map : MapList)
	{
		std::cout << "* " << map + "\r\n";
	}
	std::cout << "Starting with agents: \r\n";
	for (auto &Agent : BotConfigs)
	{
		std::cout << Agent.second.Name + "\r\n";
	}
	std::string MatchListFile = Config->GetValue("MatchupListFile");
	MatchupList *Matchups = new MatchupList(MatchListFile);
	Matchups->GenerateMatches(BotConfigs, MapList);
	Matchup NextMatch;
	try
	{

		while (Matchups->GetNextMatchup(NextMatch))
		{
			ResultType result = ResultType::InitializationError;
			std::cout << "Starting " << NextMatch.Agent1.Name << " vs " << NextMatch.Agent2.Name << " on " << NextMatch.Map << " \n";
			if (NextMatch.Agent1.Type == DefaultBot || NextMatch.Agent2.Type == DefaultBot)
			{
				if (NextMatch.Agent1.Type == DefaultBot)
				{
					// Swap so computer is always player 2
					BotConfig Temp = NextMatch.Agent1;
					NextMatch.Agent1 = NextMatch.Agent2;
					NextMatch.Agent2 = Temp;
				}
				result = StartGameVsDefault(NextMatch.Agent1, NextMatch.Agent2.Race, NextMatch.Agent2.Difficulty, NextMatch.Map);
			}
			else
			{
				// Terran bug. Skip where terran is player 2
				/*
				if (NextMatch.Agent2.Race == sc2::Race::Terran)
				{
					continue;
				}
				*/
				result = StartGame(NextMatch.Agent1, NextMatch.Agent2, NextMatch.Map);
			}
			UploadMime(result, NextMatch);
			Matchups->SaveMatchList();
		}
	
	}
	catch (const std::exception& e)
	{
		std::cout << "Exception in game " << e.what() << " \r\n";
		SaveError(NextMatch.Agent1.Name, NextMatch.Agent2.Name, NextMatch.Map);
	}
	
}

void LadderManager::SaveError(std::string Agent1, std::string Agent2, std::string Map)
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
	ofs << "\"" + Agent1 + "\"vs\"" + Agent2 + "\" " + Map + "\r\n";
	ofs.close();
}

int main(int argc, char** argv)
{
	std::cout << "LadderManager started." << std::endl;

	LadderMan = new LadderManager(argc, argv);
	if (LadderMan->LoadSetup())
	{
		LadderMan->RunLadderManager();
	}

	std::cout << "Finished." << std::endl;
}
