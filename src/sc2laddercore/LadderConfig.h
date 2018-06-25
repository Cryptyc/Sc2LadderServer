#pragma once

class LadderConfig
{
public:
    LadderConfig(std::string InConfigFile);
    bool ParseConfig();
	bool WriteConfig();
    std::string GetValue(std::string RequestedValue);
	void AddValue(const std::string &Index, const std::string &Value);
private:
    void TrimString(std::string &Str);
    std::string ConfigFileLocation;
    std::map<std::string, std::string> options; 


};
