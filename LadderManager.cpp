#include "sc2lib/sc2_lib.h"
#include "sc2api/sc2_api.h"
#include "sc2utils/sc2_manage_process.h"
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <Windows.h>
#include "types.h"
#include "LadderManager.h"
#include "MatchupList.h"

const static char *DLLDir = "e:\\sc2Dll\\";
const static char *ReplayDir = "e:\\sc2Dll\\Replays\\replay";
const static char *MapListFile = "e:\\sc2Dll\\maplist";


//*************************************************************************************************
void LadderManager::StartGame(AgentInfo Agent1, AgentInfo Agent2, std::string Map) {

	// Add the custom bot, it will control the players.

	coordinator.SetParticipants({
		CreateParticipant(Agent1.AgentRace, Agent1.Agent),
		CreateParticipant(Agent2.AgentRace, Agent2.Agent),
	});

	// Start the game.
	coordinator.LaunchStarcraft();

	// Step forward the game simulation.
	bool do_break = false;
	coordinator.StartGame(Map);
	while (coordinator.Update() && !do_break) {
		if (sc2::PollKeyPress()) {
			break;
		}
	}
	if (coordinator.HasReplays())
	{
		coordinator.SaveReplayList(ReplayDir);
	}
}

LadderManager::LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv, const char *InDllDirectory)

	: DllDirectory(InDllDirectory)
{
	coordinator.LoadSettings(InCoordinatorArgc, inCoordinatorArgv);

}
void LadderManager::GetMapList()
{
	std::ifstream file(MapListFile);
	std::string str;
	while (std::getline(file, str))
	{
		MapList.push_back(str);
	}

}

void LadderManager::RunLadderManager()
{

	RefreshAgents();
	MatchupList *Matchups = new MatchupList();
	Matchups->GenerateMatches(Agents, MapList);
	Matchup NextMatch;
	while (Matchups->GetNextMatchup(NextMatch))
	{
		StartGame(NextMatch.Agent1, NextMatch.Agent2, NextMatch.Map);
	}
	
}

void LadderManager::ClearAgents()
{
	for (AgentInfo Agent : Agents)
	{
		delete Agent.Agent;
	}
}

void LadderManager::RefreshAgents()
{
	std::string inputFolderPath = DllDirectory;
	std::string extension = "*.dll*";
	std::vector<std::string> filesPaths;
	GetMapList();
	getFilesList(inputFolderPath, extension, filesPaths);
	if (Agents.size())
	{
		Agents.clear();
	}
	for(std::string filePath : filesPaths)
	{
		HINSTANCE hGetProcIDDLL = LoadLibrary(filePath.c_str());
		if (hGetProcIDDLL) {
			// resolve function address here
			GetAgentFunction GetAgent = (GetAgentFunction)GetProcAddress(hGetProcIDDLL, "?CreateNewAgent@@YAPEAXXZ");
			GetAgentNameFunction GetAgentName = (GetAgentNameFunction)GetProcAddress(hGetProcIDDLL, "?GetAgentName@@YAPEBDXZ");
			GetAgentRaceFunction GetAgentRace = (GetAgentRaceFunction)GetProcAddress(hGetProcIDDLL, "?GetAgentRace@@YAHXZ");
			if (GetAgent && GetAgentName && GetAgentRace) {
				sc2::Agent *NewAgent = (sc2::Agent *)(GetAgent());
				const char *AgentName = GetAgentName();
				if (NewAgent && AgentName)
				{
					sc2::Race AgentRace = (sc2::Race)GetAgentRace();
					AgentInfo NewAgentInfo(NewAgent, AgentRace, std::string(AgentName), filePath);
					Agents.push_back(NewAgentInfo);
				}
			}
		}
	}

}

void LadderManager::getFilesList(std::string filePath, std::string extension, std::vector<std::string> & returnFileName)
{
	WIN32_FIND_DATA fileInfo;
	HANDLE hFind;
	std::string  fullPath = filePath + extension;
	hFind = FindFirstFile(fullPath.c_str(), &fileInfo);
	if (hFind != INVALID_HANDLE_VALUE) {
		returnFileName.push_back(filePath + fileInfo.cFileName);
		while (FindNextFile(hFind, &fileInfo) != 0) {
			returnFileName.push_back(filePath + fileInfo.cFileName);
		}
	}
}

int main(int argc, char** argv)
{
	LadderMan = new LadderManager(argc, argv, DLLDir);
	if (LadderMan != nullptr)
	{

		LadderMan->RunLadderManager();
	}
}