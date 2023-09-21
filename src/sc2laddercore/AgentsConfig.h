#pragma once

#include <string>
#include <map>

#include "Types.h"
#include "LadderConfig.h"

#define PLAYER_ID_LENGTH 16

class AgentsConfig
{
public:
    AgentsConfig(LadderConfig *InLadderConfig);
    void LoadAgents(const std::string &BaseDirectory, const std::string &BotConfigFile);
	void SaveBotConfig(const BotConfig & Agent);
    void ReadBotDirectories(const std::string &BaseDirectory);

    bool FindBot(const std::string &BotName, BotConfig &ReturnBot);
    std::vector<BotConfig> Bots();

    std::map<std::string, BotConfig> BotConfigs;

private:
    LadderConfig *Config;
    LadderConfig *PlayerIds;
    bool EnablePlayerIds;
    static std::string GeneratePlayerId(size_t Length);
};
