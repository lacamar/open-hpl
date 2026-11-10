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

#include "graphics/MaterialType_Water.h"

#include "resources/FileSearcher.h"
#include "graphics/RustMaterialShim.h"

namespace hpl {

	using namespace rustmat;

	static const OhplWaterVars* WaterVars(cMaterial *apMaterial){ return static_cast<cMaterialType_Water_Vars*>(apMaterial->GetVars()); }

	cMaterialType_Water::cMaterialType_Water(cGraphics *apGraphics, cResources *apResources) : iMaterialType(apGraphics, apResources)
	{
		InitFromRust(eOhplMatKind_Water);
	}

	cMaterialType_Water::~cMaterialType_Water()
	{
	}

	void cMaterialType_Water::DestroyProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
	{
		rustmat::DestroyProgram(eOhplMatKind_Water, aRenderMode, 0, apProgram, mpProgramManager, NULL, NULL);
	}

	void cMaterialType_Water::LoadData()
	{
		OhplGlobals g = MakeGlobals();
		// SOMA ships a dedicated water vertex shader (vertex waves)
		g.hasWaterVtx = mpResources->GetFileSearcher()->GetFilePath("water_surface_vtx.hpsl") != _W("");
		RunSetup(eOhplMatKind_Water, false, g, mpProgramManager, NULL);
	}

	void cMaterialType_Water::DestroyData()
	{
		mpProgramManager->DestroyShadersAndPrograms();
	}

	iTexture* cMaterialType_Water::GetTextureForUnit(cMaterial *apMaterial,eMaterialRenderMode aRenderMode, int alUnit)
	{
		return TextureForUnit(eOhplMatKind_Water, WaterVars(apMaterial), apMaterial, mpGraphics, aRenderMode, alUnit, NULL, NULL);
	}

	iTexture* cMaterialType_Water::GetSpecialTexture(cMaterial *apMaterial, eMaterialRenderMode aRenderMode,iRenderer *apRenderer, int alUnit)
	{
		return NULL;
	}

	iGpuProgram* cMaterialType_Water::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		return PickProgram(SelectProgram(eOhplMatKind_Water, WaterVars(apMaterial), apMaterial, aRenderMode, alSkeleton), mpProgramManager, NULL, NULL);
	}

	void cMaterialType_Water::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram,iRenderer *apRenderer)
	{
		SetupType(eOhplMatKind_Water, aRenderMode, apProgram, apRenderer);
	}

	void cMaterialType_Water::SetupMaterialSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,iRenderer *apRenderer)
	{
		SetupMaterial(eOhplMatKind_Water, WaterVars(apMaterial), aRenderMode, apProgram, apMaterial, apRenderer);
	}

	void cMaterialType_Water::SetupObjectSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable *apObject,iRenderer *apRenderer)
	{
		SetupObject(eOhplMatKind_Water, WaterVars(apObject->GetMaterial()), aRenderMode, apProgram, apObject, apRenderer);
	}

	iMaterialVars* cMaterialType_Water::CreateSpecificVariables()
	{
		return hplNew(cMaterialType_Water_Vars,());
	}

	void cMaterialType_Water::LoadVariables(cMaterial *apMaterial, cResourceVarsObject *apVars)
	{
		cMaterialType_Water_Vars *pVars = (cMaterialType_Water_Vars*)apMaterial->GetVars();
		if(pVars==NULL)
		{
			pVars = (cMaterialType_Water_Vars*)CreateSpecificVariables();
			apMaterial->SetVars(pVars);
		}

		rustmat::LoadVariables(eOhplMatKind_Water, apMaterial, apVars, static_cast<OhplWaterVars*>(pVars));
	}

	void cMaterialType_Water::GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars)
	{
		VarValues(eOhplMatKind_Water, WaterVars(apMaterial), apMaterial, apVars);
	}

	void cMaterialType_Water::CompileMaterialSpecifics(cMaterial *apMaterial)
	{
		CompileMaterial(eOhplMatKind_Water, WaterVars(apMaterial), apMaterial);
	}
}
