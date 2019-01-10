#include "LadderConfig.h"

#include "Types.h"

#include "ostreamwrapper.h"
#include "writer.h"
#include "prettywriter.h"

#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>


LadderConfig::LadderConfig(const std::string &InConfigFile)
	:ConfigFileLocation(InConfigFile)
{
	// Doing this allows us to write values to the doc
	// straight off instead of loading from file
	this->doc.SetObject();
}

bool LadderConfig::ParseConfig()
{
	std::ifstream t(this->ConfigFileLocation);
	if (t.good())
	{
		std::stringstream buffer;
		buffer << t.rdbuf();
		std::string ConfigString = buffer.str();
		return !doc.Parse(ConfigString.c_str()).HasParseError();
	}
	
	return false;
}


bool LadderConfig::WriteConfig()
{
	std::ofstream ofs(this->ConfigFileLocation.c_str());
	rapidjson::OStreamWrapper osw(ofs);
	rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
	return this->doc.Accept(writer);
}

std::string LadderConfig::GetStringValue(std::string RequestedValue)
{
    if (doc.HasMember(RequestedValue))
    {
        if (doc[RequestedValue].IsString())
        {
            return doc[RequestedValue].GetString();
        }
        throw std::invalid_argument("The value \"" + RequestedValue + "\" has to be a String! Aborting.");
    }
    return ""; // this allows config entries to not have to exist
}

bool LadderConfig::GetBoolValue(std::string RequestedValue)
{
    if (doc.HasMember(RequestedValue))
    {
        if (doc[RequestedValue].IsBool())
        {
            return doc[RequestedValue].GetBool();
        }
        throw std::invalid_argument("The value \"" + RequestedValue + "\" has to be a bool! Aborting.");
    }
    return false; // this allows config entries to not have to exist
}

int LadderConfig::GetIntValue(std::string RequestedValue)
{
    if (doc.HasMember(RequestedValue))
    {
        if (doc[RequestedValue].IsInt())
        {
            return doc[RequestedValue].GetInt();
        }
        throw std::invalid_argument("The value \"" + RequestedValue + "\" has to be an int! Aborting.");
    }
    return 0; // this allows config entries to not have to exist
}

std::vector<std::string> LadderConfig::GetArrayValue(std::string RequestedValue)
{
	std::vector<std::string> ReturnedArray;
	if (doc.HasMember(RequestedValue) && doc[RequestedValue].IsArray())
	{
		const rapidjson::Value & Values = doc[RequestedValue];
		for (auto itr = Values.Begin(); itr != Values.End(); ++itr)
		{
			ReturnedArray.push_back(itr->GetString());
		}
	}
	return ReturnedArray;
}

void LadderConfig::AddValue(const std::string &Index, const std::string &Value)
{
	rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
	doc.AddMember(
		rapidjson::Value(Index.c_str(), allocator).Move(),
		rapidjson::Value(Value.c_str(), allocator).Move(),
		allocator);
}
