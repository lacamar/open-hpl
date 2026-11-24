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

#ifndef HPL_MATERIAL_H
#define HPL_MATERIAL_H

#include "system/SystemTypes.h"
#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "resources/ResourceBase.h"
#include "graphics/RustMaterialH7.h"
#include "graphics/RustMeshMm.h"
#include <cstddef>

namespace hpl {

	//---------------------------------------------------

	class cGraphics;
	class cResources;
	class iTexture;
	class iGpuProgram;
	class iMaterialType;
	class cResourceVarsObject;

	//---------------------------------------------------
	
	class iMaterialVars
	{
	public:
		virtual ~iMaterialVars(){}
	};

	//---------------------------------------------------
	
	class cMaterialUvAnimation
	{
	public:
		cMaterialUvAnimation(eMaterialUvAnimation aType, float afSpeed, float afAmp, eMaterialAnimationAxis aAxis) :
							  mType(aType), mfSpeed(afSpeed), mfAmp(afAmp), mAxis(aAxis) {}

	    eMaterialUvAnimation mType;
		
		float mfSpeed;
		float mfAmp;

		eMaterialAnimationAxis mAxis;
	};

	//---------------------------------------------------
	
	// Plain cMaterial state, owned and laid out by rust/src/material/h7.rs (MatState); read in place.
	struct OhplH7Mat
	{
		iMaterialType *mpType;
		iMaterialVars *mpVars;
		cResources *mpResources;
		iGpuProgram *mvPrograms[2][eMaterialRenderMode_LastEnum];
		iTexture *mvTextures[eMaterialTexture_LastEnum];
		iTexture *mvTextureInUnit[eMaterialRenderMode_LastEnum][kMaxTextureUnits];
		const cMaterialUvAnimation *mpUvAnimations;
		cMatrixf m_mtxUV;
		cVector2l mvUVSubDivisions;
		float mfMaxReflectionDistance;
		int mlDecalSortOrder;
		int mlRenderFrameCount;
		tFlag mlCompiledWorldFog;
		int mBlendMode;
		int mAlphaMode;
		int mlUvAnimationNum;
		bool mbHasSpecificSettings[eMaterialRenderMode_LastEnum];
		bool mbHasObjectSpecificsSettings[eMaterialRenderMode_LastEnum];
		bool mbDepthTest, mbHasUvAnimation, mbAutoDestroyTextures, mbForceFullScaleTextures;
		bool mbHasRefraction, mbUseRefractionEdgeCheck, mbHasWorldReflection, mbWorldReflectionOcclusionTest;
		bool mbHasTranslucentIllumination, mbLargeTransperantSurface, mbAffectedByDepthOfField, mbAffectedByFog;
		bool mbUseAlphaDissolveFilter;
	};

	static_assert(offsetof(OhplH7Mat, m_mtxUV) == 1320 && offsetof(OhplH7Mat, mlUvAnimationNum) == 1416 && offsetof(OhplH7Mat, mbUseAlphaDissolveFilter) == 1446, "OhplH7Mat layout");

	//---------------------------------------------------

	class cMaterial : public iResourceBase 
	{
	friend class iMaterialType;
	public:
		cMaterial(const tString& asName, const tWString& asFullPath, cGraphics *apGraphics, cResources *apResources, iMaterialType *apType) :
			iResourceBase(asName, asFullPath, 0),
			mpRs(ohpl_mm_mat_new(OhplMmHostGet(), apType, apResources)),
			mlCompiledWorldFog(mpRs->mlCompiledWorldFog) {}
		virtual ~cMaterial();

		void SetType(iMaterialType* apType){ ohpl_mm_mat_set_type(mpRs, apType); }
		iMaterialType * GetType(){ return mpRs->mpType; }

		void Compile(){ ohpl_mm_mat_compile(mpRs); }
		void CompileTextureLookup(){ ohpl_mm_mat_compile_texture_lookup(mpRs); }

		void SetTexture(eMaterialTexture aType, iTexture *apTexture){ mpRs->mvTextures[aType] = apTexture; }
		iTexture *GetTexture(eMaterialTexture aType){ return mpRs->mvTextures[aType]; }

		iMaterialVars* GetVars(){ return mpRs->mpVars;}
		cResourceVarsObject* GetVarsObject(){ return (cResourceVarsObject*)ohpl_mm_mat_vars_object(mpRs); }
		void LoadVariablesFromVarsObject(cResourceVarsObject* apVarsObject){ ohpl_mm_mat_load_vars(mpRs, apVarsObject); }

		void SetAutoDestroyTextures(bool abX){ mpRs->mbAutoDestroyTextures = abX;}

		void SetBlendMode(eMaterialBlendMode aBlendMode){ ohpl_h7_mat_set_blend(mpRs, aBlendMode); }
		void SetAlphaMode(eMaterialAlphaMode aAlphaMode){ ohpl_h7_mat_set_alpha(mpRs, aAlphaMode); }
		void SetDepthTest(bool abDepthTest){ ohpl_h7_mat_set_depth_test(mpRs, abDepthTest); }

		bool HasRefraction(){ return mpRs->mbHasRefraction; }
		bool UseRefractionEdgeCheck(){ return mpRs->mbUseRefractionEdgeCheck;}

		bool HasWorldReflection(){ return mpRs->mbHasWorldReflection; }
		bool GetWorldReflectionOcclusionTest(){ return mpRs->mbWorldReflectionOcclusionTest;}
		float GetMaxReflectionDistance(){ return mpRs->mfMaxReflectionDistance;}

		bool HasTranslucentIllumination(){ return mpRs->mbHasTranslucentIllumination;}


		bool GetAffectedByDepthOfField(){ return mpRs->mbAffectedByDepthOfField;}


		bool GetAffectedByFog(){ return mpRs->mbAffectedByFog;}

		void SetDecalSortOrder(int alX){ mpRs->mlDecalSortOrder = alX;}

		void SetUVSubDivisions(const cVector2l& avX){ mpRs->mvUVSubDivisions = avX;}
		const cVector2l& GetUVSubDivisions(){ return mpRs->mvUVSubDivisions;}

		inline iTexture* GetTextureInUnit(eMaterialRenderMode aRenderMode, int alUnit) const { return mpRs->mvTextureInUnit[aRenderMode][alUnit];}
		inline iGpuProgram* GetProgram(char alSkeleton,eMaterialRenderMode aRenderMode) const { return mpRs->mvPrograms[(int)alSkeleton][aRenderMode];}
		void SetProgram(char alSkeleton,eMaterialRenderMode aRenderMode, iGpuProgram *apProgram){ mpRs->mvPrograms[(int)alSkeleton][aRenderMode] = apProgram;}

		inline eMaterialBlendMode GetBlendMode() const { return (eMaterialBlendMode)mpRs->mBlendMode; }
		inline eMaterialAlphaMode GetAlphaMode() const { return (eMaterialAlphaMode)mpRs->mAlphaMode; }
		inline bool GetDepthTest() const { return mpRs->mbDepthTest; }

		void SetForceFullScaleTextures(bool abX) { mpRs->mbForceFullScaleTextures = abX; }
		bool GetForceFullScaleTextures() const { return mpRs->mbForceFullScaleTextures; }

		void SetPhysicsMaterial(const tString & asPhysicsMaterial){ msPhysicsMaterial = asPhysicsMaterial;}
		const tString& GetPhysicsMaterial(){ return msPhysicsMaterial;}

		void UpdateBeforeRendering(float afTimeStep){ if(mpRs->mbHasUvAnimation) ohpl_h7_mat_update(mpRs, afTimeStep, NULL); }

		inline int GetRenderFrameCount() const { return mpRs->mlRenderFrameCount;}
		inline void SetRenderFrameCount(const int alCount) { mpRs->mlRenderFrameCount = alCount;}

		inline bool GetHasSpecificSettings(eMaterialRenderMode aMode) const{ return mpRs->mbHasSpecificSettings[aMode];}
		inline bool HasObjectSpecificsSettings(eMaterialRenderMode aMode)const { return mpRs->mbHasObjectSpecificsSettings[aMode];}

		//Animation
		void AddUvAnimation(eMaterialUvAnimation aType, float afSpeed, float afAmp, eMaterialAnimationAxis aAxis){ ohpl_h7_mat_add_uv_anim(mpRs, aType, afSpeed, afAmp, aAxis); }
		int GetUvAnimationNum(){ return mpRs->mlUvAnimationNum;}

		/**
		 * This is used so that materials do not call type specific things after types have been destroyed!
		 * Shall only be set by graphics!
		 */
		static void SetDestroyTypeSpecifics(bool abX){ ohpl_mm_mat_set_destroy_type_specifics(abX); }

		//resources stuff.
		bool Reload(){ return false;}
		void Unload(){}
		void Destroy(){}

	private:
		OhplH7Mat *mpRs;
	public:
		OhplH7Mat* Rs() const { return mpRs; }
	private:
		tString msPhysicsMaterial;
	public:
		tFlag &mlCompiledWorldFog;
	};

	//---------------------------------------------------

};
#endif // HPL_MATERIAL_H
