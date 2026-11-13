#include "SomaScriptStrings.h"

#include "hpl.h"

#include "RustSomaScript.h"
#include "RustSomaScriptS2.h"

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

static void ConstructWFromNarrow(const std::string &asIn, std::wstring *apMem)
{
	new (apMem) std::wstring(OhplGs6::Call32([&](OhplGs6Sink32 k, void *c) { ohpl_gs6_widen(OhplGs6::U(asIn), asIn.size(), k, c); }));
}

static uint8_t &CharAt(unsigned int alIdx, std::string &s)
{
	static uint8_t lDummy = 0;
	if (alIdx >= s.size())
		return lDummy;
	return *(uint8_t *)&s[alIdx];
}

static uint16_t &WCharAt(unsigned int alIdx, std::wstring &s)
{
	static uint16_t lDummy = 0;
	if (alIdx >= s.size())
		return lDummy;
	return *(uint16_t *)&s[alIdx];
}

static std::string Tos(unsigned int alKind, const float *apF, size_t alF, const int *apI = nullptr, size_t alI = 0)
{
	return OhplGs6::Call8([&](OhplGs6Sink8 s, void *c) { ohpl_gs6_tostr(alKind, apF, alF, apI, alI, s, c); });
}
static std::string ToStr(float f) { return Tos(kOhplGs6ToStrFloat, &f, 1); }
static std::string ToStr(int l) { return Tos(kOhplGs6ToStrInt, nullptr, 0, &l, 1); }
static std::string ToStr(unsigned int l)
{
	int v = (int)l;
	return Tos(kOhplGs6ToStrUint, nullptr, 0, &v, 1);
}
static std::string ToStr(bool b)
{
	int v = b;
	return Tos(kOhplGs6ToStrBool, nullptr, 0, &v, 1);
}
static std::string ToStr(const hpl::cVector2f &v)
{
	float a[] = {v.x, v.y};
	return Tos(kOhplGs6ToStrFloats, a, 2);
}
static std::string ToStr(const hpl::cVector2l &v)
{
	int a[] = {v.x, v.y};
	return Tos(kOhplGs6ToStrInts, nullptr, 0, a, 2);
}
static std::string ToStr(const hpl::cVector3f &v)
{
	float a[] = {v.x, v.y, v.z};
	return Tos(kOhplGs6ToStrFloats, a, 3);
}
static std::string ToStr(const hpl::cVector3l &v)
{
	int a[] = {v.x, v.y, v.z};
	return Tos(kOhplGs6ToStrInts, nullptr, 0, a, 3);
}
struct cVector4Layout
{
	float x, y, z, w;
};
static std::string ToStr(const cVector4Layout &v)
{
	float a[] = {v.x, v.y, v.z, v.w};
	return Tos(kOhplGs6ToStrFloats, a, 4);
}
static std::string ToStr(const hpl::cColor &c)
{
	float a[] = {c.r, c.g, c.b, c.a};
	return Tos(kOhplGs6ToStrFloats, a, 4);
}
static std::string ToStr(const hpl::cQuaternion &q)
{
	float a[] = {q.v.x, q.v.y, q.v.z, q.w};
	return Tos(kOhplGs6ToStrFloats, a, 4);
}
static std::string ToStr(const hpl::cRect2f &r)
{
	float a[] = {r.x, r.y, r.w, r.h};
	return Tos(kOhplGs6ToStrRectF, a, 4);
}
static std::string ToStr(const hpl::cRect2l &r)
{
	int a[] = {r.x, r.y, r.w, r.h};
	return Tos(kOhplGs6ToStrRectL, nullptr, 0, a, 4);
}
static std::string ToStr(const hpl::cPlanef &p)
{
	float a[] = {p.a, p.b, p.c, p.d};
	return Tos(kOhplGs6ToStrPlane, a, 4);
}
static std::string ToStr(hpl::cDate d) { return "(" + d.ToString() + ")"; }
static std::string ToStr(const hpl::cMatrixf &m)
{
	float a[16];
	for (int i = 0; i < 16; ++i)
		a[i] = m.m[i / 4][i % 4];
	return Tos(kOhplGs6ToStrMatrix, a, 16);
}
struct cSomaIDLayout
{
	uint8_t mA;
	int32_t mB, mC;
};
static std::string ToStr(const cSomaIDLayout &aID)
{
	int a[] = {aID.mA, aID.mB, aID.mC};
	return Tos(kOhplGs6ToStrId, nullptr, 0, a, 3);
}

template <class T> static std::string &AssignT(const T &v, std::string &s) { s = ToStr(v); return s; }
template <class T> static std::string &AddAssignT(const T &v, std::string &s) { s += ToStr(v); return s; }
template <class T> static std::string AddStrT(const std::string &s, const T &v) { return s + ToStr(v); }
template <class T> static std::string AddTStr(const T &v, const std::string &s) { return ToStr(v) + s; }

static std::string ToStr(const std::string &s) { return s; }
static std::wstring ToWStr(const std::string &s)
{
	return OhplGs6::Call32([&](OhplGs6Sink32 k, void *c) { ohpl_gs6_widen(OhplGs6::U(s), s.size(), k, c); });
}

template <class T> static std::wstring &WAssignT(const T &v, std::wstring &s) { return s = ToWStr(ToStr(v)); }
template <class T> static std::wstring &WAddAssignT(const T &v, std::wstring &s) { return s += ToWStr(ToStr(v)); }
template <class T> static std::wstring WAddStrT(const std::wstring &s, const T &v) { return s + ToWStr(ToStr(v)); }
template <class T> static std::wstring WAddTStr(const T &v, const std::wstring &s) { return ToWStr(ToStr(v)) + s; }

template <class T> static void RegisterWConcat(asIScriptEngine *e, const char *apType)
{
	std::string t = apType;
	int r;
	r = e->RegisterObjectMethod("tWString", ("tWString &opAssign(const " + t + " &in)").c_str(), asFUNCTION(WAssignT<T>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = e->RegisterObjectMethod("tWString", ("tWString &opAddAssign(const " + t + " &in)").c_str(), asFUNCTION(WAddAssignT<T>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = e->RegisterObjectMethod("tWString", ("tWString opAdd(const " + t + " &in) const").c_str(), asFUNCTION(WAddStrT<T>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = e->RegisterObjectMethod("tWString", ("tWString opAdd_r(const " + t + " &in) const").c_str(), asFUNCTION(WAddTStr<T>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	(void)r;
}

template <class T> static void RegisterConcat(asIScriptEngine *e, const char *apType)
{
	RegisterWConcat<T>(e, apType);
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
	if (OhplS2Rust())
	{
		OhplS2InstallHost();
		ohpl_s2_register_strings(apEngine);
		return;
	}
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

	r = apEngine->RegisterObjectBehaviour("tWString", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Construct<std::wstring>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectBehaviour("tWString", asBEHAVE_CONSTRUCT, "void f(const tWString &in)", asFUNCTION(CopyConstruct<std::wstring>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectBehaviour("tWString", asBEHAVE_CONSTRUCT, "void f(const tString &in)", asFUNCTION(ConstructWFromNarrow), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectBehaviour("tWString", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Destruct<std::wstring>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "tWString &opAssign(const tWString &in)", asMETHODPR(std::wstring, operator=, (const std::wstring &), std::wstring &), asCALL_THISCALL); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "tWString &opAddAssign(const tWString &in)", asMETHODPR(std::wstring, operator+=, (const std::wstring &), std::wstring &), asCALL_THISCALL); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "bool opEquals(const tWString &in) const", asFUNCTIONPR(std::operator==, (const std::wstring &, const std::wstring &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "int opCmp(const tWString &in) const", asFUNCTION(Cmp<std::wstring>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "tWString opAdd(const tWString &in) const", asFUNCTION(Add<std::wstring>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "uint64 length() const", asFUNCTION(Length<std::wstring>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "uint16 &opIndex(uint)", asFUNCTION(WCharAt), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "const uint16 &opIndex(uint) const", asFUNCTION(WCharAt), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("tWString", "void resize(uint64)", asFUNCTION(Resize<std::wstring>), asCALL_CDECL_OBJLAST); assert(r >= 0);
	RegisterWConcat<std::string>(apEngine, "tString");
	RegisterConcat<float>(apEngine, "float");
	RegisterConcat<int>(apEngine, "int");
	RegisterConcat<unsigned int>(apEngine, "uint");
	RegisterConcat<bool>(apEngine, "bool");
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
static void ArrInsert(asUINT alIdx, void *apValue, CScriptArray *apArr) { apArr->InsertAt(alIdx, apValue); }
static void ArrErase(asUINT alIdx, CScriptArray *apArr) { apArr->RemoveAt(alIdx); }

void RegisterSomaScriptArrayExtras(asIScriptEngine *apEngine)
{
	if (OhplS2Rust())
	{
		ohpl_s2_register_array_extras(apEngine);
		return;
	}
	int r;
	r = apEngine->RegisterObjectMethod("array<T>", "void push_back(const T&in)", asFUNCTION(ArrPushBack), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "void push_front(const T&in)", asFUNCTION(ArrPushFront), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "void insertBack(const T&in)", asFUNCTION(ArrInsertBack), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "void pop_back()", asFUNCTION(ArrPopBack), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "void pop_front()", asFUNCTION(ArrPopFront), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "void insert(uint, const T&in)", asFUNCTION(ArrInsert), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "void erase(uint)", asFUNCTION(ArrErase), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "uint size() const", asFUNCTION(ArrSize), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = apEngine->RegisterObjectMethod("array<T>", "bool empty() const", asFUNCTION(ArrEmpty), asCALL_CDECL_OBJLAST); assert(r >= 0);
	(void)r;
}
