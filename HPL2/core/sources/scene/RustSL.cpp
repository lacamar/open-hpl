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

		mbApplyTransformToBV = false;

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
		mbUpdateBoundingVolume = true;
		SetTransformUpdated();
	}

	void iLight::UpdateLogic(float afTimeStep)
	{
		OhplSLDumpLight(this, World());
		UpdateLight(afTimeStep);
		if(mpS->anim.fade_time>0 || mpS->anim.flickering)
		{
			mbUpdateBoundingVolume = true;
		}
	}

	cBoundingVolume* iLight::GetBoundingVolume()
	{
		if(mbUpdateBoundingVolume)
		{
			UpdateBoundingVolume();
			if(mpS->maskBox)
			{
				cVector3f vMin, vMax;
				ohpl_h11_light_mask_bounds(&mBoundingVolume.GetMin(), &mBoundingVolume.GetMax(), &mpS->maskCenter, &mpS->maskSize, &vMin, &vMax);
				mBoundingVolume.SetLocalMinMax(vMin, vMax);
				mBoundingVolume.SetTransform(cMatrixf::Identity);
			}
			mbUpdateBoundingVolume = false;
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

	//////////////////////////////////////////////////////////////////////////
	// cBillboard
	//////////////////////////////////////////////////////////////////////////

	cBillboard::cBillboard(const tString asName,const cVector2f& avSize,eBillboardType aType, cResources *apResources,cGraphics *apGraphics) :
	iRenderable(asName), mpS(ohpl_sl_bb_new(aType, &avSize))
	{
		mpS->matMgr = (uintptr_t)apResources->GetMaterialManager();
		iLowLevelGraphics *pLowLevelGraphics = apGraphics->GetLowLevel();
		const cVector2f &vSize = mpS->size;

		iVertexBuffer *pVtxBuffer = pLowLevelGraphics->CreateVertexBuffer(eVertexBufferType_Hardware,eVertexBufferDrawType_Tri,
													eVertexBufferUsageType_Dynamic,4,6);
		mpS->vtx = (uintptr_t)pVtxBuffer;

		pVtxBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Normal,eVertexBufferElementFormat_Float,3);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);

		cVector3f vCoords[4] = {cVector3f((vSize.x/2),-(vSize.y/2),0),
								cVector3f(-(vSize.x/2),-(vSize.y/2),0),
								cVector3f(-(vSize.x/2),(vSize.y/2),0),
								cVector3f((vSize.x/2),(vSize.y/2),0)};

		cVector3f vTexCoords[4] = {cVector3f(1,-1,0),
								cVector3f(-1,-1,0),
								cVector3f(-1,1,0),
								cVector3f(1,1,0)};
		if(cRendererDeferred::GetHdr())
			for(int i=0;i<4;i++) vTexCoords[i].y = -vTexCoords[i].y;

		for(int i=0;i<4;i++)
		{
			pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position, vCoords[i]);
			pVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, cColor(1,1,1,1));
			pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, (vTexCoords[i] + cVector2f(1,1))/2 );
			pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Normal,cVector3f(0,0,1));
		}

		for(int i=0;i<3;i++) pVtxBuffer->AddIndex(i);
		for(int i=2;i<5;i++) pVtxBuffer->AddIndex(i==4?0:i);

		if(aType == eBillboardType_FixedAxis)
		{
			for(int i=2; i>=0; i--) pVtxBuffer->AddIndex(i);
			for(int i=4; i>=2; i--) pVtxBuffer->AddIndex(i==4?0:i);
		}

		pVtxBuffer->Compile(eVertexCompileFlag_CreateTangents);

		mBoundingVolume.SetSize(cVector3f(vSize.x, vSize.y, vSize.x));
	}

	cBillboard::~cBillboard()
	{
		if(mpS->material) ((cMaterialManager*)mpS->matMgr)->Destroy((cMaterial*)mpS->material);
		if(mpS->vtx) hplDelete((iVertexBuffer*)mpS->vtx);
		if(mpS->haloBv) hplDelete((cBoundingVolume*)mpS->haloBv);
		ohpl_sl_bb_free(mpS);
	}

	void cBillboard::SetSize(const cVector2f& avSize)
	{
		mpS->size = avSize;
		mBoundingVolume.SetSize(cVector3f(mpS->size.x, mpS->size.y, mpS->size.x));

		iVertexBuffer *pVtx = GetVertexBuffer();
		ohpl_bb_vertex_positions(&mpS->size, pVtx->GetFloatArray(eVertexBufferElement_Position));
		pVtx->UpdateData(eVertexElementFlag_Position,false);

		if(mpS->kind == eBillboardType_Axis) SetAxis(mpS->axis);

		SetTransformUpdated();
	}

	void cBillboard::SetAxis(const cVector3f& avAxis)
	{
		cVector3f vAxis = avAxis;
		vAxis.Normalize();
		float fSize;
		if(ohpl_sl_bb_set_axis(mpS, &vAxis, &fSize))
		{
			mBoundingVolume.SetSize(fSize);
			SetTransformUpdated();
		}
	}

	void cBillboard::SetColor(const cColor &aColor)
	{
		if(ohpl_sl_bb_set_color(mpS, &aColor) == false) return;
		UpdateVertexColors();
	}

	void cBillboard::SetHaloAlpha(float afX)
	{
		if(ohpl_sl_bb_set_halo_alpha(mpS, afX) == false) return;
		UpdateVertexColors();
	}

	void cBillboard::SetBrightness(float afX)
	{
		if(ohpl_sl_bb_set_brightness(mpS, afX) == false) return;
		UpdateVertexColors();
	}

	void cBillboard::UpdateVertexColors()
	{
		iVertexBuffer *pVtx = GetVertexBuffer();
		ohpl_sl_bb_colors(mpS, cRendererDeferred::GetHdr(), pVtx->GetFloatArray(eVertexBufferElement_Color0));
		pVtx->UpdateData(eVertexElementFlag_Color0,false);
	}

	cMatrixf* cBillboard::GetModelMatrix(cFrustum *apFrustum)
	{
		if(apFrustum==NULL)return &GetWorldMatrix();
		if(mpS->kind == eBillboardType_FixedAxis)
		{
			return &GetWorldMatrix();
		}

		cVector3f vCamOrigin = apFrustum->GetOrigin();
		cVector3f vViewUp = apFrustum->GetViewMatrix().GetUp();
		bool bWarn;
		const cMatrixf *pMtx = ohpl_sl_bb_model(mpS, &GetWorldMatrix(), &vCamOrigin, &vViewUp, &bWarn);
		if(bWarn)
			Warning("Billboard Right vector is not correct! Contact programmer!\n");
		return const_cast<cMatrixf*>(pMtx);
	}

	bool cBillboard::IsVisible()
	{
		return ohpl_h12b_is_visible(&mpS->color, mbIsVisible);
	}

	void cBillboard::SetIsHalo(bool abX)
	{
		mpS->isHalo = abX;
		if(mpS->isHalo)
		{
			mpS->haloAlpha = 1;
			SetHaloAlpha(0);
			if(mpS->haloBv == 0) mpS->haloBv = (uintptr_t)hplNew(cBoundingVolume, ());
		}
		else
		{
			if(mpS->haloBv)
			{
				hplDelete((cBoundingVolume*)mpS->haloBv);
				mpS->haloBv = 0;
			}
		}
	}

	void cBillboard::AssignOcclusionQuery(iRenderer *apRenderer)
	{
		if(mpS->isHalo==false) return;

		mpS->haloOcclusion = cMath::MatrixScale(mpS->haloSourceSize);
		mpS->haloOcclusion = cMath::MatrixMul(GetWorldMatrix(), mpS->haloOcclusion);

		iVertexBuffer *pShapeVtx = apRenderer->GetShapeBoxVertexBuffer();
		apRenderer->AssignOcclusionObject(this,0, pShapeVtx, &mpS->haloOcclusion, true);
		apRenderer->AssignOcclusionObject(this,1, pShapeVtx, &mpS->haloOcclusion, false);
	}

	bool cBillboard::RetrieveOcculsionQuery(iRenderer *apRenderer)
	{
		if(mpS->isHalo==false) return  true;

		int lSamples = apRenderer->RetrieveOcclusionObjectSamples(this, 0);
		int lMaxSamples = apRenderer->RetrieveOcclusionObjectSamples(this, 1);

		if(lMaxSamples >0)
		{
			cFrustum *pFrustum = apRenderer->GetCurrentFrustum();
			cBoundingVolume *pHaloBV = (cBoundingVolume*)mpS->haloBv;
			const uint32_t lGates = ohpl_sl_bb_halo_gates(mpS, GetTransformUpdateCount());
			if(lGates & kOhplH12HaloSize) pHaloBV->SetSize(mpS->haloSourceSize);
			if(lGates & kOhplH12HaloTransform) pHaloBV->SetTransform(GetWorldMatrix());

			cVector3f vRustMin, vRustMax;
			const bool bClip = cMath::GetNormalizedClipRectFromBV(vRustMin, vRustMax, *pHaloBV, pFrustum, 0);
			SetHaloAlpha(ohpl_h12b_bb_halo_alpha(lSamples, lMaxSamples, bClip, &vRustMin, &vRustMax));
			return true;
		}
		else
		{
			SetHaloAlpha(0);
			return false;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// cBeam / cBeamEnd
	//////////////////////////////////////////////////////////////////////////

	cBeam::cBeam(const tString asName, cResources *apResources,cGraphics *apGraphics) :
	iRenderable(asName), mpS(ohpl_sl_beam_new())
	{
		mpS->matMgr = (uintptr_t)apResources->GetMaterialManager();
		mpS->fileSearcher = (uintptr_t)apResources->GetFileSearcher();
		iLowLevelGraphics *pLowLevelGraphics = apGraphics->GetLowLevel();
		const cVector2f &vSize = mpS->size;

		iVertexBuffer *pVtxBuffer = pLowLevelGraphics->CreateVertexBuffer(
								eVertexBufferType_Hardware,
								eVertexBufferDrawType_Tri, eVertexBufferUsageType_Dynamic,4,6);
		mpS->vtx = (uintptr_t)pVtxBuffer;

		pVtxBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Normal,eVertexBufferElementFormat_Float,3);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);

		cVector3f vCoords[4] = {cVector3f((vSize.x/2),-(vSize.y/2),0),
								cVector3f(-(vSize.x/2),-(vSize.y/2),0),
								cVector3f(-(vSize.x/2),(vSize.y/2),0),
								cVector3f((vSize.x/2),(vSize.y/2),0)};

		cVector3f vTexCoords[4] = {cVector3f(1,1,0),	//Bottom left
									cVector3f(-1,1,0),	//Bottom right
									cVector3f(-1,-1,0),	//Top left
									cVector3f(1,-1,0)};	//Top right

		for(int i=0;i<4;i++)
		{
			pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position, vCoords[i]);
			pVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, cColor(1,1,1,1));
			pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, (vTexCoords[i] + cVector2f(1,1))/2);
			pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Normal,cVector3f(0,0,1));
		}

		for(int i=0;i<3;i++) pVtxBuffer->AddIndex(i);
		for(int i=2;i<5;i++) pVtxBuffer->AddIndex(i==4?0:i);

		pVtxBuffer->Compile(eVertexCompileFlag_CreateTangents);

		cBeamEnd *pEnd = hplNew( cBeamEnd, (asName + "_end",this));
		mpS->end = (uintptr_t)pEnd;
		pEnd->AddCallback(&mEndCallback);

		mBoundingVolume.SetSize(cVector3f(vSize.x, vSize.y, vSize.x));
		mbApplyTransformToBV = false;
	}

	cBeam::~cBeam()
	{
		hplDelete(GetEnd());
		if(mpS->material) ((cMaterialManager*)mpS->matMgr)->Destroy((cMaterial*)mpS->material);
		if(mpS->vtx) hplDelete((iVertexBuffer*)mpS->vtx);
		ohpl_sl_beam_free(mpS);
	}

	tString cBeam::GetFileName()
	{
		size_t lLen;
		const char *pStr = ohpl_sl_beam_file(mpS, &lLen);
		return tString(pStr, lLen);
	}

	void cBeam::SetSize(const cVector2f& avSize)
	{
		mpS->size = avSize;
		mBoundingVolume.SetSize(cVector3f(mpS->size.x, mpS->size.y, mpS->size.x));
		SetTransformUpdated();
	}

	void cBeam::SetTileHeight(bool abX)
	{
		if(mpS->tileHeight == abX) return;
		mpS->tileHeight = abX;
		SetTransformUpdated();
	}

	void cBeam::SetColor(const cColor &aColor)
	{
		iVertexBuffer *pVtx = GetVertexBuffer();
		if(ohpl_sl_beam_set_color(mpS, &aColor, pVtx->GetFloatArray(eVertexBufferElement_Color0)) == false) return;
		pVtx->UpdateData(eVertexElementFlag_Color0,false);
	}

	cBoundingVolume* cBeam::GetBoundingVolume()
	{
		if(mbUpdateBoundingVolume)
		{
			cVector3f vStart = GetWorldPosition();
			cVector3f vEnd = GetEnd()->GetWorldPosition();
			cVector3f vMin, vMax;
			ohpl_sl_beam_bounds(mpS, &vStart, &vEnd, &vMin, &vMax);
			mBoundingVolume.SetLocalMinMax(vMin,vMax);
			mbUpdateBoundingVolume = false;
		}
		return &mBoundingVolume;
	}

	void cBeam::UpdateGraphicsForFrame(float afFrameTime)
	{
		if(ohpl_h12b_beam_needs_geometry(mpS->startTransformCount, mpS->endTransformCount, GetTransformUpdateCount())==false) return;

		cVector3f vStart = GetWorldPosition();
		cVector3f vEnd = GetEnd()->GetWorldPosition();
		iVertexBuffer *pVtx = GetVertexBuffer();
		float *pPos = pVtx->GetFloatArray(eVertexBufferElement_Position);
		float *pTex = pVtx->GetFloatArray(eVertexBufferElement_Texture0);
		ohpl_sl_beam_geometry(mpS, GetTransformUpdateCount(), &vStart, &vEnd, pPos, pTex);
		pVtx->UpdateData(eVertexElementFlag_Position | eVertexElementFlag_Texture0,false);
	}

	cMatrixf* cBeam::GetModelMatrix(cFrustum *apFrustum)
	{
		if(apFrustum==NULL)return &GetWorldMatrix();

		cVector3f vWorldPos = GetWorldPosition();
		cVector3f vCamera = apFrustum->GetOrigin();
		return const_cast<cMatrixf*>(ohpl_sl_beam_model(mpS, &GetWorldMatrix(), &vWorldPos, &vCamera));
	}

	bool cBeam::LoadXMLProperties(const tString asFile)
	{
		ohpl_sl_beam_set_file(mpS, asFile.data(), asFile.size());
		cMaterialManager *pMaterialManager = (cMaterialManager*)mpS->matMgr;
		tString sNewFile = cString::SetFileExt(asFile,"beam");
		tWString sPath = ((cFileSearcher*)mpS->fileSearcher)->GetFilePath(sNewFile);
		if(sPath != _W(""))
		{
			FILE *pFile = cPlatform::OpenFile(sPath, _W("rb"));
			if(pFile==NULL) return false;
			fclose(pFile);

			cXmlDocumentTiny doc("");
			if(doc.CreateFromFile(sPath))
			{
				cXmlElement *pMainElem = doc.GetFirstElement("MAIN");
				if(pMainElem!=NULL)
				{
					tString sMaterial = cString::ToString(pMainElem->GetAttribute("Material"),"");
					cVector2f vSize = cString::ToVector2f(pMainElem->GetAttribute("Size"),1);
					bool bTileHeight = cString::ToBool(pMainElem->GetAttribute("TileHeight"),true);
					bool bMultiplyAlphaWithColor = cString::ToBool(pMainElem->GetAttribute("MultiplyAlphaWithColor"),false);
					cColor StartColor = cString::ToColor(pMainElem->GetAttribute("StartColor"),cColor(1,1));
					cColor EndColor = cString::ToColor(pMainElem->GetAttribute("EndColor"),cColor(1,1));

					SetSize(vSize);
					SetTileHeight(bTileHeight);
					SetMultiplyAlphaWithColor(bMultiplyAlphaWithColor);
					SetColor(StartColor);
					GetEnd()->SetColor(EndColor);

					cMaterial *pMat = pMaterialManager->CreateMaterial(sMaterial);
					if(pMat)	{
						SetMaterial(pMat);
					}
					else{
						Error("Couldn't load material '%s' in Beam file '%s'",
											sMaterial.c_str(), sNewFile.c_str());
						return false;
					}
				}
				else
				{
					Error("Cannot find main element in %s\n",sNewFile.c_str());
					return false;
				}
			}
			else
			{
				Error("Couldn't load file '%s'\n",sNewFile.c_str());
			}
		}
		else
		{
			Error("Couldn't find file '%s'\n",sNewFile.c_str());
			return false;
		}

		return true;
	}

	bool cBeam::IsVisible()
	{
		return ohpl_h12b_is_visible(&mpS->color, mbIsVisible);
	}

	void cBeamEnd_UpdateCallback::OnTransformUpdate(iEntity3D * apEntity)
	{
		cBeamEnd *pEnd = static_cast<cBeamEnd*>(apEntity);
		((cBeam*)pEnd->mpS->beam)->SetTransformUpdated(true);
	}

	void cBeamEnd::SetColor(const cColor &aColor)
	{
		cBeam *pBeam = (cBeam*)mpS->beam;
		iVertexBuffer *pVtx = pBeam->GetVertexBuffer();
		if(ohpl_sl_beam_set_end_color(pBeam->mpS, mpS, &aColor, pVtx->GetFloatArray(eVertexBufferElement_Color0)) == false) return;
		pVtx->UpdateData(eVertexElementFlag_Color0,false);
	}

	//////////////////////////////////////////////////////////////////////////
	// cLensFlare / cLensFlareType
	//////////////////////////////////////////////////////////////////////////

	static cVector3f FacingAxis(iEntity3D *apEnt)
	{
		const cMatrixf &m = apEnt->GetWorldMatrix();
		if(apEnt->GetEntityParent() || apEnt->GetParent()) return cVector3f(m.m[0][2], m.m[1][2], m.m[2][2]);
		return m.GetForward();
	}

	static OhplFlareEnt FlareEnt(iEntity3D *apEnt)
	{
		OhplFlareEnt ent;
		ent.pos = apEnt->GetWorldPosition();
		ent.axis = FacingAxis(apEnt);
		return ent;
	}

	static OhplFlareView FlareView(cFrustum *apFrustum)
	{
		OhplFlareView view;
		view.origin = apFrustum->GetOrigin();
		view.view = apFrustum->GetViewMatrix();
		view.proj = apFrustum->GetProjectionMatrix();
		view.aspect = apFrustum->GetAspect();
		return view;
	}

	static cLensFlare* FlareOf(OhplSLFlareType *apType){ return (cLensFlare*)apType->parent; }

	cLensFlareType::cLensFlareType(cLensFlare *apParent, eLensFlareType aType) : iRenderable(apParent->GetName()),
		mpS(ohpl_sl_flare_type_new((uintptr_t)apParent, aType))
	{
	}

	cMaterial* cLensFlareType::GetMaterial(){ return (cMaterial*)FlareOf(mpS)->mpS->material[mpS->kind];}
	iVertexBuffer* cLensFlareType::GetVertexBuffer(){ return (iVertexBuffer*)FlareOf(mpS)->mpS->vtx;}
	int cLensFlareType::GetMatrixUpdateCount(){ return FlareOf(mpS)->GetTransformUpdateCount();}
	cBoundingVolume* cLensFlareType::GetBoundingVolume(){ return FlareOf(mpS)->GetBoundingVolume();}

	cMatrixf* cLensFlareType::GetModelMatrix(cFrustum *apFrustum)
	{
		cLensFlare *pParent = FlareOf(mpS);
		if(apFrustum==NULL) return pParent->GetModelMatrix(NULL);
		cMatrixf *pSizeRot = pParent->GetSizeAndRotation((eLensFlareType)mpS->kind, apFrustum, pParent->mpS->vis.vis);
		mpS->model = cMath::MatrixMul(*pParent->GetModelMatrix(apFrustum), *pSizeRot);
		return &mpS->model;
	}

	bool cLensFlareType::UpdateGraphicsForViewport(cFrustum *apFrustum,float afFrameTime)
	{
		cLensFlare *pParent = FlareOf(mpS);
		OhplFlareEnt ent = FlareEnt(pParent);
		OhplFlareView view = FlareView(apFrustum);
		cColor c;
		if(ohpl_sl_flare_type_color(pParent->mpS, &ent, &view, mpS->kind, pParent->mpS->vis.vis, &c)==false) return false;
		SetColorMul(c);
		return true;
	}

	cLensFlare::cLensFlare(const tString& asName, cResources *apResources, cGraphics *apGraphics) : iRenderable(asName),
		mpS(ohpl_sl_flare_new(0))
	{
		mpS->matMgr = (uintptr_t)apResources->GetMaterialManager();
		iLowLevelGraphics *pLowLevelGraphics = apGraphics->GetLowLevel();
		mpS->lowLevel = (uintptr_t)pLowLevelGraphics;

		for(int i=0; i<eLensFlareType_LastEnum; ++i)
		{
			mpS->types[i] = (uintptr_t)hplNew(cLensFlareType, (this, (eLensFlareType)i));
		}
		mpS->seed = rand(); // after the types: their iRenderable ctors draw first (C++ order)

		iVertexBuffer *pVtxBuffer = pLowLevelGraphics->CreateVertexBuffer(eVertexBufferType_Hardware,eVertexBufferDrawType_Tri, eVertexBufferUsageType_Static,4,6);
		mpS->vtx = (uintptr_t)pVtxBuffer;
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Normal,eVertexBufferElementFormat_Float,3);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);

		cVector3f vCorners[4] = {cVector3f(1,-1,0), cVector3f(-1,-1,0), cVector3f(-1,1,0), cVector3f(1,1,0)};
		for(int i=0;i<4;i++)
		{
			cVector3f vUv = vCorners[i];
			if(cRendererDeferred::GetHdr()) vUv.y = -vUv.y;
			pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position, vCorners[i]*0.5f);
			pVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, cColor(1,1));
			pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, (vUv + cVector3f(1,1,0))/2);
			pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Normal,cVector3f(0,0,1));
		}
		unsigned int vIdx[6] = {0,1,2,2,3,0};
		for(int i=0;i<6;i++) pVtxBuffer->AddIndex(vIdx[i]);
		pVtxBuffer->Compile(eVertexCompileFlag_CreateTangents);

		cBoundingVolume *pOcclusionBV = hplNew(cBoundingVolume, ());
		mpS->occlusionBv = (uintptr_t)pOcclusionBV;
		pOcclusionBV->SetSize(mpS->sourceSize);
		UpdateBV();
	}

	cLensFlare::~cLensFlare()
	{
		for(int i=0; i<eLensFlareType_LastEnum; ++i)
		{
			if(mpS->material[i]) ((cMaterialManager*)mpS->matMgr)->Destroy((cMaterial*)mpS->material[i]);
			hplDelete((cLensFlareType*)mpS->types[i]);
		}
		hplDelete((iVertexBuffer*)mpS->vtx);
		if(mpS->multiIrisVtx) hplDelete((iVertexBuffer*)mpS->multiIrisVtx);
		hplDelete((cBoundingVolume*)mpS->occlusionBv);
		ohpl_sl_flare_free(mpS);
	}

	void cLensFlare::SetFlareSourceSize(cVector3f avSize)
	{
		mpS->sourceSize = avSize;
		((cBoundingVolume*)mpS->occlusionBv)->SetSize(mpS->sourceSize);
	}

	void cLensFlare::SetFlareSize(eLensFlareType aType, cVector2f avSize)
	{
		mpS->size[aType] = avSize;
		UpdateBV();
	}

	void cLensFlare::SetFlareActive(eLensFlareType aType, bool abX)
	{
		mpS->active[aType] = abX;
		UpdateBV();
	}

	void cLensFlare::SetMaterial(eLensFlareType aType, cMaterial *apMaterial)
	{
		if(mpS->material[aType]) ((cMaterialManager*)mpS->matMgr)->Destroy((cMaterial*)mpS->material[aType]);
		mpS->material[aType] = (uintptr_t)apMaterial;
	}

	cColor cLensFlare::GetFlareColorAndBrightness(eLensFlareType aType, cFrustum *apFrustum, float afVis)
	{
		OhplFlareEnt ent = FlareEnt(this);
		OhplFlareView view = FlareView(apFrustum);
		cColor col(0,0);
		ohpl_sl_flare_color(mpS, &ent, &view, aType, afVis, &col);
		return col;
	}

	cColor cLensFlare::GetGlare(cFrustum *apFrustum, cVector3f &avScreenPos, float afScale)
	{
		OhplFlareEnt ent = FlareEnt(this);
		OhplFlareView view = FlareView(apFrustum);
		cColor col;
		ohpl_sl_flare_glare(mpS, &ent, &view, afScale, &col, &avScreenPos);
		return col;
	}

	cMatrixf* cLensFlare::GetSizeAndRotation(eLensFlareType aType, cFrustum *apFrustum, float afVis)
	{
		OhplFlareEnt ent = FlareEnt(this);
		OhplFlareView view = FlareView(apFrustum);
		return ohpl_sl_flare_size_rotation(mpS, &ent, &view, aType, afVis);
	}

	cMatrixf* cLensFlare::GetModelMatrix(cFrustum *apFrustum)
	{
		if(apFrustum==NULL) return &GetWorldMatrix();
		OhplFlareEnt ent = FlareEnt(this);
		OhplFlareView view = FlareView(apFrustum);
		return ohpl_sl_flare_billboard(mpS, &ent, &view);
	}

	void cLensFlare::UpdateBV()
	{
		float fR = ohpl_sl_flare_bv_radius(mpS);
		mBoundingVolume.SetLocalMinMax(-fR, fR);
		SetTransformUpdated();
	}

	bool cLensFlare::CollidesWithFrustum(cFrustum *apFrustum)
	{
		if(IsAnyTypeActive()==false) return false;

		cBoundingVolume bv = *GetBoundingVolume();
		OhplFlareView view = FlareView(apFrustum);
		cVector3f vWorldCenter = bv.GetWorldCenter();
		cVector3f vMin = bv.GetLocalMin(), vMax = bv.GetLocalMax(), vC = bv.GetLocalCenter();
		cVector3f vNewMin, vNewMax;
		if(ohpl_sl_flare_collide_box(mpS, &view, &vWorldCenter, &vMin, &vMax, &vC, &vNewMin, &vNewMax)==false) return false;

		bv.SetLocalMinMax(vNewMin, vNewMax);
		return apFrustum->CollideBoundingVolume(&bv) != eCollision_Outside;
	}

	void cLensFlare::AssignOcclusionQuery(iRenderer *apRenderer)
	{
		if(IsAnyTypeActive()==false) return;

		iVertexBuffer *pVtx = apRenderer->GetShapeBoxVertexBuffer();
		cMatrixf *pMtx = &mpS->occluder;
		mpS->occluder = cMath::MatrixMul(GetWorldMatrix(), cMath::MatrixScale(mpS->sourceSize));

		iEntity3D *pParent = GetEntityParent();
		if(mpS->useParentMesh && pParent && pParent->GetEntityType() == "SubMesh")
		{
			cSubMeshEntity *pSubMesh = static_cast<cSubMeshEntity*>(pParent);
			pVtx = pSubMesh->GetVertexBuffer();
			pMtx = pSubMesh->GetModelMatrix(NULL);
		}

		apRenderer->AssignOcclusionObject(this, 0, pVtx, pMtx, true);
		apRenderer->AssignOcclusionObject(this, 1, pVtx, pMtx, false);
	}

	bool cLensFlare::RetrieveOcculsionQuery(iRenderer *apRenderer)
	{
		mpS->vis.target = 0;
		if(IsAnyTypeActive()==false) return false;

		float fVis = (float)apRenderer->RetrieveOcclusionObjectSamples(this, 0);
		float fTot = (float)apRenderer->RetrieveOcclusionObjectSamples(this, 1);
		if(fVis <= 0) return false;

		cBoundingVolume *pOcclusionBV = (cBoundingVolume*)mpS->occlusionBv;
		pOcclusionBV->SetTransform(GetWorldMatrix());
		cVector3f vMin, vMax;
		if(cMath::GetNormalizedClipRectFromBV(vMin, vMax, *pOcclusionBV, apRenderer->GetCurrentFrustum(), 0)==false) return true;

		ohpl_sl_flare_set_target(mpS, fVis, fTot, &vMin, &vMax);
		return true;
	}

	iVertexBuffer* cLensFlare::PrepareMultiIrisVertexBuffer(cFrustum *apFrustum, float afVis)
	{
		int lCount = mpS->count;
		iVertexBuffer *pMultiIris = (iVertexBuffer*)mpS->multiIrisVtx;
		const uint32_t lIrisPlan = ohpl_h12b_iris_plan(IsFlareActive(eLensFlareType_MultiIris), lCount, pMultiIris ? (int32_t)pMultiIris->GetVertexNum() : -1);
		if(lIrisPlan == kOhplH12IrisNone) return NULL;

		if(lIrisPlan == kOhplH12IrisRecreate)
		{
			hplDelete(pMultiIris);
			pMultiIris = NULL;
		}

		if(pMultiIris==NULL)
		{
			pMultiIris = ((iLowLevelGraphics*)mpS->lowLevel)->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri, eVertexBufferUsageType_Dynamic, lCount*4, lCount*6);
			pMultiIris->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
			pMultiIris->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
			pMultiIris->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);

			for(int i=0; i<lCount*4; ++i)
			{
				pMultiIris->AddVertexVec4f(eVertexBufferElement_Position, 0, 1);
				pMultiIris->AddVertexColor(eVertexBufferElement_Color0, cColor(0,0));
				pMultiIris->AddVertexVec3f(eVertexBufferElement_Texture0, 0);
			}
			unsigned int vIdx[6] = {0,1,2,0,2,3};
			for(int i=0; i<lCount; ++i)
				for(int j=0; j<6; ++j) pMultiIris->AddIndex(i*4 + vIdx[j]);
			pMultiIris->Compile(0);
		}
		mpS->multiIrisVtx = (uintptr_t)pMultiIris;

		float *pPos = pMultiIris->GetFloatArray(eVertexBufferElement_Position);
		float *pCol = pMultiIris->GetFloatArray(eVertexBufferElement_Color0);
		float *pUv = pMultiIris->GetFloatArray(eVertexBufferElement_Texture0);
		OhplFlareEnt ent = FlareEnt(this);
		OhplFlareView view = FlareView(apFrustum);
		ohpl_sl_flare_multi_iris(mpS, &ent, &view, afVis, pPos, pCol, pUv, lCount);
		pMultiIris->UpdateData(eVertexElementFlag_Position | eVertexElementFlag_Color0 | eVertexElementFlag_Texture0, false);
		return pMultiIris;
	}
}
