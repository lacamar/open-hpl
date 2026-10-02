#ifndef SOMA_SCRIPT_RUNTIME_H
#define SOMA_SCRIPT_RUNTIME_H

#include <angelscript.h>

#include <functional>
#include <map>
#include <string>

class cSomaScriptBuilder;

class cSomaScriptRuntime
{
public:
	cSomaScriptRuntime();
	~cSomaScriptRuntime();

	bool Init(const std::string &asGameDir);

	asIScriptEngine *GetEngine() { return mpEngine; }

	asIScriptModule *GetModule(const std::string &asFile);

	asIScriptObject *CreateObject(asIScriptModule *apModule, const std::string &asClass);

	bool Call(asIScriptObject *apObj, const std::string &asDecl,
			  const std::function<void(asIScriptContext *)> &aSetArgs = std::function<void(asIScriptContext *)>(),
			  const std::function<void(asIScriptContext *)> &aGetResult = std::function<void(asIScriptContext *)>());

	bool CallByName(asIScriptObject *apObj, const std::string &asName, const std::string &asArg);

	void LogStubReport(int alTop);

	bool Exec(const std::string &asCode, const std::string &asModule, std::string &asError);

	static cSomaScriptRuntime *Get() { return mpInstance; }

private:
	bool Execute(asIScriptContext *apCtx, const std::string &asWhat);
	bool Execute(asIScriptContext *apCtx, const std::function<std::string()> &aWhat);

	static cSomaScriptRuntime *mpInstance;

	asIScriptEngine *mpEngine;
	cSomaScriptBuilder *mpBuilder;
	std::map<std::string, asIScriptModule *> mmapModules;
	std::map<std::pair<asITypeInfo *, std::string>, asIScriptFunction *> mmapMethods;
	int mlModuleCount;
};

#endif // SOMA_SCRIPT_RUNTIME_H
