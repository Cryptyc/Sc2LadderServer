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
#include <curl\curl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "types.h"
#include "LadderManager.h"
#include "MatchupList.h"

const static char *DLLDir = "e:\\sc2Dll\\";
const static char *CommandCenterDir = "e:\\sc2Dll\\CommandCenter\\";
const static char *ReplayDir = "e:\\sc2Dll\\Replays\\";
const static char *MapListFile = "e:\\sc2Dll\\maplist";
const static char *MainReplayDir = "C:\\Users\\crypt\\Documents\\StarCraft II\\Replays\\Multiplayer\\";
const static char *UploadReplayLocation = "http://127.0.0.1/replayupload.php"; 
const static char *UploadResultLocation = "http://127.0.0.1/fileupload.php";
// const static char *UploadReplayLocation = "https://sc2laddertest.000webhostapp.com/fileupload.php";


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
	int Agent1Army = 0;
	int Agent2Army = 0;
	coordinator->StartGame(Map);
	while (coordinator->Update() && !coordinator->AllGamesEnded()) {
		if (Sc2Agent1->Observation() != nullptr && Sc2Agent2->Observation() != nullptr)
		{
			Agent1Army = Sc2Agent1->Observation()->GetArmyCount();
			Agent2Army = Sc2Agent2->Observation()->GetArmyCount();
		}

	}
	std::string ReplayFile = ReplayDir + Agent1.AgentName + "v" + Agent2.AgentName + "-" + Map + ".Sc2Replay";
	Sc2Agent1->Control()->SaveReplay(ReplayFile);
	coordinator->WaitForAllResponses();
	int32_t result = 0;
	if (Agent1Army > Agent2Army)
	{
		result = 1;
	}
	else
	{
		result = 2;
	}
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


void LadderManager::UploadMime(int result, Matchup ThisMatch)
{
	std::string ReplayFile = ThisMatch.Agent1.AgentName + "v" + ThisMatch.Agent2.AgentName + "-" + ThisMatch.Map + ".Sc2Replay";
	std::string ReplayLoc = ReplayDir + ReplayFile;
	CURL *curl;
	CURLcode res;

	curl_mime *form = NULL;
	curl_mimepart *field = NULL;
	struct curl_slist *headerlist = NULL;
	static const char buf[] = "Expect:";

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();
	if (curl) {
		/* Create the form */
		form = curl_mime_init(curl);

		/* Fill in the file upload field */
		field = curl_mime_addpart(form);
		if (std::fstream(ReplayLoc.c_str()))
		{
			curl_mime_name(field, "replayfile", CURL_ZERO_TERMINATED);
			curl_mime_filedata(field, ReplayLoc.c_str());
		}
		/* Fill in the filename field */
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Bot1Name", CURL_ZERO_TERMINATED);
		curl_mime_data(field, ThisMatch.Agent1.AgentName.c_str(), CURL_ZERO_TERMINATED);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Bot1Race", CURL_ZERO_TERMINATED);
		curl_mime_data(field, std::to_string((int)ThisMatch.Agent1.AgentRace).c_str(), CURL_ZERO_TERMINATED);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Bot2Name", CURL_ZERO_TERMINATED);
		curl_mime_data(field, ThisMatch.Agent2.AgentName.c_str(), CURL_ZERO_TERMINATED);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Bot2Race", CURL_ZERO_TERMINATED);
		curl_mime_data(field, std::to_string((int)ThisMatch.Agent2.AgentRace).c_str(), CURL_ZERO_TERMINATED);
		field = curl_mime_addpart(form);
		curl_mime_name(field, "Map", CURL_ZERO_TERMINATED);
		curl_mime_data(field, ThisMatch.Map.c_str(), CURL_ZERO_TERMINATED);

		/* initialize custom header list (stating that Expect: 100-continue is not
		wanted */
		headerlist = curl_slist_append(headerlist, buf);
		/* what URL that receives this POST */
		curl_easy_setopt(curl, CURLOPT_URL, UploadReplayLocation);

		curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);
		/* Check for errors */
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));

		/* always cleanup */
		curl_easy_cleanup(curl);

		/* then cleanup the form */
		curl_mime_free(form);
		/* free slist */
		curl_slist_free_all(headerlist);


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
		int result = StartGame(NextMatch.Agent1, NextMatch.Agent2, NextMatch.Map);

		UploadMime(result, NextMatch);
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