/*
 * See HpslTranspiler.h for scope notes.
 */

#include "HpslTranspiler.h"

#include <regex>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>

//---------------------------------------------------------------

namespace
{
	struct cHpslParam
	{
		tString msQualifier;	// "in" or "out"
		tString msType;			// already-GLSL-mapped type
		tString msName;
		int mlSemantic;			// -1 if none (no ": N" suffix)
	};

	// Type name mapping. Word-boundary replace, applied to the whole file
	// (declarations and expressions alike - HPSL uses these as constructor
	// calls too, e.g. cVector4f(...), which becomes vec4(...) the same way
	// GLSL expects).
	const std::vector<std::pair<tString, tString> > gvTypeMap = {
		{"cVector4f", "vec4"}, {"cVector3f", "vec3"}, {"cVector2f", "vec2"},
		{"cVector4i", "ivec4"}, {"cVector3i", "ivec3"}, {"cVector2i", "ivec2"},
		{"cMatrixf", "mat4"},
		{"cTexture2D", "sampler2D"}, {"cTextureCube", "samplerCube"},
		{"cTextureRect", "sampler2DRect"},
	};

	// Vertex-input semantic name -> GLSL 120 built-in. Only vtx_vPosition
	// and vtx_vColor are verified (against clear_vtx.hpsl); the rest are
	// unverified guesses by analogy with HPL2's own GLSL shaders' use of
	// the same fixed-function built-ins - see HpslTranspiler.h.
	const std::map<tString, tString> gmapVertexBuiltins = {
		{"vtx_vPosition", "gl_Vertex"},
		{"vtx_vColor", "gl_Color"},
		{"vtx_vNormal", "gl_Normal"},
		{"vtx_vTexCoord0", "gl_MultiTexCoord0"},
		{"vtx_vTexCoord1", "gl_MultiTexCoord1"},
	};

	tString ReplaceTypeNames(const tString& asSrc)
	{
		tString sOut = asSrc;
		for (size_t i = 0; i < gvTypeMap.size(); ++i)
		{
			// \b word-boundary regex - these are all valid C-identifier
			// characters so a boundary match is exactly "not part of a
			// longer identifier", which is what we want.
			std::regex typeRe("\\b" + gvTypeMap[i].first + "\\b");
			sOut = std::regex_replace(sOut, typeRe, gvTypeMap[i].second);
		}
		return sOut;
	}

	tString Trim(const tString& asStr)
	{
		size_t lBegin = asStr.find_first_not_of(" \t\r\n");
		if (lBegin == tString::npos) return "";
		size_t lEnd = asStr.find_last_not_of(" \t\r\n");
		return asStr.substr(lBegin, lEnd - lBegin + 1);
	}

	// Splits a parameter list on top-level commas (none of the expected
	// HPSL param types contain commas themselves, so this is a plain
	// split - no paren-depth tracking needed).
	std::vector<tString> SplitParams(const tString& asParamList)
	{
		std::vector<tString> vOut;
		tString sCurrent;
		for (size_t i = 0; i < asParamList.size(); ++i)
		{
			if (asParamList[i] == ',')
			{
				vOut.push_back(sCurrent);
				sCurrent = "";
			}
			else sCurrent += asParamList[i];
		}
		if (Trim(sCurrent) != "") vOut.push_back(sCurrent);
		return vOut;
	}

	// Parses one already-type-mapped parameter string, e.g.
	// "in vec4 vtx_vPosition" or "out vec4 out_vColor0 : 0".
	bool ParseParam(const tString& asParam, cHpslParam& aParamOut, tString& asErrorOut)
	{
		tString sParam = Trim(asParam);

		aParamOut.mlSemantic = -1;
		size_t lColonPos = sParam.find(':');
		if (lColonPos != tString::npos)
		{
			tString sSemantic = Trim(sParam.substr(lColonPos + 1));
			aParamOut.mlSemantic = cString::ToInt(sSemantic.c_str(), -1);
			sParam = Trim(sParam.substr(0, lColonPos));
		}

		std::regex declRe("^(in|out)\\s+(\\S+)\\s+(\\S+)$");
		std::smatch match;
		if (std::regex_match(sParam, match, declRe) == false)
		{
			asErrorOut = "Couldn't parse parameter '" + asParam + "' (expected '(in|out) TYPE NAME [: N]')";
			return false;
		}

		aParamOut.msQualifier = match[1].str();
		aParamOut.msType = match[2].str();
		aParamOut.msName = match[3].str();
		return true;
	}

	tString ReplaceIdentifier(const tString& asSrc, const tString& asFrom, const tString& asTo)
	{
		std::regex idRe("\\b" + asFrom + "\\b");
		return std::regex_replace(asSrc, idRe, asTo);
	}
}

//---------------------------------------------------------------

bool TranspileHpslToGlsl(const tString& asPreprocessedHpsl, eGpuShaderType aType,
						  tString& asGlslOut, tString& asErrorOut)
{
	tString sSrc = ReplaceTypeNames(asPreprocessedHpsl);

	//////////////////////////////
	// Find "void main(" ... ")" and the "{" ... "}" body that follows it.
	// HPSL's main() has no leading whitespace requirement, so search
	// loosely for "main" then locate the parens/braces by depth-counting
	// rather than assuming exact spacing.
	size_t lMainPos = sSrc.find("main");
	if (lMainPos == tString::npos)
	{
		asErrorOut = "No 'main' function found";
		return false;
	}

	size_t lParamsStart = sSrc.find('(', lMainPos);
	if (lParamsStart == tString::npos)
	{
		asErrorOut = "No '(' found after 'main'";
		return false;
	}

	int lDepth = 1;
	size_t lParamsEnd = lParamsStart + 1;
	for (; lParamsEnd < sSrc.size() && lDepth > 0; ++lParamsEnd)
	{
		if (sSrc[lParamsEnd] == '(') lDepth++;
		else if (sSrc[lParamsEnd] == ')') lDepth--;
	}
	lParamsEnd--; // point at the matching ')'

	tString sParamList = sSrc.substr(lParamsStart + 1, lParamsEnd - lParamsStart - 1);

	size_t lBodyStart = sSrc.find('{', lParamsEnd);
	if (lBodyStart == tString::npos)
	{
		asErrorOut = "No '{' found after main()'s parameter list";
		return false;
	}

	lDepth = 1;
	size_t lBodyEnd = lBodyStart + 1;
	for (; lBodyEnd < sSrc.size() && lDepth > 0; ++lBodyEnd)
	{
		if (sSrc[lBodyEnd] == '{') lDepth++;
		else if (sSrc[lBodyEnd] == '}') lDepth--;
	}
	lBodyEnd--; // point at the matching '}'

	tString sBody = sSrc.substr(lBodyStart + 1, lBodyEnd - lBodyStart - 1);

	// Everything before "main" itself, MINUS the "void" return-type
	// keyword (and the whitespace before it) that HPSL's "void main(...)"
	// declaration always has right before the name - main() is
	// reassembled with its own "void main()" below, so that keyword must
	// not survive into the passed-through header text or it doubles up
	// (e.g. "void varying vec4 ...").
	tString sHeaderPrefix = sSrc.substr(0, lMainPos);
	size_t lTrimEnd = sHeaderPrefix.find_last_not_of(" \t\r\n");
	if (lTrimEnd != tString::npos)
	{
		tString sTrimmed = sHeaderPrefix.substr(0, lTrimEnd + 1);
		if (sTrimmed.size() >= 4 && sTrimmed.compare(sTrimmed.size() - 4, 4, "void") == 0 &&
			(sTrimmed.size() == 4 || !isalnum((unsigned char)sTrimmed[sTrimmed.size() - 5])))
		{
			sHeaderPrefix = sTrimmed.substr(0, sTrimmed.size() - 4);
		}
	}

	//////////////////////////////
	// Parse parameters, building global declarations + a name->substitution
	// map applied to the body.
	std::vector<tString> vRawParams = SplitParams(sParamList);
	tString sGlobals;
	std::vector<std::pair<tString, tString> > vBodySubs;
	bool bNeedsFragData = false;

	for (size_t i = 0; i < vRawParams.size(); ++i)
	{
		cHpslParam param;
		if (ParseParam(vRawParams[i], param, asErrorOut) == false)
			return false;

		if (aType == eGpuShaderType_Vertex && param.msQualifier == "in")
		{
			std::map<tString, tString>::const_iterator it = gmapVertexBuiltins.find(param.msName);
			if (it == gmapVertexBuiltins.end())
			{
				asErrorOut = "Unrecognised vertex input '" + param.msName + "' - no known GLSL built-in mapping (see HpslTranspiler.h)";
				return false;
			}
			vBodySubs.push_back(std::make_pair(param.msName, it->second));
		}
		else if (aType == eGpuShaderType_Fragment && param.mlSemantic >= 0 && param.msQualifier == "out")
		{
			tString sFragData = "gl_FragData[" + cString::ToString(param.mlSemantic) + "]";
			vBodySubs.push_back(std::make_pair(param.msName, sFragData));
			bNeedsFragData = true;
		}
		else
		{
			// Plain interpolant: vertex-shader output or fragment-shader
			// input, both become a GLSL 120 'varying' of the same name -
			// this assumes (unverified beyond the clear pair) that a
			// paired vtx/frag HPSL shader uses matching parameter names
			// for values passed between them, same as clear_vtx.hpsl's
			// px_vColor/px_vPosition match clear_frag.hpsl's.
			sGlobals += "varying " + param.msType + " " + param.msName + ";\n";
		}
	}

	//////////////////////////////
	// Apply body substitutions (vertex built-ins / gl_FragData) - longest
	// names first so e.g. a name that's a prefix of another can't
	// mis-match (not an issue for the current known name set, but cheap
	// to guard).
	std::sort(vBodySubs.begin(), vBodySubs.end(),
			  [](const std::pair<tString,tString>& a, const std::pair<tString,tString>& b)
			  { return a.first.size() > b.first.size(); });
	for (size_t i = 0; i < vBodySubs.size(); ++i)
		sBody = ReplaceIdentifier(sBody, vBodySubs[i].first, vBodySubs[i].second);

	//////////////////////////////
	// Assemble final GLSL 120 source.
	tString sVersionBlock = "#version 120\n";
	if (aType == eGpuShaderType_Fragment && bNeedsFragData)
		sVersionBlock += "#extension GL_ARB_draw_buffers : enable\n";

	asGlslOut = sVersionBlock + sHeaderPrefix + sGlobals +
				"void main()\n{" + sBody + "}\n";

	return true;
}
