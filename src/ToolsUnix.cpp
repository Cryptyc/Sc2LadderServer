#ifdef __unix__

#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "Tools.h"

void StartBotProcess(const std::string& CommandLine)
{
    FILE* pipe = popen(CommandLine.c_str(), "r");
    if (!pipe)
    {
        std::cerr << "Can't launch command '" <<
            CommandLine << "'" << std::endl;
        return;
    }

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

bool MoveReplayFile(char* lpExistingFileName, char* lpNewFileName)
{
	// todo
	throw "MoveFile is not implemented for linux yet.";
}

#endif