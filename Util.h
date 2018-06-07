#pragma once

std::vector<std::string> SplitStringByCharacter(std::string string, char splitter);
bool FileExistsInEnvironmentPath(std::string filename);
std::string GetExecutableFullFilename();
std::string GetExecutableDirectory();
std::string GetWorkingDirectory();
bool FileExistsInExecutableDirectory(std::string filename);
bool FileExistsInWorkingDirectory(std::string filename);
bool CanFindFileInEnvironment(std::string filename);