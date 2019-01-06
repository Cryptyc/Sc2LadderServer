#pragma once
#include <string>
#include <vector>

class AgentsConfig;

class MatchupList
{
public:
	MatchupList(const std::string &inMatchupListFile, AgentsConfig *InAgentConfig, std::vector<std::string> &&MapList, const std::string& sc2Path, const std::string &GeneratorType, const std::string &InServerUsername, const std::string &InServerPassword);
	bool GenerateMatches(std::vector<std::string> &&Maps);
    bool GetNextMatchup(Matchup &NextMatch);
    bool SaveMatchList();

private:
    const std::string MatchupListFile;
    std::vector<Matchup> Matchups;
    AgentsConfig *AgentConfig;
    bool LoadMatchupList();
	bool GetNextMatchFromURL(Matchup &NextMatch);

	const std::string sc2Path{""};
    MatchupListType MatchUpProcess;
    std::string ServerUsername;
    std::string ServerPassword;
};
