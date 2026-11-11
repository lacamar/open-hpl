#include "SomaScriptStrings.h"
#include "SomaScriptNatives.h"
#include "SomaScriptBind.h"
#include "RustSomaScript.h"

#include "impl/scriptarray.h"
#include "math/MathTypes.h"
#include "resources/LanguageFile.h"
#include "system/String.h"

#include <cstdio>

using namespace hpl;
using namespace OhplGs6;

static const unsigned int *W(const tWString &s) { return (const unsigned int *)s.data(); }

static tString FloatToString(float f, int alDecimals, bool abRemoveZeros)
{
	return Call8([&](OhplGs6Sink8 s, void *c) { ohpl_gs6_float_to_string(f, alDecimals, abRemoveZeros, s, c); });
}

static tString PaddedInt(int x, int p)
{
	return Call8([&](OhplGs6Sink8 s, void *c) { ohpl_gs6_int_padded(x, p, s, c); });
}

static tString Suffix(const tString &s)
{
	return Call8([&](OhplGs6Sink8 k, void *c) { ohpl_gs6_numeric_suffix(U(s), s.size(), k, c); });
}

static tWString SuffixW(const tWString &s)
{
	return Call32([&](OhplGs6Sink32 k, void *c) { ohpl_gs6_numeric_suffix_w(W(s), s.size(), k, c); });
}

void RegisterSomaScriptStringNatives(asIScriptEngine *e)
{
	RegisterSomaScriptStringConcats(e);
	SOMA_FUNC(e, "tString cString_ToString(float afX, int alNumOfDecimals=-1, bool abRemoveZeros=false)", (FloatToString));
	SOMA_FUNC(e, "tString cString_ToString(int alX, int alPaddingZeros)", +[](int x, int p) { return PaddedInt(x, p); });
	SOMA_FUNC(e, "tWString cString_ToStringW(float afX, int alNumOfDecimals=-1, bool abRemoveZeros=false)",
			  +[](float f, int d, bool r) { return cString::To16Char(FloatToString(f, d, r)); });
	SOMA_FUNC(e, "tWString cString_ToStringW(int alX, int alPaddingZeros)", +[](int x, int p) { return cString::To16Char(PaddedInt(x, p)); });
	SOMA_FUNC(e, "tWString cString_ReplaceStringToW(const tWString&in aString, const tWString&in asOldString,const tWString&in asNewString)",
			  +[](const tWString &s, const tWString &o, const tWString &n) {
				  return Call32([&](OhplGs6Sink32 k, void *c) { ohpl_gs6_replace_w(W(s), s.size(), W(o), o.size(), W(n), n.size(), k, c); });
			  });
	SOMA_FUNC(e, "float cString_ToFloat(const tString&in asStr, float afDefault)", +[](const tString &s, float d) { return cString::ToFloat(s.c_str(), d); });
	SOMA_FUNC(e, "int cString_ToInt(const tString&in asStr, int alDefault)", +[](const tString &s, int d) { return cString::ToInt(s.c_str(), d); });
	SOMA_FUNC(e, "bool cString_ToBool(const tString&in asStr, bool abDefault)", +[](const tString &s, bool d) { return cString::ToBool(s.c_str(), d); });
	SOMA_FUNC(e, "cVector2f cString_ToVector2f(const tString&in asStr, const cVector2f&in avDefault)", +[](const tString &s, const cVector2f &d) { return cString::ToVector2f(s.c_str(), d); });
	SOMA_FUNC(e, "cVector3f cString_ToVector3f(const tString&in asStr, const cVector3f&in avDefault)", +[](const tString &s, const cVector3f &d) { return cString::ToVector3f(s.c_str(), d); });
	SOMA_FUNC(e, "cColor cString_ToColor(const tString&in asStr, const cColor&in aDefault)", +[](const tString &s, const cColor &d) { return cString::ToColor(s.c_str(), d); });
	SOMA_FUNC(e, "tString cString_GetLastChar(const tString&in aString)",
			  +[](const tString &s) { return Call8([&](OhplGs6Sink8 k, void *c) { ohpl_gs6_last_char(U(s), s.size(), k, c); }); });
	SOMA_FUNC(e, "int cString_GetFirstCharPos(const tString&in aString, int8 alChar)", +[](const tString &s, int8_t c) { return ohpl_gs6_find_char(U(s), s.size(), (unsigned char)c, false); });
	SOMA_FUNC(e, "int cString_GetLastCharPos(const tString&in aString, int8 alChar)", +[](const tString &s, int8_t c) { return ohpl_gs6_find_char(U(s), s.size(), (unsigned char)c, true); });
	SOMA_FUNC(e, "int cString_GetFirstStringPos(const tString&in aString, const tString&in aChar)",
			  +[](const tString &s, const tString &c) { return ohpl_gs6_find_str(U(s), s.size(), U(c), c.size(), false); });
	SOMA_FUNC(e, "int cString_GetLastStringPos(const tString&in aString, const tString&in aChar)",
			  +[](const tString &s, const tString &c) { return ohpl_gs6_find_str(U(s), s.size(), U(c), c.size(), true); });
	SOMA_FUNC(e, "int cString_GetFirstStringPosW(const tWString&in aString, const tWString&in aChar)",
			  +[](const tWString &s, const tWString &c) { return ohpl_gs6_find_str_w(W(s), s.size(), W(c), c.size(), false); });
	SOMA_FUNC(e, "int cString_GetLastStringPosW(const tWString&in aString, const tWString&in aChar)",
			  +[](const tWString &s, const tWString &c) { return ohpl_gs6_find_str_w(W(s), s.size(), W(c), c.size(), true); });
	SOMA_FUNC(e, "int cString_CountCharsInString(const tString&in aString, const tString&in aChar)",
			  +[](const tString &s, const tString &c) { return ohpl_gs6_count_chars(U(s), s.size(), U(c), c.size()); });
	SOMA_FUNC(e, "tWString cString_GetDecodedString(const tString&in asStr)", +[](const tString &s) { return GetDecodedString(s); });
	SOMA_FUNC(e, "uint cString_GetHash(const tString&in asStr)", +[](const tString &s) { return ohpl_gs6_hash(U(s), s.size()); });
	SOMA_FUNC(e, "tString cString_GetNumericSuffix(const tString&in asStr)", +[](const tString &s) { return Suffix(s); });
	SOMA_FUNC(e, "tWString cString_GetNumericSuffixW(const tWString&in asStr)", +[](const tWString &s) { return SuffixW(s); });
	SOMA_FUNC(e, "int cString_GetNumericSuffixInt(const tString&in aString, int alDefault=0)",
			  +[](const tString &s, int d) { return ohpl_gs6_numeric_suffix_int(U(s), s.size(), d); });
	SOMA_FUNC(e, "int cString_GetNumericSuffixIntW(const tWString&in aString, int alDefault=0)",
			  +[](const tWString &s, int d) { return ohpl_gs6_numeric_suffix_int_w(W(s), s.size(), d); });
	SOMA_FUNC(e, "void cString_GetStringVecW(const tWString&in asData, array<tWString> &inout avOutStrings, const tWString&in asSepp)",
			  +[](const tWString &asData, CScriptArray &aOut, const tWString &asSep) {
				  std::vector<tWString> v;
				  ohpl_gs6_split_w(W(asData), asData.size(), W(asSep), asSep.size(), AppendWString, &v);
				  for (tWString &s : v)
					  aOut.InsertLast(&s);
			  });
	SOMA_FUNC(e, "void cString_GetStringVec(const tString&in asData, array<tString> &inout avOutStrings, const tString&in asSepp)",
			  +[](const tString &asData, CScriptArray &aOut, const tString &asSep) {
				  std::vector<tString> v;
				  ohpl_gs6_split(U(asData), asData.size(), U(asSep), asSep.size(), AppendString, &v);
				  for (tString &s : v)
					  aOut.InsertLast(&s);
			  });
	SOMA_FUNC(e, "void cString_GetFloatVec(const tString&in asData, array<float> &inout avOutFloats, const tString&in asSepp)",
			  +[](const tString &asData, CScriptArray &aOut, const tString &asSep) {
				  ohpl_gs6_float_vec(U(asData), asData.size(), U(asSep), asSep.size(), +[](void *c, float f) { ((CScriptArray *)c)->InsertLast(&f); }, &aOut);
			  });
	SOMA_FUNC(e, "void cString_GetIntVec(const tString&in asData, array<int> &inout avOutInts, const tString&in asSepp)",
			  +[](const tString &asData, CScriptArray &aOut, const tString &asSep) {
				  ohpl_gs6_int_vec(U(asData), asData.size(), U(asSep), asSep.size(), +[](void *c, int l) { ((CScriptArray *)c)->InsertLast(&l); }, &aOut);
			  });
	SOMA_FUNC(e, "tString String_SecondsToClockDisplay(float afSeconds, bool abShowHours, bool abShowMinutes, bool abShowSeconds, bool abShowHundredths)",
			  +[](float s, bool bH, bool bM, bool bS, bool bC) {
				  return Call8([&](OhplGs6Sink8 k, void *c) { ohpl_gs6_clock(s, bH, bM, bS, bC, k, c); });
			  });
}
