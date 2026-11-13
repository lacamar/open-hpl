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

#include "scene/Light.h"

#include <algorithm>
#include <cstring>

#include "system/String.h"

#include "impl/tinyXML/tinyxml.h"


#include "system/LowLevelSystem.h"
#include "system/Platform.h"

#include "math/Math.h"

#include "resources/Resources.h"
#include "resources/TextureManager.h"
#include "resources/FileSearcher.h"

#include "scene/ParticleSystem.h"
#include "scene/World.h"
#include "scene/BillBoard.h"
#include "scene/SoundEntity.h"
#include "scene/MeshEntity.h"
#include "scene/RenderableContainer.h"
#include "scene/Camera.h"
#include "impl/RustBackends.h"

#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/SubMesh.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Renderer.h"



namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iLight::iLight(tString asName, cResources *apResources) : iRenderable(asName)
	{
		///////////////////////////////
		//Managers and highlevel init
		mpWorld = NULL;
		mpTextureManager = apResources->GetTextureManager();
		mpFileSearcher = apResources->GetFileSearcher();

		///////////////////////////////
		//Render properties
		mbApplyTransformToBV = false;

		mShadowMapResolution = eShadowMapResolution_High;
		mfShadowMapBlurAmount = 6.0f;
		mbOcclusionCullShadowCasters = false;

		mfShadowMapBiasMul = 1;
		mfShadowDistance = -1;
		mfShadowMapSlopeScaleBiasMul = 1;
			
		///////////////////////////////
		//Fade and flicker init
		mDiffuseColor = 0;
		mDefaultDiffuseColor = 0;
		mSpecularColor = 0;
		mbCastShadows = false;
		mlShadowCastersAffected = eObjectVariabilityFlag_All;
		mfRadius =0;
		mfSourceRadius = 0;
		memset((void*)&mAnim, 0, sizeof(mAnim));

		mfFalloffPow = 1.0f;
		mfSpotFalloffPow = 1.0f;
		mfBrightness = 1.0f;
		mlMaskID = 0xFFFFFFFF;
		mbMaskBox = false;
		mbGoboSpecular = false;

		///////////////////////////////
		//Data init
		static bool bFalloffMissing = mbHpl3Visibility;
		mpFalloffMap = bFalloffMissing ? NULL : mpTextureManager->Create1D("core_falloff_linear",false);
		bFalloffMissing = mpFalloffMap == NULL;
		if(mpFalloffMap)
		{
			mpFalloffMap->SetWrapS(eTextureWrap_ClampToEdge);
			mpFalloffMap->SetWrapT(eTextureWrap_ClampToEdge);
		}

		mpGoboTexture = NULL;


        mpVisibleNodeTracker = hplNew( cVisibleRCNodeTracker, () );
	}

	//-----------------------------------------------------------------------

	iLight::~iLight()
	{
		for(size_t i=0; i<mvParticleSystems.size(); ++i) mvParticleSystems[i]->mpLight = NULL;
		if(mpVisibleNodeTracker) hplDelete(mpVisibleNodeTracker);
		if(mpH11ShadowCache) ohpl_h11_shadow_cache_free(mpH11ShadowCache);
		if(mpFalloffMap) mpTextureManager->Destroy(mpFalloffMap);
		if(mpGoboTexture) mpTextureManager->Destroy(mpGoboTexture);
	}	

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void iLight::OnChangeVisible()
	{ 
		for(size_t i =0; i<mvBillboards.size(); ++i)
		{
			mvBillboards[i].mpBillboard->SetVisible(mbIsVisible);
		}
		for(size_t i=0; i<mvParticleSystems.size(); ++i) mvParticleSystems[i]->SetVisible(mbIsVisible);
	}

	bool iLight::mbHpl3Visibility = false;

	bool iLight::IsLit()
	{
		if(OhplBackendRustRequested()) return ohpl_h11_light_is_lit(&mDiffuseColor, mfBrightness, mbHpl3Visibility);
		if(mbHpl3Visibility)
			return (mDiffuseColor.r >0 || mDiffuseColor.g >0 || mDiffuseColor.b >0) && mfBrightness != 0;
		return mDiffuseColor.r >0 || mDiffuseColor.g >0 || mDiffuseColor.b >0 || mDiffuseColor.a >0;
	}

	bool iLight::IsVisible()
	{ 
		if(OhplBackendRustRequested()) return ohpl_h11_light_is_visible(IsLit(), mfRadius, mbIsVisible);
		if(!IsLit()) return false;
		if(mfRadius <= 0) return false;

		return mbIsVisible; 
	}

	//-----------------------------------------------------------------------


	void iLight::SetDiffuseColor(cColor aColor)
	{
		bool bWasVisble = IsLit();
		
		mDiffuseColor = aColor;

		//Check if the light changed its visibility
		if(mbIsVisible && IsLit() != bWasVisble && mpRenderCallback)
		{
			mpRenderCallback->OnVisibleChange(this); 
		}

		OnSetDiffuse();
	}

	void iLight::SetBrightness(float afX)
	{
		bool bWasVisble = IsLit();
		mfBrightness = afX;
		for(size_t i=0; i<mvParticleSystems.size(); ++i) mvParticleSystems[i]->mfLightBrightness = afX;
		if(mbIsVisible && IsLit() != bWasVisble && mpRenderCallback)
			mpRenderCallback->OnVisibleChange(this);
	}

	//-----------------------------------------------------------------------

	OhplLightLive iLight::LiveState() const
	{
		OhplLightLive live;
		live.diffuse = mDiffuseColor;
		live.radius = mfRadius;
		live.brightness = mfBrightness;
		return live;
	}

	void iLight::FadeBrightnessTo(float afBrightness, float afTime, std::function<float(float)> aEasing)
	{
		OhplLightLive live = LiveState();
		ohpl_light_fade_brightness_to(&mAnim, &live, afBrightness, afTime);
		mBrightFadeEasing = aEasing;
	}

	void iLight::UpdateLight(float afTimeStep)
	{
		OhplLightLive live = LiveState();
		OhplLightHostVt host;
		host.ctx = this;
		host.ease = [](void *p, float afT){
			iLight *pLight = static_cast<iLight*>(p);
			return pLight->mBrightFadeEasing ? pLight->mBrightFadeEasing(afT) : afT;
		};
		host.set_brightness = [](void *p, float afX){ static_cast<iLight*>(p)->SetBrightness(afX); };
		host.set_radius = [](void *p, float afX){ static_cast<iLight*>(p)->SetRadius(afX); };
		host.set_diffuse = [](void *p, const cColor *apCol){ static_cast<iLight*>(p)->SetDiffuseColor(*apCol); };
		host.assign_diffuse = [](void *p, const cColor *apCol){ static_cast<iLight*>(p)->mDiffuseColor = *apCol; };
		host.raw_radius = [](void *p, float afX){ static_cast<iLight*>(p)->mfRadius = afX; };
		host.flicker_sound = [](void *p, bool abOn){
			iLight *pLight = static_cast<iLight*>(p);
			cSoundEntity *pSound = pLight->mpWorld->CreateSoundEntity(abOn ? "FlickerOn" : "FlickerOff",
								abOn ? pLight->msFlickerOnSound : pLight->msFlickerOffSound, true);
			if(pSound)
			{
				pSound->SetIsSaved(false);
				pSound->SetPosition(pLight->GetWorldPosition());
			}
		};
		host.flicker_event = [](void *p, bool abOn){
			iLight *pLight = static_cast<iLight*>(p);
			if(abOn) pLight->OnFlickerOn(); else pLight->OnFlickerOff();
		};
		ohpl_light_update(&mAnim, &live, msFlickerOnSound != "", msFlickerOffSound != "", &host, afTimeStep);
	}

	//-----------------------------------------------------------------------

	void iLight::FadeTo(const cColor& aCol, float afRadius, float afTime)
	{
		OhplLightLive live = LiveState();
		ohpl_light_fade_to(&mAnim, &live, &aCol, afRadius, afTime);
	}

	void iLight::StopFading()
	{
		mAnim.fade_time = 0;
	}

	bool iLight::IsFading()
	{
		return mAnim.fade_time != 0;
	}

	//-----------------------------------------------------------------------

	void iLight::SetFlickerActive(bool abX)
	{
		mAnim.flickering = abX;
	}

	void iLight::SetFlicker(const cColor& aOffCol, float afOffRadius,
		float afOnMinLength, float afOnMaxLength,const tString &asOnSound,const tString &asOnPS,
		float afOffMinLength, float afOffMaxLength,const tString &asOffSound,const tString &asOffPS,
		bool abFade,	float afOnFadeMinLength, float afOnFadeMaxLength,
						float afOffFadeMinLength, float afOffFadeMaxLength)
	{
		msFlickerOnSound = asOnSound;
		msFlickerOnPS = asOnPS;
		msFlickerOffSound = asOffSound;
		msFlickerOffPS = asOffPS;

		OhplFlickerCfg cfg;
		cfg.off_color = aOffCol;
		cfg.off_radius = afOffRadius;
		cfg.on_min_length = afOnMinLength;
		cfg.on_max_length = afOnMaxLength;
		cfg.off_min_length = afOffMinLength;
		cfg.off_max_length = afOffMaxLength;
		cfg.fade = abFade;
		cfg.on_fade_min_length = afOnFadeMinLength;
		cfg.on_fade_max_length = afOnFadeMaxLength;
		cfg.off_fade_min_length = afOffFadeMinLength;
		cfg.off_fade_max_length = afOffFadeMaxLength;
		OhplLightLive live = LiveState();
		ohpl_light_set_flicker(&mAnim, &live, &cfg);
	}

	//-----------------------------------------------------------------------

	bool iLight::CheckObjectIntersection(iRenderable *apObject)
	{
		//Log("------ Checking %s with light %s -----\n",apObject->GetName().c_str(), GetName().c_str());
		//Log(" BV: min: %s max: %s\n",	apObject->GetBoundingVolume()->GetMin().ToString().c_str(),
		//								apObject->GetBoundingVolume()->GetMax().ToString().c_str());
		
		//////////////////////////////////////////////////////////////
		// If the lights cast shadows, cull objects that are in shadow
		if(mbCastShadows)
		{
			return CollidesWithBV(apObject->GetBoundingVolume());
		}
		/////////////////////////////////////////////////
		//Light is not in shadow, do not do any culling
		else
		{
			//Log("No shadow, using BV\n");
			return CollidesWithBV(apObject->GetBoundingVolume());
		}

				
	}

	//-----------------------------------------------------------------------

	void iLight::SetRadius(float afX)
	{ 
		if(mfRadius == afX) return;

		mfRadius = afX;

		mbUpdateBoundingVolume = true;
		
		//This is so that the render container is updated.
		SetTransformUpdated();
	}
	//-----------------------------------------------------------------------

	void iLight::UpdateLogic(float afTimeStep)
	{
		UpdateLight(afTimeStep);
		if(mAnim.fade_time>0 || mAnim.flickering)
		{
			mbUpdateBoundingVolume = true;
			
			//This is so that the render container is updated.
			//SetTransformUpdated();
		}
	}

	//-----------------------------------------------------------------------

	cBoundingVolume* iLight::GetBoundingVolume()
	{
		if(mbUpdateBoundingVolume)
		{
			UpdateBoundingVolume();
			if(mbMaskBox && OhplBackendRustRequested())
			{
				cVector3f vMin, vMax;
				ohpl_h11_light_mask_bounds(&mBoundingVolume.GetMin(), &mBoundingVolume.GetMax(), &mvMaskCenter, &mvMaskSize, &vMin, &vMax);
				mBoundingVolume.SetLocalMinMax(vMin, vMax);
				mBoundingVolume.SetTransform(cMatrixf::Identity);
			}
			else if(mbMaskBox)
			{
				cVector3f vHalf = mvMaskSize * 0.5f;
				mBoundingVolume.SetLocalMinMax(cMath::Vector3Max(mBoundingVolume.GetMin(), mvMaskCenter - vHalf),
											   cMath::Vector3Min(mBoundingVolume.GetMax(), mvMaskCenter + vHalf));
				mBoundingVolume.SetTransform(cMatrixf::Identity);
			}
			mbUpdateBoundingVolume = false;
		}

		return &mBoundingVolume;
	}
	
	//-----------------------------------------------------------------------

	cMatrixf* iLight::GetModelMatrix(cFrustum* apFrustum)
	{
		return &GetWorldMatrix();
	}
	
	//-----------------------------------------------------------------------
	
	iTexture *iLight::GetFalloffMap()
	{
		return mpFalloffMap;
	}

	void iLight::SetFalloffMap(iTexture* apTexture)
	{
		if(mpFalloffMap) mpTextureManager->Destroy(mpFalloffMap);

		mpFalloffMap = apTexture;
		mpFalloffMap->SetWrapS(eTextureWrap_ClampToEdge);
		mpFalloffMap->SetWrapT(eTextureWrap_ClampToEdge);
		
	}

	//-----------------------------------------------------------------------

	void iLight::SetGoboTexture(iTexture *apTexture)
	{
		//Destroy any old texture.
		if(mpGoboTexture) mpTextureManager->Destroy(mpGoboTexture);

		mpGoboTexture = apTexture;
		if(mpGoboTexture) mpGoboTexture->SetWrapSTR(eTextureWrap_ClampToEdge);
	}

	//-----------------------------------------------------------------------

	iTexture* iLight::GetGoboTexture()
	{
		return mpGoboTexture;	
	}

	//-----------------------------------------------------------------------

	void iLight::AddShadowCaster(iRenderable *apObject)
	{
		if(OhplBackendRustRequested())
		{
			if(!mpH11ShadowCache) mpH11ShadowCache = ohpl_h11_shadow_cache_new();
			ohpl_h11_shadow_cache_add(mpH11ShadowCache, (uintptr_t)apObject, apObject->GetTransformUpdateCount());
			return;
		}
		m_mapShadowCasterCache.insert(tShadowCasterCacheMap::value_type(apObject, apObject->GetTransformUpdateCount()));
	}
	
	bool iLight::ShadowCasterIsValid(iRenderable *apObject)
	{
		if(OhplBackendRustRequested())
		{
			return mpH11ShadowCache && ohpl_h11_shadow_cache_valid(mpH11ShadowCache, (uintptr_t)apObject, apObject->GetTransformUpdateCount());
		}
		tShadowCasterCacheMapIt it = m_mapShadowCasterCache.find(apObject);
		if(it == m_mapShadowCasterCache.end()) return false;

		return it->second == apObject->GetTransformUpdateCount();
	}
	
	static std::vector<OhplH11Caster> H11Casters(const tRenderableVec &avObjects)
	{
		std::vector<OhplH11Caster> vCasters(avObjects.size());
		for(size_t i=0; i<avObjects.size(); ++i)
		{
			vCasters[i].object = (uintptr_t)avObjects[i];
			vCasters[i].count = avObjects[i]->GetTransformUpdateCount();
			vCasters[i].is_static = avObjects[i]->IsStatic();
		}
		return vCasters;
	}

	bool iLight::ShadowCastersAreUnchanged(const tRenderableVec &avObjects)
	{
		if(OhplBackendRustRequested())
		{
			if(!mpH11ShadowCache) mpH11ShadowCache = ohpl_h11_shadow_cache_new();
			std::vector<OhplH11Caster> vCasters = H11Casters(avObjects);
			return ohpl_h11_shadow_cache_unchanged(mpH11ShadowCache, vCasters.data(), (int32_t)vCasters.size());
		}
		size_t lDynObjectCount=0;
		for(size_t i=0; i<avObjects.size(); ++i)
		{
			iRenderable *pObject = avObjects[i];
			if(pObject->IsStatic()==false)
			{
				lDynObjectCount++;
				if(ShadowCasterIsValid(avObjects[i])==false)
				{
					return false;
				}
			}
		}
		
		if(lDynObjectCount != m_mapShadowCasterCache.size()) return false;
		
		return true;
	}

	void iLight::SetShadowCasterCacheFromVec(const tRenderableVec &avObjects)
	{
		if(OhplBackendRustRequested())
		{
			if(!mpH11ShadowCache) mpH11ShadowCache = ohpl_h11_shadow_cache_new();
			std::vector<OhplH11Caster> vCasters = H11Casters(avObjects);
			ohpl_h11_shadow_cache_set(mpH11ShadowCache, vCasters.data(), (int32_t)vCasters.size());
			return;
		}
		m_mapShadowCasterCache.clear();

		for(size_t i=0; i<avObjects.size(); ++i)
		{
			if(avObjects[i]->IsStatic()==false) AddShadowCaster(avObjects[i]);
		}
	}
	
	void iLight::ClearShadowCasterCache()
	{
		if(OhplBackendRustRequested())
		{
			if(mpH11ShadowCache) ohpl_h11_shadow_cache_clear(mpH11ShadowCache);
			return;
		}
		m_mapShadowCasterCache.clear();
	}

	//-----------------------------------------------------------------------

	void iLight::LoadXMLProperties(const tString asFile)
	{
		tWString sPath = mpFileSearcher->GetFilePath(asFile);
		if(sPath != _W(""))
		{
			FILE *pFile = cPlatform::OpenFile(sPath, _W("rb"));
			if(pFile==NULL) return;

			TiXmlDocument *pDoc = hplNew( TiXmlDocument,() );
			if(pDoc->LoadFile(pFile))
			{
				TiXmlElement *pRootElem = pDoc->RootElement();
				
                TiXmlElement *pMainElem = pRootElem->FirstChildElement("MAIN");
				if(pMainElem!=NULL)
				{
					mbCastShadows = cString::ToBool(pMainElem->Attribute("CastsShadows"),mbCastShadows);

					mDiffuseColor.a = cString::ToFloat(pMainElem->Attribute("Specular"),mDiffuseColor.a);
					
					tString sFalloffImage = cString::ToString(pMainElem->Attribute("FalloffImage"),"");
					iTexture *pTexture = mpTextureManager->Create1D(sFalloffImage,false);
					if(pTexture) SetFalloffMap(pTexture);

					ExtraXMLProperties(pMainElem);
				}
				else
				{
					Error("Cannot find main element in %s\n",asFile.c_str());
				}
			}
			else
			{
				Error("Couldn't load file '%s'\n",asFile.c_str());
			}
			if(pFile) fclose(pFile);
			hplDelete(pDoc);
		}
		else
		{
			Error("Couldn't find file '%s'\n",asFile.c_str());
		}

	}

	//-----------------------------------------------------------------------

	void iLight::AttachBillboard(cBillboard *apBillboard, const cColor &aBaseColor)
	{
		cLightBillboardConnection bbConnection;

		bbConnection.mpBillboard = apBillboard;
		bbConnection.mBaseColor = aBaseColor;

		apBillboard->SetColor(aBaseColor * cColor(mDiffuseColor.r,mDiffuseColor.g,mDiffuseColor.b,1));
		apBillboard->SetVisible(IsVisible());

		mvBillboards.push_back(bbConnection);
	}

	//-----------------------------------------------------------------------

	void iLight::AttachParticleSystem(cParticleSystem *apPS)
	{
		apPS->mLightColor = cColor(mDiffuseColor.r,mDiffuseColor.g,mDiffuseColor.b,1);
		apPS->mfLightBrightness = mfBrightness;
		apPS->SetVisible(mbIsVisible);
		if(apPS->mpLight) apPS->mpLight->RemoveParticleSystem(apPS);
		apPS->mpLight = this;
		mvParticleSystems.push_back(apPS);
	}

	void iLight::RemoveParticleSystem(cParticleSystem *apPS)
	{
		std::vector<cParticleSystem*>::iterator it = std::find(mvParticleSystems.begin(), mvParticleSystems.end(), apPS);
		if(it == mvParticleSystems.end()) return;
		mvParticleSystems.erase(it);
		apPS->mpLight = NULL;
	}

	//-----------------------------------------------------------------------

	void iLight::RemoveBillboard(cBillboard *apBillboard)
	{
		std::vector<cLightBillboardConnection>::iterator it = mvBillboards.begin();
		for(; it != mvBillboards.end(); ++it)
		{
			cLightBillboardConnection &bbConnection = *it;
			if(bbConnection.mpBillboard == apBillboard)
			{
				mvBillboards.erase(it);
				break;
			}
		}
	}

	//-----------------------------------------------------------------------


	
	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iLight::RenderShadow(iRenderable *apObject,cRenderSettings *apRenderSettings,
				iLowLevelGraphics *apLowLevelGraphics)
	{
		
	}


	//-----------------------------------------------------------------------
	
	void iLight::OnFlickerOff()
	{
		//Particle system
		if(msFlickerOffPS!=""){
			cParticleSystem *pPS = mpWorld->CreateParticleSystem(GetName() + "_PS", msFlickerOffPS, cVector3f(1,1,1));
			if(pPS) pPS->SetMatrix(GetWorldMatrix());
		}
	}

	//-----------------------------------------------------------------------

	void iLight::OnFlickerOn()
	{
		//Particle system
		if(msFlickerOnPS!=""){
			cParticleSystem *pPS = mpWorld->CreateParticleSystem(GetName() + "_PS", msFlickerOnPS, cVector3f(1,1,1));
			if(pPS) pPS->SetMatrix(GetWorldMatrix());
		}

	}

	//-----------------------------------------------------------------------

	void iLight::OnSetDiffuse()
	{
		for(size_t i =0; i<mvBillboards.size(); ++i)
		{
			mvBillboards[i].mpBillboard->SetColor( mvBillboards[i].mBaseColor * cColor(mDiffuseColor.r,mDiffuseColor.g,mDiffuseColor.b,1));
		}
		for(size_t i=0; i<mvParticleSystems.size(); ++i)
			mvParticleSystems[i]->mLightColor = cColor(mDiffuseColor.r,mDiffuseColor.g,mDiffuseColor.b,1);
	}

	
	//-----------------------------------------------------------------------

}
