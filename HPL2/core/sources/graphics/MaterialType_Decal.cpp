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

#include "graphics/Bitmap.h"
#include "graphics/RustMaterialShim.h"

namespace hpl {

	using namespace rustmat;

	static const OhplTerrainBlendVars* TerrainVars(cMaterial *apMaterial){ return static_cast<cMaterialType_TerrainBlend_Vars*>(apMaterial->GetVars()); }
	static const OhplUndergrowthVars* UndergrowthVars(cMaterial *apMaterial){ return static_cast<cMaterialType_Undergrowth_Vars*>(apMaterial->GetVars()); }

	//////////////////////////////////////////////////////////////////////////
	// DECAL
	//////////////////////////////////////////////////////////////////////////

	cMaterialType_Decal::cMaterialType_Decal(cGraphics *apGraphics, cResources *apResources) : iMaterialType(apGraphics, apResources)
	{
		InitFromRust(eOhplMatKind_Decal);
	}

	cMaterialType_Decal::~cMaterialType_Decal()
	{
	}

	void cMaterialType_Decal::DestroyProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
	{
		rustmat::DestroyProgram(eOhplMatKind_Decal, aRenderMode, 0, apProgram, mpProgramManager, NULL, NULL);
	}

	void cMaterialType_Decal::LoadData()
	{
		RunSetup(eOhplMatKind_Decal, false, MakeGlobals(), mpProgramManager, NULL);
	}

	void cMaterialType_Decal::DestroyData()
	{
		mpProgramManager->DestroyShadersAndPrograms();
	}

	iTexture* cMaterialType_Decal::GetTextureForUnit(cMaterial *apMaterial,eMaterialRenderMode aRenderMode, int alUnit)
	{
		return TextureForUnit(eOhplMatKind_Decal, NULL, apMaterial, mpGraphics, aRenderMode, alUnit, NULL, NULL);
	}

	iTexture* cMaterialType_Decal::GetSpecialTexture(cMaterial *apMaterial, eMaterialRenderMode aRenderMode,iRenderer *apRenderer, int alUnit)
	{
		return NULL;
	}

	iGpuProgram* cMaterialType_Decal::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		return PickProgram(SelectProgram(eOhplMatKind_Decal, NULL, apMaterial, aRenderMode, alSkeleton), mpProgramManager, NULL, NULL);
	}

	void cMaterialType_Decal::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram,iRenderer *apRenderer)
	{
	}

	void cMaterialType_Decal::SetupMaterialSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,iRenderer *apRenderer)
	{
		SetupMaterial(eOhplMatKind_Decal, NULL, aRenderMode, apProgram, apMaterial, apRenderer);
	}

	void cMaterialType_Decal::SetupObjectSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable *apObject,iRenderer *apRenderer)
	{
	}

	iMaterialVars* cMaterialType_Decal::CreateSpecificVariables()
	{
		return hplNew(cMaterialType_Decal_Vars,());
	}

	void cMaterialType_Decal::LoadVariables(cMaterial *apMaterial, cResourceVarsObject *apVars)
	{
		cMaterialType_Decal_Vars *pVars = (cMaterialType_Decal_Vars*)apMaterial->GetVars();
		if(pVars==NULL)
		{
			pVars = (cMaterialType_Decal_Vars*)CreateSpecificVariables();
			apMaterial->SetVars(pVars);
		}
	}

	void cMaterialType_Decal::GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars)
	{
	}

	void cMaterialType_Decal::CompileMaterialSpecifics(cMaterial *apMaterial)
	{
		CompileMaterial(eOhplMatKind_Decal, NULL, apMaterial);
	}

	//////////////////////////////////////////////////////////////////////////
	// TERRAIN BLEND
	//////////////////////////////////////////////////////////////////////////

	cMaterialType_TerrainBlend::cMaterialType_TerrainBlend(cGraphics *apGraphics, cResources *apResources) : iMaterialType(apGraphics, apResources)
	{
		InitFromRust(eOhplMatKind_TerrainBlend);
	}

	void cMaterialType_TerrainBlend::DestroyProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
	{
		rustmat::DestroyProgram(eOhplMatKind_TerrainBlend, aRenderMode, 0, apProgram, mpProgramManager, NULL, NULL);
	}

	void cMaterialType_TerrainBlend::LoadData()
	{
		RunSetup(eOhplMatKind_TerrainBlend, false, MakeGlobals(), mpProgramManager, NULL);

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
		return TextureForUnit(eOhplMatKind_TerrainBlend, TerrainVars(apMaterial), apMaterial, mpGraphics, aRenderMode, alUnit, NULL, mpWhiteTexture);
	}

	iGpuProgram* cMaterialType_TerrainBlend::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		return PickProgram(SelectProgram(eOhplMatKind_TerrainBlend, TerrainVars(apMaterial), apMaterial, aRenderMode, alSkeleton), mpProgramManager, NULL, NULL);
	}

	void cMaterialType_TerrainBlend::SetupMaterialSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,iRenderer *apRenderer)
	{
		SetupMaterial(eOhplMatKind_TerrainBlend, TerrainVars(apMaterial), aRenderMode, apProgram, apMaterial, apRenderer);
	}

	void cMaterialType_TerrainBlend::CompileMaterialSpecifics(cMaterial *apMaterial)
	{
		CompileMaterial(eOhplMatKind_TerrainBlend, TerrainVars(apMaterial), apMaterial);
	}

	//////////////////////////////////////////////////////////////////////////
	// UNDERGROWTH
	//////////////////////////////////////////////////////////////////////////

	cMaterialType_Undergrowth::cMaterialType_Undergrowth(cGraphics *apGraphics, cResources *apResources) : iMaterialType(apGraphics, apResources)
	{
		InitFromRust(eOhplMatKind_Undergrowth);
	}

	void cMaterialType_Undergrowth::DestroyProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
	{
		rustmat::DestroyProgram(eOhplMatKind_Undergrowth, aRenderMode, 0, apProgram, mpProgramManager, NULL, NULL);
	}

	void cMaterialType_Undergrowth::LoadData()
	{
		RunSetup(eOhplMatKind_Undergrowth, false, MakeGlobals(), mpProgramManager, NULL);
	}

	void cMaterialType_Undergrowth::DestroyData()
	{
		mpProgramManager->DestroyShadersAndPrograms();
		for(auto& vProgs : mvPrograms) for(iGpuProgram*& pProg : vProgs) pProg = NULL;
	}

	iTexture* cMaterialType_Undergrowth::GetTextureForUnit(cMaterial *apMaterial,eMaterialRenderMode aRenderMode, int alUnit)
	{
		return TextureForUnit(eOhplMatKind_Undergrowth, UndergrowthVars(apMaterial), apMaterial, mpGraphics, aRenderMode, alUnit, NULL, NULL);
	}

	iGpuProgram* cMaterialType_Undergrowth::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		return PickProgram(SelectProgram(eOhplMatKind_Undergrowth, UndergrowthVars(apMaterial), apMaterial, aRenderMode, alSkeleton), mpProgramManager, NULL, NULL);
	}

	void cMaterialType_Undergrowth::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram,iRenderer *apRenderer)
	{
		SetupType(eOhplMatKind_Undergrowth, aRenderMode, apProgram, apRenderer);
	}

	void cMaterialType_Undergrowth::SetupMaterialSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,iRenderer *apRenderer)
	{
		SetupMaterial(eOhplMatKind_Undergrowth, UndergrowthVars(apMaterial), aRenderMode, apProgram, apMaterial, apRenderer, mvFields, &mlFieldNum);
	}

	// Rebirth's SetupUndergrowth: single/four field variant from the fields within fade range of the camera
	iGpuProgram* cMaterialType_Undergrowth::GetRenderProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iRenderer *apRenderer)
	{
		OhplRenderView r = MakeRenderView(apRenderer);
		HostCtx ctx = { apRenderer, NULL, mvFields, &mlFieldNum };
		OhplHostVt host = MakeHost(&ctx);
		OhplProgSel sel;
		size_t lFields;
		ohpl_mat_undergrowth_render_program(UndergrowthVars(apMaterial), aRenderMode, &r, &host, &sel, &lFields);
		if(sel.manager == kOhplMgrNone) return NULL;

		mlFieldNum = (int)lFields;
		iGpuProgram*& pProg = mvPrograms[aRenderMode][sel.flags];
		if(pProg == NULL) pProg = mpProgramManager->GenerateProgram(sel.mode, sel.flags);
		return pProg;
	}

	void cMaterialType_Undergrowth::CompileMaterialSpecifics(cMaterial *apMaterial)
	{
		CompileMaterial(eOhplMatKind_Undergrowth, UndergrowthVars(apMaterial), apMaterial);
	}

}
