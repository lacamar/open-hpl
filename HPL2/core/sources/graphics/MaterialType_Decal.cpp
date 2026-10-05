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

#include "graphics/MaterialType_Decal.h"

#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"

#include "resources/Resources.h"

#include "math/Math.h"
#include "math/Frustum.h"

#include "graphics/Graphics.h"
#include "graphics/Bitmap.h"
#include "graphics/Texture.h"
#include "graphics/Material.h"
#include "graphics/GPUShader.h"
#include "graphics/GPUProgram.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/ProgramComboManager.h"
#include "graphics/Renderable.h"
#include "graphics/Renderer.h"
#include "scene/World.h"
#include "scene/ForceField.h"



namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// DEFINES
	//////////////////////////////////////////////////////////////////////////
	
	//------------------------------
	// Variables
	//------------------------------
	#define kVar_a_mtxUV						0	
	
	//------------------------------
	//Diffuse Features and data
	//------------------------------
	#define eFeature_Diffuse_UvAnimation	eFlagBit_0

	#define kDiffuseFeatureNum 1
	
	static cProgramComboFeature vDiffuseFeatureVec[] =
	{
		cProgramComboFeature("UseUvAnimation", kPC_VertexBit),							
	};

	//////////////////////////////////////////////////////////////////////////
	// DECAL
	//////////////////////////////////////////////////////////////////////////
	
	//--------------------------------------------------------------------------
	
	cMaterialType_Decal::cMaterialType_Decal(cGraphics *apGraphics, cResources *apResources) : iMaterialType(apGraphics, apResources)
	{
		mbIsTranslucent = true;
		mbIsDecal = true;

		mbHasTypeSpecifics[eMaterialRenderMode_Diffuse] = true;

		AddUsedTexture(eMaterialTexture_Diffuse);
	}

	cMaterialType_Decal::~cMaterialType_Decal()
	{
	}

	//--------------------------------------------------------------------------

	void cMaterialType_Decal::DestroyProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
	{
		mpProgramManager->DestroyGeneratedProgram(aRenderMode, apProgram);
	}

	//--------------------------------------------------------------------------


	void cMaterialType_Decal::LoadData()
	{
		cParserVarContainer defaultVars;
		defaultVars.Add("UseUv");
		defaultVars.Add("UseColor");
		
		mpProgramManager->SetupGenerateProgramData(	eMaterialRenderMode_Diffuse,"Diffuse","deferred_base_vtx.glsl", "deferred_decal_frag.glsl", 
											vDiffuseFeatureVec,kDiffuseFeatureNum, defaultVars);

		////////////////////////////////
		//Set up variable ids
		mpProgramManager->AddGenerateProgramVariableId("a_mtxUV",kVar_a_mtxUV,eMaterialRenderMode_Diffuse);
	}

	//--------------------------------------------------------------------------

	void cMaterialType_Decal::DestroyData()
	{
		mpProgramManager->DestroyShadersAndPrograms();
	}

	//--------------------------------------------------------------------------

	iTexture* cMaterialType_Decal::GetTextureForUnit(cMaterial *apMaterial,eMaterialRenderMode aRenderMode, int alUnit)
	{
		////////////////////////////
		//Diffuse
		if(aRenderMode == eMaterialRenderMode_Diffuse)
		{
			switch(alUnit)
			{
			case 0: return apMaterial->GetTexture(eMaterialTexture_Diffuse);
			}
		}

		return NULL;
	}

	//--------------------------------------------------------------------------

	iTexture* cMaterialType_Decal::GetSpecialTexture(cMaterial *apMaterial, eMaterialRenderMode aRenderMode,iRenderer *apRenderer, int alUnit)
	{
		return NULL;
	}
	
	//--------------------------------------------------------------------------
	
	iGpuProgram* cMaterialType_Decal::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		////////////////////////////
		//Diffuse
		if(aRenderMode == eMaterialRenderMode_Diffuse)
		{
			tFlag lFlags =0;
			if(apMaterial->HasUvAnimation())						lFlags |= eFeature_Diffuse_UvAnimation;

			return mpProgramManager->GenerateProgram(aRenderMode,lFlags);
		}

		return NULL;
	}

	//--------------------------------------------------------------------------

	void cMaterialType_Decal::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram,iRenderer *apRenderer)
	{
	}
	
	//--------------------------------------------------------------------------

	void cMaterialType_Decal::SetupMaterialSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,iRenderer *apRenderer)
	{
		////////////////////////////
		//Diffuse
		if(aRenderMode == eMaterialRenderMode_Diffuse)
		{
			cMaterialType_Decal_Vars *pVars = static_cast<cMaterialType_Decal_Vars*>(apMaterial->GetVars());

			/////////////////////////
			//UV Animation
			if(apMaterial->HasUvAnimation())
			{
				apProgram->SetMatrixf(kVar_a_mtxUV, apMaterial->GetUvMatrix());
			}
		}
	}
	
	//--------------------------------------------------------------------------

	void cMaterialType_Decal::SetupObjectSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable *apObject,iRenderer *apRenderer)
	{
	}


	//--------------------------------------------------------------------------

	iMaterialVars* cMaterialType_Decal::CreateSpecificVariables()
	{
		return hplNew(cMaterialType_Decal_Vars,());
	}

	//--------------------------------------------------------------------------

	void cMaterialType_Decal::LoadVariables(cMaterial *apMaterial, cResourceVarsObject *apVars)
	{
		cMaterialType_Decal_Vars *pVars = (cMaterialType_Decal_Vars*)apMaterial->GetVars();
		if(pVars==NULL)
		{
			pVars = (cMaterialType_Decal_Vars*)CreateSpecificVariables();
			apMaterial->SetVars(pVars);
		}
	}

	//--------------------------------------------------------------------------

	void cMaterialType_Decal::GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars)
	{
		cMaterialType_Decal_Vars* pVars = (cMaterialType_Decal_Vars*)apMaterial->GetVars();
	}

	//--------------------------------------------------------------------------

	void cMaterialType_Decal::CompileMaterialSpecifics(cMaterial *apMaterial)
	{
		cMaterialType_Decal_Vars *pVars = static_cast<cMaterialType_Decal_Vars*>(apMaterial->GetVars());

		//////////////////////////////////
		//UV animation specifics
		if(apMaterial->HasUvAnimation())
		{
			apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse,true);
		}
		
		/////////////////////////////////////
		//Set up the blend mode
		//apMaterial->SetBlendMode(eMaterialBlendMode_Alpha);
	}
	
	//--------------------------------------------------------------------------



	//--------------------------------------------------------------------------

	cMaterialType_TerrainBlend::cMaterialType_TerrainBlend(cGraphics *apGraphics, cResources *apResources) : iMaterialType(apGraphics, apResources)
	{
		mbIsTranslucent = true;
		mbIsDecal = true;
		mbHasTypeSpecifics[eMaterialRenderMode_Diffuse] = true;
	}

	void cMaterialType_TerrainBlend::DestroyProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
	{
		mpProgramManager->DestroyGeneratedProgram(aRenderMode, apProgram);
	}

	void cMaterialType_TerrainBlend::LoadData()
	{
		cParserVarContainer defaultVars;
		defaultVars.Add("UseUv");
		static cProgramComboFeature vFeatures[] = { cProgramComboFeature("UseBaseTexture", kPC_FragmentBit) };
		mpProgramManager->SetupGenerateProgramData(eMaterialRenderMode_Diffuse, "Diffuse", "deferred_base_vtx.glsl", "cache_terrain_diffuse_frag.glsl",
												   vFeatures, 1, defaultVars);
		mpProgramManager->AddGenerateProgramVariableId("avTextureCoordScale", 0, eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("avOneMinusFadeStart", 1, eMaterialRenderMode_Diffuse);
		mpProgramManager->AddGenerateProgramVariableId("afBaseTextureCoordScale", 2, eMaterialRenderMode_Diffuse);

		cBitmap bmp;
		bmp.CreateData(cVector3l(1,1,1), ePixelFormat_RGBA, 0, 0);
		bmp.Clear(cColor(1,1), 0, 0);
		mpWhiteTexture = mpGraphics->CreateTexture("TerrainBlendWhite", eTextureType_2D, eTextureUsage_Normal);
		mpWhiteTexture->SetUseMipMaps(false);
		mpWhiteTexture->CreateFromBitmap(&bmp);
	}

	void cMaterialType_TerrainBlend::DestroyData()
	{
		mpProgramManager->DestroyShadersAndPrograms();
		if(mpWhiteTexture) mpGraphics->DestroyTexture(mpWhiteTexture);
		mpWhiteTexture = NULL;
	}

	iTexture* cMaterialType_TerrainBlend::GetTextureForUnit(cMaterial *apMaterial,eMaterialRenderMode aRenderMode, int alUnit)
	{
		if(aRenderMode != eMaterialRenderMode_Diffuse || alUnit > 9) return NULL;
		iTexture *pTex = apMaterial->GetTexture((eMaterialTexture)alUnit);
		return pTex ? pTex : mpWhiteTexture;
	}

	iGpuProgram* cMaterialType_TerrainBlend::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		if(aRenderMode != eMaterialRenderMode_Diffuse) return NULL;
		return mpProgramManager->GenerateProgram(aRenderMode, apMaterial->GetTexture(eMaterialTexture_Illumination) ? eFlagBit_0 : 0);
	}

	void cMaterialType_TerrainBlend::SetupMaterialSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,iRenderer *apRenderer)
	{
		cMaterialType_TerrainBlend_Vars *pVars = static_cast<cMaterialType_TerrainBlend_Vars*>(apMaterial->GetVars());
		const float *s = pVars->mvTextureCoordScale, *f = pVars->mvOneMinusFadeStart;
		apProgram->SetVec4f(0, s[0], s[1], s[2], s[3]);
		apProgram->SetVec4f(1, f[0], f[1], f[2], f[3]);
		apProgram->SetFloat(2, pVars->mfBaseTextureCoordScale);
	}

	//--------------------------------------------------------------------------

	static bool UndergrowthMode(eMaterialRenderMode aMode){ return aMode == eMaterialRenderMode_Z || aMode == eMaterialRenderMode_Diffuse; }

	cMaterialType_Undergrowth::cMaterialType_Undergrowth(cGraphics *apGraphics, cResources *apResources) : iMaterialType(apGraphics, apResources)
	{
		mbHasTypeSpecifics[eMaterialRenderMode_Z] = true;
		mbHasTypeSpecifics[eMaterialRenderMode_Diffuse] = true;
	}

	void cMaterialType_Undergrowth::DestroyProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
	{
		mpProgramManager->DestroyGeneratedProgram(aRenderMode, apProgram);
	}

	void cMaterialType_Undergrowth::LoadData()
	{
		static cProgramComboFeature vFeatures[] = { cProgramComboFeature("UseWind", kPC_VertexBit), cProgramComboFeature("UseSingleForceField", kPC_VertexBit),
													cProgramComboFeature("UseFourForceFields", kPC_VertexBit) };
		cParserVarContainer vars;
		for (eMaterialRenderMode mode : {eMaterialRenderMode_Z, eMaterialRenderMode_Diffuse})
		{
			mpProgramManager->SetupGenerateProgramData(mode, mode == eMaterialRenderMode_Z ? "Z" : "Diffuse", "deferred_undergrowth_gbuffer_vtx.glsl",
													   "deferred_undergrowth_gbuffer_frag.glsl", vFeatures, 3, vars);
			mpProgramManager->AddGenerateProgramVariableId("afInvFarPlane", 0, mode);
			mpProgramManager->AddGenerateProgramVariableId("afT", 1, mode);
			mpProgramManager->AddGenerateProgramVariableId("avDissolveStartSizeDepth", 2, mode);
			mpProgramManager->AddGenerateProgramVariableId("avWindProperties", 3, mode);
			mpProgramManager->AddGenerateProgramVariableId("avWindOctavesMul", 4, mode);
			AddForceFieldVariableIds(mpProgramManager, 5, mode);
			mpProgramManager->AddGenerateProgramVariableId("avForceFieldPos", 14, mode);
			mpProgramManager->AddGenerateProgramVariableId("avForceFieldProp", 15, mode);
		}
	}

	void cMaterialType_Undergrowth::DestroyData()
	{
		mpProgramManager->DestroyShadersAndPrograms();
		for(auto& vProgs : mvPrograms) for(iGpuProgram*& pProg : vProgs) pProg = NULL;
	}

	iTexture* cMaterialType_Undergrowth::GetTextureForUnit(cMaterial *apMaterial,eMaterialRenderMode aRenderMode, int alUnit)
	{
		return UndergrowthMode(aRenderMode) && alUnit == 0 ? apMaterial->GetTexture(eMaterialTexture_Diffuse) : NULL;
	}

	iGpuProgram* cMaterialType_Undergrowth::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		if(!UndergrowthMode(aRenderMode)) return NULL;
		return mpProgramManager->GenerateProgram(aRenderMode, static_cast<cMaterialType_Undergrowth_Vars*>(apMaterial->GetVars())->mbWind ? eFlagBit_0 : 0);
	}

	void cMaterialType_Undergrowth::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram,iRenderer *apRenderer)
	{
		apProgram->SetFloat(0, 1.0f / apRenderer->GetCurrentFrustum()->GetFarPlane());
		apProgram->SetFloat(1, apRenderer->GetTimeCount());
	}

	void cMaterialType_Undergrowth::SetupMaterialSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,iRenderer *apRenderer)
	{
		cMaterialType_Undergrowth_Vars *pVars = static_cast<cMaterialType_Undergrowth_Vars*>(apMaterial->GetVars());
		apProgram->SetVec2f(2, pVars->mvDissolve);
		apProgram->SetVec3f(3, pVars->mvWind);
		apProgram->SetVec3f(4, pVars->mvWindOctaves);
		if(mlFieldNum == 1)
		{
			cForceField *pField = mvFields[0];
			float fStart = pField->GetFinalFalloffStartRadius();
			apProgram->SetVec4f(14, pField->GetWorldPosition().x, pField->GetWorldPosition().y, pField->GetWorldPosition().z, pField->GetT());
			apProgram->SetVec4f(15, fStart, pField->GetFinalRadius() - fStart, pField->GetFinalForce() * pVars->mfForceFieldMul, pVars->mfForceFieldMul);
		}
		else if(mlFieldNum > 1)
			SetForceFieldVars(apProgram, 5, mvFields, mlFieldNum, pVars->mfForceFieldMul, pVars->mfMaxForceFieldForce);
	}

	// Rebirth's SetupUndergrowth: single/four field variant from the fields within fade range of the camera
	iGpuProgram* cMaterialType_Undergrowth::GetRenderProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iRenderer *apRenderer)
	{
		if(!UndergrowthMode(aRenderMode)) return NULL;
		cMaterialType_Undergrowth_Vars *pVars = static_cast<cMaterialType_Undergrowth_Vars*>(apMaterial->GetVars());
		mlFieldNum = 0;
		if(pVars->mfForceFieldMul > 0)
		{
			cVector3f vCam = apRenderer->GetCurrentFrustum()->GetOrigin();
			float fRange = pVars->mvDissolve.x + pVars->mvDissolve.y;
			mlFieldNum = apRenderer->GetCurrentWorld()->GetForceFields(vCam - fRange, vCam + fRange, mvFields);
		}
		int lFlags = (pVars->mbWind ? eFlagBit_0 : 0) | (mlFieldNum == 1 ? eFlagBit_1 : mlFieldNum > 1 ? eFlagBit_2 : 0);
		iGpuProgram*& pProg = mvPrograms[aRenderMode][lFlags];
		if(pProg == NULL) pProg = mpProgramManager->GenerateProgram(aRenderMode, lFlags);
		return pProg;
	}

	void cMaterialType_Undergrowth::CompileMaterialSpecifics(cMaterial *apMaterial)
	{
		apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Z, true);
		apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse, true);
	}

}
