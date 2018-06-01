#pragma once

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
    CommandCenter,
	Python,
	DefaultBot
};

enum ResultType
{
    InitializationError,
    Timeout,
    ProcessingReplay,
    Player1Win,
    Player1Crash,
    Player2Win,
    Player2Crash,
	Tie,
	Error
};

enum ExitCase
{
	InProgress,
	GameEnd,
	ClientRequestExit,
	ClientTimeout,
	GameTimeout
};

typedef struct SGameState
{
    bool IsInGame;
    int GameLoop;
    float Score;
    SGameState()
        : IsInGame(true)
        , GameLoop(-1)
        , Score(-1)
    {}

} GameState;

typedef struct SBotConfig
{
    BotType Type;
    std::string BotName;
	std::string RootPath;
	std::string FileName;
    sc2::Race Race;
	sc2::Difficulty Difficulty;
	std::string Args; //Optional arguments
    SBotConfig() {}
    SBotConfig(BotType InType,const std::string & InBotName,sc2::Race InBotRace, const std::string & InBotPath, const std::string & InFileName, sc2::Difficulty InDifficulty = sc2::Difficulty::Easy, const std::string & InArgs="")
        : Type(InType)
        , Race(InBotRace)
        , BotName(InBotName)
        , RootPath(InBotPath)
		, FileName(InFileName)
		, Difficulty(InDifficulty)
		, Args(InArgs)
    {}
    bool operator ==(const SBotConfig &Other)
    {
        return BotName == Other.BotName;
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

static std::string GetRaceString(const sc2::Race RaceIn)
{
	switch (RaceIn)
	{
	case sc2::Race::Protoss:
		return "Protoss";
	case sc2::Race::Random:
		return "Random";
	case sc2::Race::Terran:
		return "Terran";
	case sc2::Race::Zerg:
		return "Zerg";
	}
	return "Random";
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
	else if (type == "computer")
	{
		return BotType::DefaultBot;
	}
	else if (type == "python")
	{
		return BotType::Python;
	}
	return BotType::BinaryCpp;
}

static sc2::Difficulty GetDifficultyFromString(std::string InDifficulty)
{
	if (InDifficulty == "VeryEasy")
	{
		return sc2::Difficulty::VeryEasy;
	}
	if (InDifficulty == "Easy")
	{
		return sc2::Difficulty::Easy;
	}
	if (InDifficulty == "Medium")
	{
		return sc2::Difficulty::Medium;
	}
	if (InDifficulty == "MediumHard")
	{
		return sc2::Difficulty::MediumHard;
	}
	if (InDifficulty == "Hard")
	{
		return sc2::Difficulty::Hard;
	}
	if (InDifficulty == "HardVeryHard")
	{
		return sc2::Difficulty::HardVeryHard;
	}
	if (InDifficulty == "VeryHard")
	{
		return sc2::Difficulty::VeryHard;
	}
	if (InDifficulty == "CheatVision")
	{
		return sc2::Difficulty::CheatVision;
	}
	if (InDifficulty == "CheatMoney")
	{
		return sc2::Difficulty::CheatMoney;
	}
	if (InDifficulty == "CheatInsane")
	{
		return sc2::Difficulty::CheatInsane;
	}

	return sc2::Difficulty::Easy;
}
static std::string GetDifficultyString(sc2::Difficulty InDifficulty)
{
	switch (InDifficulty)
	{
	case sc2::Difficulty::VeryEasy :
	{
		return "VeryEasy";
	}
	case sc2::Difficulty::Easy:
	{
		return "Easy";
	}
	case sc2::Difficulty::Medium:
	{
		return "Medium";
	}
	case sc2::Difficulty::MediumHard:
	{
		return "MediumHard";
	}
	case sc2::Difficulty::Hard:
	{
		return "Hard";
	}
	case sc2::Difficulty::HardVeryHard:
	{
		return "HardVeryHard";
	}
	case sc2::Difficulty::VeryHard:
	{
		return "VeryHard";
	}
	case sc2::Difficulty::CheatVision:
	{
		return "CheatVision";
	}
	case sc2::Difficulty::CheatMoney:
	{
		return "CheatMoney";
	}
	case sc2::Difficulty::CheatInsane:
	{
		return "CheatInsane";
	}
	}
	return "Easy";
}

static std::string GetResultType(ResultType InResultType)
{
	switch (InResultType)
	{
	case InitializationError:
		return "InitializationError";
		
	case Timeout :
		return "Timeout";

	case ProcessingReplay :
		return "ProcessingReplay";

	case Player1Win :
		return "Player1Win";

	case Player1Crash :
		return "Player1Crash";

	case Player2Win :
		return "Player2Win";

	case Player2Crash :
		return "Player2Crash";

	case Tie :
		return "Tie";

	default:
		return "Error";
	}
}