#include <sstream>
#include <Windows.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <experimental/filesystem>

#include "util.h"

std::vector<std::string> SplitStringByCharacter(const std::string& string, const char splitter)
{
	std::string segment;
	std::vector<std::string> segments;

	std::stringstream stream(string);
	// split by designated splitter character
	while (getline(stream, segment, splitter))
		segments.push_back(segment);

	return segments;
}

bool FileExistsInEnvironmentPath(const std::string& filename)
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
	return std::string(buf);
}

std::string GetExecutableDirectory()
{
	auto executableFile = GetExecutableFullFilename();
	auto found = executableFile.find_last_of('\\');
	if (found == std::string::npos)
		throw "Expected backslash in executableFile, but none was present.";
	// strip out the executable filename at the end
	return executableFile.substr(0, found);
}

std::string GetWorkingDirectory()
{
	// if the following statement gives anyone trouble, it might be
	// that current_path() is a C++17 function.
	return std::experimental::filesystem::current_path().generic_string();
}

bool FileExistsInExecutableDirectory(const std::string& filename)
{
	return std::ifstream(GetExecutableDirectory() + "\\" + filename).good();
}

bool FileExistsInWorkingDirectory(const std::string& filename)
{
	return std::ifstream(GetWorkingDirectory() + "\\" + filename).good();
}

bool FileExists(const std::string& filename)
{
	return std::ifstream(filename).good();
}

bool CanFindFileInEnvironment(const std::string& filename)
{
	// If filename contains directories, only check it against
	// the an absolute reference or the working directory.
	// Only straight commands such as "Bot.exe" (i.e. no directory)
	// will successfully run a program found in the executable's
	// directory or the env path
	return filename.find('\\') != std::string::npos
		|| filename.find('/') != std::string::npos
		? FileExists(filename)
		|| FileExistsInWorkingDirectory(filename)
		: FileExists(filename)
		|| FileExistsInExecutableDirectory(filename)
		|| FileExistsInWorkingDirectory(filename)
		|| FileExistsInEnvironmentPath(filename);
}