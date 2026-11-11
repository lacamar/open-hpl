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

#include "resources/TextureManager.h"
#include "graphics/RustMaterialShim.h"

namespace hpl {

	using namespace rustmat;

	bool iMaterialType_SolidBase::mbGlobalDataCreated = false;
	cProgramComboManager* iMaterialType_SolidBase::mpGlobalProgramManager;

	float iMaterialType_SolidBase::mfVirtualPositionAddScale = 0.03f;

	static const OhplSolidDiffuseVars* DiffuseVars(cMaterial *apMaterial){ return static_cast<cMaterialType_SolidDiffuse_Vars*>(apMaterial->GetVars()); }
	static const OhplProjectedUvVars* ProjectedVars(cMaterial *apMaterial){ return static_cast<cMaterialType_ProjectedUV_Vars*>(apMaterial->GetVars()); }
	static const OhplVertexBlendVars* BlendVars(cMaterial *apMaterial){ return static_cast<cMaterialType_VertexBlend_Vars*>(apMaterial->GetVars()); }

	//////////////////////////////////////////////////////////////////////////
	// SOLID BASE
	//////////////////////////////////////////////////////////////////////////

	iMaterialType_SolidBase::iMaterialType_SolidBase(cGraphics *apGraphics, cResources *apResources) : iMaterialType(apGraphics,apResources)
	{
		mbIsGlobalDataCreator = false;
	}

	iMaterialType_SolidBase::~iMaterialType_SolidBase()
	{

	}

	void iMaterialType_SolidBase::DestroyProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
	{
		rustmat::DestroyProgram(eOhplMatKind_SolidDiffuse, aRenderMode, 0, apProgram, mpProgramManager, mpGlobalProgramManager, NULL);
	}

	void iMaterialType_SolidBase::CreateGlobalPrograms()
	{
		if(mbGlobalDataCreated) return;

		mbGlobalDataCreated = true;
		mbIsGlobalDataCreator = true;

		//This makes this material's program manager responsible for managing the global programs!
		RunSetup(eOhplMatKind_SolidDiffuse, true, MakeGlobals(), mpProgramManager, NULL);

		mpGlobalProgramManager = mpProgramManager;
	}

	void iMaterialType_SolidBase::LoadData()
	{
		//Global data init (that is shared between Solid materials)
		CreateGlobalPrograms();

		mpDissolveTexture = mpResources->GetTextureManager()->Create2D("core_dissolve.tga",true);

		LoadSpecificData();
	}

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

	void iMaterialType_SolidBase::LoadVariables(cMaterial *apMaterial, cResourceVarsObject *apVars)
	{

	}

	void iMaterialType_SolidBase::GetVariableValues(cMaterial *apMaterial, cResourceVarsObject *apVars)
	{

	}

	void iMaterialType_SolidBase::CompileMaterialSpecifics(cMaterial *apMaterial)
	{
		CompileSolidSpecifics(apMaterial);
	}

	//////////////////////////////////////////////////////////////////////////
	// SOLID DIFFUSE
	//////////////////////////////////////////////////////////////////////////

	cMaterialType_SolidDiffuse::cMaterialType_SolidDiffuse(cGraphics *apGraphics, cResources *apResources) : iMaterialType_SolidBase(apGraphics, apResources)
	{
		InitFromRust(eOhplMatKind_SolidDiffuse);
	}

	cMaterialType_SolidDiffuse::~cMaterialType_SolidDiffuse()
	{
	}

	void cMaterialType_SolidDiffuse::LoadSpecificData()
	{
		RunSetup(eOhplMatKind_SolidDiffuse, false, MakeGlobals(mpGraphics, mfVirtualPositionAddScale), mpProgramManager, NULL);
	}

	void cMaterialType_SolidDiffuse::CompileSolidSpecifics(cMaterial *apMaterial)
	{
		CompileMaterial(eOhplMatKind_SolidDiffuse, DiffuseVars(apMaterial), apMaterial);
	}

	iTexture* cMaterialType_SolidDiffuse::GetTextureForUnit(cMaterial *apMaterial,eMaterialRenderMode aRenderMode, int alUnit)
	{
		return TextureForUnit(eOhplMatKind_SolidDiffuse, DiffuseVars(apMaterial), apMaterial, mpGraphics, aRenderMode, alUnit, mpDissolveTexture, NULL);
	}

	iTexture* cMaterialType_SolidDiffuse::GetSpecialTexture(cMaterial *apMaterial, eMaterialRenderMode aRenderMode,iRenderer *apRenderer, int alUnit)
	{
		return NULL;
	}

	iGpuProgram* cMaterialType_SolidDiffuse::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		return PickProgram(SelectProgram(eOhplMatKind_SolidDiffuse, DiffuseVars(apMaterial), apMaterial, aRenderMode, alSkeleton), mpProgramManager, mpGlobalProgramManager, NULL);
	}

	void cMaterialType_SolidDiffuse::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderer *apRenderer)
	{
		SetupType(eOhplMatKind_SolidDiffuse, aRenderMode, apProgram, apRenderer);
	}

	void cMaterialType_SolidDiffuse::SetupMaterialSpecificData(	eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,
																iRenderer *apRenderer)
	{
		SetupMaterial(eOhplMatKind_SolidDiffuse, DiffuseVars(apMaterial), aRenderMode, apProgram, apMaterial, apRenderer);
	}

	void cMaterialType_SolidDiffuse::SetupObjectSpecificData(	eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable *apObject,
																iRenderer *apRenderer)
	{
		SetupObject(eOhplMatKind_SolidDiffuse, DiffuseVars(apObject->GetMaterial()), aRenderMode, apProgram, apObject, apRenderer);
	}

	iMaterialVars* cMaterialType_SolidDiffuse::CreateSpecificVariables()
	{
		return hplNew(cMaterialType_SolidDiffuse_Vars,());
	}

	void cMaterialType_SolidDiffuse::LoadVariables(cMaterial* apMaterial, cResourceVarsObject *apVars)
	{
		cMaterialType_SolidDiffuse_Vars *pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();
		if(pVars==NULL)
		{
			pVars = (cMaterialType_SolidDiffuse_Vars*)CreateSpecificVariables();
			apMaterial->SetVars(pVars);
		}

		rustmat::LoadVariables(eOhplMatKind_SolidDiffuse, apMaterial, apVars, static_cast<OhplSolidDiffuseVars*>(pVars));
	}

	void cMaterialType_SolidDiffuse::GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars)
	{
		VarValues(eOhplMatKind_SolidDiffuse, DiffuseVars(apMaterial), apMaterial, apVars);
	}

	//////////////////////////////////////////////////////////////////////////
	// PROJECTED UV
	//////////////////////////////////////////////////////////////////////////

	cMaterialType_ProjectedUV::cMaterialType_ProjectedUV(cGraphics *apGraphics, cResources *apResources) : iMaterialType_SolidBase(apGraphics, apResources)
	{
		InitFromRust(eOhplMatKind_ProjectedUv);
	}

	void cMaterialType_ProjectedUV::LoadSpecificData()
	{
		RunSetup(eOhplMatKind_ProjectedUv, false, MakeGlobals(), mpProgramManager, NULL);
	}

	void cMaterialType_ProjectedUV::CompileSolidSpecifics(cMaterial *apMaterial)
	{
		CompileMaterial(eOhplMatKind_ProjectedUv, ProjectedVars(apMaterial), apMaterial);
	}

	iTexture* cMaterialType_ProjectedUV::GetTextureForUnit(cMaterial *apMaterial,eMaterialRenderMode aRenderMode, int alUnit)
	{
		return TextureForUnit(eOhplMatKind_ProjectedUv, ProjectedVars(apMaterial), apMaterial, mpGraphics, aRenderMode, alUnit, mpDissolveTexture, NULL);
	}

	iGpuProgram* cMaterialType_ProjectedUV::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		return PickProgram(SelectProgram(eOhplMatKind_ProjectedUv, ProjectedVars(apMaterial), apMaterial, aRenderMode, alSkeleton), mpProgramManager, mpGlobalProgramManager, NULL);
	}

	void cMaterialType_ProjectedUV::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderer *apRenderer)
	{
		SetupType(eOhplMatKind_ProjectedUv, aRenderMode, apProgram, apRenderer);
	}

	void cMaterialType_ProjectedUV::SetupMaterialSpecificData(	eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,
																iRenderer *apRenderer)
	{
		SetupMaterial(eOhplMatKind_ProjectedUv, ProjectedVars(apMaterial), aRenderMode, apProgram, apMaterial, apRenderer);
	}

	void cMaterialType_ProjectedUV::SetupObjectSpecificData(	eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable *apObject,
																iRenderer *apRenderer)
	{
		SetupObject(eOhplMatKind_ProjectedUv, ProjectedVars(apObject->GetMaterial()), aRenderMode, apProgram, apObject, apRenderer);
	}

	void cMaterialType_ProjectedUV::LoadVariables(cMaterial* apMaterial, cResourceVarsObject *apVars)
	{
		cMaterialType_ProjectedUV_Vars *pVars = (cMaterialType_ProjectedUV_Vars*)apMaterial->GetVars();
		if(pVars==NULL)
		{
			pVars = (cMaterialType_ProjectedUV_Vars*)CreateSpecificVariables();
			apMaterial->SetVars(pVars);
		}

		rustmat::LoadVariables(eOhplMatKind_ProjectedUv, apMaterial, apVars, static_cast<OhplProjectedUvVars*>(pVars));
	}

	//////////////////////////////////////////////////////////////////////////
	// VERTEX BLEND
	//////////////////////////////////////////////////////////////////////////

	cMaterialType_VertexBlend::cMaterialType_VertexBlend(cGraphics *apGraphics, cResources *apResources) : iMaterialType_SolidBase(apGraphics, apResources)
	{
		InitFromRust(eOhplMatKind_VertexBlend);
	}

	void cMaterialType_VertexBlend::LoadSpecificData()
	{
		RunSetup(eOhplMatKind_VertexBlend, false, MakeGlobals(), mpProgramManager, NULL);
	}

	void cMaterialType_VertexBlend::CompileSolidSpecifics(cMaterial *apMaterial)
	{
		CompileMaterial(eOhplMatKind_VertexBlend, BlendVars(apMaterial), apMaterial);
	}

	iTexture* cMaterialType_VertexBlend::GetTextureForUnit(cMaterial *apMaterial,eMaterialRenderMode aRenderMode, int alUnit)
	{
		return TextureForUnit(eOhplMatKind_VertexBlend, BlendVars(apMaterial), apMaterial, mpGraphics, aRenderMode, alUnit, mpDissolveTexture, NULL);
	}

	iGpuProgram* cMaterialType_VertexBlend::GetGpuProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
	{
		return PickProgram(SelectProgram(eOhplMatKind_VertexBlend, BlendVars(apMaterial), apMaterial, aRenderMode, alSkeleton), mpProgramManager, mpGlobalProgramManager, NULL);
	}

	void cMaterialType_VertexBlend::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderer *apRenderer)
	{
		SetupType(eOhplMatKind_VertexBlend, aRenderMode, apProgram, apRenderer);
	}

	void cMaterialType_VertexBlend::SetupMaterialSpecificData(	eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial,
																iRenderer *apRenderer)
	{
		SetupMaterial(eOhplMatKind_VertexBlend, BlendVars(apMaterial), aRenderMode, apProgram, apMaterial, apRenderer);
	}

	void cMaterialType_VertexBlend::SetupObjectSpecificData(	eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable *apObject,
																iRenderer *apRenderer)
	{
		SetupObject(eOhplMatKind_VertexBlend, BlendVars(apObject->GetMaterial()), aRenderMode, apProgram, apObject, apRenderer);
	}

	void cMaterialType_VertexBlend::LoadVariables(cMaterial* apMaterial, cResourceVarsObject *apVars)
	{
		cMaterialType_VertexBlend_Vars *pVars = (cMaterialType_VertexBlend_Vars*)apMaterial->GetVars();
		if(pVars==NULL)
		{
			pVars = (cMaterialType_VertexBlend_Vars*)CreateSpecificVariables();
			apMaterial->SetVars(pVars);
		}

		rustmat::LoadVariables(eOhplMatKind_VertexBlend, apMaterial, apVars, static_cast<OhplVertexBlendVars*>(pVars));
	}
}
