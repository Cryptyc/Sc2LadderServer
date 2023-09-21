#pragma once
#include "Types.h"
#include "LadderConfig.h"

#define PORT_START_BOT 5677
#define PORT_START 5690

class LadderGame
{
public:
    LadderGame(int InCoordinatorArgc, char** InCoordinatorArgv, LadderConfig *InConfig);
    GameResult StartGame(const std::vector<BotConfig> & Agents, const std::string & Map);



private:
    static void LogStartGame(const std::vector<BotConfig>& Agents);

    int CoordinatorArgc;
    char** CoordinatorArgv;
    LadderConfig *Config;
    uint32_t MaxGameTime{0U};
    uint32_t MaxRealGameTime{0U};
    bool RealTime{false};


};
