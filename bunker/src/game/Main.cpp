/*
 * Phase 0 scaffolding entry point for the Amnesia: The Bunker game module.
 * Mirrors soma/src/game/Main.cpp - see that file for the pattern.
 */

#include "BunkerBase.h"

//---------------------------------------

int hplMain(const tString &asCommandline)
{
	gpBunkerBase = hplNew(cBunkerBase, ());

	if (gpBunkerBase->Init(asCommandline))
	{
		gpBunkerBase->Run();
		gpBunkerBase->Exit();
	}
	else
	{
		if (gpBunkerBase->msErrorMessage == _W(""))
			gpBunkerBase->msErrorMessage = _W("Error occured");

		cPlatform::CreateMessageBox(_W("Error!"), gpBunkerBase->msErrorMessage.c_str());
	}

	hplDelete(gpBunkerBase);

	cMemoryManager::LogResults();

	return 0;
}
