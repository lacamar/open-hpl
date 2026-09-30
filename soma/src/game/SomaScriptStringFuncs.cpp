#include "SomaScriptNatives.h"
#include "SomaScriptBind.h"

#include "impl/scriptarray.h"
#include "math/MathTypes.h"
#include "system/String.h"

#include <cstdio>

using namespace hpl;

static tString FloatToString(float f, int alDecimals, bool abRemoveZeros)
{
	char sBuf[64];
	if (alDecimals < 0)
		snprintf(sBuf, sizeof(sBuf), "%g", f);
	else
		snprintf(sBuf, sizeof(sBuf), "%.*f", alDecimals, f);
	tString s = sBuf;
	if (abRemoveZeros && s.find('.') != tString::npos)
	{
		while (!s.empty() && s.back() == '0')
			s.pop_back();
		if (!s.empty() && s.back() == '.')
			s.pop_back();
	}
	return s;
}

static int FindChar(const tString &s, char c, bool abLast)
{
	size_t p = abLast ? s.find_last_of(c) : s.find_first_of(c);
	return p == tString::npos ? -1 : (int)p;
}

template <class S> static int FindStr(const S &s, const S &sub, bool abLast)
{
	size_t p = abLast ? s.rfind(sub) : s.find(sub);
	return p == S::npos ? -1 : (int)p;
}

static void SplitInto(const tString &asData, const tString &asSep, std::vector<tString> &aOut)
{
	// Empty separators: cString::GetStringVec's defaults
	const tString &sSep = asSep.empty() ? tString(" \n\r\t,") : asSep;
	tString sCur;
	for (char c : asData)
	{
		if (sSep.find(c) != tString::npos)
		{
			if (!sCur.empty())
				aOut.push_back(sCur);
			sCur.clear();
		}
		else
			sCur += c;
	}
	if (!sCur.empty())
		aOut.push_back(sCur);
}

void RegisterSomaScriptStringNatives(asIScriptEngine *e)
{
	SOMA_FUNC(e, "tWString cString_To16Char(const tString &in asString)", +[](const tString &s) { return cString::To16Char(s); });
	SOMA_FUNC(e, "tString cString_To8Char(const tWString &in awsString)", +[](const tWString &s) { return cString::To8Char(s); });
	SOMA_FUNC(e, "tString cString_ToString(float afX, int alNumOfDecimals=-1, bool abRemoveZeros=false)", (FloatToString));
	SOMA_FUNC(e, "tString cString_ToString(int alX, int alPaddingZeros)", +[](int x, int p) { char b[32]; snprintf(b, sizeof(b), "%0*d", p, x); return tString(b); });
	SOMA_FUNC(e, "tWString cString_ToStringW(float afX, int alNumOfDecimals=-1, bool abRemoveZeros=false)",
			  +[](float f, int d, bool r) { return cString::To16Char(FloatToString(f, d, r)); });
	SOMA_FUNC(e, "tWString cString_ToStringW(int alX, int alPaddingZeros)", +[](int x, int p) { char b[32]; snprintf(b, sizeof(b), "%0*d", p, x); return cString::To16Char(b); });
	SOMA_FUNC(e, "float cString_ToFloat(const tString&in asStr, float afDefault)", +[](const tString &s, float d) { return cString::ToFloat(s.c_str(), d); });
	SOMA_FUNC(e, "int cString_ToInt(const tString&in asStr, int alDefault)", +[](const tString &s, int d) { return cString::ToInt(s.c_str(), d); });
	SOMA_FUNC(e, "bool cString_ToBool(const tString&in asStr, bool abDefault)", +[](const tString &s, bool d) { return cString::ToBool(s.c_str(), d); });
	SOMA_FUNC(e, "cVector2f cString_ToVector2f(const tString&in asStr, const cVector2f&in avDefault)", +[](const tString &s, const cVector2f &d) { return cString::ToVector2f(s.c_str(), d); });
	SOMA_FUNC(e, "cVector3f cString_ToVector3f(const tString&in asStr, const cVector3f&in avDefault)", +[](const tString &s, const cVector3f &d) { return cString::ToVector3f(s.c_str(), d); });
	SOMA_FUNC(e, "cColor cString_ToColor(const tString&in asStr, const cColor&in aDefault)", +[](const tString &s, const cColor &d) { return cString::ToColor(s.c_str(), d); });
	SOMA_FUNC(e, "tString cString_Sub(const tString&in asString,int alStart, int alCount=-1)", +[](const tString &s, int a, int c) { return cString::Sub(s, a, c); });
	SOMA_FUNC(e, "tWString cString_SubW(const tWString&in asString,int alStart, int alCount=-1)", +[](const tWString &s, int a, int c) { return cString::SubW(s, a, c); });
	SOMA_FUNC(e, "tString cString_ToLowerCase(const tString&in aString)", +[](const tString &s) { return cString::ToLowerCase(s); });
	SOMA_FUNC(e, "tWString cString_ToLowerCaseW(const tWString&in aString)", +[](const tWString &s) { return cString::ToLowerCaseW(s); });
	SOMA_FUNC(e, "tString cString_ToUpperCase(const tString&in aString)", +[](const tString &s) { return cString::ToUpperCase(s); });
	SOMA_FUNC(e, "tString cString_ReplaceCharTo(const tString&in aString, const tString&in asOldChar,const tString&in asNewChar)",
			  +[](const tString &s, const tString &a, const tString &b) { return cString::ReplaceCharTo(s, a, b); });
	SOMA_FUNC(e, "tString cString_ReplaceStringTo(const tString&in aString, const tString&in asOldString,const tString&in asNewString)",
			  +[](const tString &s, const tString &a, const tString &b) { return cString::ReplaceStringTo(s, a, b); });
	SOMA_FUNC(e, "tString cString_GetLastChar(const tString&in aString)", +[](const tString &s) { return s.empty() ? tString() : s.substr(s.size() - 1); });
	SOMA_FUNC(e, "int cString_GetFirstCharPos(const tString&in aString, int8 alChar)", +[](const tString &s, int8_t c) { return FindChar(s, (char)c, false); });
	SOMA_FUNC(e, "int cString_GetLastCharPos(const tString&in aString, int8 alChar)", +[](const tString &s, int8_t c) { return FindChar(s, (char)c, true); });
	SOMA_FUNC(e, "int cString_GetFirstStringPos(const tString&in aString, const tString&in aChar)", +[](const tString &s, const tString &c) { return FindStr(s, c, false); });
	SOMA_FUNC(e, "int cString_GetLastStringPos(const tString&in aString, const tString&in aChar)", +[](const tString &s, const tString &c) { return FindStr(s, c, true); });
	SOMA_FUNC(e, "int cString_GetFirstStringPosW(const tWString&in aString, const tWString&in aChar)", +[](const tWString &s, const tWString &c) { return FindStr(s, c, false); });
	SOMA_FUNC(e, "int cString_GetLastStringPosW(const tWString&in aString, const tWString&in aChar)", +[](const tWString &s, const tWString &c) { return FindStr(s, c, true); });
	SOMA_FUNC(e, "int cString_CountCharsInString(const tString&in aString, const tString&in aChar)",
			  +[](const tString &s, const tString &c) { int n = 0; for (size_t p = s.find(c); !c.empty() && p != tString::npos; p = s.find(c, p + c.size())) ++n; return n; });
	SOMA_FUNC(e, "uint cString_GetHash(const tString&in asStr)", +[](const tString &s) { unsigned int h = 5381; for (char c : s) h = h * 33 + (unsigned char)c; return h; });
	SOMA_FUNC(e, "tString cString_GetNumericSuffix(const tString&in asStr)", +[](const tString &s) { size_t p = s.find_last_not_of("0123456789"); return p == tString::npos ? s : s.substr(p + 1); });
	SOMA_FUNC(e, "int cString_GetNumericSuffixInt(const tString&in aString, int alDefault=0)",
			  +[](const tString &s, int d) { size_t p = s.find_last_not_of("0123456789"); tString n = p == tString::npos ? s : s.substr(p + 1); return n.empty() ? d : atoi(n.c_str()); });
	SOMA_FUNC(e, "void cString_GetStringVec(const tString&in asData, array<tString> &inout avOutStrings, const tString&in asSepp)",
			  +[](const tString &asData, CScriptArray &aOut, const tString &asSep) {
				  std::vector<tString> v;
				  SplitInto(asData, asSep, v);
				  for (tString &s : v)
					  aOut.InsertLast(&s);
			  });
	SOMA_FUNC(e, "void cString_GetFloatVec(const tString&in asData, array<float> &inout avOutFloats, const tString&in asSepp)",
			  +[](const tString &asData, CScriptArray &aOut, const tString &asSep) {
				  std::vector<tString> v;
				  SplitInto(asData, asSep, v);
				  for (tString &s : v) { float f = (float)atof(s.c_str()); aOut.InsertLast(&f); }
			  });
	SOMA_FUNC(e, "void cString_GetIntVec(const tString&in asData, array<int> &inout avOutInts, const tString&in asSepp)",
			  +[](const tString &asData, CScriptArray &aOut, const tString &asSep) {
				  std::vector<tString> v;
				  SplitInto(asData, asSep, v);
				  for (tString &s : v) { int l = atoi(s.c_str()); aOut.InsertLast(&l); }
			  });
	SOMA_FUNC(e, "tString cString_GetFileExt(const tString&in aString)", +[](const tString &s) { return cString::GetFileExt(s); });
	SOMA_FUNC(e, "tString cString_SetFileExt(const tString&in aString,const tString&in aExt)", +[](const tString &s, const tString &x) { return cString::SetFileExt(s, x); });
	SOMA_FUNC(e, "tString cString_GetFileName(const tString&in aString)", +[](const tString &s) { return cString::GetFileName(s); });
	SOMA_FUNC(e, "tString cString_GetFilePath(const tString&in aString)", +[](const tString &s) { return cString::GetFilePath(s); });
	SOMA_FUNC(e, "tString cString_AddSlashAtEnd(const tString&in asPath)", +[](const tString &s) { return cString::AddSlashAtEnd(s); });
	SOMA_FUNC(e, "tString cString_RemoveSlashAtEnd(const tString&in asPath)", +[](const tString &s) { return cString::RemoveSlashAtEnd(s); });
}
