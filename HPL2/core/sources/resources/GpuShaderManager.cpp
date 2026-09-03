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

#include "resources/GpuShaderManager.h"

#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"
#include "system/Platform.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/GPUShader.h"

#include "resources/FileSearcher.h"

#include <regex>

#ifdef WIN32
#include <io.h>
#endif

namespace hpl {

	//-----------------------------------------------------------------------

	// HPSL declares each texture uniform's binding as a D3D-style
	// "uniform cTextureX NAME : N;" suffix (see HpslTranspiler.cpp's
	// StripUniformBindingIndices(), which discards it since GLSL 120 has no
	// such syntax) - unlike Dark Descent's own hand-written .glsl shaders,
	// which instead rely on a "@define sampler_NAME N" preprocessor line
	// parsed into cPreprocessParser's own var container and consumed just
	// below (the "Sampler to texture units setup" block). HPSL source has no
	// such @define, so that block finds nothing and every fragment-shader
	// sampler silently stays bound to GLSL's default texture unit 0 - live-
	// confirmed via SOMA's real deferred_light_frag.hpsl: aDiffuseMap/
	// aNormalDepthMap/aSpecMap/aShadowMap/aShadowOffsetMap all sampling
	// whatever happened to be bound to unit 0, producing visible garbage
	// instead of real lighting even once the shader compiles and runs.
	// Extracted here (from the pre-transpile, post-@ifdef-preprocessing HPSL
	// text, where the ": N" suffix is still present) and fed to the same
	// iGpuShader::AddSamplerUnit() the @define path already uses, so both
	// mechanisms land on the same consumer (cGLSLProgram::Compile()).
	static void ApplyHpslTextureBindings(iGpuShader* apShader, const tString& asHpslText)
	{
		static const std::regex bindRe("uniform\\s+cTexture\\w*\\s+(\\w+)\\s*:\\s*(\\d+)\\s*;");
		auto begin = std::sregex_iterator(asHpslText.begin(), asHpslText.end(), bindRe);
		for(auto it = begin; it != std::sregex_iterator(); ++it)
		{
			apShader->AddSamplerUnit((*it)[1].str(), cString::ToInt((*it)[2].str().c_str(), 0));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	tHpslTranspileCallback cGpuShaderManager::mpHpslTranspileCallback = NULL;

	//-----------------------------------------------------------------------

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
	}

	cGpuShaderManager::~cGpuShaderManager()
	{
		hplDelete(mpPreprocessParser);

		DestroyAll();

		Log(" Done with Gpu programs\n");
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
		if(IsShaderSupported("_test_array_support_frag.glsl", eGpuShaderType_Fragment)==false)
		{
			Log("ATTENTION: System does not support const arrays in glsl!\n");
			mpPreprocessParser->GetEnvVarContainer()->Add("FeatureNotSupported_ConstArray");
		}
	}

	//-----------------------------------------------------------------------

	iGpuShader* cGpuShaderManager::CreateShader(const tString& asName, eGpuShaderType aType,
												cParserVarContainer *apVarContainer)
	{
		iGpuShader* pShader;

		BeginLoad(asName);

		/////////////////////////////////////////
        // If we have a variable container do NOT add the shader as a resource!
		if(apVarContainer)
		{
			tString sFileData;
			tString sParsedOutput;

			/////////////////////////////////
			//Get file from file searcher
			bool bIsHpslFallback = false;
			tString sHpslName;
			tWString sPath = mpFileSearcher->GetFilePath(asName);
			if(sPath==_W("") && mpHpslTranspileCallback)
			{
				/////////////////////////////////
				// No .glsl by this name - see if a same-named .hpsl exists
				// (SOMA/Rebirth/Bunker's HPL3 shader source). Only tried
				// when a game module has registered a transpiler via
				// SetHpslTranspileCallback(); Dark Descent/AMFP never do,
				// so this block is unreachable for them and sPath=="" falls
				// straight into the existing error path below unchanged.
				sHpslName = cString::SetFileExt(asName, "hpsl");
				tWString sHpslPath = mpFileSearcher->GetFilePath(sHpslName);
				if(sHpslPath != _W(""))
				{
					sPath = sHpslPath;
					bIsHpslFallback = true;
				}
			}
			if(sPath==_W("")){
				Error("Couldn't find file '%s' in resources\n",asName.c_str());
				EndLoad();
				return NULL;
			}

			/////////////////////////////////
			//Load data
			unsigned int lFileSize = cPlatform::GetFileSize(sPath);

			sFileData.resize(lFileSize);
			cPlatform::CopyFileToBuffer(sPath,&sFileData[0],lFileSize);

			/////////////////////////////////
			//Parse file
			if(bIsHpslFallback)
			{
				/////////////////////////////////
				// HPL2's own material combo-variable-setting C++
				// (MaterialType_BasicSolid.cpp etc.) was written for Dark
				// Descent's own hand-written GLSL @ifdef vocabulary, and is
				// reused as-is here (Dark Descent's own compiles never take
				// this bIsHpslFallback branch at all, so mutating
				// apVarContainer here can't affect them). Most flag names
				// happen to already match HPSL's own vocabulary verbatim
				// (UseUv/UseNormals/UseNormalMapping/UseColor/UseEnvMap/
				// UseCubeMapAlpha/...), but a few HPL2-legacy names
				// represent the same real concept under a different exact
				// spelling HPSL's own @ifdefs never check for - alias them
				// here. Found live (real headless start_map run against
				// 00_01_apartment.hpm, see PORTING_NOTES.md "SOMA" section):
				// cMaterialType_SolidDiffuse::LoadSpecificData()
				// unconditionally sets "UseDepth" whenever G-buffer solid
				// rendering needs linear depth written (which is always),
				// but deferred_base_vtx.hpsl/deferred_gbuffer_solid_frag.hpsl
				// gate their own (also always-written) linear-depth
				// interpolant behind "UseLinearDepth" instead - without this
				// alias, the fragment shader's body unconditionally reads
				// px_fLinearDepth while neither shader ever declares it,
				// since the combo variable their @ifdef actually checks for
				// is never set.
				if(apVarContainer->Get("UseDepth") != NULL)
					apVarContainer->Add("UseLinearDepth");

				/////////////////////////////////
				// "UseExtendedArgs" gates deferred_base_vtx.hpsl's legacy
				// cVertexArguments cBuffer's extra members (afInvFarPlane/
				// afT/sway/force-field/scrolling-noise/soft-particle/
				// instancing-offset uniforms) - the *only* place this name
				// appears anywhere in the real .hpsl corpus (confirmed via
				// grep across every file), and every one of those members
				// is a plain declaration only read by code that's itself
				// separately gated behind its own specific combo variable
				// (UseSway/UseScrollingNoise/UseSoftParticle/...) - so
				// there's no downside to always declaring them, unused or
				// not. Unlike "UseDepth"/"UseLinearDepth" above, this has
				// no HPL2-legacy equivalent flag to alias from at all (it's
				// pure HPSL-side cBuffer-visibility bookkeeping with no
				// Dark-Descent-shader analog) - just always turn it on for
				// every HPSL compile. Found live the same way as the
				// UseDepth/UseLinearDepth alias above: turning that alias
				// on newly activated deferred_base_vtx.hpsl's
				// "px_fLinearDepth = ... * afInvFarPlane;" line, which
				// reads afInvFarPlane from inside this gated block.
				apVarContainer->Add("UseExtendedArgs");
			}
			mpPreprocessParser->Parse(&sFileData, &sParsedOutput,apVarContainer,cString::GetFilePathW(sPath));

			/////////////////////////////////
			//HPSL -> GLSL fallback: same preprocessor as the .glsl path
			//above, transpiled the rest of the way by the registered
			//game-module callback.
			tString sHpslPreTranspile;
			if(bIsHpslFallback)
			{
				tString sGlsl, sTranspileError;
				sHpslPreTranspile = sParsedOutput;
				if(mpHpslTranspileCallback(sParsedOutput, aType, sGlsl, sTranspileError)==false)
				{
					Error("Couldn't transpile HPSL shader '%s' (from '%s'): %s\n",
						  asName.c_str(), sHpslName.c_str(), sTranspileError.c_str());
					EndLoad();
					return NULL;
				}
				sParsedOutput = sGlsl;
			}

			/////////////////////////////////
			//Compile
			pShader = mpLowLevelGraphics->CreateGpuShader(asName, aType);
			pShader->SetFullPath(sPath);

			if(pShader->CreateFromString(sParsedOutput.c_str())==false)
			{
				Error("Couldn't create program '%s'\n",asName.c_str());
				hplDelete(pShader);
				EndLoad();
				return NULL;
			}

			/////////////////////////////////
			//Sampler to texture units setup, if needed
			if(aType == eGpuShaderType_Fragment && pShader->SamplerNeedsTextureUnitSetup())
			{
				if(bIsHpslFallback) ApplyHpslTextureBindings(pShader, sHpslPreTranspile);

				tParseVarMap *pVarMap = mpPreprocessParser->GetParsingVarContainer()->GetMapPtr();
				tParseVarMapIt varIt = pVarMap->begin();
				for(; varIt != pVarMap->end(); ++varIt)
				{
					const tString& sVarName = varIt->first;
					const tString& sVarVal = varIt->second;
					if(sVarName == "") continue;

					tStringVec vStrings;
					tString sSepp = "_";
					cString::GetStringVec(sVarName,vStrings,&sSepp);
					if(vStrings.size()>=2 && vStrings[0]=="sampler")
					{
						int lUnit = cString::ToInt(sVarVal.c_str(), 0);
						
						pShader->AddSamplerUnit(vStrings[1], lUnit);
					}
					
				}
			}
		}
		/////////////////////////////////////////
		// Normal resource load
		else
		{
			tWString sPath;
			pShader = static_cast<iGpuShader*>(FindLoadedResource(asName,sPath));

			if(pShader==NULL && sPath!=_W(""))
			{
				pShader = mpLowLevelGraphics->CreateGpuShader(asName, aType);

				if(pShader->CreateFromFile(sPath)==false)
				{
					Error("Couldn't create program '%s'\n",asName.c_str());
					hplDelete(pShader);
					EndLoad();
					return NULL;
				}

				AddResource(pShader);
			}
			//////////////////////////////////////////////
			// HPSL -> GLSL fallback (see the apVarContainer branch above for
			// the full explanation) - only reachable when a game module has
			// registered a transpiler; Dark Descent/AMFP leave
			// mpHpslTranspileCallback NULL and never enter this block.
			else if(pShader==NULL && sPath==_W("") && mpHpslTranspileCallback)
			{
				tString sHpslName = cString::SetFileExt(asName, "hpsl");
				tWString sHpslPath = mpFileSearcher->GetFilePath(sHpslName);
				if(sHpslPath != _W(""))
				{
					tString sFileData;
					unsigned int lFileSize = cPlatform::GetFileSize(sHpslPath);
					sFileData.resize(lFileSize);
					cPlatform::CopyFileToBuffer(sHpslPath,&sFileData[0],lFileSize);

					// This branch has no caller-supplied cParserVarContainer
					// (that's what distinguishes it from the branch above),
					// so preprocess with an empty one - same convention
					// HpslTranspilerSelfTest.cpp uses.
					cParserVarContainer emptyVars;
					tString sParsedOutput;
					mpPreprocessParser->Parse(&sFileData, &sParsedOutput, &emptyVars, cString::GetFilePathW(sHpslPath));

					tString sGlsl, sTranspileError;
					if(mpHpslTranspileCallback(sParsedOutput, aType, sGlsl, sTranspileError))
					{
						pShader = mpLowLevelGraphics->CreateGpuShader(asName, aType);
						pShader->SetFullPath(sHpslPath);

						if(pShader->CreateFromString(sGlsl.c_str())==false)
						{
							Error("Couldn't create program '%s' (from transpiled HPSL '%s')\n",
								  asName.c_str(), sHpslName.c_str());
							hplDelete(pShader);
							pShader = NULL;
						}
						else
						{
							if(aType == eGpuShaderType_Fragment && pShader->SamplerNeedsTextureUnitSetup())
								ApplyHpslTextureBindings(pShader, sParsedOutput);

							AddResource(pShader);
						}
					}
					else
					{
						Error("Couldn't transpile HPSL shader '%s' (from '%s'): %s\n",
							  asName.c_str(), sHpslName.c_str(), sTranspileError.c_str());
					}
				}
			}

			if(pShader)pShader->IncUserCount();
			else Error("Couldn't load program '%s'\n",asName.c_str());
		}
		
		
		EndLoad();
		return pShader;
     }

	//-----------------------------------------------------------------------

	void cGpuShaderManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cGpuShaderManager::Destroy(iResourceBase* apResource)
	{
		apResource->DecUserCount();

		if(apResource->HasUsers()==false){
			RemoveResource(apResource);
			hplDelete(apResource);
		}
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
}
