#include <iostream>

#include "HumanBot.h"
#include <sc2api/sc2_api.h>
#include "LadderInterface.h"

HumanBot::HumanBot(sc2::Race InRace)
    :Race(InRace)
{
}

int HumanBot::StartHuman(int32_t GamePort, int32_t StartPort, std::string ServerAddress, bool COmputerOpponent, std::string OpponentId)
{
    Options = new ConnectionOptions();
    Options->GamePort = GamePort;
    Options->StartPort = StartPort;
    Options->ServerAddress = ServerAddress;
    Options->OpponentId = OpponentId;
    RunBot(this, Race, *Options);

    return 0;
}

void HumanBot::OnGameStart()
{
}

void HumanBot::OnStep()
{
}
