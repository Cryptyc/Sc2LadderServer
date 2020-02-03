#if defined(__unix__) || defined(__APPLE__)

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <array>

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Tools.h"
#include "Types.h"

namespace {

int RedirectOutput(const BotConfig &Agent, int SrcFD, const char *LogFile)
{
    int logFD = open(LogFile, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
    if (logFD < 0) {
        std::cerr << Agent.BotName +
            ": Failed to create a log file, error: " +
            strerror(errno) << std::endl;
            return logFD;
    }

    int ret = dup2(logFD, SrcFD);
    if (ret < 0) {
        std::cerr << Agent.BotName +
            ": Can't redirect output to a log file, error: " +
            strerror(errno) << std::endl;
        return ret;
    }

    close(logFD);
    return ret;
}

} // namespace

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
        // Move child to a new process group so that it can not kill the ladderManager
        setpgid(0, 0);
        int ret = chdir(Agent.RootPath.c_str());
        if (ret < 0) {
            std::cerr << Agent.BotName +
                ": Can't change working directory to " + Agent.RootPath +
                ", error: " + strerror(errno) << std::endl;
            exit(errno);
        }

        if (RedirectOutput(Agent, STDERR_FILENO, "data/stderr.log") < 0)
            exit(errno);

        if (Agent.Debug)
        {
            if (RedirectOutput(Agent, STDOUT_FILENO, "data/stdout.log") < 0)
                exit(errno);
        }
        else
            close(STDOUT_FILENO);

        close(STDIN_FILENO);

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
        if (Agent.Type != BinaryCpp)
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

void StartExternalProcess(const std::string &CommandLine)
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

void KillBotProcess(unsigned long pid)
{
    if (pid == 0)
    {
        //Maybe a warning?
        return;
    }
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

std::string PerformRestRequest(const std::string &location, const std::vector<std::string> &arguments)
{
	std::array<char, 10000> buffer;
	std::string result;
	std::string command = "curl";
	for (const std::string &NextArgument : arguments)
	{
		command = command + NextArgument;
	}
	command = command + " " + location;
	std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
	if (!pipe) throw std::runtime_error("popen() failed!");
	while (!feof(pipe.get())) {
		if (fgets(buffer.data(), 10000, pipe.get()) != nullptr)
			result += buffer.data();
	}
	return result;
}

bool ZipArchive(const std::string &InDirectory, const std::string &OutArchive)
{
	return false;
}

bool UnzipArchive(const std::string &InArchive, const std::string &OutDirectory)
{
	return false;
}

std::string GenerateMD5(std::string&)
{
    return std::string();
}

bool MakeDirectory(const std::string& directory_name)
{
    return mkdir(directory_name.c_str(), 0755);
}

#endif
