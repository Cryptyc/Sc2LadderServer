#if defined(__unix__) || defined(__APPLE__)

#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "Types.h"

#include "Tools.h"

void StartBotProcess(const BotConfig &Agent, const std::string& CommandLine, void **ProcessId)
{
    FILE* pipe = popen(CommandLine.c_str(), "r");
    if (!pipe)
    {
        std::cerr << "Can't launch command '" <<
            CommandLine << "'" << std::endl;
        return;
    }
	*ProcessId = pipe;
    int returnCode = pclose(pipe);
    if (returnCode != 0)
    {
        std::cerr << "Failed to finish command '" <<
            CommandLine << "', code: " << returnCode << std::endl;
    }
}

void SleepFor(int seconds)
{
    sleep(seconds);
}

void KillSc2Process(unsigned long pid)
{
    kill(pid, SIGKILL);
}

bool MoveReplayFile(const char* lpExistingFileName, const  char* lpNewFileName)
{
	// todo
	throw "MoveFile is not implemented for linux yet.";
}

void KillBotProcess(void *ProcessStruct);
{
	// This needs to be implemented
}

#endif