#include <sstream>
#include <Windows.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <experimental/filesystem>

#include "util.h"

std::vector<std::string> SplitStringByCharacter(std::string string, char splitter)
{
	std::string segment;
	std::vector<std::string> segments;

	std::stringstream stream(string);
	// split by designated splitter character
	while (getline(stream, segment, splitter))
		segments.push_back(segment);

	return segments;
}

bool FileExistsInEnvironmentPath(std::string filename)
{
	// 65535 - longest windows path var
	DWORD buffSize = 65535;
	std::string buffer;
	buffer.resize(buffSize);
	GetEnvironmentVariable("PATH", &buffer[0], buffSize);
	auto paths = SplitStringByCharacter(std::string(buffer), ';');
	for (auto&& s : paths) {
		if (std::ifstream(s + "\\" + filename).good())
			return true;
	}
	return false;
}

std::string GetExecutableFullFilename()
{
	char buf[MAX_PATH];
	auto bytes = GetModuleFileName(NULL, buf, MAX_PATH);
	if (bytes == 0)
		throw "Error: Could not retrieve executable file name.";
	else
		return std::string(buf);
}

std::string GetExecutableDirectory()
{
	auto executableFile = GetExecutableFullFilename();
	auto found = executableFile.find_last_of('\\');
	if (found != std::string::npos) // strip out the executable filename at the end
		return executableFile.substr(0, found);
	else
		"Expected backslash in executableFile, but none was present.";
}

std::string GetWorkingDirectory()
{
	return std::experimental::filesystem::current_path().generic_string();
}

bool FileExistsInExecutableDirectory(std::string filename)
{
	if (std::ifstream(GetExecutableDirectory() + "\\" + filename).good())
		return true;
	return false;
}

bool FileExistsInWorkingDirectory(std::string filename)
{
	if (std::ifstream(
		GetWorkingDirectory() + "\\" + filename
	).good())
		return true;
	return false;
}

bool CanFindFileInEnvironment(std::string filename)
{
	return FileExistsInExecutableDirectory(filename)
		|| FileExistsInWorkingDirectory(filename)
		|| FileExistsInEnvironmentPath(filename);
}