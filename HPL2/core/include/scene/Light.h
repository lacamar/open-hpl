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

#ifndef HPL_LIGHT_H
#define HPL_LIGHT_H

#include "scene/Entity3D.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Renderable.h"
#include <functional>
#include "scene/RustLight.h"
#include "scene/RustSceneH11.h"
#include "scene/RustSL.h"


namespace hpl {
	
	//------------------------------------------

	class iLowLevelGraphics;
	class cRenderSettings;
	class cCamera;
	class cFrustum;
	class iGpuProgram;
	class iTexture;
	class cTextureManager;
	class cResources;
	class cFileSearcher;
	class cBillboard;
	class cXmlElement;
	class cParticleSystem;
	class cSectorVisibilityContainer;
	class cWorld;
	class cVisibleRCNodeTracker;
	
	//------------------------------------------
	
	enum eLightType
	{
		eLightType_Point,
		eLightType_Spot,
		eLightType_Box,
		eLightType_Directional,
		eLightType_LastEnum
	};

	enum eShadowVolumeType
	{
		eShadowVolumeType_None,
		eShadowVolumeType_ZPass,
		eShadowVolumeType_ZFail,
		eShadowVolumeType_LastEnum,
	};

	//------------------------------------------


	//------------------------------------------


	//------------------------------------------

	class iLight : public iRenderable
	{
	#ifdef __GNUC__
		typedef iRenderable __super;
	#endif
	public:
		iLight(tString asName, cResources *apResources, eLightType aType);
		virtual ~iLight();

		void UpdateLogic(float afTimeStep);

		bool CheckObjectIntersection(iRenderable *apObject){ return CollidesWithBV(apObject->GetBoundingVolume());}
		
		eLightType GetLightType(){ return (eLightType)mpS->lightType;}

		iTexture *GetFalloffMap(){ return (iTexture*)mpS->falloffMap;}
		void SetFalloffMap(iTexture* apTexture);
		
		void SetGoboTexture(iTexture *apTexture);
		iTexture* GetGoboTexture(){ return (iTexture*)mpS->gobo;}

		///////////////////////////////
		//iEntity implementation
		tString GetEntityType(){ return "iLight";}

		virtual bool IsVisible();
		void OnChangeVisible();
		static void SetHpl3Visibility(bool abX){ ohpl_sl_set_hpl3_visibility(abX);}
		
		///////////////////////////////
		//Renderable implementation:
		cMaterial *GetMaterial(){ return NULL;}
		iVertexBuffer* GetVertexBuffer(){ return NULL;}

		eRenderableType GetRenderType(){ return eRenderableType_Light;}

		cBoundingVolume* GetBoundingVolume();

		int GetMatrixUpdateCount(){ return GetTransformUpdateCount();}

		cMatrixf* GetModelMatrix(cFrustum* apFrustum){ return &GetWorldMatrix();}

		inline void RenderShadow(iRenderable *apObject,cRenderSettings *apRenderSettings,iLowLevelGraphics *apLowLevelGraphics);

		void LoadXMLProperties(const tString asFile);

		void AttachBillboard(cBillboard *apBillboard, const cColor &aBaseColor);
		void RemoveBillboard(cBillboard *apBillboard);
		bool HasBillboard(cBillboard *apBillboard){ return ohpl_sl_light_has_billboard(mpS, (uintptr_t)apBillboard);}
				void AttachParticleSystem(cParticleSystem *apPS);
		void RemoveParticleSystem(cParticleSystem *apPS);

		//////////////////////////
		//Shadow caster cache
		void AddShadowCaster(iRenderable *apObject){ ohpl_sl_light_cache_add(mpS, (uintptr_t)apObject, apObject->GetTransformUpdateCount());}
		bool ShadowCasterIsValid(iRenderable *apObject){ return ohpl_sl_light_cache_valid(mpS, (uintptr_t)apObject, apObject->GetTransformUpdateCount());}
		bool ShadowCastersAreUnchanged(const tRenderableVec &avObjects);
		void SetShadowCasterCacheFromVec(const tRenderableVec &avObjects);
		void ClearShadowCasterCache(){ ohpl_sl_light_cache_clear(mpS);}

        //////////////////////////
		//Fading
		void FadeTo(const cColor& aCol, float afRadius, float afTime);
		void StopFading(){ mpS->anim.fade_time = 0;}
		bool IsFading(){ return mpS->anim.fade_time != 0;}
		cColor GetDestColor(){ return mpS->anim.dest_col;}
		float GetDestRadius(){ return mpS->anim.dest_radius;}


		//////////////////////////
		//FLickering
		void SetFlickerActive(bool abX){ mpS->anim.flickering = abX;}
		bool GetFlickerActive(){return mpS->anim.flickering;}

		void SetFlicker(const cColor& aOffCol, float afOffRadius,
			float afOnMinLength, float afOnMaxLength,const tString &asOnSound,const tString &asOnPS,
			float afOffMinLength, float afOffMaxLength,const tString &asOffSound,const tString &asOffPS,
			bool abFade,	float afOnFadeMinLength, float afOnFadeMaxLength, 
							float afOffFadeMinLength, float afOffFadeMaxLength);

		tString GetFlickerOffSound(){ return FlickerString(2);}
		tString GetFlickerOnSound(){ return FlickerString(0);}
		tString GetFlickerOffPS(){ return FlickerString(3);}
		tString GetFlickerOnPS(){ return FlickerString(1);}
		float GetFlickerOnMinLength(){ return mpS->anim.flicker_on_min_length;}
		float GetFlickerOffMinLength(){ return mpS->anim.flicker_off_min_length;}
		float GetFlickerOnMaxLength(){ return mpS->anim.flicker_on_max_length;}
		float GetFlickerOffMaxLength(){ return mpS->anim.flicker_off_max_length;}
		cColor GetFlickerOffColor(){ return mpS->anim.flicker_off_color;}
		float GetFlickerOffRadius(){ return mpS->anim.flicker_off_radius;}
		bool GetFlickerFade(){ return mpS->anim.flicker_fade;}
		float GetFlickerOnFadeMinLength(){ return mpS->anim.flicker_on_fade_min_length;}
		float GetFlickerOnFadeMaxLength(){ return mpS->anim.flicker_on_fade_max_length;}
		float GetFlickerOffFadeMinLength(){ return mpS->anim.flicker_off_fade_min_length;}
		float GetFlickerOffFadeMaxLength(){ return mpS->anim.flicker_on_fade_max_length;}

		cColor GetFlickerOnColor(){ return mpS->anim.flicker_on_color;}
		float GetFlickerOnRadius(){ return mpS->anim.flicker_on_radius;}

		//////////////////////////
		//Properties
		const cColor& GetDiffuseColor(){ return mpS->diffuse; }
		void SetDiffuseColor(cColor aColor);
		
		const cColor&  GetDefaultDiffuseColor(){ return mpS->defaultDiffuse;}
		void SetDefaultDiffuseColor(const cColor& aColor) { mpS->defaultDiffuse = aColor; }
		
		const cColor& GetSpecularColor(){ return mpS->specular; }
		void SetSpecularColor(cColor aColor){ mpS->specular = aColor; }

		bool GetCastShadows(){ return mpS->castShadows;}
		void SetCastShadows(bool afX){ mpS->castShadows = afX;}

		tObjectVariabilityFlag GetShadowCastersAffected(){ return mpS->shadowCastersAffected;}
		void SetShadowCastersAffected(tObjectVariabilityFlag alX){ mpS->shadowCastersAffected = alX;}

		inline eShadowMapResolution GetShadowMapResolution() const{ return (eShadowMapResolution)mpS->shadowRes;}
		inline void SetShadowMapResolution(eShadowMapResolution aQuality){ mpS->shadowRes = aQuality;}

		inline float GetShadowMapBlurAmount() const{ return mpS->shadowBlur;}
		inline void SetShadowMapBlurAmount(float afX){ mpS->shadowBlur  = afX;}

		inline bool GetOcclusionCullShadowCasters() const{ return mpS->occlusionCull;}
		inline void SetOcclusionCullShadowCasters(bool abX){ mpS->occlusionCull  = abX;}

		inline cVisibleRCNodeTracker * GetVisibleNodeTracker(){ return (cVisibleRCNodeTracker*)mpS->tracker;}

		float GetShadowMapBiasMul(){ return mpS->shadowBiasMul;}
		float GetShadowMapSlopeScaleBiasMul(){ return mpS->shadowSlopeBiasMul;}
		void SetShadowMapBiasMul(float afX){ mpS->shadowBiasMul = afX;}
		float GetShadowDistance(){ return mpS->shadowDistance;}
		void SetShadowDistance(float afX){ mpS->shadowDistance = afX;}
		void SetShadowMapSlopeScaleBiasMul(float afX){ mpS->shadowSlopeBiasMul = afX;}
		
		virtual void SetRadius(float afX);
		float GetRadius(){return mpS->radius;}

		void SetFalloffPow(float afX){ mpS->falloffPow = afX;}
		float GetFalloffPow(){ return mpS->falloffPow;}
		void SetSpotFalloffPow(float afX){ mpS->spotFalloffPow = afX;}
		float GetSpotFalloffPow(){ return mpS->spotFalloffPow;}
		void SetTranslucency(float afX){ mpS->translucency = afX;}
		float GetTranslucency(){ return mpS->translucency;}
		void SetBrightness(float afX);
		float GetBrightness(){ return mpS->brightness;}
		void SetDefaultBrightness(float afX){ mpS->defaultBrightness = afX;}
		float GetDefaultBrightness(){ return mpS->defaultBrightness;}
		void FadeBrightnessTo(float afBrightness, float afTime, std::function<float(float)> aEasing = nullptr);
		void SetMaskID(unsigned int alX){ mpS->maskId = alX;}
		void SetGoboSpecular(bool abX){ mpS->goboSpecular = abX;}
		bool GetGoboSpecular(){ return mpS->goboSpecular;}
		void SetCastDiffuseLight(bool abX){ mpS->castDiffuse = abX;}
		bool GetCastDiffuseLight(){ return mpS->castDiffuse;}
		void SetCastSpecularLight(bool abX){ mpS->castSpecular = abX;}
		bool GetCastSpecularLight(){ return mpS->castSpecular;}
		unsigned int GetMaskID(){ return mpS->maskId;}
		void SetMaskBox(bool abActive, const cVector3f& avCenter=0, const cVector3f& avSize=0){ mpS->maskBox = abActive; mpS->maskCenter = avCenter; mpS->maskSize = avSize; mbUpdateBoundingVolume = true; SetTransformUpdated();}
		bool HasMaskBox(){ return mpS->maskBox;}
		const cVector3f& GetMaskCenter(){ return mpS->maskCenter;}
		const cVector3f& GetMaskSize(){ return mpS->maskSize;}


		float GetSourceRadius(){ return mpS->sourceRadius;}
		void SetSourceRadius(float afX){ mpS->sourceRadius = afX;}

		void UpdateLight(float afTimeStep);

		void SetWorld(cWorld *apWorld){ mpS->world = (uintptr_t)apWorld;}


	protected:
		void OnFlickerOff();
		void OnFlickerOn();
		void OnSetDiffuse();
		tString FlickerString(int alWhich)
		{
			size_t lLen; const char *pStr = ohpl_sl_light_string(mpS, alWhich, &lLen);
			return tString(pStr, lLen);
		}

        virtual void ExtraXMLProperties(cXmlElement *apMainElem){}
		virtual void UpdateBoundingVolume()=0;

		bool IsLit(){ return ohpl_sl_light_is_lit(mpS);}

		cTextureManager *TextureManager(){ return (cTextureManager*)mpS->texMgr;}
		cWorld *World(){ return (cWorld*)mpS->world;}

		std::function<float(float)> mBrightFadeEasing;
		OhplSLLight *mpS;
	};

	typedef std::list<iLight*> tLightList;
	typedef tLightList::iterator tLightListIt;
};
#endif // HPL_LIGHT_H
