#include "SomaLuxEntity.h"
#include "SomaScriptBind.h"
#include "SomaLux.h"
#include "SomaSave.h"
#include "SomaGlobalFuncsTable.h"
#include "SomaBase.h"
#include "SomaScriptNatives.h"
#include "SomaScriptRuntime.h"
#include "RustSomaScript.h"
#include "impl/scriptarray.h"

#include <map>
#include <unordered_map>
#include <cstring>
#include <limits>
#include <sstream>

using namespace OhplGs6;

static void SetWords(unsigned int alScope, const tString &asName, int alIdx, unsigned int alField, const void *apData, size_t alBytes)
{
	unsigned int vWords[16] = {};
	memcpy(vWords, apData, alBytes);
	ohpl_gs6_var_set(alScope, U(asName), asName.size(), alIdx, alField, vWords, nullptr, 0);
}

static void GetWords(unsigned int alScope, const tString &asName, int alIdx, unsigned int alField, void *apData, size_t alBytes)
{
	unsigned int vWords[16];
	ohpl_gs6_var_get(alScope, U(asName), asName.size(), alIdx, alField, vWords);
	memcpy(apData, vWords, alBytes);
}

static void SetField(unsigned int alScope, const tString &asName, int alIdx, const tString &x)
{
	ohpl_gs6_var_set(alScope, U(asName), asName.size(), alIdx, kOhplGs6FieldString, nullptr, U(x), x.size());
}
static void SetField(unsigned int alScope, const tString &asName, int alIdx, bool x)
{
	unsigned int w = x;
	SetWords(alScope, asName, alIdx, kOhplGs6FieldBool, &w, 4);
}
static void SetField(unsigned int alScope, const tString &asName, int alIdx, int x) { SetWords(alScope, asName, alIdx, kOhplGs6FieldInt, &x, 4); }
static void SetField(unsigned int alScope, const tString &asName, int alIdx, float x) { SetWords(alScope, asName, alIdx, kOhplGs6FieldFloat, &x, 4); }
static void SetField(unsigned int alScope, const tString &asName, int alIdx, const cVector2f &x) { SetWords(alScope, asName, alIdx, kOhplGs6FieldVector2f, x.v, 8); }
static void SetField(unsigned int alScope, const tString &asName, int alIdx, const cVector3f &x) { SetWords(alScope, asName, alIdx, kOhplGs6FieldVector3f, x.v, 12); }
static void SetField(unsigned int alScope, const tString &asName, int alIdx, const cMatrixf &x) { SetWords(alScope, asName, alIdx, kOhplGs6FieldMatrix, x.v, 64); }
static void SetField(unsigned int alScope, const tString &asName, int alIdx, const cColor &x) { SetWords(alScope, asName, alIdx, kOhplGs6FieldColor, x.v, 16); }
static void SetField(unsigned int alScope, const tString &asName, int alIdx, const cSomaID &x)
{
	int v[3] = {x.mA, x.mB, x.mC};
	SetWords(alScope, asName, alIdx, kOhplGs6FieldId, v, 12);
}

template <class T> static T GetField(unsigned int alScope, const tString &asName, int alIdx);
template <> tString GetField<tString>(unsigned int alScope, const tString &asName, int alIdx)
{
	return Call8([&](OhplGs6Sink8 s, void *c) { ohpl_gs6_var_get_str(alScope, U(asName), asName.size(), alIdx, s, c); });
}
template <> bool GetField<bool>(unsigned int alScope, const tString &asName, int alIdx)
{
	unsigned int w;
	GetWords(alScope, asName, alIdx, kOhplGs6FieldBool, &w, 4);
	return w != 0;
}
template <> int GetField<int>(unsigned int alScope, const tString &asName, int alIdx)
{
	int v;
	GetWords(alScope, asName, alIdx, kOhplGs6FieldInt, &v, 4);
	return v;
}
template <> float GetField<float>(unsigned int alScope, const tString &asName, int alIdx)
{
	float v;
	GetWords(alScope, asName, alIdx, kOhplGs6FieldFloat, &v, 4);
	return v;
}
template <> cVector2f GetField<cVector2f>(unsigned int alScope, const tString &asName, int alIdx)
{
	cVector2f v;
	GetWords(alScope, asName, alIdx, kOhplGs6FieldVector2f, v.v, 8);
	return v;
}
template <> cVector3f GetField<cVector3f>(unsigned int alScope, const tString &asName, int alIdx)
{
	cVector3f v;
	GetWords(alScope, asName, alIdx, kOhplGs6FieldVector3f, v.v, 12);
	return v;
}
template <> cMatrixf GetField<cMatrixf>(unsigned int alScope, const tString &asName, int alIdx)
{
	cMatrixf m = cMatrixf::Identity;
	GetWords(alScope, asName, alIdx, kOhplGs6FieldMatrix, m.v, 64);
	return m;
}
template <> cColor GetField<cColor>(unsigned int alScope, const tString &asName, int alIdx)
{
	cColor c(0, 0);
	GetWords(alScope, asName, alIdx, kOhplGs6FieldColor, c.v, 16);
	return c;
}
template <> cSomaID GetField<cSomaID>(unsigned int alScope, const tString &asName, int alIdx)
{
	int v[3];
	GetWords(alScope, asName, alIdx, kOhplGs6FieldId, v, 12);
	cSomaID id;
	id.mA = (uint8_t)v[0];
	id.mB = v[1];
	id.mC = v[2];
	return id;
}

static const tString gsNoName;

std::string gsSomaExecOutput;
int glSomaUnderwaterUsers = 0;
bool gbSomaUnderwaterEffects = false;
const tString &SomaCurrentLanguage();

tString SomaSerializeGlobalVars()
{
	return Call8([&](OhplGs6Sink8 s, void *c) { ohpl_gs6_serialize_vars(s, c); });
}

void SomaDeserializeGlobalVars(const tString &asData)
{
	ohpl_gs6_deserialize_vars(U(asData), asData.size());
}

tWString SomaParseString(const tWString &asText);

static const char *EndLine(const tString &s) { return s.empty() || s.back() != '\n' ? "\n" : ""; }

static bool IsClassOrDerived(asITypeInfo *apType, const tString &asClass)
{
	for (; apType; apType = apType->GetBaseType())
		if (asClass == apType->GetName())
			return true;
	return false;
}

static std::vector<cSomaLuxScriptable *> FindScriptables(const tString &asObject)
{
	std::vector<cSomaLuxScriptable *> vRet;
	const std::vector<cSomaLuxScriptable *> &vAll = cSomaLuxScriptable::GetAll();
	if (asObject.find('*') != tString::npos)
	{
		for (cSomaLuxScriptable *p : vAll)
			if (p->msScriptName.empty() == false && cString::MatchesWildcard(asObject, p->msScriptName))
				vRet.push_back(p);
		return vRet;
	}
	// msScriptName is set after construction, so a miss falls back to a scan and drops the index
	static std::unordered_multimap<tString, cSomaLuxScriptable *> mapByName;
	static uint64_t lVersion = ~0ull;
	if (lVersion != cSomaLuxScriptable::GetAllVersion())
	{
		mapByName.clear();
		for (cSomaLuxScriptable *p : vAll)
			if (p->msScriptName.empty() == false)
				mapByName.emplace(p->msScriptName, p);
		lVersion = cSomaLuxScriptable::GetAllVersion();
	}
	auto range = mapByName.equal_range(asObject);
	for (auto it = range.first; it != range.second; ++it)
		if (it->second->msScriptName == asObject)
			vRet.push_back(it->second);
	if (vRet.empty())
	{
		for (cSomaLuxScriptable *p : vAll)
			if (p->msScriptName == asObject)
				vRet.push_back(p);
		if (vRet.empty() == false)
			lVersion = ~0ull;
	}
	return vRet;
}

bool SomaRunGlobalFunc(const tString &asObject, const tString &asClass, const tString &asFunc)
{
	bool bFound = false;
	for (cSomaLuxScriptable *p : FindScriptables(asObject))
	{
		asIScriptObject *pScript = p->GetScript();
		if (pScript == NULL)
			continue;
		if (asClass != "" && IsClassOrDerived(pScript->GetObjectType(), asClass) == false)
			continue;
		if (p->Call("void " + asFunc + "()"))
			bFound = true;
	}
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap && pMap->GetScript() && cString::MatchesWildcard(asObject, pMap->GetName()) &&
		(asClass == "" || IsClassOrDerived(pMap->GetScript()->GetObjectType(), asClass)))
		bFound = cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + asFunc + "()") || bFound;
	return bFound;
}

// cGlobalScriptFuncs::FAKE_*: secret codes, keys measured on the official game
static void ShowSecretCode(const char *apKey, tString asX)
{
	if (ohpl_gs6_secret_code(U(apKey), U(asX), asX.size()))
		SomaRunGlobalFunc("DescriptionHandler", "", "_Global_Add");
}

#define SOMA_GLOBAL_TYPE(NAME, ASTYPE, CTYPE)                                                                                                                  \
	SOMA_FUNC(e, "void cScript_SetGlobalVar" NAME "(const tString &in asName, " ASTYPE ")", +[](const tString &n, CTYPE x) { SetField(kOhplGs6ScopeVar, n, 0, x); }); \
	SOMA_FUNC(e, "void cScript_SetGlobalArg" NAME "(int alIdx, " ASTYPE ")", +[](int i, CTYPE x) { SetField(kOhplGs6ScopeArg, gsNoName, i, x); });                  \
	SOMA_FUNC(e, "void cScript_SetGlobalReturn" NAME "(" ASTYPE ")", +[](CTYPE x) { SetField(kOhplGs6ScopeReturn, gsNoName, 0, x); });

// Official natives that set the global args, run an entity script's _Global_ function and return
// the global return value (SomaGlobalFuncsTable.cpp)
static void ForwardToEntityScript(asIScriptGeneric *g)
{
	const cSomaGlobalFunc *pFunc = (const cSomaGlobalFunc *)g->GetFunction()->GetUserData(kSomaForwardUserData);
	asIScriptEngine *pEngine = g->GetEngine();
	auto TypeName = [pEngine](int alTypeId) -> std::string {
		asITypeInfo *t = pEngine->GetTypeInfoById(alTypeId);
		return t ? t->GetName() : "";
	};
	for (asUINT i = 1; i < g->GetArgCount(); ++i)
	{
		int lTypeId = g->GetArgTypeId(i);
		int lArg = (int)i - 1;
		std::string sType = TypeName(lTypeId);
		if (lTypeId == asTYPEID_BOOL)
			SetField(kOhplGs6ScopeArg, gsNoName, lArg, g->GetArgByte(i) != 0);
		else if (lTypeId == asTYPEID_FLOAT)
			SetField(kOhplGs6ScopeArg, gsNoName, lArg, g->GetArgFloat(i));
		else if (lTypeId <= asTYPEID_DOUBLE || (pEngine->GetTypeInfoById(lTypeId) && (pEngine->GetTypeInfoById(lTypeId)->GetFlags() & asOBJ_ENUM)))
			SetField(kOhplGs6ScopeArg, gsNoName, lArg, (int)g->GetArgDWord(i));
		else if (sType == "tString")
			SetField(kOhplGs6ScopeArg, gsNoName, lArg, *(tString *)g->GetArgObject(i));
		else if (sType == "cVector3f")
			SetField(kOhplGs6ScopeArg, gsNoName, lArg, *(cVector3f *)g->GetArgObject(i));
		else if (sType == "cVector2f")
			SetField(kOhplGs6ScopeArg, gsNoName, lArg, *(cVector2f *)g->GetArgObject(i));
		else if (sType == "cColor")
			SetField(kOhplGs6ScopeArg, gsNoName, lArg, *(cColor *)g->GetArgObject(i));
		else if (sType == "tID")
			SetField(kOhplGs6ScopeArg, gsNoName, lArg, *(cSomaID *)g->GetArgObject(i));
	}
	ohpl_gs6_reset_return();
	SomaRunGlobalFunc(*(tString *)g->GetArgObject(0), pFunc->mpClass, pFunc->mpFunc);
	int lRet = g->GetFunction()->GetReturnTypeId();
	std::string sRet = TypeName(lRet);
	if (lRet == asTYPEID_VOID)
		return;
	if (lRet == asTYPEID_BOOL)
		g->SetReturnByte(GetField<bool>(kOhplGs6ScopeReturn, gsNoName, 0));
	else if (lRet == asTYPEID_FLOAT)
		g->SetReturnFloat(GetField<float>(kOhplGs6ScopeReturn, gsNoName, 0));
	else if (lRet <= asTYPEID_DOUBLE || (pEngine->GetTypeInfoById(lRet) && (pEngine->GetTypeInfoById(lRet)->GetFlags() & asOBJ_ENUM)))
		g->SetReturnDWord((asDWORD)GetField<int>(kOhplGs6ScopeReturn, gsNoName, 0));
	else if (sRet == "tString")
		new (g->GetAddressOfReturnLocation()) tString(GetField<tString>(kOhplGs6ScopeReturn, gsNoName, 0));
	else if (sRet == "cVector3f")
		new (g->GetAddressOfReturnLocation()) cVector3f(GetField<cVector3f>(kOhplGs6ScopeReturn, gsNoName, 0));
	else if (sRet == "cVector2f")
		new (g->GetAddressOfReturnLocation()) cVector2f(GetField<cVector2f>(kOhplGs6ScopeReturn, gsNoName, 0));
}

// Natives that need engine objects: resources, languages, the world
void SomaRegisterGlobalEngineBlock(asIScriptEngine *e)
{
	typedef const tString &S;
	SOMA_FUNC(e, "const tString &cLux_GetCurrentLanguage()", +[]() -> const tString & { return SomaCurrentLanguage(); });
	SOMA_FUNC(e, "tString cLux_GetDefaultGameLanguage()", +[]() { return tString("english.lang"); });
	SOMA_FUNC(e, "iEyeTracker@ cInput_GetEyeTracker()", +[]() -> void * { return NULL; });
	SOMA_FUNC(e, "const tWString& cLux_Translate(const tString &in asCat, const tString &in asEntry)",
			  +[](S c, S n) -> const tWString & { return gpSomaBase->mpEngine->GetResources()->Translate(c, n); });
	// cLanguageFile::TranslatePlatformSpecific: entry + platform suffix, else the plain entry
	SOMA_FUNC(e, "const tWString& cLux_TranslatePlatformSpecific(const tString &in asCat, const tString &in asEntry)",
			  +[](S c, S n) -> const tWString & { return gpSomaBase->mpEngine->GetResources()->Translate(c, n); });
	SOMA_FUNC(e, "bool cLux_HasTranslation(const tString &in asCat, const tString &in asEntry)",
			  +[](S c, S n) {
				  cLanguageFile *pLang = gpSomaBase->mpEngine->GetResources()->GetLanguageFile();
				  if (pLang == NULL)
					  return false;
				  return pLang->HasEntry(c, n);
			  });
	SOMA_FUNC(e, "tWString cLux_ParseString(const tWString&in asInput)", +[](const tWString &s) { return SomaParseString(s); });
	SOMA_FUNC(e, "cMaterial@ cResources_CreateMaterial(const tString&in asName)",
			  +[](S n) { return gpSomaBase->mpEngine->GetResources()->GetMaterialManager()->CreateMaterial(cString::SetFileExt(n, "mat")); });
	SOMA_FUNC(e, "void cResources_DestroyMaterial(cMaterial @apMaterial)",
			  +[](cMaterial *m) { if (m) gpSomaBase->mpEngine->GetResources()->GetMaterialManager()->Destroy(m); });
	SOMA_FUNC(e, "cMesh@ cResources_CreateMesh(const tString&in asName)",
			  +[](S n) {
				  cResources *pRes = gpSomaBase->mpEngine->GetResources();
				  tString sName = n;
				  // shipped .msh is the official binary format; load its source model
				  if (cString::GetFileExt(n) == "msh")
					  for (const char *sExt : {"dae", "fbx"})
						  if (pRes->GetFileSearcher()->GetFilePath(cString::SetFileExt(n, sExt)) != _W("")) {
							  sName = cString::SetFileExt(n, sExt);
							  break;
						  }
				  return pRes->GetMeshManager()->CreateMesh(sName);
			  });
	SOMA_FUNC(e, "void cResources_DestroyMesh(cMesh@ apMesh)",
			  +[](cMesh *m) { if (m) gpSomaBase->mpEngine->GetResources()->GetMeshManager()->Destroy(m); });
	SOMA_FUNC(e, "iTexture@ cResources_CreateTexture2D(const tString&in asName, bool abUseMipMaps)",
			  +[](S n, bool mip) { return gpSomaBase->mpEngine->GetResources()->GetTextureManager()->Create2D(n, mip); });
	SOMA_FUNC(e, "iTexture@ cResources_CreateTextureCubeMap(const tString&in asName, bool abUseMipMaps)",
			  +[](S n, bool mip) { return gpSomaBase->mpEngine->GetResources()->GetTextureManager()->CreateCubeMap(n, mip); });
	SOMA_METHOD(e, "cMeshEntity", "void SetDiffuseColorMul(const cColor&in aColor)", +[](cMeshEntity *m, const cColor &c) { m->SetColorMul(c); });
	SOMA_METHOD_NEW(e, "cMeshEntity", "const cColor& GetDiffuseColorMul()", +[](cMeshEntity *m) -> const cColor & {
		static const cColor gWhite(1, 1);
		return m->GetSubMeshEntityNum() > 0 ? m->GetSubMeshEntity(0)->GetColorMul() : gWhite;
	});
	SOMA_METHOD(e, "cMeshEntity", "int GetSubMeshEntityIndex(const tString &in asName)", +[](cMeshEntity *m, const tString &asName) -> int {
		for (int i = 0; i < m->GetSubMeshEntityNum(); ++i)
			if (m->GetSubMeshEntity(i)->GetName() == asName) return i;
		return -1;
	});
	SOMA_METHOD(e, "cWorld", "void GetSubMeshEntityInArea(array<cSubMeshEntity@> &inout avObjects, const cVector3f&in avMin, const cVector3f&in avMax)",
				+[](cWorld *w, CScriptArray &a, const cVector3f &vMin, const cVector3f &vMax) {
					for (cMeshEntityIterator it : {w->GetStaticMeshEntityIterator(), w->GetDynamicMeshEntityIterator()})
						while (it.HasNext())
						{
							cMeshEntity *pMesh = it.Next();
							for (int i = 0; i < pMesh->GetSubMeshEntityNum(); ++i)
							{
								cSubMeshEntity *pSub = pMesh->GetSubMeshEntity(i);
								cBoundingVolume *pBV = pSub->GetBoundingVolume();
								if (cMath::CheckAABBIntersection(pBV->GetMin(), pBV->GetMax(), vMin, vMax))
									a.InsertLast(&pSub);
							}
						}
				});
}

void RegisterSomaScriptGlobalNatives(asIScriptEngine *e)
{
	for (int i = 0; i < glSomaGlobalFuncsNum; ++i)
	{
		const cSomaGlobalFunc &f = gvSomaGlobalFuncs[i];
		std::string sDecl = cSomaScriptApi::NativeDecl(NULL, f.mpDecl);
		if (e->GetGlobalFunctionByDecl(sDecl.c_str()))
			continue;
		int r = e->RegisterGlobalFunction(sDecl.c_str(), asFUNCTION(ForwardToEntityScript), asCALL_GENERIC);
		if (r >= 0)
			e->GetFunctionById(r)->SetUserData((void *)&f, kSomaForwardUserData);
		else
			Warning("SOMA script: could not register forwarding function %s (%d)\n", f.mpDecl, r);
	}

	SOMA_FUNC(e, "void SlideDoor_SetClosed(const tString& in asName, bool abClosed, bool abInstant = false)", +[](const tString &n, bool c, bool i) {
		SetField(kOhplGs6ScopeArg, gsNoName, 0, c ? 0.0f : 1.0f);
		SetField(kOhplGs6ScopeArg, gsNoName, 1, i);
		SomaRunGlobalFunc(n, "cScrPropSlideDoor", "_Global_SetOpenAmount");
	});
	SOMA_FUNC(e, "bool cScript_RunGlobalFunc(const tString&in asObjectName, const tString&in asClassName, const tString&in asFuncName)", (SomaRunGlobalFunc));

	SOMA_GLOBAL_TYPE("String", "const tString &in asVar", const tString &)
	SOMA_GLOBAL_TYPE("Bool", "bool abX", bool)
	SOMA_GLOBAL_TYPE("Int", "int alX", int)
	SOMA_GLOBAL_TYPE("Float", "float afX", float)
	SOMA_GLOBAL_TYPE("Vector2f", "const cVector2f&in avX", const cVector2f &)
	SOMA_GLOBAL_TYPE("Vector3f", "const cVector3f&in avX", const cVector3f &)
	SOMA_GLOBAL_TYPE("Matrix", "const cMatrixf&in a_mtxX", const cMatrixf &)
	SOMA_GLOBAL_TYPE("Color", "const cColor&in aX", const cColor &)
	SOMA_GLOBAL_TYPE("ID", "tID alX", cSomaID)

	SOMA_FUNC(e, "tString cScript_GetGlobalVarString(const tString &in asName)", +[](const tString &n) { return GetField<tString>(kOhplGs6ScopeVar, n, 0); });
	SOMA_FUNC(e, "bool cScript_GetGlobalVarBool(const tString &in asName)", +[](const tString &n) { return GetField<bool>(kOhplGs6ScopeVar, n, 0); });
	SOMA_FUNC(e, "int cScript_GetGlobalVarInt(const tString &in asName)", +[](const tString &n) { return GetField<int>(kOhplGs6ScopeVar, n, 0); });
	SOMA_FUNC(e, "float cScript_GetGlobalVarFloat(const tString &in asName)", +[](const tString &n) { return GetField<float>(kOhplGs6ScopeVar, n, 0); });
	SOMA_FUNC(e, "cVector2f cScript_GetGlobalVarVector2f(const tString &in asName)", +[](const tString &n) { return GetField<cVector2f>(kOhplGs6ScopeVar, n, 0); });
	SOMA_FUNC(e, "cVector3f cScript_GetGlobalVarVector3f(const tString &in asName)", +[](const tString &n) { return GetField<cVector3f>(kOhplGs6ScopeVar, n, 0); });
	SOMA_FUNC(e, "cMatrixf cScript_GetGlobalVarMatrix(const tString &in asName)", +[](const tString &n) { return GetField<cMatrixf>(kOhplGs6ScopeVar, n, 0); });
	SOMA_FUNC(e, "cColor cScript_GetGlobalVarColor(const tString &in asName)", +[](const tString &n) { return GetField<cColor>(kOhplGs6ScopeVar, n, 0); });
	SOMA_FUNC(e, "tID cScript_GetGlobalVarID(const tString &in asName)", +[](const tString &n) { return GetField<cSomaID>(kOhplGs6ScopeVar, n, 0); });

	SOMA_FUNC(e, "tString cScript_GetGlobalArgString(int alIdx)", +[](int i) { return GetField<tString>(kOhplGs6ScopeArg, gsNoName, i); });
	SOMA_FUNC(e, "bool cScript_GetGlobalArgBool(int alIdx)", +[](int i) { return GetField<bool>(kOhplGs6ScopeArg, gsNoName, i); });
	SOMA_FUNC(e, "int cScript_GetGlobalArgInt(int alIdx)", +[](int i) { return GetField<int>(kOhplGs6ScopeArg, gsNoName, i); });
	SOMA_FUNC(e, "float cScript_GetGlobalArgFloat(int alIdx)", +[](int i) { return GetField<float>(kOhplGs6ScopeArg, gsNoName, i); });
	SOMA_FUNC(e, "cVector2f cScript_GetGlobalArgVector2f(int alIdx)", +[](int i) { return GetField<cVector2f>(kOhplGs6ScopeArg, gsNoName, i); });
	SOMA_FUNC(e, "cVector3f cScript_GetGlobalArgVector3f(int alIdx)", +[](int i) { return GetField<cVector3f>(kOhplGs6ScopeArg, gsNoName, i); });
	SOMA_FUNC(e, "cMatrixf cScript_GetGlobalArgMatrix(int alIdx)", +[](int i) { return GetField<cMatrixf>(kOhplGs6ScopeArg, gsNoName, i); });
	SOMA_FUNC(e, "cColor cScript_GetGlobalArgColor(int alIdx)", +[](int i) { return GetField<cColor>(kOhplGs6ScopeArg, gsNoName, i); });
	SOMA_FUNC(e, "tID cScript_GetGlobalArgID(int alIdx)", +[](int i) { return GetField<cSomaID>(kOhplGs6ScopeArg, gsNoName, i); });

	SOMA_FUNC(e, "void cLux_DropPageFocus(const tString&in asX)", +[](const tString &x) { ShowSecretCode("\x1e\x28\x56", x); });
	SOMA_FUNC(e, "void cLux_PreloadWebpage(const tString &in asX)", +[](const tString &x) { ShowSecretCode("\x53\x58\x15", x); });
	SOMA_FUNC(e, "void cLux_SetAreaOffline(int alX)", +[](int x) { ShowSecretCode("\x40\x48\x21", tString(3, (char)x)); });
	SOMA_FUNC(e, "void cLux_RegisterCollisionRadius(int alX)", +[](int x) { ShowSecretCode("\x62\x71\x60", tString(3, (char)x)); });
	SOMA_FUNC(e, "void cLux_ResetShudderEffects(int alX)", +[](int x) { ShowSecretCode("\x4e\x41\x53", tString(3, (char)x)); });

	SOMA_FUNC(e, "const tString& cScript_GetGlobalReturnString()", +[]() -> const tString & {
		static tString sReturn;
		sReturn = GetField<tString>(kOhplGs6ScopeReturn, gsNoName, 0);
		return sReturn;
	});
	SOMA_FUNC(e, "bool cScript_GetGlobalReturnBool()", +[]() { return GetField<bool>(kOhplGs6ScopeReturn, gsNoName, 0); });
	SOMA_FUNC(e, "int cScript_GetGlobalReturnInt()", +[]() { return GetField<int>(kOhplGs6ScopeReturn, gsNoName, 0); });
	SOMA_FUNC(e, "float cScript_GetGlobalReturnFloat()", +[]() { return GetField<float>(kOhplGs6ScopeReturn, gsNoName, 0); });
	SOMA_FUNC(e, "cVector2f cScript_GetGlobalReturnVector2f()", +[]() { return GetField<cVector2f>(kOhplGs6ScopeReturn, gsNoName, 0); });
	SOMA_FUNC(e, "cVector3f cScript_GetGlobalReturnVector3f()", +[]() { return GetField<cVector3f>(kOhplGs6ScopeReturn, gsNoName, 0); });
	SOMA_FUNC(e, "cMatrixf cScript_GetGlobalReturnMatrix()", +[]() { return GetField<cMatrixf>(kOhplGs6ScopeReturn, gsNoName, 0); });
	SOMA_FUNC(e, "cColor cScript_GetGlobalReturnColor()", +[]() { return GetField<cColor>(kOhplGs6ScopeReturn, gsNoName, 0); });
	SOMA_FUNC(e, "tID cScript_GetGlobalReturnID()", +[]() { return GetField<cSomaID>(kOhplGs6ScopeReturn, gsNoName, 0); });

	typedef const tString &S;
	SomaRegisterGlobalEngineBlock(e);
	SOMA_FUNC(e, "bool cLux_ScriptDebugOn()", +[]() { return false; });
	SOMA_FUNC(e, "bool cLux_DebugModeOn()", +[]() { return false; });
	SOMA_FUNC(e, "bool cLux_GetGodModeActivated()", +[]() { static bool b = getenv("OPENHPL_SOMA_GODMODE") != NULL; return b; });
	SOMA_FUNC(e, "bool cLux_GetUnderwaterEffectsActive()", +[]() { return gbSomaUnderwaterEffects; });
	// ponytail: state only, no underwater audio/voice filters yet
	SOMA_FUNC(e, "void cLux_SetUnderwaterEffectsActive(bool abX, bool abUseStartAndEndEffects)", +[](bool b, bool) { gbSomaUnderwaterEffects = b; });
	SOMA_FUNC(e, "void cLux_IncUnderwaterEffectUserCount()", +[]() { ++glSomaUnderwaterUsers; });
	SOMA_FUNC(e, "void cLux_DecUnderwaterEffectUserCount()", +[]() { --glSomaUnderwaterUsers; });
	SOMA_FUNC(e, "bool cLux_HasUnderwaterEffectUsers()", +[]() { return glSomaUnderwaterUsers > 0; });
	SOMA_FUNC(e, "void __print(const tString&in asText)", +[](S s) { gsSomaExecOutput += s + "\n"; });
	SOMA_FUNC(e, "void Log(const tString&in asString)", +[](S s) { Log("%s%s", s.c_str(), EndLine(s)); });
	SOMA_FUNC(e, "void Warning(const tString&in asString)", +[](S s) { Warning("%s%s", s.c_str(), EndLine(s)); });
	SOMA_FUNC(e, "void Error(const tString&in asString)", +[](S s) { Error("%s%s", s.c_str(), EndLine(s)); });
	SOMA_FUNC(e, "void cLux_AddDebugMessage(const tString&in asText, bool abCheckForDuplicates)", +[](S s, bool) { Log("SOMA debug: %s\n", s.c_str()); });
	SOMA_FUNC(e, "void cLux_AddDebugMessage(const tString&in asText)", +[](S s) { Log("SOMA debug: %s\n", s.c_str()); });
	SOMA_FUNC(e, "void cLux_AddTodoMessage(const tString&in asText, bool abCheckForDuplicates)", +[](S s, bool) { Log("SOMA todo: %s\n", s.c_str()); });
	SOMA_FUNC(e, "void cLux_AddTodoMessage(const tString&in asText)", +[](S s) { Log("SOMA todo: %s\n", s.c_str()); });
}
