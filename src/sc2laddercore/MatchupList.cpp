
#include "sc2utils/sc2_manage_process.h"

#include <iostream>
#include <fstream>

#include <vector>
#include <string>
#include <algorithm>
#include <random>
#define RAPIDJSON_HAS_STDSTRING 1

#include "Types.h"
#include "MatchupList.h"
#include "Tools.h"
#include "AgentsConfig.h"

#define URL_REGEX 

MatchupList::MatchupList(const std::string &inMatchupListFile, AgentsConfig *InAgentConfig, std::vector<std::string> &&MapList, const std::string& sc2Path, const std::string &GeneratorType, const std::string &InServerUsername, const std::string &InServerPassword)
	: MatchupListFile(inMatchupListFile)
	, AgentConfig(InAgentConfig)
	, sc2Path(sc2Path)
	, ServerUsername(InServerUsername)
	, ServerPassword(InServerPassword)
{
    // Do not create matches if we can not run them.
    if (sc2Path.empty() || !sc2::DoesFileExist(sc2Path))
    {
        return;
    }
	MatchUpProcess = GetMatchupListTypeFromString(GeneratorType);
	if(MatchUpProcess == MatchupListType::None)
	{
		PrintThread{} << "Unknown Matchuplist generator type: " + GeneratorType + " Should be either \"file\" or \"url\"" << std::endl;
		return;
	}
	GenerateMatches(std::move(MapList));
}


bool MatchupList::GenerateMatches(std::vector<std::string> &&maps)
{
	Matchups.clear();
	PrintThread{} << "Found Agents: " << std::endl;
	for (const auto &Agent : AgentConfig->BotConfigs)
	{
        PrintThread{} << "* " << Agent.second.BotName << std::endl;
	}
	const auto firstInvalidMapIt = std::remove_if(maps.begin(),maps.end(),[&](const auto& map)->bool { return !isMapAvailable(map, sc2Path);});
	if (firstInvalidMapIt != maps.cbegin())
	{
		PrintThread{} << "Found the following maps: " << std::endl;
		for (auto mapIt(maps.begin());mapIt != firstInvalidMapIt;++mapIt)
		{
			PrintThread{} << "* " << *mapIt << std::endl;
		}
	}
	if (firstInvalidMapIt != maps.cend())
	{
		PrintThread{} << "Failed to locate the following maps: " << std::endl;
		for (auto mapIt(firstInvalidMapIt);mapIt != maps.end();++mapIt)
		{
			PrintThread{} << "* " << *mapIt << std::endl;
		}
		PrintThread{} << "Please put the map files in '" << sc2::GetGameMapsDirectory(sc2Path) << "'." << std::endl;
	}
	maps.erase(firstInvalidMapIt,maps.end());

    const std::vector<BotConfig> &bots = AgentConfig->Bots();

    for (const std::string &map : maps) {
        Matchup NextMatchup(bots, map);
        Matchups.push_back(NextMatchup);
    }

    std::random_device rd;
    std::default_random_engine rng(rd());
    std::shuffle(std::begin(Matchups), std::end(Matchups), rng);

	return true;
}

bool MatchupList::GetNextMatchup(Matchup &NextMatch)
{
	switch (MatchUpProcess)
	{
		case MatchupListType::File:
		{
            if (Matchups.empty())
            {
                return false;
            }
            NextMatch = Matchups.back();
            Matchups.pop_back();
            return true;
		}
		default:
		{
			return false;
		}
	}
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
		ofs << "\"" + NextMatch.GameName() + "\" " + NextMatch.Map << std::endl;
	}
	ofs.close();
	return true;

}
