#pragma once

class LadderConfig
{
public:
    LadderConfig(std::string InConfigFile);
    bool ParseConfig();
    std::string GetValue(std::string RequestedValue);
private:
    void TrimString(std::string &Str);
    std::string ConfigFileLocation;
    std::map<std::string, std::string> options; 


};
