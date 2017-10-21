#pragma once

class MatchupList
{
public:
    MatchupList(std::string inMatchupListFile);
    bool GenerateMatches(std::map<std::string, BotConfig> Agents, std::vector<std::string> Maps);
    bool GetNextMatchup(Matchup &NextMatch);
    bool SaveMatchList();

private:
    std::string MatchupListFile;
    std::vector<Matchup> Matchups;
    bool LoadMatchupList(std::map<std::string, BotConfig> Agents);
};