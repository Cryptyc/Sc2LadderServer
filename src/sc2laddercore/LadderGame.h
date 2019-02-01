#pragma once
#include "Types.h"
#include "LadderConfig.h"

#define PORT_START 5690

#define FIRST_PLAYER_NAME "foo5679"
#define SECOND_PLAYER_NAME "foo5680"

class LadderGame
{
public:
    LadderGame(int InCoordinatorArgc, char** InCoordinatorArgv, LadderConfig *InConfig);
    GameResult StartGame(const BotConfig & Agent1, const BotConfig & Agent2, const std::string & Map);



private:
    void LogStartGame(const BotConfig & Bot1, const BotConfig & Bot2);
    void ChangeBotNames(const std::string &ReplayFile, const std::string &Bot1Name, const std::string &Bot2Name);

    int CoordinatorArgc;
    char** CoordinatorArgv;
    LadderConfig *Config;
    uint32_t MaxGameTime{0U};
    uint32_t MaxRealGameTime{0U};
    bool RealTime{false};
};
