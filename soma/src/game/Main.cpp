/*
 * Phase 0 scaffolding entry point for the SOMA game module.
 *
 * Mirrors the pattern of amnesia/src/game/Main.cpp: the engine
 * (HPL2/core/sources/impl/LowLevelSystemSDL.cpp) calls this extern
 * hplMain(), which delegates almost everything to a base object -
 * cSomaBase here, cLuxBase there.
 */

#include "SomaBase.h"

//---------------------------------------

int hplMain(const tString &asCommandline)
{
	//////////////////////////
	// Game creation and exit
	gpSomaBase = hplNew(cSomaBase, ());

	//Init and run if all okay
	if (gpSomaBase->Init(asCommandline))
	{
		gpSomaBase->Run();
		gpSomaBase->Exit();
	}
	//Error occurred
	else
	{
		if (gpSomaBase->msErrorMessage == _W(""))
			gpSomaBase->msErrorMessage = _W("Error occured");

		cPlatform::CreateMessageBox(_W("Error!"), gpSomaBase->msErrorMessage.c_str());
		//No Exit, since it was not sure everything was created as it should.
	}

	hplDelete(gpSomaBase);

	cMemoryManager::LogResults();

	return 0;
}
