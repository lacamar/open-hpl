#include "SomaScriptApi.h"
#include "SomaImGuiDefaults.h"
#include "SomaImGui.h"
#include "SomaScriptNatives.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <new>
#include <set>
#include <sstream>
#include <unordered_map>

static std::vector<std::string> SplitTabs(const std::string &asLine)
{
	std::vector<std::string> v;
	std::stringstream ss(asLine);
	std::string s;
	while (std::getline(ss, s, '\t'))
		v.push_back(s);
	return v;
}

// AngelScript 2.28 accepted defaults on &out parameters; 2.38 spells "discard" as = void.
static std::string OutDefaultsToVoid(std::string s)
{
	for (size_t p = s.find("&out"); p != std::string::npos; p = s.find("&out", p + 4))
	{
		size_t lEq = s.find('=', p);
		size_t lComma = s.find_first_of(",)", p);
		if (lEq == std::string::npos || lEq > lComma)
			continue;
		int lDepth = 0;
		size_t e = lEq + 1;
		for (; e < s.size(); ++e)
		{
			if (s[e] == '(' || s[e] == '<')
				++lDepth;
			else if ((s[e] == ')' || s[e] == '>') && lDepth > 0)
				--lDepth;
			else if ((s[e] == ',' || s[e] == ')') && lDepth == 0)
				break;
		}
		s.replace(lEq, e - lEq, "= void");
	}
	return s;
}

// Engine objects are registered NOCOUNT, where auto handles are meaningless and rejected.
static std::string NoAutoHandles(std::string s)
{
	for (size_t p; (p = s.find("@+")) != std::string::npos;)
		s.erase(p + 1, 1);
	return s;
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

bool cSomaScriptApi::Load(const std::string &asFile)
{
	std::ifstream f(asFile.c_str());
	if (f.is_open() == false)
		return false;

	std::string sLine;
	std::map<std::string, size_t> mapTypeIdx;
	cSomaScriptApiType *pCur = NULL;
	while (std::getline(f, sLine))
	{
		if (sLine.empty() || sLine[0] == '#')
			continue;
		std::vector<std::string> v = SplitTabs(sLine);
		const std::string &tag = v[0];
		if (tag == "E")
			mvEnums.push_back(std::make_pair(v[1], std::vector<std::pair<std::string, int>>()));
		else if (tag == "V")
			mvEnums.back().second.push_back(std::make_pair(v[1], atoi(v[2].c_str())));
		else if (tag == "T")
		{
			cSomaScriptApiType t;
			t.msName = v[1];
			t.msKind = v[2];
			t.mlSize = atoi(v[3].c_str());
			if (t.msName == "cVector2f" && t.mlSize == 0)
				t.mlSize = 8; // its RegisterType call is not recovered
			mapTypeIdx[t.msName] = mvTypes.size();
			mvTypes.push_back(t);
		}
		else if (tag == "O")
			pCur = &mvTypes[mapTypeIdx[v[1]]];
		else if (tag == "B" && pCur)
			pCur->mvBehaviours.push_back(std::make_pair(v[1], v.size() > 2 ? v[2] : ""));
		else if (tag == "M" && pCur)
			pCur->mvMethods.push_back(OutDefaultsToVoid(NoAutoHandles(v[1])));
		else if (tag == "P" && pCur)
			pCur->mvProps.push_back(std::make_pair(v[1], atoi(v[2].c_str())));
		else if (tag == "C" && pCur)
			pCur->mvCasts.push_back(v[1]);
		else if (tag == "G")
			mvGlobals.push_back(OutDefaultsToVoid(NoAutoHandles(v[1])));
		else if (tag == "GP")
			mvGlobalProps.push_back(v[1]);
	}
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
	if (sName.find("By") != std::string::npos || sName.compare(0, 4, "Find") == 0 || sName.find("Latest") != std::string::npos ||
		sName.find("FromName") != std::string::npos || sName.find("FromID") != std::string::npos)
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

	asIScriptFunction *pFunc = apGen->GetFunction();
	asDWORD lFlags = 0;
	int lTypeId = pFunc->GetReturnTypeId(&lFlags);
	if (lTypeId == asTYPEID_VOID)
		return;
	if (lFlags & asTM_INOUTREF)
	{
		apGen->SetReturnAddress(DefaultInstance(apGen->GetEngine(), lTypeId & ~asTYPEID_OBJHANDLE));
		return;
	}
	if (lTypeId & asTYPEID_OBJHANDLE)
	{
		apGen->SetReturnAddress(DummyForReturn(apGen->GetEngine(), pFunc, lTypeId));
		return;
	}
	ConstructDefaultAt(apGen->GetEngine(), lTypeId, apGen->GetAddressOfReturnLocation());
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

static void StructRelease(asIScriptGeneric *apGen)
{
	auto it = gmapStructRefs.find(apGen->GetObject());
	if (it != gmapStructRefs.end() && --it->second == 0)
	{
		free(it->first);
		gmapStructRefs.erase(it);
	}
}

static bool IsScriptStruct(const cSomaScriptApiType &aType)
{
	// Backed by HPL2 objects
	static const std::set<std::string> setEngine = {"cBoundingVolume", "cCollideData", "cFrustum", "cAINodeIterator", "cBinaryBuffer"};
	if (aType.msKind != "ref" || setEngine.count(aType.msName))
		return false;
	for (const auto &b : aType.mvBehaviours)
		if (b.first == "factory" || b.first == "FactoryDefault")
			return true;
	return false;
}

static void StubFactory(asIScriptGeneric *apGen)
{
	asITypeInfo *pType = apGen->GetEngine()->GetTypeInfoById(apGen->GetFunction()->GetReturnTypeId() & ~asTYPEID_OBJHANDLE);
	const cSomaStructDefaults *pDefaults = pType ? FindStructDefaults(pType->GetName()) : NULL;
	// Official factories zero these
	static const std::set<std::string> setZeroed = {"cLuxClosestEntityData", "cLuxClosestCharCollider", "cLuxSoundExtraData", "cLuxScreenTextFormatParameters"};
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
	size_t lEnd = asDecl.find_last_not_of(" ;");
	size_t lStart = asDecl.find_last_of(" @&", lEnd);
	std::string sName = asDecl.substr(lStart + 1, lEnd - lStart);
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

static bool IsPrimitiveTypeName(const std::string &asType, const std::set<std::string> &aEnums)
{
	static const std::set<std::string> setPrim = {"bool", "int", "uint", "int8", "int16", "int64", "uint8", "uint16",
												   "uint64", "float", "double"};
	return setPrim.count(asType) || aEnums.count(asType);
}

void cSomaScriptApi::RegisterPropertyAccessors(asIScriptEngine *apEngine, const cSomaScriptApiType &aType, const std::string &asDecl)
{
	std::string sDecl = asDecl;
	while (!sDecl.empty() && (sDecl.back() == ' ' || sDecl.back() == ';'))
		sDecl.pop_back();
	size_t lNamePos = sDecl.find_last_of(" \t@&");
	if (lNamePos == std::string::npos)
		return;
	std::string sName = sDecl.substr(lNamePos + 1);
	std::string sType = sDecl.substr(0, lNamePos + 1);
	while (!sType.empty() && sType.back() == ' ')
		sType.pop_back();

	bool bConst = sType.compare(0, 6, "const ") == 0;
	std::string sBare = bConst ? sType.substr(6) : sType;
	bool bHandle = !sBare.empty() && sBare.back() == '@';

	std::set<std::string> setEnums;
	for (size_t i = 0; i < mvEnums.size(); ++i)
		setEnums.insert(mvEnums[i].first);

	std::string sGet = sBare + " get_" + sName + "() const property";
	int r = apEngine->RegisterObjectMethod(aType.msName.c_str(), sGet.c_str(), asFUNCTION(Stub), asCALL_GENERIC);
	if (r < 0 && r != asALREADY_REGISTERED)
		Fail(aType.msName + "::" + sGet, r);
	if (bConst)
		return;

	std::string sArg = (bHandle || IsPrimitiveTypeName(sBare, setEnums)) ? sBare : "const " + sBare + " &in";
	std::string sSet = "void set_" + sName + "(" + sArg + ") property";
	r = apEngine->RegisterObjectMethod(aType.msName.c_str(), sSet.c_str(), asFUNCTION(Stub), asCALL_GENERIC);
	if (r < 0 && r != asALREADY_REGISTERED)
		Fail(aType.msName + "::" + sSet, r);
}

void cSomaScriptApi::RegisterTypes(asIScriptEngine *apEngine)
{
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
				bool bCopy = sParams == "const " + t.msName + " &in" || sParams == "const " + t.msName + "&in";
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
