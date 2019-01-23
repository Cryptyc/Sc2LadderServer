#include "Proxy.h"
#include "Tools.h"
#include "sc2utils/sc2_manage_process.h"

#include <fstream>

using namespace std::chrono_literals;

bool Proxy::m_mapAlreadyLoaded{false};

Proxy::Proxy(const uint32_t maxGameLoops,const uint32_t maxRealGameTime, const BotConfig& botConfig):
    m_maxGameLoops(maxGameLoops)
  , m_maxRealGameTime(maxRealGameTime)
  , m_botConfig(botConfig)
{ }

Proxy::~Proxy()
{
    // Set it false for the match after this one.
    m_mapAlreadyLoaded = false;

    // Check if the bot is still running.
    auto start = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds{0};
    std::future_status botProgStatus{std::future_status::deferred};
    //toDo: add to config
    constexpr auto maxWaitTime{20};
    if (m_botProgramThread.valid())
    {
        while (elapsed_seconds.count() < maxWaitTime)
        {
            botProgStatus = m_botProgramThread.wait_for(1s);
            if (botProgStatus == std::future_status::ready)
            {
                PrintThread{} << m_botConfig.BotName << " : Bot terminated properly." << std::endl;
                break;
            }
            elapsed_seconds = std::chrono::system_clock::now() - start;
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

bool Proxy::startSC2Instance(const sc2::ProcessSettings& processSettings, const int portServer, const int portClient)
{
    // magic numbers
    m_server.Listen(std::to_string(portServer).c_str(), "100000", "100000", "5");

    m_gameClientPid = sc2::StartProcess(processSettings.process_path,
    { "-listen", m_localHost,
      "-port", std::to_string(portClient),
      "-displayMode", "0",
      "-dataVersion", processSettings.data_version });

    // Depending on the hardware the client sometimes needs a second or two.
    size_t connectionAttempts = 0;
    constexpr size_t abondonConnectionAttemptAfter = 60; // sec
    constexpr bool withDebugOutput = false;
    while (!m_client.Connect(m_localHost, portClient, withDebugOutput))
    {
        ++connectionAttempts;
        sc2::SleepFor(1000);
        if (connectionAttempts > abondonConnectionAttemptAfter)
        {
            PrintThread{} << "Failed to connect client (" << m_botConfig.BotName << ")" << std::endl;
            return false;
        }
    }

    // Check if client is reacting
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();
    request->mutable_ping();
    m_client.Send(request.get());
    auto* response = receiveResponse();
    return response;
}

// Technically, we only need opponents race. But I think it looks clearer on the caller side with both races.
bool Proxy::setupGame(const sc2::ProcessSettings& processSettings, const std::string& map, const bool realTimeMode, const sc2::Race bot1Race, const sc2::Race bot2Race)
{
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
    playerSetup->set_race(SC2APIProtocol::Race(static_cast<int>(bot1Race) + 1)); // Ugh
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
        SC2APIProtocol::LocalMap* localMap = requestCreateGame->mutable_local_map();
        // Absolute path
        if (sc2::DoesFileExist(map))
        {
            localMap->set_map_path(map);
        }
        else
        {

            // Relative path - Game maps directory
            const std::string game_relative = sc2::GetGameMapsDirectory(processSettings.process_path) + map;
            if (sc2::DoesFileExist(game_relative))
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
                    // Relative path - Remotely saved maps directory
                    // toDo
                    //localMap->set_map_path(map);
                }
            }
        }
    }

    // Real time mode
    requestCreateGame->set_realtime(realTimeMode);

    // Send the request
    m_client.Send(request.get());
    SC2APIProtocol::Response* createGameResponse = receiveResponse();

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
    std::string botStartCommand = GetBotCommandLine(portServer, portStart, opponentPlayerId);
    if (botStartCommand == m_botConfig.executeCommand)
    {
        return false;
    }
    m_botProgramThread = std::async(std::launch::async, &StartBotProcess, m_botConfig, botStartCommand, &m_botThreadId);
    if (m_botProgramThread.wait_for(2s) == std::future_status::ready)
    {
        return false;
    }
    return true;
}

void Proxy::startGame()
{
    m_gameUpdateThread = std::async(std::launch::async, &Proxy::gameUpdate, this);
}

bool Proxy::gameFinished() const
{
    return std::future_status::ready == m_gameUpdateThread.wait_for(0ms);
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

std::string Proxy::GetBotCommandLine(const int gamePort, const int startPort, const std::string &OpponentId, bool CompOpp, sc2::Race CompRace, sc2::Difficulty CompDifficulty) const
{
    // Add universal arguments
    std::string OutCmdLine = m_botConfig.executeCommand + " --GamePort " + std::to_string(gamePort) + " --StartPort " + std::to_string(startPort) + " --LadderServer " + m_localHost + " --OpponentId " + OpponentId;

    if (CompOpp)
    {
        OutCmdLine += " --ComputerOpponent 1 --ComputerRace " + GetRaceString(CompRace) + " --ComputerDifficulty " + GetDifficultyString(CompDifficulty);
    }

    return OutCmdLine;
}



void Proxy::gameUpdate()
{
    PrintThread{} << "Starting proxy for " << m_botConfig.BotName << std::endl;
    // toDo: somehow check if the other functions were already used.


    // Actually, the game still loads...
    const auto gameStartTime = std::chrono::system_clock::now();
    // The bot has 1 minute + time out time to send the first request.
    bool alreadySurrendered = false;
    ExitCase currentExitCase = ExitCase::Unknown;

    while (m_gameStatus == SC2APIProtocol::Status::in_game || m_gameStatus == SC2APIProtocol::Status::init_game || m_gameStatus == SC2APIProtocol::Status::launched)
    {
        // If we know that the bot crashed we surrender for it.
        if (currentExitCase == ExitCase::BotCrashed || currentExitCase == ExitCase::BotStepTimeout)
        {
            // The bot is dead. So we will surrender on its behalf
            if(!alreadySurrendered)
            {
                terminateGame();
                alreadySurrendered = true;
            }
            // and step the simulation until the match has officially ended.
            else
            {
                doAStep();
            }
            continue;
        }

        if (m_server.HasRequest())
        {
            const sc2::RequestData& request = m_server.PeekRequest();
            // Analyse request
            // Returns false if a quit request was made.
            bool validRequest = processRequest(request);
            // A quit request is handled as if the bot crashed.
            // Especially, we do not want to forward the request to the client.
            // We still need it for the replay.
            if (!validRequest)
            {
                currentExitCase = ExitCase::BotCrashed;
                continue;
            }
            // Forward the valid request
            m_server.SendRequest(m_client.connection_);

            // Block for sc2's response then queue it.
            SC2APIProtocol::Response* response = receiveResponse();
            bool validResponse = processResponse(response);

            if (!validResponse)
            {
                PrintThread{} << m_botConfig.BotName << " : response not valid." << std::endl;
                currentExitCase = ExitCase::Error;
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
                    currentExitCase = ExitCase::BotCrashed;
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
                currentExitCase = ExitCase::Error;
                break;
            }
        }
        else
        {
            const uint32_t maxStepTime = getMaxStepTime();

            const auto timeSinceLastResponse = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_lastResponseSendTime).count();
            if (!m_botConfig.Debug && maxStepTime && timeSinceLastResponse > static_cast<int>(maxStepTime))
            {
                PrintThread{} << m_botConfig.BotName << " : bot is too slow. " << timeSinceLastResponse << " ms passed. Max step time: " << static_cast<int>(maxStepTime) << std::endl;
                // ToDo: Make a chat announcment
                // ToDo: Can we handle this better. It
                currentExitCase = ExitCase::BotStepTimeout;
            }

            // Check if the bot thread has send the crashed signal.
            // This is a fast check to not slow down the game
            if (isBotCrashed(0))
            {
                PrintThread{} << m_botConfig.BotName << " : crashed." << std::endl;
                currentExitCase = ExitCase::BotCrashed;
                continue;
            }
            if (m_server.connections_.empty() || m_client.connection_ == nullptr)
            {
                // Time for a serious check if the bot crashed.
                if (isBotCrashed(1000))
                {
                    PrintThread{} << m_botConfig.BotName << " : crashed." << std::endl;
                    currentExitCase = ExitCase::BotCrashed;
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
                    currentExitCase = ExitCase::Error;
                    break;
                }

                // If there is no connection to the client it probably crashed.
                if (m_client.connection_ == nullptr)
                {
                    PrintThread{} << m_botConfig.BotName << " :  Receive: m_client.connection_ == nullptr" << std::endl;
                    currentExitCase = ExitCase::Error;
                    break;
                }
            }
        }
        const auto gameDurationRealTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - gameStartTime).count();
        if (m_maxRealGameTime && gameDurationRealTime > m_maxRealGameTime)
        {
            m_result = ExitCase::GameTimeOver;
        }
    }

    if (currentExitCase == ExitCase::Unknown)
    {
        // The game ended normally for this bot. Get the result from the observation.
        const SC2APIProtocol::Result result = getGameResult();
        switch (result)
        {
        case SC2APIProtocol::Result::Victory:
        {
            currentExitCase = ExitCase::GameEndVictory;
            break;
        }
        case SC2APIProtocol::Result::Defeat:
        {
            currentExitCase = ExitCase::GameEndDefeat;
            break;
        }
        case SC2APIProtocol::Result::Tie:
        {
            currentExitCase = ExitCase::GameEndTie;
            break;
        }
        case SC2APIProtocol::Result::Undecided:
        default:
        {
            currentExitCase = ExitCase::Error;
            break;
        }
        }
    }
    m_stats.avgLoopDuration = m_totalTime/static_cast<float>(m_currentGameLoop)/1000.0f;
    m_stats.gameLoops = m_currentGameLoop;
    m_result = currentExitCase;
    PrintThread{} << m_botConfig.BotName << " : Exiting with " << GetExitCaseString(currentExitCase) << " Average step time " << m_stats.avgLoopDuration << " microseconds, total time: " << m_totalTime/1000000.0f << " seconds, game loops: " << m_currentGameLoop << std::endl;
}

bool Proxy::isBotCrashed(const int millisecons) const
{
    return m_botProgramThread.wait_for(std::chrono::milliseconds(millisecons)) == std::future_status::ready;
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
        else if (request.second->has_leave_game())
        {
            // Leave game requests are also a problem.
            // ToDo: Read a "gg" from chat
            PrintThread{} << m_botConfig.BotName << " has issued a leave game request. Please don't do that." << std::endl;
            return false;
        }
        else if (request.second->has_debug() && !m_usedDebugInterface)
        {
            PrintThread{} << m_botConfig.BotName << " : IS USING DEBUG INTERFACE.  POSSIBLE CHEAT! Please tell them not to." << std::endl;
            m_usedDebugInterface = true;
        }
        else if (request.second->has_step() && m_currentGameLoop)
        {
            const auto thisStepTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - m_lastResponseSendTime).count();
            m_totalTime += static_cast<float_t>(thisStepTime);
        }
    }
    return true;
}

bool Proxy::processResponse(const SC2APIProtocol::Response* response)
{
    if (response == nullptr)
    {
        PrintThread{} << m_botConfig.BotName << " : Waiting for a response had a timeout." << std::endl;
        return false;
    }
    if (response->has_observation())
    {
        const SC2APIProtocol::Observation& observation = response->observation().observation();
        m_currentGameLoop = observation.game_loop();
        // toDo: handle forced tie situation
        //if (ExitCase::TimeOut)
        //{
            //auto test = m_response.mutable_observation();
            //auto test2 = test->add_player_result();
            //test2->set_result(SC2APIProtocol::Result::Tie);
        //}

        if (m_maxGameLoops && m_currentGameLoop > m_maxGameLoops)
        {
            terminateGame();
            m_result = ExitCase::GameTimeOver;
        }
    }
    if (response->has_step())
    {
        m_lastResponseSendTime = std::chrono::system_clock::now();
    }
    return true;
}


void Proxy::terminateGame()
{
    sc2::ProtoInterface proto;
    sc2::GameRequestPtr request = proto.MakeRequest();

    SC2APIProtocol::RequestDebug* debugRequest = request->mutable_debug();

    auto debugCommand = debugRequest->add_debug();
    auto endGame = debugCommand->mutable_end_game();
    // If the proxy has to end the game it is because the bot failed somehow (crash, too slow, etc), aka lost.
    endGame->set_end_result(SC2APIProtocol::DebugEndGame_EndResult::DebugEndGame_EndResult_Surrender);

    m_client.Send(request.get());
    SC2APIProtocol::Response* debugResponse = receiveResponse();
    if (debugResponse)
    {
        // toDo check for errors and whether it is actually a debug response
        PrintThread{} << m_botConfig.BotName << " : surrender" << std::endl;
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

    stepRequest->set_count(1); // ToDo: this can be made smarter
    m_client.Send(request.get());
    SC2APIProtocol::Response* response = receiveResponse();
    if (response)
    {
        // todo: correct response?
        PrintThread{} << m_botConfig.BotName << " : step"<< std::endl;
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
        return 50U;  // ToDo: Add this to config file.
    }
    return 0U;
}

void Proxy::updateStatus(const SC2APIProtocol::Status newStatus)
{
    if (newStatus != m_gameStatus)
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
    SC2APIProtocol::Response* response = receiveResponse();
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
        PrintThread{} << m_botConfig.BotName << " : Could not open replay file." << std::endl;
        return false;
    }

    file.write(&replay.data()[0], static_cast<std::streamsize>(replay.data().size()));
    return true;
}

// toDo: change all receives
// toDo: give the expected response type
SC2APIProtocol::Response* Proxy::receiveResponse()
{
    SC2APIProtocol::Response* response{nullptr};
    if (!m_client.Receive(response, m_responseTimeOutMS))
    {
        return nullptr;
    }
    if (response->error_size())
    {
        std::ostringstream ss;
        ss << m_botConfig.BotName << " : response '" << responseCaseToString(response->response_case()) << "' has " << response->error_size() << "  error(s)!" << std::endl;
        for (int i(0); i < response->error_size(); ++i)
        {
            ss << "\t * " << response->error(i) << std::endl;
        }
        return nullptr;
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
    SC2APIProtocol::Response* observationResponse = receiveResponse();
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
