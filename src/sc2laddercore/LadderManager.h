#pragma once
#include <memory.h>
#include <sstream>
#include <mutex>
#include <iostream>
#include <sc2api/sc2_api.h>
#include "LadderConfig.h"
#define PORT_START 5690
#define PLAYER_ID_LENGTH 16


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
	std::string GerneratePlayerId(size_t Length);
    bool LoadSetup();
	void SaveJsonResult(const BotConfig & Bot1, const BotConfig & Bot2, const std::string & Map, ResultType Result, int32_t GameTime);
	void RunLadderManager();

	static bool ProcessObservationResponse(SC2APIProtocol::ResponseObservation Response, std::vector<sc2::PlayerResult>* PlayerResults);
	static sc2::GameRequestPtr CreateLeaveGameRequest();
	static sc2::GameRequestPtr CreateQuitRequest();
	static sc2::GameResponsePtr CreateErrorResponse();
	static std::string RemoveMapExtension(const std::string & filename);
	static bool SendDataToConnection(sc2::Connection *Connection, const SC2APIProtocol::Request *request);

private:
	bool SaveReplay(sc2::Connection *client, const std::string & path);
	std::string GetBotCommandLine(const BotConfig &Config, int GamePort, int StartPort, const std::string &OpponentId, bool CompOpp = false, sc2::Race CompRace = sc2::Race::Terran, sc2::Difficulty CompDifficulty = sc2::Difficulty::Easy);
	ResultType GetPlayerResults(sc2::Connection *client);
	ResultType StartGameVsDefault(const BotConfig &Agent1, sc2::Race CompRace, sc2::Difficulty CompDifficulty, const std::string &Map, int32_t &GameLoop);
	ResultType StartGame(const BotConfig &Agent1, const BotConfig &Agent2, const std::string &Map, int32_t &GameLoop);
	void LoadAgents();
	bool UploadMime(ResultType result, const Matchup &ThisMatch);
	bool LoginToServer();
    std::map<std::string, BotConfig> BotConfigs;
    std::vector<std::string> MapList;
	std::string ResultsLogFile;
	LadderConfig *PlayerIds;

	void SaveError(const std::string &Agent1, const std::string &Agent2, const std::string &Map);

    int CoordinatorArgc;
    char **CoordinatorArgv;
	char *ConfigFile;

	bool EnableReplayUploads;
	bool EnableServerLogin;
	bool EnablePlayerIds;
	std::string ServerUsername;
	std::string ServerPassword;
	std::string ServerLoginAddress;
	uint32_t MaxGameTime;
    bool Sc2Launched;
    sc2::Coordinator *coordinator;
    LadderConfig *Config;
};
