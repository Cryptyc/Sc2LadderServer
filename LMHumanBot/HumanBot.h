#pragma once
#include <sc2api/sc2_api.h>
using namespace sc2;

class BuildingManager;

class HumanBot : public Agent
{
public:
    HumanBot(sc2::Race InRace);

    int StartHuman(int32_t GamePort, int32_t StartPort, std::string ServerAddress, bool COmputerOpponent, std::string OpponentId);


    // Override functions
    virtual void OnGameStart() override;
    virtual void OnStep() override;
private:
    std::string OpponentId;
    struct ConnectionOptions* Options;
    sc2::Race Race;
};
