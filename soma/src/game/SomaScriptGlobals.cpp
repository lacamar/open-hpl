#include "SomaLuxEntity.h"
#include "SomaScriptBind.h"
#include "SomaLux.h"
#include "SomaSave.h"
#include "SomaGlobalFuncsTable.h"
#include "SomaBase.h"
#include "SomaScriptNatives.h"
#include "SomaScriptRuntime.h"

#include <map>
#include <limits>
#include <sstream>

struct cSomaVariant
{
	tString s;
	bool b = false;
	int i = 0;
	float f = 0;
	cVector2f v2 = 0;
	cVector3f v3 = 0;
	float v4[4] = {0, 0, 0, 0};
	cMatrixf m = cMatrixf::Identity;
	cColor c = cColor(0, 0);
	cSomaID id;
};

struct cSomaVector4 { float x, y, z, w; };

static std::map<tString, cSomaVariant> gmapVars;
static std::map<int, cSomaVariant> gmapArgs;
static cSomaVariant gReturn;

std::string gsSomaExecOutput;
const tString &SomaCurrentLanguage();

tString SomaSerializeGlobalVars()
{
	std::ostringstream out;
	out.precision(9);
	for (auto &it : gmapVars)
	{
		const cSomaVariant &v = it.second;
		out << "var\t" << it.first.size() << ":" << it.first << v.s.size() << ":" << v.s << " " << v.b << " " << v.i << " " << v.f << " "
			<< v.v2.x << " " << v.v2.y << " " << v.v3.x << " " << v.v3.y << " " << v.v3.z << " " << v.v4[0] << " " << v.v4[1] << " "
			<< v.v4[2] << " " << v.v4[3] << " " << v.c.r << " " << v.c.g << " " << v.c.b << " " << v.c.a << " " << (int)v.id.mA << " "
			<< v.id.mB << " " << v.id.mC;
		for (int i = 0; i < 16; ++i)
			out << " " << v.m.v[i];
		out << "\n";
	}
	return out.str();
}

void SomaDeserializeGlobalVars(const tString &asData)
{
	gmapVars.clear();
	std::istringstream in(asData);
	std::string sKey;
	while (in >> sKey)
	{
		if (sKey != "var")
		{
			in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			continue;
		}
		auto ReadStr = [&in]() {
			size_t lLen = 0;
			in >> lLen;
			in.get();
			std::string s(lLen, '\0');
			in.read(&s[0], lLen);
			return s;
		};
		in.get();
		tString sName = ReadStr();
		cSomaVariant &v = gmapVars[sName];
		v.s = ReadStr();
		int lA = 0;
		in >> v.b >> v.i >> v.f >> v.v2.x >> v.v2.y >> v.v3.x >> v.v3.y >> v.v3.z >> v.v4[0] >> v.v4[1] >> v.v4[2] >> v.v4[3] >> v.c.r >> v.c.g >> v.c.b >> v.c.a >>
			lA >> v.id.mB >> v.id.mC;
		v.id.mA = (uint8_t)lA;
		for (int i = 0; i < 16; ++i)
			in >> v.m.v[i];
	}
}

// "$Input{Action}" -> the key bound to the action
tWString SomaParseString(const tWString &asText)
{
	tWString sOut;
	size_t lPos = 0;
	while (true)
	{
		size_t lStart = asText.find(_W("$Input{"), lPos);
		size_t lEnd = lStart == tWString::npos ? tWString::npos : asText.find(_W('}'), lStart);
		if (lEnd == tWString::npos)
			break;
		sOut += asText.substr(lPos, lStart - lPos);
		cAction *pAction = gpSomaBase->mpEngine->GetInput()->GetAction(cString::To8Char(asText.substr(lStart + 7, lEnd - lStart - 7)));
		tString sKey = pAction && pAction->GetSubActionNum() > 0 ? pAction->GetSubAction(0)->GetInputName() : "?";
		sOut += _W("[") + cString::To16Char(sKey) + _W("]");
		lPos = lEnd + 1;
	}
	return sOut + asText.substr(lPos);
}

static const char *EndLine(const tString &s) { return s.empty() || s.back() != '\n' ? "\n" : ""; }

static bool IsClassOrDerived(asITypeInfo *apType, const tString &asClass)
{
	for (; apType; apType = apType->GetBaseType())
		if (asClass == apType->GetName())
			return true;
	return false;
}

static bool RunGlobalFunc(const tString &asObject, const tString &asClass, const tString &asFunc)
{
	bool bFound = false;
	std::vector<cSomaLuxScriptable *> vAll = cSomaLuxScriptable::GetAll();
	for (cSomaLuxScriptable *p : vAll)
	{
		asIScriptObject *pScript = p->GetScript();
		if (pScript == NULL || p->msScriptName.empty() || SomaWildcardMatch(asObject, p->msScriptName) == false)
			continue;
		if (asClass != "" && IsClassOrDerived(pScript->GetObjectType(), asClass) == false)
			continue;
		if (p->Call("void " + asFunc + "()"))
			bFound = true;
	}
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap && pMap->GetScript() && SomaWildcardMatch(asObject, pMap->GetName()) &&
		(asClass == "" || IsClassOrDerived(pMap->GetScript()->GetObjectType(), asClass)))
		bFound = cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + asFunc + "()") || bFound;
	return bFound;
}

#define SOMA_GLOBAL_TYPE(NAME, ASTYPE, CTYPE, FIELD)                                                                                                         \
	SOMA_FUNC(e, "void cScript_SetGlobalVar" NAME "(const tString &in asName, " ASTYPE ")", +[](const tString &n, CTYPE x) { gmapVars[n].FIELD = x; });    \
	SOMA_FUNC(e, "void cScript_SetGlobalArg" NAME "(int alIdx, " ASTYPE ")", +[](int i, CTYPE x) { gmapArgs[i].FIELD = x; });                              \
	SOMA_FUNC(e, "void cScript_SetGlobalReturn" NAME "(" ASTYPE ")", +[](CTYPE x) { gReturn.FIELD = x; });

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
		cSomaVariant &v = gmapArgs[(int)i - 1];
		std::string sType = TypeName(lTypeId);
		if (lTypeId == asTYPEID_BOOL)
			v.b = g->GetArgByte(i) != 0;
		else if (lTypeId == asTYPEID_FLOAT)
			v.f = g->GetArgFloat(i);
		else if (lTypeId <= asTYPEID_DOUBLE || (pEngine->GetTypeInfoById(lTypeId) && (pEngine->GetTypeInfoById(lTypeId)->GetFlags() & asOBJ_ENUM)))
			v.i = (int)g->GetArgDWord(i);
		else if (sType == "tString")
			v.s = *(tString *)g->GetArgObject(i);
		else if (sType == "cVector3f")
			v.v3 = *(cVector3f *)g->GetArgObject(i);
		else if (sType == "cVector2f")
			v.v2 = *(cVector2f *)g->GetArgObject(i);
		else if (sType == "cColor")
			v.c = *(cColor *)g->GetArgObject(i);
		else if (sType == "tID")
			v.id = *(cSomaID *)g->GetArgObject(i);
	}
	gReturn = cSomaVariant();
	RunGlobalFunc(*(tString *)g->GetArgObject(0), pFunc->mpClass, pFunc->mpFunc);
	int lRet = g->GetFunction()->GetReturnTypeId();
	std::string sRet = TypeName(lRet);
	if (lRet == asTYPEID_VOID)
		return;
	if (lRet == asTYPEID_BOOL)
		g->SetReturnByte(gReturn.b);
	else if (lRet == asTYPEID_FLOAT)
		g->SetReturnFloat(gReturn.f);
	else if (lRet <= asTYPEID_DOUBLE || (pEngine->GetTypeInfoById(lRet) && (pEngine->GetTypeInfoById(lRet)->GetFlags() & asOBJ_ENUM)))
		g->SetReturnDWord((asDWORD)gReturn.i);
	else if (sRet == "tString")
		new (g->GetAddressOfReturnLocation()) tString(gReturn.s);
	else if (sRet == "cVector3f")
		new (g->GetAddressOfReturnLocation()) cVector3f(gReturn.v3);
	else if (sRet == "cVector2f")
		new (g->GetAddressOfReturnLocation()) cVector2f(gReturn.v2);
}

void RegisterSomaScriptGlobalNatives(asIScriptEngine *e)
{
	for (int i = 0; i < glSomaGlobalFuncsNum; ++i)
	{
		const cSomaGlobalFunc &f = gvSomaGlobalFuncs[i];
		if (e->GetGlobalFunctionByDecl(f.mpDecl))
			continue;
		int r = e->RegisterGlobalFunction(f.mpDecl, asFUNCTION(ForwardToEntityScript), asCALL_GENERIC);
		if (r >= 0)
			e->GetFunctionById(r)->SetUserData((void *)&f, kSomaForwardUserData);
		else
			Warning("SOMA script: could not register forwarding function %s (%d)\n", f.mpDecl, r);
	}

	SOMA_FUNC(e, "void SlideDoor_SetClosed(const tString& in asName, bool abClosed, bool abInstant = false)", +[](const tString &n, bool c, bool i) {
		gmapArgs[0].f = c ? 0.0f : 1.0f;
		gmapArgs[1].b = i;
		RunGlobalFunc(n, "cScrPropSlideDoor", "_Global_SetOpenAmount");
	});
	SOMA_FUNC(e, "bool cScript_RunGlobalFunc(const tString&in asObjectName, const tString&in asClassName, const tString&in asFuncName)", (RunGlobalFunc));

	SOMA_GLOBAL_TYPE("String", "const tString &in asVar", const tString &, s)
	SOMA_GLOBAL_TYPE("Bool", "bool abX", bool, b)
	SOMA_GLOBAL_TYPE("Int", "int alX", int, i)
	SOMA_GLOBAL_TYPE("Float", "float afX", float, f)
	SOMA_GLOBAL_TYPE("Vector2f", "const cVector2f&in avX", const cVector2f &, v2)
	SOMA_GLOBAL_TYPE("Vector3f", "const cVector3f&in avX", const cVector3f &, v3)
	SOMA_GLOBAL_TYPE("Matrix", "const cMatrixf&in a_mtxX", const cMatrixf &, m)
	SOMA_GLOBAL_TYPE("Color", "const cColor&in aX", const cColor &, c)
	SOMA_GLOBAL_TYPE("ID", "tID alX", cSomaID, id)

	SOMA_FUNC(e, "tString cScript_GetGlobalVarString(const tString &in asName)", +[](const tString &n) { return gmapVars[n].s; });
	SOMA_FUNC(e, "bool cScript_GetGlobalVarBool(const tString &in asName)", +[](const tString &n) { return gmapVars[n].b; });
	SOMA_FUNC(e, "int cScript_GetGlobalVarInt(const tString &in asName)", +[](const tString &n) { return gmapVars[n].i; });
	SOMA_FUNC(e, "float cScript_GetGlobalVarFloat(const tString &in asName)", +[](const tString &n) { return gmapVars[n].f; });
	SOMA_FUNC(e, "cVector2f cScript_GetGlobalVarVector2f(const tString &in asName)", +[](const tString &n) { return gmapVars[n].v2; });
	SOMA_FUNC(e, "cVector3f cScript_GetGlobalVarVector3f(const tString &in asName)", +[](const tString &n) { return gmapVars[n].v3; });
	SOMA_FUNC(e, "cMatrixf cScript_GetGlobalVarMatrix(const tString &in asName)", +[](const tString &n) { return gmapVars[n].m; });
	SOMA_FUNC(e, "cColor cScript_GetGlobalVarColor(const tString &in asName)", +[](const tString &n) { return gmapVars[n].c; });
	SOMA_FUNC(e, "tID cScript_GetGlobalVarID(const tString &in asName)", +[](const tString &n) { return gmapVars[n].id; });

	SOMA_FUNC(e, "tString cScript_GetGlobalArgString(int alIdx)", +[](int i) { return gmapArgs[i].s; });
	SOMA_FUNC(e, "bool cScript_GetGlobalArgBool(int alIdx)", +[](int i) { return gmapArgs[i].b; });
	SOMA_FUNC(e, "int cScript_GetGlobalArgInt(int alIdx)", +[](int i) { return gmapArgs[i].i; });
	SOMA_FUNC(e, "float cScript_GetGlobalArgFloat(int alIdx)", +[](int i) { return gmapArgs[i].f; });
	SOMA_FUNC(e, "cVector2f cScript_GetGlobalArgVector2f(int alIdx)", +[](int i) { return gmapArgs[i].v2; });
	SOMA_FUNC(e, "cVector3f cScript_GetGlobalArgVector3f(int alIdx)", +[](int i) { return gmapArgs[i].v3; });
	SOMA_FUNC(e, "cMatrixf cScript_GetGlobalArgMatrix(int alIdx)", +[](int i) { return gmapArgs[i].m; });
	SOMA_FUNC(e, "cColor cScript_GetGlobalArgColor(int alIdx)", +[](int i) { return gmapArgs[i].c; });
	SOMA_FUNC(e, "tID cScript_GetGlobalArgID(int alIdx)", +[](int i) { return gmapArgs[i].id; });

	SOMA_FUNC(e, "const tString& cScript_GetGlobalReturnString()", +[]() -> const tString & { return gReturn.s; });
	SOMA_FUNC(e, "bool cScript_GetGlobalReturnBool()", +[]() { return gReturn.b; });
	SOMA_FUNC(e, "int cScript_GetGlobalReturnInt()", +[]() { return gReturn.i; });
	SOMA_FUNC(e, "float cScript_GetGlobalReturnFloat()", +[]() { return gReturn.f; });
	SOMA_FUNC(e, "cVector2f cScript_GetGlobalReturnVector2f()", +[]() { return gReturn.v2; });
	SOMA_FUNC(e, "cVector3f cScript_GetGlobalReturnVector3f()", +[]() { return gReturn.v3; });
	SOMA_FUNC(e, "cMatrixf cScript_GetGlobalReturnMatrix()", +[]() { return gReturn.m; });
	SOMA_FUNC(e, "cColor cScript_GetGlobalReturnColor()", +[]() { return gReturn.c; });
	SOMA_FUNC(e, "tID cScript_GetGlobalReturnID()", +[]() { return gReturn.id; });

	typedef const tString &S;
	SOMA_FUNC(e, "const tString &cLux_GetCurrentLanguage()", +[]() -> const tString & { return SomaCurrentLanguage(); });
	SOMA_FUNC(e, "tString cLux_GetDefaultGameLanguage()", +[]() { return tString("english.lang"); });
	SOMA_FUNC(e, "iEyeTracker@ cInput_GetEyeTracker()", +[]() -> void * { return NULL; });
	SOMA_FUNC(e, "const tWString& cLux_Translate(const tString &in asCat, const tString &in asEntry)",
			  +[](S c, S n) -> const tWString & { return gpSomaBase->mpEngine->GetResources()->Translate(c, n); });
	SOMA_FUNC(e, "bool cLux_HasTranslation(const tString &in asCat, const tString &in asEntry)",
			  +[](S c, S n) {
				  cLanguageFile *pLang = gpSomaBase->mpEngine->GetResources()->GetLanguageFile();
				  if (pLang == NULL)
					  return false;
				  auto it = pLang->GetCategoryMap()->find(c);
				  return it != pLang->GetCategoryMap()->end() && it->second->m_mapEntries.count(n) > 0;
			  });
	SOMA_FUNC(e, "tWString cLux_ParseString(const tWString&in asInput)", +[](const tWString &s) { return SomaParseString(s); });
	SOMA_FUNC(e, "cMaterial@ cResources_CreateMaterial(const tString&in asName)",
			  +[](S n) { return gpSomaBase->mpEngine->GetResources()->GetMaterialManager()->CreateMaterial(cString::SetFileExt(n, "mat")); });
	SOMA_FUNC(e, "void cResources_DestroyMaterial(cMaterial @apMaterial)",
			  +[](cMaterial *m) { if (m) gpSomaBase->mpEngine->GetResources()->GetMaterialManager()->Destroy(m); });
	SOMA_FUNC(e, "iTexture@ cResources_CreateTexture2D(const tString&in asName, bool abUseMipMaps)",
			  +[](S n, bool mip) { return gpSomaBase->mpEngine->GetResources()->GetTextureManager()->Create2D(n, mip); });
	SOMA_FUNC(e, "bool cLux_ScriptDebugOn()", +[]() { return false; });
	SOMA_FUNC(e, "bool cLux_DebugModeOn()", +[]() { return false; });
	SOMA_FUNC(e, "bool cLux_GetGodModeActivated()", +[]() { return false; });
	SOMA_FUNC(e, "bool cLux_GetUnderwaterEffectsActive()", +[]() { return false; });
	SOMA_FUNC(e, "void __print(const tString&in asText)", +[](S s) { gsSomaExecOutput += s + "\n"; });
	SOMA_FUNC(e, "void Log(const tString&in asString)", +[](S s) { Log("%s%s", s.c_str(), EndLine(s)); });
	SOMA_FUNC(e, "void Warning(const tString&in asString)", +[](S s) { Warning("%s%s", s.c_str(), EndLine(s)); });
	SOMA_FUNC(e, "void Error(const tString&in asString)", +[](S s) { Error("%s%s", s.c_str(), EndLine(s)); });
	SOMA_FUNC(e, "void cLux_AddDebugMessage(const tString&in asText, bool abCheckForDuplicates)", +[](S s, bool) { Log("SOMA debug: %s\n", s.c_str()); });
	SOMA_FUNC(e, "void cLux_AddDebugMessage(const tString&in asText)", +[](S s) { Log("SOMA debug: %s\n", s.c_str()); });
	SOMA_FUNC(e, "void cLux_AddTodoMessage(const tString&in asText, bool abCheckForDuplicates)", +[](S s, bool) { Log("SOMA todo: %s\n", s.c_str()); });
	SOMA_FUNC(e, "void cLux_AddTodoMessage(const tString&in asText)", +[](S s) { Log("SOMA todo: %s\n", s.c_str()); });
}
