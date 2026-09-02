/*
 * See HpslTranspilerSelfTest.h for scope notes.
 */

#include "HpslTranspilerSelfTest.h"
#include "HpslTranspiler.h"

#include <vector>

//---------------------------------------------------------------

namespace
{
	bool LoadFileString(cFileSearcher *apFileSearcher, const tString& asName, tString& asOut, tWString& asDirOut)
	{
		const tWString& sPath = apFileSearcher->GetFilePath(asName);
		if (sPath == _W(""))
		{
			Error("HpslTranspilerSelfTest: couldn't find '%s' in resources\n", asName.c_str());
			return false;
		}

		unsigned int lSize = cPlatform::GetFileSize(sPath);
		asOut.resize(lSize);
		cPlatform::CopyFileToBuffer(sPath, &asOut[0], lSize);
		asDirOut = cString::GetFilePathW(sPath);
		return true;
	}

	bool TestOneShader(cResources *apResources, iLowLevelGraphics *apLowLevel,
						const tString& asHpslName, eGpuShaderType aType,
						const std::vector<tString>& avDefines = std::vector<tString>())
	{
		tString sRaw;
		tWString sDir;
		if (LoadFileString(apResources->GetFileSearcher(), asHpslName, sRaw, sDir) == false)
			return false;

		//////////////////////////////
		// Same @ifdef/@include preprocessor HPL2's own GLSL shaders go
		// through (cPreprocessParser) - neither clear_vtx.hpsl nor
		// clear_frag.hpsl actually uses any directives, so this step is a
		// no-op for them, but it's included so this self-test exercises
		// the real intended pipeline, not a shortcut around it. avDefines
		// sets combo-variables (e.g. "UseDiffuse") the same way the real
		// material system would, so materially-gated real shaders (e.g.
		// deferred_base_vtx.hpsl/deferred_base_frag.hpsl) can be exercised
		// with a specific, deliberately chosen combo rather than only the
		// "everything undefined" case - see PORTING_NOTES.md's "SOMA"
		// section for why UseTextureBuffer/UseMeshInstancing/
		// UseStaticMeshInstancing are never in this list: leaving them
		// undefined steers real material shaders onto their simpler
		// "TEMP BACKWARD COMPATIBILITY" @else branch, which the transpiler
		// actually supports (no cTextureBuffer instancing).
		cPreprocessParser preprocessor;
		cParserVarContainer vars;
		for (size_t i = 0; i < avDefines.size(); ++i)
			vars.Add(avDefines[i]);
		tString sPreprocessed;
		if (preprocessor.Parse(&sRaw, &sPreprocessed, &vars, sDir) == false)
		{
			Error("HpslTranspilerSelfTest: '%s' failed HPL preprocessing\n", asHpslName.c_str());
			return false;
		}

		tString sGlsl, sTranspileError;
		if (TranspileHpslToGlsl(sPreprocessed, aType, sGlsl, sTranspileError) == false)
		{
			Error("HpslTranspilerSelfTest: '%s' transpile failed: %s\n", asHpslName.c_str(), sTranspileError.c_str());
			return false;
		}

		Log("HpslTranspilerSelfTest: '%s' transpiled to:\n%s\n", asHpslName.c_str(), sGlsl.c_str());

		iGpuShader *pShader = apLowLevel->CreateGpuShader(asHpslName, aType);
		bool bCompiled = pShader->CreateFromString(sGlsl.c_str());
		hplDelete(pShader);

		if (bCompiled)
			Log("HpslTranspilerSelfTest: '%s' PASSED (compiled as real GLSL)\n", asHpslName.c_str());
		else
			Error("HpslTranspilerSelfTest: '%s' FAILED to compile (see GLSL compiler log above)\n", asHpslName.c_str());

		return bCompiled;
	}
}

//---------------------------------------------------------------

void RunHpslTranspilerSelfTest(cEngine *apEngine)
{
	iLowLevelGraphics *pLowLevel = apEngine->GetGraphics()->GetLowLevel();
	cResources *pResources = apEngine->GetResources();

	struct cCase { const char *psFile; eGpuShaderType mType; };
	static const cCase vCases[] = {
		{ "clear_vtx.hpsl", eGpuShaderType_Vertex },
		{ "clear_frag.hpsl", eGpuShaderType_Fragment },
		{ "null_vtx.hpsl", eGpuShaderType_Vertex },
		{ "null_frag.hpsl", eGpuShaderType_Fragment },
		{ "deferred_depthonly_frag.hpsl", eGpuShaderType_Fragment },
		{ "deferred_posteffect_quad_vtx.hpsl", eGpuShaderType_Vertex },
		{ "debug_overdraw_frag.hpsl", eGpuShaderType_Fragment },
	};

	bool bAllOk = true;
	for (size_t i = 0; i < sizeof(vCases) / sizeof(vCases[0]); ++i)
	{
		bool bOk = TestOneShader(pResources, pLowLevel, vCases[i].psFile, vCases[i].mType);
		bAllOk = bAllOk && bOk;
	}

	//////////////////////////////
	// Real deferred-rendering material shaders (as opposed to the trivial
	// fixed-function-only files above) - deliberately a minimal combo
	// (diffuse texture + vertex color + normals; no UseTextureBuffer/
	// UseMeshInstancing/UseSkeleton/UseSway/UseNormalMapping/...) chosen to
	// exercise the new cBuffer-flattening and custom-attribute-fallback
	// support with the smallest real permutation that still needs both -
	// see PORTING_NOTES.md "SOMA" section. UseTextureBuffer is left
	// undefined on purpose: it steers deferred_base_vtx.hpsl/
	// deferred_base_frag.hpsl onto their "TEMP BACKWARD COMPATIBILITY"
	// @else branch (a single flat cBuffer, no MaterialType indirection, no
	// cTextureBuffer instancing), which is what's actually implemented.
	std::vector<tString> vMinimalMaterialCombo;
	vMinimalMaterialCombo.push_back("UseUv");
	vMinimalMaterialCombo.push_back("UseNormals");
	vMinimalMaterialCombo.push_back("UseColor");
	vMinimalMaterialCombo.push_back("UseDiffuse");

	bool bVtxOk = TestOneShader(pResources, pLowLevel, "deferred_base_vtx.hpsl", eGpuShaderType_Vertex, vMinimalMaterialCombo);
	bool bFragOk = TestOneShader(pResources, pLowLevel, "deferred_base_frag.hpsl", eGpuShaderType_Fragment, vMinimalMaterialCombo);
	bAllOk = bAllOk && bVtxOk && bFragOk;

	// deferred_gbuffer_solid_frag.hpsl - the actual G-buffer fragment shader
	// real lit materials need (not just deferred_base_frag.hpsl's simpler
	// diffuse-only path). "out_vNormal.w = px_fLinearDepth;" is written
	// unconditionally in this file's body regardless of combo, so
	// UseLinearDepth must be set for it to even reference a declared
	// input - not optional the way it is for deferred_base_vtx/frag.
	std::vector<tString> vGBufferSolidCombo;
	vGBufferSolidCombo.push_back("UseNormals");
	vGBufferSolidCombo.push_back("UseLinearDepth");
	bool bGBufferOk = TestOneShader(pResources, pLowLevel, "deferred_gbuffer_solid_frag.hpsl", eGpuShaderType_Fragment, vGBufferSolidCombo);
	bAllOk = bAllOk && bGBufferOk;

	// deferred_base_frag.hpsl again, this time with UseDissolve - exercises
	// the cTexture3D/sampler3D "aDissolveMap" path (a coordinator-flagged
	// concern from a parallel session's real GpuShaderManager wiring run)
	// that vMinimalMaterialCombo above deliberately doesn't touch. UseUv is
	// required alongside UseDiffuse (the diffuse-sampling body line always
	// reads px_vTexCoord0, which deferred_base_frag.hpsl only declares as a
	// main() input when UseUv is set) - any real material combo would
	// always set both together; a first attempt at this test case with
	// UseDissolve alone (no UseUv) correctly failed to compile with
	// "px_vTexCoord0' undeclared", which was this test's own incoherent
	// combo, not a transpiler bug - real GLSL rightly rejecting an invalid
	// permutation this test itself asked for.
	std::vector<tString> vDissolveCombo;
	vDissolveCombo.push_back("UseUv");
	vDissolveCombo.push_back("UseDiffuse");
	vDissolveCombo.push_back("UseDissolve");
	bool bDissolveOk = TestOneShader(pResources, pLowLevel, "deferred_base_frag.hpsl", eGpuShaderType_Fragment, vDissolveCombo);
	bAllOk = bAllOk && bDissolveOk;

	Log("HpslTranspilerSelfTest: overall result: %s\n", bAllOk ? "PASS" : "FAIL");
}
