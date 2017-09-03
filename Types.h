#pragma once;

#include <string>
typedef void* (*GetAgentFunction)();
typedef char* (*GetAgentNameFunction)();
typedef int(*GetAgentRaceFunction)();
typedef void* (*CCGetAgentFunction)(const char*);
typedef char* (*CCGetAgentNameFunction)(const char*);
typedef int(*CCGetAgentRaceFunction)(const char*);

typedef struct SAgentInfo
{
    GetAgentFunction AgentFunction;
    sc2::Race AgentRace;
    std::string AgentName;
    std::string DllFile;
    SAgentInfo() {}
    SAgentInfo(GetAgentFunction InAgentFunction, sc2::Race InAgentRace, std::string InAgentName, std::string InDllFile)
        : AgentFunction(InAgentFunction)
        , AgentRace(InAgentRace)
        , AgentName(InAgentName)
        , DllFile(InDllFile)
    {}
    bool operator ==(const SAgentInfo &Other)
    {
        return AgentName == Other.AgentName;
    }
} AgentInfo;


typedef struct SMatchup
{
    AgentInfo Agent1;
    AgentInfo Agent2;
    std::string Map;
    SMatchup() {}
    SMatchup(AgentInfo InAgent1, AgentInfo InAgent2, std::string InMap)
        : Agent1(InAgent1),
        Agent2(InAgent2),
        Map(InMap)
    {

    }


} Matchup;
