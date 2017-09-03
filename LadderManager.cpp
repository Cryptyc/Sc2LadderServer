#include "sc2lib/sc2_lib.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_interfaces.h"

#include "sc2api/sc2_map_info.h"
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
const static char *CommandCenterDir = "e:\\sc2Dll\\CommandCenter\\";
const static char *ReplayDir = "e:\\sc2Dll\\Replays\\";
const static char *MapListFile = "e:\\sc2Dll\\maplist";


//*************************************************************************************************
int LadderManager::StartGame(AgentInfo Agent1, AgentInfo Agent2, std::string Map) {

	// Add the custom bot, it will control the players.
	sc2::Agent *Sc2Agent1;
	sc2::Agent *Sc2Agent2;
	if (Agent1.DllFile.find("ccbot") != std::string::npos)
	{
		Sc2Agent1 = (sc2::Agent *)CCGetAgent(Agent1.DllFile.c_str());
	}
	else
	{
		Sc2Agent1 = (sc2::Agent *)Agent1.AgentFunction();
	}
	if (Agent2.DllFile.find("ccbot") != std::string::npos)
	{
		Sc2Agent2 = (sc2::Agent *)CCGetAgent(Agent2.DllFile.c_str());
	}
	else
	{
		Sc2Agent2 = (sc2::Agent *)Agent2.AgentFunction();
	}
	StartCoordinator();
	coordinator->SetParticipants({
		CreateParticipant(Agent1.AgentRace, Sc2Agent1),
		CreateParticipant(Agent2.AgentRace, Sc2Agent2),
	});
	// Start the game.
	coordinator->LaunchStarcraft();
	// Step forward the game simulation.
	bool do_break = false;
	coordinator->StartGame("InterloperLE.SC2Map");
	while (coordinator->Update() && !coordinator->AllGamesEnded()) {
		if (sc2::PollKeyPress()) {
			break;
		}
	}
	std::string ReplayFile = ReplayDir + Agent1.AgentName + "v" + Agent2.AgentName + "-" + Map + ".Sc2Replay";
	Sc2Agent1->Control()->SaveReplay(ReplayFile);
	coordinator->WaitForAllResponses();
	int32_t result = 0;
	coordinator->LeaveGame();
	coordinator->WaitForAllResponses();
	delete Sc2Agent1;
	delete Sc2Agent2;
	return result;
}

LadderManager::LadderManager(int InCoordinatorArgc, char** inCoordinatorArgv, const char *InDllDirectory)

	: DllDirectory(InDllDirectory)
	, coordinator(nullptr)
	, CoordinatorArgc(InCoordinatorArgc)
	, CoordinatorArgv(inCoordinatorArgv)
{
	std::string CCDllLoc = std::string(CommandCenterDir) + "CommandCenter.dll";
	HINSTANCE hGetProcIDDLL = LoadLibrary(CCDllLoc.c_str());
	if (hGetProcIDDLL) {
		CCGetAgent = (CCGetAgentFunction)GetProcAddress(hGetProcIDDLL, "?CreateNewAgent@@YAPEAXPEBD@Z");
		CCGetAgentName = (CCGetAgentNameFunction)GetProcAddress(hGetProcIDDLL, "?GetAgentName@@YAPEBDPEBD@Z");
		CCGetAgentRace = (CCGetAgentRaceFunction)GetProcAddress(hGetProcIDDLL, "?GetAgentRace@@YAHPEBD@Z");
	}

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

void LadderManager::LoadCCBots()
{
	if (CCGetAgent && CCGetAgentName && CCGetAgentRace) {
		std::string extension = "*.ccbot*";
		std::vector<std::string> filesPaths;
		getFilesList(CommandCenterDir, extension, filesPaths);
		for (std::string path : filesPaths)
		{
			const char *AgentName = CCGetAgentName(path.c_str());
			if (AgentName)
			{
				sc2::Race AgentRace = (sc2::Race)CCGetAgentRace(path.c_str());
				AgentInfo NewAgentInfo(nullptr, AgentRace, std::string(AgentName), path);
				Agents.push_back(NewAgentInfo);
			}
		}	
	}
}
void LadderManager::StartCoordinator()
{
	if (coordinator != nullptr)
	{
		delete coordinator;
		Sleep(10000);

	}
	coordinator = new sc2::Coordinator();
	coordinator->LoadSettings(CoordinatorArgc, CoordinatorArgv);
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
				const char *AgentName = GetAgentName();
				if (AgentName)
				{
					sc2::Race AgentRace = (sc2::Race)GetAgentRace();
					AgentInfo NewAgentInfo(GetAgent, AgentRace, std::string(AgentName), filePath);
					Agents.push_back(NewAgentInfo);
				}
			}
		}
	}
	LoadCCBots();

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