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

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <Windows.h>
#include <curl\curl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>   
#include "types.h"
#include "LadderConfig.h"
#include "LadderManager.h"
#include "MatchupList.h"

const static char *ConfigFile = "LadderManager.conf";

void StartBotProcess(std::string CommandLine)
{
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Executes the given command using CreateProcess() and WaitForSingleObject().
	// Returns FALSE if the command could not be executed or if the exit code could not be determined.
	PROCESS_INFORMATION processInformation = { 0 };
	STARTUPINFO startupInfo = { 0 };
	DWORD exitCode;
	startupInfo.cb = sizeof(startupInfo);
	int nStrBuffer = CommandLine.length() + 50;
	LPSTR cmdLine = const_cast<char *>(CommandLine.c_str());

	// Create the process
	BOOL result = CreateProcess(NULL, cmdLine,
		NULL, NULL, FALSE,
		NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW,
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

std::string LadderManager::GetBotCommandLine(BotConfig AgentConfig, int GamePort, int StartPort, std::string PipeName)
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
	OutCmdLine += " --GamePort " + std::to_string(GamePort) + " --StartPort " + std::to_string(StartPort) + " --LadderServer 127.0.0.1 --PipeName " + PipeName;
	return OutCmdLine;

}

GameState LadderManager::RequestGameState(HANDLE hPipe)
{
	char buffer[1024];
	DWORD dwRead;
	const char *request = "GAMESTATE\0";

	DWORD numWritten;
	WriteFile(hPipe, request, strlen(request), &numWritten, NULL);
	ReadFile(hPipe, buffer, sizeof(buffer) - 1, &dwRead, NULL);
	/* add terminating zero */
	buffer[dwRead] = '\0';
	/* do something with data in buffer */
	std::cout << "recieved " << buffer << std::endl;
	rapidjson::Document doc;
	bool parsingFailed = doc.Parse(buffer).HasParseError();
	GameState CurrentGameState;
	if (parsingFailed)
	{
		std::cerr << "Unable to parse game state information" << std::endl;
		return CurrentGameState;
	}
	if (doc.HasMember("InGame"))
	{
		if (strncmp(doc["InGame"].GetString(), "true", 4) == 0)
		{
			CurrentGameState.IsInGame = true;
		}
		else
		{
			CurrentGameState.IsInGame = false;
		}
	}

	if (doc.HasMember("GameLoop"))
	{
		CurrentGameState.GameLoop = doc["GameLoop"].GetInt();
	}
	if (doc.HasMember("CurrentScore") )
	{
		CurrentGameState.Score = doc["CurrentScore"].GetDouble();
	}
	return CurrentGameState;

}

void LadderManager::RequestExitGame(HANDLE hPipe)
{
	char buffer[1024];
	DWORD dwRead;
	const char *request = "ENDGAME\0"; 
	DWORD numWritten;
	WriteFile(hPipe, request, strlen(request), &numWritten, NULL);

}

HANDLE LadderManager::SetupNamedPipe(std::string PipeName)
{
	HANDLE hPipe;
	std::string ConvertedPipeName = "\\\\.\\pipe\\" + PipeName;
	hPipe = CreateNamedPipe(TEXT(ConvertedPipeName.c_str()),
		PIPE_ACCESS_DUPLEX | PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,   // FILE_FLAG_FIRST_PIPE_INSTANCE is not needed but forces CreateNamedPipe(..) to fail if the pipe already exists...
		PIPE_WAIT,
		1,
		1024 * 16,
		1024 * 16,
		NMPWAIT_USE_DEFAULT_WAIT,
		NULL);
//	ConnectNamedPipe(hPipe, NULL);
	std::cout << "Pipe " + PipeName + " Connected" << std::endl;
	return hPipe;

}

ResultType LadderManager::StartGame(BotConfig Agent1, BotConfig Agent2, std::string Map)
{


	// Add the custom bot, it will control the players.
	sc2::Agent bot1;
	sc2::Agent bot2;
	StartCoordinator();
	coordinator->SetParticipants({
		CreateParticipant(Agent1.Race, &bot1),
		CreateParticipant(Agent2.Race, &bot2),
	});

	// Start the game.
	coordinator->LaunchStarcraft();

	// Step forward the game simulation.
	bool do_break = false;

	coordinator->StartGameCoordinator(Map);

	const sc2::ProcessInfo ps1 = bot1.Control()->GetProcessInfo();
	const sc2::ProcessInfo ps2 = bot2.Control()->GetProcessInfo();

	std::string Agent1Path = GetBotCommandLine(Agent1, ps1.port, ps2.port, "bot1Pipe");
	std::string Agent2Path = GetBotCommandLine(Agent1, ps1.port, ps2.port, "bot2Pipe");
	if (Agent1Path == "" || Agent2Path == "")
	{
		return InitializationError;
	}
	bool TimeoutResult = false;
//	std::thread bot1Thread(StartBotProcess, Agent1Path);
//	std::thread bot2Thread(StartBotProcess, Agent2Path);
	HANDLE bot1Pipe = SetupNamedPipe("bot1Pipe");
	HANDLE bot2Pipe = SetupNamedPipe("bot2Pipe");
	Sleep(30000);
	int CurrentBotRequest = 0;
	bool AllBotsExited = false;
	while (!AllBotsExited)
	{
		GameState Bot1GameState = RequestGameState(bot1Pipe);
		GameState Bot2GameState = RequestGameState(bot2Pipe);
		if (Bot1GameState.IsInGame == false && Bot2GameState.IsInGame == false)
		{
			AllBotsExited = true;
		}
		if (Bot1GameState.GameLoop > MaxGameTime && Bot2GameState.GameLoop > MaxGameTime)
		{
			RequestExitGame(bot1Pipe);
			RequestExitGame(bot2Pipe);
		}
		Sleep(10000);

	}
	std::string ReplayDir = Config->GetValue("LocalReplayDirectory");

	std::string ReplayFile = ReplayDir + Agent1.Name + "v" + Agent2.Name + "-" + Map + ".Sc2Replay";
	bot1.Control()->SaveReplay(ReplayFile);
	coordinator->WaitForAllResponses();
	coordinator->LeaveGame();
	coordinator->WaitForAllResponses();
	if (TimeoutResult)
	{
		return Timeout;
	}
	return ProcessingReplay;
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


void LadderManager::UploadMime(int result, Matchup ThisMatch)
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
		curl_mime_name(field, "Winner");
		curl_mime_data(field, std::to_string(result).c_str(), CURL_ZERO_TERMINATED);
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
			int result = 1;
			StartGame(NextMatch.Agent1, NextMatch.Agent2, NextMatch.Map);
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

	LadderMan = new LadderManager(argc, argv);
	if (LadderMan->LoadSetup())
	{
		LadderMan->RunLadderManager();
	}

}