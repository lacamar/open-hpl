#include "SomaScriptRuntime.h"
#include "SomaScriptApi.h"
#include "SomaScriptBuilder.h"
#include "SomaScriptNatives.h"
#include "SomaScriptStrings.h"

#include "impl/scriptarray.h"
#include "system/LowLevelSystem.h"

#include <algorithm>
#include <fstream>
#include <unistd.h>
#include <vector>

using namespace hpl;

cSomaScriptRuntime *cSomaScriptRuntime::mpInstance = NULL;

//---------------------------------------

static void MessageCallback(const asSMessageInfo *apMsg, void *)
{
	if (apMsg->type == asMSGTYPE_ERROR)
		Error("SOMA script: %s:%d:%d: %s\n", apMsg->section, apMsg->row, apMsg->col, apMsg->message);
	else if (apMsg->type == asMSGTYPE_WARNING)
		Log("SOMA script warning: %s:%d: %s\n", apMsg->section, apMsg->row, apMsg->message);
}

static std::string FindApiFile()
{
	std::vector<std::string> vCandidates;
	if (const char *pEnv = getenv("OPENHPL_SOMA_SCRIPT_API"))
		vCandidates.push_back(pEnv);
	char sExe[4096];
	ssize_t lLen = readlink("/proc/self/exe", sExe, sizeof(sExe) - 1);
	if (lLen > 0)
	{
		sExe[lLen] = 0;
		std::string sDir(sExe);
		sDir = sDir.substr(0, sDir.find_last_of('/') + 1);
		vCandidates.push_back(sDir + "script_api.txt");
		vCandidates.push_back(sDir + "../share/open-hpl/soma/script_api.txt");
	}
	vCandidates.push_back("/usr/share/open-hpl/soma/script_api.txt");
#ifdef OPENHPL_SOURCE_DIR
	vCandidates.push_back(std::string(OPENHPL_SOURCE_DIR) + "/soma/data/script_api.txt");
#endif
	for (size_t i = 0; i < vCandidates.size(); ++i)
		if (std::ifstream(vCandidates[i].c_str()).is_open())
			return vCandidates[i];
	return "";
}

//---------------------------------------

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
	std::string sApi = FindApiFile();
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
	api.RegisterTypes(mpEngine);
	RegisterSomaScriptNatives(mpEngine);
	int lErrors = api.Register(mpEngine);
	if (lErrors)
	{
		for (size_t i = 0; i < api.GetErrors().size() && i < 20; ++i)
			Error("SOMA script API: %s\n", api.GetErrors()[i].c_str());
		return false;
	}

	mpBuilder = new cSomaScriptBuilder(asGameDir);
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
	int r = apCtx->Execute();
	if (r == asEXECUTION_FINISHED)
		return true;
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

bool cSomaScriptRuntime::Call(asIScriptObject *apObj, const std::string &asDecl, const std::function<void(asIScriptContext *)> &aSetArgs)
{
	if (apObj == NULL)
		return false;
	asIScriptFunction *pFunc = apObj->GetObjectType()->GetMethodByDecl(asDecl.c_str());
	if (pFunc == NULL)
		return false;

	asIScriptContext *pCtx = mpEngine->RequestContext();
	pCtx->Prepare(pFunc);
	pCtx->SetObject(apObj);
	if (aSetArgs)
		aSetArgs(pCtx);
	bool bOk = Execute(pCtx, std::string(apObj->GetObjectType()->GetName()) + "::" + asDecl);
	mpEngine->ReturnContext(pCtx);
	return bOk;
}

bool cSomaScriptRuntime::CallByName(asIScriptObject *apObj, const std::string &asName, const std::string &asArg)
{
	if (apObj == NULL)
		return false;
	asITypeInfo *pType = apObj->GetObjectType();
	std::string sWithArg = "void " + asName + "(const tString &in)";
	if (pType->GetMethodByDecl(sWithArg.c_str()))
		return Call(apObj, sWithArg, [&](asIScriptContext *apCtx) { apCtx->SetArgObject(0, (void *)&asArg); });
	std::string sNoArg = "void " + asName + "()";
	if (pType->GetMethodByDecl(sNoArg.c_str()))
		return Call(apObj, sNoArg);
	Warning("SOMA script: callback '%s' not found on %s\n", asName.c_str(), pType->GetName());
	return false;
}

void cSomaScriptRuntime::LogStubReport(int alTop)
{
	std::vector<std::pair<int, std::string>> v;
	const std::map<std::string, int> &counts = cSomaScriptApi::GetStubCallCounts();
	for (std::map<std::string, int>::const_iterator it = counts.begin(); it != counts.end(); ++it)
		v.push_back(std::make_pair(it->second, it->first));
	std::sort(v.rbegin(), v.rend());
	Log("SOMA script: %d unimplemented API functions called\n", (int)v.size());
	for (int i = 0; i < (int)v.size() && i < alTop; ++i)
		Log("  %6d  %s\n", v[i].first, v[i].second.c_str());
}
