#include "SomaScriptCheck.h"
#include "RustSomaScript.h"
#include "RustSomaScriptS2.h"

#include "impl/RustL7.h"

#include <angelscript.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

#if __has_include("impl/RustEasArray.h")
#include "impl/RustEasArray.h"
extern "C" const char *ohpl_asx_str_data(const void *);
extern "C" size_t ohpl_asx_str_len(const void *);
static std::string SuiteText(const void *p) { return std::string(ohpl_asx_str_data(p), ohpl_asx_str_len(p)); }
#else
#include "../../../cshim/as_shim.h"
#define EAS_BASE 1
static std::string SuiteText(const void *p) { return std::string(ohpl_as_str_data(p), ohpl_as_str_len(p)); }
#endif

int RunSomaScriptCheck(const std::string &asGameDir, const std::string &asApiFile, const std::string &asReport)
{
	unsigned lFailed = ohpl_l7_selftest();
	if (lFailed != 0)
	{
		fprintf(stderr, "rust script selftest failed: mask 0x%x\n", lFailed);
		return 1;
	}
	OhplS2InstallHost();
	return ohpl_s2_run_check(OhplGs6::U(asGameDir), asGameDir.size(), OhplGs6::U(asApiFile), asApiFile.size(), OhplGs6::U(asReport), asReport.size());
}

static void SuiteOut(const void *apStr)
{
	printf("%s\n", SuiteText(apStr).c_str());
	fflush(stdout);
}

static void SuiteException()
{
	asIScriptContext *pCtx = asGetActiveContext();
	const char *pInfo = pCtx ? pCtx->GetExceptionString() : NULL;
	printf("exception: %s\n", pInfo ? pInfo : "none");
	fflush(stdout);
}

int RunAddonSuite(const std::string &asFile)
{
	std::ifstream f(asFile);
	std::stringstream ss;
	ss << f.rdbuf();
	std::string sSrc = ss.str();
#ifdef EAS_BASE
	OhplL7InstallAddons();
#endif
	OhplL7Engine *pEngine = ohpl_l7_engine_new();
	ohpl_l7_add_func(pEngine, "void say(const string &in)", (const void *)&SuiteOut);
	ohpl_l7_add_func(pEngine, "void exc()", (const void *)&SuiteException);
	OhplL7Script *pScript = ohpl_l7_script_new(pEngine, 0, 0);
	auto Sink = [](void *c, const char *p, size_t n) { ((std::string *)c)->append(p, n); };
	std::string sMsg, sChunks;
	int lRet = ohpl_l7_script_build(pScript, sSrc.c_str(), sSrc.size(), Sink, &sMsg, Sink, &sChunks);
	if (lRet != 0)
	{
		fprintf(stderr, "build failed (%d):\n%s%s\n", lRet, sMsg.c_str(), sChunks.c_str());
		return 1;
	}
	ohpl_l7_script_run_handle(pScript, ohpl_l7_script_func_handle(pScript, "main"));
	// the engine's message callback is freed before the engine, so a message during teardown crashes
	fflush(stdout);
	_exit(0);
}

static void Dummy() {}

int RunA1Check(const std::string &asDecls, const std::string &asDirs)
{
	OhplL7Engine *pEngine = ohpl_l7_engine_new();
	std::ifstream d(asDecls);
	std::string sLine;
	int lDecls = 0, lBad = 0;
	while (std::getline(d, sLine))
	{
		std::string sDecl = sLine.substr(0, sLine.rfind(" -> "));
		if (!ohpl_l7_add_func(pEngine, sDecl.c_str(), (const void *)&Dummy))
		{
			++lBad;
			fprintf(stderr, "decl rejected: %s\n", sDecl.c_str());
		}
		++lDecls;
	}
	int lOk = 0, lFail = 0;
	std::stringstream ds(asDirs);
	std::string sDir;
	while (std::getline(ds, sDir, ':'))
		for (auto &p : std::filesystem::recursive_directory_iterator(sDir))
		{
			if (p.path().extension() != ".hps")
				continue;
			std::ifstream f(p.path());
			std::stringstream ss;
			ss << f.rdbuf();
			std::string sSrc = ss.str();
			OhplL7Script *pScript = ohpl_l7_script_new(pEngine, lOk + lFail, 0);
			auto Sink = [](void *c, const char *pStr, size_t n) { ((std::string *)c)->append(pStr, n); };
			std::string sMsg, sChunks;
			if (ohpl_l7_script_build(pScript, sSrc.c_str(), sSrc.size(), Sink, &sMsg, Sink, &sChunks) != 0)
			{
				++lFail;
				printf("FAIL %s\n%s", p.path().c_str(), sMsg.c_str());
			}
			else
				++lOk;
			ohpl_l7_script_free(pScript);
		}
	printf("decls %d rejected %d compiled %d failed %d\n", lDecls, lBad, lOk, lFail);
	fflush(stdout);
	_exit(lBad || lFail ? 1 : 0);
}
