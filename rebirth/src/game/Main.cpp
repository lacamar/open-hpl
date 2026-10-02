
#include "RebirthBase.h"

int hplMain(const tString &asCommandline)
{
	gpRebirthBase = hplNew(cRebirthBase, ());

	if (gpRebirthBase->Init(asCommandline))
	{
		gpRebirthBase->Run();
		gpRebirthBase->Exit();
	}
	else
	{
		if (gpRebirthBase->msErrorMessage == _W(""))
			gpRebirthBase->msErrorMessage = _W("Error occured");

		cPlatform::CreateMessageBox(_W("Error!"), gpRebirthBase->msErrorMessage.c_str());
	}

	hplDelete(gpRebirthBase);

	cMemoryManager::LogResults();

	return 0;
}
