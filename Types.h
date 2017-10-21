#pragma once;

#include <string>
typedef void* (*GetAgentFunction)();
typedef char* (*GetAgentNameFunction)();
typedef int(*GetAgentRaceFunction)();
typedef void* (*CCGetAgentFunction)(const char*);
typedef char* (*CCGetAgentNameFunction)(const char*);
typedef int(*CCGetAgentRaceFunction)(const char*);

enum BotType
{
    BinaryCpp,
    CommandCenter
};

typedef struct SBotConfig
{
    BotType Type;
    std::string Name;
    std::string Path;
    sc2::Race Race;
    SBotConfig() {}
    SBotConfig(BotType InType, std::string InBotName, sc2::Race InBotRace, std::string InBotPath)
        : Type(InType)
        , Race(InBotRace)
        , Name(InBotName)
        , Path(InBotPath)
    {}
    bool operator ==(const SBotConfig &Other)
    {
        return Name == Other.Name;
    }
} BotConfig;


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
    BotConfig Agent1;
    BotConfig Agent2;
    std::string Map;
    SMatchup() {}
    SMatchup(BotConfig InAgent1, BotConfig InAgent2, std::string InMap)
        : Agent1(InAgent1),
        Agent2(InAgent2),
        Map(InMap)
    {

    }


} Matchup;


static sc2::Race GetRaceFromString(const std::string & RaceIn)
{
    std::string race(RaceIn);
    std::transform(race.begin(), race.end(), race.begin(), ::tolower);

    if (race == "terran")
    {
        return sc2::Race::Terran;
    }
    else if (race == "protoss")
    {
        return sc2::Race::Protoss;
    }
    else if (race == "zerg")
    {
        return sc2::Race::Zerg;
    }
    else if (race == "random")
    {
        return sc2::Race::Random;
    }

    return sc2::Race::Random;
}

static BotType GetTypeFromString(const std::string &TypeIn)
{
    std::string type(TypeIn);
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);
    if (type == "binarycpp")
    {
        return BotType::BinaryCpp;
    }
    else if (type == "commandcenter")
    {
        return BotType::CommandCenter;
    }
    return BotType::CommandCenter;
}