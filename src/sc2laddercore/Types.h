#pragma once

#include <string>
#include <sc2api/sc2_api.h>
#include <ctime>
#include <sstream>
#include <iostream>
#include <iomanip>

class PrintThread : public std::ostringstream
{
public:
    PrintThread() = default;

    ~PrintThread()
    {
        std::lock_guard<std::mutex> guard(_mutexPrint);
        std::time_t t = std::time(nullptr);
        std::tm tm = *std::localtime(&t);
        std::cout << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") << ": " << this->str();
    }

private:
    static std::mutex _mutexPrint;
};

enum BotType
{
	BinaryCpp,
	CommandCenter,
	Python,
	Wine,
	Mono,
	DotNetCore,
    Java,
    NodeJS,
};

enum class ResultType
{
	InitializationError,
	Timeout,
	ProcessingReplay,
    Player1Win,
    Player1Crash,
    Player1TimeOut,
	Player2Win,
	Player2Crash,
    Player2TimeOut,
	Tie,
	Error
};

enum MatchupListType
{
    File,
    URL,
    None
};

enum class ExitCase
{
    Unknown,
    GameEndVictory,
    GameEndDefeat,
    GameEndTie,
    BotStepTimeout,
    BotCrashed,
    GameTimeOver,
    Error
};

struct GameState
{
	bool IsInGame;
	int GameLoop;
	float Score;
	GameState()
		: IsInGame(true)
		, GameLoop(-1)
		, Score(-1)
	{}

};

struct GameResult
{
    ResultType Result;
    float Bot1AvgFrame;
    float Bot2AvgFrame;
    uint32_t GameLoop;
    std::string TimeStamp;
    GameResult()
        : Result(ResultType::InitializationError)
        , Bot1AvgFrame(0)
        , Bot2AvgFrame(0)
        , GameLoop(0)
        , TimeStamp("")
    {}

};

struct BotConfig
{
	BotType Type;
	std::string BotName;
	std::string RootPath;
	std::string FileName;
    std::string CheckSum;
	sc2::Race Race;
	sc2::Difficulty Difficulty;
	std::string Args; //Optional arguments
	std::string PlayerId;
	bool Debug;
    bool Enabled;
    bool Skeleton;
    int ELO;
    std::string executeCommand;
    std::string SurrenderPhrase{"pineapple"};

	BotConfig()
		: Type(BotType::BinaryCpp)
		, Race(sc2::Race::Random)
		, Difficulty(sc2::Difficulty::Easy)
		, Debug(false)
        , Enabled(true)
        , Skeleton(false)
        , ELO(0)
	{}

	BotConfig(BotType InType, const std::string & InBotName, sc2::Race InBotRace, const std::string & InBotPath, const std::string & InFileName, sc2::Difficulty InDifficulty = sc2::Difficulty::Easy, const std::string & InArgs = "")
        : Type(InType)
		, BotName(InBotName)
		, RootPath(InBotPath)
        , FileName(InFileName)
        , Race(InBotRace)
		, Difficulty(InDifficulty)
		, Args(InArgs)
		, Debug(false)
        , Skeleton(false)
    {}

	bool operator ==(const BotConfig &Other) const
	{
		return BotName == Other.BotName;
    }
};

struct Matchup
{
	BotConfig Agent1;
	BotConfig Agent2;
    std::string Bot1Id;
    std::string Bot1Checksum;
    std::string Bot1DataChecksum;
    std::string Bot2Id;
    std::string Bot2Checksum;
    std::string Bot2DataChecksum;
    std::string Map;
	Matchup() {}
	Matchup(const BotConfig &InAgent1, const BotConfig &InAgent2, const std::string &InMap)
		: Agent1(InAgent1),
		Agent2(InAgent2),
		Map(InMap)
	{

	}


};

static MatchupListType GetMatchupListTypeFromString(const std::string GeneratorType)
{
    std::string type(GeneratorType);
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);

    if (type == "url")
    {
        return MatchupListType::URL;
    }
    else if (type == "file")
    {
        return MatchupListType::File;
    }
    else
    {
        return MatchupListType::None;
    }
}

static std::string GetExitCaseString(ExitCase ExitCaseIn)
{
	switch (ExitCaseIn)
    {
    case ExitCase::BotCrashed:
        return "BotCrashed";
    case ExitCase::BotStepTimeout:
        return "BotStepTimeout";
    case ExitCase::Error:
        return "Error";
    case ExitCase::GameEndVictory:
        return "GameEndWin";
    case ExitCase::GameEndDefeat:
        return "GameEndLoss";
    case ExitCase::GameEndTie:
        return "GameEndTie";
    case ExitCase::GameTimeOver:
        return "GameTimeOver";
    case ExitCase::Unknown:
        return "Unknown";
	}
	return "Error";
}

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
	else if (type == "python")
	{
		return BotType::Python;
	}
	else if (type == "wine")
	{
		return BotType::Wine;
	}
	else if (type == "dotnetcore")
	{
		return BotType::DotNetCore;
	}
	else if (type == "mono")
	{
		return BotType::Mono;
	}
    else if (type == "java")
    {
        return BotType::Java;
    }
    else if (type == "nodejs")
    {
        return BotType::NodeJS;
    }
	return BotType::BinaryCpp;
}

static sc2::Difficulty GetDifficultyFromString(const std::string &InDifficulty)
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
	case sc2::Difficulty::VeryEasy:
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
    case ResultType::InitializationError:
		return "InitializationError";

    case ResultType::Timeout:
		return "Timeout";

    case ResultType::ProcessingReplay:
		return "ProcessingReplay";

    case ResultType::Player1Win:
		return "Player1Win";

    case ResultType::Player1Crash:
        return "Player1Crash";

    case ResultType::Player1TimeOut:
        return "Player1TimeOut";

    case ResultType::Player2Win:
		return "Player2Win";

    case ResultType::Player2Crash:
		return "Player2Crash";

    case ResultType::Player2TimeOut:
        return "Player2TimeOut";

    case ResultType::Tie:
		return "Tie";

	default:
		return "Error";
	}
}

static std::string RemoveMapExtension(const std::string& filename)
{
    size_t lastdot = filename.find_last_of(".");
    if (lastdot == std::string::npos)
    {
        return filename;
    }
    return filename.substr(0, lastdot);
}

static ResultType getEndResultFromProxyResults(const ExitCase resultBot1, const ExitCase resultBot2)
{
    if ((resultBot1 == ExitCase::BotCrashed && resultBot2 == ExitCase::BotCrashed)
            || (resultBot1 == ExitCase::BotStepTimeout && resultBot2 == ExitCase::BotStepTimeout)
            || (resultBot1 == ExitCase::Error && resultBot2 == ExitCase::Error))
    {
        // If both bots crashed we assume the bots are not the problem.
        return ResultType::Error;
    }
    if (resultBot1 == ExitCase::BotCrashed)
    {
        return ResultType::Player1Crash;
    }
    if (resultBot2 == ExitCase::BotCrashed)
    {
        return ResultType::Player2Crash;
    }
    if (resultBot1 == ExitCase::GameEndVictory && (resultBot2 == ExitCase::GameEndDefeat || resultBot2 == ExitCase::BotStepTimeout || resultBot2 == ExitCase::Error))
    {
        return  ResultType::Player1Win;
    }
    if (resultBot2 == ExitCase::GameEndVictory && (resultBot1 == ExitCase::GameEndDefeat || resultBot1 == ExitCase::BotStepTimeout || resultBot1 == ExitCase::Error))
    {
        return  ResultType::Player2Win;
    }
    if (resultBot1 == ExitCase::GameEndTie && resultBot2 == ExitCase::GameEndTie)
    {
        return  ResultType::Tie;
    }
    if (resultBot1 == ExitCase::GameTimeOver && resultBot2 == ExitCase::GameTimeOver)
    {
        return  ResultType::Timeout;
    }
    return ResultType::Error;
}

static std::string statusToString(SC2APIProtocol::Status status)
{
    switch (status)
    {
    case SC2APIProtocol::Status::launched: return "launched";
    case SC2APIProtocol::Status::init_game: return "init_game";
    case SC2APIProtocol::Status::in_game: return "in_game";
    case SC2APIProtocol::Status::in_replay: return "in_replay";
    case SC2APIProtocol::Status::ended: return "ended";
    case SC2APIProtocol::Status::quit: return "quit";
    case SC2APIProtocol::Status::unknown: return "unknown";
    }
    return "unknown (" + std::to_string(static_cast<int>(status)) + ")";
}

static std::string responseCaseToString(SC2APIProtocol::Response::ResponseCase responseCase)
{
    switch(responseCase)
    {
    case SC2APIProtocol::Response::ResponseCase::kCreateGame: return "CreateGame";
    case SC2APIProtocol::Response::ResponseCase::kJoinGame: return "JoinGame";
    case SC2APIProtocol::Response::ResponseCase::kRestartGame : return "RestartGame";
    case SC2APIProtocol::Response::ResponseCase::kStartReplay: return "StartReplay";
    case SC2APIProtocol::Response::ResponseCase::kLeaveGame: return "LeaveGame";
    case SC2APIProtocol::Response::ResponseCase::kQuickSave: return "QuickSave";
    case SC2APIProtocol::Response::ResponseCase::kQuickLoad: return "QuickLoad";
    case SC2APIProtocol::Response::ResponseCase::kQuit: return "Quit";
    case SC2APIProtocol::Response::ResponseCase::kGameInfo: return "GameInfo";
    case SC2APIProtocol::Response::ResponseCase::kObservation: return "Observation";
    case SC2APIProtocol::Response::ResponseCase::kAction: return "Action";
    case SC2APIProtocol::Response::ResponseCase::kObsAction: return "ObsAction";
    case SC2APIProtocol::Response::ResponseCase::kStep: return "Step";
    case SC2APIProtocol::Response::ResponseCase::kData: return "Data";
    case SC2APIProtocol::Response::ResponseCase::kQuery: return "Query";
    case SC2APIProtocol::Response::ResponseCase::kSaveReplay: return "SaveReplay";
    case SC2APIProtocol::Response::ResponseCase::kReplayInfo: return "ReplayInfo";
    case SC2APIProtocol::Response::ResponseCase::kAvailableMaps: return "AvailableMaps";
    case SC2APIProtocol::Response::ResponseCase::kSaveMap: return "SaveMap";
    //case SC2APIProtocol::Response::ResponseCase::kMapCommand: return "MapCommand";
    case SC2APIProtocol::Response::ResponseCase::kPing: return "Ping";
    case SC2APIProtocol::Response::ResponseCase::kDebug: return "Debug";
    case SC2APIProtocol::Response::ResponseCase::RESPONSE_NOT_SET: return "RESPONSE_NOT_SET";
    }
    return "unknown (" + std::to_string(static_cast<int>(responseCase)) + ")";
}
