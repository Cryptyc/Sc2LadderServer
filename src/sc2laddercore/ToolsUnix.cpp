#if defined(__unix__) || defined(__APPLE__)

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Tools.h"
#include "Types.h"

void StartBotProcess(const BotConfig &Agent, const std::string &CommandLine, unsigned long *ProcessId)
{
	pid_t pID = fork();

	if (pID < 0)
	{
		std::cerr << Agent.BotName + ": Can't fork the bot process, error: " +
			strerror(errno) << std::endl;
		return;
	}

	if (pID == 0) // child
	{
		close(STDIN_FILENO);
		close(STDOUT_FILENO);

		// FIXME (alkurbatov): Redirect stderr to a file like ToolsWindows do.
		//close(STDERR_FILENO);

		int ret = chdir(Agent.RootPath.c_str());
		if (ret < 0) {
			std::cerr << Agent.BotName +
				": Can't change working directory to " + Agent.RootPath +
				", error: " + strerror(errno) << std::endl;
			exit(errno);
		}

		std::vector<char*> unix_cmd;
		std::istringstream stream(CommandLine);
		std::istream_iterator<std::string> begin(stream), end;
		std::vector<std::string> tokens(begin, end);
		for (const auto& i : tokens)
			unix_cmd.push_back(const_cast<char*>(i.c_str()));

		// FIXME (alkurbatov): Unfortunately, the cmdline uses relative path.
		// This hack is needed because we have to change the working directory
		// before calling to exec.
		if (Agent.Type == BinaryCpp)
			unix_cmd[0] = const_cast<char*>(Agent.FileName.c_str());

		unix_cmd.push_back(NULL);

		// NOTE (alkurbatov): For the Python bots we need to search in the PATH
		// for the interpreter.
		if (Agent.Type == Python || Agent.Type == Wine || Agent.Type == Mono)
			ret = execvp(unix_cmd.front(), &unix_cmd.front());
		else
			ret = execv(unix_cmd.front(), &unix_cmd.front());

		if (ret < 0)
		{
			std::cerr << Agent.BotName + ": Failed to execute '" + CommandLine +
				"', error: " + strerror(errno) << std::endl;
			exit(errno);
		}

		exit(0);
	}

	// parent
	*ProcessId = pID;

	int exit_status = 0;
	int ret = waitpid(pID, &exit_status, 0);
	if (ret < 0) {
		std::cerr << Agent.BotName +
			": Can't wait for the child process, error:" +
			strerror(errno) << std::endl;
	}
}

void StartExternalProcess(const std::string CommandLine)
{
	execve(CommandLine.c_str(), NULL, NULL);
}

void SleepFor(int seconds)
{
	sleep(seconds);
}

void KillSc2Process(unsigned long pid)
{
	int ret = kill(pid, SIGKILL);
	if (ret < 0)
	{
		std::cerr << std::string("Failed to send SIGKILL, error:") +
			strerror(errno) << std::endl;
	}
}

bool MoveReplayFile(const char* lpExistingFileName, const  char* lpNewFileName)
{
	int ret = rename(lpExistingFileName, lpNewFileName);
	if (ret < 0)
	{
		std::cerr << std::string("Failed to move a replay file, error:") +
			strerror(errno) << std::endl;
	}

	return ret == 0;
}

#endif