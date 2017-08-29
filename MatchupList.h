#pragma once

class MatchupList
{
public:
    MatchupList();
    bool GenerateMatches(std::vector<AgentInfo> Agents, std::vector<std::string> Maps);
    Matchup GetNextMatchup();
    
private:
    std::vector<Matchup> Matchups;

};