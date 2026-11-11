#include "SomaBase.h"
#include "SomaScriptCheck.h"
#include "RustSomaSave.h"

int hplMain(const tString &asCommandline)
{
	// OPENHPL_SOMA_SCRIPT_CHECK=<report.json>: compile all game scripts against the recovered API, no engine
	std::vector<std::string> vCheck;
	if (ohpl_gs4_main_plan(getenv("OPENHPL_SOMA_SCRIPT_CHECK"), getenv("OPENHPL_SOMA_GAME_DIR"), getenv("OPENHPL_SOMA_SCRIPT_API"),
						   [](void *c, const unsigned char *apStr, size_t alLen) { ((std::vector<std::string> *)c)->emplace_back((const char *)apStr, alLen); },
						   &vCheck))
		return RunSomaScriptCheck(vCheck[1], vCheck[2], vCheck[0]);

	gpSomaBase = hplNew(cSomaBase, ());

	if (gpSomaBase->Init(asCommandline))
	{
		gpSomaBase->Run();
		gpSomaBase->Exit();
	}
	else
	{
		ohpl_gs4_error_message((const uint32_t *)gpSomaBase->msErrorMessage.data(), gpSomaBase->msErrorMessage.size(),
							   [](void *c, const uint32_t *apMsg, size_t alLen) { ((tWString *)c)->assign((const wchar_t *)apMsg, alLen); },
							   &gpSomaBase->msErrorMessage);

		cPlatform::CreateMessageBox(_W("Error!"), gpSomaBase->msErrorMessage.c_str());
		// no Exit: init may be partial
	}

	hplDelete(gpSomaBase);

	cMemoryManager::LogResults();

	return 0;
}
