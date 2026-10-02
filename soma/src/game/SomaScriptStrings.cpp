#include "SomaScriptStrings.h"

#include "hpl.h"

#include "impl/scriptarray.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <new>
#include <string>

class cStringFactory : public asIStringFactory
{
public:
	const void *GetStringConstant(const char *apData, asUINT alLength)
	{
		std::string s(apData, alLength);
		std::map<std::string, int>::iterator it = mapCache.find(s);
		if (it == mapCache.end())
			it = mapCache.insert(std::make_pair(s, 0)).first;
		++it->second;
		return &it->first;
	}
	int ReleaseStringConstant(const void *apStr)
	{
		std::map<std::string, int>::iterator it = mapCache.find(*(const std::string *)apStr);
		if (it != mapCache.end() && --it->second == 0)
			mapCache.erase(it);
		return 0;
	}
	int GetRawStringData(const void *apStr, char *apData, asUINT *apLength) const
	{
		const std::string &s = *(const std::string *)apStr;
		if (apLength)
			*apLength = (asUINT)s.size();
		if (apData)
			memcpy(apData, s.data(), s.size());
		return 0;
	}

private:
	std::map<std::string, int> mapCache;
};

static cStringFactory gStringFactory;

template <class S> static void Construct(S *apMem) { new (apMem) S(); }
template <class S> static void CopyConstruct(const S &aOther, S *apMem) { new (apMem) S(aOther); }
template <class S> static void Destruct(S *apMem) { apMem->~S(); }
template <class S> static int Cmp(const S &a, const S &b) { return a.compare(b) < 0 ? -1 : (a.compare(b) > 0 ? 1 : 0); }
template <class S> static S Add(const S &a, const S &b) { return a + b; }
template <class S> static asQWORD Length(const S &s) { return s.size(); }
template <class S> static void Resize(asQWORD alSize, S &s) { s.resize((size_t)alSize); }

static void ConstructWFromNarrow(const std::string &asIn, std::wstring *apMem) { new (apMem) std::wstring(asIn.begin(), asIn.end()); }
static std::wstring &AssignWFromNarrow(const std::string &asIn, std::wstring &asOut) { return asOut.assign(asIn.begin(), asIn.end()); }

static uint8_t &CharAt(unsigned int alIdx, std::string &s)
{
	static uint8_t lDummy = 0;
	if (alIdx >= s.size())
		return lDummy;
	return *(uint8_t *)&s[alIdx];
}

static std::string ToStr(float f)
{
	char sBuf[64];
	snprintf(sBuf, sizeof(sBuf), "%f", f);
	return sBuf;
}
static std::string ToStr(int l) { return std::to_string(l); }
static std::string ToStr(unsigned int l) { return std::to_string(l); }
static std::string ToStr(bool b) { return b ? "true" : "false"; }

static std::string Join(std::initializer_list<std::string> avParts)
{
	std::string s = "(";
	for (const std::string &p : avParts)
		s += (s.size() > 1 ? " : " : "") + p;
	return s + ")";
}
static std::string ToStr(const hpl::cVector2f &v) { return Join({ToStr(v.x), ToStr(v.y)}); }
static std::string ToStr(const hpl::cVector2l &v) { return Join({ToStr(v.x), ToStr(v.y)}); }
static std::string ToStr(const hpl::cVector3f &v) { return Join({ToStr(v.x), ToStr(v.y), ToStr(v.z)}); }
static std::string ToStr(const hpl::cVector3l &v) { return Join({ToStr(v.x), ToStr(v.y), ToStr(v.z)}); }
struct cVector4Layout
{
	float x, y, z, w;
};
static std::string ToStr(const cVector4Layout &v) { return Join({ToStr(v.x), ToStr(v.y), ToStr(v.z), ToStr(v.w)}); }
static std::string ToStr(const hpl::cColor &c) { return Join({ToStr(c.r), ToStr(c.g), ToStr(c.b), ToStr(c.a)}); }
static std::string ToStr(const hpl::cQuaternion &q) { return Join({ToStr(q.v.x), ToStr(q.v.y), ToStr(q.v.z), ToStr(q.w)}); }
static std::string ToStr(const hpl::cRect2f &r) { return "(" + Join({ToStr(r.x), ToStr(r.y)}) + Join({ToStr(r.w), ToStr(r.h)}) + ")"; }
static std::string ToStr(const hpl::cRect2l &r) { return "(" + Join({ToStr(r.x), ToStr(r.y)}) + Join({ToStr(r.w), ToStr(r.h)}) + ")"; }
static std::string ToStr(const hpl::cPlanef &p) { return "(" + Join({ToStr(p.a), ToStr(p.b), ToStr(p.c), ToStr(p.d)}) + ")"; }
static std::string ToStr(hpl::cDate d) { return "(" + d.ToString() + ")"; }
static std::string ToStr(const hpl::cMatrixf &m)
{
	std::string s = "(";
	for (int i = 0; i < 4; ++i)
	{
		std::string sRow = Join({ToStr(m.m[i][0]), ToStr(m.m[i][1]), ToStr(m.m[i][2]), ToStr(m.m[i][3])});
		s += (i ? " [" : "[") + sRow.substr(1, sRow.size() - 2) + "]";
	}
	return s + ")";
}
struct cSomaIDLayout
{
	uint8_t mA;
	int32_t mB, mC;
};
static std::string ToStr(const cSomaIDLayout &aID) { return "(" + std::to_string(aID.mA) + " " + std::to_string(aID.mB) + " " + std::to_string(aID.mC) + ")"; }

template <class T> static std::string &AssignT(const T &v, std::string &s) { s = ToStr(v); return s; }
template <class T> static std::string &AddAssignT(const T &v, std::string &s) { s += ToStr(v); return s; }
template <class T> static std::string AddStrT(const std::string &s, const T &v) { return s + ToStr(v); }
template <class T> static std::string AddTStr(const T &v, const std::string &s) { return ToStr(v) + s; }

template <class T> static void RegisterConcat(asIScriptEngine *e, const char *apType)
{
	std::string t = apType;
	int r;
	r = e->RegisterObjectMethod("tString", ("tString &opAssign(const " + t + " &in)").c_str(), asFUNCTION(AssignT<T>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = e->RegisterObjectMethod("tString", ("tString &opAddAssign(const " + t + " &in)").c_str(), asFUNCTION(AddAssignT<T>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = e->RegisterObjectMethod("tString", ("tString opAdd(const " + t + " &in) const").c_str(), asFUNCTION(AddStrT<T>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = e->RegisterObjectMethod("tString", ("tString opAdd_r(const " + t + " &in) const").c_str(), asFUNCTION(AddTStr<T>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	(void)r;
}

void RegisterSomaScriptStringConcats(asIScriptEngine *apEngine)
{
	RegisterConcat<hpl::cVector2f>(apEngine, "cVector2f");
	RegisterConcat<hpl::cVector2l>(apEngine, "cVector2l");
	RegisterConcat<hpl::cVector3f>(apEngine, "cVector3f");
	RegisterConcat<hpl::cVector3l>(apEngine, "cVector3l");
	RegisterConcat<cVector4Layout>(apEngine, "cVector4f");
	RegisterConcat<hpl::cMatrixf>(apEngine, "cMatrixf");
	RegisterConcat<hpl::cQuaternion>(apEngine, "cQuaternion");
	RegisterConcat<hpl::cRect2f>(apEngine, "cRect2f");
	RegisterConcat<hpl::cRect2l>(apEngine, "cRect2l");
	RegisterConcat<hpl::cPlanef>(apEngine, "cPlanef");
	RegisterConcat<hpl::cColor>(apEngine, "cColor");
	RegisterConcat<hpl::cDate>(apEngine, "cDate");
	RegisterConcat<cSomaIDLayout>(apEngine, "tID");
}

void RegisterSomaScriptStrings(asIScriptEngine *apEngine)
{
	int r;
	r = apEngine->RegisterObjectType("tString", sizeof(std::string), asOBJ_VALUE | asGetTypeTraits<std::string>()); assert(r >= 0);
	r = apEngine->RegisterObjectType("tWString", sizeof(std::wstring), asOBJ_VALUE | asGetTypeTraits<std::wstring>()); assert(r >= 0);
	r = apEngine->RegisterStringFactory("tString", &gStringFactory); assert(r >= 0);

	r = apEngine->RegisterObjectBehaviour("tString", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Construct<std::string>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectBehaviour("tString", asBEHAVE_CONSTRUCT, "void f(const tString &in)", asFUNCTION(CopyConstruct<std::string>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectBehaviour("tString", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Destruct<std::string>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tString", "tString &opAssign(const tString &in)", asMETHODPR(std::string, operator=, (const std::string &), std::string &), asCALL_THISCALL); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tString", "tString &opAddAssign(const tString &in)", asMETHODPR(std::string, operator+=, (const std::string &), std::string &), asCALL_THISCALL); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tString", "bool opEquals(const tString &in) const", asFUNCTIONPR(std::operator==, (const std::string &, const std::string &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tString", "int opCmp(const tString &in) const", asFUNCTION(Cmp<std::string>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tString", "tString opAdd(const tString &in) const", asFUNCTION(Add<std::string>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tString", "uint64 length() const", asFUNCTION(Length<std::string>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tString", "void resize(uint64)", asFUNCTION(Resize<std::string>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tString", "uint8 &opIndex(uint)", asFUNCTION(CharAt), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tString", "const uint8 &opIndex(uint) const", asFUNCTION(CharAt), asCALL_CDECL_OBJLAST); assert(r >= 0);
	RegisterConcat<float>(apEngine, "float");
	RegisterConcat<int>(apEngine, "int");
	RegisterConcat<unsigned int>(apEngine, "uint");
	RegisterConcat<bool>(apEngine, "bool");

	r = apEngine->RegisterObjectBehaviour("tWString", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Construct<std::wstring>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectBehaviour("tWString", asBEHAVE_CONSTRUCT, "void f(const tWString &in)", asFUNCTION(CopyConstruct<std::wstring>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectBehaviour("tWString", asBEHAVE_CONSTRUCT, "void f(const tString &in)", asFUNCTION(ConstructWFromNarrow), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectBehaviour("tWString", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Destruct<std::wstring>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "tWString &opAssign(const tWString &in)", asMETHODPR(std::wstring, operator=, (const std::wstring &), std::wstring &), asCALL_THISCALL); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "tWString &opAssign(const tString &in)", asFUNCTION(AssignWFromNarrow), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "tWString &opAddAssign(const tWString &in)", asMETHODPR(std::wstring, operator+=, (const std::wstring &), std::wstring &), asCALL_THISCALL); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "bool opEquals(const tWString &in) const", asFUNCTIONPR(std::operator==, (const std::wstring &, const std::wstring &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "int opCmp(const tWString &in) const", asFUNCTION(Cmp<std::wstring>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "tWString opAdd(const tWString &in) const", asFUNCTION(Add<std::wstring>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "uint64 length() const", asFUNCTION(Length<std::wstring>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "void resize(uint64)", asFUNCTION(Resize<std::wstring>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	(void)r;
}

// SOMA's array<T> also has STL-style names

static void ArrPushBack(void *apValue, CScriptArray *apArr) { apArr->InsertLast(apValue); }
static void ArrPushFront(void *apValue, CScriptArray *apArr) { apArr->InsertAt(0, apValue); }
static void ArrPopBack(CScriptArray *apArr) { if (apArr->GetSize()) apArr->RemoveLast(); }
static void ArrPopFront(CScriptArray *apArr) { if (apArr->GetSize()) apArr->RemoveAt(0); }
static asUINT ArrSize(const CScriptArray *apArr) { return apArr->GetSize(); }
static bool ArrEmpty(const CScriptArray *apArr) { return apArr->IsEmpty(); }
static void ArrInsertBack(void *apValue, CScriptArray *apArr) { apArr->InsertLast(apValue); }

void RegisterSomaScriptArrayExtras(asIScriptEngine *apEngine)
{
	int r;
	r = apEngine->RegisterObjectMethod("array<T>", "void push_back(const T&in)", asFUNCTION(ArrPushBack), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "void push_front(const T&in)", asFUNCTION(ArrPushFront), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "void insertBack(const T&in)", asFUNCTION(ArrInsertBack), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "void pop_back()", asFUNCTION(ArrPopBack), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "void pop_front()", asFUNCTION(ArrPopFront), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "uint size() const", asFUNCTION(ArrSize), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "bool empty() const", asFUNCTION(ArrEmpty), asCALL_CDECL_OBJLAST); assert(r >= 0);
	(void)r;
}
