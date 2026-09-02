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

#ifdef WIN32
#include <io.h>
#endif

namespace hpl {

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
			mpPreprocessParser->Parse(&sFileData, &sParsedOutput,apVarContainer,cString::GetFilePathW(sPath));

			/////////////////////////////////
			//HPSL -> GLSL fallback: same preprocessor as the .glsl path
			//above, transpiled the rest of the way by the registered
			//game-module callback.
			if(bIsHpslFallback)
			{
				tString sGlsl, sTranspileError;
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
