#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

#include "LadderInterface.h"

class DebugBot : public sc2::Agent {
public:
	virtual void OnGameStart() final {};

	virtual void OnStep() final {};

private:
};

int main(int argc, char* argv[])
{
	RunBot(argc, argv, new DebugBot(), sc2::Race::Terran);
}