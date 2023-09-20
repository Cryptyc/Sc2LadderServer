#include "sc2api/sc2_score.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2api/sc2_game_settings.h"
#include "sc2api/sc2_args.h"
#include "civetweb.h"
#include <exception>
#include <numeric>

#define RAPIDJSON_HAS_STDSTRING 1

#include "document.h"
#include "ostreamwrapper.h"
#include "prettywriter.h"
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <future>
#include <chrono>
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

    EnableReplayUploads = Config->GetBoolValue("EnableReplayUpload");

	ResultsLogFile = Config->GetStringValue("ResultsLogFile");
	ServerUsername = Config->GetStringValue("ServerUsername");
	ServerPassword = Config->GetStringValue("ServerPassword");
	std::string EnableServerLoginString = Config->GetStringValue("EnableServerLogin");
	if (EnableServerLoginString == "True")
	{
		EnableServerLogin = true;
		ServerLoginAddress = Config->GetStringValue("ServerLoginAddress");
	}
	BotCheckLocation = Config->GetStringValue("BotInfoLocation");

	std::string MaxEloDiffStr = Config->GetStringValue("MaxEloDiff");
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
    case ResultType::Player1Win:
    case ResultType::Player2Crash:
    case ResultType::Player2TimeOut:
		NewResult.AddMember("Winner", Bot1.BotName, alloc);
		break;
    case ResultType::Player2Win:
    case ResultType::Player1Crash:
    case ResultType::Player1TimeOut:
		NewResult.AddMember("Winner", Bot2.BotName, alloc);
		break;
    case ResultType::Tie:
    case ResultType::Timeout:
		NewResult.AddMember("Winner", "Tie", alloc);
		break;
    case ResultType::InitializationError:
    case ResultType::Error:
    case ResultType::ProcessingReplay:
		NewResult.AddMember("Winner", "Error", alloc);
		break;
	}

	NewResult.AddMember("Map", Map, alloc);
	NewResult.AddMember("Result", GetResultType(Result.Result), alloc);
	NewResult.AddMember("GameTime", Result.GameLoop, alloc);
	NewResult.AddMember("TimeStamp", Result.TimeStamp, alloc);
	ResultsArray.PushBack(NewResult, alloc);
	ResultsDoc.AddMember("Results", ResultsArray, alloc);
	std::ofstream ofs(ResultsLogFile.c_str());
	rapidjson::OStreamWrapper osw(ofs);
	rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
	ResultsDoc.Accept(writer);
}

bool LadderManager::UploadCmdLine(GameResult result, const Matchup &ThisMatch, const std::string UploadResultLocation)
{
	std::string ReplayDir = Config->GetStringValue("LocalReplayDirectory");
	std::string RawMapName = RemoveMapExtension(ThisMatch.Map);
	std::string ReplayFile = ThisMatch.ReplayName() + ".Sc2Replay";
	ReplayFile.erase(remove_if(ReplayFile.begin(), ReplayFile.end(), isspace), ReplayFile.end());
	std::string ReplayLoc = ReplayDir + ReplayFile;

    std::vector<std::string> arguments;
    std::string  argument = " -b cookies.txt";
    arguments.push_back(argument);
    argument = " -F Username=" + ServerUsername;
    arguments.push_back(argument);
    argument = " -F Password=" + ServerPassword;
    arguments.push_back(argument);

    int botCounter = 1;
    for (const BotConfig &bot : ThisMatch.Agents) { // todo use cpp23 enumerate (if it happen...)
        argument = " -F Bot" + std::to_string(botCounter) + "Name=" + bot.BotName;
        arguments.push_back(argument);

        // todo update avgframe fields
        argument = " -F Bot1AvgFrame=" + std::to_string(result.Bot1AvgFrame);
        arguments.push_back(argument);
        argument = " -F Bot2AvgFrame=" + std::to_string(result.Bot2AvgFrame);
        arguments.push_back(argument);
        botCounter += 1;
    }

    argument = " -F Frames=" + std::to_string(result.GameLoop);
    arguments.push_back(argument);
    argument = " -F Map=" + RawMapName;
    arguments.push_back(argument);
    argument = " -F Result=" + GetResultType(result.Result);
    arguments.push_back(argument);
    argument = " -F replayfile=@" + ReplayLoc;
    arguments.push_back(argument);
    PerformRestRequest(UploadResultLocation, arguments);

	return true;
}


bool LadderManager::LoginToServer()
{
    std::vector<std::string> arguments;
    std::string  argument = " -c cookies.txt";
    arguments.push_back(argument);
    argument = " -F Username=" + ServerUsername;
    arguments.push_back(argument);
    argument = " -F Password=" + ServerPassword;
    arguments.push_back(argument);
    PerformRestRequest(ServerLoginAddress, arguments);
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

bool LadderManager::DownloadBot(const std::string& BotName, const std::string& Checksum, bool Data)
{
    constexpr int DownloadRetrys = 3;
    std::string RootPath = Config->GetStringValue("BaseBotDirectory") + "/" + BotName;
    if (Data)
    {
        RootPath += "/data";
    }
    RemoveDirectoryRecursive(RootPath);
    for (int RetryCount = 0; RetryCount < DownloadRetrys; RetryCount++)
    {
        std::vector<std::string> arguments;
        std::string argument = " -F Username=" + ServerUsername;
        arguments.push_back(argument);
        argument = " -F Password=" + ServerPassword;
        arguments.push_back(argument);
        argument = " -F BotName=" + BotName;
        arguments.push_back(argument);
        if (Data)
        {
            argument = " -F Data=1";
            arguments.push_back(argument);
        }
        std::string BotZipLocation = Config->GetStringValue("BaseBotDirectory") + "/" + BotName + ".zip";
        remove(BotZipLocation.c_str());
        argument = " -o " + BotZipLocation;
        arguments.push_back(argument);
        PerformRestRequest(Config->GetStringValue("BotDownloadPath"), arguments);
        std::string BotMd5 = GenerateMD5(BotZipLocation);
        PrintThread{} << "Download checksum: " << Checksum << " Bot checksum: " << BotMd5 << std::endl;

        if (BotMd5.compare(Checksum) == 0)
        {
            UnzipArchive(BotZipLocation, RootPath);
            remove(BotZipLocation.c_str());
            return true;
        }
        remove(BotZipLocation.c_str());
    }
    return false;
}

bool LadderManager::VerifyUploadRequest(const std::string &UploadResult)
{
    rapidjson::Document doc;
    bool parsingFailed = doc.Parse(UploadResult.c_str()).HasParseError();
    if (parsingFailed)
    {
        std::cerr << "Unable to parse incoming upload result: " << UploadResult << std::endl;
        return false;
    }
    if (doc.HasMember("result") && doc["result"].IsBool() && doc["result"].GetBool())
    {
        return true;
    }
    if (doc.HasMember("error") && doc["error"].IsBool())
    {
        PrintThread{} << "Error uploading bot: " << doc["error"].GetString() << std::endl;
    }
    return false;
}


bool LadderManager::UploadBot(const BotConfig &bot, bool Data)
{
    std::string BotZipLocation = Config->GetStringValue("BaseBotDirectory") + "/" + bot.BotName + ".zip";
    std::string InputLocation = bot.RootPath;
    if (Data)
    {
        InputLocation += "/data";
    }
    ZipArchive(InputLocation, BotZipLocation);
    std::string BotMd5 = GenerateMD5(BotZipLocation);
    std::vector<std::string> arguments;
	std::string argument = " -F Username=" + ServerUsername;
	arguments.push_back(argument);
	argument = " -F Password=" + ServerPassword;
	arguments.push_back(argument);
	argument = " -F BotName=" + bot.BotName;
	arguments.push_back(argument);
    argument = " -F Checksum=" + BotMd5;
    arguments.push_back(argument);
    if (Data)
    {
        argument = " -F Data=1";
        arguments.push_back(argument);
    }
    argument = " -F BotFile=@" + BotZipLocation;
	arguments.push_back(argument);
    constexpr int UploadRetrys = 3;
    for (int RetryCount = 0; RetryCount < UploadRetrys; RetryCount++)
    {
       	std::string UploadResult = PerformRestRequest(Config->GetStringValue("BotUploadPath"), arguments);
        if (VerifyUploadRequest(UploadResult))
        {
            SleepFor(1);
            RemoveDirectoryRecursive(InputLocation);
            remove(BotZipLocation.c_str());
            return true;
        }
    }
    RemoveDirectoryRecursive(InputLocation);
    remove(BotZipLocation.c_str());
    return false;
}

bool LadderManager::GetBot(BotConfig& Agent, const std::string& BotChecksum, const std::string& DataChecksum)
{
    if (!BotChecksum.empty() && BotChecksum != Agent.CheckSum)
    {
        if (!DownloadBot(Agent.BotName, BotChecksum, false))
        {
            PrintThread{} << "Bot download failed, skipping game" << std::endl;
            LogNetworkFailure(Agent.BotName, "Download");
            Agent.CheckSum = "";
            return false;
        }
    }
    if (!DataChecksum.empty())
    {
        if (!DownloadBot(Agent.BotName, DataChecksum, true))
        {
            PrintThread{} << "Bot data download failed, skipping game" << std::endl;
            LogNetworkFailure(Agent.BotName, "Download Data");
            return false;
        }
    }
    else
    {
        const std::string DataLocation = Config->GetStringValue("BaseBotDirectory") + "/" + Agent.BotName + "/data";
        MakeDirectory(DataLocation);
    }
    return true;
}

bool LadderManager::ConfigureBot(BotConfig& Agent,
                                 const std::string& BotId,
                                 const std::string& Checksum,
                                 const std::string& DataChecksum) {

    if (!Config->GetStringValue("BotDownloadPath").empty()) {
        if (Checksum.empty()) {
            PrintThread{} << "No bot checksum found. Skipping game" << std::endl;
            return false;
        }

        if (!GetBot(Agent, Checksum, DataChecksum)) {
            return false;
        }

        const std::string BotLocation = Config->GetStringValue(
                "BaseBotDirectory"
        ) + "/" + Agent.BotName;

        AgentConfig->LoadAgents(BotLocation,
                                BotLocation + "/ladderbots.json");
    }

    AgentConfig->FindBot(Agent.BotName, Agent);

    if (Agent.Skeleton) {
        PrintThread{} << "Unable to download bot " << Agent.BotName << std::endl;
        return false;
    }

    if (BotId != "")
    {
        Agent.PlayerId = BotId;
    }

    Agent.CheckSum = Checksum;
    AgentConfig->SaveBotConfig(Agent);

    return true;

}

void LadderManager::RunLadderManager()
{
	AgentConfig = new AgentsConfig(Config);

	auto *Matchups = new MatchupList(
            Config->GetStringValue("MatchupListFile"),
            AgentConfig,
            Config->GetArrayValue("Maps"),
            getSC2Path(),
            Config->GetStringValue("MatchupGenerator"),
            Config->GetStringValue("ServerUsername"),
            Config->GetStringValue("ServerPassword")
    );

    PrintThread{} << "Initialization finished." << std::endl << std::endl;
	Matchup NextMatch;

	try {
		if (EnableServerLogin) {
			LoginToServer();
		}

		while (Matchups->GetNextMatchup(NextMatch)) {
    		GameResult result;

			PrintThread{} << "Starting " << NextMatch.GameName()
                            << " on " << NextMatch.Map << std::endl;

            LadderGame CurrentLadderGame(CoordinatorArgc,
                                         CoordinatorArgv,
                                         Config);

            for (int i = 0; i < NextMatch.Agents.size(); i++) {
                std::string botId = NextMatch.BotIds.size() > i ? NextMatch.BotIds[i] : std::string();
                std::string botChecksum = NextMatch.BotChecksums.size() > i ? NextMatch.BotChecksums[i] : std::string();
                std::string botDataChecksum = NextMatch.BotDataChecksums.size() > i ? NextMatch.BotDataChecksums[i] : std::string();

                if (!ConfigureBot(NextMatch.Agents[i],
                                  botId,
                                  botChecksum,
                                  botDataChecksum)) {
                    PrintThread{} << "Error configuring bot "
                        << NextMatch.Agents[i].BotName
                        << " Skipping game" << std::endl;
                    break;
                }
            }

            // todo replace with vector argument
			result = CurrentLadderGame.StartGame(NextMatch.Agents[0],
                                                 NextMatch.Agents[1],
                                                 NextMatch.Map);

			if (!Config->GetStringValue("BotUploadPath").empty()) {
                for (auto & Agent : NextMatch.Agents) {
                    if(!UploadBot(Agent, true)) {
                        LogNetworkFailure(Agent.BotName, "Upload");
                    }
                }
       		}

            PrintThread{} << "Game finished with result: "
                << GetResultType(result.Result)
                << std::endl << std::endl;

		    if (EnableReplayUploads) {
    			UploadCmdLine(result,
                              NextMatch,
                              Config->GetStringValue("UploadResultLocation"));
	    	}

    		if (!ResultsLogFile.empty()) {
			    SaveJsonResult(NextMatch.Agents[0],
                               NextMatch.Agents[1],
                               NextMatch.Map,
                               result);
    		}

            Matchups->SaveMatchList();
		}
	}
	catch (const std::exception& e)
	{
		PrintThread{} << "Exception in game " << NextMatch.GameName()
            << " : " << e.what() << std::endl;
        SaveError(NextMatch.AgentNames(), NextMatch.Map);
	}
}

void LadderManager::LogNetworkFailure(const std::string &AgentName, const std::string &Action)
{
    std::string ErrorListFile = Config->GetStringValue("ErrorListFile");
    if (ErrorListFile.empty())
    {
        return;
    }
    std::ofstream ofs(ErrorListFile, std::ofstream::app);
    if (!ofs)
    {
        return;
    }
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);

    ofs << std::put_time(&tm, "%d-%m-%Y %H-%M-%S")
        << ": " << AgentName + " Failed to " << Action  << std::endl;
    ofs.close();
}

void LadderManager::SaveError(const std::vector<std::string> &AgentNames,
                              const std::string &Map)
{
	std::string ErrorListFile = Config->GetStringValue("ErrorListFile");
	if (ErrorListFile.empty()) {
		return;
	}

	std::ofstream ofs(ErrorListFile, std::ofstream::app);
	if (!ofs) {
		return;
	}

    auto agents = std::accumulate(
            std::begin(AgentNames),
            std::end(AgentNames),
            std::string(" "),
            [](const std::string& a, const std::string& b) {
                return a + "vs" + b;
            });

	ofs << "\"" + agents + "\" " + Map << std::endl;
	ofs.close();
}

std::string LadderManager::getSC2Path() const
{
	sc2::ProcessSettings process_settings;
	sc2::GameSettings game_settings;
	sc2::ParseSettings(CoordinatorArgc, CoordinatorArgv, process_settings, game_settings);
    if (process_settings.process_path.empty())
    {
        PrintThread{} << "Error: Could not detect StarCraft II executable." << std::endl;
    }
    if (!sc2::DoesFileExist(process_settings.process_path))
    {
        PrintThread{} << "Error: Could not detect StarCraft II executable at " << process_settings.process_path << std::endl;
    }

	return process_settings.process_path;
}
