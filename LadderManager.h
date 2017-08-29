typedef void* (*GetAgentFunction)();
typedef char* (*GetAgentNameFunction)();
typedef struct SAgentInfo
{
    sc2::Agent *Agent;
    std::string AgentName;
    std::string DllFile;
    SAgentInfo(sc2::Agent *InAgent, std::string InAgentName, std::string InDllFile)
        : Agent(InAgent)
        , AgentName(InAgentName)
        , DllFile(InDllFile)
    {}
} AgentInfo;
class LadderManager
{
public:
    LadderManager(int InCoordinatorArgv, char** inCoordinatorArgc, const char *InDllDirectory);
    void RunLadderManager();

private:
    void StartGame(AgentInfo Agent1, AgentInfo Agent2);
    void RefreshAgents();
    void ClearAgents();
    std::vector<AgentInfo> Agents;
    void getFilesList(std::string filePath, std::string extension, std::vector<std::string> & returnFileName);

    const char *DllDirectory;
    sc2::Coordinator coordinator;
};

LadderManager *LadderMan;
