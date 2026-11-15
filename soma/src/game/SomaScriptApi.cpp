#include "SomaScriptApi.h"
#include "RustSomaScript.h"
#include "RustSomaScriptS2.h"
#include "SomaImGuiDefaults.h"
#include "SomaImGui.h"
#include "SomaScriptNatives.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <new>
#include <set>
#include <iterator>
#include <sstream>
#include <unordered_map>

// The generic wrappers read enums and ints the same way and ignore trailing arguments, so a native bound
// to an older game's declaration can serve one that swapped an enum for an int or appended defaulted params
std::string cSomaScriptApi::NativeDecl(const char *apType, const char *apDecl)
{
	size_t lType = apType ? strlen(apType) : 0, lDecl = strlen(apDecl);
	return OhplGs6::Call8([&](OhplGs6Sink8 k, void *c) { ohpl_s2_native_decl(apType ? (const unsigned char *)apType : NULL, lType, (const unsigned char *)apDecl, lDecl, k, c); });
}

std::map<std::string, int> &cSomaScriptApi::GetStubCallCounts()
{
	static std::map<std::string, int> mapRust;
	mapRust.clear();
	ohpl_s2_stub_counts(+[](void *c, const unsigned char *p, size_t n, int l) { (*(std::map<std::string, int> *)c)[std::string((const char *)p, n)] = l; }, &mapRust);
	return mapRust;
}

// Native code and the recovered defaults use SOMA's layout for these structs in every game
int SomaNativeOffset(const std::string &asType, const std::string &asDecl, int alOffset)
{
	static const struct { const char *mpType, *mpName; int mlOffset; } vLayout[] = {
#include "SomaImGuiLayout.inc"
		{"cImGuiGfx", "mvAlphaLimit", 160}, // Rebirth only, spare bytes of the SOMA struct
	};
	std::string sName = asDecl.substr(asDecl.find_last_of(' ') + 1);
	bool bType = false;
	for (auto &e : vLayout)
	{
		if (asType != e.mpType)
			continue;
		if (sName == e.mpName)
			return e.mlOffset;
		bType = true;
	}
	return bType ? -1 : alOffset;
}

// native code uses SOMA's enum values; later games insert names in the middle
void cSomaScriptApi::RebaseEnums(const cSomaScriptApi &aBase)
{
	ohpl_s2_api_rebase(mpRust, aBase.mpRust);
}

bool cSomaScriptApi::Load(const std::string &asFile)
{
	std::ifstream fRust(asFile.c_str());
	if (fRust.is_open() == false)
		return false;
	std::string sRust((std::istreambuf_iterator<char>(fRust)), std::istreambuf_iterator<char>());
	OhplS2InstallHost();
	mpRust = ohpl_s2_api_new();
	ohpl_s2_api_load(mpRust, OhplGs6::U(sRust), sRust.size());
	return true;
}

bool SomaScriptIsDummy(void *apObj)
{
	return ohpl_s2_is_dummy(apObj);
}

void *SomaScriptDummyOf(asIScriptEngine *apEngine, const char *apType)
{
	return ohpl_s2_dummy_of(apEngine, apType);
}

void SomaScriptStubCall(asIScriptGeneric *apGen)
{
	ohpl_s2_stub_call(apGen);
}

bool SomaScriptIsStub(asIScriptFunction *apFunc)
{
	return ohpl_s2_is_stub(apFunc);
}

const tString *SomaIntern(const tString &asStr)
{
	static std::set<tString> setStrings;
	return &*setStrings.insert(asStr).first;
}

const cSomaStructDefaults *FindStructDefaults(const char *apType)
{
	for (int i = 0; i < glSomaStructDefaultsNum; ++i)
		if (strcmp(gvSomaStructDefaults[i].mpType, apType) == 0)
			return &gvSomaStructDefaults[i];
	return NULL;
}

void *SomaNewScriptStruct(const char *apType)
{
	return ohpl_s2_new_struct(apType, false);
}

void *SomaNewOwnedScriptStruct(const char *apType)
{
	return ohpl_s2_new_struct(apType, true);
}

// AngelScript hands the callee a reference with each handle argument
void SomaScriptReleaseHandleArgs(asIScriptGeneric *apGen)
{
	ohpl_s2_release_handle_args(apGen);
}

cSomaScriptApi::cSomaScriptApi() {}

cSomaScriptApi::~cSomaScriptApi()
{
	if (mpRust)
		ohpl_s2_api_free(mpRust);
}

void cSomaScriptApi::Sync() const
{
	if (mpRust == NULL)
		return;
	std::vector<std::string> *vLists[] = {&mvErrors, &mvWarnings, &mvRemaps};
	for (unsigned i = 0; i < 3; ++i)
	{
		vLists[i]->clear();
		ohpl_s2_api_list(mpRust, i, OhplGs6::AppendString, vLists[i]);
	}
}

void SomaSetIndirectProps(const std::string &asType, int alPointerOffset)
{
	ohpl_s2_set_indirect(OhplGs6::U(asType), asType.size(), alPointerOffset);
}

int SomaIndirectPropOffset(const std::string &asType, const std::string &asName)
{
	return ohpl_s2_indirect_offset(OhplGs6::U(asType), asType.size(), OhplGs6::U(asName), asName.size());
}

char *SomaNewPropBlock(const std::string &asType)
{
	return (char *)ohpl_s2_new_prop_block(OhplGs6::U(asType), asType.size());
}

void SomaFreePropBlock(const std::string &asType, char *apBlock)
{
	ohpl_s2_free_prop_block(OhplGs6::U(asType), asType.size(), apBlock);
}

void cSomaScriptApi::RegisterTypes(asIScriptEngine *apEngine)
{
	ohpl_s2_api_register_types(mpRust, apEngine);
}

int cSomaScriptApi::Register(asIScriptEngine *apEngine)
{
	int lRust = ohpl_s2_api_register(mpRust, apEngine);
	return lRust;
}

void ConfigureSomaScriptEngine(asIScriptEngine *apEngine)
{
	ohpl_s2_configure_engine(apEngine);
}
