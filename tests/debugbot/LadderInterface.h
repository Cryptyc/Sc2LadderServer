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

struct ConnectionOptions
{
	int32_t GamePort;
	int32_t StartPort;
	std::string ServerAddress;
	bool ComputerOpponent;
	sc2::Difficulty ComputerDifficulty;
	sc2::Race ComputerRace;
	std::string OpponentId;
};

static void ParseArguments(int argc, char *argv[], ConnectionOptions &connect_options)
{
	sc2::ArgParser arg_parser(argv[0]);
	arg_parser.AddOptions({
		{ "-g", "--GamePort", "Port of client to connect to", false },
		{ "-o", "--StartPort", "Starting server port", false },
		{ "-l", "--LadderServer", "Ladder server address", false },
		{ "-c", "--ComputerOpponent", "If we set up a computer oppenent" },
		{ "-a", "--ComputerRace", "Race of computer oppent"},
		{ "-d", "--ComputerDifficulty", "Difficulty of computer oppenent"},
		{ "-x", "--OpponentId", "PlayerId of opponent"}
		});
	arg_parser.Parse(argc, argv);
	std::string GamePortStr;
	if (arg_parser.Get("GamePort", GamePortStr)) {
		connect_options.GamePort = atoi(GamePortStr.c_str());
	}
	std::string StartPortStr;
	if (arg_parser.Get("StartPort", StartPortStr)) {
		connect_options.StartPort = atoi(StartPortStr.c_str());
	}
	arg_parser.Get("LadderServer", connect_options.ServerAddress);
	std::string CompOpp;
	if (arg_parser.Get("ComputerOpponent", CompOpp))
	{
		connect_options.ComputerOpponent = true;
		std::string CompRace;
		if (arg_parser.Get("ComputerRace", CompRace))
		{
			connect_options.ComputerRace = GetRaceFromString(CompRace);
		}
		std::string CompDiff;
		if (arg_parser.Get("ComputerDifficulty", CompDiff))
		{
			connect_options.ComputerDifficulty = GetDifficultyFromString(CompDiff);
		}

	}
	else
	{
		connect_options.ComputerOpponent = false;
	}
	arg_parser.Get("OpponentId", connect_options.OpponentId);
}

static void RunBot(int argc, char *argv[], sc2::Agent *Agent,sc2::Race race)
{
	ConnectionOptions Options;
	ParseArguments(argc, argv, Options);

	sc2::Coordinator coordinator;

	// Add the custom bot, it will control the players.
	int num_agents;
	if (Options.ComputerOpponent)
	{
		num_agents = 1;
		coordinator.SetParticipants({
			CreateParticipant(race, Agent),
			CreateComputer(Options.ComputerRace, Options.ComputerDifficulty)
			});
	}
	else
	{
		num_agents = 2;
		coordinator.SetParticipants({
			CreateParticipant(race, Agent),
			});
	}

	// Start the game.

	// Step forward the game simulation.
	std::cout << "Connecting to port " << Options.GamePort << std::endl;
	coordinator.Connect(Options.GamePort);
	coordinator.SetupPorts(num_agents, Options.StartPort, false);
	// Step forward the game simulation.
	coordinator.JoinGame();
	coordinator.SetTimeoutMS(10000);
	std::cout << " Successfully joined game" << std::endl;
	while (coordinator.Update()) {
	}
}
