
#include "AmfpBase.h"

int hplMain(const tString &asCommandline)
{
	gpAmfpBase = hplNew(cAmfpBase, ());

	if (gpAmfpBase->Init(asCommandline))
	{
		gpAmfpBase->Run();
		gpAmfpBase->Exit();
	}
	else
	{
		if (gpAmfpBase->msErrorMessage == _W(""))
			gpAmfpBase->msErrorMessage = _W("Error occured");

		cPlatform::CreateMessageBox(_W("Error!"), gpAmfpBase->msErrorMessage.c_str());
	}

	hplDelete(gpAmfpBase);

	cMemoryManager::LogResults();

	return 0;
}
