#include "SomaBase.h"
#include "SomaScriptCheck.h"

int hplMain(const tString &asCommandline)
{
	// OPENHPL_SOMA_SCRIPT_CHECK=<report.json>: compile all game scripts against the recovered API, no engine
	if (const char *pReport = getenv("OPENHPL_SOMA_SCRIPT_CHECK"))
	{
		const char *pGame = getenv("OPENHPL_SOMA_GAME_DIR");
		const char *pApi = getenv("OPENHPL_SOMA_SCRIPT_API");
		return RunSomaScriptCheck(pGame ? pGame : ".", pApi ? pApi : "script_api.txt", pReport);
	}

	gpSomaBase = hplNew(cSomaBase, ());

	if (gpSomaBase->Init(asCommandline))
	{
		gpSomaBase->Run();
		gpSomaBase->Exit();
	}
	else
	{
		if (gpSomaBase->msErrorMessage == _W(""))
			gpSomaBase->msErrorMessage = _W("Error occured");

		cPlatform::CreateMessageBox(_W("Error!"), gpSomaBase->msErrorMessage.c_str());
		// no Exit: init may be partial
	}

	hplDelete(gpSomaBase);

	cMemoryManager::LogResults();

	return 0;
}
