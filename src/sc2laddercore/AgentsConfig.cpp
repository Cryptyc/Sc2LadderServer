#include "AgentsConfig.h"
#include "Tools.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_scan_directory.h"
#define RAPIDJSON_HAS_STDSTRING 1
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
                NewBot.RootPath = BaseDirectory + val["RootPath"].GetString();
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
                    NewBot.PlayerId = GeneratePlayerId(PLAYER_ID_LENGTH);
                    PlayerIds->AddValue(NewBot.BotName, NewBot.PlayerId);
                    PlayerIds->WriteConfig();
                }
            }

            std::string OutCmdLine;

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
    if (FindBot(Agent.BotName, SavedBot)) {
        BotConfigs[Agent.BotName] = Agent;
    } else {
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


std::string AgentsConfig::GeneratePlayerId(size_t Length)
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

std::vector<BotConfig> AgentsConfig::Bots() {
    std::vector<BotConfig> bots = std::vector<BotConfig>();
    for (const auto &Bot : this->BotConfigs) {
        bots.push_back(Bot.second);
    }

    return bots;
}
