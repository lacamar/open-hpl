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

#ifndef HPL_MATERIAL_TYPE_H
#define HPL_MATERIAL_TYPE_H

#include "system/SystemTypes.h"
#include "engine/EngineTypes.h"
#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Material.h"
#include "graphics/RustMt.h"

namespace hpl {

	class cGraphics;
	class cResources;
	class iGpuProgram;
	class iRenderable;
	class iRenderer;
	class cResourceVarsObject;

	// Opaque: `this` is the Rust material type (rust/src/material/mt.rs); Rust owns the program managers and the type logic.
	class iMaterialType
	{
	public:
		static iMaterialType* Create(int alCode, cGraphics *apGraphics, cResources *apResources){ return static_cast<iMaterialType*>(ohpl_mt_new(OhplMtHostGet(), alCode, apGraphics, apResources)); }
		static iMaterialType* Blank(){ static iMaterialType *pBlank = Create(eOhplMtCode_Blank, NULL, NULL); return pBlank; }
		void Destroy(){ ohpl_mt_free(this); }

		void SetName(const tString& asName){ ohpl_mt_set_name(this, (const unsigned char*)asName.data(), asName.size()); }
		tString GetName(){ const unsigned char *p; size_t n; ohpl_mt_name(this, &p, &n); return tString((const char*)p, n); }

		bool IsTranslucent(){ return ohpl_mt_is_translucent(this); }
		bool IsDecal(){ return ohpl_mt_is_decal(this); }
		bool HasTypeSpecifics(eMaterialRenderMode aMode){ return ohpl_mt_has_type_specifics(this, aMode); }
		int GetCode(){ return ohpl_mt_code(this); }
		int GetUsedTextureNum(){ return ohpl_mt_used_texture_num(this); }
		int GetUsedTexture(int alIdx){ return ohpl_mt_used_texture(this, alIdx); }

		void LoadData(){ ohpl_mt_load_data(this); }
		void DestroyData(){ ohpl_mt_destroy_data(this); }
		void Reload(){ ohpl_mt_reload(this); }

		iGpuProgram* GetRenderProgram(cMaterial *apMaterial, eMaterialRenderMode aRenderMode, iRenderer *apRenderer){ return static_cast<iGpuProgram*>(ohpl_mt_render_program(this, apMaterial->mpRs, aRenderMode, apRenderer)); }
		void SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderer *apRenderer){ ohpl_mt_setup_type(this, aRenderMode, apProgram, apRenderer); }
		void SetupMaterialSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial *apMaterial, iRenderer *apRenderer){ ohpl_mt_setup_material(this, aRenderMode, apProgram, apMaterial->mpRs, apRenderer); }
		void SetupObjectSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable *apObject, iRenderer *apRenderer){ ohpl_mt_setup_object(this, aRenderMode, apProgram, apObject, apRenderer); }
		void LoadVariables(cMaterial *apMaterial, cResourceVarsObject *apVars){ ohpl_mt_load_variables(this, apMaterial->mpRs, apVars); }

		static OhplH7Mat* State(cMaterial *apMaterial){ return apMaterial->mpRs; }

	private:
		iMaterialType() = delete;
	};

	// Settings of the translucent type the renderers write and the other types read; the state lives in Rust.
	class cMaterialType_Translucent
	{
	public:
		static void SetLightProbes(bool abX){ ohpl_mt_set_light_probes(abX); }
		static void SetWorldFog(bool abUnderwater, bool abSecondary){ ohpl_mt_set_world_fog(abUnderwater, abSecondary); }
		static tFlag GetWorldFog(){ return ohpl_mt_get_world_fog(); }
		static void SetDepthOfField(bool abX, const cColor &avParams = cColor(0,0)){ ohpl_mt_set_depth_of_field(abX, avParams.r, avParams.g, avParams.b, avParams.a); }
		static bool GetLightProbes(){ return ohpl_mt_get_light_probes(); }
		static bool GetDepthOfField(){ return ohpl_mt_get_depth_of_field(); }
	};

	//---------------------------------------------------
	// Material variables: the Ohpl*Vars bases are laid out and defaulted by Rust
	//---------------------------------------------------

	class cMaterialType_SolidDiffuse_Vars : public iMaterialVars, public OhplSolidDiffuseVars
	{
	public:
		cMaterialType_SolidDiffuse_Vars(){ ohpl_mat_default_vars(eOhplMatKind_SolidDiffuse, static_cast<OhplSolidDiffuseVars*>(this)); }
	};

	class cMaterialType_ProjectedUV_Vars : public iMaterialVars, public OhplProjectedUvVars
	{
	public:
		cMaterialType_ProjectedUV_Vars(){ ohpl_mat_default_vars(eOhplMatKind_ProjectedUv, static_cast<OhplProjectedUvVars*>(this)); }
	};

	class cMaterialType_VertexBlend_Vars : public iMaterialVars, public OhplVertexBlendVars
	{
	public:
		cMaterialType_VertexBlend_Vars(){ ohpl_mat_default_vars(eOhplMatKind_VertexBlend, static_cast<OhplVertexBlendVars*>(this)); }
	};

	class cMaterialType_Translucent_Vars : public iMaterialVars, public OhplTranslucentVars
	{
	public:
		cMaterialType_Translucent_Vars(){ ohpl_mat_default_vars(eOhplMatKind_Translucent, static_cast<OhplTranslucentVars*>(this)); }
	};

	class cMaterialType_Water_Vars : public iMaterialVars, public OhplWaterVars
	{
	public:
		cMaterialType_Water_Vars(){ ohpl_mat_default_vars(eOhplMatKind_Water, static_cast<OhplWaterVars*>(this)); }
	};

	class cMaterialType_TerrainBlend_Vars : public iMaterialVars, public OhplTerrainBlendVars
	{
	public:
		cMaterialType_TerrainBlend_Vars(){ ohpl_mat_default_vars(eOhplMatKind_TerrainBlend, static_cast<OhplTerrainBlendVars*>(this)); }
	};

	class cMaterialType_Undergrowth_Vars : public iMaterialVars, public OhplUndergrowthVars
	{
	public:
		cMaterialType_Undergrowth_Vars(){ ohpl_mat_default_vars(eOhplMatKind_Undergrowth, static_cast<OhplUndergrowthVars*>(this)); }
	};

};
#endif // HPL_MATERIAL_TYPE_H
