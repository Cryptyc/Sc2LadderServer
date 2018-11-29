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
    GameResult StartGameVsDefault(const BotConfig &Agent1, sc2::Race CompRace, sc2::Difficulty CompDifficulty, const std::string &Map);



private:
    void LogStartGame(const BotConfig & Bot1, const BotConfig & Bot2);
    bool SaveReplay(sc2::Connection * client, const std::string & path);
    bool ProcessObservationResponse(SC2APIProtocol::ResponseObservation Response, std::vector<sc2::PlayerResult>* PlayerResults);
    std::string GetBotCommandLine(const BotConfig &AgentConfig, int GamePort, int StartPort, const std::string &OpponentId, bool CompOpp = false, sc2::Race CompRace = sc2::Race::Terran, sc2::Difficulty CompDifficulty = sc2::Difficulty::Easy);
    void ResolveMap(const std::string & map_name, SC2APIProtocol::RequestCreateGame * request, sc2::ProcessSettings process_settings);
    sc2::GameRequestPtr CreateStartGameRequest(const std::string & MapName, std::vector<sc2::PlayerSetup> players, sc2::ProcessSettings process_settings);
    sc2::GameResponsePtr CreateErrorResponse();
    sc2::GameRequestPtr CreateLeaveGameRequest();
    sc2::GameRequestPtr CreateQuitRequest();
    ResultType GetPlayerResults(sc2::Connection * client);
    bool SendDataToConnection(sc2::Connection * Connection, const SC2APIProtocol::Request * request);
    void LadderGame::ChangeBotNames(const std::string ReplayFile, const std::string &Bot1Name, const std::string Bot2Name);

    int CoordinatorArgc;
    char** CoordinatorArgv;
    LadderConfig *Config;
    uint32_t MaxGameTime;
    uint32_t MaxRealGameTime;

};
