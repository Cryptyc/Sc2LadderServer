#include "Tools.h"

#ifdef _WIN32
#include "dirent.h"
#else
#include <dirent.h>
#define MAX_PATH 255
#endif

std::string NormalisePath(std::string Path)
{
    size_t pos = Path.find_last_of("/./");
    size_t len = Path.length();
    if (pos != std::string::npos && (pos + 1) == Path.length())
    {
        Path.erase(pos - 2, 3);
    }
    return Path;
}

void RemoveDirectoryRecursive(std::string Path)
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
