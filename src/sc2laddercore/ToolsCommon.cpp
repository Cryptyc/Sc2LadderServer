#include "Tools.h"

#include "sc2utils/sc2_manage_process.h"

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
    if (dir == NULL)
    {
        return;
    }
    while ((entry = readdir(dir)))
    {
        DIR *SubDir = NULL;
        FILE *file = NULL;
        char AbsPath[MAX_PATH] = { 0 };
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
        {
            sprintf(AbsPath, "%s/%s", Path.c_str(), entry->d_name);
            if ((SubDir = opendir(AbsPath)))
            {
                closedir(SubDir);
                RemoveDirectoryRecursive(AbsPath);
            }
            else
            {
                if ((file = fopen(AbsPath, "r")))
                {
                    fclose(file);
                    remove(AbsPath);
                }
            }
        }
    }
    remove(Path.c_str());
}

bool isMapAvailable(const std::string& map_name, const std::string& sc2Path)
{
	// BattleNet map
	if (!sc2::HasExtension(map_name, ".SC2Map"))
	{
		// Not sure if we should return true here.
		return true;
	}

	// Absolute path
	if (sc2::DoesFileExist(map_name))
	{
		return true;
	}

	// Relative path - Game maps directory
	std::string game_relative = sc2::GetGameMapsDirectory(sc2Path) + map_name;
	if (sc2::DoesFileExist(game_relative))
	{
		return true;
	}
	return false;
}
