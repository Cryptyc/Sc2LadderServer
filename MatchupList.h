#pragma once

class MatchupList
{
public:
    MatchupList();
    bool GenerateMatches(std::vector<AgentInfo> Agents, std::vector<std::string> Maps);
    bool GetNextMatchup(Matchup &NextMatch);
    
private:
    std::vector<Matchup> Matchups;

};