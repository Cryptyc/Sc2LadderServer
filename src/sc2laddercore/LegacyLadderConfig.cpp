#include "LegacyLadderConfig.h"

#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>  

LegacyLadderConfig::LegacyLadderConfig(const std::string &InConfigFile)
	:ConfigFileLocation(InConfigFile)
{
}



bool LegacyLadderConfig::ParseConfig()
{
	std::ifstream ifs(ConfigFileLocation, std::ifstream::binary);
	if (!ifs)
	{
		return false;
	}


	std::string line;
	while (std::getline(ifs, line))
	{
		std::istringstream is_line(line);
		std::string key;
		if (std::getline(is_line, key, '='))
		{
			std::string value;
			if (std::getline(is_line, value))
			{
				TrimString(key);
				TrimString(value);
				options.insert(std::make_pair(key, value));
			}
		}
	}
	return true;
}

bool LegacyLadderConfig::WriteConfig()
{
	std::ofstream outfile(ConfigFileLocation, std::ofstream::binary);
	for (const auto& setting : options)
	{
		outfile << setting.first << "=" << setting.second << std::endl;
	}
	outfile.close();
	return true;
}

std::string LegacyLadderConfig::GetValue(std::string RequestedValue)
{
	auto search = options.find(RequestedValue);
	if (search != options.end())
	{
		return search->second;
	}
	return std::string();
}

void LegacyLadderConfig::TrimString(std::string &Str)
{
	size_t p = Str.find_first_not_of(" \t");
	Str.erase(0, p);
	p = Str.find_last_not_of(" \t\r\n");
	if (std::string::npos != p)
	{
		Str.erase(p + 1);
	}
}

void LegacyLadderConfig::AddValue(const std::string &Index, const std::string &Value)
{
	options.insert(std::make_pair(Index, Value));
}
