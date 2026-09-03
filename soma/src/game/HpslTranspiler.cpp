/*
 * See HpslTranspiler.h for scope notes.
 */

#include "HpslTranspiler.h"

#include <regex>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <functional>

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
		{"cVector4l", "ivec4"}, {"cVector3l", "ivec3"}, {"cVector2l", "ivec2"},
		{"cMatrixf", "mat4"}, {"cMatrix3f", "mat3"},
		{"cTexture2D", "sampler2D"}, {"cTextureCube", "samplerCube"},
		{"cTextureRect", "sampler2DRect"}, {"cTexture3D", "sampler3D"},
		{"cTexture2DCmp", "sampler2DShadow"},
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

	// Strips "//..." line comments from a parameter list, replacing the
	// comment text (not the newline itself, so structure is otherwise
	// unchanged) with nothing. Real case this fixes: deferred_gbuffer_
	// solid_frag.hpsl's main() declares
	// "out cVector4f out_vDiffuse : 0,   //diffuse rgb, translucency a" -
	// SplitParams()'s naive comma-split would otherwise treat the comma
	// *inside that trailing comment* as a parameter separator too (the
	// comment text itself contains a comma), corrupting the split into
	// bogus pieces that then fail ParseParam()'s regex - found live via
	// this project's real GpuShaderManager wiring session (see
	// PORTING_NOTES.md), not hypothetical.
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
				++i; // skip the second '/' too
				continue;
			}
			sOut += c;
		}
		return sOut;
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

	// Strips a D3D-style "uniform TYPE NAME : N;" texture-unit-binding
	// suffix down to "uniform TYPE NAME;" - GLSL 120 has no such syntax.
	// Only ever seen on "uniform cTextureX ..." lines in practice, but the
	// pattern is generic (any uniform decl) since nothing else in HPSL
	// syntax uses a bare ": <int>" after a declaration outside main()'s
	// own parameter list (handled separately, see ParseParam).
	tString StripUniformBindingIndices(const tString& asSrc)
	{
		std::regex bindRe("(uniform\\s+\\S+\\s+\\w+)\\s*:\\s*\\d+(\\s*;)");
		return std::regex_replace(asSrc, bindRe, "$1$2");
	}

	// Rewrites one already-brace-matched "cBuffer" block BODY (the text
	// between - but not including - the '{' and '}') into the same text
	// with "uniform " prepended to every member-declaration line, leaving
	// blank lines, "//" comments and "#define"/other '#' preprocessor
	// lines untouched. See FlattenConstantBuffers() below for why a flat
	// rewrite (rather than a real GLSL uniform block) is correct here.
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
				// Preserve the line's own leading whitespace, just inject
				// "uniform " right before the first non-blank character -
				// cosmetic only (the flattened output is still generated
				// code), but keeps a diff-friendly resemblance to the
				// source when logged for debugging.
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

	// Rewrites every "cBuffer NAME [: N] { members... };" block (HPSL's
	// HLSL-derived constant-buffer syntax - see helper_type_arguments.hpsl
	// and deferred_base_vtx.hpsl's legacy "cBuffer cVertexArguments" block,
	// both real SOMA files) into a flat sequence of top-level
	// "uniform TYPE NAME;" declarations, dropping the "cBuffer NAME [: N]"
	// wrapper and its braces entirely.
	//
	// Why flattening (not a real GLSL uniform block) is the right target:
	// GLSL 120 has no named-uniform-block syntax at all - that's a GLSL 140
	// feature (`layout(std140) uniform Name { ... };`), unavailable at the
	// GLSL 120 baseline every other hand-written shader in this engine
	// targets (see HpslTranspiler.h). But a real GPU-backed uniform buffer
	// object was never actually required for correctness here: HPSL's own
	// cBuffer members are referenced *unqualified* everywhere they're used
	// (e.g. deferred_base_vtx.hpsl's legacy body writes
	// "mul(a_mtxViewProjection, ...)", never "cVertexArguments.a_mtx...")
	// - i.e. HLSL's own cbuffer convention (which HPSL's syntax is visibly
	// derived from - the "cBuffer NAME : N" spelling mirrors HLSL's
	// "cbuffer NAME : register(bN)") already treats a constant buffer as
	// nothing more than a *named group of otherwise-ordinary globals*, and
	// this engine's own hand-written GLSL 120 shaders (e.g. a real
	// Dark Descent install's core/shaders/deferred_base_vtx.glsl, and
	// HPSL's own non-cBuffer files like base_vtx.hpsl) already set the
	// equivalent uniforms individually by name via glUniform*, not via a
	// bound buffer object - so a flat rewrite is both sufficient (nothing
	// in the shader body needs block-qualified access) and faithful (same
	// unqualified names HPSL itself already uses).
	//
	// The optional ": N" is a D3D-style constant-buffer *register* binding
	// (`register(bN)`) - meaningless to GLSL uniforms set by name, so it's
	// simply discarded along with the rest of the "cBuffer NAME" header
	// (same rationale as StripUniformBindingIndices() above, for the same
	// D3D-register-binding convention on texture uniforms).
	//
	// NOTE ON SCOPE: helper_type_arguments.hpsl @ifdef-branches between
	// several *mutually exclusive* cBuffer sets keyed by which
	// "MaterialType_X" combo variable is defined. That branching is HPSL's
	// own @ifdef syntax, already resolved by cPreprocessParser::Parse()
	// before this function ever runs (see HpslTranspiler.h's scope note) -
	// by the time source reaches here, at most the one cBuffer block for
	// the actually-selected MaterialType survives, so this function never
	// needs to know about MaterialType selection itself, only flatten
	// whatever cBuffer block(s) remain in the text it's given.
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
				size_t lAfter = lCandidate + 7; // strlen("cBuffer")
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
			size_t lBraceClose = i - 1; // points at the matching '}'

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

	// Paren-depth-aware split of a call's argument list on top-level
	// commas - unlike SplitParams() (used only for main()'s own parameter
	// list, where no HPSL param type ever contains a paren or comma),
	// intrinsic call arguments are arbitrary expressions that can nest
	// constructor calls (e.g. "mul(m, cVector4f(x,y,z,w))"), so a naive
	// comma split would mis-split those.
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

	// Rewrites every call to asFuncName(...) in asSrc via aFormatter(args),
	// processing whichever occurrence is rightmost in the string first on
	// each pass. This is always safe for nested calls (e.g.
	// "mul(mul(a,b),c)"): any call nested inside another necessarily
	// starts at a strictly greater string offset than its enclosing call
	// (it appears somewhere inside the enclosing call's own argument
	// list, which begins after the enclosing call's own name+'('), so the
	// rightmost remaining "name(" at any point in the loop can never
	// itself contain another not-yet-rewritten occurrence - it's always
	// a leaf call, safe to rewrite immediately.
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
			lArgsEnd--; // point at the matching ')'

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

	// Collects uniform texture declarations (post type-mapping, so already
	// e.g. "uniform sampler2D aColorMap;") so sample() calls can be routed
	// to the right GLSL 120 sampling function for the referenced texture's
	// type - GLSL 120 has no overloaded generic "sample", unlike HPSL.
	std::map<tString, tString> CollectSamplerTypes(const tString& asSrc)
	{
		static const std::map<tString, tString> smapSamplerToFunc = {
			{"sampler2D", "texture2D"},
			{"samplerCube", "textureCube"},
			{"sampler2DRect", "texture2DRect"},
			{"sampler3D", "texture3D"},
		};

		std::map<tString, tString> mapOut;
		std::regex declRe("uniform\\s+(sampler2D|samplerCube|sampler2DRect|sampler3D)\\s+(\\w+)\\s*;");
		auto begin = std::sregex_iterator(asSrc.begin(), asSrc.end(), declRe);
		for (auto it = begin; it != std::sregex_iterator(); ++it)
		{
			std::smatch match = *it;
			mapOut[match[2].str()] = smapSamplerToFunc.at(match[1].str());
		}
		return mapOut;
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

	bool RewriteSampleIntrinsic(const tString& asSrc, tString& asOut, tString& asErrorOut)
	{
		std::map<tString, tString> mapSamplers = CollectSamplerTypes(asSrc);

		auto formatter = [&mapSamplers](const std::vector<tString>& aArgs, tString& asRepl, tString& asErr) -> bool
		{
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
						"(or is a texture type this transpiler doesn't map - see gvTypeMap)";
				return false;
			}
			asRepl = it->second + "(" + aArgs[0] + ", " + aArgs[1] + ")";
			return true;
		};
		return RewriteCallIntrinsic(asSrc, "sample", formatter, asOut, asErrorOut);
	}

	// sampleCmp(texture, uv, refZ) - HPSL's HLSL-style shadow-comparison
	// sample (3-argument: a cTexture2DCmp uniform, a 2-component uv, and a
	// scalar reference depth to compare against) - rewritten to GLSL 120's
	// built-in shadow2D(sampler2DShadow, vec3(uv, refZ)), reduced to a
	// scalar via .x to match sampleCmp's scalar-float HLSL convention
	// (every real call site assigns its result straight to a float, e.g.
	// "float fShadowAmount = sampleCmp(...);"). Deliberately the plain
	// (non-projective) shadow2D, not shadow2DProj: unlike this engine's own
	// hand-written deferred_light_frag.glsl (which calls shadow2DProj with
	// a full vec4 including a .w for the perspective divide), every real
	// HPSL sampleCmp() call site only ever passes 3 arguments and never
	// reads its uv source's .w component at all (e.g.
	// "sampleCmp(aShadowMap, avLocation.xy + avOffset, avLocation.z)" from
	// a cVector4f avLocation - .w simply isn't part of the call) - HPSL's
	// own shadow-lookup convention here has no projective-divide step to
	// preserve, so inventing a .w argument that isn't in the source would
	// be unfaithful, not just unnecessary.
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

	// load(texture, ivecN coords, mipLevel) - HPSL's HLSL-style
	// Texture.Load() exact-texel fetch (no filtering, integer pixel
	// coordinates) - real use: deferred_light_frag.hpsl's G-buffer readback,
	// "load(aNormalDepthMap, vMapCoords, 0)" where vMapCoords is a
	// cVector2l/ivec2. Maps 1:1 to GLSL's texelFetch(sampler2D, ivec2, int
	// lod) - same 3-argument shape, no argument reshuffling needed (unlike
	// sampleCmp above). Only sampler2D is supported (the only texture type
	// any real load() call site references so far - see PORTING_NOTES.md);
	// other sampler types are rejected with a clear error rather than
	// guessed at, same conservative approach as sample()/sampleCmp().
	//
	// texelFetch is GLSL 130+, not available in GLSL 120 at all (this
	// engine's baseline for every other shader) - TranspileHpslToGlsl()
	// bumps the emitted #version to 130 for exactly (and only) the shader
	// files where this rewrite actually fires, tracked via abFired. GLSL
	// 130 is still a compatibility-profile version (core profiles didn't
	// exist until 150), so gl_FragData/gl_FragCoord/"varying"/gl_Vertex/
	// gl_MultiTexCoordN etc. all keep working unchanged in the shaders that
	// need this bump - see HpslTranspiler.h for why a wholesale version
	// bump was deliberately avoided elsewhere (cTextureBuffer/instancing)
	// but is safe and scoped here (fragment-shader-only in every real case
	// found, and additive - texelFetch is a new function, not a
	// replacement for anything gl_Vertex-related that a vertex shader would
	// need).
	bool RewriteLoadIntrinsic(const tString& asSrc, tString& asOut, bool& abFired, tString& asErrorOut)
	{
		std::map<tString, tString> mapSamplers = CollectSamplerTypes(asSrc);
		abFired = false;

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

//---------------------------------------------------------------

bool TranspileHpslToGlsl(const tString& asPreprocessedHpsl, eGpuShaderType aType,
						  tString& asGlslOut, tString& asErrorOut)
{
	tString sSrc = ReplaceTypeNames(asPreprocessedHpsl);
	if (FlattenConstantBuffers(sSrc, sSrc, asErrorOut) == false) return false;
	sSrc = StripUniformBindingIndices(sSrc);

	if (RewriteMulIntrinsic(sSrc, sSrc, asErrorOut) == false) return false;
	if (RewriteSampleCmpIntrinsic(sSrc, sSrc, asErrorOut) == false) return false;
	if (RewriteSampleIntrinsic(sSrc, sSrc, asErrorOut) == false) return false;
	bool bNeedsTexelFetch = false;
	if (RewriteLoadIntrinsic(sSrc, sSrc, bNeedsTexelFetch, asErrorOut) == false) return false;

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

		if (param.msName == "px_vPosition" && param.mlSemantic == -1 &&
			aType == eGpuShaderType_Vertex && param.msQualifier == "out")
		{
			// HPSL's HLSL-SV_Position-equivalent convention name: the
			// vertex shader's clip-space position output, consumed by the
			// fixed-function clipper/rasterizer - NOT an interpolated
			// varying (see HpslTranspiler.h scope note).
			vBodySubs.push_back(std::make_pair(param.msName, tString("gl_Position")));
		}
		else if (param.msName == "px_vPosition" && aType == eGpuShaderType_Fragment && param.msQualifier == "in")
		{
			// Mirror of the above on the fragment side: the interpolated
			// screen-space position, read from the built-in rather than a
			// same-named varying (which the vertex shader side never
			// declares, per the case above).
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
				// No GLSL 120 fixed-function built-in carries this semantic
				// (e.g. vtx_vTangent, vtx_vBoneIndices, vtx_vBoneWeight in
				// deferred_base_vtx.hpsl - real SOMA material shaders that
				// need per-vertex tangent/skinning data, which GL's legacy
				// fixed-function pipeline has no dedicated attribute for).
				// Declared as an ordinary GLSL 120 "attribute" of the same
				// name instead - valid syntax, and no substitution needed
				// since the body already spells the name this way.
				//
				// Deliberately NOT aliased onto a spare gl_MultiTexCoordN
				// slot the way this engine's own hand-written
				// deferred_base_vtx.glsl packs tangent data into
				// gl_MultiTexCoord1: that shader has no separate second-UV
				// input to conflict with, but HPSL shaders declare
				// vtx_vTexCoord1 (mapped to gl_MultiTexCoord1 above) *and*
				// vtx_vTangent as distinct main() parameters - aliasing
				// both to the same built-in would silently corrupt data
				// whenever a material uses both (UseUvCoord1 +
				// UseNormalMapping together).
				//
				// NOT yet wired end-to-end: something on the C++ mesh-
				// upload side (cVertexBuffer -> LowLevelGraphicsSDL, whoever
				// owns cGpuShaderManager::CreateShader()'s calling
				// convention) still needs to glBindAttribLocation /
				// glVertexAttribPointer this same attribute name to actual
				// per-vertex tangent/bone data for it to do anything beyond
				// compile - see PORTING_NOTES.md "SOMA" section. Tracked as
				// a follow-up, not attempted in this pass (out of this
				// transpiler's scope: it only has the shader source, not
				// the mesh format or draw-call setup).
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
	// Assemble final GLSL source - #version 120 for every shader, except a
	// #version 130 bump for the specific files that actually use load()
	// (texelFetch needs it - see RewriteLoadIntrinsic() above for why this
	// narrow, per-file bump is safe unlike a blanket engine-wide one).
	tString sVersionBlock = bNeedsTexelFetch ? "#version 130\n" : "#version 120\n";
	if (aType == eGpuShaderType_Fragment && bNeedsFragData)
		sVersionBlock += "#extension GL_ARB_draw_buffers : enable\n";

	asGlslOut = sVersionBlock + sHeaderPrefix + sGlobals +
				"void main()\n{" + sBody + "}\n";

	return true;
}
