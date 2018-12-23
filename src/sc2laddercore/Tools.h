#pragma once

#include <string>
#include "Types.h"
#ifdef _WIN32
#include "dirent.h"
#else
#include <dirent.h>
#endif


void StartBotProcess(const BotConfig &Agent, const std::string& CommandLine, unsigned long *ProcessId);

void SleepFor(int seconds);

void KillBotProcess(unsigned long pid);

bool MoveReplayFile(const char* lpExistingFileName, const char* lpNewFileName);

void StartExternalProcess(const std::string CommandLine);

std::string PerformRestRequest(const std::string &location, const std::vector<std::string> &arguments);

bool ZipArchive(const std::string &InDirectory, const std::string &OutArchive);

bool UnzipArchive(const std::string &InArchive, const std::string &OutDirectory);

static std::string NormalisePath(std::string Path)
{
    size_t pos = Path.find_last_of("/./");
    size_t len = Path.length();
    if (pos != std::string::npos && (pos + 1) == Path.length())
    {
        Path.erase(pos - 2, 3);
    }
    return Path;
}
static void RemoveDirectoryRecursive(std::string Path)
{
    Path = NormalisePath(Path);
    struct dirent *entry = NULL;
    DIR *dir = NULL;
    dir = opendir(Path.c_str());
    while (entry = readdir(dir))
    {
        DIR *SubDir = NULL;
        FILE *file = NULL;
        char AbsPath[MAX_PATH] = { 0 };
        if (*(entry->d_name) != '.')
        {
            sprintf(AbsPath, "%s/%s", Path.c_str(), entry->d_name);
            if (SubDir = opendir(AbsPath))
            {
                closedir(SubDir);
                RemoveDirectoryRecursive(AbsPath);
            }
            else
            {
                if (file = fopen(AbsPath, "r"))
                {
                    fclose(file);
                    remove(AbsPath);
                }
            }
        }
    }
    remove(Path.c_str());
}
