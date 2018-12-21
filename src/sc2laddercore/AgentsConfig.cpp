#include "AgentsConfig.h"
#include "Tools.h"
#include "sc2utils/sc2_manage_process.h"
#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson.h"
#include "document.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <experimental/filesystem>

AgentsConfig::AgentsConfig(LadderConfig *InLadderConfig)
	: PlayerIds(nullptr),
	EnablePlayerIds(false),
	Config(InLadderConfig)

{
	if (Config == nullptr)
	{
		return;
	}
	std::string PlayerIdFile = Config->GetValue("PlayerIdFile");
	if (PlayerIdFile.length() > 0)
	{
		PlayerIds = new LadderConfig(PlayerIdFile);
		EnablePlayerIds = true;
	}
	if (Config->GetValue("BotConfigFile") != "")
	{
		LoadAgents("", Config->GetValue("BotConfigFile"));
	}
	else if (Config->GetValue("BaseBotDirectory") != "")
	{
		ReadBotDirectories(Config->GetValue("BaseBotDirectory"));
	}
}

void AgentsConfig::ReadBotDirectories(const std::string &BaseDirectory)
{
	BotConfigs.clear();
	for (auto& Directory : std::experimental::filesystem::directory_iterator(BaseDirectory))
	{
		if (Directory.status().type() == std::experimental::filesystem::file_type::directory)
		{
			std::string CurrentConfigFile = Directory.path().string() + "/ladderbots.json";
			LoadAgents(Directory.path().string(), CurrentConfigFile);
		}
	}

}

void AgentsConfig::LoadAgents(const std::string &BaseDirectory, const std::string &BotConfigFile)
{
	//	std::string BotConfigFile = Config->GetValue("BotConfigFile");
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
					std::cerr << "Is the path " << NewBot.RootPath << "correct?" << std::endl;
					continue;
				}
				if (val.HasMember("Args") && val["Args"].IsString())
				{
					NewBot.Args = val["Arg"].GetString();
				}
				if (val.HasMember("Debug") && val["Debug"].IsBool()) {
					NewBot.Debug = val["Debug"].GetBool();
				}
			}
			else
			{
				if (val.HasMember("Difficulty") && val["Difficulty"].IsString())
				{
					NewBot.Difficulty = GetDifficultyFromString(val["Difficulty"].GetString());
				}
			}
            /*
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
            */
			BotConfigs.insert(std::make_pair(std::string(NewBot.BotName), NewBot));

		}
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
	std::string BotCheckLocation = Config->GetValue("BotInfoLocation");
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
