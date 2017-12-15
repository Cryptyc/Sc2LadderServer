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

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <Windows.h>
#include <future>
#include <chrono>
#include <curl\curl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>   
#include "types.h"
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


ExitCase GameUpdate(sc2::Connection *client, sc2::Server *server, std::vector<sc2::PlayerResult> *PlayerResults)
{
	//    std::cout << "Sending Join game request" << std::endl;
	//    sc2::GameRequestPtr Create_game_request = CreateJoinGameRequest();
	//    Client->Send(Create_game_request.get());
	ExitCase CurrentExitCase = ExitCase::InProgress;
	std::cout << "Starting proxy" << std::endl;

	clock_t LastRequest = clock();
	while (CurrentExitCase == ExitCase::InProgress) {
		SC2APIProtocol::Status CurrentStatus;
		if (server->HasRequest()) {
			const sc2::RequestData request = server->PeekRequest();
			if(request.second->has_debug() || request.second->has_create_game())
			{
				// Drop these request types
				continue;
			}
			else if(request.second->has_leave_game() || request.second->has_quit())
			{
				// Intercept leave game and quit requests, we want to keep game alive to save replays
				CurrentExitCase = ExitCase::ClientRequestExit;
				break;
			}
			server->SendRequest(client->connection_);

			// Block for sc2's response then queue it.
			SC2APIProtocol::Response* response = nullptr;
			client->Receive(response, 100000);
			if (response != nullptr)
			{
				CurrentStatus = response->status();
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

			server->QueueResponse(client->connection_, response);

			// Send the response back to the client.
			server->SendResponse();
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

bool ProcessObservationResponse(SC2APIProtocol::ResponseObservation Response, std::vector<sc2::PlayerResult> *PlayerResults)
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

std::string LadderManager::GetBotCommandLine(BotConfig AgentConfig, int GamePort, int StartPort)
{
	std::string OutCmdLine;
	switch (AgentConfig.Type)
	{
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
	}
	OutCmdLine += " --GamePort " + std::to_string(GamePort) + " --StartPort " + std::to_string(StartPort) + " --LadderServer 127.0.0.1 ";
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
sc2::GameRequestPtr LadderManager::CreateLeaveGameRequest()
{
	sc2::ProtoInterface proto;
	sc2::GameRequestPtr request = proto.MakeRequest();
	request->mutable_leave_game();

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
	client->Send(ObservationRequest.get());
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
	sc2::StartProcess(process_settings.process_path,
	{ "-listen", "127.0.0.1",
		"-port", "5679",
		"-displayMode", "0",
		"-dataVersion", process_settings.data_version }
	);

	sc2::StartProcess(process_settings.process_path,
	{ "-listen", "127.0.0.1",
		"-port", "5680",
		"-displayMode", "0",
		"-dataVersion", process_settings.data_version }
	);
	sc2::SleepFor(10000);

	// Connect to running sc2 process.
	sc2::Connection client;
	client.Connect("127.0.0.1", 5679);
	sc2::Connection client2;
	client2.Connect("127.0.0.1", 5680);


	std::vector<sc2::PlayerSetup> Players;
	Players.push_back(sc2::PlayerSetup(sc2::PlayerType::Participant, sc2::Race::Terran, nullptr, sc2::Easy));
	Players.push_back(sc2::PlayerSetup(sc2::PlayerType::Participant, sc2::Race::Terran, nullptr, sc2::Easy));
	sc2::GameRequestPtr Create_game_request = CreateStartGameRequest("Odyssey LE", Players, process_settings);
	client.Send(Create_game_request.get());
	SC2APIProtocol::Response* create_response = nullptr;
	if (client.Receive(create_response, 100000))
	{
		std::cout << "Recieved create game response " << create_response->data().DebugString() << std::endl;
		ProcessResponse(create_response->create_game());
	}
	auto bot1ProgramThread = std::thread(StartBotProcess, Agent1Path);
	auto bot2ProgramThread = std::thread(StartBotProcess, Agent2Path);
	std::vector<sc2::PlayerResult> Player1Results;
	std::vector<sc2::PlayerResult> Player2Results;

	auto bot1UpdateThread = std::async(&GameUpdate, &client, &server, &Player1Results);
	auto bot2UpdateThread = std::async(&GameUpdate, &client2, &server2, &Player2Results);
	ResultType CurrentResult = ResultType::InitializationError;
	bool GameRunning = true;
	sc2::ProtoInterface proto_1;

	Sleep(10000);
	while (GameRunning)
	{

		auto update1status = bot2UpdateThread.wait_for(1s);
		auto update2status = bot2UpdateThread.wait_for(0ms);
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
			ExitCase BotExitCase = bot1UpdateThread.get();
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
	}
	if (CurrentResult == ResultType::ProcessingReplay)
	{
		CurrentResult = GetPlayerResults(&client);
	}

	std::string ReplayDir = Config->GetValue("LocalReplayDirectory");
	std::string ReplayFile = ReplayDir + Agent1.Name + "v" + Agent2.Name + "-" + Map + ".Sc2Replay";
	client.Send(CreateLeaveGameRequest().get());
	client2.Send(CreateLeaveGameRequest().get());

	bot2ProgramThread.join();
	bot1ProgramThread.join();
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
		std::cerr << "Unable to parse bot config file" << std::endl;
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
			}
			else
			{
				std::cerr << "Unable to parse path for bot " << NewBot.Name << std::endl;
				continue;
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


void LadderManager::UploadMime(ResultType result, Matchup ThisMatch)
{
	std::string ReplayDir = Config->GetValue("LocalReplayDirectory");
	std::string UploadResultLocation = Config->GetValue("UploadResultLocation");
	std::string ReplayFile = ThisMatch.Agent1.Name + "v" + ThisMatch.Agent2.Name + "-" + ThisMatch.Map + ".Sc2Replay";
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
		curl_mime_data(field, ThisMatch.Map.c_str(), CURL_ZERO_TERMINATED);
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
			std::cout << "Starting " << NextMatch.Agent1.Name << " vs " << NextMatch.Agent2.Name << " on " << NextMatch.Map << " \n";
			ResultType result = StartGame(NextMatch.Agent1, NextMatch.Agent2, NextMatch.Map);
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

void LadderManager::StartCoordinator()
{
	if (coordinator != nullptr)
	{
		delete coordinator;
		Sleep(10000);

	}
	coordinator = new sc2::Coordinator();
	coordinator->LoadSettings(CoordinatorArgc, CoordinatorArgv);
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