#pragma once
#define MAX_GAME_TIME 60480


class LadderManager
{
public:
    LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv);
    bool LoadSetup();
	void RunLadderManager();

private:
	void StartAsyncGame();
	int StartGame(AgentInfo Agent1, AgentInfo Agent2, std::string Map);
    void RefreshAgents();
    void LoadCCBots();
    void StartCoordinator();
    void GetMapList();
    void UploadMime(int result, Matchup ThisMatch);
    std::map<std::string, AgentInfo> Agents;
    std::vector<std::string> MapList;
    void getFilesList(std::string filePath, std::string extension, std::vector<std::string> & returnFileName);

	void SaveError(std::string Agent1, std::string Agent2, std::string Map);

    int CoordinatorArgc;
    char **CoordinatorArgv;

    int32_t MaxGameTime;
    std::string DllDirectory;
    bool Sc2Launched;
    sc2::Coordinator *coordinator;
    CCGetAgentFunction CCGetAgent;
    CCGetAgentNameFunction CCGetAgentName;
    CCGetAgentRaceFunction CCGetAgentRace;
    LadderConfig *Config;
};

LadderManager *LadderMan;
