/*
 * Hosts SOMA's AngelScript game scripts at runtime: one engine with the recovered API
 * (cSomaScriptApi) plus native implementations, a module per script file, and calls into
 * script objects by declaration. Script exceptions are logged with file and line.
 */

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

	// asFile is relative to script/ (config ScriptFile values) or a path; compiled once.
	asIScriptModule *GetModule(const std::string &asFile);

	asIScriptObject *CreateObject(asIScriptModule *apModule, const std::string &asClass);

	// Calls a method on a script object if it exists; returns false if missing or it threw.
	bool Call(asIScriptObject *apObj, const std::string &asDecl,
			  const std::function<void(asIScriptContext *)> &aSetArgs = std::function<void(asIScriptContext *)>(),
			  const std::function<void(asIScriptContext *)> &aGetResult = std::function<void(asIScriptContext *)>());

	// Calls a method by name taking (const tString &in) or nothing: timer and callback targets.
	bool CallByName(asIScriptObject *apObj, const std::string &asName, const std::string &asArg);

	void LogStubReport(int alTop);

	// Compiles asCode as the body of a void function and runs it (headless script_exec)
	bool Exec(const std::string &asCode, std::string &asError);

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
