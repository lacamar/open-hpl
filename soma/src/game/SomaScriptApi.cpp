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

static std::string DeclName(const std::string &asDecl)
{
	return OhplGs6::Call8([&](OhplGs6Sink8 k, void *c) { ohpl_gs6_decl_name(OhplGs6::U(asDecl), asDecl.size(), k, c); });
}

static cSomaScriptApi *gpActiveApi = NULL;

// The generic wrappers read enums and ints the same way and ignore trailing arguments, so a native bound
// to an older game's declaration can serve one that swapped an enum for an int or appended defaulted params
std::string cSomaScriptApi::NativeDecl(const char *apType, const char *apDecl)
{
	size_t lType = apType ? strlen(apType) : 0, lDecl = strlen(apDecl);
	return OhplGs6::Call8([&](OhplGs6Sink8 k, void *c) { ohpl_s2_native_decl(apType ? (const unsigned char *)apType : NULL, lType, (const unsigned char *)apDecl, lDecl, k, c); });
	cSomaScriptApi *pApi = gpActiveApi;
	if (pApi == NULL)
		return apDecl;
	if (pApi->mmapSigs.empty())
	{
		for (const cSomaScriptApiType &t : pApi->mvTypes)
			for (const std::string &m : t.mvMethods)
				pApi->mmapSigs[t.msName + "::" + DeclName(m)].push_back(m);
		for (const std::string &g : pApi->mvGlobals)
			pApi->mmapSigs["::" + DeclName(g)].push_back(g);
		for (auto &e : pApi->mvEnums)
			pApi->msetEnumNames.insert(e.first);
	}
	std::string sDecl = apDecl;
	auto it = pApi->mmapSigs.find(std::string(apType ? apType : "") + "::" + DeclName(sDecl));
	if (it == pApi->mmapSigs.end())
		return apDecl;
	std::vector<OhplGs6Str> vCands, vEnums;
	for (const std::string &s : it->second)
		vCands.push_back(OhplGs6::Str(s));
	for (const std::string &s : pApi->msetEnumNames)
		vEnums.push_back(OhplGs6::Str(s));
	ptrdiff_t lPick = ohpl_gs6_native_decl_pick(vCands.data(), vCands.size(), vEnums.data(), vEnums.size(), OhplGs6::U(sDecl), sDecl.size());
	if (lPick < 0)
		return apDecl;
	const std::string &sMatch = it->second[lPick];
	pApi->mvRemaps.push_back(std::string(apType ? apType : "") + "::" + apDecl + " -> " + sMatch);
	return sMatch;
}

static std::set<std::string> gsetNativeValueTypes = {"tString", "tWString"};

bool cSomaScriptApi::IsValueTypeNative(const std::string &asName)
{
	return gsetNativeValueTypes.count(asName) != 0;
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

static OhplGs6Enums *ToRustEnums(const std::vector<std::pair<std::string, std::vector<std::pair<std::string, int>>>> &avEnums)
{
	OhplGs6Enums *pEnums = ohpl_gs6_enums_new();
	for (const auto &e : avEnums)
	{
		ohpl_gs6_enums_add(pEnums, OhplGs6::U(e.first), e.first.size());
		for (const auto &v : e.second)
			ohpl_gs6_enums_add_value(pEnums, OhplGs6::U(v.first), v.first.size(), v.second);
	}
	return pEnums;
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

static void *DefaultInstance(asIScriptEngine *apEngine, int alTypeId)
{
	static std::map<int, void *> mapDefaults;
	std::map<int, void *>::iterator it = mapDefaults.find(alTypeId);
	if (it != mapDefaults.end())
		return it->second;

	void *pObj = NULL;
	asITypeInfo *pType = apEngine->GetTypeInfoById(alTypeId);
	if (pType && (pType->GetFlags() & asOBJ_VALUE) && (pType->GetFlags() & asOBJ_POD) == 0)
		pObj = apEngine->CreateScriptObject(pType);
	else
	{
		int lSize = pType ? pType->GetSize() : apEngine->GetSizeOfPrimitiveType(alTypeId);
		pObj = calloc(1, lSize > 0 ? lSize : 64);
	}
	mapDefaults[alTypeId] = pObj;
	return pObj;
}

static void ConstructDefaultAt(asIScriptEngine *apEngine, int alTypeId, void *apMem)
{
	asITypeInfo *pType = apEngine->GetTypeInfoById(alTypeId);
	if (pType == NULL)
	{
		memset(apMem, 0, apEngine->GetSizeOfPrimitiveType(alTypeId));
		return;
	}
	if (pType->GetFlags() & asOBJ_POD)
	{
		memset(apMem, 0, pType->GetSize());
		return;
	}
	if (strcmp(pType->GetName(), "tString") == 0)
		new (apMem) std::string();
	else if (strcmp(pType->GetName(), "tWString") == 0)
		new (apMem) std::wstring();
	else
		memset(apMem, 0, pType->GetSize());
}

static void ConstructMembers(asIScriptEngine *apEngine, asITypeInfo *apType, char *apObj);

// Engine objects not implemented yet: stubs hand out one zeroed "null object" per type instead of
// null, so scripts keep running; natives called on one fall back to the stub (SomaBind).
static std::set<void *> gsetDummies;

bool SomaScriptIsDummy(void *apObj)
{
	return ohpl_s2_is_dummy(apObj);
}

static void *DummyForReturn(asIScriptEngine *apEngine, asIScriptFunction *apFunc, int alTypeId)
{
	asITypeInfo *pType = apEngine->GetTypeInfoById(alTypeId & ~asTYPEID_OBJHANDLE);
	if (pType == NULL || (pType->GetFlags() & asOBJ_SCRIPT_OBJECT) || (pType->GetFlags() & asOBJ_NOCOUNT) == 0)
		return NULL;
	// Lookups: null means "not found"
	std::string sName = apFunc->GetName();
	if (ohpl_gs6_is_lookup_name(OhplGs6::U(sName), sName.size()))
		return NULL;
	static std::map<int, void *> mapDummies;
	void *&pDummy = mapDummies[pType->GetTypeId()];
	if (pDummy == NULL)
	{
		pDummy = calloc(1, 4096);
		ConstructMembers(apEngine, pType, (char *)pDummy);
		gsetDummies.insert(pDummy);
	}
	return pDummy;
}

void *SomaScriptDummyOf(asIScriptEngine *apEngine, const char *apType)
{
	return ohpl_s2_dummy_of(apEngine, apType);
}

static void CountStub(asIScriptGeneric *apGen)
{
	asIScriptFunction *pFunc = apGen->GetFunction();
	++cSomaScriptApi::GetStubCallCounts()[pFunc->GetDeclaration(true, false, false)];
}

static void Stub(asIScriptGeneric *apGen)
{
	CountStub(apGen);
	SomaScriptReleaseHandleArgs(apGen);

	asIScriptFunction *pFunc = apGen->GetFunction();
	asDWORD lFlags = 0;
	int lTypeId = pFunc->GetReturnTypeId(&lFlags);
	switch (ohpl_gs6_stub_return_kind(lTypeId == asTYPEID_VOID, (lFlags & asTM_INOUTREF) != 0, (lTypeId & asTYPEID_OBJHANDLE) != 0))
	{
	case kOhplGs6StubVoid:
		return;
	case kOhplGs6StubDefaultInstance:
		apGen->SetReturnAddress(DefaultInstance(apGen->GetEngine(), lTypeId & ~asTYPEID_OBJHANDLE));
		return;
	case kOhplGs6StubHandle:
		apGen->SetReturnAddress(DummyForReturn(apGen->GetEngine(), pFunc, lTypeId));
		return;
	default:
		ConstructDefaultAt(apGen->GetEngine(), lTypeId, apGen->GetAddressOfReturnLocation());
	}
}

void SomaScriptStubCall(asIScriptGeneric *apGen)
{
	ohpl_s2_stub_call(apGen);
}

static void StubConstruct(asIScriptGeneric *apGen)
{
	CountStub(apGen);
	asITypeInfo *pType = apGen->GetFunction()->GetObjectType();
	ConstructDefaultAt(apGen->GetEngine(), pType->GetTypeId(), apGen->GetObject());
}

static void MarkStub(asIScriptEngine *apEngine, int alFuncId)
{
	if (asIScriptFunction *pFunc = apEngine->GetFunctionById(alFuncId))
		pFunc->SetUserData((void *)1, kSomaStubUserData);
}

bool SomaScriptIsStub(asIScriptFunction *apFunc)
{
	return ohpl_s2_is_stub(apFunc);
}

// Copy constructors of POD value types copy the bytes
static void CopyConstructPod(asIScriptGeneric *apGen)
{
	asITypeInfo *pType = apGen->GetFunction()->GetObjectType();
	memcpy(apGen->GetObject(), apGen->GetArgObject(0), pType->GetSize());
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

static void ApplyStructDefaults(const cSomaStructDefaults *apDefaults, char *apObj)
{
	memcpy(apObj + 16, apDefaults->mpBytes, apDefaults->mlSize - 16);
	for (int lOff : apDefaults->mvStringOffsets)
		*(const tString **)(apObj + lOff) = SomaIntern("");
}

// Properties sit at their recovered offsets (max 3560), so a zeroed block holds any of them;
// only strings and structs with recovered defaults need constructing.
static void ConstructMembers(asIScriptEngine *apEngine, asITypeInfo *apType, char *apObj)
{
	for (asUINT i = 0; i < apType->GetPropertyCount(); ++i)
	{
		int lTypeId, lOffset;
		apType->GetProperty(i, NULL, &lTypeId, NULL, NULL, &lOffset);
		if (lTypeId & asTYPEID_OBJHANDLE)
			continue;
		asITypeInfo *pPropType = apEngine->GetTypeInfoById(lTypeId);
		if (pPropType == NULL)
			continue;
		if (strcmp(pPropType->GetName(), "tString") == 0)
			new (apObj + lOffset) std::string();
		else if (strcmp(pPropType->GetName(), "tWString") == 0)
			new (apObj + lOffset) std::wstring();
		else if (const cSomaStructDefaults *pDefaults = FindStructDefaults(pPropType->GetName()))
			ApplyStructDefaults(pDefaults, apObj + lOffset);
		else
			ConstructMembers(apEngine, pPropType, apObj + lOffset);
	}
}

void *SomaNewScriptStruct(const char *apType)
{
	return ohpl_s2_new_struct(apType, false);
}

// Refcounts of script-created structs; embedded or native-owned structs are absent
static std::unordered_map<void *, int> gmapStructRefs;

void *SomaNewOwnedScriptStruct(const char *apType)
{
	return ohpl_s2_new_struct(apType, true);
}

static void StructAddRef(asIScriptGeneric *apGen)
{
	auto it = gmapStructRefs.find(apGen->GetObject());
	if (it != gmapStructRefs.end())
		++it->second;
}

static void ReleaseStruct(void *apObj)
{
	auto it = gmapStructRefs.find(apObj);
	if (it != gmapStructRefs.end() && --it->second == 0)
	{
		free(it->first);
		gmapStructRefs.erase(it);
	}
}

static void StructRelease(asIScriptGeneric *apGen)
{
	ReleaseStruct(apGen->GetObject());
}

// AngelScript hands the callee a reference with each handle argument
void SomaScriptReleaseHandleArgs(asIScriptGeneric *apGen)
{
	ohpl_s2_release_handle_args(apGen);
}

static bool IsScriptStruct(const cSomaScriptApiType &aType)
{
	bool bFactory = false;
	for (const auto &b : aType.mvBehaviours)
		if (b.first == "factory" || b.first == "FactoryDefault")
			bFactory = true;
	return ohpl_gs6_is_script_struct(OhplGs6::U(aType.msKind), aType.msKind.size(), OhplGs6::U(aType.msName), aType.msName.size(), bFactory);
}

static void StubFactory(asIScriptGeneric *apGen)
{
	asITypeInfo *pType = apGen->GetEngine()->GetTypeInfoById(apGen->GetFunction()->GetReturnTypeId() & ~asTYPEID_OBJHANDLE);
	const cSomaStructDefaults *pDefaults = pType ? FindStructDefaults(pType->GetName()) : NULL;
	// Official factories zero these
	static const std::set<std::string> setZeroed = {"cLuxClosestEntityData", "cLuxClosestCharCollider", "cLuxSoundExtraData", "cLuxScreenTextFormatParameters", "cPhysicsContactData"};
	if ((pDefaults == NULL && (pType == NULL || setZeroed.count(pType->GetName()) == 0)) || apGen->GetArgCount() > 0)
		CountStub(apGen);
	char *pObj = (char *)calloc(1, 4096);
	if (pDefaults)
		ApplyStructDefaults(pDefaults, pObj);
	else if (pType)
		ConstructMembers(apGen->GetEngine(), pType, pObj);
	// Rebirth cLuxScreenTextFormatParameters::Clear: unset line width and icon heights
	if (pType && strcmp(pType->GetName(), "cLuxScreenTextFormatParameters") == 0)
		for (int o : {16, 360, 364, 368})
			*(float *)(pObj + o) = -1;
	if (pType && (pType->GetFlags() & asOBJ_NOCOUNT) == 0)
		gmapStructRefs[pObj] = 1;
	*(void **)apGen->GetAddressOfReturnLocation() = pObj;
}

// Member-wise copy by recovered layout: primitives, strings, nested members; handles are copied as pointers.
static void AssignMembers(asIScriptEngine *apEngine, asITypeInfo *apType, char *apDst, const char *apSrc)
{
	for (asUINT i = 0; i < apType->GetPropertyCount(); ++i)
	{
		int lTypeId, lOffset;
		apType->GetProperty(i, NULL, &lTypeId, NULL, NULL, &lOffset);
		if (lTypeId & asTYPEID_OBJHANDLE)
		{
			*(void **)(apDst + lOffset) = *(void *const *)(apSrc + lOffset);
			continue;
		}
		asITypeInfo *pPropType = apEngine->GetTypeInfoById(lTypeId);
		if (pPropType == NULL)
			memcpy(apDst + lOffset, apSrc + lOffset, apEngine->GetSizeOfPrimitiveType(lTypeId));
		else if (strcmp(pPropType->GetName(), "tString") == 0)
			*(std::string *)(apDst + lOffset) = *(const std::string *)(apSrc + lOffset);
		else if (strcmp(pPropType->GetName(), "tWString") == 0)
			*(std::wstring *)(apDst + lOffset) = *(const std::wstring *)(apSrc + lOffset);
		else if (pPropType->GetFlags() & asOBJ_POD)
			memcpy(apDst + lOffset, apSrc + lOffset, pPropType->GetSize());
		else
			AssignMembers(apEngine, pPropType, apDst + lOffset, apSrc + lOffset);
	}
}

static void MemberAssign(asIScriptGeneric *apGen)
{
	asITypeInfo *pType = apGen->GetFunction()->GetObjectType();
	// Structs with recovered layouts carry unregistered members (file names)
	if (const cSomaStructDefaults *pDefaults = FindStructDefaults(pType->GetName()))
	{
		if (apGen->GetObject() != apGen->GetArgObject(0))
			memcpy((char *)apGen->GetObject() + 16, (const char *)apGen->GetArgObject(0) + 16, pDefaults->mlSize - 16);
		apGen->SetReturnAddress(apGen->GetObject());
		return;
	}
	AssignMembers(apGen->GetEngine(), pType, (char *)apGen->GetObject(), (const char *)apGen->GetArgObject(0));
	apGen->SetReturnAddress(apGen->GetObject());
}

static void StubCastSelf(asIScriptGeneric *apGen)
{
	*(void **)apGen->GetAddressOfReturnLocation() = apGen->GetObject();
}

static bool HasProperty(asITypeInfo *apType, const std::string &asDecl)
{
	std::string sName = OhplGs6::Call8([&](OhplGs6Sink8 k, void *c) { ohpl_gs6_prop_name(OhplGs6::U(asDecl), asDecl.size(), k, c); });
	for (asUINT i = 0; i < apType->GetPropertyCount(); ++i)
	{
		const char *pName;
		apType->GetProperty(i, &pName);
		if (sName == pName)
			return true;
	}
	return false;
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

void cSomaScriptApi::Fail(const std::string &asWhat, int alCode)
{
	char sBuf[32];
	snprintf(sBuf, sizeof(sBuf), " (%d)", alCode);
	mvErrors.push_back(asWhat + sBuf);
}

namespace
{
	struct cIndirectProps
	{
		int mlPointerOffset = -1;
		int mlSize = 0;
		std::vector<int> mvStringOffsets;
		std::map<std::string, int> mmapOffsets;
	};
	std::map<std::string, cIndirectProps> gmapIndirect;
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

void cSomaScriptApi::RegisterPropertyAccessors(asIScriptEngine *apEngine, const cSomaScriptApiType &aType, const std::string &asDecl)
{
	std::vector<OhplGs6Str> vEnums;
	for (size_t i = 0; i < mvEnums.size(); ++i)
		vEnums.push_back(OhplGs6::Str(mvEnums[i].first));
	std::string vDecls[2];
	if (ohpl_gs6_property_accessors(OhplGs6::U(asDecl), asDecl.size(), vEnums.data(), vEnums.size(),
									+[](void *c, const unsigned char *p, size_t n) { ((std::string *)c)[0].assign((const char *)p, n); },
									+[](void *c, const unsigned char *p, size_t n) { ((std::string *)c)[1].assign((const char *)p, n); }, vDecls) == false)
		return;

	int r = apEngine->RegisterObjectMethod(aType.msName.c_str(), vDecls[0].c_str(), asFUNCTION(Stub), asCALL_GENERIC);
	if (r < 0 && r != asALREADY_REGISTERED)
		Fail(aType.msName + "::" + vDecls[0], r);
	if (vDecls[1].empty())
		return;
	r = apEngine->RegisterObjectMethod(aType.msName.c_str(), vDecls[1].c_str(), asFUNCTION(Stub), asCALL_GENERIC);
	if (r < 0 && r != asALREADY_REGISTERED)
		Fail(aType.msName + "::" + vDecls[1], r);
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
