/*
 * Phase 0 scaffolding entry point for the SOMA game module.
 *
 * Mirrors the pattern of amnesia/src/game/Main.cpp: the engine
 * (HPL2/core/sources/impl/LowLevelSystemSDL.cpp) calls this extern
 * hplMain(), which delegates almost everything to a base object -
 * cSomaBase here, cLuxBase there.
 */

#include "SomaBase.h"
#include "SomaScriptCheck.h"

//---------------------------------------

int hplMain(const tString &asCommandline)
{
	// OPENHPL_SOMA_SCRIPT_CHECK=<report.json>: compile all game scripts against the recovered API, no engine
	if (const char *pReport = getenv("OPENHPL_SOMA_SCRIPT_CHECK"))
	{
		const char *pGame = getenv("OPENHPL_SOMA_GAME_DIR");
		const char *pApi = getenv("OPENHPL_SOMA_SCRIPT_API");
		return RunSomaScriptCheck(pGame ? pGame : ".", pApi ? pApi : "script_api.txt", pReport);
	}

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
