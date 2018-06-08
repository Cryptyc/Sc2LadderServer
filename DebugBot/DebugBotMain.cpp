#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

#include <Windows.h>
#include <iostream>

#include "LadderInterface.h"

class DebugBot : public sc2::Agent {
public:
	virtual void OnGameStart() final {
		const sc2::ObservationInterface* observation = Observation();
		std::cout << "I am player number " << observation->GetPlayerID() << std::endl;
		std::cout << "Executable: " << GetExecutableFullFileName() << std::endl;
		std::cout << "Working directory: " << GetWorkingDirectory() << std::endl;
	};

	virtual void OnStep() final {
		const sc2::ObservationInterface* observation = Observation();
		sc2::ActionInterface* action = Actions();
		switch (observation->GetPlayerID())
		{
			case 1: { // Player 1 lose
				const sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self);
				for (const auto& commandCenter : units) {
					if (static_cast<sc2::UNIT_TYPEID>(commandCenter->unit_type) == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER)
					{
						for (const auto& scv : units) {
							action->UnitCommand(scv, sc2::ABILITY_ID::ATTACK, commandCenter);
						}
					}
				}
			} break;
			case 2: { // Player 2 win
			} break;
		}

	};

private:
	// These are copies of functions found in the main LadderManager project
	// Ideally they could be moved to a common library eventually to avoid duplcation.
	std::string GetExecutableFullFileName()
	{
		char buf[MAX_PATH];
		int bytes = GetModuleFileName(NULL, buf, MAX_PATH);
		if (bytes == 0)
			return "Error: Could not retrieve executable file name.";
		else
			return std::string(buf);
	}
	std::string GetWorkingDirectory()
	{
		char buf[MAX_PATH + 1];
		if (GetCurrentDirectory(MAX_PATH + 1, buf))
			return buf;
		else
			throw "Unable to get working directory: call to GetCurrentDirectory() returned nothing.";
	}
};

int main(int argc, char* argv[])
{
	RunBot(argc, argv, new DebugBot(), sc2::Race::Terran);
}