# Sc2LadderServer
A Ladder server for SC2 api

Currently very early wip.  Will pull in DLL files from a directory.  DLL librarys should have the following functions exported:

void *CreateNewAgent();  // Returns a pointer to a class deriving from sc2::Agent


const char *GetAgentName();  // Returns a string identifier for the agent name

int GetAgentRace();  // Returns the agents prefered race.  should be sc2::Race cast to int.

CommandCenter bots are not currently supported, hopefully will support that this week.

Requires Blizzard API files