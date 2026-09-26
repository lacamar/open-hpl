/*
 * SOMA's script API as registered by the official engine, recovered from its binary
 * (scripts/soma-re-script-api.py -> soma/data/script_api.txt), registered into an
 * AngelScript engine. Anything without a native implementation gets a generic stub that
 * returns a default value and is counted, so every game script compiles and runs.
 */

#ifndef SOMA_SCRIPT_API_H
#define SOMA_SCRIPT_API_H

#include <angelscript.h>

#include <map>
#include <string>
#include <vector>

struct cSomaScriptApiType
{
	std::string msName;
	std::string msKind; // value, ref, interface, template
	int mlSize;
	std::vector<std::pair<std::string, std::string>> mvBehaviours; // kind, params
	std::vector<std::string> mvMethods;
	std::vector<std::pair<std::string, int>> mvProps; // decl, offset (-1 unknown)
	std::vector<std::string> mvCasts;
};

class cSomaScriptApi
{
public:
	bool Load(const std::string &asFile);

	// Enums and types only, so natives can be registered on them before the rest.
	void RegisterTypes(asIScriptEngine *apEngine);
	// Everything not registered yet (natives first) gets a stub; returns the number of failures.
	int Register(asIScriptEngine *apEngine);

	const std::vector<std::string> &GetErrors() const { return mvErrors; }
	const std::vector<std::string> &GetWarnings() const { return mvWarnings; }

	static std::map<std::string, int> &GetStubCallCounts();
	static bool IsValueTypeNative(const std::string &asName);

private:
	void RegisterPropertyAccessors(asIScriptEngine *apEngine, const cSomaScriptApiType &aType, const std::string &asDecl);
	void Fail(const std::string &asWhat, int alCode);

	std::vector<std::pair<std::string, std::vector<std::pair<std::string, int>>>> mvEnums;
	std::vector<cSomaScriptApiType> mvTypes;
	std::vector<std::string> mvGlobals;
	std::vector<std::string> mvGlobalProps;
	std::vector<std::string> mvErrors;
	std::vector<std::string> mvWarnings;
	bool mbTypesRegistered = false;
};

void ConfigureSomaScriptEngine(asIScriptEngine *apEngine);

bool SomaScriptIsDummy(void *apObj);
void SomaScriptStubCall(asIScriptGeneric *apGen);

#endif // SOMA_SCRIPT_API_H
