#pragma once

#include "AgentsConfig.h"

#include <string>
#include <future>

#include "sc2api/sc2_game_settings.h"
#include "sc2api/sc2_connection.h"
#include "sc2api/sc2_server.h"


struct Stats
{
    float avgLoopDuration{0.0f};
    size_t gameLoops{0U};
    // Fill this with more stats
    // time for first Loop
    // number of actions
};

class Proxy
{
    // Client
    sc2::Server m_server{};
    sc2::Connection m_client{};
    uint64_t m_gameClientPid{0UL};

    // Game
    static bool m_mapAlreadyLoaded;
    const uint32_t m_maxGameLoops{0U};
    const uint32_t m_maxRealGameTime{0U};  // sec
    uint32_t m_currentGameLoop{0U};
    uint32_t m_surrenderLoop{0U};
    SC2APIProtocol::Status m_gameStatus{SC2APIProtocol::Status::unknown};
    std::future<void> m_gameUpdateThread{};
    ExitCase m_result{ExitCase::Unknown};
    bool m_realTimeMode{false};

    // Bot
    const BotConfig m_botConfig{};
    std::future<void> m_botProgramThread{};
    unsigned long m_botThreadId{0};
    bool m_usedDebugInterface{false};

    // stats
    Stats m_stats{};
    using clock = std::chrono::system_clock;  // there is also steady_clock and high_resolution_clock
    clock::time_point m_lastResponseSendTime{};
    clock::duration m_totalTime{std::chrono::seconds(0)};


    // constants
    static constexpr auto m_localHost{"127.0.0.1"};  // is there a way to get this without hardcoding?
    static constexpr int m_responseTimeOutMS{100000};


    bool createGameHasErrors(const SC2APIProtocol::ResponseCreateGame& createGameResponse) const;
    std::string getBotCommandLine(const int gamePort, const int startPort, const std::string& opponentID) const;
    bool isBotCrashed(const int milliseconds) const;
    bool isClientCrashed(const int milliseconds) const;
    bool processRequest(const sc2::RequestData& request);
    bool processResponse(SC2APIProtocol::Response* const response);
    void gameUpdate();
    void terminateGame();
    void doAStep();
    void updateStatus(const SC2APIProtocol::Status newStatus);
    uint32_t getMaxStepTime() const;
    SC2APIProtocol::Response* receiveResponse(SC2APIProtocol::Response::ResponseCase responseCase);
    SC2APIProtocol::Result getGameResult();

 public:
    Proxy() = delete;
    ~Proxy();
    Proxy(const uint32_t maxGameLoops, const uint32_t maxRealGameTime, const BotConfig& botConfig);

    bool ConnectToSC2Instance(const sc2::ProcessSettings & processSettings, const int portServer, const int portClient);
    void startSC2Instance(const sc2::ProcessSettings& processSettings, const int portServer, const int portClient);
    bool setupGame(const sc2::ProcessSettings& processSettings, const std::string& map, const bool realTimeMode, const sc2::Race bot1Race, const sc2::Race bot2Race);
    bool startBot(const int portServer, const int portStart, const std::string & opponentPlayerId);
    void startGame();

    bool gameFinished() const;
    bool saveReplay(const std::string& replayFile);
    ExitCase getResult() const;
    const Stats& stats() const;
};
