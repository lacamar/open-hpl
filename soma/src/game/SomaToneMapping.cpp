#include "SomaToneMapping.h"
#include "SomaBase.h"
#include "SomaScriptBind.h"
#include "SomaScriptApi.h"
#include "SomaLuxGame.h"

#include <angelscript.h>

cSomaToneMapping *cSomaToneMapping::mpInstance = NULL;

namespace
{
	struct cFade
	{
		float mfGoal = 1e30f;
		float mfSpeed = 0;
	};
	cFade gExposureFade, gWhiteCutFade;

	void Step(float &afValue, cFade &aFade, float afGoal, float afTime, float afTimeStep)
	{
		if (afGoal != aFade.mfGoal)
		{
			aFade.mfGoal = afGoal;
			aFade.mfSpeed = afTime > 0 ? std::fabs(afGoal - afValue) / afTime : 1e30f;
		}
		float fStep = aFade.mfSpeed * afTimeStep;
		if (std::fabs(afGoal - afValue) <= fStep) afValue = afGoal;
		else afValue += afGoal > afValue ? fStep : -fStep;
	}

	const cWorldExposureArea *AreaAt(cWorld *apWorld, const cVector3f &avPos)
	{
		for (const cWorldExposureArea &area : apWorld->GetExposureAreas())
		{
			cVector3f vLocal = cMath::MatrixMul(area.m_mtxInvTransform, avPos);
			if (std::fabs(vLocal.x) <= area.mvHalfSize.x && std::fabs(vLocal.y) <= area.mvHalfSize.y &&
				std::fabs(vLocal.z) <= area.mvHalfSize.z)
				return &area;
		}
		return NULL;
	}
}

cSomaToneMapping::cSomaToneMapping() : iUpdateable("SomaToneMapping")
{
	mpInstance = this;
	mpWorld = NULL;
	mpGradingTexture = NULL;
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
	mfWorldExposure = mfExposure = 0;
	mfWorldWhiteCut = mfWhiteCut = 3.5f;
	mfWorldFadeTime = mfTransitionTime = 0.5f;
}

void cSomaToneMapping::OnMapLoaded(cWorld *apWorld)
{
	mpWorld = apWorld;
	cTextureManager *pTexMgr = gpSomaBase->mpEngine->GetResources()->GetTextureManager();
	if (mpGradingTexture) pTexMgr->Destroy(mpGradingTexture);
	mpGradingTexture = NULL;
	if (apWorld->GetColorGradingTexture() != "")
	{
		mpGradingTexture = pTexMgr->Create3D(apWorld->GetColorGradingTexture(), false);
		if (mpGradingTexture)
		{
			mpGradingTexture->SetWrapSTR(eTextureWrap_ClampToEdge);
			mpGradingTexture->SetFilter(eTextureFilter_Bilinear);
		}
	}
	mfKey = apWorld->GetToneMappingKey();
	mfWorldExposure = mfExposure = apWorld->GetToneMappingExposure();
	mfWorldWhiteCut = mfWhiteCut = apWorld->GetToneMappingWhiteCut();
	mfWorldFadeTime = 0.5f;
	cCamera *pCam = gpSomaBase->GetDebugCamera();
	const cWorldExposureArea *pArea = pCam ? AreaAt(apWorld, pCam->GetPosition()) : NULL;
	if (pArea)
	{
		mfExposure = pArea->mfExposure;
		mfWhiteCut = pArea->mfWhiteCut;
	}
	gExposureFade = cFade();
	gWhiteCutFade = cFade();
	Update(0);
}

void cSomaToneMapping::FadeExposure(float afExposure, float afWhiteCut, float afTime)
{
	mfWorldExposure = afExposure;
	mfWorldWhiteCut = afWhiteCut;
	mfWorldFadeTime = afTime;
}

void cSomaToneMapping::Update(float afTimeStep)
{
	if (mpWorld && mpWorld == gpSomaBase->GetCurrentWorld())
	{
		float fGoalExp = mfWorldExposure, fGoalWhiteCut = mfWorldWhiteCut;
		mfTransitionTime = mfWorldFadeTime;
		cCamera *pCam = gpSomaBase->GetDebugCamera();
		const cWorldExposureArea *pArea = pCam ? AreaAt(mpWorld, pCam->GetPosition()) : NULL;
		if (pArea)
		{
			fGoalExp = pArea->mfExposure;
			fGoalWhiteCut = pArea->mfWhiteCut;
			mfTransitionTime = pArea->mfTransitionTime;
		}
		Step(mfExposure, gExposureFade, fGoalExp, mfTransitionTime, afTimeStep);
		Step(mfWhiteCut, gWhiteCutFade, fGoalWhiteCut, mfTransitionTime, afTimeStep);
	}
	cRendererDeferred::SetToneMapping(mfKey, powf(2.0f, mfExposure), mfWhiteCut, mfGamma);
	cRendererDeferred::SetColorGradingTexture(mbColorGradingActive ? mpGradingTexture : NULL);
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
	SOMA_METHOD(e, "cWorld", "float GetToneMappingFadeTime()", +[](cWorld *) -> float { return T::Get()->mfWorldFadeTime; });
}
