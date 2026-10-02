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

	void RegisterTypes(asIScriptEngine *apEngine);
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
void *SomaScriptDummyOf(asIScriptEngine *apEngine, const char *apType);

// Native objects smaller than the official layout keep the recovered offset members in a side block
// that a pointer member (at alPointerOffset) addresses
void SomaSetIndirectProps(const std::string &asType, int alPointerOffset);
int SomaIndirectPropOffset(const std::string &asType, const std::string &asName);
char *SomaNewPropBlock(const std::string &asType);
void SomaFreePropBlock(const std::string &asType, char *apBlock);

const asPWORD kSomaStubUserData = 0x50b0;
const asPWORD kSomaForwardUserData = 0x50b1;
bool SomaScriptIsStub(asIScriptFunction *apFunc);
void SomaScriptStubCall(asIScriptGeneric *apGen);

#endif // SOMA_SCRIPT_API_H
