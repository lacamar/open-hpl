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

class TiXmlElement;

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

	typedef std::map<iRenderable*, int> tShadowCasterCacheMap;
	typedef tShadowCasterCacheMap::iterator tShadowCasterCacheMapIt;

	//------------------------------------------

	class cLightBillboardConnection
	{
	public:
		cBillboard *mpBillboard;
		cColor mBaseColor;
	};

	//------------------------------------------

	class iLight : public iRenderable
	{
	#ifdef __GNUC__
		typedef iRenderable __super;
	#endif
	public:
		iLight(tString asName, cResources *apResources);
		virtual ~iLight();

		void UpdateLogic(float afTimeStep);

		bool CheckObjectIntersection(iRenderable *apObject);
		
		eLightType GetLightType(){ return mLightType;}

		iTexture *GetFalloffMap();
		void SetFalloffMap(iTexture* apTexture);
		
		void SetGoboTexture(iTexture *apTexture);
		iTexture* GetGoboTexture();

		///////////////////////////////
		//iEntity implementation
		tString GetEntityType(){ return "iLight";}

		virtual bool IsVisible();
		void OnChangeVisible();
		static void SetHpl3Visibility(bool abX){ mbHpl3Visibility = abX;}
		
		///////////////////////////////
		//Renderable implementation:
		cMaterial *GetMaterial(){ return NULL;}
		iVertexBuffer* GetVertexBuffer(){ return NULL;}

		eRenderableType GetRenderType(){ return eRenderableType_Light;}

		cBoundingVolume* GetBoundingVolume();

		int GetMatrixUpdateCount(){ return GetTransformUpdateCount();}

		cMatrixf* GetModelMatrix(cFrustum* apFrustum);

		inline void RenderShadow(iRenderable *apObject,cRenderSettings *apRenderSettings,iLowLevelGraphics *apLowLevelGraphics);

		void LoadXMLProperties(const tString asFile);

		void AttachBillboard(cBillboard *apBillboard, const cColor &aBaseColor);
		void RemoveBillboard(cBillboard *apBillboard);
		std::vector<cLightBillboardConnection>* GetBillboardVec(){ return &mvBillboards;}
		void AttachParticleSystem(cParticleSystem *apPS);
		void RemoveParticleSystem(cParticleSystem *apPS);

		//////////////////////////
		//Shadow caster cache
		void AddShadowCaster(iRenderable *apObject);
		bool ShadowCasterIsValid(iRenderable *apObject);
		bool ShadowCastersAreUnchanged(const tRenderableVec &avObjects);
		void SetShadowCasterCacheFromVec(const tRenderableVec &avObjects);
		void ClearShadowCasterCache();

        //////////////////////////
		//Fading
		void FadeTo(const cColor& aCol, float afRadius, float afTime);
		void StopFading();
		bool IsFading();
		cColor GetDestColor(){ return mAnim.dest_col;}
		float GetDestRadius(){ return mAnim.dest_radius;}


		//////////////////////////
		//FLickering
		void SetFlickerActive(bool abX);
		bool GetFlickerActive(){return mAnim.flickering;}

		void SetFlicker(const cColor& aOffCol, float afOffRadius,
			float afOnMinLength, float afOnMaxLength,const tString &asOnSound,const tString &asOnPS,
			float afOffMinLength, float afOffMaxLength,const tString &asOffSound,const tString &asOffPS,
			bool abFade,	float afOnFadeMinLength, float afOnFadeMaxLength, 
							float afOffFadeMinLength, float afOffFadeMaxLength);

		tString GetFlickerOffSound(){ return msFlickerOffSound;}
		tString GetFlickerOnSound(){ return msFlickerOnSound;}
		tString GetFlickerOffPS(){ return msFlickerOffPS;}
		tString GetFlickerOnPS(){ return msFlickerOnPS;}
		float GetFlickerOnMinLength(){ return mAnim.flicker_on_min_length;}
		float GetFlickerOffMinLength(){ return mAnim.flicker_off_min_length;}
		float GetFlickerOnMaxLength(){ return mAnim.flicker_on_max_length;}
		float GetFlickerOffMaxLength(){ return mAnim.flicker_off_max_length;}
		cColor GetFlickerOffColor(){ return mAnim.flicker_off_color;}
		float GetFlickerOffRadius(){ return mAnim.flicker_off_radius;}
		bool GetFlickerFade(){ return mAnim.flicker_fade;}
		float GetFlickerOnFadeMinLength(){ return mAnim.flicker_on_fade_min_length;}
		float GetFlickerOnFadeMaxLength(){ return mAnim.flicker_on_fade_max_length;}
		float GetFlickerOffFadeMinLength(){ return mAnim.flicker_off_fade_min_length;}
		float GetFlickerOffFadeMaxLength(){ return mAnim.flicker_on_fade_max_length;}

		cColor GetFlickerOnColor(){ return mAnim.flicker_on_color;}
		float GetFlickerOnRadius(){ return mAnim.flicker_on_radius;}

		//////////////////////////
		//Properties
		const cColor& GetDiffuseColor(){ return mDiffuseColor; }
		void SetDiffuseColor(cColor aColor);
		
		const cColor&  GetDefaultDiffuseColor(){ return mDefaultDiffuseColor;}
		void SetDefaultDiffuseColor(const cColor& aColor) { mDefaultDiffuseColor = aColor; }
		
		const cColor& GetSpecularColor(){ return mSpecularColor; }
		void SetSpecularColor(cColor aColor){ mSpecularColor = aColor; }

		bool GetCastShadows(){ return mbCastShadows;}
		void SetCastShadows(bool afX){ mbCastShadows = afX;}

		tObjectVariabilityFlag GetShadowCastersAffected(){ return mlShadowCastersAffected;}
		void SetShadowCastersAffected(tObjectVariabilityFlag alX){ mlShadowCastersAffected = alX;}

		inline eShadowMapResolution GetShadowMapResolution() const{ return mShadowMapResolution;}
		inline void SetShadowMapResolution(eShadowMapResolution aQuality){ mShadowMapResolution  = aQuality;}

		inline float GetShadowMapBlurAmount() const{ return mfShadowMapBlurAmount;}
		inline void SetShadowMapBlurAmount(float afX){ mfShadowMapBlurAmount  = afX;}

		inline bool GetOcclusionCullShadowCasters() const{ return mbOcclusionCullShadowCasters;}
		inline void SetOcclusionCullShadowCasters(bool abX){ mbOcclusionCullShadowCasters  = abX;}

		inline cVisibleRCNodeTracker * GetVisibleNodeTracker(){ return mpVisibleNodeTracker;}

		float GetShadowMapBiasMul(){ return mfShadowMapBiasMul;}
		float GetShadowMapSlopeScaleBiasMul(){ return mfShadowMapSlopeScaleBiasMul;}
		void SetShadowMapBiasMul(float afX){ mfShadowMapBiasMul = afX;}
		float GetShadowDistance(){ return mfShadowDistance;}
		void SetShadowDistance(float afX){ mfShadowDistance = afX;}
		void SetShadowMapSlopeScaleBiasMul(float afX){ mfShadowMapSlopeScaleBiasMul = afX;}
		
		virtual void SetRadius(float afX);
		float GetRadius(){return mfRadius;}

		void SetFalloffPow(float afX){ mfFalloffPow = afX;}
		float GetFalloffPow(){ return mfFalloffPow;}
		void SetSpotFalloffPow(float afX){ mfSpotFalloffPow = afX;}
		float GetSpotFalloffPow(){ return mfSpotFalloffPow;}
		void SetTranslucency(float afX){ mfTranslucency = afX;}
		float GetTranslucency(){ return mfTranslucency;}
		void SetBrightness(float afX);
		float GetBrightness(){ return mfBrightness;}
		void SetDefaultBrightness(float afX){ mfDefaultBrightness = afX;}
		float GetDefaultBrightness(){ return mfDefaultBrightness;}
		void FadeBrightnessTo(float afBrightness, float afTime, std::function<float(float)> aEasing = nullptr);
		void SetMaskID(unsigned int alX){ mlMaskID = alX;}
		void SetGoboSpecular(bool abX){ mbGoboSpecular = abX;}
		bool GetGoboSpecular(){ return mbGoboSpecular;}
		void SetCastDiffuseLight(bool abX){ mbCastDiffuseLight = abX;}
		bool GetCastDiffuseLight(){ return mbCastDiffuseLight;}
		void SetCastSpecularLight(bool abX){ mbCastSpecularLight = abX;}
		bool GetCastSpecularLight(){ return mbCastSpecularLight;}
		unsigned int GetMaskID(){ return mlMaskID;}
		void SetMaskBox(bool abActive, const cVector3f& avCenter=0, const cVector3f& avSize=0){ mbMaskBox = abActive; mvMaskCenter = avCenter; mvMaskSize = avSize; mbUpdateBoundingVolume = true; SetTransformUpdated();}
		bool HasMaskBox(){ return mbMaskBox;}
		const cVector3f& GetMaskCenter(){ return mvMaskCenter;}
		const cVector3f& GetMaskSize(){ return mvMaskSize;}


		float GetSourceRadius(){ return mfSourceRadius;}
		void SetSourceRadius(float afX){ mfSourceRadius = afX;}

		void UpdateLight(float afTimeStep);

		void SetWorld(cWorld *apWorld){ mpWorld = apWorld;}


	protected:
		void OnFlickerOff();
		void OnFlickerOn();
		void OnSetDiffuse();
		OhplLightLive LiveState() const;

        virtual void ExtraXMLProperties(TiXmlElement *apMainElem){}
		virtual void UpdateBoundingVolume()=0;
		
		eLightType mLightType;

		cTextureManager *mpTextureManager;
		cFileSearcher *mpFileSearcher;
		cWorld *mpWorld;

		iTexture *mpFalloffMap;
		
		iTexture *mpGoboTexture;

		eShadowMapResolution mShadowMapResolution;
		float mfShadowMapBlurAmount;
		bool mbOcclusionCullShadowCasters;

		cVisibleRCNodeTracker *mpVisibleNodeTracker;

		std::vector<cLightBillboardConnection> mvBillboards;
		std::vector<cParticleSystem*> mvParticleSystems;

		cColor mDiffuseColor;
		cColor mDefaultDiffuseColor;

		cColor mSpecularColor;
		float mfSourceRadius;
		float mfRadius;
		float mfFalloffPow;
		float mfSpotFalloffPow;
		float mfTranslucency = 1;
		float mfBrightness;
		float mfDefaultBrightness = 1;
		std::function<float(float)> mBrightFadeEasing;
		static bool mbHpl3Visibility;
		bool IsLit();
		unsigned int mlMaskID;
		bool mbGoboSpecular;
		bool mbCastDiffuseLight = true;
		bool mbCastSpecularLight = true;
		bool mbMaskBox;
		cVector3f mvMaskCenter;
		cVector3f mvMaskSize;

		bool mbCastShadows;
		tObjectVariabilityFlag mlShadowCastersAffected;

		tShadowCasterCacheMap m_mapShadowCasterCache;
		OhplH11ShadowCache *mpH11ShadowCache = NULL;

		float mfShadowMapBiasMul;
		float mfShadowDistance;
		float mfShadowMapSlopeScaleBiasMul;

		OhplLightAnim mAnim;

		tString msFlickerOffSound;
		tString msFlickerOnSound;
		tString msFlickerOffPS;
		tString msFlickerOnPS;
	};

	typedef std::list<iLight*> tLightList;
	typedef tLightList::iterator tLightListIt;
};
#endif // HPL_LIGHT_H
