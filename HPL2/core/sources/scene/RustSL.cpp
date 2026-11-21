// Engine-side glue of the Rust-owned light, billboard and beam shells (state and logic: rust/src/scene/sl.rs).

#include "scene/Light.h"
#include "scene/LightPoint.h"
#include "scene/LightSpot.h"
#include "scene/LightBox.h"
#include "scene/BillBoard.h"
#include "scene/Beam.h"
#include "scene/LensFlare.h"
#include "scene/SubMeshEntity.h"
#include "scene/RustSL.h"
#include "scene/RustSceneH11.h"
#include "scene/RustSceneH12.h"
#include "scene/RustParticles.h"

#include <algorithm>
#include <cstring>

#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "impl/XmlDocumentTiny.h"
#include "math/Math.h"
#include "math/Frustum.h"
#include "resources/Resources.h"
#include "resources/TextureManager.h"
#include "resources/MaterialManager.h"
#include "resources/FileSearcher.h"
#include "resources/XmlDocument.h"
#include "scene/ParticleSystem.h"
#include "scene/World.h"
#include "scene/Scene.h"
#include "scene/SoundEntity.h"
#include "scene/RenderableContainer.h"
#include "scene/Camera.h"
#include "graphics/Graphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Material.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Renderer.h"
#include "graphics/RendererDeferred.h"
#include "engine/Engine.h"

namespace hpl {

	void OhplSLDumpLight(iLight *apLight, cWorld *apWorld);

	//////////////////////////////////////////////////////////////////////////
	// iLight
	//////////////////////////////////////////////////////////////////////////

	iLight::iLight(tString asName, cResources *apResources, eLightType aType) : iRenderable(asName), mpS(ohpl_sl_light_new(aType))
	{
		cTextureManager *pTextureManager = apResources->GetTextureManager();
		mpS->texMgr = (uintptr_t)pTextureManager;
		mpS->fileSearcher = (uintptr_t)apResources->GetFileSearcher();

		mpE->applyBV = false;

		static bool bFalloffMissing = ohpl_sl_hpl3_visibility();
		iTexture *pFalloffMap = bFalloffMissing ? NULL : pTextureManager->Create1D("core_falloff_linear",false);
		bFalloffMissing = pFalloffMap == NULL;
		if(pFalloffMap)
		{
			pFalloffMap->SetWrapS(eTextureWrap_ClampToEdge);
			pFalloffMap->SetWrapT(eTextureWrap_ClampToEdge);
		}
		mpS->falloffMap = (uintptr_t)pFalloffMap;
		mpS->tracker = (uintptr_t)hplNew( cVisibleRCNodeTracker, () );
	}

	iLight::~iLight()
	{
		for(size_t i=0; i<ohpl_sl_light_ps_count(mpS); ++i) ((cParticleSystem*)ohpl_sl_light_ps_at(mpS, i))->SetLight(NULL);
		if(mpS->tracker) hplDelete((cVisibleRCNodeTracker*)mpS->tracker);
		if(mpS->falloffMap) TextureManager()->Destroy((iTexture*)mpS->falloffMap);
		if(mpS->gobo) TextureManager()->Destroy((iTexture*)mpS->gobo);
		ohpl_sl_light_free(mpS);
	}

	void iLight::OnChangeVisible()
	{
		for(size_t i=0, n=ohpl_sl_light_billboard_count(mpS); i<n; ++i)
		{
			cColor base;
			((cBillboard*)ohpl_sl_light_billboard_at(mpS, i, &base))->SetVisible(mbIsVisible);
		}
		for(size_t i=0, n=ohpl_sl_light_ps_count(mpS); i<n; ++i) ((cParticleSystem*)ohpl_sl_light_ps_at(mpS, i))->SetVisible(mbIsVisible);
	}

	bool iLight::IsVisible()
	{
		return ohpl_sl_light_is_visible(mpS, mbIsVisible);
	}

	void iLight::SetDiffuseColor(cColor aColor)
	{
		bool bLitChanged = ohpl_sl_light_set_diffuse(mpS, &aColor);
		if(mbIsVisible && bLitChanged && mpRenderCallback)
		{
			mpRenderCallback->OnVisibleChange(this);
		}
		OnSetDiffuse();
	}

	void iLight::SetBrightness(float afX)
	{
		bool bLitChanged = ohpl_sl_light_set_brightness(mpS, afX);
		for(size_t i=0, n=ohpl_sl_light_ps_count(mpS); i<n; ++i) ((cParticleSystem*)ohpl_sl_light_ps_at(mpS, i))->SetLightBrightness(afX);
		if(mbIsVisible && bLitChanged && mpRenderCallback)
			mpRenderCallback->OnVisibleChange(this);
	}

	void iLight::FadeBrightnessTo(float afBrightness, float afTime, std::function<float(float)> aEasing)
	{
		ohpl_sl_light_fade_brightness_to(mpS, afBrightness, afTime);
		mBrightFadeEasing = aEasing;
	}

	void iLight::UpdateLight(float afTimeStep)
	{
		OhplSLHostVt host;
		host.ctx = this;
		host.ease = [](void *p, float afT){
			iLight *pLight = static_cast<iLight*>(p);
			return pLight->mBrightFadeEasing ? pLight->mBrightFadeEasing(afT) : afT;
		};
		host.setBrightness = [](void *p, float afX){ static_cast<iLight*>(p)->SetBrightness(afX); };
		host.setRadius = [](void *p, float afX){ static_cast<iLight*>(p)->SetRadius(afX); };
		host.setDiffuse = [](void *p, const cColor *apCol){ static_cast<iLight*>(p)->SetDiffuseColor(*apCol); };
		host.flickerSound = [](void *p, bool abOn){
			iLight *pLight = static_cast<iLight*>(p);
			cSoundEntity *pSound = pLight->World()->CreateSoundEntity(abOn ? "FlickerOn" : "FlickerOff",
								pLight->FlickerString(abOn ? 0 : 2), true);
			if(pSound)
			{
				pSound->SetIsSaved(false);
				pSound->SetPosition(pLight->GetWorldPosition());
			}
		};
		host.flickerEvent = [](void *p, bool abOn){
			iLight *pLight = static_cast<iLight*>(p);
			if(abOn) pLight->OnFlickerOn(); else pLight->OnFlickerOff();
		};
		ohpl_sl_light_update(mpS, &host, afTimeStep);
	}

	void iLight::FadeTo(const cColor& aCol, float afRadius, float afTime)
	{
		ohpl_sl_light_fade_to(mpS, &aCol, afRadius, afTime);
	}

	void iLight::SetFlicker(const cColor& aOffCol, float afOffRadius,
		float afOnMinLength, float afOnMaxLength,const tString &asOnSound,const tString &asOnPS,
		float afOffMinLength, float afOffMaxLength,const tString &asOffSound,const tString &asOffPS,
		bool abFade,	float afOnFadeMinLength, float afOnFadeMaxLength,
						float afOffFadeMinLength, float afOffFadeMaxLength)
	{
		const char *vStrings[4] = { asOnSound.c_str(), asOnPS.c_str(), asOffSound.c_str(), asOffPS.c_str() };
		const size_t vLens[4] = { asOnSound.size(), asOnPS.size(), asOffSound.size(), asOffPS.size() };
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
		ohpl_sl_light_set_flicker(mpS, &cfg, &vStrings, &vLens);
	}

	void iLight::SetRadius(float afX)
	{
		if(mpS->radius == afX) return;
		mpS->radius = afX;
		mpE->updateBV = true;
		SetTransformUpdated();
	}

	void iLight::UpdateLogic(float afTimeStep)
	{
		OhplSLDumpLight(this, World());
		UpdateLight(afTimeStep);
		if(mpS->anim.fade_time>0 || mpS->anim.flickering)
		{
			mpE->updateBV = true;
		}
	}

	cBoundingVolume* iLight::GetBoundingVolume()
	{
		if(mpE->updateBV)
		{
			UpdateBoundingVolume();
			if(mpS->maskBox)
			{
				cVector3f vMin, vMax;
				ohpl_h11_light_mask_bounds(&mBoundingVolume.GetMin(), &mBoundingVolume.GetMax(), &mpS->maskCenter, &mpS->maskSize, &vMin, &vMax);
				mBoundingVolume.SetLocalMinMax(vMin, vMax);
				mBoundingVolume.SetTransform(cMatrixf::Identity);
			}
			mpE->updateBV = false;
		}
		return &mBoundingVolume;
	}

	void iLight::SetFalloffMap(iTexture* apTexture)
	{
		if(mpS->falloffMap) TextureManager()->Destroy((iTexture*)mpS->falloffMap);
		mpS->falloffMap = (uintptr_t)apTexture;
		apTexture->SetWrapS(eTextureWrap_ClampToEdge);
		apTexture->SetWrapT(eTextureWrap_ClampToEdge);
	}

	void iLight::SetGoboTexture(iTexture *apTexture)
	{
		if(mpS->gobo) TextureManager()->Destroy((iTexture*)mpS->gobo);
		mpS->gobo = (uintptr_t)apTexture;
		if(apTexture) apTexture->SetWrapSTR(eTextureWrap_ClampToEdge);
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
		std::vector<OhplH11Caster> vCasters = H11Casters(avObjects);
		return ohpl_sl_light_cache_unchanged(mpS, vCasters.data(), (int)vCasters.size());
	}

	void iLight::SetShadowCasterCacheFromVec(const tRenderableVec &avObjects)
	{
		std::vector<OhplH11Caster> vCasters = H11Casters(avObjects);
		ohpl_sl_light_cache_set(mpS, vCasters.data(), (int)vCasters.size());
	}

	void iLight::LoadXMLProperties(const tString asFile)
	{
		cFileSearcher *pFileSearcher = (cFileSearcher*)mpS->fileSearcher;
		tWString sPath = pFileSearcher->GetFilePath(asFile);
		if(sPath != _W(""))
		{
			FILE *pFile = cPlatform::OpenFile(sPath, _W("rb"));
			if(pFile==NULL) return;
			fclose(pFile);

			cXmlDocumentTiny doc("");
			if(doc.CreateFromFile(sPath))
			{
				cXmlElement *pMainElem = doc.GetFirstElement("MAIN");
				if(pMainElem!=NULL)
				{
					mpS->castShadows = cString::ToBool(pMainElem->GetAttribute("CastsShadows"),mpS->castShadows);
					mpS->diffuse.a = cString::ToFloat(pMainElem->GetAttribute("Specular"),mpS->diffuse.a);

					tString sFalloffImage = cString::ToString(pMainElem->GetAttribute("FalloffImage"),"");
					iTexture *pTexture = TextureManager()->Create1D(sFalloffImage,false);
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
		}
		else
		{
			Error("Couldn't find file '%s'\n",asFile.c_str());
		}
	}

	void iLight::AttachBillboard(cBillboard *apBillboard, const cColor &aBaseColor)
	{
		const cColor vBase = aBaseColor;
		apBillboard->SetColor(vBase * cColor(mpS->diffuse.r,mpS->diffuse.g,mpS->diffuse.b,1));
		apBillboard->SetVisible(IsVisible());
		ohpl_sl_light_attach_billboard(mpS, (uintptr_t)apBillboard, &vBase);
	}

	void iLight::AttachParticleSystem(cParticleSystem *apPS)
	{
		apPS->SetLightColor(cColor(mpS->diffuse.r,mpS->diffuse.g,mpS->diffuse.b,1));
		apPS->SetLightBrightness(mpS->brightness);
		apPS->SetVisible(mbIsVisible);

		if(apPS->GetLight()) apPS->GetLight()->RemoveParticleSystem(apPS);
		apPS->SetLight(this);
		ohpl_sl_light_attach_ps(mpS, (uintptr_t)apPS);
	}

	void iLight::RemoveParticleSystem(cParticleSystem *apPS)
	{
		if(ohpl_sl_light_remove_ps(mpS, (uintptr_t)apPS) == false) return;
		apPS->SetLight(NULL);
	}

	void iLight::RemoveBillboard(cBillboard *apBillboard)
	{
		ohpl_sl_light_remove_billboard(mpS, (uintptr_t)apBillboard);
	}

	void iLight::RenderShadow(iRenderable *apObject,cRenderSettings *apRenderSettings,
				iLowLevelGraphics *apLowLevelGraphics)
	{
	}

	void iLight::OnFlickerOff()
	{
		tString sPS = FlickerString(3);
		if(sPS!=""){
			cParticleSystem *pPS = World()->CreateParticleSystem(GetName() + "_PS", sPS, cVector3f(1,1,1));
			if(pPS) pPS->SetMatrix(GetWorldMatrix());
		}
	}

	void iLight::OnFlickerOn()
	{
		tString sPS = FlickerString(1);
		if(sPS!=""){
			cParticleSystem *pPS = World()->CreateParticleSystem(GetName() + "_PS", sPS, cVector3f(1,1,1));
			if(pPS) pPS->SetMatrix(GetWorldMatrix());
		}
	}

	void iLight::OnSetDiffuse()
	{
		for(size_t i=0, n=ohpl_sl_light_billboard_count(mpS); i<n; ++i)
		{
			cColor base;
			cBillboard *pBillboard = (cBillboard*)ohpl_sl_light_billboard_at(mpS, i, &base);
			pBillboard->SetColor( base * cColor(mpS->diffuse.r,mpS->diffuse.g,mpS->diffuse.b,1));
		}
		for(size_t i=0, n=ohpl_sl_light_ps_count(mpS); i<n; ++i)
			((cParticleSystem*)ohpl_sl_light_ps_at(mpS, i))->SetLightColor(cColor(mpS->diffuse.r,mpS->diffuse.g,mpS->diffuse.b,1));
	}

	//////////////////////////////////////////////////////////////////////////
	// cLightPoint
	//////////////////////////////////////////////////////////////////////////

	cLightPoint::cLightPoint(tString asName, cResources *apResources) : iLight(asName,apResources,eLightType_Point)
	{
		UpdateBoundingVolume();
	}

	void cLightPoint::UpdateBoundingVolume()
	{
		mBoundingVolume.SetSize(mpS->radius*2);
		mBoundingVolume.SetPosition(GetWorldPosition());
	}

	//////////////////////////////////////////////////////////////////////////
	// cLightSpot
	//////////////////////////////////////////////////////////////////////////

	cLightSpot::cLightSpot(tString asName, cResources *apResources) : iLight(asName,apResources,eLightType_Spot)
	{
		Spot()->frustum = (uintptr_t)hplNew( cFrustum, () );

		static bool bFalloffMissing = ohpl_sl_hpl3_visibility();
		iTexture *pFalloffMap = bFalloffMissing ? NULL : TextureManager()->Create1D("core_falloff_linear",false);
		bFalloffMissing = pFalloffMap == NULL;
		if(pFalloffMap)
		{
			pFalloffMap->SetWrapS(eTextureWrap_ClampToEdge);
			pFalloffMap->SetWrapT(eTextureWrap_ClampToEdge);
		}
		Spot()->spotFalloffMap = (uintptr_t)pFalloffMap;

		UpdateBoundingVolume();
	}

	cLightSpot::~cLightSpot()
	{
		if(Spot()->spotFalloffMap) TextureManager()->Destroy((iTexture*)Spot()->spotFalloffMap);
		hplDelete((cFrustum*)Spot()->frustum);
	}

	void cLightSpot::SetRadius(float afX)
	{
		mpS->radius = afX;
		UpdateBoundingVolume();
		SetTransformUpdated();
		Spot()->ctl.projection_updated = true;
	}

	void cLightSpot::SetFOV(float afAngle)
	{
		Spot()->fov = afAngle;
		Spot()->ctl.projection_updated = true;
		ohpl_light_spot_half_fov(Spot()->fov, &Spot()->tanHalfFov, &Spot()->cosHalfFov);
	}

	const cMatrixf& cLightSpot::GetViewMatrix()
	{
		if(ohpl_h11_spot_view_stale(&Spot()->ctl, GetTransformUpdateCount()))
		{
			Spot()->ctl.view_count = GetTransformUpdateCount();
			Spot()->view = cMath::MatrixInverse(GetWorldMatrix());
		}
		return Spot()->view;
	}

	const cMatrixf& cLightSpot::GetProjectionMatrix()
	{
		if(ohpl_h11_spot_projection_stale(&Spot()->ctl))
		{
			ohpl_light_spot_projection(Spot()->fov, Spot()->aspect, Spot()->nearClip, GetShadowNearClip(), mpS->radius, &Spot()->projection);
		}
		return Spot()->projection;
	}

	const cMatrixf& cLightSpot::GetViewProjMatrix()
	{
		if(ohpl_h11_spot_view_proj_stale(&Spot()->ctl, GetTransformUpdateCount()))
		{
			const cMatrixf &mtxProj = GetProjectionMatrix();
			const cMatrixf &mtxView = GetViewMatrix();
			ohpl_light_spot_view_proj(&mtxProj, &mtxView, &Spot()->viewProj);
			ohpl_h11_spot_view_proj_done(&Spot()->ctl, GetTransformUpdateCount());
		}
		return Spot()->viewProj;
	}

	cFrustum* cLightSpot::GetFrustum()
	{
		cFrustum *pFrustum = (cFrustum*)Spot()->frustum;
		if(ohpl_h11_spot_frustum_stale(&Spot()->ctl, GetTransformUpdateCount()))
		{
			pFrustum->SetupPerspectiveProj(GetProjectionMatrix(),
											GetViewMatrix(),
											mpS->radius,ohpl_light_spot_near(Spot()->nearClip, GetShadowNearClip()),
											Spot()->fov,Spot()->aspect,GetWorldPosition(),false);
			ohpl_h11_spot_frustum_done(&Spot()->ctl, GetTransformUpdateCount());
		}
		return pFrustum;
	}

	void cLightSpot::SetSpotFalloffMap(iTexture* apTexture)
	{
		if(Spot()->spotFalloffMap) TextureManager()->Destroy((iTexture*)Spot()->spotFalloffMap);
		Spot()->spotFalloffMap = (uintptr_t)apTexture;
		if(apTexture)
		{
			apTexture->SetWrapS(eTextureWrap_ClampToEdge);
			apTexture->SetWrapT(eTextureWrap_ClampToEdge);
		}
	}

	bool cLightSpot::CollidesWithBV(cBoundingVolume *apBV)
	{
		if(cMath::CheckBVIntersection(*GetBoundingVolume(), *apBV)==false) return false;
		return GetFrustum()->CollideBoundingVolume(apBV)!= eCollision_Outside;
	}

	bool cLightSpot::CollidesWithFrustum(cFrustum *apFrustum)
	{
		return apFrustum->CollideFrustum(GetFrustum())!=eCollision_Outside;
	}

	static eTextureAnimMode GetAnimMode(const tString& asType)
	{
		if(cString::ToLowerCase(asType) == "none") return eTextureAnimMode_None;
		else if(cString::ToLowerCase(asType) == "loop") return eTextureAnimMode_Loop;
		else if(cString::ToLowerCase(asType) == "oscillate") return eTextureAnimMode_Oscillate;
		return eTextureAnimMode_None;
	}

	void cLightSpot::ExtraXMLProperties(cXmlElement *apMainElem)
	{
		tString sTexture = cString::ToString(apMainElem->GetAttribute("ProjectionImage"),"");
		eTextureAnimMode animMode = GetAnimMode(cString::ToString(apMainElem->GetAttribute("ProjectionAnimMode"),"None"));
		float fFrameTime = cString::ToFloat(apMainElem->GetAttribute("ProjectionFrameTime"),1.0f);

		iTexture *pTex = NULL;
        if(animMode != eTextureAnimMode_None)
		{
			pTex = TextureManager()->CreateAnim(sTexture,true,eTextureType_2D);
			pTex->SetAnimMode(animMode);
			pTex->SetFrameTime(fFrameTime);
		}
		else
		{
			pTex = TextureManager()->Create2D(sTexture,true);
		}

		if(pTex)
		{
			SetGoboTexture(pTex);
		}

		Spot()->aspect = cString::ToFloat(apMainElem->GetAttribute("Aspect"),Spot()->aspect);
		Spot()->nearClip = cString::ToFloat(apMainElem->GetAttribute("NearClipPlane"),Spot()->nearClip);
	}

	void cLightSpot::UpdateBoundingVolume()
	{
		mBoundingVolume = GetFrustum()->GetBoundingVolume();
	}

	//////////////////////////////////////////////////////////////////////////
	// cLightBox
	//////////////////////////////////////////////////////////////////////////

	cLightBox::cLightBox(tString asName, cResources *apResources) : iLight(asName,apResources,eLightType_Box)
	{
		UpdateBoundingVolume();
	}

	void cLightBox::AddIrradianceSet(const tString& asName, const std::vector<cVector3f>& avBands)
	{
		ohpl_sl_box_add_set(Box(), asName.data(), asName.size(), avBands.data(), avBands.size());
	}

	void cLightBox::FadeIrradianceSet(const tString& asName, float afTime)
	{
		if(ohpl_sl_box_fade_set(Box(), asName.data(), asName.size(), afTime) == false) return;
		UpdateLogic(0);
	}

	void cLightBox::UpdateLogic(float afTimeStep)
	{
		iLight::UpdateLogic(afTimeStep);
		ohpl_sl_box_fade_step(Box(), afTimeStep);
	}

	void cLightBox::UpdateBoundingVolume()
	{
		mBoundingVolume.SetSize(Box()->size);
		mBoundingVolume.SetPosition(GetWorldPosition());
	}
}
