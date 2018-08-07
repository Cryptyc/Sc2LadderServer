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


LadderConfig::LadderConfig(const std::string &ConfigFile)
	:ConfigFileLocation(ConfigFile)
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

std::string LadderConfig::GetValue(std::string RequestedValue)
{
	if (doc.HasMember(RequestedValue))
	{
		if (doc[RequestedValue].IsString())
		{
			return doc[RequestedValue].GetString();
		}
		
		throw std::string("Unable to obtain RequestedValue: ") + RequestedValue;
	}
	
	return ""; // this allows config entries to not have to exist
}

void LadderConfig::AddValue(const std::string &Index, const std::string &Value)
{
	rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
	doc.AddMember(
		rapidjson::Value(Index.c_str(), allocator).Move(),
		rapidjson::Value(Value.c_str(), allocator).Move(),
		allocator);
}