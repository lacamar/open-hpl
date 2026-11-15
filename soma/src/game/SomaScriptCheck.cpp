#include "SomaScriptCheck.h"
#include "RustSomaScript.h"
#include "RustSomaScriptS2.h"

#include "impl/RustL7.h"

int RunSomaScriptCheck(const std::string &asGameDir, const std::string &asApiFile, const std::string &asReport)
{
	OhplL7InstallAddons();
	unsigned lFailed = ohpl_l7_selftest();
	if (lFailed != 0)
	{
		fprintf(stderr, "rust script selftest failed: mask 0x%x\n", lFailed);
		return 1;
	}
	OhplS2InstallHost();
	return ohpl_s2_run_check(OhplGs6::U(asGameDir), asGameDir.size(), OhplGs6::U(asApiFile), asApiFile.size(), OhplGs6::U(asReport), asReport.size());
}
