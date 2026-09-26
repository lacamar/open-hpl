// cScript_*: script-to-script calls by object name with typed global args, vars and return value
#include "SomaLuxEntity.h"
#include "SomaScriptBind.h"
#include "SomaScriptNatives.h"
#include "SomaScriptRuntime.h"

#include <map>

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

static bool RunGlobalFunc(const tString &asObject, const tString &asClass, const tString &asFunc)
{
	bool bFound = false;
	std::vector<cSomaLuxScriptable *> vAll = cSomaLuxScriptable::GetAll();
	for (cSomaLuxScriptable *p : vAll)
	{
		asIScriptObject *pScript = p->GetScript();
		if (pScript == NULL || p->msScriptName.empty() || SomaWildcardMatch(asObject, p->msScriptName) == false)
			continue;
		if (asClass != "" && asClass != pScript->GetObjectType()->GetName())
			continue;
		if (p->Call("void " + asFunc + "()"))
			bFound = true;
	}
	return bFound;
}

#define SOMA_GLOBAL_TYPE(NAME, ASTYPE, CTYPE, FIELD)                                                                                                         \
	SOMA_FUNC(e, "void cScript_SetGlobalVar" NAME "(const tString &in asName, " ASTYPE ")", +[](const tString &n, CTYPE x) { gmapVars[n].FIELD = x; });    \
	SOMA_FUNC(e, "void cScript_SetGlobalArg" NAME "(int alIdx, " ASTYPE ")", +[](int i, CTYPE x) { gmapArgs[i].FIELD = x; });                              \
	SOMA_FUNC(e, "void cScript_SetGlobalReturn" NAME "(" ASTYPE ")", +[](CTYPE x) { gReturn.FIELD = x; });

void RegisterSomaScriptGlobalNatives(asIScriptEngine *e)
{
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
}
