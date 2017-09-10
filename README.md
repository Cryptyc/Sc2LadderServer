# Sc2LadderServer
A Ladder server for SC2 api

Currently very early wip.  Will pull in DLL files from a directory.  DLL librarys should have the following functions exported:

void *CreateNewAgent();  // Returns a pointer to a class deriving from sc2::Agent

const char *GetAgentName();  // Returns a string identifier for the agent name

int GetAgentRace();  // Returns the agents prefered race.  should be sc2::Race cast to int.

Requires Blizzard API files

------
Command Center bots are supported.  Just add the json config file in the command center directory with the .ccbot extension.

LadderManager will read a config file from the LadderManager.conf file with the following values

DllDirectory				Directory to read DLL files from

CommandCenterDirectory		Directory to read .ccbot command center config files

LocalReplayDirectory		Directory to store local replays

MapListFile					Location of the map list file.  Should be each map on a single line

UploadResultLocation		Location of remote server to store results
