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

#include "graphics/ProgramComboManager.h"

#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"

#include "graphics/Graphics.h"
#include "graphics/GPUShader.h"
#include "graphics/GPUProgram.h"
#include "graphics/LowLevelGraphics.h"

#include "resources/Resources.h"
#include "resources/TextureManager.h"
#include "resources/GpuShaderManager.h"

#include "math/Math.h"

#include "system/String.h"
#include "graphics/RustGraphicsH5.h"
#include "impl/RustBackends.h"

namespace hpl {

	const static bool gbLogStuff = false;

	static inline uintptr_t PtrId(const void *apPtr){ return (uintptr_t)apPtr; }

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cProgramComboManager::cProgramComboManager(const tString& asName,cGraphics *apGraphics, cResources *apResources, int alNumOfMainModes)
	{
		mpGraphics = apGraphics;
		mpResources = apResources;

		msName = asName;

		mlNumOfMainModes = alNumOfMainModes;
		
		mvCombinationNum.resize(mlNumOfMainModes);
		
		mpCombo = ohpl_combo_new(mlNumOfMainModes);
		mpSettings = OhplBackendRustRequested() ? ohpl_h5_combo_new((const unsigned char*)msName.data(), msName.size(), mlNumOfMainModes > 0 ? mlNumOfMainModes : 0) : NULL;

		mvVarLists.resize(mlNumOfMainModes);
		mvSettings.resize(mlNumOfMainModes);
	}

	cProgramComboManager::~cProgramComboManager()
	{
		DestroyShadersAndPrograms();
		ohpl_combo_free(mpCombo);
		ohpl_h5_combo_free(mpSettings);
	}

	void cProgramComboManager::SetName(const tString& asName)
	{
		msName = asName;
		if(mpSettings) ohpl_h5_combo_set_name(mpSettings, (const unsigned char*)msName.data(), msName.size());
	}

	static void CollectBytes(void *apCtx, const unsigned char *apData, size_t alLen)
	{
		((tString*)apCtx)->assign((const char*)apData, alLen);
	}

	static void CollectVar(void *apCtx, const unsigned char *apName, size_t alNameLen, const unsigned char *apVal, size_t alValLen)
	{
		((cParserVarContainer*)apCtx)->Add(tString((const char*)apName, alNameLen), tString((const char*)apVal, alValLen));
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//--------------------------------------------------------------------------

	iGpuProgram* cProgramComboManager::GenerateProgram(int alMainMode, int alFlags)
	{
		uintptr_t lFound = 0;
		if(ohpl_combo_program_acquire(mpCombo, alMainMode, alFlags, &lFound))
		{
			return reinterpret_cast<iGpuProgram*>(lFound);
		}

		cProgramComboSettings& comboSettings = mvSettings[alMainMode];
		tString sProgramName = GenerateProgramName(alMainMode, alFlags);

		tString sVtxName = comboSettings.msVtxShader, sFragName = comboSettings.msFragShader;
		if(mpSettings)
		{
			ohpl_h5_combo_shader_file(mpSettings, alMainMode, kPC_VertexBit, CollectBytes, &sVtxName);
			ohpl_h5_combo_shader_file(mpSettings, alMainMode, kPC_FragmentBit, CollectBytes, &sFragName);
		}

		////////////////////////////////////
		// Create (or get) shaders
		iGpuShader *pVtxShader = GetShaderForCombo(alMainMode, alFlags, sVtxName, kPC_VertexBit);
		iGpuShader *pFragShader = GetShaderForCombo(alMainMode, alFlags, sFragName, kPC_FragmentBit);

		////////////////////////////////////
		// Create program
		iGpuProgram* pProgram = NULL;

		if(pVtxShader && pFragShader)
			pProgram = CreateProgramFromShaders(sProgramName, pVtxShader, pFragShader,false);

		ohpl_combo_program_insert(mpCombo, alMainMode, alFlags, PtrId(pProgram));

		////////////////////////////////////
		// Init variables
		if(pProgram)
		{
			pProgram->SetUserId(alFlags);

			tProgramComboVariableListIt it = mvVarLists[alMainMode].begin();
			tProgramComboVariableListIt endIt = mvVarLists[alMainMode].end();
			for(; it != endIt; ++it)
			{
				cProgramComboVariable &var = *it;
				pProgram->GetVariableAsId(var.msVarName,var.mlId);
			}
		}

		return pProgram;
	}

	//--------------------------------------------------------------------------

	
	void cProgramComboManager::SetupGenerateProgramData(int alMainMode, const tString &asModeName,
														const tString &asVtxShaderName, const tString &asFragShaderName,
														cProgramComboFeature *apFeatures, int alFeatureNum, 
														cParserVarContainer &avDefaultVars)
	{
		////////////////////////////////
		//Set up variables and containers
		std::vector<unsigned> vStages(alFeatureNum > 0 ? alFeatureNum : 0);
		for(int i=0;i<alFeatureNum; ++i)
			vStages[i] = apFeatures[i].mlShaders;
		mvCombinationNum[alMainMode] = ohpl_combo_setup(mpCombo, alMainMode, vStages.data(), vStages.size());
				
		if(mpSettings)
		{
			ohpl_h5_combo_setup_mode(mpSettings, alMainMode, (const unsigned char*)asModeName.data(), asModeName.size(),
									 (const unsigned char*)asVtxShaderName.data(), asVtxShaderName.size(),
									 (const unsigned char*)asFragShaderName.data(), asFragShaderName.size());
			for(int i=0;i<alFeatureNum; ++i)
				ohpl_h5_combo_add_feature(mpSettings, alMainMode, apFeatures[i].mlShaders,
										  (const unsigned char*)apFeatures[i].msVariable.data(), apFeatures[i].msVariable.size());
			tParseVarMap* pRustVars = avDefaultVars.GetMapPtr();
			for(tParseVarMapIt it = pRustVars->begin(); it != pRustVars->end(); ++it)
				ohpl_h5_combo_add_default(mpSettings, alMainMode, (const unsigned char*)it->first.data(), it->first.size(),
										  (const unsigned char*)it->second.data(), it->second.size());
		}

		////////////////////////////////
		//Save settings
		cProgramComboSettings& comboSettings = mvSettings[alMainMode];
		comboSettings.msName = asModeName;
		comboSettings.msVtxShader = asVtxShaderName;
		comboSettings.msFragShader = asFragShaderName;
		
		comboSettings.mvFeatures.resize(alFeatureNum);
		for(int i=0;i<alFeatureNum; ++i)
			comboSettings.mvFeatures[i] = apFeatures[i];		
		
		tParseVarMap* pVarMap = avDefaultVars.GetMapPtr();
		comboSettings.mvDefaultVars.reserve(pVarMap->size());

		tParseVarMapIt it = pVarMap->begin();
		for(; it != pVarMap->end(); ++it)
		{
			comboSettings.mvDefaultVars.push_back(cProgramComboSettingsVar(it->first, it->second));
		}
	}
	
	//-----------------------------------------------------------------------

	void cProgramComboManager::AddGenerateProgramVariableId(const tString& asVarName, int alId, int alMainMode)
	{
		mvVarLists[alMainMode].push_back(cProgramComboVariable(asVarName, alId));
	}

	//-----------------------------------------------------------------------

	void cProgramComboManager::DestroyGeneratedProgram(int alMainMode, iGpuProgram* apProgram)
	{
		uintptr_t lHandle = 0;
		int lResult = ohpl_combo_program_release(mpCombo, alMainMode, apProgram->GetUserId(), &lHandle);
		if(lResult == 0)
		{
			Error("Could not find and destroy generated program: %s\n", apProgram->GetName().c_str());
			return;
		}

		/////////////////////////
		//Destroy if the user count reached 0
		if(lResult == 2)
		{
			iGpuProgram *pProgram = reinterpret_cast<iGpuProgram*>(lHandle);
			iGpuShader *pVtxShader = pProgram->GetShader(eGpuShaderType_Vertex);
			iGpuShader *pFragShader = pProgram->GetShader(eGpuShaderType_Fragment);

			/////////////////////////
			//Destroy program
			if(pProgram) hplDelete(pProgram);

			/////////////////////////
			//Destroy shaders (must be done after program deletion!)
			if(pVtxShader)
				DestroyGeneratedShader(alMainMode,pVtxShader , eGpuShaderType_Vertex);
			if(pFragShader)
				DestroyGeneratedShader(alMainMode,pFragShader , eGpuShaderType_Fragment);
		}
	}

	//-----------------------------------------------------------------------

	void cProgramComboManager::DestroyGeneratedShader(int alMainMode, iGpuShader* apShader, eGpuShaderType aType)
	{
		uintptr_t lHandle = 0;
		int lResult = ohpl_combo_shader_release(mpCombo, alMainMode, aType== eGpuShaderType_Vertex ? kPC_VertexBit : kPC_FragmentBit, apShader->GetUserId(), &lHandle);
		if(lResult == 0)
		{
			Error("Could not find and destroy generated shader: '%s' with id %d in combo manager '%s'\n", apShader->GetName().c_str(), apShader->GetUserId(), msName.c_str());
			return;
		}

		if(lResult == 2)
		{
			iGpuShader *pShader = reinterpret_cast<iGpuShader*>(lHandle);
			if(pShader) mpResources->GetGpuShaderManager()->Destroy(pShader);
		}
	}

	//-----------------------------------------------------------------------

	iGpuProgram *cProgramComboManager::CreateProgram(const tString& asName, bool abAddtoList)
	{
		iGpuProgram *pProgram = mpGraphics->GetLowLevel()->CreateGpuProgram(asName);
		pProgram->SetAutoDestroyShaders(false);
		if(abAddtoList)
		{
			mlstExtraPrograms.push_back(pProgram);
		}

		return pProgram;
	}

	//-----------------------------------------------------------------------

	iGpuShader *cProgramComboManager::CreateShader(const tString& asName,eGpuShaderType aType, cParserVarContainer *apVars, bool abAddtoList)
	{
		iGpuShader *pShader = mpResources->GetGpuShaderManager()->CreateShader(asName,aType,apVars);
		if(pShader == NULL)
		{
			Error("Could not load material '%s' shader '%s'\n",msName.c_str(),asName.c_str());
			return NULL;
		}

		if(pShader && abAddtoList) 
		{
			mlstExtraShaders.push_back(pShader);
		}

		return pShader;
	}

	//-----------------------------------------------------------------------

	void cProgramComboManager::DestroyShader(iGpuShader * apShader)
	{
		if(apShader == NULL) return;

		STLFindAndRemove(mlstExtraShaders, apShader);
		
		mpResources->GetGpuShaderManager()->Destroy(apShader);
	}

	//-----------------------------------------------------------------------

	void cProgramComboManager::DestroyProgram(iGpuProgram * apProgram)
	{
		STLFindAndDelete(mlstExtraPrograms, apProgram);
	}

	//-----------------------------------------------------------------------

	iGpuProgram* cProgramComboManager::CreateProgramFromShaders(const tString &asProgramName,
																const tString &asVtxShaderName, 
																const tString &asFragShaderName,
																cParserVarContainer *apVars,
																bool abAddtoList)
	{
		iGpuProgram *pProgram;
		iGpuShader *pVtxShader, *pFragShader;

		pVtxShader = CreateShader(asVtxShaderName,eGpuShaderType_Vertex,apVars, abAddtoList);
		if(pVtxShader == NULL) return NULL;

		pFragShader = CreateShader(asFragShaderName,eGpuShaderType_Fragment,apVars, abAddtoList);
		if(pFragShader == NULL){
			DestroyShader(pVtxShader);
			return NULL;
		}

		pProgram = CreateProgram(asProgramName, abAddtoList);
		pProgram->SetShader(eGpuShaderType_Vertex, pVtxShader);
		pProgram->SetShader(eGpuShaderType_Fragment, pFragShader);

		if(pProgram->Link()==false)
		{
			Error("Could not link material '%s' program %s!\n",msName.c_str(),asProgramName.c_str());
			//////////////////
			// Remove from list and destroy
			if(abAddtoList)
			{
				DestroyShader(pVtxShader);
				DestroyShader(pFragShader);
				DestroyProgram(pProgram);
			}
			//////////////////
			// Just destroy
			else
			{
				mpResources->GetGpuShaderManager()->Destroy(pVtxShader);
				mpResources->GetGpuShaderManager()->Destroy(pFragShader);
				hplDelete(pProgram);		
			}

			return NULL;
		}

		if(abAddtoList==false)
		{
			pProgram->SetAutoDestroyShaders(true);
			pProgram->SetResources(mpResources);
		}

		return pProgram;
	}

	//-----------------------------------------------------------------------

	iGpuProgram* cProgramComboManager::CreateProgramFromShaders(const tString &asProgramName, iGpuShader *apVtxShader,iGpuShader *apFragShader, 
																bool abAddtoList)
	{
		iGpuProgram *pProgram;

		pProgram = CreateProgram(asProgramName, abAddtoList);
		pProgram->SetShader(eGpuShaderType_Vertex, apVtxShader);
		pProgram->SetShader(eGpuShaderType_Fragment, apFragShader);

		if(pProgram->Link()==false)
		{
			Error("Could not link program '%s' in ProgramComboManager!\n",asProgramName.c_str());
			if(abAddtoList)
				DestroyProgram(pProgram);
			else
				hplDelete(pProgram);
			return NULL;
		}

		return pProgram;
	}

	
	//-----------------------------------------------------------------------

	void cProgramComboManager::DestroyShadersAndPrograms()
	{
		for(int rmode =0; rmode < mlNumOfMainModes; ++rmode)
		{
			///////////////////////////////
			// Destroy Generated Content

			//Programs
			uintptr_t lHandle = 0;
			while(ohpl_combo_pop_program(mpCombo, rmode, &lHandle))
			{
				if(lHandle) hplDelete(reinterpret_cast<iGpuProgram*>(lHandle));
			}

			//Shaders (must be after programs!)
			while(ohpl_combo_pop_shader(mpCombo, rmode, kPC_VertexBit, &lHandle))
				if(lHandle)	mpResources->GetGpuShaderManager()->Destroy(reinterpret_cast<iGpuShader*>(lHandle));

			while(ohpl_combo_pop_shader(mpCombo, rmode, kPC_FragmentBit, &lHandle))
				if(lHandle)	mpResources->GetGpuShaderManager()->Destroy(reinterpret_cast<iGpuShader*>(lHandle));

			mvVarLists[rmode].clear();

			///////////////////////////////
			// Destroy Extra Content
			
			//Programs
			STLDeleteAll(mlstExtraPrograms);

			//Shaders (must be after programs!)
			for(tGpuShaderListIt it = mlstExtraShaders.begin(); it!= mlstExtraShaders.end(); ++it)
			{
				mpResources->GetGpuShaderManager()->Destroy(*it);	
			}
			mlstExtraShaders.clear();
		}
	}

	//--------------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//--------------------------------------------------------------------------


	tString cProgramComboManager::GenerateProgramName(int alMainMode, int alBitFlags)
	{
		if(mpSettings)
		{
			tString sRust;
			ohpl_h5_combo_program_name(mpSettings, alMainMode, alBitFlags, CollectBytes, &sRust);
			return sRust;
		}

		cProgramComboSettings& settings = mvSettings[alMainMode];

		tString sName = msName + "_" + settings.msName;
		for(size_t i=0; i<settings.mvFeatures.size(); ++i)
		{
			if(alBitFlags & cMath::GetFlagBit((int)i)) 
			{
				sName += "_" + settings.mvFeatures[i].msVariable;
			}
		}

		return sName;
	}

	//--------------------------------------------------------------------------


	iGpuShader* cProgramComboManager::GetShaderForCombo(int alMainMode, int alBitFlags, const tString& asShaderName, tFlag aShaderType)
	{
		cProgramComboSettings &comboSettings = mvSettings[alMainMode];

		//////////////////////////////
		// Calculate the valid bit flags for current shader type
		// Valid means that only bits use the correct shader type is part of bit string.
		int lValidBits = (int)ohpl_combo_valid_bits(mpCombo, alMainMode, alBitFlags, aShaderType);

		//////////////////////////////
		// Check if program exists and if so return it
		uintptr_t lFound = 0;
		if(ohpl_combo_shader_acquire(mpCombo, alMainMode, aShaderType, lValidBits, &lFound))
		{
			return reinterpret_cast<iGpuShader*>(lFound);
		}

		//////////////////////////////
		// Create program, add to set and return it
		iGpuShader* pShader = mpSettings ? CreateShaderFromRustVars(asShaderName, alMainMode, aShaderType, alBitFlags) :
						CreateShaderFromFeatures(	asShaderName, aShaderType, alBitFlags,
														comboSettings.mvFeatures.empty() ? NULL : &comboSettings.mvFeatures[0], (int)comboSettings.mvFeatures.size(),
														comboSettings.mvDefaultVars.empty() ? NULL : &comboSettings.mvDefaultVars[0], (int)comboSettings.mvDefaultVars.size());
		ohpl_combo_shader_insert(mpCombo, alMainMode, aShaderType, lValidBits, PtrId(pShader));

		if(pShader) pShader->SetUserId(lValidBits);

		return pShader;
	}

	//--------------------------------------------------------------------

	iGpuShader* cProgramComboManager::CreateShaderFromRustVars(const tString& asShaderFile, int alMainMode, tFlag aShaderType, int alBitFlags)
	{
		cParserVarContainer vars;
		ohpl_h5_combo_shader_vars(mpSettings, alMainMode, alBitFlags, aShaderType, CollectVar, &vars);
		eGpuShaderType shaderType = aShaderType == kPC_VertexBit ? eGpuShaderType_Vertex : eGpuShaderType_Fragment;
		return CreateShader(asShaderFile,shaderType,&vars,false);
	}

	//--------------------------------------------------------------------

	iGpuShader* cProgramComboManager::CreateShaderFromFeatures(	const tString& asShaderFile, tFlag aShaderType,
																int alBitFlags, cProgramComboFeature* apFeatures, int alFeatureNum, 
																cProgramComboSettingsVar *apDefaultVars, int alDefaultVarsNum)
	{
		cParserVarContainer vars;

		///////////////////////////
		//Add variables to variable container
		for(int lFeature=0; lFeature<alFeatureNum; ++lFeature)
		{
			//This check if feature j in in current flag
			if( alBitFlags & cMath::GetFlagBit(lFeature))
			{
				if(apFeatures[lFeature].mlShaders & aShaderType)
				{
					vars.Add(apFeatures[lFeature].msVariable);
				}
			}
		}
		
		/////////////////////////
		//Add the default variables,
		for(int i=0; i<alDefaultVarsNum; ++i)
		{
			vars.Add(apDefaultVars[i].msName, apDefaultVars[i].msValue);
		}

		/////////////////////////
		//Create the shader
		eGpuShaderType shaderType = aShaderType == kPC_VertexBit ? eGpuShaderType_Vertex : eGpuShaderType_Fragment;
		return CreateShader(asShaderFile,shaderType,&vars,false);
	}

	//--------------------------------------------------------------------------

	
	

}
