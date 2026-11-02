#ifndef SOMA_SCRIPT_RUNTIME_H
#define SOMA_SCRIPT_RUNTIME_H

#include <angelscript.h>

#include <functional>
#include <map>
#include <unordered_map>
#include <string>
#include <string_view>

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

	bool Call(asIScriptObject *apObj, std::string_view asDecl,
			  const std::function<void(asIScriptContext *)> &aSetArgs = std::function<void(asIScriptContext *)>(),
			  const std::function<void(asIScriptContext *)> &aGetResult = std::function<void(asIScriptContext *)>());

	bool CallByName(asIScriptObject *apObj, const std::string &asName, const std::string &asArg);

	void LogStubReport(int alTop);

	bool Exec(const std::string &asCode, const std::string &asModule, std::string &asError);

	static cSomaScriptRuntime *Get() { return mpInstance; }

	// inclusive seconds and calls per function, collected while mbProfile is set
	bool mbProfile = false;
	std::map<std::string, std::pair<double, int>> mmapProfile;

private:
	bool Execute(asIScriptContext *apCtx, const std::string &asWhat);
	bool Execute(asIScriptContext *apCtx, const std::function<std::string()> &aWhat);

	static cSomaScriptRuntime *mpInstance;

	asIScriptEngine *mpEngine;
	cSomaScriptBuilder *mpBuilder;
	std::map<std::string, asIScriptModule *> mmapModules;
	struct StringHash { using is_transparent = void; size_t operator()(std::string_view s) const { return std::hash<std::string_view>()(s); } };
	std::unordered_map<asITypeInfo *, std::unordered_map<std::string, asIScriptFunction *, StringHash, std::equal_to<>>> mmapMethods;
	int mlModuleCount;
};

#endif // SOMA_SCRIPT_RUNTIME_H
