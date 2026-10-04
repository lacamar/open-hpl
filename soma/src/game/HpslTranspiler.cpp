#include "HpslTranspiler.h"

#include <regex>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <functional>

namespace
{
	struct cHpslParam
	{
		tString msQualifier;
		tString msType;
		tString msName;
		int mlSemantic;			// -1 if none (no ": N" suffix)
	};

	const std::map<tString, tString> gmapTypeNames = {
		{"cVector4f", "vec4"}, {"cVector3f", "vec3"}, {"cVector2f", "vec2"},
		{"cVector4i", "ivec4"}, {"cVector3i", "ivec3"}, {"cVector2i", "ivec2"},
		{"cVector4l", "ivec4"}, {"cVector3l", "ivec3"}, {"cVector2l", "ivec2"},
		{"cVector4b", "bvec4"}, {"cVector3b", "bvec3"}, {"cVector2b", "bvec2"},
		// GLSL 1.20 has no unsigned vectors
		{"cVector4u", "ivec4"}, {"cVector3u", "ivec3"}, {"cVector2u", "ivec2"},
		{"cMatrixf", "mat4"}, {"cMatrix4f", "mat4"}, {"cMatrix3f", "mat3"}, {"cMatrix3x3f", "mat3"}, {"cMatrix2x2f", "mat2"}, {"cMatrix2x4f", "mat2x4"},
		{"cTexture2D", "sampler2D"}, {"cTextureCube", "samplerCube"},
		{"cTextureRect", "sampler2DRect"}, {"cTexture3D", "sampler3D"},
		{"cTexture2DCmp", "sampler2DShadow"},
	};

	// unbound generic attributes read (0,0,0,1); tangent is on texture unit 1, shared with UV1
	const std::map<tString, tString> gmapVertexBuiltins = {
		{"vtx_vPosition", "gl_Vertex"},
		{"vtx_vColor", "gl_Color"},
		{"vtx_vNormal", "gl_Normal"},
		{"vtx_vTexCoord0", "gl_MultiTexCoord0"},
		{"vtx_vTexCoord1", "gl_MultiTexCoord1"},
		{"vtx_vTangent", "gl_MultiTexCoord1"},
	};

	tString ReplaceIdentifiers(const tString& asSrc, const std::map<tString, tString>& aMap)
	{
		tString sOut;
		sOut.reserve(asSrc.size());
		for (size_t i = 0; i < asSrc.size();)
		{
			size_t j = i;
			while (j < asSrc.size() && (isalnum((unsigned char)asSrc[j]) || asSrc[j] == '_')) ++j;
			if (j == i) { sOut += asSrc[i++]; continue; }
			std::map<tString, tString>::const_iterator it = aMap.find(asSrc.substr(i, j - i));
			sOut.append(it != aMap.end() ? it->second : asSrc.substr(i, j - i));
			i = j;
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

	// trailing comments may contain commas that break SplitParams
	tString StripLineComments(const tString& asSrc)
	{
		tString sOut;
		sOut.reserve(asSrc.size());
		bool bInComment = false;
		for (size_t i = 0; i < asSrc.size(); ++i)
		{
			char c = asSrc[i];
			if (bInComment)
			{
				if (c == '\n') { bInComment = false; sOut += c; }
				continue;
			}
			if (c == '/' && i + 1 < asSrc.size() && asSrc[i + 1] == '/')
			{
				bInComment = true;
				++i;
				continue;
			}
			sOut += c;
		}
		return sOut;
	}

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
		return ReplaceIdentifiers(asSrc, {{asFrom, asTo}});
	}

	tString StripUniformBindingIndices(const tString& asSrc)
	{
		std::regex bindRe("(uniform\\s+\\S+\\s+\\w+)\\s*:\\s*\\d+(\\s*;)");
		return std::regex_replace(asSrc, bindRe, "$1$2");
	}

	tString FlattenBufferBody(const tString& asBody)
	{
		tString sOut;
		size_t lLineStart = 0;
		while (lLineStart <= asBody.size())
		{
			size_t lLineEnd = asBody.find('\n', lLineStart);
			bool bLastLine = (lLineEnd == tString::npos);
			tString sLine = bLastLine ? asBody.substr(lLineStart) : asBody.substr(lLineStart, lLineEnd - lLineStart);

			tString sTrimmed = Trim(sLine);
			if (sTrimmed.empty() || sTrimmed.compare(0, 2, "//") == 0 || sTrimmed[0] == '#')
			{
				sOut += sLine;
			}
			else
			{
				size_t lIndentEnd = sLine.find_first_not_of(" \t");
				if (lIndentEnd == tString::npos) lIndentEnd = 0;
				sOut += sLine.substr(0, lIndentEnd) + "uniform " + sLine.substr(lIndentEnd);
			}

			if (bLastLine) break;
			sOut += "\n";
			lLineStart = lLineEnd + 1;
		}
		return sOut;
	}

	// flattened: GLSL 120 has no uniform blocks; members are referenced unqualified
	bool FlattenConstantBuffers(const tString& asSrc, tString& asOut, tString& asErrorOut)
	{
		tString sResult;
		size_t lCursor = 0;

		for (;;)
		{
			size_t lFound = tString::npos;
			size_t lSearchFrom = lCursor;
			for (;;)
			{
				size_t lCandidate = asSrc.find("cBuffer", lSearchFrom);
				if (lCandidate == tString::npos) break;
				bool bBoundaryOk = (lCandidate == 0 || !(isalnum((unsigned char)asSrc[lCandidate - 1]) || asSrc[lCandidate - 1] == '_'));
				size_t lAfter = lCandidate + 7;
				bBoundaryOk = bBoundaryOk && (lAfter >= asSrc.size() || !(isalnum((unsigned char)asSrc[lAfter]) || asSrc[lAfter] == '_'));
				if (bBoundaryOk) { lFound = lCandidate; break; }
				lSearchFrom = lCandidate + 7;
			}

			if (lFound == tString::npos)
			{
				sResult += asSrc.substr(lCursor);
				break;
			}
			sResult += asSrc.substr(lCursor, lFound - lCursor);

			size_t lBraceOpen = asSrc.find('{', lFound);
			if (lBraceOpen == tString::npos)
			{
				asErrorOut = "Found 'cBuffer' with no following '{'";
				return false;
			}

			int lDepth = 1;
			size_t i = lBraceOpen + 1;
			for (; i < asSrc.size() && lDepth > 0; ++i)
			{
				if (asSrc[i] == '{') lDepth++;
				else if (asSrc[i] == '}') lDepth--;
			}
			if (lDepth != 0)
			{
				asErrorOut = "Unterminated 'cBuffer { ... }' block (unbalanced braces)";
				return false;
			}
			size_t lBraceClose = i - 1;

			size_t lSemi = lBraceClose + 1;
			while (lSemi < asSrc.size() && isspace((unsigned char)asSrc[lSemi])) lSemi++;
			if (lSemi >= asSrc.size() || asSrc[lSemi] != ';')
			{
				asErrorOut = "'cBuffer { ... }' block not terminated with ';' after the closing brace";
				return false;
			}

			tString sBody = asSrc.substr(lBraceOpen + 1, lBraceClose - (lBraceOpen + 1));
			sResult += FlattenBufferBody(sBody);

			lCursor = lSemi + 1;
		}

		asOut = sResult;
		return true;
	}

	// paren-depth aware: arguments can nest calls
	std::vector<tString> SplitTopLevelArgs(const tString& asArgList)
	{
		std::vector<tString> vOut;
		tString sCurrent;
		int lDepth = 0;
		for (size_t i = 0; i < asArgList.size(); ++i)
		{
			char c = asArgList[i];
			if (c == '(') lDepth++;
			else if (c == ')') lDepth--;

			if (c == ',' && lDepth == 0)
			{
				vOut.push_back(Trim(sCurrent));
				sCurrent = "";
			}
			else sCurrent += c;
		}
		if (Trim(sCurrent) != "") vOut.push_back(Trim(sCurrent));
		return vOut;
	}

	// rightmost first: always a leaf call
	bool RewriteCallIntrinsic(const tString& asSrc, const tString& asFuncName,
							   const std::function<bool(const std::vector<tString>&, tString&, tString&)>& aFormatter,
							   tString& asOut, tString& asErrorOut)
	{
		tString sSrc = asSrc;
		tString sPattern = asFuncName + "(";

		for (;;)
		{
			size_t lPos = tString::npos;
			size_t lSearchFrom = sSrc.size();
			for (;;)
			{
				if (lSearchFrom == 0) break;
				size_t lFound = sSrc.rfind(sPattern, lSearchFrom - 1);
				if (lFound == tString::npos) break;

				bool bBoundaryOk = (lFound == 0) ||
					!(isalnum((unsigned char)sSrc[lFound - 1]) || sSrc[lFound - 1] == '_');
				if (bBoundaryOk) { lPos = lFound; break; }
				lSearchFrom = lFound;
			}
			if (lPos == tString::npos) break;

			size_t lArgsStart = lPos + sPattern.size();
			int lDepth = 1;
			size_t lArgsEnd = lArgsStart;
			for (; lArgsEnd < sSrc.size() && lDepth > 0; ++lArgsEnd)
			{
				if (sSrc[lArgsEnd] == '(') lDepth++;
				else if (sSrc[lArgsEnd] == ')') lDepth--;
			}
			if (lDepth != 0)
			{
				asErrorOut = "Unterminated '" + asFuncName + "(' call (unbalanced parens)";
				return false;
			}
			lArgsEnd--;

			tString sArgs = sSrc.substr(lArgsStart, lArgsEnd - lArgsStart);
			std::vector<tString> vArgs = SplitTopLevelArgs(sArgs);

			tString sReplacement;
			if (aFormatter(vArgs, sReplacement, asErrorOut) == false)
				return false;

			sSrc = sSrc.substr(0, lPos) + sReplacement + sSrc.substr(lArgsEnd + 1);
		}

		asOut = sSrc;
		return true;
	}

	std::map<tString, tString> CollectSamplerTypes(const tString& asSrc)
	{
		static const std::map<tString, tString> smapSamplerToFunc = {
			{"sampler2D", "texture2D"},
			{"samplerCube", "textureCube"},
			{"sampler2DRect", "texture2DRect"},
			{"sampler3D", "texture3D"},
		};

		std::map<tString, tString> mapOut;
		// uniforms and function parameters
		std::regex declRe("(?:uniform\\s+|[(,]\\s*)(sampler2D|samplerCube|sampler2DRect|sampler3D)\\s+(\\w+)(?=\\s*[;,)])");
		auto begin = std::sregex_iterator(asSrc.begin(), asSrc.end(), declRe);
		for (auto it = begin; it != std::sregex_iterator(); ++it)
		{
			std::smatch match = *it;
			mapOut[match[2].str()] = smapSamplerToFunc.at(match[1].str());
		}
		return mapOut;
	}

	// engine feeds the fixed-function matrix stack, never a_mtx* uniforms; a_mtxModel/a_mtxUV left alone
	tString SubstituteFixedFunctionMatrixUniforms(const tString& asSrc)
	{
		static const std::vector<std::pair<tString, tString> > gvMatrixBuiltins = {
			{"a_mtxModelViewProjection", "gl_ModelViewProjectionMatrix"},
			{"a_mtxModelView", "gl_ModelViewMatrix"},
			{"a_mtxProjection", "gl_ProjectionMatrix"},
			{"a_mtxNormal", "gl_NormalMatrix"},
		};

		tString sOut = asSrc;
		for (size_t i = 0; i < gvMatrixBuiltins.size(); ++i)
		{
			const tString& sName = gvMatrixBuiltins[i].first;
			const tString& sBuiltin = gvMatrixBuiltins[i].second;

			std::regex declRe("uniform\\s+mat4\\s+" + sName + "\\s*;\\n?");
			if (std::regex_search(sOut, declRe) == false) continue;

			sOut = std::regex_replace(sOut, declRe, "");
			sOut = ReplaceIdentifier(sOut, sName, sBuiltin);
		}
		return sOut;
	}

	bool RewriteMulIntrinsic(const tString& asSrc, tString& asOut, tString& asErrorOut)
	{
		auto formatter = [](const std::vector<tString>& aArgs, tString& asRepl, tString& asErr) -> bool
		{
			if (aArgs.size() != 2)
			{
				asErr = "mul() with " + cString::ToString((int)aArgs.size()) +
						" argument(s) is not supported (only the 2-argument matrix/vector form is)";
				return false;
			}
			asRepl = "(" + aArgs[0] + " * " + aArgs[1] + ")";
			return true;
		};
		return RewriteCallIntrinsic(asSrc, "mul", formatter, asOut, asErrorOut);
	}

	bool RewriteSampleIntrinsic(const tString& asSrc, tString& asOut, bool& abNeedsOffset, tString& asErrorOut)
	{
		std::map<tString, tString> mapSamplers = CollectSamplerTypes(asSrc);

		auto formatter = [&mapSamplers, &abNeedsOffset](const std::vector<tString>& aArgs, tString& asRepl, tString& asErr) -> bool
		{
			if (aArgs.size() == 3)
			{
				asRepl = "textureOffset(" + aArgs[0] + ", " + aArgs[1] + ", " + aArgs[2] + ")";
				abNeedsOffset = true;
				return true;
			}
			if (aArgs.size() != 2)
			{
				asErr = "sample() with " + cString::ToString((int)aArgs.size()) +
						" argument(s) is not supported (only the 2-argument texture/uv form is)";
				return false;
			}
			std::map<tString, tString>::const_iterator it = mapSamplers.find(aArgs[0]);
			if (it == mapSamplers.end())
			{
				asErr = "sample() references '" + aArgs[0] + "', which isn't a declared uniform texture "
						"(or is a texture type this transpiler doesn't map - see gmapTypeNames)";
				return false;
			}
			asRepl = it->second + "(" + aArgs[0] + ", " + aArgs[1] + ")";
			return true;
		};
		return RewriteCallIntrinsic(asSrc, "sample", formatter, asOut, asErrorOut);
	}

	// plain shadow2D: call sites never pass a projective .w
	bool RewriteSampleCmpIntrinsic(const tString& asSrc, tString& asOut, tString& asErrorOut)
	{
		auto formatter = [](const std::vector<tString>& aArgs, tString& asRepl, tString& asErr) -> bool
		{
			if (aArgs.size() != 3)
			{
				asErr = "sampleCmp() with " + cString::ToString((int)aArgs.size()) +
						" argument(s) is not supported (only the 3-argument texture/uv/refZ form is)";
				return false;
			}
			asRepl = "shadow2D(" + aArgs[0] + ", vec3(" + aArgs[1] + ", " + aArgs[2] + ")).x";
			return true;
		};
		return RewriteCallIntrinsic(asSrc, "sampleCmp", formatter, asOut, asErrorOut);
	}

	bool RewriteSampleLodGradIntrinsic(const tString& asSrc, const char* asName, const char* asGlslName, size_t alArgs,
									   tString& asOut, bool& abFired, tString& asErrorOut)
	{
		auto formatter = [&](const std::vector<tString>& aArgs, tString& asRepl, tString& asErr) -> bool
		{
			if (aArgs.size() != alArgs)
			{
				asErr = tString(asName) + "() with " + cString::ToString((int)aArgs.size()) + " argument(s) is not supported";
				return false;
			}
			asRepl = tString(asGlslName) + "(";
			for (size_t i = 0; i < aArgs.size(); ++i) asRepl += (i ? ", " : "") + aArgs[i];
			asRepl += ")";
			abFired = true;
			return true;
		};
		return RewriteCallIntrinsic(asSrc, asName, formatter, asOut, asErrorOut);
	}

	// texelFetch needs GLSL 130; abFired bumps #version
	bool RewriteLoadIntrinsic(const tString& asSrc, tString& asOut, bool& abFired, tString& asErrorOut)
	{
		std::map<tString, tString> mapSamplers = CollectSamplerTypes(asSrc);

		auto formatter = [&mapSamplers, &abFired](const std::vector<tString>& aArgs, tString& asRepl, tString& asErr) -> bool
		{
			if (aArgs.size() != 3)
			{
				asErr = "load() with " + cString::ToString((int)aArgs.size()) +
						" argument(s) is not supported (only the 3-argument texture/coords/mipLevel form is)";
				return false;
			}
			std::map<tString, tString>::const_iterator it = mapSamplers.find(aArgs[0]);
			if (it == mapSamplers.end() || it->second != "texture2D")
			{
				asErr = "load() references '" + aArgs[0] + "', which isn't a declared uniform sampler2D "
						"(load() is only supported on sampler2D - see HpslTranspiler.cpp)";
				return false;
			}
			asRepl = "texelFetch(" + aArgs[0] + ", " + aArgs[1] + ", " + aArgs[2] + ")";
			abFired = true;
			return true;
		};
		return RewriteCallIntrinsic(asSrc, "load", formatter, asOut, asErrorOut);
	}
}

bool TranspileHpslToGlsl(const tString& asPreprocessedHpsl, eGpuShaderType aType,
						  tString& asGlslOut, tString& asErrorOut)
{
	tString sSrc = asPreprocessedHpsl;
	// water_surface_frag.hpsl calls it without including helper_gamma_correction.hpsl
	if (sSrc.find("GammaToLinearCorrection(") != tString::npos && sSrc.find("GammaToLinearCorrection(in ") == tString::npos)
	{
		size_t lMain = sSrc.find("void main");
		if (lMain != tString::npos)
			sSrc.insert(lMain, "cVector3f GammaToLinearCorrection(in cVector3f v) { return pow(v, cVector3f(2.2)); }\n"
							   "cVector4f GammaToLinearCorrection(in cVector4f v) { return pow(v, cVector4f(2.2)); }\n");
	}
	sSrc = ReplaceIdentifiers(sSrc, gmapTypeNames);
	if (FlattenConstantBuffers(sSrc, sSrc, asErrorOut) == false) return false;
	sSrc = SubstituteFixedFunctionMatrixUniforms(sSrc);
	sSrc = StripUniformBindingIndices(sSrc);

	if (RewriteMulIntrinsic(sSrc, sSrc, asErrorOut) == false) return false;
	if (RewriteSampleCmpIntrinsic(sSrc, sSrc, asErrorOut) == false) return false;
	bool bNeedsTexelFetch = false;
	if (RewriteSampleIntrinsic(sSrc, sSrc, bNeedsTexelFetch, asErrorOut) == false) return false;
	if (RewriteLoadIntrinsic(sSrc, sSrc, bNeedsTexelFetch, asErrorOut) == false) return false;
	bool bNeedsGrad = false;
	if (RewriteSampleLodGradIntrinsic(sSrc, "sampleGrad", "textureGrad", 4, sSrc, bNeedsGrad, asErrorOut) == false) return false;
	if (RewriteSampleLodGradIntrinsic(sSrc, "sampleLod", "textureLod", 3, sSrc, bNeedsGrad, asErrorOut) == false) return false;
	bNeedsTexelFetch |= bNeedsGrad;

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
	lParamsEnd--;

	tString sParamList = StripLineComments(sSrc.substr(lParamsStart + 1, lParamsEnd - lParamsStart - 1));

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
	lBodyEnd--;

	tString sBody = sSrc.substr(lBodyStart + 1, lBodyEnd - lBodyStart - 1);

	// drop the "void" before main; it is re-emitted below
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

	std::vector<tString> vRawParams = SplitParams(sParamList);
	// Z prepass and G-buffer programs must hit identical depths (Equal test)
	tString sGlobals = aType == eGpuShaderType_Vertex ? "invariant gl_Position;\n" : "";
	std::vector<std::pair<tString, tString> > vBodySubs;
	bool bNeedsFragData = false;

	for (size_t i = 0; i < vRawParams.size(); ++i)
	{
		cHpslParam param;
		if (ParseParam(vRawParams[i], param, asErrorOut) == false)
			return false;

		if (param.msName == "px_vPosition" && param.mlSemantic == -1 &&
			aType == eGpuShaderType_Vertex && param.msQualifier == "out")
		{
			vBodySubs.push_back(std::make_pair(param.msName, tString("gl_Position")));
		}
		else if (param.msName == "px_vPosition" && aType == eGpuShaderType_Fragment && param.msQualifier == "in")
		{
			vBodySubs.push_back(std::make_pair(param.msName, tString("gl_FragCoord")));
		}
		else if (aType == eGpuShaderType_Vertex && param.msQualifier == "in")
		{
			std::map<tString, tString>::const_iterator it = gmapVertexBuiltins.find(param.msName);
			if (it != gmapVertexBuiltins.end())
			{
				vBodySubs.push_back(std::make_pair(param.msName, it->second));
			}
			else
			{
				// no built-in for bone data: plain attribute, not bound by the engine yet
				sGlobals += "attribute " + param.msType + " " + param.msName + ";\n";
			}
		}
		else if (aType == eGpuShaderType_Fragment && param.mlSemantic >= 0 && param.msQualifier == "out")
		{
			tString sFragData = "gl_FragData[" + cString::ToString(param.mlSemantic) + "]";
			vBodySubs.push_back(std::make_pair(param.msName, sFragData));
			bNeedsFragData = true;
		}
		else
		{
			sGlobals += "varying " + param.msType + " " + param.msName + ";\n";
		}
	}

	std::sort(vBodySubs.begin(), vBodySubs.end(),
			  [](const std::pair<tString,tString>& a, const std::pair<tString,tString>& b)
			  { return a.first.size() > b.first.size(); });
	for (size_t i = 0; i < vBodySubs.size(); ++i)
		sBody = ReplaceIdentifier(sBody, vBodySubs[i].first, vBodySubs[i].second);

	// #version 130 only when load() is used
	bool bNeedsIntOps = std::regex_search(sBody, std::regex("%|>>|<<|\\bisnan\\("));
	tString sVersionBlock = bNeedsTexelFetch || bNeedsIntOps ? "#version 130\n" : "#version 120\n";
	if (aType == eGpuShaderType_Fragment && bNeedsFragData)
		sVersionBlock += "#extension GL_ARB_draw_buffers : enable\n";

	asGlslOut = sVersionBlock + sHeaderPrefix + sGlobals +
				"void main()\n{" + sBody + "}\n";

	return true;
}
