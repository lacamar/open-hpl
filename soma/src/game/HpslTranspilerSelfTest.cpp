/*
 * See HpslTranspilerSelfTest.h for scope notes.
 */

#include "HpslTranspilerSelfTest.h"
#include "HpslTranspiler.h"

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
						const tString& asHpslName, eGpuShaderType aType)
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
		// the real intended pipeline, not a shortcut around it.
		cPreprocessParser preprocessor;
		cParserVarContainer emptyVars;
		tString sPreprocessed;
		if (preprocessor.Parse(&sRaw, &sPreprocessed, &emptyVars, sDir) == false)
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

	bool bVtxOk = TestOneShader(pResources, pLowLevel, "clear_vtx.hpsl", eGpuShaderType_Vertex);
	bool bFragOk = TestOneShader(pResources, pLowLevel, "clear_frag.hpsl", eGpuShaderType_Fragment);

	Log("HpslTranspilerSelfTest: overall result: %s\n", (bVtxOk && bFragOk) ? "PASS" : "FAIL");
}
