#if defined(__unix__) || defined(__APPLE__)

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <iterator>

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <limits.h>
#include <dirent.h>
#include <string.h>
#include <fstream>

#include "Tools.h"
#include "Types.h"
#include "md5.h"

namespace {

int RedirectOutput(const BotConfig &Agent, int SrcFD, const char *LogFile)
{
    int logFD = open(LogFile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (logFD < 0) {
        std::cerr << Agent.BotName +
            ": Failed to create a log file, error: " <<  errno << std::endl;
            return logFD;
    }

    int ret = dup2(logFD, SrcFD);
    if (ret < 0) {
        std::cerr << Agent.BotName +
            ": Can't redirect output to a log file, error: "  << errno << std::endl;
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
		std::cerr << Agent.BotName + ": Can't fork the bot process, error: " << errno << std::endl;
		return;
	}

	if (pID == 0) // child
	{
		// Move child to a new process group so that it can not kill the ladderManager
		setpgid(0, 0);

		//	sleep(60);
		int ret = chdir(Agent.RootPath.c_str());
		if (ret < 0)
		{
			std::cerr << Agent.BotName + ": Can't change working directory to " + Agent.RootPath << ", error: " << errno << std::endl;
			//            exit(errno);
		}

		//	RedirectOutput(Agent, STDERR_FILENO, "data/stderr.log");
			//            exit(errno);

		//	if (Agent.Debug)
		//	{
		//		RedirectOutput(Agent, STDOUT_FILENO, "data/stdout.log");
			//                exit(errno);
		//	}
		//	else
		//	{
		//		close(STDOUT_FILENO);
		//	}

		close(STDIN_FILENO);

		system(CommandLine.c_str());
	}
	/*
        int ret = chdir(Agent.RootPath.c_str());
        if (ret < 0) {
            std::cerr << Agent.BotName +
                ": Can't change working directory to " + Agent.RootPath << ", error: " << errno << std::endl;
//            exit(errno);
        }

//		RedirectOutput(Agent, STDERR_FILENO, "data/stderr.log");
//            exit(errno);

//        if (Agent.Debug)
//        {
//			RedirectOutput(Agent, STDOUT_FILENO, "data/stdout.log");
//                exit(errno);
//        }
//		else
//		{
			close(STDOUT_FILENO);
//		}

        close(STDIN_FILENO);

        std::vector<char*> unix_cmd;
        std::istringstream stream(CommandLine);
        std::istream_iterator<std::string> begin(stream), end;
        std::vector<std::string> tokens(begin, end);
		for (const auto& i : tokens)
		{
			unix_cmd.push_back(const_cast<char*>(i.c_str()));
		}

        // FIXME (alkurbatov): Unfortunately, the cmdline uses relative path.
        // This hack is needed because we have to change the working directory
        // before calling to exec.
		if (Agent.Type == BinaryCpp)
		{
			unix_cmd[0] = const_cast<char*>(Agent.FileName.c_str());
		}

        unix_cmd.push_back(NULL);
		for (const auto& s : unix_cmd)
		{
			std::cerr << s << " ";
		}

        // NOTE (alkurbatov): For the Python bots we need to search in the PATH
        // for the interpreter.
		if (Agent.Type != BinaryCpp)
		{
			system(CommandLine.c_str());
//			ret = execvp(unix_cmd.front(), &unix_cmd.front());
		}
		else
		{
			system(CommandLine.c_str());
//			ret = execv(unix_cmd.front(), &unix_cmd.front());
		}

        if (ret < 0)
        {
            std::cerr << Agent.BotName + ": Failed to execute '" + CommandLine +
                "', error: " << errno << std::endl;
//            exit(errno);
        }

//        exit(0);
    }

    // parent
	*/
    *ProcessId = pID;

    int exit_status = 0;
    int ret = waitpid(pID, &exit_status, 0);
    if (ret < 0) {
        std::cerr << Agent.BotName +
            ": Can't wait for the child process, error:" << errno << std::endl;
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
        std::cerr << std::string("Failed to send SIGKILL, error:") << errno << std::endl;
    }
}

bool MoveReplayFile(const char* lpExistingFileName, const  char* lpNewFileName)
{
    int ret = rename(lpExistingFileName, lpNewFileName);
    if (ret < 0)
    {
        std::cerr << std::string("Failed to move a replay file, error:") << errno << std::endl;
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
	std::cout << command << std::endl;
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
	std::string Command = "unzip " + InArchive + " -d " + OutDirectory;
	std::cout << "Unzipping: " << Command << std::endl;
	system(Command.c_str());
}

std::string GenerateMD5(const std::string filename)
{
	std::ifstream InFile;
	InFile.open(filename.c_str(), std::ios::binary | std::ios::in);
	if (!InFile.is_open())
	{
		std::cout << "GenerateMD5: Unable to open file: " << filename << std::endl;

	}
	//Find length of file
	InFile.seekg(0, std::ios::end);
	long Length = InFile.tellg();
	InFile.seekg(0, std::ios::beg);

	//read in the data from your file
	char* InFileData = new char[Length];
	InFile.read(InFileData, Length);

	//Calculate MD5 hash
	std::string Temp = md5(InFileData, Length);

	//Clean up
	delete[] InFileData;

	return Temp;
}

bool MakeDirectory(const std::string& directory_name)
{
    return mkdir(directory_name.c_str(), 0755);
}

bool CheckExists(const std::string& name)
{
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

void RemoveDirectoryRecursive(std::string Path)
{
	DIR* dir;
	struct dirent* entry;
	char CurrentPath[PATH_MAX];

	dir = opendir(Path.c_str());
	if (dir == NULL) {
		perror("Error opendir()");
		return;
	}

	while ((entry = readdir(dir)) != NULL)
	{
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) 
		{
			snprintf(CurrentPath, (size_t)PATH_MAX, "%s/%s", Path.c_str(), entry->d_name);
			if (entry->d_type == DT_DIR)
			{
				RemoveDirectoryRecursive(CurrentPath);
			}
			remove(CurrentPath);
		}
	}
	closedir(dir);
	remove(Path.c_str());
}

#endif
