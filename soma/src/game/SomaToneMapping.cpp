#include "SomaToneMapping.h"
#include "RustSomaUi.h"
#include "SomaBase.h"
#include "SomaScriptBind.h"
#include "SomaScriptApi.h"
#include "SomaLuxGame.h"

#include <angelscript.h>
#include <cstring>

cSomaToneMapping *cSomaToneMapping::mpInstance = NULL;

namespace
{
	OhplGs7ToneState gTone;

	void Pull(cSomaToneMapping *p)
	{
		gTone.world_exposure = p->mfWorldExposure;
		gTone.world_white_cut = p->mfWorldWhiteCut;
		gTone.world_fade_time = p->mfWorldFadeTime;
		gTone.exposure = p->mfExposure;
		gTone.white_cut = p->mfWhiteCut;
		gTone.transition_time = p->mfTransitionTime;
	}

	void Push(cSomaToneMapping *p)
	{
		p->mfWorldExposure = gTone.world_exposure;
		p->mfWorldWhiteCut = gTone.world_white_cut;
		p->mfWorldFadeTime = gTone.world_fade_time;
		p->mfExposure = gTone.exposure;
		p->mfWhiteCut = gTone.white_cut;
		p->mfTransitionTime = gTone.transition_time;
	}

	// Index into the world's exposure areas, -1 for none; the chosen area is copied to apOut
	int AreaAt(cWorld *apWorld, const cVector3f &avPos, OhplGs7ExposureArea *apOut)
	{
		std::vector<OhplGs7ExposureArea> vAreas;
		for (const cWorldExposureArea &area : apWorld->GetExposureAreas())
		{
			OhplGs7ExposureArea a;
			static_assert(sizeof(a.inv_transform) == sizeof(area.m_mtxInvTransform.m), "matrix layout");
			memcpy(a.inv_transform, area.m_mtxInvTransform.m, sizeof(a.inv_transform));
			memcpy(a.half_size, area.mvHalfSize.v, sizeof(a.half_size));
			a.exposure = area.mfExposure;
			a.white_cut = area.mfWhiteCut;
			a.transition_time = area.mfTransitionTime;
			vAreas.push_back(a);
		}
		int lIdx = ohpl_gs7_tone_area_at(vAreas.data(), vAreas.size(), avPos.v);
		if (lIdx >= 0)
			*apOut = vAreas[lIdx];
		return lIdx;
	}
}

static iTexture *CreateGrading(const tString &asName)
{
	iTexture *pTex = gpSomaBase->mpEngine->GetResources()->GetTextureManager()->Create3D(asName, false);
	if (pTex)
	{
		pTex->SetWrapSTR(eTextureWrap_ClampToEdge);
		pTex->SetFilter(eTextureFilter_Bilinear);
	}
	return pTex;
}

cSomaToneMapping::cSomaToneMapping() : iUpdateable("SomaToneMapping")
{
	mpInstance = this;
	mpWorld = NULL;
	mpGradingTexture = mpGradingTarget = NULL;
	mfGradingBlend = mfGradingFadeTime = 0;
	mfKey = 0.5f;
	mfGamma = 2.2f;
	mfFilmGrainIntensity = 1;
	mfBrightPass = 0.75f;
	mfBloomWidth = 128;
	mBloomTint = cColor(1, 1);
	mfBloomFalloff = 0.5f;
	mbBloomActive = true;
	mbFilmGrainActive = true;
	mbColorGradingActive = true;
	mbSRGB = false;
	ohpl_gs7_tone_state_default(&gTone);
	Push(this);
}

void cSomaToneMapping::OnMapLoaded(cWorld *apWorld)
{
	mpWorld = apWorld;
	cTextureManager *pTexMgr = gpSomaBase->mpEngine->GetResources()->GetTextureManager();
	if (mpGradingTexture) pTexMgr->Destroy(mpGradingTexture);
	if (mpGradingTarget) pTexMgr->Destroy(mpGradingTarget);
	mpGradingTexture = mpGradingTarget = NULL;
	if (apWorld->GetColorGradingTexture() != "") mpGradingTexture = CreateGrading(apWorld->GetColorGradingTexture());
	mfKey = apWorld->GetToneMappingKey();
	Pull(this);
	cCamera *pCam = gpSomaBase->GetDebugCamera();
	OhplGs7ExposureArea area;
	bool bArea = pCam && AreaAt(apWorld, pCam->GetPosition(), &area) >= 0;
	ohpl_gs7_tone_map_loaded(&gTone, apWorld->GetToneMappingExposure(), apWorld->GetToneMappingWhiteCut(), bArea ? &area : NULL);
	Push(this);
	Update(0);
}

void cSomaToneMapping::FadeGrading(const tString &asName, float afTime)
{
	iTexture *pTex = CreateGrading(asName);
	if (pTex == NULL) return;
	cTextureManager *pTexMgr = gpSomaBase->mpEngine->GetResources()->GetTextureManager();
	if (mpGradingTarget)
	{
		if (mpGradingTexture) pTexMgr->Destroy(mpGradingTexture);
		mpGradingTexture = mpGradingTarget;
	}
	if (mpGradingTexture == NULL) mpGradingTexture = CreateGrading("grading_default.dds");
	mpGradingTarget = pTex;
	mfGradingBlend = 0;
	mfGradingFadeTime = afTime;
}

void cSomaToneMapping::SetGradingLayer(int alPrio, const tString &asName, float afAlpha)
{
	for (cGradingLayer &layer : mvGradingLayers)
		if (layer.msName == asName)
		{
			layer.mfAlpha = afAlpha;
			return;
		}
	iTexture *pTex = CreateGrading(asName);
	if (pTex == NULL) return;
	std::vector<int> vPrios;
	for (cGradingLayer &layer : mvGradingLayers) vPrios.push_back(layer.mlPrio);
	size_t lAt = ohpl_gs7_tone_layer_insert_index(vPrios.data(), vPrios.size(), alPrio);
	mvGradingLayers.insert(mvGradingLayers.begin() + lAt, {alPrio, asName, pTex, afAlpha});
}

void cSomaToneMapping::RemoveGradingLayer(const tString &asName)
{
	for (size_t i = 0; i < mvGradingLayers.size(); ++i)
		if (mvGradingLayers[i].msName == asName)
		{
			gpSomaBase->mpEngine->GetResources()->GetTextureManager()->Destroy(mvGradingLayers[i].mpTexture);
			mvGradingLayers.erase(mvGradingLayers.begin() + i);
			return;
		}
}

void cSomaToneMapping::FadeExposure(float afExposure, float afWhiteCut, float afTime)
{
	mfWorldExposure = afExposure;
	mfWorldWhiteCut = afWhiteCut;
	mfWorldFadeTime = afTime;
}

void cSomaToneMapping::Update(float afTimeStep)
{
	Pull(this);
	gTone.grading_blend = mfGradingBlend;
	gTone.grading_fade_time = mfGradingFadeTime;
	bool bWorld = mpWorld && mpWorld == gpSomaBase->GetCurrentWorld();
	OhplGs7ExposureArea area;
	bool bArea = false;
	if (bWorld)
	{
		cCamera *pCam = gpSomaBase->GetDebugCamera();
		bArea = pCam && AreaAt(mpWorld, pCam->GetPosition(), &area) >= 0;
	}
	ohpl_gs7_tone_update_exposure(&gTone, bWorld, bArea ? &area : NULL, afTimeStep);
	Push(this);
	cRendererDeferred::SetToneMapping(mfKey, ohpl_gs7_tone_exposure_scale(mfExposure), mfWhiteCut, mfGamma);
	if (ohpl_gs7_tone_update_grading(&gTone, mpGradingTarget != NULL, mpGradingTexture != NULL, afTimeStep))
	{
		if (mpGradingTexture) gpSomaBase->mpEngine->GetResources()->GetTextureManager()->Destroy(mpGradingTexture);
		mpGradingTexture = mpGradingTarget;
		mpGradingTarget = NULL;
	}
	mfGradingBlend = gTone.grading_blend;
	if (mvGradingLayers.empty() == false && mpGradingTexture == NULL) mpGradingTexture = CreateGrading("grading_default.dds");
	std::vector<std::pair<iTexture *, float>> vLayers;
	for (cGradingLayer &layer : mvGradingLayers) vLayers.push_back({layer.mpTexture, layer.mfAlpha});
	cRendererDeferred::SetColorGradingLayers(vLayers);
	cRendererDeferred::SetColorGradingTexture(mbColorGradingActive ? mpGradingTexture : NULL, mbColorGradingActive ? mpGradingTarget : NULL,
											  mfGradingBlend);
	cRendererDeferred::SetBloom(mbBloomActive && SomaUserConfig()->GetBool("Graphics", "BloomActive", true), mfBrightPass,
								mfBloomWidth, mBloomTint);
	static iTexture *pNoise = NULL;
	if (pNoise == NULL && mbFilmGrainActive)
	{
		pNoise = gpSomaBase->mpEngine->GetResources()->GetTextureManager()->Create2D("core_noise2D.dds", false);
		if (pNoise) pNoise->SetWrapSTR(eTextureWrap_Repeat);
	}
	cRendererDeferred::SetFilmGrain(mbFilmGrainActive ? pNoise : NULL, mfFilmGrainIntensity);
	cRendererDeferred::SetToneMapSRGB(mbSRGB);
}

void cSomaToneMapping::RegisterNatives(asIScriptEngine *e)
{
	typedef cSomaToneMapping T;
	const char *pType = "cPostEffect_ToneMapping";
	// Effect handlers init before the first viewport exists
	SOMA_FUNC(e, "cViewport@ cLux_GetViewport()", +[]() -> cViewport * {
		cViewport *pViewport = gpSomaBase->GetCurrentViewport();
		return pViewport ? pViewport : (cViewport *)SomaScriptDummyOf(asGetActiveContext()->GetEngine(), "cViewport");
	});
	e->RegisterObjectMethod("cViewport", "cPostEffect_ToneMapping@ GetToneMappingEffect()",
							asFUNCTION(+[](asIScriptGeneric *g) { g->SetReturnAddress(T::Get()); }), asCALL_GENERIC);

	SOMA_METHOD(e, pType, "void FadeExposure(float afExposure, float afWhiteCut, float afTime)",
				+[](T *p, float x, float w, float t) { p->FadeExposure(x, w, t); });
	SOMA_METHOD(e, pType, "void FadeWindowExposure(float afExposure, float afWhiteCut)",
				+[](T *p, float x, float w) { p->FadeExposure(x, w, p->mfTransitionTime); });
	SOMA_METHOD(e, pType, "float GetTransitionTime()", +[](T *p) -> float { return p->mfTransitionTime; });
	SOMA_METHOD(e, pType, "void SetColorGradingActive(bool abX)", +[](T *p, bool b) { p->mbColorGradingActive = b; });
	SOMA_METHOD(e, pType, "void SetBloomActive(bool abX)", +[](T *p, bool b) { p->mbBloomActive = b; });
	SOMA_METHOD(e, pType, "void SetFilmGrainActive(bool abX)", +[](T *p, bool b) { p->mbFilmGrainActive = b; });
	SOMA_METHOD(e, pType, "bool GetColorGradingActive()", +[](T *p) -> bool { return p->mbColorGradingActive; });
	SOMA_METHOD(e, pType, "bool GetBloomActive()", +[](T *p) -> bool { return p->mbBloomActive; });
	SOMA_METHOD(e, pType, "bool GetFilmGrainActive()", +[](T *p) -> bool { return p->mbFilmGrainActive; });
	SOMA_METHOD(e, pType, "void SetSRGBGamma(bool abX)", +[](T *p, bool b) { p->mbSRGB = b; });
	SOMA_METHOD(e, pType, "float GetExposure()", +[](T *p) -> float { return p->mfExposure; });
	SOMA_METHOD(e, pType,
				"void GetParams(float &out afKey, float &out afGammaCorrection, float &out afFilmGrainIntensity, float &out afBrightPass, "
				"float &out afBloomWidth, cColor &out avBloomTint, float &out afBloomFalloff)",
				+[](T *p, float &k, float &g, float &i, float &b, float &w, cColor &c, float &f) {
					k = p->mfKey; g = p->mfGamma; i = p->mfFilmGrainIntensity; b = p->mfBrightPass;
					w = p->mfBloomWidth; c = p->mBloomTint; f = p->mfBloomFalloff;
				});
	SOMA_METHOD(e, pType,
				"void SetParams(float afKey, float afGammaCorrection, float afFilmGrainIntensity, float afBrightPass, "
				"float afBloomWidth, const cColor&in avBloomTint, float afBloomFalloff)",
				+[](T *p, float k, float g, float i, float b, float w, const cColor &c, float f) {
					p->mfKey = k; p->mfGamma = g; p->mfFilmGrainIntensity = i; p->mfBrightPass = b;
					p->mfBloomWidth = w; p->mBloomTint = c; p->mfBloomFalloff = f;
				});

	SOMA_METHOD(e, "cWorld", "void FadeToneMappingExposure(float afX, float afTime)", +[](cWorld *, float x, float t) { T::Get()->FadeWorldExposure(x, t); });
	SOMA_METHOD(e, "cWorld", "void FadeToneMappingWhiteCut(float afX, float afTime)", +[](cWorld *, float x, float t) { T::Get()->FadeWorldWhiteCut(x, t); });
	SOMA_METHOD(e, "cWorld", "void SetToneMappingKey(float afX)", +[](cWorld *, float x) { T::Get()->mfKey = x; });
	SOMA_METHOD(e, "cWorld", "float GetToneMappingExposure()", +[](cWorld *) -> float { return T::Get()->mfWorldExposure; });
	SOMA_METHOD(e, "cWorld", "float GetToneMappingKey()", +[](cWorld *) -> float { return T::Get()->mfKey; });
	SOMA_METHOD(e, "cWorld", "float GetToneMappingWhiteCut()", +[](cWorld *) -> float { return T::Get()->mfWorldWhiteCut; });
	SOMA_METHOD(e, "cWorld", "void FadeGradingTexture(const tString&in asTexture, float afTime)", +[](cWorld *, const tString &s, float t) { T::Get()->FadeGrading(s, t); });
	SOMA_METHOD(e, pType, "void FadeGradingTexture(iTexture @apGrading, float afTime)", +[](T *p, iTexture *x, float t) { if (x) p->FadeGrading(x->GetName(), t); });
	SOMA_FUNC(e, "void cScene_FadeGradingTexture(cWorld@ apWorld, iTexture@ apGrading, float afTime)", +[](cWorld *, iTexture *x, float t) { if (x) T::Get()->FadeGrading(x->GetName(), t); });
	SOMA_FUNC(e, "iTexture@ cResources_CreateTexture3D(const tString&in asName, bool abUseMipMaps)", +[](const tString &s, bool) { return CreateGrading(s); });
	SOMA_METHOD(e, pType, "void SetGradingTexture(int alPrio, const tString&in asTexture, float afAlpha)", +[](T *p, int n, const tString &s, float a) { p->SetGradingLayer(n, s, a); });
	SOMA_METHOD(e, pType, "void RemoveGradingTexture(const tString&in asTexture)", +[](T *p, const tString &s) { p->RemoveGradingLayer(s); });
	SOMA_METHOD(e, "cWorld", "float GetToneMappingFadeTime()", +[](cWorld *) -> float { return T::Get()->mfWorldFadeTime; });
}
