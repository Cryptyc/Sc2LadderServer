#include "AgentsConfig.h"
#include "Tools.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_scan_directory.h"
#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson.h"
#include "document.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

AgentsConfig::AgentsConfig(LadderConfig *InLadderConfig)
    : Config(InLadderConfig),
      PlayerIds(nullptr),
      EnablePlayerIds(false)
{
	if (Config == nullptr)
	{
		return;
	}
	std::string PlayerIdFile = Config->GetStringValue("PlayerIdFile");
	if (PlayerIdFile.length() > 0)
	{
		PlayerIds = new LadderConfig(PlayerIdFile);
        PlayerIds->ParseConfig();
		EnablePlayerIds = true;
	}
	if (Config->GetStringValue("BotConfigFile") != "")
	{
		LoadAgents("", Config->GetStringValue("BotConfigFile"));
	}
	else if (Config->GetStringValue("BaseBotDirectory") != "")
	{
		ReadBotDirectories(Config->GetStringValue("BaseBotDirectory"));
	}

}

void AgentsConfig::ReadBotDirectories(const std::string &BaseDirectory)
{
	std::vector<std::string> directories;
	sc2::scan_directory(BaseDirectory.c_str(), directories, true, true);
	for (const std::string  &Directory : directories)
	{
		std::string CurrentConfigFile = Directory + "/ladderbots.json";
		LoadAgents(Directory, CurrentConfigFile);
	}

}

void AgentsConfig::LoadAgents(const std::string &BaseDirectory, const std::string &BotConfigFile)
{
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
            if (val.HasMember("RootPath") && val["RootPath"].IsString())
            {
                NewBot.RootPath = BaseDirectory;
                if (NewBot.RootPath.back() != '/')
                {
                    NewBot.RootPath += '/';
                }
                NewBot.RootPath = NewBot.RootPath + val["RootPath"].GetString();
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
                std::cerr << "Is the path " << NewBot.RootPath << " correct?" << std::endl;
                continue;
            }
            const std::string dataLocation = NewBot.RootPath + "/data";
            MakeDirectory(dataLocation); // If a directory exists this just fails and does nothing.
            if (val.HasMember("Args") && val["Args"].IsString())
            {
                NewBot.Args = val["Args"].GetString();
            }
            if (val.HasMember("Debug") && val["Debug"].IsBool()) {
                NewBot.Debug = val["Debug"].GetBool();
            }

            if (val.HasMember("SurrenderPhrase") && val["SurrenderPhrase"].IsString()) {
                NewBot.SurrenderPhrase = val["SurrenderPhrase"].GetString();
            }

            if (EnablePlayerIds)
            {
                NewBot.PlayerId = PlayerIds->GetStringValue(NewBot.BotName);
                if (NewBot.PlayerId.empty())
                {
                    NewBot.PlayerId = GerneratePlayerId(PLAYER_ID_LENGTH);
                    PlayerIds->AddValue(NewBot.BotName, NewBot.PlayerId);
                    PlayerIds->WriteConfig();
                }
            }

            std::string OutCmdLine = "";
            switch (NewBot.Type)
            {
            case Python:
            {
                OutCmdLine = Config->GetStringValue("PythonBinary") + " " + NewBot.FileName;
                break;
            }
            case Wine:
            {
                OutCmdLine = "wine " + NewBot.FileName;
                break;
            }
            case Mono:
            {
                OutCmdLine = "mono " + NewBot.FileName;
                break;
            }
            case DotNetCore:
            {
                OutCmdLine = "dotnet " + NewBot.FileName;
                break;
            }
            case CommandCenter:
            {
                OutCmdLine = Config->GetStringValue("CommandCenterPath") + " --ConfigFile " + NewBot.FileName;
                break;
            }
            case BinaryCpp:
            {
                OutCmdLine = NewBot.RootPath + NewBot.FileName;
                break;
            }
            case Java:
            {
                OutCmdLine = "java -jar " + NewBot.FileName;
                break;
            }
            case NodeJS:
            {
                OutCmdLine = Config->GetStringValue("NodeJSBinary") + " " + NewBot.FileName;
                break;
            }
            }

            if (NewBot.Args != "")
            {
                OutCmdLine += " " + NewBot.Args;
            }

            NewBot.executeCommand = OutCmdLine;
            BotConfig SavedBot;
            if (FindBot(NewBot.BotName, SavedBot))
            {
                NewBot.CheckSum = SavedBot.CheckSum;
                BotConfigs[NewBot.BotName] = NewBot;
            }
            else
            {
                BotConfigs.insert(std::make_pair(std::string(NewBot.BotName), NewBot));
            }

        }
	}

}

void AgentsConfig::SaveBotConfig(const BotConfig& Agent)
{
    BotConfig SavedBot;
    if (FindBot(Agent.BotName, SavedBot))
    {
        BotConfigs[Agent.BotName] = Agent;
    }
    else
    {
        BotConfigs.insert(std::make_pair(std::string(Agent.BotName), Agent));
    }
}


bool AgentsConfig::FindBot(const std::string &BotName, BotConfig &ReturnBot)
{
	auto ThisBot = BotConfigs.find(BotName);
	if (ThisBot != BotConfigs.end())
	{
		ReturnBot = ThisBot->second;
		return true;
	}
	return false;
}

bool AgentsConfig::CheckDiactivatedBots()
{
	std::string BotCheckLocation = Config->GetStringValue("BotInfoLocation");
	if (BotCheckLocation.empty())
	{
		return false;
	}
	std::vector<std::string> arguments;
	std::string result = PerformRestRequest(BotCheckLocation, arguments);
	if (result.empty())
	{
		return false;
	}
	rapidjson::Document doc;
	bool parsingFailed = doc.Parse(result.c_str()).HasParseError();
	if (parsingFailed)
	{
		std::cerr << "Unable to parse incoming bot config: " << BotCheckLocation << std::endl;
		return false;
	}
	if (doc.HasMember("Bots") && doc["Bots"].IsArray())
	{
		const rapidjson::Value & Units = doc["Bots"];
		for (const auto& val : Units.GetArray())
		{
			if (val.HasMember("name") && val["name"].IsString())
			{
				auto ThisBot = BotConfigs.find(val["name"].GetString());
				if (ThisBot != BotConfigs.end())
				{
					if (val.HasMember("deactivated") && val.HasMember("deleted") && val["deactivated"].IsBool() && val["deleted"].IsBool())
					{
						if ((val["deactivated"].GetBool() || val["deleted"].GetBool()) && ThisBot->second.Enabled)
						{
							// Set bot to disabled
							PrintThread{} << "Deactivating bot " << ThisBot->second.BotName << std::endl;
							ThisBot->second.Enabled = false;

						}
						else if (val["deactivated"].GetBool() == false && val["deleted"].GetBool() == false && ThisBot->second.Enabled == false)
						{
							// reenable a bot
							PrintThread{} << "Activating bot " << ThisBot->second.BotName;
							ThisBot->second.Enabled = true;
						}
					}
					if (val.HasMember("elo") && val["elo"].IsString())
					{
						ThisBot->second.ELO = std::stoi(val["elo"].GetString());
					}
				}

			}
		}
		return true;
	}
	return false;
}

std::string AgentsConfig::GerneratePlayerId(size_t Length)
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
