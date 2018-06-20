#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

#include "LadderInterface.h"

class DebugBot : public sc2::Agent {
public:
	virtual void OnGameStart() final {
		const sc2::ObservationInterface* observation = Observation();
		std::cout << "I am player number " << observation->GetPlayerID() << std::endl;
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
};

int main(int argc, char* argv[])
{
	RunBot(argc, argv, new DebugBot(), sc2::Race::Terran);
}