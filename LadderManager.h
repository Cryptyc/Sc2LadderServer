#pragma once

const extern char *DLLDir;
const extern char *ReplayDir;
const extern char *MapListFile;

class LadderManager
{
public:
    LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv, const char *InDllDirectory);
    void RunLadderManager();

private:
    int StartGame(AgentInfo Agent1, AgentInfo Agent2, std::string Map);
    void RefreshAgents();
    void LoadCCBots();
    void StartCoordinator();
    void GetMapList();
    std::vector<AgentInfo> Agents;
    std::vector<std::string> MapList;
    void getFilesList(std::string filePath, std::string extension, std::vector<std::string> & returnFileName);

    int CoordinatorArgc;
    char **CoordinatorArgv;

    const char *DllDirectory;
    bool Sc2Launched;
    sc2::Coordinator *coordinator;
    CCGetAgentFunction CCGetAgent;
    CCGetAgentNameFunction CCGetAgentName;
    CCGetAgentRaceFunction CCGetAgentRace;
};

LadderManager *LadderMan;
