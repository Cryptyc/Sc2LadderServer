#pragma once

class AgentsConfig;

class MatchupList
{
public:
    MatchupList(const std::string &inMatchupListFile, AgentsConfig *InAgentConfig, const std::vector<std::string> &MapList, const std::string &GeneratorType, const std::string &InServerUsername, const std::string &InServerPassword);
    bool GenerateMatches(const std::vector<std::string> &Maps);
    bool GetNextMatchup(Matchup &NextMatch);
    bool SaveMatchList();

private:
    const std::string &MatchupListFile;
    std::vector<Matchup> Matchups;
    AgentsConfig *AgentConfig;
    bool LoadMatchupList();
    bool GetNetMatchFromURL(Matchup &NextMatch);

    MatchupListType MatchUpProcess;
    std::string ServerUsername;
    std::string ServerPassword;
};
