#pragma once

std::vector<std::string> SplitStringByCharacter(const std::string& string, char splitter);
bool FileExistsInEnvironmentPath(const std::string& filename);
std::string GetExecutableFullFilename();
std::string GetExecutableDirectory();
std::string GetWorkingDirectory();
bool FileExistsInExecutableDirectory(const std::string& filename);
bool FileExistsInWorkingDirectory(const std::string& filename);
bool CanFindFileInEnvironment(const std::string& filename);