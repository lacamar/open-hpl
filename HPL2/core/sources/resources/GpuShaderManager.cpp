/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include "resources/GpuShaderManager.h"
#include "resources/RustResMgr.h"

#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"
#include "system/Platform.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/GPUShader.h"

#include "resources/FileSearcher.h"
#include "resources/TextureManager.h"
#include "resources/RustShader.h"

#include <cstdlib>

#ifdef WIN32
#include <io.h>
#endif

namespace hpl {


	namespace {

		void AppendSink(void* apCtx, const char* apData, size_t alLen) noexcept
		{
			static_cast<tString*>(apCtx)->append(apData, alLen);
		}

		void SamplerCallback(void* apCtx, const char* apName, size_t alLen, int alUnit) noexcept
		{
			static_cast<iGpuShader*>(apCtx)->AddSamplerUnit(tString(apName, alLen), alUnit);
		}
	}

	static void ApplyHpslTextureBindings(iGpuShader* apShader, const tString& asHpslText)
	{
		ohpl_shader_texture_bindings(asHpslText.data(), asHpslText.size(), SamplerCallback, apShader);
	}


	static int gnHpslDumpCounter = 0;

	static void DumpTranspiledShaderIfRequested(const tString &asName, const tString &asGlsl)
	{
		const char *pDumpDir = getenv("OPENHPL_DUMP_HPSL_SHADERS_DIR");
		if(pDumpDir == NULL) return;

		tString sSanitizedName = asName;
		for(size_t i=0; i<sSanitizedName.size(); ++i)
		{
			if(sSanitizedName[i] == '/' || sSanitizedName[i] == '\\') sSanitizedName[i] = '_';
		}

		tString sPath = tString(pDumpDir) + "/" + cString::ToString(gnHpslDumpCounter++) + "_" + sSanitizedName + ".glsl";
		FILE *pFile = cPlatform::OpenFile(cString::To16Char(sPath), _W("wb"));
		if(pFile == NULL) return;

		fwrite(asGlsl.data(), 1, asGlsl.size(), pFile);
		fclose(pFile);
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	tHpslTranspileCallback cGpuShaderManager::mpHpslTranspileCallback = NULL;
	tStringVec cGpuShaderManager::mvGlobalDefines;

	cGpuShaderManager::cGpuShaderManager(cFileSearcher *apFileSearcher,iLowLevelGraphics *apLowLevelGraphics, 
		iLowLevelResources *apLowLevelResources,iLowLevelSystem *apLowLevelSystem)
		: iResourceManager(apFileSearcher, apLowLevelResources,apLowLevelSystem)
	{
		mpLowLevelGraphics = apLowLevelGraphics;

		mpPreprocessParser = hplNew(cPreprocessParser, () );

		mpPreprocessParser->GetEnvVarContainer()->Add("ScreenWidth",mpLowLevelGraphics->GetScreenSizeInt().x);
		mpPreprocessParser->GetEnvVarContainer()->Add("ScreenHeigth",mpLowLevelGraphics->GetScreenSizeInt().y);

		#ifdef WIN32
			mpPreprocessParser->GetEnvVarContainer()->Add("OS_Windows");
		#elif defined(__APPLE__)
			mpPreprocessParser->GetEnvVarContainer()->Add("OS_OSX");
		#elif defined(__linux__)
			mpPreprocessParser->GetEnvVarContainer()->Add("OS_Linux");
		#endif

		for(size_t i=0; i<mvGlobalDefines.size(); ++i)
			mpPreprocessParser->GetEnvVarContainer()->Add(mvGlobalDefines[i]);
	}

	cGpuShaderManager::~cGpuShaderManager()
	{
		hplDelete(mpPreprocessParser);
		ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_GPU_SHADER);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cGpuShaderManager::CheckFeatureSupport()
	{
		////////////////////////////
		//Shader model variables
		if(mpLowLevelGraphics->GetCaps(eGraphicCaps_ShaderModel_2))		mpPreprocessParser->GetEnvVarContainer()->Add("ShaderModel_2");
		if(mpLowLevelGraphics->GetCaps(eGraphicCaps_ShaderModel_3))		mpPreprocessParser->GetEnvVarContainer()->Add("ShaderModel_3");
		if(mpLowLevelGraphics->GetCaps(eGraphicCaps_ShaderModel_4))		mpPreprocessParser->GetEnvVarContainer()->Add("ShaderModel_4");

		/////////////////////////
		// Test Feature support
		if(mpFileSearcher->GetFilePath("_test_array_support_frag.glsl") != _W("") &&
		   IsShaderSupported("_test_array_support_frag.glsl", eGpuShaderType_Fragment)==false)
		{
			Log("ATTENTION: System does not support const arrays in glsl!\n");
			mpPreprocessParser->GetEnvVarContainer()->Add("FeatureNotSupported_ConstArray");
		}
	}

	//-----------------------------------------------------------------------
	static void PatchHpslSource(const tString& asFile, tString& asData)
	{
		const char *pPatch = getenv("OPENHPL_HPSL_PATCH");
		tString sPatch = pPatch ? pPatch : "";
		tString sOut;
		ohpl_shader_patch_hpsl(asFile.data(), asFile.size(), asData.data(), asData.size(), cTextureManager::GetHardwareSRGB(),
							   sPatch.data(), sPatch.size(), AppendSink, &sOut);
		asData = sOut;
	}

	static tString GetHpslFallbackName(const tString& asGlslName)
	{
		tString sOut;
		ohpl_shader_fallback_name(asGlslName.data(), asGlslName.size(), AppendSink, &sOut);
		return sOut;
	}

	iGpuShader* cGpuShaderManager::CreateShader(const tString& asName, eGpuShaderType aType,
												cParserVarContainer *apVarContainer)
	{
		return H3Res<iGpuShader>(ohpl_h3f_gpu_shader(mpRs, H3Ops(), RM_STR(asName), (int32_t)aType, apVarContainer, mpHpslTranspileCallback != NULL, aType == eGpuShaderType_Fragment));
	}
	//-----------------------------------------------------------------------

	void cGpuShaderManager::Destroy(iResourceBase* apResource)
	{
		ohpl_h3f_gpu_destroy(mpRs, H3Ops(), apResource);
	}

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	bool cGpuShaderManager::IsShaderSupported(const tString& asName, eGpuShaderType aType)
	{
		/////////////////////////////////
		//Get file from file searcher
		tWString sPath = mpFileSearcher->GetFilePath(asName);
		if(sPath==_W("")){
			Error("Couldn't find test file '%s' in resources\n",asName.c_str());
			return false;
		}

		/////////////////////////////////
		//Compile
		iGpuShader* pShader = mpLowLevelGraphics->CreateGpuShader(asName, aType);
		
		bool bRet = pShader->CreateFromFile(sPath, "main", false);
		hplDelete(pShader);
		
		return bRet;
	}
	
	//-----------------------------------------------------------------------

	void* cGpuShaderManager::H3Create(OhplH3Create &aCreate)
	{
		tString sName = H3Name(aCreate.name);
		tWString sPath = H3Wide(aCreate.path);
		eGpuShaderType type = (eGpuShaderType)aCreate.i[0];
		switch(aCreate.kind)
		{
		case OHPL_H3_SHADER_PARSE:
		{
			tString sFileData;
			unsigned int lFileSize = cPlatform::GetFileSize(sPath);
			sFileData.resize(lFileSize);
			cPlatform::CopyFileToBuffer(sPath,&sFileData[0],lFileSize);
			msScratchParsed.clear();
			if(aCreate.i[0] == 0)
			{
				cParserVarContainer *pVars = static_cast<cParserVarContainer*>(aCreate.ptr);
				if(aCreate.i[1])
				{
					if(pVars->Get("UseDepth") != NULL) pVars->Add("UseLinearDepth");
					pVars->Add("UseExtendedArgs");
					PatchHpslSource(H3Name(aCreate.name2), sFileData);
				}
				mpPreprocessParser->Parse(&sFileData, &msScratchParsed, pVars, cString::GetFilePathW(sPath));
				if(aCreate.i[1])
				{
					// Rebirth reads BC5 as RG; cSDLTexture swizzles it to SOMA's LATC2 layout
					size_t lBC5 = msScratchParsed.find("avNormalValue.yxx");
					if(lBC5 != tString::npos) msScratchParsed.replace(lBC5, 17, "avNormalValue.wyx");
				}
			}
			else
			{
				cParserVarContainer emptyVars;
				mpPreprocessParser->Parse(&sFileData, &msScratchParsed, &emptyVars, cString::GetFilePathW(sPath));
			}
			msScratchPre = msScratchParsed;
			return NULL;
		}
		case OHPL_H3_SHADER_TRANSPILE:
		{
			tString sGlsl;
			aCreate.status = 0;
			if(mpHpslTranspileCallback(msScratchParsed, type, sGlsl, msTranspileError)==false)
			{
				aCreate.status = 1;
				return NULL;
			}
			msScratchParsed = sGlsl;
			DumpTranspiledShaderIfRequested(sName, sGlsl);
			return NULL;
		}
		case OHPL_H3_SHADER_FILE:
		{
			iGpuShader *pShader = mpLowLevelGraphics->CreateGpuShader(sName, type);
			if(pShader->CreateFromFile(sPath)==false)
			{
				hplDelete(pShader);
				return NULL;
			}
			return static_cast<iResourceBase*>(pShader);
		}
		case OHPL_H3_SHADER_STRING:
		{
			iGpuShader *pShader = mpLowLevelGraphics->CreateGpuShader(sName, type);
			pShader->SetFullPath(sPath);
			if(pShader->CreateFromString(msScratchParsed.c_str())==false)
			{
				hplDelete(pShader);
				return NULL;
			}
			return static_cast<iResourceBase*>(pShader);
		}
		}
		return NULL;
	}

	int64_t cGpuShaderManager::H3Call(int alWhat, void *apObj, int64_t alArg)
	{
		if(alWhat != OHPL_H3C_SHADER_POST) return 0;
		iGpuShader *pShader = H3Res<iGpuShader>(apObj);
		if((alArg & 1) && pShader->SamplerNeedsTextureUnitSetup()) ApplyHpslTextureBindings(pShader, msScratchPre);
		if((alArg & 2) && pShader->SamplerNeedsTextureUnitSetup())
		{
			std::vector<ohpl_shader_kv> vVars;
			for(auto& kv : *mpPreprocessParser->GetParsingVarContainer()->GetMapPtr())
			{
				ohpl_shader_kv e = { kv.first.data(), kv.first.size(), kv.second.data(), kv.second.size() };
				vVars.push_back(e);
			}
			ohpl_shader_sampler_units(vVars.data(), vVars.size(), SamplerCallback, pShader);
		}
		return 0;
	}

	void cGpuShaderManager::H3Text(int alWhat, void *apObj, const OhplH3WStr &aPath, void (*apSink)(void*, const uint8_t*, size_t), void *apCtx)
	{
		if(alWhat == OHPL_H3C_SHADER_ERROR) apSink(apCtx, reinterpret_cast<const uint8_t*>(msTranspileError.data()), msTranspileError.size());
	}
}
