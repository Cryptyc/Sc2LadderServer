
#include "sc2lib/sc2_lib.h"
#include "sc2api/sc2_api.h"
#include "sc2utils/sc2_manage_process.h"

#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include "Types.h"

#include "MatchupList.h"

MatchupList::MatchupList()
{

}

bool MatchupList::GenerateMatches(std::vector<AgentInfo> Agents, std::vector<std::string> Maps)
{
	Matchups.clear();
	for (AgentInfo Agent1 : Agents)
	{
		for (AgentInfo Agent2 : Agents)
		{
			if (Agent1 == Agent2)
			{
				continue;
			}
			for (std::string map : Maps)
			{
				Matchup NextMatchup(Agent1, Agent2, map);
				Matchups.push_back(NextMatchup);
			}
		}
	}
	auto rng = std::default_random_engine{};
	std::shuffle(std::begin(Matchups), std::end(Matchups), rng);
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
	return true;;
}