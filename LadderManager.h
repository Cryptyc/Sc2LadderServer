#pragma once



class LadderManager
{
public:
    LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv);
    bool LoadSetup();
	void RunLadderManager();

private:
    int StartGame(AgentInfo Agent1, AgentInfo Agent2, std::string Map);
    void RefreshAgents();
    void LoadCCBots();
    void StartCoordinator();
    void GetMapList();
    void UploadMime(int result, Matchup ThisMatch);
    std::vector<AgentInfo> Agents;
    std::vector<std::string> MapList;
    void getFilesList(std::string filePath, std::string extension, std::vector<std::string> & returnFileName);

    int CoordinatorArgc;
    char **CoordinatorArgv;

    std::string DllDirectory;
    bool Sc2Launched;
    sc2::Coordinator *coordinator;
    CCGetAgentFunction CCGetAgent;
    CCGetAgentNameFunction CCGetAgentName;
    CCGetAgentRaceFunction CCGetAgentRace;
    LadderConfig *Config;
};

LadderManager *LadderMan;
