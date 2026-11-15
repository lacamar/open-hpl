#ifndef SOMA_SCRIPT_API_H
#define SOMA_SCRIPT_API_H

#include <angelscript.h>

#include <map>
#include <set>
#include <string>
#include <vector>

struct OhplS2Api;

class cSomaScriptApi
{
public:
	cSomaScriptApi();
	~cSomaScriptApi();
	cSomaScriptApi(const cSomaScriptApi &) = delete;
	cSomaScriptApi &operator=(const cSomaScriptApi &) = delete;

	bool Load(const std::string &asFile);
	void RebaseEnums(const cSomaScriptApi &aBase);

	void RegisterTypes(asIScriptEngine *apEngine);
	int Register(asIScriptEngine *apEngine);

	const std::vector<std::string> &GetErrors() const { Sync(); return mvErrors; }
	const std::vector<std::string> &GetWarnings() const { Sync(); return mvWarnings; }
	const std::vector<std::string> &GetRemaps() const { Sync(); return mvRemaps; }

	static std::string NativeDecl(const char *apType, const char *apDecl);

	static std::map<std::string, int> &GetStubCallCounts();

private:
	void Sync() const;

	OhplS2Api *mpRust = NULL;

	mutable std::vector<std::string> mvErrors;
	mutable std::vector<std::string> mvWarnings;
	mutable std::vector<std::string> mvRemaps;
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
void SomaScriptReleaseHandleArgs(asIScriptGeneric *apGen);

#endif // SOMA_SCRIPT_API_H
