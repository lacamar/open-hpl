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

#include "graphics/MaterialType_BasicSolid.h"

#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"

#include "resources/Resources.h"
#include "resources/TextureManager.h"
#include "resources/GpuShaderManager.h"

#include "math/Frustum.h"
#include "math/Math.h"

#include "graphics/Graphics.h"
#include "graphics/GPUShader.h"
#include "graphics/GPUProgram.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Renderer.h"
#include "graphics/Material.h"
#include "graphics/ProgramComboManager.h"
#include "graphics/Renderable.h"
#include "graphics/RendererDeferred.h"
#include "scene/World.h"
#include "scene/ForceField.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// DEFINES
	//////////////////////////////////////////////////////////////////////////
	
	//------------------------------
	// Variables
	//------------------------------
	#define kVar_afInvFarPlane					0
	#define kVar_avHeightMapScaleAndBias		1
	#define kVar_a_mtxUV						2	
	#define kVar_afColorMul						3
	#define kVar_afDissolveAmount				4
	#define kVar_avFrenselBiasPow				5
	#define kVar_a_mtxInvViewRotation			6
	#define kVar_avColorMul						7
	#define kVar_avIlluminationMul				8
	#define kVar_afT							9
	#define kVar_avSwayProperties				10
	#define kVar_avSwayOctavesMul				11
	#define kVar_afSwayYFreqMul					12
	#define kVar_avSwaySingleDirection			13
	#define kVar_avSwaySinglesampleDirection	14
	#define kVar_a_mtxModel						15
	#define kVar_afFarPlane						16
	#define kVar_avDetailProperties				17
	#define kVar_avDetailWeights				18
	#define kVar_a_mtxNormalFrag				19
	#define kVar_afBlendHardness				20
	#define kVar_afBlendHardnessVtx				21
	#define kVar_afNormalMapBlendImpact			22
	#define kVar_avTextureScale					23
	#define kVar_avForceFieldPos0				24


	//------------------------------
	//Diffuse Features and data
	//------------------------------
	#define eFeature_Diffuse_NormalMaps		eFlagBit_0
	#define eFeature_Diffuse_Specular		eFlagBit_1
	#define eFeature_Diffuse_Parallax		eFlagBit_2
	#define eFeature_Diffuse_UvAnimation	eFlagBit_3
	#define eFeature_Diffuse_Skeleton		eFlagBit_4
	#define eFeature_Diffuse_EnvMap			eFlagBit_5
	#define eFeature_Diffuse_CubeMapAlpha	eFlagBit_6
	#define eFeature_Diffuse_Sway			eFlagBit_7
	#define eFeature_Diffuse_SwaySingleDir	eFlagBit_8
	#define eFeature_Diffuse_SwayMap		eFlagBit_9
	#define eFeature_Diffuse_DetailDiffuse	eFlagBit_10
	#define eFeature_Diffuse_DetailNormal	eFlagBit_11
	#define eFeature_Diffuse_Translucency	eFlagBit_12
	#define eFeature_Diffuse_ForceFields	eFlagBit_13
		
	#define kDiffuseFeatureNum 14

	static cProgramComboFeature vDiffuseFeatureVec[] =
	{
		cProgramComboFeature("UseNormalMapping", kPC_VertexBit | kPC_FragmentBit),
		cProgramComboFeature("UseSpecular", kPC_FragmentBit),		
		cProgramComboFeature("UseParallax", kPC_VertexBit | kPC_FragmentBit, eFeature_Diffuse_NormalMaps),							
		cProgramComboFeature("UseUvAnimation", kPC_VertexBit),							
		cProgramComboFeature("UseSkeleton",	kPC_VertexBit),	
		cProgramComboFeature("UseEnvMap", kPC_VertexBit | kPC_FragmentBit),
		cProgramComboFeature("UseCubeMapAlpha", kPC_FragmentBit),
		cProgramComboFeature("UseSway", kPC_VertexBit),
		cProgramComboFeature("UseSwaySingleDir", kPC_VertexBit),
		cProgramComboFeature("UseSwayMap", kPC_VertexBit),
		cProgramComboFeature("UseDetailDiffuse", kPC_FragmentBit),
		cProgramComboFeature("UseDetailNormal", kPC_FragmentBit, eFeature_Diffuse_NormalMaps),
		cProgramComboFeature("UseTranslucency", kPC_FragmentBit),
		cProgramComboFeature("UseFourForceFields", kPC_VertexBit),
	};

	//------------------------------
	//Illumination Features and data
	//------------------------------
	#define eFeature_Illum_UvAnimation	eFlagBit_0
	#define eFeature_Illum_Skeleton		eFlagBit_1

	#define kIllumFeatureNum 2

	cProgramComboFeature vIllumFeatureVec[] =
	{
		cProgramComboFeature("UseUvAnimation", kPC_VertexBit),							
		cProgramComboFeature("UseSkeleton",	kPC_VertexBit),							
	};

	//------------------------------
	//Z Features and data
	//------------------------------
	#define eFeature_Z_UseAlpha					eFlagBit_0
	#define eFeature_Z_UvAnimation				eFlagBit_1
	#define eFeature_Z_Dissolve					eFlagBit_2
	#define eFeature_Z_DissolveAlpha			eFlagBit_3
	#define eFeature_Z_UseAlphaDissolveFilter	eFlagBit_4
	#define eFeature_Z_Sway						eFlagBit_5
	#define eFeature_Z_SwaySingleDir			eFlagBit_6
	#define eFeature_Z_SwayMap					eFlagBit_7
	#define eFeature_Z_ForceFields				eFlagBit_8
	
	#define kZFeatureNum 9

	cProgramComboFeature vZFeatureVec[] =
	{
			cProgramComboFeature("UseAlphaMap",					kPC_FragmentBit),
			cProgramComboFeature("UseUvAnimation",				kPC_VertexBit),
			cProgramComboFeature("UseDissolve",					kPC_FragmentBit),
			cProgramComboFeature("UseDissolveAlphaMap",			kPC_FragmentBit),
			cProgramComboFeature("UseAlphaUseDissolveFilter",	kPC_FragmentBit),
			cProgramComboFeature("UseSway",						kPC_VertexBit),
			cProgramComboFeature("UseSwaySingleDir",			kPC_VertexBit),
			cProgramComboFeature("UseSwayMap",					kPC_VertexBit),
			cProgramComboFeature("UseFourForceFields",			kPC_VertexBit),
	};

	//------------------------------
	//Projected UV Features and data
	//------------------------------
	#define eFeature_PUV_NormalMaps			eFlagBit_0
	#define eFeature_PUV_Specular			eFlagBit_1
	#define eFeature_PUV_BottomTexture		eFlagBit_2
	#define eFeature_PUV_DiffuseMap			eFlagBit_3
	#define eFeature_PUV_NMapBlendWeight	eFlagBit_4

	#define kProjectedUVFeatureNum 5

	static cProgramComboFeature vProjectedUVFeatureVec[] =
	{
		cProgramComboFeature("UseNormalMapping", kPC_VertexBit | kPC_FragmentBit),
		cProgramComboFeature("UseSpecular", kPC_FragmentBit),
		cProgramComboFeature("UseBottomTexture", kPC_FragmentBit),
		cProgramComboFeature("UseDiffuseMap", kPC_FragmentBit),
		cProgramComboFeature("UseNormalMapBlendWeight", kPC_FragmentBit, eFeature_PUV_NormalMaps),
	};

	//------------------------------

	static void AddSwayVariableIds(cProgramComboManager *apManager, int alMode)
	{
		apManager->AddGenerateProgramVariableId("afT", kVar_afT, alMode);
		apManager->AddGenerateProgramVariableId("avSwayProperties", kVar_avSwayProperties, alMode);
		apManager->AddGenerateProgramVariableId("avSwayOctavesMul", kVar_avSwayOctavesMul, alMode);
		apManager->AddGenerateProgramVariableId("afSwayYFreqMul", kVar_afSwayYFreqMul, alMode);
		apManager->AddGenerateProgramVariableId("avSwaySingleDirection", kVar_avSwaySingleDirection, alMode);
		apManager->AddGenerateProgramVariableId("avSwaySinglesampleDirection", kVar_avSwaySinglesampleDirection, alMode);
		apManager->AddGenerateProgramVariableId("a_mtxModel", kVar_a_mtxModel, alMode);
		iMaterialType::AddForceFieldVariableIds(apManager, kVar_avForceFieldPos0, alMode);
	}

	static tFlag SwayFlags(cMaterialType_SolidDiffuse_Vars *apVars, cMaterial *apMaterial, tFlag alSway, tFlag alSingleDir, tFlag alMap, tFlag alForceFields)
	{
		if(apVars->mbSwayActive == false) return 0;
		tFlag lFlags = alSway;
		if(apVars->mbSwaySingleDir) lFlags |= alSingleDir;
		if(apVars->mbSwayForceFieldAffected) lFlags |= alForceFields;
		if(apMaterial->GetTexture(eMaterialTexture_Height)) lFlags |= alMap;
		return lFlags;
	}

	//------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// STATIC OBJECTS
	//////////////////////////////////////////////////////////////////////////

	//--------------------------------------------------------------------------

	bool iMaterialType_SolidBase::mbGlobalDataCreated = false;
	cProgramComboManager* iMaterialType_SolidBase::mpGlobalProgramManager;

	float iMaterialType_SolidBase::mfVirtualPositionAddScale = 0.03f;

	//--------------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// SOLID BASE
	//////////////////////////////////////////////////////////////////////////

	//--------------------------------------------------------------------------

	iMaterialType_SolidBase::iMaterialType_SolidBase(cGraphics *apGraphics, cResources *apResources) : iMaterialType(apGraphics,apResources)
	{
		mbIsGlobalDataCreator = false;
	}

	//--------------------------------------------------------------------------

	iMaterialType_SolidBase::~iMaterialType_SolidBase()
	{

	}

	//--------------------------------------------------------------------------

	void iMaterialType_SolidBase::DestroyProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
	{
		/////////////////////////////
		// Remove from global manager
		if(aRenderMode == eMaterialRenderMode_Z || aRenderMode ==  eMaterialRenderMode_Z_Dissolve)
		{
			mpGlobalProgramManager->DestroyGeneratedProgram(eMaterialRenderMode_Z, apProgram);
		}
		/////////////////////////////
		// Remove from normal manager
		else
		{
			mpProgramManager->DestroyGeneratedProgram(aRenderMode, apProgram);
		}
	}
	
	//--------------------------------------------------------------------------

	void iMaterialType_SolidBase::CreateGlobalPrograms()
	{
		if(mbGlobalDataCreated) return;

		mbGlobalDataCreated = true;
		mbIsGlobalDataCreator = true;

		/////////////////////////////
		//Load programs
		//This makes this material's program manager responsible for managing the global programs!
		cParserVarContainer defaultVars;
		defaultVars.Add("UseUv");
		
		mpProgramManager->SetupGenerateProgramData(	eMaterialRenderMode_Z,"Z","deferred_base_vtx.glsl", "deferred_base_frag.glsl", 
													vZFeatureVec,kZFeatureNum, defaultVars);
		
		mpProgramManager->AddGenerateProgramVariableId("a_mtxUV",kVar_a_mtxUV,eMaterialRenderMode_Z);
		mpProgramManager->AddGenerateProgramVariableId("afDissolveAmount",kVar_afDissolveAmount,eMaterialRenderMode_Z);
		AddSwayVariableIds(mpProgramManager, eMaterialRenderMode_Z);

		mpGlobalProgramManager = mpProgramManager;
	}

	//--------------------------------------------------------------------------

	void iMaterialType_SolidBase::LoadData()
	{
		/////////////////////////////
		//Global data init (that is shared between Solid materials)
		CreateGlobalPrograms();

		//////////////
		// Create textures
		mpDissolveTexture = mpResources->GetTextureManager()->Create2D("core_dissolve.tga",true);


		LoadSpecificData();
	}

	//--------------------------------------------------------------------------

	void iMaterialType_SolidBase::DestroyData()
	{
		if(mpDissolveTexture) mpResources->GetTextureManager()->Destroy(mpDissolveTexture);

		//If this instace was global data creator, then it needs to be recreated.
		if(mbIsGlobalDataCreator)
		{
			mbGlobalDataCreated = false;
		}
		mpProgramManager->DestroyShadersAndPrograms();
	}

	//--------------------------------------------------------------------------

	void iMaterialType_SolidBase::LoadVariables(cMaterial *apMaterial, cResourceVarsObject *apVars)
	{

	}

	void iMaterialType_SolidBase::GetVariableValues(cMaterial *apMaterial, cResourceVarsObject *apVars)
	{

	}

	//--------------------------------------------------------------------------

	void iMaterialType_SolidBase::CompileMaterialSpecifics(cMaterial *apMaterial)
	{
		////////////////////////
		//If there is an alpha texture, set alpha mode to trans, else solid.
		if(apMaterial->GetTexture(eMaterialTexture_Alpha))
		{
			apMaterial->SetAlphaMode(eMaterialAlphaMode_Trans);
		}
		else
		{
			apMaterial->SetAlphaMode(eMaterialAlphaMode_Solid);
		}

		CompileSolidSpecifics(apMaterial);
	}

	//--------------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// SOLID DIFFUSE
	//////////////////////////////////////////////////////////////////////////
	
	//--------------------------------------------------------------------------
	
	cMaterialType_SolidDiffuse::cMaterialType_SolidDiffuse(cGraphics *apGraphics, cResources *apResources) : iMaterialType_SolidBase(apGraphics, apResources)
	{
		AddUsedTexture(eMaterialTexture_Diffuse);
		AddUsedTexture(eMaterialTexture_NMap);
		AddUsedTexture(eMaterialTexture_Alpha);
		AddUsedTexture(eMaterialTexture_Specular);
		AddUsedTexture(eMaterialTexture_Height);
		AddUsedTexture(eMaterialTexture_Illumination);
		AddUsedTexture(eMaterialTexture_DissolveAlpha);
		AddUsedTexture(eMaterialTexture_CubeMap);
		AddUsedTexture(eMaterialTexture_CubeMapAlpha);
		AddUsedTexture(eMaterialTexture_DetailDiffuse);
		AddUsedTexture(eMaterialTexture_DetailNMap);
		AddUsedTexture(eMaterialTexture_Translucency);

		mbHasTypeSpecifics[eMaterialRenderMode_Diffuse] = true;

		AddVarFloat("HeightMapScale", 0.05f, "");
		AddVarFloat("HeightMapBias", 0, "");
		AddVarFloat("FrenselBias", 0.2f, "Bias for Fresnel term. values: 0-1. Higher means that more of reflection is seen when looking straight at object.");
		AddVarFloat("FrenselPow", 8.0f, "The higher the 'sharper' the reflection is, meaning that it is only clearly seen at sharp angles.");
		AddVarBool("AlphaDissolveFilter", false, "If alpha values between 0 and 1 should be used and dissolve the texture. This can be useful for things like hair.");
	}
	
	//--------------------------------------------------------------------------

	cMaterialType_SolidDiffuse::~cMaterialType_SolidDiffuse()
	{
	}

	//--------------------------------------------------------------------------


	void cMaterialType_SolidDiffuse::LoadSpecificData()
	{
		/////////////////////////////
		//Load Diffuse programs
		cParserVarContainer defaultVars;
		defaultVars.Add("UseUv");
		defaultVars.Add("UseNormals");
		defaultVars.Add("UseDepth");
		defaultVars.Add("VirtualPositionAddScale",mfVirtualPositionAddScale);
		
		//Get the G-buffer type
		if(cRendererDeferred::GetGBufferType() == eDeferredGBuffer_32Bit)	defaultVars.Add("Deferred_32bit");
		else																defaultVars.Add("Deferred_64bit");

		//Set up number of gbuffer textures used
		if(cRendererDeferred::GetNumOfGBufferTextures() == 4)	defaultVars.Add("RenderTargets_4");
		else													defaultVars.Add("RenderTargets_3");

		//Set up relief mapping method
		if(	iRenderer::GetParallaxQuality() != eParallaxQuality_Low &&
			mpGraphics->GetLowLevel()->GetCaps(eGraphicCaps_ShaderModel_3)!=0) 
		{
			defaultVars.Add("ParallaxMethod_Relief");
		}
		else														
		{
			defaultVars.Add("ParallaxMethod_Simple");
		}

		
		
		if(cRendererDeferred::GetHdr())
		{
			defaultVars.Add("UseColor");
			defaultVars.Add("UseColorMul");
		}
		mpProgramManager->SetupGenerateProgramData(	eMaterialRenderMode_Diffuse,"Diffuse","deferred_base_vtx.glsl", "deferred_gbuffer_solid_frag.glsl", 
													vDiffuseFeatureVec,kDiffuseFeatureNum, defaultVars);

		/////////////////////////////
		//Load Illumination programs
		defaultVars.Clear();
		defaultVars.Add("UseUv");
		mpProgramManager->SetupGenerateProgramData(	eMaterialRenderMode_Illumination,"Illum","deferred_base_vtx.glsl", "deferred_illumination_frag.glsl", 
													vIllumFeatureVec,kIllumFeatureNum, defaultVars);

		
		////////////////////////////////
		//Set up variable ids
		mpProgramManager->AddGenerateProgramVariableId("afInvFarPlane",kVar_afInvFarPlane,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("avHeightMapScaleAndBias",kVar_avHeightMapScaleAndBias, eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("a_mtxUV",kVar_a_mtxUV,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("avFrenselBiasPow", kVar_avFrenselBiasPow,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("a_mtxInvViewRotation", kVar_a_mtxInvViewRotation,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("a_mtxInvView", kVar_a_mtxInvViewRotation,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("avColorMul", kVar_avColorMul,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("afFarPlane", kVar_afFarPlane,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("avDetailProperties", kVar_avDetailProperties,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("avDetailWeights", kVar_avDetailWeights,eMaterialRenderMode_Diffuse);
		AddSwayVariableIds(mpProgramManager, eMaterialRenderMode_Diffuse);

		mpProgramManager->AddGenerateProgramVariableId("a_mtxUV",kVar_a_mtxUV,eMaterialRenderMode_Illumination);
		mpProgramManager->AddGenerateProgramVariableId("afColorMul",kVar_afColorMul,eMaterialRenderMode_Illumination);
		mpProgramManager->AddGenerateProgramVariableId("avIlluminationMul",kVar_avIlluminationMul,eMaterialRenderMode_Illumination);
	}

	//--------------------------------------------------------------------------

	void cMaterialType_SolidDiffuse::CompileSolidSpecifics(cMaterial *apMaterial)
	{
		cMaterialType_SolidDiffuse_Vars *pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();

		//////////////////////////////////
		//Z specifics
		apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_Z_Dissolve,true);
		if(cRendererDeferred::GetHdr()) apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_Diffuse,true);
		apMaterial->SetUseAlphaDissolveFilter(pVars->mbAlphaDissolveFilter);
		if(pVars->mbSwayActive)
		{
			for(eMaterialRenderMode mode : {eMaterialRenderMode_Z, eMaterialRenderMode_Z_Dissolve, eMaterialRenderMode_Diffuse})
			{
				apMaterial->SetHasSpecificSettings(mode,true);
				apMaterial->SetHasObjectSpecificsSettings(mode,true);
			}
		}
		
		//////////////////////////////////
		//Normal map and height specifics
		if(apMaterial->GetTexture(eMaterialTexture_NMap))
		{
			if(apMaterial->GetTexture(eMaterialTexture_Height))
			{
				apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse,true);
			}
		}

		//////////////////////////////////
		//Uv animation specifics
		if(apMaterial->HasUvAnimation())
		{
			apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Z,true);
			apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse,true);
			apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Illumination,true);
		}

		//////////////////////////////////
		//Cubemap
		if(apMaterial->GetTexture(eMaterialTexture_CubeMap))
		{
			apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse,true);
		}

		//////////////////////////////////
		//Illuminations specifics
		if(apMaterial->GetTexture(eMaterialTexture_Illumination))
		{
			apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_Illumination,true);
		}
	}

	//--------------------------------------------------------------------------

	
	iTexture* cMaterialType_SolidDiffuse::GetTextureForUnit(cMaterial *apMaterial,eMaterialRenderMode aRenderMode, int alUnit)
	{
		cMaterialType_SolidDiffuse_Vars *pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();

		////////////////////////////
		//Z
		if(aRenderMode == eMaterialRenderMode_Z)
		{
			switch(alUnit)
			{
			case 0: return apMaterial->GetTexture(eMaterialTexture_Alpha);
			case 1: return mpDissolveTexture;
			case 3: return pVars->mbSwayActive ? apMaterial->GetTexture(eMaterialTexture_Height) : NULL;
			}
		}
		////////////////////////////
		//Z Dissolve
		else if(aRenderMode == eMaterialRenderMode_Z_Dissolve)
		{
			switch(alUnit)
			{
			case 0: return apMaterial->GetTexture(eMaterialTexture_Alpha);
			case 1: return mpDissolveTexture;
			case 2: return apMaterial->GetTexture(eMaterialTexture_DissolveAlpha);
			case 3: return pVars->mbSwayActive ? apMaterial->GetTexture(eMaterialTexture_Height) : NULL;
			}
		}
		////////////////////////////
		//Diffuse
		else if(aRenderMode == eMaterialRenderMode_Diffuse)
		{
			if(alUnit > 3 && cGpuShaderManager::IsHpsl())
			{
				switch(alUnit)
				{
				case 5: return apMaterial->GetTexture(eMaterialTexture_CubeMap);
				case 6: return apMaterial->GetTexture(eMaterialTexture_CubeMapAlpha);
				case 8: return apMaterial->GetTexture(eMaterialTexture_DetailDiffuse);
				case 9: return apMaterial->GetTexture(eMaterialTexture_NMap) ? apMaterial->GetTexture(eMaterialTexture_DetailNMap) : NULL;
				case 10: return apMaterial->GetTexture(eMaterialTexture_Translucency);
				}
				return NULL;
			}
			switch(alUnit)
			{
			case 0: return apMaterial->GetTexture(eMaterialTexture_Diffuse);
			case 1: return apMaterial->GetTexture(eMaterialTexture_NMap);
			case 2: return apMaterial->GetTexture(eMaterialTexture_Specular);
			case 3: return apMaterial->GetTexture(eMaterialTexture_Height);
			case 4: return apMaterial->GetTexture(eMaterialTexture_CubeMap);
			case 5: return apMaterial->GetTexture(eMaterialTexture_CubeMapAlpha);
			}
		}
		////////////////////////////
		//Illumination
		else if(aRenderMode == eMaterialRenderMode_Illumination)
		{
			switch(alUnit)
			{
			case 0: return apMaterial->GetTexture(eMaterialTexture_Illumination);
			}
		}

		return NULL;
	}
	//--------------------------------------------------------------------------

	iTexture* cMaterialType_SolidDiffuse::GetSpecialTexture(cMaterial *apMaterial, eMaterialRenderMode aRenderMode,iRenderer *apRenderer, int alUnit)
	{
		return NULL;
	}
	
	//--------------------------------------------------------------------------
	
	iGpuProgram* cMaterialType_SolidDiffuse::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		cMaterialType_SolidDiffuse_Vars *pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();

		////////////////////////////
		//Z
		if(aRenderMode == eMaterialRenderMode_Z)
		{
			tFlag lFlags =0;
			if(apMaterial->GetTexture(eMaterialTexture_Alpha))	lFlags |= eFeature_Z_UseAlpha;
			if(apMaterial->HasUvAnimation())					lFlags |= eFeature_Z_UvAnimation;
			if(pVars->mbAlphaDissolveFilter)					lFlags |= eFeature_Z_UseAlphaDissolveFilter;
			lFlags |= SwayFlags(pVars, apMaterial, eFeature_Z_Sway, eFeature_Z_SwaySingleDir, eFeature_Z_SwayMap, eFeature_Z_ForceFields);

			return mpGlobalProgramManager->GenerateProgram(eMaterialRenderMode_Z, lFlags);
		}
		////////////////////////////
		//Z Dissolve
		else if(aRenderMode == eMaterialRenderMode_Z_Dissolve)
		{
			tFlag lFlags =0;
			lFlags |= eFeature_Z_Dissolve;
			if(apMaterial->GetTexture(eMaterialTexture_Alpha))			lFlags |= eFeature_Z_UseAlpha;
			if(apMaterial->GetTexture(eMaterialTexture_DissolveAlpha))	lFlags |= eFeature_Z_DissolveAlpha;
			if(apMaterial->HasUvAnimation())							lFlags |= eFeature_Z_UvAnimation;
			if(pVars->mbAlphaDissolveFilter)							lFlags |= eFeature_Z_UseAlphaDissolveFilter;
			lFlags |= SwayFlags(pVars, apMaterial, eFeature_Z_Sway, eFeature_Z_SwaySingleDir, eFeature_Z_SwayMap, eFeature_Z_ForceFields);

			return mpGlobalProgramManager->GenerateProgram(eMaterialRenderMode_Z, lFlags);
		}
		////////////////////////////
		//Diffuse
		else if(aRenderMode == eMaterialRenderMode_Diffuse)
		{
			tFlag lFlags =0;
			if(apMaterial->GetTexture(eMaterialTexture_NMap))			lFlags |= eFeature_Diffuse_NormalMaps;
			if(apMaterial->GetTexture(eMaterialTexture_Specular))		lFlags |= eFeature_Diffuse_Specular;
			if(	apMaterial->GetTexture(eMaterialTexture_Height) && 
				iRenderer::GetParallaxEnabled() && pVars->mbSwayActive == false)	lFlags |= eFeature_Diffuse_Parallax;
			if(apMaterial->GetTexture(eMaterialTexture_CubeMap))
			{	
				lFlags |= eFeature_Diffuse_EnvMap;
				if(apMaterial->GetTexture(eMaterialTexture_CubeMapAlpha))	lFlags |= eFeature_Diffuse_CubeMapAlpha;
			}
			if(apMaterial->HasUvAnimation())							lFlags |= eFeature_Diffuse_UvAnimation;
			if(apMaterial->GetTexture(eMaterialTexture_DetailDiffuse))	lFlags |= eFeature_Diffuse_DetailDiffuse;
			if(apMaterial->GetTexture(eMaterialTexture_DetailNMap))		lFlags |= eFeature_Diffuse_DetailNormal;
			if(apMaterial->GetTexture(eMaterialTexture_Translucency))	lFlags |= eFeature_Diffuse_Translucency;
			lFlags |= SwayFlags(pVars, apMaterial, eFeature_Diffuse_Sway, eFeature_Diffuse_SwaySingleDir, eFeature_Diffuse_SwayMap, eFeature_Diffuse_ForceFields);

			return mpProgramManager->GenerateProgram(aRenderMode,lFlags);
		}
		////////////////////////////
		//Illumination
		else if(aRenderMode == eMaterialRenderMode_Illumination)
		{
			tFlag lFlags =0;
			if(apMaterial->HasUvAnimation())	lFlags |= eFeature_Illum_UvAnimation;

			return mpProgramManager->GenerateProgram(aRenderMode,lFlags);
		}

		return NULL;
	}

	//--------------------------------------------------------------------------

	void cMaterialType_SolidDiffuse::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderer *apRenderer)
	{
		////////////////////////////
		//Diffuse
		if(aRenderMode == eMaterialRenderMode_Diffuse)
		{
			cFrustum *pFrustum = apRenderer->GetCurrentFrustum();

			apProgram->SetFloat(kVar_afInvFarPlane, 1.0f/pFrustum->GetFarPlane());
			apProgram->SetFloat(kVar_afFarPlane, pFrustum->GetFarPlane());
		}
		
	}

	//--------------------------------------------------------------------------

	void cMaterialType_SolidDiffuse::SetupMaterialSpecificData(	eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,
																iRenderer *apRenderer)
	{
		if(	aRenderMode == eMaterialRenderMode_Diffuse || 
			aRenderMode == eMaterialRenderMode_Z || 
			aRenderMode == eMaterialRenderMode_Z_Dissolve || 
			aRenderMode == eMaterialRenderMode_Illumination)
		{
			/////////////////////////
			//UV Animation
			if(apMaterial->HasUvAnimation())
			{
				apProgram->SetMatrixf(kVar_a_mtxUV, apMaterial->GetUvMatrix());
			}

			cMaterialType_SolidDiffuse_Vars* pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();
			if(pVars->mbSwayActive && aRenderMode != eMaterialRenderMode_Illumination)
			{
				apProgram->SetFloat(kVar_afT, apRenderer->GetTimeCount());
				apProgram->SetVec3f(kVar_avSwayProperties, pVars->mvSwayProperties);
				apProgram->SetVec3f(kVar_avSwayOctavesMul, pVars->mvSwayOctaveMuls);
				apProgram->SetFloat(kVar_afSwayYFreqMul, pVars->mfSwayYFreqMul);
				apProgram->SetVec3f(kVar_avSwaySingleDirection, pVars->mvSwaySingleDir);
				apProgram->SetVec3f(kVar_avSwaySinglesampleDirection, pVars->mvSwaySingleSampleDir);
			}
			
			if(aRenderMode == eMaterialRenderMode_Diffuse)
			{
				/////////////////////////
				//Parallax
				if(apMaterial->GetTexture(eMaterialTexture_Height) && iRenderer::GetParallaxEnabled() && pVars->mbSwayActive == false)
				{
					apProgram->SetVec2f(kVar_avHeightMapScaleAndBias, pVars->mfHeightMapScale, pVars->mfHeightMapBias);
				}

				/////////////////////////
				//Cube Map
				if(apMaterial->GetTexture(eMaterialTexture_CubeMap))
				{
					apProgram->SetVec2f(kVar_avFrenselBiasPow, pVars->mfFrenselBias, pVars->mfFrenselPow);
					
					cMatrixf mtxInvView = apRenderer->GetCurrentFrustum()->GetViewMatrix().GetTranspose();
					apProgram->SetMatrixf(kVar_a_mtxInvViewRotation, mtxInvView.GetRotation());
				}

				if(apMaterial->GetTexture(eMaterialTexture_DetailDiffuse) || apMaterial->GetTexture(eMaterialTexture_DetailNMap))
				{
					const float *p = pVars->mvDetailProperties;
					apProgram->SetVec4f(kVar_avDetailProperties, p[0], p[1], p[2], p[3]);
					apProgram->SetVec3f(kVar_avDetailWeights, pVars->mvDetailWeights);
				}
			}
		}
	}
	
	//--------------------------------------------------------------------------

	void cMaterialType_SolidDiffuse::SetupObjectSpecificData(	eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable *apObject,
																iRenderer *apRenderer)
	{
		cMaterialType_SolidDiffuse_Vars *pVars = (cMaterialType_SolidDiffuse_Vars*)apObject->GetMaterial()->GetVars();
		if(pVars->mbSwayActive && aRenderMode != eMaterialRenderMode_Illumination)
		{
			cMatrixf *pMtx = apObject->GetModelMatrixPtr();
			apProgram->SetMatrixf(kVar_a_mtxModel, pMtx ? *pMtx : cMatrixf::Identity);
			if(pVars->mbSwayForceFieldAffected)
			{
				cForceField *vFields[4];
				cBoundingVolume *pBV = apObject->GetBoundingVolume();
				int lNum = apRenderer->GetCurrentWorld()->GetForceFields(pBV->GetMin(), pBV->GetMax(), vFields);
				iMaterialType::SetForceFieldVars(apProgram, kVar_avForceFieldPos0, vFields, lNum, pVars->mfSwayForceFieldMul, pVars->mfSwayForceFieldMax);
			}
		}
		
		////////////////////////////
		//Z Dissolve
		if(aRenderMode == eMaterialRenderMode_Z_Dissolve)
		{
			bool bRet = apProgram->SetFloat(kVar_afDissolveAmount, apObject->GetCoverageAmount());
			if(bRet==false)Error("Could not set variable!\n");
		}
		////////////////////////////
		//Illumination
		else if(aRenderMode == eMaterialRenderMode_Illumination)
		{
			bool bRet = apProgram->SetFloat(kVar_afColorMul, apObject->GetIlluminationAmount());
			apProgram->SetColor4f(kVar_avIlluminationMul, apObject->GetIlluminationColor() * apObject->GetIlluminationAmount());
		}
		else if(aRenderMode == eMaterialRenderMode_Diffuse)
		{
			apProgram->SetColor4f(kVar_avColorMul, apObject->GetColorMul());
		}
	}


	//--------------------------------------------------------------------------

	iMaterialVars* cMaterialType_SolidDiffuse::CreateSpecificVariables()
	{
		return hplNew(cMaterialType_SolidDiffuse_Vars,());
	}

	//--------------------------------------------------------------------------

	void cMaterialType_SolidDiffuse::LoadVariables(cMaterial* apMaterial, cResourceVarsObject *apVars)
	{
		cMaterialType_SolidDiffuse_Vars *pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();
		if(pVars==NULL)
		{
			pVars = (cMaterialType_SolidDiffuse_Vars*)CreateSpecificVariables();
			apMaterial->SetVars(pVars);
		}
				
		pVars->mfHeightMapScale = apVars->GetVarFloat("HeightMapScale", 0.1f);
		pVars->mfHeightMapBias = apVars->GetVarFloat("HeightMapBias", 0);
		pVars->mfFrenselBias = apVars->GetVarFloat("FrenselBias", 0.2f);
		pVars->mfFrenselPow = apVars->GetVarFloat("FrenselPow", 8.0f);
		pVars->mbAlphaDissolveFilter = apVars->GetVarBool("AlphaDissolveFilter", false);
		pVars->mbSwayActive = apVars->GetVarBool("SwayActive", false);
		pVars->mbSwaySingleDir = apVars->GetVarBool("SwaySingleDir", false);
		pVars->mvSwayProperties = cVector3f(apVars->GetVarFloat("SwayFreq", 1), apVars->GetVarFloat("SwayAmplitude", 0.1f), apVars->GetVarFloat("SwaySpeed", 1));
		pVars->mvSwayOctaveMuls = apVars->GetVarVector3f("SwayOctaveMuls", cVector3f(0.125f, 0.25f, 1));
		pVars->mfSwayYFreqMul = apVars->GetVarFloat("SwayYFreqMul", 0);
		pVars->mvSwaySingleDir = apVars->GetVarVector3f("SwaySingleDirVector", cVector3f(0, 0, 1));
		pVars->mvSwaySingleSampleDir = apVars->GetVarVector3f("SwaySingleSampleVector", cVector3f(1, 0, 0));
		pVars->mbSwayForceFieldAffected = apVars->GetVarBool("SwayForceFieldAffected", false);
		pVars->mfSwayForceFieldMul = apVars->GetVarFloat("SwayForceFieldMul", 0);
		pVars->mfSwayForceFieldMax = apVars->GetVarFloat("SwayForceFieldMax", 0);
		float fFadeStart = apVars->GetVarFloat("DetailFadeStart", 5);
		cVector2f vDetailUvMul = apVars->GetVarVector2f("DetailUvMul", 4);
		pVars->mvDetailProperties[0] = fFadeStart;
		pVars->mvDetailProperties[1] = apVars->GetVarFloat("DetailFadeEnd", 10) - fFadeStart;
		pVars->mvDetailProperties[2] = vDetailUvMul.x;
		pVars->mvDetailProperties[3] = vDetailUvMul.y;
		pVars->mvDetailWeights = cVector3f(apVars->GetVarFloat("DetailWeight_Diffuse", 1), apVars->GetVarFloat("DetailWeight_Specular", 1), apVars->GetVarFloat("DetailWeight_Normal", 1));
	}

	//--------------------------------------------------------------------------

	void cMaterialType_SolidDiffuse::GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars)
	{
		cMaterialType_SolidDiffuse_Vars* pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();

		apVars->AddVarFloat("HeightMapScale", pVars->mfHeightMapScale);
		apVars->AddVarFloat("HeightMapBias", pVars->mfHeightMapBias);
		apVars->AddVarFloat("FrenselBias", pVars->mfFrenselBias);
		apVars->AddVarFloat("FrenselPow", pVars->mfFrenselPow);
		apVars->AddVarBool("AlphaDissolveFilter", pVars->mbAlphaDissolveFilter);
	}

	//--------------------------------------------------------------------------
	//////////////////////////////////////////////////////////////////////////
	// PROJECTED UV
	//////////////////////////////////////////////////////////////////////////

	//--------------------------------------------------------------------------

	// ponytail: no detail/blend maps or PlanarUVRotation/TextureUVOffset (unused or all default in SOMA data)
	cMaterialType_ProjectedUV::cMaterialType_ProjectedUV(cGraphics *apGraphics, cResources *apResources) : iMaterialType_SolidBase(apGraphics, apResources)
	{
		for(int i=eMaterialTexture_DiffuseSide; i<=eMaterialTexture_SpecularBottom; ++i)
			AddUsedTexture((eMaterialTexture)i);

		mbHasTypeSpecifics[eMaterialRenderMode_Diffuse] = true;
	}

	//--------------------------------------------------------------------------

	void cMaterialType_ProjectedUV::LoadSpecificData()
	{
		cParserVarContainer defaultVars;
		defaultVars.Add("UseUv");
		defaultVars.Add("UseNormals");
		defaultVars.Add("UseDepth");
		defaultVars.Add("UseColor");
		defaultVars.Add("UseColorMul");
		mpProgramManager->SetupGenerateProgramData(	eMaterialRenderMode_Diffuse,"Diffuse","deferred_projected_uv_vtx.glsl", "deferred_projected_uv_frag.glsl",
													vProjectedUVFeatureVec,kProjectedUVFeatureNum, defaultVars);

		mpProgramManager->AddGenerateProgramVariableId("afInvFarPlane",kVar_afInvFarPlane,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("a_mtxUV",kVar_a_mtxUV,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("avColorMul", kVar_avColorMul,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("a_mtxNormalFrag", kVar_a_mtxNormalFrag,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("afBlendHardness", kVar_afBlendHardness,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("afBlendHardnessVtx", kVar_afBlendHardnessVtx,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("afNormalMapBlendImpact", kVar_afNormalMapBlendImpact,eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("avTextureScale", kVar_avTextureScale,eMaterialRenderMode_Diffuse);
	}

	//--------------------------------------------------------------------------

	void cMaterialType_ProjectedUV::CompileSolidSpecifics(cMaterial *apMaterial)
	{
		apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse,true);
		apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_Diffuse,true);
		apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_Z_Dissolve,true);
	}

	//--------------------------------------------------------------------------

	static bool HasProjectedTexture(cMaterial *apMaterial, int alFirst, int alStep)
	{
		for(int i=0; i<3; ++i)
			if(apMaterial->GetTexture((eMaterialTexture)(alFirst + i*alStep))) return true;
		return false;
	}

	iTexture* cMaterialType_ProjectedUV::GetTextureForUnit(cMaterial *apMaterial,eMaterialRenderMode aRenderMode, int alUnit)
	{
		if(aRenderMode == eMaterialRenderMode_Z || aRenderMode == eMaterialRenderMode_Z_Dissolve)
			return alUnit == 1 ? mpDissolveTexture : NULL;

		if(aRenderMode != eMaterialRenderMode_Diffuse || alUnit > 8) return NULL;

		//Units 0-8: Diffuse, NMap, Specular x Side, Top, Bottom; a missing one falls back to another of its kind
		int lFirst = eMaterialTexture_DiffuseSide + alUnit/3*3;
		for(int i=0; i<3; ++i)
			if(iTexture *pTex = apMaterial->GetTexture((eMaterialTexture)(lFirst + (alUnit%3 + i)%3))) return pTex;
		return NULL;
	}

	//--------------------------------------------------------------------------

	iGpuProgram* cMaterialType_ProjectedUV::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		if(aRenderMode == eMaterialRenderMode_Z)			return mpGlobalProgramManager->GenerateProgram(eMaterialRenderMode_Z, 0);
		if(aRenderMode == eMaterialRenderMode_Z_Dissolve)	return mpGlobalProgramManager->GenerateProgram(eMaterialRenderMode_Z, eFeature_Z_Dissolve);
		if(aRenderMode != eMaterialRenderMode_Diffuse)		return NULL;

		cMaterialType_ProjectedUV_Vars *pVars = (cMaterialType_ProjectedUV_Vars*)apMaterial->GetVars();
		tFlag lFlags = 0;
		if(HasProjectedTexture(apMaterial, eMaterialTexture_DiffuseSide, 1))	lFlags |= eFeature_PUV_DiffuseMap;
		if(HasProjectedTexture(apMaterial, eMaterialTexture_NMapSide, 1))		lFlags |= eFeature_PUV_NormalMaps;
		if(HasProjectedTexture(apMaterial, eMaterialTexture_SpecularSide, 1))	lFlags |= eFeature_PUV_Specular;
		if(HasProjectedTexture(apMaterial, eMaterialTexture_DiffuseBottom, 3))	lFlags |= eFeature_PUV_BottomTexture;
		if(pVars->mfNormalMapBlendImpact > 0)									lFlags |= eFeature_PUV_NMapBlendWeight;

		return mpProgramManager->GenerateProgram(aRenderMode,lFlags);
	}

	//--------------------------------------------------------------------------

	void cMaterialType_ProjectedUV::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderer *apRenderer)
	{
		if(aRenderMode != eMaterialRenderMode_Diffuse) return;

		apProgram->SetFloat(kVar_afInvFarPlane, 1.0f/apRenderer->GetCurrentFrustum()->GetFarPlane());
	}

	//--------------------------------------------------------------------------

	void cMaterialType_ProjectedUV::SetupMaterialSpecificData(	eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,
																iRenderer *apRenderer)
	{
		if(aRenderMode != eMaterialRenderMode_Diffuse) return;

		cMaterialType_ProjectedUV_Vars *pVars = (cMaterialType_ProjectedUV_Vars*)apMaterial->GetVars();
		apProgram->SetFloat(kVar_afBlendHardness, pVars->mfBlendHardness);
		apProgram->SetFloat(kVar_afBlendHardnessVtx, pVars->mfBlendHardness);
		apProgram->SetFloat(kVar_afNormalMapBlendImpact, pVars->mfNormalMapBlendImpact);
		apProgram->SetVec3f(kVar_avTextureScale, pVars->mvTextureScale);
	}

	//--------------------------------------------------------------------------

	void cMaterialType_ProjectedUV::SetupObjectSpecificData(	eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable *apObject,
																iRenderer *apRenderer)
	{
		if(aRenderMode == eMaterialRenderMode_Z_Dissolve)
		{
			apProgram->SetFloat(kVar_afDissolveAmount, apObject->GetCoverageAmount());
		}
		else if(aRenderMode == eMaterialRenderMode_Diffuse)
		{
			//Official static batches are in world space; other objects only project in world space with DynamicObjectSupport
			cMatrixf *pMtx = apObject->GetModelMatrixPtr();
			bool bWorld = pMtx && (apObject->IsStatic() || ((cMaterialType_ProjectedUV_Vars*)apObject->GetMaterial()->GetVars())->mbDynamicObjectSupport);
			apProgram->SetMatrixf(kVar_a_mtxUV, bWorld ? *pMtx : cMatrixf::Identity);
			const cMatrixf &mtxView = apRenderer->GetCurrentFrustum()->GetViewMatrix();
			apProgram->SetMatrixf(kVar_a_mtxNormalFrag, bWorld || pMtx == NULL ? mtxView : cMath::MatrixMul(mtxView, *pMtx));
			apProgram->SetColor4f(kVar_avColorMul, apObject->GetColorMul());
		}
	}

	//--------------------------------------------------------------------------

	void cMaterialType_ProjectedUV::LoadVariables(cMaterial* apMaterial, cResourceVarsObject *apVars)
	{
		cMaterialType_ProjectedUV_Vars *pVars = (cMaterialType_ProjectedUV_Vars*)apMaterial->GetVars();
		if(pVars==NULL)
		{
			pVars = (cMaterialType_ProjectedUV_Vars*)CreateSpecificVariables();
			apMaterial->SetVars(pVars);
		}

		pVars->mfBlendHardness = cMath::Clamp(1 - apVars->GetVarFloat("BlendSmoothness", 0.5f), 0.0f, 0.99f);
		pVars->mfNormalMapBlendImpact = cMath::Clamp(apVars->GetVarFloat("NormalMapBlendImpact", 0), 0.0f, 1.0f);
		pVars->mvTextureScale = cVector3f(	1.0f / apVars->GetVarFloat("TextureUVScaleSide", 1),
											1.0f / apVars->GetVarFloat("TextureUVScaleTop", 1),
											1.0f / apVars->GetVarFloat("TextureUVScaleBottom", 1));
		pVars->mbDynamicObjectSupport = apVars->GetVarBool("DynamicObjectSupport", false);
	}

	//--------------------------------------------------------------------------
}
