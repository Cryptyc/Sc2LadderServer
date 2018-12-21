
#include "sc2lib/sc2_lib.h"
#include "sc2api/sc2_api.h"
#include "sc2utils/sc2_manage_process.h"

#include <iostream>
#include <sstream>
#include <fstream>      

#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <regex>
#define RAPIDJSON_HAS_STDSTRING 1

#include "rapidjson.h"

#include "Types.h"
#include "LadderManager.h"
#include "MatchupList.h"
#include "Tools.h"
#include "AgentsConfig.h"

#define URL_REGEX 

MatchupList::MatchupList(const std::string &inMatchupListFile, AgentsConfig *InAgentConfig, std::vector<std::string> MapList, const std::string &GeneratorType, const std::string &InServerUsername, const std::string &InServerPassword)
	: MatchupListFile(inMatchupListFile)
	, ServerUsername(InServerUsername)
	, ServerPassword(InServerPassword)
	, AgentConfig(InAgentConfig)
{
	MatchUpProcess = GetMatchupListTypeFromString(GeneratorType);
	if(MatchUpProcess == MatchupListType::None)
	{
		PrintThread{} << "Unknown Matchuplist generator type: " + GeneratorType + " Should be either \"file\" or \"url\"" << std::endl;
		return;
	}
	GenerateMatches(MapList);
}


bool MatchupList::GenerateMatches(std::vector<std::string> Maps)
{
	Matchups.clear();
    if (MatchUpProcess == MatchupListType::URL)
    {
        return true;
    }
	if (!LoadMatchupList())
	{
		PrintThread{} << "Could not load MatchupList from file. Generating new one.." << std::endl;
		for (const auto &Agent1 : AgentConfig->BotConfigs)
		{
			for (const auto &Agent2 : AgentConfig->BotConfigs)
			{
				if (Agent1.first == Agent2.first)
				{
					continue;
				}
				for (std::string map : Maps)
				{
					Matchup NextMatchup(Agent1.second, Agent2.second, map);
					Matchups.push_back(NextMatchup);
				}
			}
		}
		srand(time(0));
		std::random_shuffle(std::begin(Matchups), std::end(Matchups));
		return true;
	}
	else
	{
		PrintThread{} << "MatchupList loaded from file with " << Matchups.size() << " matches to go." << std::endl;
	}
	return true;
}

bool MatchupList::GetNextMatchup(Matchup &NextMatch)
{
	switch (MatchUpProcess)
	{
		case MatchupListType::File:
		{
			if (MatchUpProcess == MatchupListType::File)
			{
				if (Matchups.empty())
				{
					return false;
				}
				NextMatch = Matchups.back();
				Matchups.pop_back();
				return true;
			}
			break;
		}
		case MatchupListType::URL:
		{

			return GetNetMatchFromURL(NextMatch);
		}
		default:
		{
			return false;
		}
	}
	return false;
}

bool MatchupList::SaveMatchList()
{
	std::ofstream ofs(MatchupListFile, std::ofstream::binary);
	if (!ofs)
	{
		return false;
	}
	for (Matchup &NextMatch : Matchups)
	{
		ofs << "\"" + NextMatch.Agent1.BotName + "\"vs\"" + NextMatch.Agent2.BotName + "\" " + NextMatch.Map << std::endl;
	}
	ofs.close();
	return true;

}

bool MatchupList::LoadMatchupList()
{
	std::ifstream ifs(MatchupListFile);
	if (!ifs.good())
	{
		return false;
	}
	Matchups.clear();
	std::string line;
	while (std::getline(ifs, line))
	{
		size_t p = line.find_first_not_of("\"");
		line.erase(0, p);
		p = line.find_first_of("\"");
		std::string FirstAgent = line.substr(0, p);
		line.erase(0, p + 1);
		p = line.find_first_of("\"");
		line.erase(0, p + 1);
		p = line.find_first_of("\"");
		std::string SecondAgent = line.substr(0, p);
		line.erase(0, p + 1);
		p = line.find_first_not_of(" ");
		line.erase(0, p);
		p = line.find_last_not_of(" \t\r\n");
		std::string Map = line.substr(0, p + 1);
		PrintThread{} << "Creating match: " + FirstAgent + " vs " + SecondAgent + " on " + Map << std::endl;
		BotConfig Agent2, Agent1;
		if (!AgentConfig->FindBot(FirstAgent, Agent1))
		{
			PrintThread{} << "Unable to find agent: " + FirstAgent << std::endl;
			continue;
		}
		if (!AgentConfig->FindBot(SecondAgent, Agent2))
		{
			PrintThread{} << "Unable to find agent: " + FirstAgent << std::endl;
			continue;
		}
		Matchup NextMatchup(Agent1, Agent2, Map);
		Matchups.push_back(NextMatchup);

	}
	return true;
}

bool MatchupList::GetNetMatchFromURL(Matchup &NextMatch)
{
	std::vector<std::string> arguments;
	std::string argument = " -F Username=" + ServerUsername;
	arguments.push_back(argument);
	argument = " -F Password=" + ServerPassword;
	arguments.push_back(argument);
	std::string ReturnString = PerformRestRequest(MatchupListFile, arguments);

	rapidjson::Document doc;
	bool parsingFailed = doc.Parse(ReturnString.c_str()).HasParseError();
	if (parsingFailed)
	{
		std::cerr << "Unable to parse incoming bot config: " << ReturnString << std::endl;
		return false;
	}
	if (doc.HasMember("Bot1") && doc["Bot1"].IsObject())
	{
		const rapidjson::Value &Bot1Value = doc["Bot1"];
		if (Bot1Value.HasMember("name") && Bot1Value["name"].IsString())
		{
			if(!AgentConfig->FindBot(Bot1Value["name"].GetString(), NextMatch.Agent1))
			{
                BotConfig Agent1;
                Agent1.BotName = Bot1Value["name"].GetString();
                Agent1.Skeleton = true;
                NextMatch.Agent1 = Agent1;
                if (Bot1Value.HasMember("playerid") && Bot1Value["playerid"].IsString())
                {
                    NextMatch.Bot1Id = Bot1Value["playerid"].GetString();
                    Agent1.PlayerId = Bot1Value["playerid"].GetString();
                }
            }
        }
	}
	if (doc.HasMember("Bot2") && doc["Bot2"].IsObject())
	{
		const rapidjson::Value &Bot2Value = doc["Bot2"];
		if (Bot2Value.HasMember("name") && Bot2Value["name"].IsString())
		{
			if (!AgentConfig->FindBot(Bot2Value["name"].GetString(), NextMatch.Agent2))
			{
                BotConfig Agent2;
                Agent2.BotName = Bot2Value["name"].GetString();
                Agent2.Skeleton = true;
                NextMatch.Agent2 = Agent2;
                if (Bot2Value.HasMember("playerid") && Bot2Value["playerid"].IsString())
                {
                    NextMatch.Bot2Id = Bot2Value["playerid"].GetString();
                    Agent2.PlayerId = Bot2Value["playerid"].GetString();
                }

            }
		}
	}
	if (doc.HasMember("Map") && doc["Map"].IsString())
	{
		NextMatch.Map = doc["Map"].GetString();
	}
	return true;
}
