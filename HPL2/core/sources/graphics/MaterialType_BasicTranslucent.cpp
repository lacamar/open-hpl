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

#include "graphics/MaterialType_BasicTranslucent.h"

#include "system/String.h"
#include "graphics/RustMaterialShim.h"

namespace hpl {

	using namespace rustmat;

	bool cMaterialType_Translucent::mbLightProbes = false;
	tFlag cMaterialType_Translucent::mlWorldFog = 0;
	bool cMaterialType_Translucent::mbDepthOfField = false;
	cColor cMaterialType_Translucent::mvDepthOfFieldParams = cColor(0,0);

	static const OhplTranslucentVars* TranslucentVars(cMaterial *apMaterial){ return static_cast<cMaterialType_Translucent_Vars*>(apMaterial->GetVars()); }

	cMaterialType_Translucent::cMaterialType_Translucent(cGraphics *apGraphics, cResources *apResources) : iMaterialType(apGraphics, apResources)
	{
		InitFromRust(eOhplMatKind_Translucent);

		for(int i=0; i<5; ++i)
			mpBlendProgramManager[i] = hplNew( cProgramComboManager, ("Blend"+cString::ToString(i),mpGraphics, mpResources,eMaterialRenderMode_LastEnum) );
	}

	cMaterialType_Translucent::~cMaterialType_Translucent()
	{
		for(int i=0; i<5; ++i)
			hplDelete(mpBlendProgramManager[i]);
	}

	void cMaterialType_Translucent::SetWorldFog(bool abUnderwater, bool abSecondary)
	{
		mlWorldFog = ohpl_mat_world_fog_flags(abUnderwater, abSecondary);
	}

	void cMaterialType_Translucent::DestroyProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
	{
		rustmat::DestroyProgram(eOhplMatKind_Translucent, aRenderMode, apMaterial->GetBlendMode(), apProgram, NULL, NULL, mpBlendProgramManager);
	}

	void cMaterialType_Translucent::LoadData()
	{
		RunSetup(eOhplMatKind_Translucent, false, MakeGlobals(), NULL, mpBlendProgramManager);
	}

	void cMaterialType_Translucent::DestroyData()
	{
		for(int i=0; i<5; ++i) mpBlendProgramManager[i]->DestroyShadersAndPrograms();
	}

	iTexture* cMaterialType_Translucent::GetTextureForUnit(cMaterial *apMaterial,eMaterialRenderMode aRenderMode, int alUnit)
	{
		return TextureForUnit(eOhplMatKind_Translucent, TranslucentVars(apMaterial), apMaterial, mpGraphics, aRenderMode, alUnit, NULL, NULL);
	}

	iTexture* cMaterialType_Translucent::GetSpecialTexture(cMaterial *apMaterial, eMaterialRenderMode aRenderMode,iRenderer *apRenderer, int alUnit)
	{
		return NULL;
	}

	iGpuProgram* cMaterialType_Translucent::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		return PickProgram(SelectProgram(eOhplMatKind_Translucent, TranslucentVars(apMaterial), apMaterial, aRenderMode, alSkeleton), NULL, NULL, mpBlendProgramManager);
	}

	iGpuProgram* cMaterialType_Translucent::GetRenderProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iRenderer *apRenderer)
	{
		OhplGlobals g = MakeGlobals();
		if(ohpl_mat_render_program_slot(eOhplMatKind_Translucent, aRenderMode, &g) == 0)
			return apMaterial->GetProgram(0, aRenderMode);
		if(apMaterial->GetProgram(1, aRenderMode)==NULL)
			apMaterial->SetProgram(1, aRenderMode, GetGpuProgram(apMaterial, aRenderMode, 1));
		return apMaterial->GetProgram(1, aRenderMode);
	}

	void cMaterialType_Translucent::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram,iRenderer *apRenderer)
	{
		SetupType(eOhplMatKind_Translucent, aRenderMode, apProgram, apRenderer);
	}

	void cMaterialType_Translucent::SetupMaterialSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,iRenderer *apRenderer)
	{
		SetupMaterial(eOhplMatKind_Translucent, TranslucentVars(apMaterial), aRenderMode, apProgram, apMaterial, apRenderer);
	}

	void cMaterialType_Translucent::SetupObjectSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable *apObject,iRenderer *apRenderer)
	{
		SetupObject(eOhplMatKind_Translucent, TranslucentVars(apObject->GetMaterial()), aRenderMode, apProgram, apObject, apRenderer);
	}

	iMaterialVars* cMaterialType_Translucent::CreateSpecificVariables()
	{
		return hplNew(cMaterialType_Translucent_Vars,());
	}

	void cMaterialType_Translucent::LoadVariables(cMaterial *apMaterial, cResourceVarsObject *apVars)
	{
		cMaterialType_Translucent_Vars *pVars = (cMaterialType_Translucent_Vars*)apMaterial->GetVars();
		if(pVars==NULL)
		{
			pVars = (cMaterialType_Translucent_Vars*)CreateSpecificVariables();
			apMaterial->SetVars(pVars);
		}

		rustmat::LoadVariables(eOhplMatKind_Translucent, apMaterial, apVars, static_cast<OhplTranslucentVars*>(pVars));
	}

	void cMaterialType_Translucent::GetVariableValues(cMaterial *apMaterial, cResourceVarsObject *apVars)
	{
		VarValues(eOhplMatKind_Translucent, TranslucentVars(apMaterial), apMaterial, apVars);
	}

	void cMaterialType_Translucent::CompileMaterialSpecifics(cMaterial *apMaterial)
	{
		CompileMaterial(eOhplMatKind_Translucent, TranslucentVars(apMaterial), apMaterial);
	}
}
