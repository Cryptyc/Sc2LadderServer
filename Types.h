#pragma once;

#include <string>


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

    SMatchup(AgentInfo InAgent1, AgentInfo InAgent2, std::string InMap)
        : Agent1(InAgent1),
        Agent2(InAgent2),
        Map(InMap)
    {

    }


} Matchup;
