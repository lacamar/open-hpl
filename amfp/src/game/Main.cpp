/*
 * Phase 0 scaffolding entry point for the AMFP game module.
 *
 * Mirrors the pattern of amnesia/src/game/Main.cpp and soma/src/game/Main.cpp:
 * the engine (HPL2/core/sources/impl/LowLevelSystemSDL.cpp) calls this
 * extern hplMain(), which delegates almost everything to a base object -
 * cAmfpBase here.
 */

#include "AmfpBase.h"

//---------------------------------------

int hplMain(const tString &asCommandline)
{
	//////////////////////////
	// Game creation and exit
	gpAmfpBase = hplNew(cAmfpBase, ());

	//Init and run if all okay
	if (gpAmfpBase->Init(asCommandline))
	{
		gpAmfpBase->Run();
		gpAmfpBase->Exit();
	}
	//Error occurred
	else
	{
		if (gpAmfpBase->msErrorMessage == _W(""))
			gpAmfpBase->msErrorMessage = _W("Error occured");

		cPlatform::CreateMessageBox(_W("Error!"), gpAmfpBase->msErrorMessage.c_str());
		//No Exit, since it was not sure everything was created as it should.
	}

	hplDelete(gpAmfpBase);

	cMemoryManager::LogResults();

	return 0;
}
