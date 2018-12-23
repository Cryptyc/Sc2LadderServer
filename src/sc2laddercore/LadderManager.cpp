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
#include "LadderGame.h"

#ifdef _WIN32
#include "dirent.h"
#else
#include <dirent.h>
#endif

std::mutex PrintThread::_mutexPrint{};



LadderManager::LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv)

	: CoordinatorArgc(InCoordinatorArgc)
	, CoordinatorArgv(inCoordinatorArgv)
	, MaxEloDiff(0)
	, ConfigFile("LadderManager.json")
	, EnableReplayUploads(false)
	, EnableServerLogin(false)
	, Config(nullptr)
{
}

// Used for tests
LadderManager::LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv, const char *InConfigFile)

	: CoordinatorArgc(InCoordinatorArgc)
	, CoordinatorArgv(inCoordinatorArgv)
	, MaxEloDiff(0)
	, ConfigFile(InConfigFile)
	, EnableReplayUploads(false)
	, EnableServerLogin(false)
	, Config(nullptr)
{
}



bool LadderManager::LoadSetup()
{
	delete Config;
	Config = new LadderConfig(ConfigFile);
	if (!Config->ParseConfig())
	{
		PrintThread{} << "Unable to parse config (not found or not valid): " << ConfigFile << std::endl;
		return false;
	}

	std::string EnableReplayUploadString = Config->GetValue("EnableReplayUpload");
	if (EnableReplayUploadString == "True")
	{
		EnableReplayUploads = true;
	}

	ResultsLogFile = Config->GetValue("ResultsLogFile");
	ServerUsername = Config->GetValue("ServerUsername");
	ServerPassword = Config->GetValue("ServerPassword");
	std::string EnableServerLoginString = Config->GetValue("EnableServerLogin");
	if (EnableServerLoginString == "True")
	{
		EnableServerLogin = true;
		ServerLoginAddress = Config->GetValue("ServerLoginAddress");
	}
	BotCheckLocation = Config->GetValue("BotInfoLocation");

	std::string MaxEloDiffStr = Config->GetValue("MaxEloDiff");
	if (MaxEloDiffStr.length() > 0)
	{
		MaxEloDiff = std::stoi(MaxEloDiffStr);
	}

	return true;
}

void LadderManager::SaveJsonResult(const BotConfig &Bot1, const BotConfig &Bot2, const std::string  &Map, GameResult Result)
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
	switch (Result.Result)
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
	NewResult.AddMember("Result", GetResultType(Result.Result), alloc);
	NewResult.AddMember("GameTime", Result.GameLoop, alloc);
	ResultsArray.PushBack(NewResult, alloc);
	ResultsDoc.AddMember("Results", ResultsArray, alloc);
	std::ofstream ofs(ResultsLogFile.c_str());
	rapidjson::OStreamWrapper osw(ofs);
	rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
	ResultsDoc.Accept(writer);
}

bool LadderManager::UploadCmdLine(GameResult result, const Matchup &ThisMatch, const std::string UploadResultLocation)
{
	std::string ReplayDir = Config->GetValue("LocalReplayDirectory");
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

	std::string CurlCmd = "curl";
	CurlCmd = CurlCmd + " -b cookies.txt";
    CurlCmd = CurlCmd + " -F Username=" + ServerUsername;
    CurlCmd = CurlCmd + " -F Password=" + ServerPassword;
    CurlCmd = CurlCmd + " -F Bot1Name=" + ThisMatch.Agent1.BotName;
	CurlCmd = CurlCmd + " -F Bot1Race=" + std::to_string((int)ThisMatch.Agent1.Race);
	CurlCmd = CurlCmd + " -F Bot2Name=" + ThisMatch.Agent2.BotName;
	CurlCmd = CurlCmd + " -F Bot2Race=" + std::to_string((int)ThisMatch.Agent2.Race);
	CurlCmd = CurlCmd + " -F Bot1AvgFrame=" + std::to_string(result.Bot1AvgFrame);
	CurlCmd = CurlCmd + " -F Bot2AvgFrame=" + std::to_string(result.Bot2AvgFrame);
	CurlCmd = CurlCmd + " -F Frames=" + std::to_string(result.GameLoop);
	CurlCmd = CurlCmd + " -F Map=" + RawMapName;
	CurlCmd = CurlCmd + " -F Result=" + GetResultType(result.Result);
	CurlCmd = CurlCmd + " -F replayfile=@" + ReplayLoc;
	CurlCmd = CurlCmd + " " + UploadResultLocation;
	StartExternalProcess(CurlCmd);
	return true;
}


bool LadderManager::LoginToServer()
{
	std::string CurlCmd = "curl";
	CurlCmd = CurlCmd + " -F username=" + ServerUsername;
	CurlCmd = CurlCmd + " -F password=" + ServerPassword;
	CurlCmd = CurlCmd + " -c cookies.txt";
	CurlCmd = CurlCmd + " " + ServerLoginAddress;
	StartExternalProcess(CurlCmd);
	return true;
}

bool LadderManager::IsBotEnabled(std::string BotName)
{
	BotConfig ThisBot;
	if (AgentConfig->FindBot(BotName, ThisBot))
	{
		return ThisBot.Enabled;

	}
	return false;
}
bool LadderManager::IsInsideEloRange(std::string Bot1Name, std::string Bot2Name)
{
	if (MaxEloDiff == 0)
	{
		return true;
	}
	BotConfig Bot1, Bot2;
	if (AgentConfig->FindBot(Bot1Name, Bot1) && AgentConfig->FindBot(Bot2Name, Bot2))
	{
		int32_t EloDiff = abs(Bot1.ELO - Bot2.ELO);
		PrintThread{} << Bot1Name << " ELO: " << Bot1.ELO << " | " << Bot2Name << " ELO: " << Bot2.ELO << " | Diff: " << EloDiff << std::endl;

		if (Bot1.ELO > 0 && Bot2.ELO > 0 && abs(EloDiff) > MaxEloDiff)
		{
			return false;
		}
		return true;

	}
	return true;
}

void LadderManager::DownloadBot(const BotConfig &bot)
{
	std::vector<std::string> arguments;
	std::string argument = " -F Username=" + ServerUsername;
	arguments.push_back(argument);
	argument = " -F Password=" + ServerPassword;
	arguments.push_back(argument);
	argument = " -F BotName=" + bot.BotName;
	arguments.push_back(argument);
	std::string BotZipLocation = Config->GetValue("BaseBotDirectory") + "/" + bot.BotName + ".zip";
    remove(BotZipLocation.c_str());
    argument = " -o " + BotZipLocation;
	arguments.push_back(argument);
    std::string RootPath = bot.RootPath;
    if (RootPath == "")
    {
        RootPath = Config->GetValue("BaseBotDirectory") + "/" + bot.BotName;
    }
	PerformRestRequest(Config->GetValue("BotDownloadPath"), arguments);
    UnzipArchive(BotZipLocation, RootPath);
}

void LadderManager::UploadBot(const BotConfig &bot)
{
	std::string BotZipLocation = Config->GetValue("BaseBotDirectory") + "/" + bot.BotName + ".zip";
	ZipArchive(bot.RootPath, BotZipLocation);
	std::vector<std::string> arguments;
	std::string argument = " -F Username=" + ServerUsername;
	arguments.push_back(argument);
	argument = " -F Password=" + ServerPassword;
	arguments.push_back(argument);
	argument = " -F BotName=" + bot.BotName;
	arguments.push_back(argument);
	argument = " -F BotFile=@" + BotZipLocation;
	arguments.push_back(argument);
	PerformRestRequest(Config->GetValue("BotUploadPath"), arguments);
    SleepFor(1);
    RemoveDirectoryRecursive(bot.RootPath);
    remove(BotZipLocation.c_str());

}

void LadderManager::RunLadderManager()
{
    AgentConfig = new AgentsConfig(Config);
	PrintThread{} << "Loaded agents: " << std::endl;
	for (auto &Agent : AgentConfig->BotConfigs)
	{
		PrintThread{} << Agent.second.BotName << std::endl;
	}

	MapList = Config->GetArray("Maps");
	PrintThread{} << "Starting with " << MapList.size() << " maps:" << std::endl;
	for (auto &map : MapList)
	{
		PrintThread{} << "* " << map << std::endl;
	}
	MatchupList *Matchups = new MatchupList(Config->GetValue("MatchupListFile"), AgentConfig, MapList, Config->GetValue("MatchupGenerator"), Config->GetValue("ServerUsername"), Config->GetValue("ServerPassword"));
	Matchup NextMatch;
	try
	{
		if (EnableServerLogin)
		{
			LoginToServer();
		}
		while (Matchups->GetNextMatchup(NextMatch))
		{
    		GameResult result;
	    	PrintThread{} << "Starting " << NextMatch.Agent1.BotName << " vs " << NextMatch.Agent2.BotName << " on " << NextMatch.Map << std::endl;
            LadderGame CurrentLadderGame(CoordinatorArgc, CoordinatorArgv, Config);
    		if (NextMatch.Agent1.Type == DefaultBot || NextMatch.Agent2.Type == DefaultBot)
	    	{
		    	if (NextMatch.Agent1.Type == DefaultBot)
			    {
			    	// Swap so computer is always player 2
				    BotConfig Temp = NextMatch.Agent1;
				    NextMatch.Agent1 = NextMatch.Agent2;
				    NextMatch.Agent2 = Temp;
			    }
			    result = CurrentLadderGame.StartGameVsDefault(NextMatch.Agent1, NextMatch.Agent2.Race, NextMatch.Agent2.Difficulty, NextMatch.Map);
		    }
		    else
    	    {
                if (Config->GetValue("BotDownloadPath") != "")
                {
                    DownloadBot(NextMatch.Agent1);
                    DownloadBot(NextMatch.Agent2);
                    AgentConfig->ReadBotDirectories(Config->GetValue("BaseBotDirectory"));
                }
                AgentConfig->FindBot(NextMatch.Agent1.BotName, NextMatch.Agent1);
                AgentConfig->FindBot(NextMatch.Agent2.BotName, NextMatch.Agent2);
                if (NextMatch.Agent1.Skeleton || NextMatch.Agent2.Skeleton)
                {
                    PrintThread{} << "Unable to start game " << NextMatch.Agent1.BotName << " vs " << NextMatch.Agent2.BotName << " Unable to download bot";
                    if (NextMatch.Agent1.Skeleton)
                    {
                        PrintThread{} << "Unable to download bot " << NextMatch.Agent1.BotName;
                    }
                    if (NextMatch.Agent2.Skeleton)
                    {
                        PrintThread{} << "Unable to download bot " << NextMatch.Agent2.BotName;
                    }
                    continue;
                }
                       
                if (NextMatch.Bot1Id != "")
                {
                    NextMatch.Agent1.PlayerId = NextMatch.Bot1Id;
                }
                if (NextMatch.Bot2Id != "")
                {
                    NextMatch.Agent2.PlayerId = NextMatch.Bot2Id;
                }
                    
			    result = CurrentLadderGame.StartGame(NextMatch.Agent1, NextMatch.Agent2, NextMatch.Map);
			    if (Config->GetValue("BotUploadPath") != "")
       		    {
                    if (IsValidResult(result))
                    {
                        UploadBot(NextMatch.Agent1);
                        UploadBot(NextMatch.Agent2);
                    }
       		    }
		    }
		    PrintThread{} << "Game finished with result: " << GetResultType(result.Result) << std::endl;
		    if (EnableReplayUploads)
		    {
    			UploadCmdLine(result, NextMatch, Config->GetValue("UploadResultLocation"));
	    	}
    		if (ResultsLogFile.size() > 0)
	    	{
			    SaveJsonResult(NextMatch.Agent1, NextMatch.Agent2, NextMatch.Map, result);
    		}
            Matchups->SaveMatchList();
		}
	}
	catch (const std::exception& e)
	{
		PrintThread{} << "Exception in game " << e.what() << std::endl;
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

bool LadderManager::IsValidResult(GameResult Result)
{
    if (Result.Result == ResultType::Player2Win || Result.Result == ResultType::Player1Win || Result.Result == ResultType::Tie)
    {
        return true;
    }
    return false;
}
