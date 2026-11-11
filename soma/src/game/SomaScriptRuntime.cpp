#include "SomaScriptRuntime.h"
#include "RustSomaScript.h"
#include "SomaBase.h"
#include "SomaScriptApi.h"
#include "SomaFsb.h"
#include "SomaScriptBuilder.h"
#include "SomaScriptNatives.h"
#include "SomaScriptStrings.h"

#include "impl/scriptarray.h"
#include "system/LowLevelSystem.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <vector>

using namespace hpl;

cSomaScriptRuntime *cSomaScriptRuntime::mpInstance = NULL;

static void MessageCallback(const asSMessageInfo *apMsg, void *)
{
	if (apMsg->type == asMSGTYPE_ERROR)
		Error("SOMA script: %s:%d:%d: %s\n", apMsg->section, apMsg->row, apMsg->col, apMsg->message);
	else if (apMsg->type == asMSGTYPE_WARNING)
		Log("SOMA script warning: %s:%d: %s\n", apMsg->section, apMsg->row, apMsg->message);
}

static std::string FindApiFile(const std::string &asGame)
{
	using namespace OhplGs6;
	const char *pEnv = getenv("OPENHPL_SOMA_SCRIPT_API");
	std::string sEnv = pEnv ? pEnv : "";
	char sExe[4096];
	ssize_t lLen = readlink("/proc/self/exe", sExe, sizeof(sExe) - 1);
	std::string sExePath = lLen > 0 ? std::string(sExe, lLen) : "";
	std::string sSource;
#ifdef OPENHPL_SOURCE_DIR
	sSource = OPENHPL_SOURCE_DIR;
#endif
	const std::string &sCurrent = SomaGameId();
	return Call8([&](OhplGs6Sink8 s, void *c) {
		ohpl_gs6_find_api_file(Str(asGame), Str(sCurrent), pEnv ? Str(sEnv) : StrNull(), lLen > 0 ? Str(sExePath) : StrNull(),
							   sSource.empty() ? StrNull() : Str(sSource), s, c);
	});
}

cSomaScriptRuntime::cSomaScriptRuntime() : mpEngine(NULL), mpBuilder(NULL), mlModuleCount(0)
{
	mpInstance = this;
}

cSomaScriptRuntime::~cSomaScriptRuntime()
{
	delete mpBuilder;
	if (mpEngine)
		mpEngine->ShutDownAndRelease();
	mpInstance = NULL;
}

bool cSomaScriptRuntime::Init(const std::string &asGameDir)
{
	std::string sApi = FindApiFile(SomaGameId());
	if (sApi.empty())
	{
		Error("SOMA script: script_api.txt not found - game scripts disabled\n");
		return false;
	}

	mpEngine = asCreateScriptEngine();
	mpEngine->SetMessageCallback(asFUNCTION(MessageCallback), NULL, asCALL_CDECL);
	ConfigureSomaScriptEngine(mpEngine);
	RegisterSomaScriptStrings(mpEngine);
	RegisterScriptArray(mpEngine, true);
	RegisterSomaScriptArrayExtras(mpEngine);

	cSomaScriptApi api;
	if (api.Load(sApi) == false)
		return false;
	cSomaScriptApi base;
	if (SomaGameId() != "soma" && base.Load(FindApiFile("soma")))
		api.RebaseEnums(base);
	api.RegisterTypes(mpEngine);
	RegisterSomaScriptNatives(mpEngine);
	int lErrors = api.Register(mpEngine);
	if (lErrors)
	{
		for (size_t i = 0; i < api.GetErrors().size() && i < 20; ++i)
			Error("SOMA script API: %s\n", api.GetErrors()[i].c_str());
		return false;
	}

	mpBuilder = new cSomaScriptBuilder(asGameDir, cString::To8Char(cSomaFsb::GetCacheDir(_W("scripts"))));
	Log("SOMA script: runtime ready (%s)\n", sApi.c_str());
	return true;
}

asIScriptModule *cSomaScriptRuntime::GetModule(const std::string &asFile)
{
	std::map<std::string, asIScriptModule *>::iterator it = mmapModules.find(asFile);
	if (it != mmapModules.end())
		return it->second;

	std::string sPath = asFile;
	if (std::ifstream(sPath.c_str()).is_open() == false)
		sPath = mpBuilder->Resolve(asFile, mpBuilder->GetGameDir() + "/script/");
	if (sPath.empty() && gpSomaBase)
		sPath = cString::To8Char(gpSomaBase->mpEngine->GetResources()->GetFileSearcher()->GetFilePath(asFile));

	std::string sModule = "m" + std::to_string(mlModuleCount++);
	std::string sMissing;
	int r = sPath.empty() ? asERROR : mpBuilder->Build(mpEngine, sModule, sPath, &sMissing);
	asIScriptModule *pModule = NULL;
	if (r < 0)
		Error("SOMA script: could not compile '%s'%s\n", asFile.c_str(), sMissing.empty() ? "" : (" (missing include " + sMissing + ")").c_str());
	else
		pModule = mpEngine->GetModule(sModule.c_str());
	mmapModules[asFile] = pModule;
	return pModule;
}

asIScriptObject *cSomaScriptRuntime::CreateObject(asIScriptModule *apModule, const std::string &asClass)
{
	if (apModule == NULL)
		return NULL;
	asITypeInfo *pType = apModule->GetTypeInfoByName(asClass.c_str());
	if (pType == NULL)
	{
		Error("SOMA script: class '%s' not found in module\n", asClass.c_str());
		return NULL;
	}
	asIScriptObject *pObj = (asIScriptObject *)mpEngine->CreateScriptObject(pType);
	if (pObj == NULL)
		Error("SOMA script: could not create '%s'\n", asClass.c_str());
	return pObj;
}

bool cSomaScriptRuntime::Execute(asIScriptContext *apCtx, const std::string &asWhat)
{
	return Execute(apCtx, [&] { return asWhat; });
}

bool cSomaScriptRuntime::Execute(asIScriptContext *apCtx, const std::function<std::string()> &aWhat)
{
	std::chrono::steady_clock::time_point t0;
	if (mbProfile)
		t0 = std::chrono::steady_clock::now();
	int r = apCtx->Execute();
	if (mbProfile)
	{
		auto &e = mmapProfile[aWhat()];
		e.first += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		++e.second;
	}
	if (r == asEXECUTION_FINISHED)
		return true;
	std::string asWhat = aWhat();
	if (r == asEXECUTION_EXCEPTION)
	{
		const char *pSection = NULL;
		int lLine = apCtx->GetExceptionLineNumber(NULL, &pSection);
		asIScriptFunction *pFunc = apCtx->GetExceptionFunction();
		Error("SOMA script exception in %s: '%s' at %s:%d (%s)\n", asWhat.c_str(), apCtx->GetExceptionString(),
			  pSection ? pSection : "?", lLine, pFunc ? pFunc->GetDeclaration() : "?");
	}
	else
		Error("SOMA script: %s did not finish (%d)\n", asWhat.c_str(), r);
	return false;
}

bool cSomaScriptRuntime::Call(asIScriptObject *apObj, std::string_view asDecl, const std::function<void(asIScriptContext *)> &aSetArgs,
							  const std::function<void(asIScriptContext *)> &aGetResult)
{
	if (apObj == NULL)
		return false;
	asITypeInfo *pType = apObj->GetObjectType();
	auto &mapType = mmapMethods[pType];
	auto it = mapType.find(asDecl);
	if (it == mapType.end())
	{
		// "$Func": a global function of the object's module, like iScriptUserClassInterface::ScriptPrepare
		std::string sDecl(asDecl), sPlain;
		bool bGlobal = ohpl_gs6_split_global_marker(OhplGs6::U(sDecl), sDecl.size(), OhplGs6::ToString, &sPlain);
		asIScriptFunction *pFound = bGlobal == false ? pType->GetMethodByDecl(sDecl.c_str())
									: pType->GetModule() ? pType->GetModule()->GetFunctionByDecl(sPlain.c_str()) : NULL;
		it = mapType.emplace(std::move(sDecl), pFound).first;
	}
	asIScriptFunction *pFunc = it->second;
	if (pFunc == NULL)
		return false;

	asIScriptContext *pCtx = mpEngine->RequestContext();
	pCtx->Prepare(pFunc);
	if (pFunc->GetObjectType())
		pCtx->SetObject(apObj);
	if (aSetArgs)
		aSetArgs(pCtx);
	bool bOk = Execute(pCtx, [&] { return std::string(pType->GetName()) + "::" + std::string(asDecl); });
	// an exception reads as 0/false, like iScriptUserClassInterface::GetReturn*
	if (aGetResult)
		aGetResult(pCtx);
	mpEngine->ReturnContext(pCtx);
	return bOk;
}

namespace
{
	struct cCallbackDecls
	{
		std::string msFunc, msWithArg, msNoArg;
	};
}

bool cSomaScriptRuntime::CallByName(asIScriptObject *apObj, const std::string &asName, const std::string &asArg)
{
	if (apObj == NULL || asName.empty())
		return false;
	cCallbackDecls d;
	bool bGlobal = ohpl_gs6_callback_decls(
		OhplGs6::U(asName), asName.size(), +[](void *c, const unsigned char *p, size_t n) { ((cCallbackDecls *)c)->msFunc.assign((const char *)p, n); },
		+[](void *c, const unsigned char *p, size_t n) { ((cCallbackDecls *)c)->msWithArg.assign((const char *)p, n); },
		+[](void *c, const unsigned char *p, size_t n) { ((cCallbackDecls *)c)->msNoArg.assign((const char *)p, n); }, &d);
	// "$Func": a global function of the object's module rather than a method
	if (bGlobal)
	{
		asIScriptModule *pModule = apObj->GetObjectType()->GetModule();
		asIScriptFunction *pFunc = pModule ? pModule->GetFunctionByDecl(d.msWithArg.c_str()) : NULL;
		bool bArg = pFunc != NULL;
		if (pFunc == NULL && pModule)
			pFunc = pModule->GetFunctionByDecl(d.msNoArg.c_str());
		if (pFunc == NULL)
		{
			Warning("SOMA script: global callback '%s' not found\n", d.msFunc.c_str());
			return false;
		}
		asIScriptContext *pCtx = mpEngine->RequestContext();
		pCtx->Prepare(pFunc);
		if (bArg)
			pCtx->SetArgObject(0, (void *)&asArg);
		bool bOk = Execute(pCtx, d.msFunc);
		mpEngine->ReturnContext(pCtx);
		return bOk;
	}
	asITypeInfo *pType = apObj->GetObjectType();
	if (pType->GetMethodByDecl(d.msWithArg.c_str()))
		return Call(apObj, d.msWithArg, [&](asIScriptContext *apCtx) { apCtx->SetArgObject(0, (void *)&asArg); });
	if (pType->GetMethodByDecl(d.msNoArg.c_str()))
		return Call(apObj, d.msNoArg);
	Warning("SOMA script: callback '%s' not found on %s\n", asName.c_str(), pType->GetName());
	return false;
}

void cSomaScriptRuntime::LogStubReport(int alTop)
{
	const std::map<std::string, int> &counts = cSomaScriptApi::GetStubCallCounts();
	std::vector<OhplGs6Str> vNames;
	std::vector<int> vCounts;
	for (std::map<std::string, int>::const_iterator it = counts.begin(); it != counts.end(); ++it)
	{
		vNames.push_back(OhplGs6::Str(it->first));
		vCounts.push_back(it->second);
	}
	std::vector<std::string> vLines;
	ohpl_gs6_stub_report(vNames.data(), vCounts.data(), vNames.size(), alTop, OhplGs6::AppendString, &vLines);
	for (const std::string &sLine : vLines)
		Log("%s", sLine.c_str());
}

bool cSomaScriptRuntime::Exec(const std::string &asCode, const std::string &asModule, std::string &asError)
{
	asIScriptModule *pModule = asModule.empty() ? mpEngine->GetModule("__exec", asGM_ALWAYS_CREATE) : NULL;
	std::vector<OhplGs6Str> vKeys;
	std::vector<unsigned char> vLoaded;
	std::vector<asIScriptModule *> vModules;
	for (std::map<std::string, asIScriptModule *>::iterator it = mmapModules.begin(); it != mmapModules.end(); ++it)
	{
		vKeys.push_back(OhplGs6::Str(it->first));
		vLoaded.push_back(it->second != NULL);
		vModules.push_back(it->second);
	}
	if (pModule == NULL)
	{
		ptrdiff_t lPick = ohpl_gs6_pick_module(vKeys.data(), (const bool *)vLoaded.data(), vKeys.size(), OhplGs6::U(asModule), asModule.size());
		if (lPick >= 0)
			pModule = vModules[lPick];
	}
	if (pModule == NULL)
	{
		asError = OhplGs6::Call8([&](OhplGs6Sink8 s, void *c) { ohpl_gs6_script_files_error(vKeys.data(), vKeys.size(), s, c); });
		return false;
	}
	asIScriptFunction *pFunc = NULL;
	std::string sMessages;
	mpEngine->SetMessageCallback(asFUNCTION(+[](const asSMessageInfo *msg, void *param) {
		*(std::string *)param += OhplGs6::Call8([&](OhplGs6Sink8 s, void *c) { ohpl_gs6_exec_message(OhplGs6::U(msg->message), strlen(msg->message), msg->row, msg->col, s, c); });
	}), &sMessages, asCALL_CDECL);
	int r = pModule->CompileFunction("exec", OhplGs6::Call8([&](OhplGs6Sink8 s, void *c) { ohpl_gs6_exec_source(OhplGs6::U(asCode), asCode.size(), s, c); }).c_str(), -1, 0, &pFunc);
	mpEngine->SetMessageCallback(asFUNCTION(MessageCallback), NULL, asCALL_CDECL);
	if (r < 0)
	{
		asError = sMessages;
		if (asModule.empty())
			mpEngine->DiscardModule("__exec");
		return false;
	}
	asIScriptContext *pCtx = mpEngine->RequestContext();
	pCtx->Prepare(pFunc);
	bool bOk = Execute(pCtx, "script_exec");
	if (bOk == false)
		asError = pCtx->GetExceptionString() ? pCtx->GetExceptionString() : "failed";
	mpEngine->ReturnContext(pCtx);
	pFunc->Release();
	if (asModule.empty())
		mpEngine->DiscardModule("__exec");
	return bOk;
}
