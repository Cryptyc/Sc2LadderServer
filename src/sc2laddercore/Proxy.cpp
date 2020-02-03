#include "Proxy.h"

#include <fstream>

#include "Tools.h"

#include "sc2utils/sc2_manage_process.h"


bool Proxy::m_mapAlreadyLoaded{false};



Proxy::Proxy(const uint32_t maxGameLoops, const uint32_t maxRealGameTime, const BotConfig& botConfig):
    m_maxGameLoops(maxGameLoops)
  , m_maxRealGameTime(maxRealGameTime)
  , m_botConfig(botConfig)
{
}

Proxy::~Proxy()
{
    // Set it back to false for the match after this one.
    m_mapAlreadyLoaded = false;

    // Check if the bot is still running.
    const auto start = clock::now();
    std::chrono::duration<double> elapsedSeconds{0};
    std::future_status botProgStatus{std::future_status::deferred};
    // toDo: add to config?
    constexpr auto maxWaitTime{20};
    if (m_botProgramThread.valid())
    {
        while (elapsedSeconds.count() < maxWaitTime)
        {
            botProgStatus = m_botProgramThread.wait_for(std::chrono::seconds(1));
            if (botProgStatus == std::future_status::ready)
            {
                PrintThread{} << m_botConfig.BotName << " : Bot terminated properly." << std::endl;
                break;
            }
            elapsedSeconds = clock::now() - start;
        }
        if (botProgStatus != std::future_status::ready)
        {
            PrintThread{} << m_botConfig.BotName << " : Bot is still running after " << maxWaitTime << " seconds. Sending kill signal." << std::endl;
            KillBotProcess(m_botThreadId);
        }
        sc2::SleepFor(5000);
    }
    if (m_gameClientPid)
    {
        if (!sc2::TerminateProcess(m_gameClientPid))
        {
            PrintThread{} << m_botConfig.BotName << " : Terminating SC2 failed!" << std::endl;
        }
        sc2::SleepFor(5000);
    }
}
void Proxy::startSC2Instance(const sc2::ProcessSettings& processSettings, const int portServer, const int portClient)
{
    // magic numbers
    m_server.Listen(std::to_string(portServer).c_str(), "100000", "100000", "5");

    m_gameClientPid = sc2::StartProcess(processSettings.process_path,
        { "-listen", m_localHost,
          "-port", std::to_string(portClient),
          "-displayMode", "0",
          "-dataVersion", processSettings.data_version });
}

bool Proxy::ConnectToSC2Instance(const sc2::ProcessSettings& processSettings, const int portServer, const int portClient)
{
    // Depending on the hardware the client sometimes needs a second or two.
    size_t connectionAttempts = 0;
    constexpr size_t abandonConnectionAttemptAfter = 60;  // sec
    constexpr bool withDebugOutput = false;
    while (!m_client.Connect(m_localHost, portClient, withDebugOutput))
    {
        ++connectionAttempts;
        sc2::SleepFor(1000);
        if (connectionAttempts > abandonConnectionAttemptAfter)
        {
            PrintThread{} << "Failed to connect to client (" << m_botConfig.BotName << ")" << std::endl;
            return false;
        }
    }

    // Check if client is reacting
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();
    request->mutable_ping();
    m_client.Send(request.get());
    auto* response = receiveResponse(SC2APIProtocol::Response::ResponseCase::kPing);
    return response;
}

// Technically, we only need opponents race. But I think it looks clearer on the caller side with both races.
bool Proxy::setupGame(const sc2::ProcessSettings& processSettings, const std::string& map, const bool realTimeMode, const sc2::Race bot1Race, const sc2::Race bot2Race)
{
    m_realTimeMode = realTimeMode;
    // Only one client needs to / is allowed to send the create game request.
    if (m_mapAlreadyLoaded)
    {
        return true;
    }
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();

    SC2APIProtocol::RequestCreateGame* requestCreateGame = request->mutable_create_game();

    // Player 1
    SC2APIProtocol::PlayerSetup* playerSetup = requestCreateGame->add_player_setup();
    playerSetup->set_type(SC2APIProtocol::PlayerType::Participant);
    playerSetup->set_race(SC2APIProtocol::Race(static_cast<int>(bot1Race) + 1));  // Ugh
    playerSetup->set_difficulty(SC2APIProtocol::Difficulty::VeryEasy);

    // Player 2
    playerSetup = requestCreateGame->add_player_setup();
    playerSetup->set_type(SC2APIProtocol::PlayerType::Participant);
    playerSetup->set_race(SC2APIProtocol::Race(static_cast<int>(bot2Race) + 1));
    playerSetup->set_difficulty(SC2APIProtocol::Difficulty::VeryEasy);

    // Map
    // BattleNet map
    if (!sc2::HasExtension(map, ".SC2Map"))
    {
        requestCreateGame->set_battlenet_map_name(map);
    }
    else
    {
        // Local map file
        SC2APIProtocol::LocalMap* const localMap = requestCreateGame->mutable_local_map();
        // Absolute path
        if (sc2::DoesFileExist(map))
        {
            localMap->set_map_path(map);
        }
        else
        {
            // Relative path - Game maps directory
            const std::string gameRelative = sc2::GetGameMapsDirectory(processSettings.process_path) + map;
            if (sc2::DoesFileExist(gameRelative))
            {
                localMap->set_map_path(map);
            }
            else
            {
                // Relative path - Library maps directory
                const std::string libraryRelative = sc2::GetLibraryMapsDirectory() + map;
                if (sc2::DoesFileExist(libraryRelative))
                {
                    localMap->set_map_path(libraryRelative);
                }
                else
                {
                    return false;
                }
            }
        }
    }

    // Real time mode
    requestCreateGame->set_realtime(realTimeMode);

    // Send the request
    m_client.Send(request.get());
    SC2APIProtocol::Response* createGameResponse = receiveResponse(SC2APIProtocol::Response::ResponseCase::kCreateGame);

    // Check if the request was successful
    if (!createGameResponse || createGameHasErrors(createGameResponse->create_game()))
    {
        return false;
    }
    m_mapAlreadyLoaded = true;
    return true;
}

bool Proxy::startBot(const int portServer, const int portStart, const std::string & opponentPlayerId)
{
    const std::string botStartCommand = getBotCommandLine(portServer, portStart, opponentPlayerId);
    if (botStartCommand == m_botConfig.executeCommand)
    {
        return false;
    }
    m_botProgramThread = std::async(std::launch::async, &StartBotProcess, m_botConfig, botStartCommand, &m_botThreadId);
    if (m_botProgramThread.wait_for(std::chrono::seconds(2)) == std::future_status::ready)
    {
        return false;
    }
    constexpr size_t maxStartUpTime = 10U; // The bot gets 10 seconds to connect to the proxy. This is NOT the first game loop time.
    for (auto waitedFor(0U); waitedFor < maxStartUpTime; ++waitedFor)
    {
        if (!m_server.connections_.empty())
        {
            return true;
        }
        sc2::SleepFor(1000);
    }
    return false;
}

void Proxy::startGame()
{
    m_gameUpdateThread = std::async(std::launch::async, &Proxy::gameUpdate, this);
}

bool Proxy::gameFinished() const
{
    return std::future_status::ready == m_gameUpdateThread.wait_for(std::chrono::seconds(0));
}

ExitCase Proxy::getResult() const
{
    return m_result;
}

bool Proxy::createGameHasErrors(const SC2APIProtocol::ResponseCreateGame& createGameResponse) const
{
    bool hasError = false;
    if (createGameResponse.has_error())
    {
        std::string errorCode = "Unknown";
        switch (createGameResponse.error())
        {
        case SC2APIProtocol::ResponseCreateGame::MissingMap:
        {
            errorCode = "Missing Map";
            break;
        }
        case SC2APIProtocol::ResponseCreateGame::InvalidMapPath:
        {
            errorCode = "Invalid Map Path";
            break;
        }
        case SC2APIProtocol::ResponseCreateGame::InvalidMapData:
        {
            errorCode = "Invalid Map Data";
            break;
        }
        case SC2APIProtocol::ResponseCreateGame::InvalidMapName:
        {
            errorCode = "Invalid Map Name";
            break;
        }
        case SC2APIProtocol::ResponseCreateGame::InvalidMapHandle:
        {
            errorCode = "Invalid Map Handle";
            break;
        }
        case SC2APIProtocol::ResponseCreateGame::MissingPlayerSetup:
        {
            errorCode = "Missing Player Setup";
            break;
        }
        case SC2APIProtocol::ResponseCreateGame::InvalidPlayerSetup:
        {
            errorCode = "Invalid Player Setup";
            break;
        }
        default:
        {
            break;
        }
        }

        PrintThread{} << m_botConfig.BotName << " : CreateGame request returned an error code: " << errorCode << " (" << createGameResponse.error() << ")" << std::endl;
        hasError = true;
    }
    if (createGameResponse.has_error_details() && createGameResponse.error_details().length() > 0)
    {
        PrintThread{} << m_botConfig.BotName << " : CreateGame request returned error details: " << createGameResponse.error_details() << std::endl;
        hasError = true;
    }
    return hasError;
}

std::string Proxy::getBotCommandLine(const int gamePort, const int startPort, const std::string& opponentID) const
{
    // Add universal arguments
    return m_botConfig.executeCommand + " --GamePort " + std::to_string(gamePort) + " --StartPort " + std::to_string(startPort) + " --LadderServer " + m_localHost + " --OpponentId " + opponentID;
}



void Proxy::gameUpdate()
{
    PrintThread{} << "Starting proxy for " << m_botConfig.BotName << std::endl;
    // toDo: somehow check if the other functions were already used.


    // Actually, the game still loads...
    const auto gameStartTime = clock::now();
    // The bot has 1 minute + time out time to send the first request.
    bool alreadySurrendered = false;

    while (m_gameStatus == SC2APIProtocol::Status::in_game || m_gameStatus == SC2APIProtocol::Status::init_game || m_gameStatus == SC2APIProtocol::Status::launched)
    {
        // If we know that the bot crashed we surrender for it.
        if (m_result == ExitCase::BotCrashed || m_result == ExitCase::BotStepTimeout || m_result == ExitCase::GameTimeOver)
        {
            // The bot is dead. So we will surrender on its behalf
            if (!alreadySurrendered)
            {
                terminateGame();
                alreadySurrendered = true;
                continue;
            }
            // and step the simulation until the match has officially ended.
            if (m_result == ExitCase::BotCrashed || m_result == ExitCase::BotStepTimeout)
            {
                doAStep();
                continue;
            }
        }

        if (m_server.HasRequest())
        {
            const sc2::RequestData& request = m_server.PeekRequest();
            // Analyse request
            // Returns false if a quit request was made.
            const bool validRequest = processRequest(request);
            // A quit request is handled as if the bot crashed.
            // Especially, we do not want to forward the request to the client.
            // We still need it for the replay.
            if (!validRequest)
            {
                m_result = ExitCase::BotCrashed;
                continue;
            }
            // Forward the valid request
            // The cast puts a lot of trust in Blizzard
            const auto expectedResponseCase = static_cast<SC2APIProtocol::Response::ResponseCase>(request.second->request_case());
            m_server.SendRequest(m_client.connection_);

            // Block for sc2's response then queue it.
            SC2APIProtocol::Response* response = receiveResponse(expectedResponseCase);
            const bool validResponse = processResponse(response);

            if (!validResponse)
            {
                m_result = ExitCase::Error;
                break;
            }
            // Send the response back to the client.
            if (!m_server.connections_.empty() && m_client.connection_ != nullptr)
            {
                m_server.QueueResponse(m_client.connection_, response);
                m_server.SendResponse();
            }
            else
            {
                // This usually happens if the bot crashed.
                // Check if the bot thread has send the crashed signal aka ready signal.
                if (isBotCrashed(1000))
                {
                    PrintThread{} << m_botConfig.BotName << " : crashed." << std::endl;
                    m_result = ExitCase::BotCrashed;
                    continue;
                }
                // Maybe it is the client ?
                if (isClientCrashed(1000))
                {
                    PrintThread{} << m_botConfig.BotName << " : crashed." << std::endl;
                }
                // toDo: Are there other cases when this happens?
                if (m_server.connections_.empty())
                {
                    PrintThread{} << m_botConfig.BotName << " : Response: m_server.connections_.empty()" << std::endl;
                }
                else
                {
                    PrintThread{} << m_botConfig.BotName << " : Response: m_client.connection_ == nullptr" << std::endl;
                }
                m_result = ExitCase::Error;
                break;
            }
        }
        else
        {
            const uint32_t maxStepTime = getMaxStepTime();  // ms
            const auto timeSinceLastResponse = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - m_lastResponseSendTime).count();
            if (!m_botConfig.Debug && !m_realTimeMode && maxStepTime && timeSinceLastResponse > static_cast<int>(maxStepTime))
            {
                PrintThread{} << m_botConfig.BotName << " : bot is too slow. " << timeSinceLastResponse << " milliseconds passed. Max step time: " << static_cast<int>(maxStepTime) << " milliseconds." << std::endl;
                // ToDo: Make a chat announcement
                // ToDo: Can we handle this better. It
                m_result = ExitCase::BotStepTimeout;
            }

            // Check if the bot thread has send the crashed signal.
            // This is a fast check to not slow down the game
            if (isBotCrashed(0))
            {
                PrintThread{} << m_botConfig.BotName << " : crashed." << std::endl;
                m_result = ExitCase::BotCrashed;
                continue;
            }
            if (m_server.connections_.empty() || m_client.connection_ == nullptr)
            {
                // Time for a serious check if the bot crashed.
                if (isBotCrashed(1000))
                {
                    PrintThread{} << m_botConfig.BotName << " : crashed." << std::endl;
                    m_result = ExitCase::BotCrashed;
                    continue;
                }
                // Maybe it is the client ?
                if (isClientCrashed(1000))
                {
                    PrintThread{} << m_botConfig.BotName << " : crashed." << std::endl;
                }
                if (m_server.connections_.empty())
                {
                    PrintThread{} << m_botConfig.BotName << " : Receive: server->connections_.empty()" << std::endl;
                    m_result = ExitCase::Error;
                    break;
                }

                // If there is no connection to the client it probably crashed.
                if (m_client.connection_ == nullptr)
                {
                    PrintThread{} << m_botConfig.BotName << " :  Receive: m_client.connection_ == nullptr" << std::endl;
                    m_result = ExitCase::Error;
                    break;
                }
            }
        }
        const auto gameDurationRealTime = std::chrono::duration_cast<std::chrono::seconds>(clock::now() - gameStartTime).count();
        if (m_maxRealGameTime && gameDurationRealTime > m_maxRealGameTime)
        {
            m_result = ExitCase::GameTimeOver;
        }
    }

    if (m_result == ExitCase::Unknown)
    {
        // The game ended normally for this bot. Get the result from the observation.
        const SC2APIProtocol::Result result = getGameResult();
        switch (result)
        {
        case SC2APIProtocol::Result::Victory:
        {
            m_result = ExitCase::GameEndVictory;
            break;
        }
        case SC2APIProtocol::Result::Defeat:
        {
            m_result = ExitCase::GameEndDefeat;
            break;
        }
        case SC2APIProtocol::Result::Tie:
        {
            m_result = ExitCase::GameEndTie;
            break;
        }
        case SC2APIProtocol::Result::Undecided:
        default:
        {
            m_result = ExitCase::Error;
            break;
        }
        }
    }
    m_stats.avgLoopDuration = std::chrono::duration_cast<std::chrono::milliseconds>(m_totalTime).count()/static_cast<float>(m_currentGameLoop);
    m_stats.gameLoops = m_currentGameLoop;
    PrintThread{} << m_botConfig.BotName << " : Exiting with " << GetExitCaseString(m_result) << " Average step time " << m_stats.avgLoopDuration << " microseconds, total time: " << std::chrono::duration_cast<std::chrono::seconds>(m_totalTime).count() << " seconds, game loops: " << m_currentGameLoop << std::endl;
}

bool Proxy::isBotCrashed(const int milliseconds) const
{
    return m_botProgramThread.wait_for(std::chrono::milliseconds(milliseconds)) == std::future_status::ready;
}

bool Proxy::isClientCrashed(const int) const
{
    // toDo
    // Big effort due to cross compatibility,
    // little gain since it is only needed to make a clearer error message.
    return false;
}

bool Proxy::processRequest(const sc2::RequestData& request)
{
    if (request.second)
    {
        if (request.second->has_quit())
        {
            // Intercept quit requests, we want to keep game alive to save replays.
            // If a s2client-api (c++) throws an exception a quit request gets issued.
            // So check if it crashed.
            if (isBotCrashed(1000))
            {
                PrintThread{} << m_botConfig.BotName << " : crashed." << std::endl;
            }
            else
            {
                PrintThread{} << m_botConfig.BotName << " HAS ISSUED A QUIT REQUEST. Please tell the author not to." << std::endl;
            }
            return false;
        }
        if (request.second->has_leave_game())
        {
            // Leave game requests are also a problem.
            PrintThread{} << m_botConfig.BotName << " has issued a leave game request. Please don't do that." << std::endl;
            // return false;
        }
        else if (request.second->has_debug() && !m_usedDebugInterface)
        {
            PrintThread{} << m_botConfig.BotName << " : IS USING DEBUG INTERFACE.  POSSIBLE CHEAT! Please tell them not to." << std::endl;
            m_usedDebugInterface = true;
        }
        else if (request.second->has_step() && m_currentGameLoop)
        {
            m_totalTime += clock::now() - m_lastResponseSendTime;
        }
    }
    return true;
}

bool Proxy::processResponse(SC2APIProtocol::Response* const response)
{
    if (response == nullptr)
    {
        PrintThread{} << m_botConfig.BotName << " : Waiting for a response had a timeout or was invalid." << std::endl;
        return false;
    }
    if (response->has_observation())
    {
        const SC2APIProtocol::Observation& observation = response->observation().observation();
        m_currentGameLoop = observation.game_loop();
        // forced tie situation
        if (m_result == ExitCase::GameTimeOver && response->observation().player_result_size() > 0)
        {
            auto* const obs = response->mutable_observation();
            std::vector<uint32_t> allPlayerIDs;
            for (int i(0); i < obs->player_result_size(); ++i)
            {
                allPlayerIDs.push_back(obs->player_result(i).player_id());
            }
            obs->clear_player_result();
            for (const auto& playerID : allPlayerIDs)
            {
                auto* const result = obs->add_player_result();
                result->set_player_id(playerID);
                result->set_result(SC2APIProtocol::Result::Tie);
            }
        }
        for (int i(0); i < response->observation().chat_size(); ++i)
        {
            const auto& chat = response->observation().chat(i);
            if (observation.player_common().player_id() == chat.player_id())
            {
                if (chat.has_message() && chat.message() == m_botConfig.SurrenderPhrase)
                {
                    m_surrenderLoop = m_currentGameLoop + 68; // ~3 in-game sec
                }
            }
        }
        if (m_surrenderLoop && m_currentGameLoop >= m_surrenderLoop)
        {
            terminateGame();
        }

        if (m_maxGameLoops && m_currentGameLoop > m_maxGameLoops)
        {
            m_result = ExitCase::GameTimeOver;
        }
    }
    if (response->has_step())
    {
        m_lastResponseSendTime = clock::now();
    }
    return true;
}


void Proxy::terminateGame()
{
    PrintThread{} << m_botConfig.BotName << " : surrender." << std::endl;
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();

    SC2APIProtocol::RequestDebug* debugRequest = request->mutable_debug();

    auto debugCommand = debugRequest->add_debug();
    auto endGame = debugCommand->mutable_end_game();
    // If the proxy has to end the game it is because the bot failed somehow (crash, too slow, etc), aka lost.
    endGame->set_end_result(SC2APIProtocol::DebugEndGame_EndResult::DebugEndGame_EndResult_Surrender);

    m_client.Send(request.get());
    SC2APIProtocol::Response* debugResponse = receiveResponse(SC2APIProtocol::Response::ResponseCase::kDebug);
    if (debugResponse)
    {
        if (debugResponse->has_status())
        {
            updateStatus(debugResponse->status());
        }
    }
}

// If the bot is dead and the opponent bot has a step_size so that
// the current loop is an 'off step' loop the proxy needs to step on behalf of the bot.
void Proxy::doAStep()
{
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();

    SC2APIProtocol::RequestStep* stepRequest = request->mutable_step();

    stepRequest->set_count(1);  // ToDo: this can maybe made smarter
    m_client.Send(request.get());
    SC2APIProtocol::Response* response = receiveResponse(SC2APIProtocol::Response::ResponseCase::kStep);
    if (response)
    {
        if (response->has_status())
        {
            updateStatus(response->status());
        }
    }
}

uint32_t Proxy::getMaxStepTime() const
{
    if (m_currentGameLoop)
    {
        return 20000U;  // ToDo: Add this to config file.
    }
    return 0U;
}

void Proxy::updateStatus(const SC2APIProtocol::Status newStatus)
{
    if (newStatus != m_gameStatus && (m_gameStatus == SC2APIProtocol::Status::unknown || static_cast<int>(newStatus) > static_cast<int>(m_gameStatus)))
    {
        PrintThread{} << m_botConfig.BotName << " : Client changed status from "<< statusToString(m_gameStatus) << " to " << statusToString(newStatus) << std::endl;
        m_gameStatus = newStatus;
    }
}

bool Proxy::saveReplay(const std::string& replayFile)
{
    if (m_result == ExitCase::Error)
    {
        PrintThread{} << m_botConfig.BotName << " : Match ended in error. Can not save replay." << std::endl;
        // Maybe we could. But most likely we will get an assertion failed or even exception. Better safe than sorry.
        return false;
    }
    PrintThread{} << m_botConfig.BotName << " : Saving replay." << std::endl;
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();
    request->mutable_save_replay();
    m_client.Send(request.get());
    SC2APIProtocol::Response* response = receiveResponse(SC2APIProtocol::Response::ResponseCase::kSaveReplay);
    if (!response || !response->has_save_replay())
    {
        PrintThread{} << m_botConfig.BotName << " : Failed to receive replay response." << std::endl;
        return false;
    }

    const SC2APIProtocol::ResponseSaveReplay& replay = response->save_replay();

    if (replay.data().empty())
    {
        PrintThread{} << m_botConfig.BotName << " : Replay data empty." << std::endl;
        return false;
    }

    std::ofstream file;
    file.open(replayFile, std::fstream::binary);
    if (!file.is_open())
    {
        PrintThread{} << m_botConfig.BotName << " : Could not open replay file: " << replayFile << std::endl;
        return false;
    }

    file.write(&replay.data()[0], static_cast<std::streamsize>(replay.data().size()));
    return true;
}

SC2APIProtocol::Response* Proxy::receiveResponse(const SC2APIProtocol::Response::ResponseCase responseCase)
{
    SC2APIProtocol::Response* response{nullptr};
    if (!m_client.Receive(response, m_responseTimeOutMS))
    {
        return nullptr;
    }
    bool hasErrors = false;
    if (responseCase != response->response_case())
    {
        PrintThread{} << m_botConfig.BotName << " : expected " << responseCaseToString(responseCase) << " but got " << responseCaseToString(response->response_case()) <<std::endl;
        hasErrors = true;
    }
    if (response->error_size())
    {
        std::ostringstream ss;
        ss << m_botConfig.BotName << " : response " << responseCaseToString(response->response_case()) << " has " << response->error_size() << " error(s)!" << std::endl;
        for (int i(0); i < response->error_size(); ++i)
        {
            ss << "\t \t \t * " << response->error(i) << std::endl;
        }
        PrintThread{} << ss.str();
        hasErrors = true;
    }
    if (hasErrors)
    {
        updateStatus(SC2APIProtocol::Status::ended);
    }

    // During the game we only update the status if the bot also gets the update at the same time.
    // The bot gets the update via the observation, so we can only update if the response has an observation.
    // If we wouldn't do this, the LM would know the game ended and wouldn't proxy anymore steps.
    // Bad if the bot has an 'off step' due to step size != 1.
    if (response->has_status() && (response->has_observation() || m_gameStatus != SC2APIProtocol::Status::in_game))
    {
        updateStatus(response->status());
    }
    return response;
}

const Stats& Proxy::stats() const
{
    return m_stats;
}

SC2APIProtocol::Result Proxy::getGameResult()
{
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();

    request->mutable_observation();
    m_client.Send(request.get());
    SC2APIProtocol::Response* observationResponse = receiveResponse(SC2APIProtocol::Response::ResponseCase::kObservation);
    if (observationResponse)
    {
        const SC2APIProtocol::ResponseObservation& obs = observationResponse->observation();
        const SC2APIProtocol::Observation& observation = obs.observation();
        const auto playerID = observation.player_common().player_id();
        for (int i(0); i < obs.player_result_size(); ++i)
        {
            if (obs.player_result(i).player_id() == playerID)
            {
                return obs.player_result(i).result();
            }
        }
    }
    return SC2APIProtocol::Result::Undecided;
}
