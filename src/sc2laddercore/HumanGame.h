#pragma once
#include "Types.h"
#include "LadderConfig.h"

#define PORT_START 5690

#define FIRST_PLAYER_NAME "foo5679"
#define SECOND_PLAYER_NAME "foo5680"

class HumanGame
{
public:
    HumanGame(std::string ReplayFile);
    GameResult StartGame(const BotConfig& Agent1, const BotConfig& Agent2, const std::string& Map);
       
private:
    int CoordinatorArgc = 0;
    char* CoordinatorArgv = "";
    uint32_t MaxGameTime{ 0U };
    uint32_t MaxRealGameTime{ 0U };
    bool RealTime{ true };
    std::string ReplayLocation;
};
