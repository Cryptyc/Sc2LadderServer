#pragma once

typedef void* (*GetAgentFunction)();
typedef char* (*GetAgentNameFunction)();
const extern char *DLLDir;
const extern char *ReplayDir;
const extern char *MapListFile;

class LadderManager
{
public:
    LadderManager(int InCoordinatorArgv, char** inCoordinatorArgc, const char *InDllDirectory);
    void RunLadderManager();

private:
    void StartGame(AgentInfo Agent1, AgentInfo Agent2, std::string Map);
    void RefreshAgents();
    void ClearAgents();
    void GetMapList();
    std::vector<AgentInfo> Agents;
    std::vector<std::string> MapList;
    void getFilesList(std::string filePath, std::string extension, std::vector<std::string> & returnFileName);

    const char *DllDirectory;
    sc2::Coordinator coordinator;
};

LadderManager *LadderMan;
