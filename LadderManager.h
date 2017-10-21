#pragma once
#define MAX_GAME_TIME 60480


class LadderManager
{
public:
    LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv);
    bool LoadSetup();
	void RunLadderManager();

private:
    std::string GetBotCommandLine(BotConfig Config, int GamePort, int StartPort);
    int StartGame(BotConfig Agent1, BotConfig Agent2, std::string Map);
    void StartCoordinator();
    void LoadAgents();
    void GetMapList();
    void UploadMime(int result, Matchup ThisMatch);
    std::map<std::string, BotConfig> BotConfigs;
    std::vector<std::string> MapList;

	void SaveError(std::string Agent1, std::string Agent2, std::string Map);

    int CoordinatorArgc;
    char **CoordinatorArgv;

    int32_t MaxGameTime;
    std::string DllDirectory;
    bool Sc2Launched;
    sc2::Coordinator *coordinator;
    LadderConfig *Config;
};

LadderManager *LadderMan;
