#include <string.h>

#include <Types.h>
#include <LadderManager.h>


// There's not much here...?
// That's because everything else is compiled as libraries
// in order to be included in the tests.
int main(int argc, char** argv)
{
	std::cout << "LadderManager started." << std::endl;

	LadderManager LadderMan(argc, argv);
	if (LadderMan.LoadSetup())
	{
		LadderMan.RunLadderManager();
	}

	std::cout << "Finished." << std::endl;
}
