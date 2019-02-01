#pragma once

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson.h"
#include "document.h"

#include <string>
#include <map>
#include <vector>

class LadderConfig
{
public:
    explicit LadderConfig(const std::string &InConfigFile);
    bool ParseConfig();
    bool WriteConfig();
    bool GetBoolValue(std::string RequestedValue);
    int GetIntValue(std::string RequestedValue);
    std::string GetStringValue(std::string RequestedValue);
    std::vector<std::string> GetArrayValue(std::string RequestedValue);
	void AddValue(const std::string &Index, const std::string &Value);
private:
    const std::string ConfigFileLocation;
    rapidjson::Document doc;
};
