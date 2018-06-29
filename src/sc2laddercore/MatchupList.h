#pragma once

class MatchupList
{
public:
    MatchupList(const std::string &inMatchupListFile);
    bool GenerateMatches(std::map<std::string, BotConfig> Agents, std::vector<std::string> Maps);
    bool GetNextMatchup(Matchup &NextMatch);
    bool SaveMatchList();

private:
    const std::string &MatchupListFile;
    std::vector<Matchup> Matchups;
    bool LoadMatchupList(std::map<std::string, BotConfig> Agents);
};