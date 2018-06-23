#pragma once
#include <memory.h>
#include <sstream>
#include <mutex>
#include <iostream>
#include <sc2api/sc2_api.h>
#include "LadderConfig.h"
#define MAX_GAME_TIME 60480
#define PORT_START 5690


class PrintThread : public std::ostringstream
{
public:
	PrintThread() = default;

	~PrintThread()
	{
		std::lock_guard<std::mutex> guard(_mutexPrint);
		std::cout << this->str();
	}

private:
	static std::mutex _mutexPrint;
};

class LadderManager
{
public:
	LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv);
	LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv, char *ConfigFile);
    bool LoadSetup();
	void SaveJsonResult(const BotConfig & Bot1, const BotConfig & Bot2, const std::string & Map, ResultType Result, int32_t GameTime);
	void RunLadderManager();

private:
	bool SaveReplay(sc2::Connection *client, const std::string & path);
	bool ProcessObservationResponse(SC2APIProtocol::ResponseObservation Response, std::vector<sc2::PlayerResult>* PlayerResults);
	std::string GetBotCommandLine(const BotConfig &Config, int GamePort, int StartPort, bool CompOpp = false, sc2::Race CompRace = sc2::Race::Terran, sc2::Difficulty CompDifficulty = sc2::Difficulty::Easy);
	sc2::GameResponsePtr CreateErrorResponse();
	sc2::GameRequestPtr CreateLeaveGameRequest();
	sc2::GameRequestPtr CreateQuitRequest();
	ResultType GetPlayerResults(sc2::Connection *client);
	ResultType StartGameVsDefault(BotConfig Agent1, sc2::Race CompRace, sc2::Difficulty CompDifficulty, std::string Map);
	bool SendDataToConnection(sc2::Connection *Connection, const SC2APIProtocol::Request *request);
	ResultType StartGame(BotConfig Agent1, BotConfig Agent2, std::string Map);
    void LoadAgents();
	std::string RemoveMapExtension(const std::string & filename);
    void UploadMime(ResultType result, Matchup ThisMatch);
    std::map<std::string, BotConfig> BotConfigs;
    std::vector<std::string> MapList;
	std::string ResultsLogFile;

	void SaveError(const std::string &Agent1, const std::string &Agent2, const std::string &Map);

    int CoordinatorArgc;
    char **CoordinatorArgv;
	char *ConfigFile;

	bool EnableReplayUploads;
    int32_t MaxGameTime;
    bool Sc2Launched;
    sc2::Coordinator *coordinator;
    LadderConfig *Config;
};
