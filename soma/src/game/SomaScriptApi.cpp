#include "SomaScriptApi.h"
#include "RustSomaScript.h"
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
	static std::map<std::string, int> counts;
	return counts;
}

// Native code and the recovered defaults use SOMA's layout for these structs in every game
static int NativeOffset(const std::string &asType, const std::string &asDecl, int alOffset)
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
	OhplGs6Enums *pEnums = ToRustEnums(mvEnums), *pBase = ToRustEnums(aBase.mvEnums);
	ohpl_gs6_enums_rebase(pEnums, pBase);
	std::vector<int> vValues;
	ohpl_gs6_enums_values(pEnums, OhplGs6::ToInts, &vValues);
	size_t lNext = 0;
	for (auto &e : mvEnums)
		for (auto &v : e.second)
			v.second = vValues[lNext++];
	ohpl_gs6_enums_free(pEnums);
	ohpl_gs6_enums_free(pBase);
}

bool cSomaScriptApi::Load(const std::string &asFile)
{
	std::ifstream f(asFile.c_str());
	if (f.is_open() == false)
		return false;
	std::string sText((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

	ohpl_gs6_api_load(OhplGs6::U(sText), sText.size(),
		+[](void *c, unsigned int alKind, size_t alType, const unsigned char *apA, size_t alA, const unsigned char *apB, size_t alB, int alNum) {
			cSomaScriptApi *self = (cSomaScriptApi *)c;
			std::string sA((const char *)apA, alA), sB((const char *)apB, alB);
			cSomaScriptApiType *pCur = alType < self->mvTypes.size() ? &self->mvTypes[alType] : NULL;
			switch (alKind)
			{
			case kOhplGs6RecEnum:
				self->mvEnums.push_back(std::make_pair(sA, std::vector<std::pair<std::string, int>>()));
				break;
			case kOhplGs6RecEnumValue:
				self->mvEnums.back().second.push_back(std::make_pair(sA, alNum));
				break;
			case kOhplGs6RecType:
			{
				cSomaScriptApiType t;
				t.msName = sA;
				t.msKind = sB;
				t.mlSize = alNum;
				self->mvTypes.push_back(t);
				break;
			}
			case kOhplGs6RecBehaviour:
				if (pCur)
					pCur->mvBehaviours.push_back(std::make_pair(sA, sB));
				break;
			case kOhplGs6RecMethod:
				if (pCur)
					pCur->mvMethods.push_back(sA);
				break;
			case kOhplGs6RecProp:
				if (pCur)
					pCur->mvProps.push_back(std::make_pair(sA, alNum));
				break;
			case kOhplGs6RecCast:
				if (pCur)
					pCur->mvCasts.push_back(sA);
				break;
			case kOhplGs6RecGlobal:
				self->mvGlobals.push_back(sA);
				break;
			case kOhplGs6RecGlobalProp:
				self->mvGlobalProps.push_back(sA);
				break;
			}
		},
		this);
	for (auto &t : mvTypes)
		for (auto &prop : t.mvProps)
			prop.second = NativeOffset(t.msName, prop.first, prop.second);
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
	return apObj && gsetDummies.count(apObj) != 0;
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
	static std::map<std::string, void *> mapDummies;
	void *&pDummy = mapDummies[apType];
	asITypeInfo *pType = apEngine->GetTypeInfoByName(apType);
	if (pDummy == NULL && pType)
	{
		pDummy = calloc(1, 4096);
		ConstructMembers(apEngine, pType, (char *)pDummy);
		gsetDummies.insert(pDummy);
	}
	return pDummy;
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
	Stub(apGen);
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
	return apFunc && apFunc->GetUserData(kSomaStubUserData) != NULL;
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

static const cSomaStructDefaults *FindStructDefaults(const char *apType)
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
	char *pObj = (char *)calloc(1, 4096);
	if (const cSomaStructDefaults *pDefaults = FindStructDefaults(apType))
		ApplyStructDefaults(pDefaults, pObj);
	return pObj;
}

// Refcounts of script-created structs; embedded or native-owned structs are absent
static std::unordered_map<void *, int> gmapStructRefs;

void *SomaNewOwnedScriptStruct(const char *apType)
{
	void *pObj = SomaNewScriptStruct(apType);
	gmapStructRefs[pObj] = 1;
	return pObj;
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
	asIScriptFunction *pFunc = apGen->GetFunction();
	for (asUINT i = 0; i < pFunc->GetParamCount(); ++i)
	{
		int lTypeId;
		asDWORD lFlags;
		pFunc->GetParam(i, &lTypeId, &lFlags);
		if ((lTypeId & asTYPEID_OBJHANDLE) && lFlags == 0)
			ReleaseStruct(*(void **)apGen->GetAddressOfArg(i));
	}
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

void SomaSetIndirectProps(const std::string &asType, int alPointerOffset) { gmapIndirect[asType].mlPointerOffset = alPointerOffset; }

int SomaIndirectPropOffset(const std::string &asType, const std::string &asName)
{
	auto &m = gmapIndirect[asType].mmapOffsets;
	auto it = m.find(asName);
	return it == m.end() ? -1 : it->second;
}

char *SomaNewPropBlock(const std::string &asType)
{
	const cIndirectProps &props = gmapIndirect[asType];
	char *pBlock = (char *)calloc(1, props.mlSize + 64);
	for (int lOff : props.mvStringOffsets)
		new (pBlock + lOff) std::string();
	return pBlock;
}

void SomaFreePropBlock(const std::string &asType, char *apBlock)
{
	if (apBlock == NULL)
		return;
	for (int lOff : gmapIndirect[asType].mvStringOffsets)
		((std::string *)(apBlock + lOff))->~basic_string();
	free(apBlock);
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
	gpActiveApi = this;
	if (mbTypesRegistered)
		return;
	mbTypesRegistered = true;
	int r;
	for (size_t i = 0; i < mvEnums.size(); ++i)
	{
		r = apEngine->RegisterEnum(mvEnums[i].first.c_str());
		if (r < 0 && r != asALREADY_REGISTERED)
			Fail("enum " + mvEnums[i].first, r);
		for (size_t j = 0; j < mvEnums[i].second.size(); ++j)
		{
			r = apEngine->RegisterEnumValue(mvEnums[i].first.c_str(), mvEnums[i].second[j].first.c_str(), mvEnums[i].second[j].second);
			if (r < 0 && r != asALREADY_REGISTERED)
				Fail("enum value " + mvEnums[i].second[j].first, r);
		}
	}

	for (size_t i = 0; i < mvTypes.size(); ++i)
	{
		const cSomaScriptApiType &t = mvTypes[i];
		if (IsValueTypeNative(t.msName) || apEngine->GetTypeInfoByName(t.msName.c_str()))
			continue;
		if (t.msKind == "interface")
			r = apEngine->RegisterInterface(t.msName.c_str());
		else if (t.msKind == "value")
			r = apEngine->RegisterObjectType(t.msName.c_str(), t.mlSize, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE);
		else if (t.msKind == "template")
			continue;
		else if (IsScriptStruct(t))
		{
			r = apEngine->RegisterObjectType(t.msName.c_str(), 0, asOBJ_REF);
			if (r >= 0)
			{
				apEngine->RegisterObjectBehaviour(t.msName.c_str(), asBEHAVE_ADDREF, "void f()", asFUNCTION(StructAddRef), asCALL_GENERIC);
				apEngine->RegisterObjectBehaviour(t.msName.c_str(), asBEHAVE_RELEASE, "void f()", asFUNCTION(StructRelease), asCALL_GENERIC);
			}
		}
		else
			r = apEngine->RegisterObjectType(t.msName.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT);
		if (r < 0 && r != asALREADY_REGISTERED)
			Fail("type " + t.msName, r);
	}
}

int cSomaScriptApi::Register(asIScriptEngine *apEngine)
{
	RegisterTypes(apEngine);
	int r;

	for (size_t i = 0; i < mvTypes.size(); ++i)
	{
		const cSomaScriptApiType &t = mvTypes[i];
		if (t.msKind == "template" || apEngine->GetTypeInfoByName(t.msName.c_str()) == NULL)
			continue;
		const char *pName = t.msName.c_str();

		if (t.msKind == "interface")
		{
			for (size_t j = 0; j < t.mvMethods.size(); ++j)
			{
				r = apEngine->RegisterInterfaceMethod(pName, t.mvMethods[j].c_str());
				if (r < 0 && r != asALREADY_REGISTERED)
					Fail(t.msName + "::" + t.mvMethods[j], r);
			}
			continue;
		}

		for (size_t j = 0; j < t.mvBehaviours.size() && IsValueTypeNative(t.msName) == false && SomaScriptHasNativeBehaviours(pName) == false; ++j)
		{
			const std::string &sKind = t.mvBehaviours[j].first;
			const std::string &sParams = t.mvBehaviours[j].second;
			if (sKind == "construct" && t.msKind == "value")
			{
				std::string sDecl = "void f(" + sParams + ")";
				bool bCopy = ohpl_gs6_is_copy_ctor(OhplGs6::U(sParams), sParams.size(), OhplGs6::U(t.msName), t.msName.size());
				r = apEngine->RegisterObjectBehaviour(pName, asBEHAVE_CONSTRUCT, sDecl.c_str(), bCopy ? asFUNCTION(CopyConstructPod) : asFUNCTION(StubConstruct),
													  asCALL_GENERIC);
			}
			else if ((sKind == "factory" || sKind == "FactoryDefault") && t.msKind == "ref")
			{
				std::string sDecl = t.msName + "@ f(" + (sKind == "factory" ? sParams : "") + ")";
				r = apEngine->RegisterObjectBehaviour(pName, asBEHAVE_FACTORY, sDecl.c_str(), asFUNCTION(StubFactory), asCALL_GENERIC);
			}
			else
				continue;
			if (r < 0 && r != asALREADY_REGISTERED)
				Fail(t.msName + " behaviour " + sKind + "(" + sParams + ")", r);
		}

		asITypeInfo *pTypeInfo = apEngine->GetTypeInfoByName(pName);
		for (size_t j = 0; j < t.mvMethods.size(); ++j)
		{
			if (pTypeInfo->GetMethodByDecl(t.mvMethods[j].c_str()))
				continue;
			bool bLayoutAssign = t.msKind == "ref" && !t.mvProps.empty() && t.mvMethods[j].find("opAssign(const " + t.msName) != std::string::npos;
			r = apEngine->RegisterObjectMethod(pName, t.mvMethods[j].c_str(), asFUNCTION(bLayoutAssign ? MemberAssign : Stub), asCALL_GENERIC);
			if (r >= 0 && bLayoutAssign == false)
				MarkStub(apEngine, r);
			if (r < 0 && r != asALREADY_REGISTERED)
				Fail(t.msName + "::" + t.mvMethods[j], r);
		}

		// The official layout has 8-byte strings: move them past the other props
		auto itIndirect = gmapIndirect.find(t.msName);
		int lStringArea = 0;
		if (itIndirect != gmapIndirect.end())
			for (auto &prop : t.mvProps)
				lStringArea = std::max(lStringArea, prop.second + 32);
		for (size_t j = 0; j < t.mvProps.size(); ++j)
		{
			if (HasProperty(pTypeInfo, t.mvProps[j].first))
				continue;
			if (t.mvProps[j].second >= 0 && itIndirect != gmapIndirect.end())
			{
				cIndirectProps &props = itIndirect->second;
				bool bString = t.mvProps[j].first.compare(0, 8, "tString ") == 0;
				int lOffset = t.mvProps[j].second;
				if (bString)
				{
					lOffset = lStringArea;
					lStringArea += (int)sizeof(std::string);
					props.mvStringOffsets.push_back(lOffset);
				}
				r = apEngine->RegisterObjectProperty(pName, t.mvProps[j].first.c_str(), lOffset, props.mlPointerOffset, true);
				props.mmapOffsets[t.mvProps[j].first.substr(t.mvProps[j].first.find_last_of(' ') + 1)] = lOffset;
				props.mlSize = std::max(props.mlSize, lOffset + 32);
				if (r < 0 && r != asALREADY_REGISTERED)
					Fail(t.msName + " prop " + t.mvProps[j].first, r);
			}
			else if (t.mvProps[j].second >= 0)
			{
				r = apEngine->RegisterObjectProperty(pName, t.mvProps[j].first.c_str(), t.mvProps[j].second);
				if (r < 0 && r != asALREADY_REGISTERED)
					Fail(t.msName + " prop " + t.mvProps[j].first, r);
			}
			else
				RegisterPropertyAccessors(apEngine, t, t.mvProps[j].first);
		}
	}

	// Casts need both types registered: derived -> base implicit, base -> derived explicit
	for (size_t i = 0; i < mvTypes.size(); ++i)
	{
		const cSomaScriptApiType &t = mvTypes[i];
		for (size_t j = 0; j < t.mvCasts.size(); ++j)
		{
			asITypeInfo *pOther = apEngine->GetTypeInfoByName(t.mvCasts[j].c_str());
			if (pOther == NULL || (pOther->GetFlags() & asOBJ_REF) == 0)
				continue;
			std::string sUp = t.mvCasts[j] + "@ opImplCast()";
			std::string sDown = t.msName + "@ opCast()";
			asITypeInfo *pCur = apEngine->GetTypeInfoByName(t.msName.c_str());
			if (pCur == NULL || pCur->GetMethodByDecl(sUp.c_str()) || pOther->GetMethodByDecl(sDown.c_str()))
				continue;
			r = apEngine->RegisterObjectMethod(t.msName.c_str(), sUp.c_str(), asFUNCTION(StubCastSelf), asCALL_GENERIC);
			if (r < 0 && r != asALREADY_REGISTERED)
				Fail(t.msName + "::" + sUp, r);
			r = apEngine->RegisterObjectMethod(t.mvCasts[j].c_str(), sDown.c_str(), asFUNCTION(StubCastSelf), asCALL_GENERIC);
			if (r < 0 && r != asALREADY_REGISTERED)
				Fail(t.mvCasts[j] + "::" + sDown, r);
		}
	}

	std::set<std::string> setNativeNames;
	for (asUINT i = 0; i < apEngine->GetGlobalFunctionCount(); ++i)
		setNativeNames.insert(apEngine->GetGlobalFunctionByIndex(i)->GetName());
	for (size_t i = 0; i < mvGlobals.size(); ++i)
	{
		if (apEngine->GetGlobalFunctionByDecl(mvGlobals[i].c_str()))
			continue;
		size_t lParen = mvGlobals[i].find('(');
		size_t lNameStart = mvGlobals[i].find_last_of(" &@", lParen) + 1;
		std::string sName = mvGlobals[i].substr(lNameStart, lParen - lNameStart);
		if (setNativeNames.count(sName))
			mvWarnings.push_back("native overload of '" + sName + "' does not match API declaration: " + mvGlobals[i]);
		r = apEngine->RegisterGlobalFunction(mvGlobals[i].c_str(), asFUNCTION(Stub), asCALL_GENERIC);
		if (r >= 0)
			MarkStub(apEngine, r);
		if (r < 0 && r != asALREADY_REGISTERED)
			Fail(mvGlobals[i], r);
	}

	for (size_t i = 0; i < mvGlobalProps.size(); ++i)
	{
		std::string sName = mvGlobalProps[i].substr(mvGlobalProps[i].find_last_of(' ') + 1);
		if (apEngine->GetGlobalPropertyIndexByName(sName.c_str()) >= 0)
			continue;
		void *pMem = calloc(1, 256);
		r = apEngine->RegisterGlobalProperty(mvGlobalProps[i].c_str(), pMem);
		if (r < 0 && r != asALREADY_REGISTERED)
			Fail(mvGlobalProps[i], r);
	}

	return (int)mvErrors.size();
}

void ConfigureSomaScriptEngine(asIScriptEngine *apEngine)
{
	// SOMA's scripts use 'x' as integer literals and unqualified & references
	apEngine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, 1);
	apEngine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, 1);
	apEngine->SetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE, 3);
	// AS 2.28 (official): members exist before the constructor body (CreditsHandler's `this = cCreditsStyle();`)
	apEngine->SetEngineProperty(asEP_MEMBER_INIT_MODE, 0);
}
