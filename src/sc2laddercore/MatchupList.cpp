
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
#include "Types.h"
#include "LadderManager.h"
#include "MatchupList.h"

MatchupList::MatchupList(const std::string &inMatchupListFile)
	: MatchupListFile(inMatchupListFile)
{

}

bool MatchupList::GenerateMatches(std::map<std::string, BotConfig> Agents, std::vector<std::string> Maps)
{
	Matchups.clear();
	if (!LoadMatchupList(Agents))
	{
		PrintThread{} << "Could not load MatchupList from file. Generating new one.." << std::endl;
		for (int i = 0; i < 2; i++)
		{
			for (const auto &Agent1 : Agents)
			{
				for (const auto &Agent2 : Agents)
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
		}
		srand(time(0));
		std::random_shuffle(std::begin(Matchups), std::end(Matchups));
		return true;
	}
	else {
		PrintThread{} << "MatchupList loaded from file with " << Matchups.size() << " matches to go." << std::endl;
	}
	return true;
}

bool MatchupList::GetNextMatchup(Matchup &NextMatch)
{
	if (Matchups.empty())
	{
		return false;
	}
	NextMatch = Matchups.back();
	Matchups.pop_back();
	return true;
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

bool MatchupList::LoadMatchupList(std::map<std::string, BotConfig> Agents)
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
		auto search = Agents.find(FirstAgent);
		PrintThread{} << "Creating match: " + FirstAgent + " vs " + SecondAgent + " on " + Map << std::endl;
		if (search == Agents.end())
		{
			PrintThread{} << "Unable to find agent: " + FirstAgent << std::endl;
			continue;
		}
		BotConfig Agent1 = search->second;
		search = Agents.find(SecondAgent);
		if (search == Agents.end())
		{
			PrintThread{} << "Unable to find agent: " + SecondAgent << std::endl;
			continue;
		}
		BotConfig Agent2 = search->second;
		Matchup NextMatchup(Agent1, Agent2, Map);
		Matchups.push_back(NextMatchup);

	}
	return true;
}