#include "SomaPostEffects.h"
#include "SomaBase.h"
#include "SomaScriptBind.h"

#include <angelscript.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace
{
	struct cType
	{
		const char *mpName, *mpShader;
		int mlKey;
		float mfDefaults[8];
	};
	const cType gvTypes[] = {
		{"ChromaticAberration", "chromatic_aberration", 0, {0, kPif / 3}},
		{"RadialBlur", "radial_blur", 1, {0.06f}},
		{"ImageTrail", "image_trail", 0, {0.3f}},
		{"ImageFadeFX", "image_fade_fx", 0, {1}},
		{"VideoDistortion", "video_distortion", 0, {1, 0, 0.15f, 1}},
		{"FXAA", NULL, -1, {}},
	};
	enum { eChromatic, eRadialBlur, eImageTrail, eImageFade, eVideoDistortion };
	iGpuProgram *gvPrograms[std::size(gvTypes)] = {};
	bool gvProgramTried[std::size(gvTypes)] = {};

	std::set<cSomaPostEffect *> gsetEffects;
	cPostEffectComposite *gpViewportComposite = NULL;

	cSomaPostEffect *Effect(void *apObj)
	{
		cSomaPostEffect *pEffect = static_cast<cSomaPostEffect *>(apObj);
		return gsetEffects.count(pEffect) ? pEffect : NULL;
	}

	cColor Hue(float afDeg)
	{
		auto f = [&](float n) { float k = fmodf(n + afDeg / 60.0f, 6.0f); return 1 - std::max(0.0f, std::min({k, 4 - k, 1.0f})); };
		return cColor(f(5), f(3), f(1), 1);
	}
}

cSomaPostEffect::cSomaPostEffect(const tString &asType)
	: iPostEffect(gpSomaBase->mpEngine->GetGraphics(), gpSomaBase->mpEngine->GetResources(), NULL), msType(asType)
{
	mlType = std::find_if(std::begin(gvTypes), std::end(gvTypes), [&](const cType &t) { return asType == t.mpName; }) - gvTypes;
	std::copy(gvTypes[mlType].mfDefaults, gvTypes[mlType].mfDefaults + 8, mfParams);
	gsetEffects.insert(this);
}

cSomaPostEffect::~cSomaPostEffect() { gsetEffects.erase(this); }

void cSomaPostEffect::Set(std::initializer_list<float> alParams)
{
	std::copy(alParams.begin(), alParams.end(), mfParams);
	SetActive(mfParams[gvTypes[mlType].mlKey] > 0);
}

iTexture *cSomaPostEffect::RenderEffect(iTexture *apIn, iFrameBuffer *apOut)
{
	if (mlType == eImageTrail)
	{
		// applied next frame by cRendererDeferred::RenderImageTrail, on the HDR buffer
		float fFrameTime = mpCurrentComposite->GetCurrentFrameTime();
		cRendererDeferred::SetImageTrailAlpha(mbClear ? 1 : std::max(0.05f, 1 - powf(1 - expf(mfParams[0] * -60 * 0.015f), fFrameTime * 60)));
		mbClear = false;
		return apIn;
	}
	const cType &type = gvTypes[mlType];
	if (type.mpShader && gvProgramTried[mlType] == false)
	{
		gvProgramTried[mlType] = true;
		cParserVarContainer vars;
		vars.Add("UseUv");
		gvPrograms[mlType] = mpGraphics->CreateGpuProgramFromShaders(tString("Soma") + type.mpName, "posteffect_quad_vtx.glsl",
																	 tString("posteffect_") + type.mpShader + "_frag.glsl", &vars);
	}
	iGpuProgram *pProg = gvPrograms[mlType];
	if (pProg == NULL)
		return apIn;

	cPostEffectComposite *c = mpCurrentComposite;
	auto U = [&](const char *n) { return pProg->GetVariableId(n); };
	const float *p = mfParams;
	cVector2f vTex = apIn->GetSizeFloat2D();
	float fFrameTime = c->GetCurrentFrameTime();

	c->SetFlatProjection();
	c->SetBlendMode(eMaterialBlendMode_None);
	c->SetChannelMode(eMaterialChannelMode_RGBA);
	c->SetTextureRange(NULL, 1);
	c->SetTexture(0, apIn);

	SetFinalFrameBuffer(apOut);
	c->SetProgram(pProg);
	if (mlType != eRadialBlur)
		apIn->SetFilter(eTextureFilter_Bilinear);

	if (mlType == eChromatic)
	{
		cVector2f vScale = cVector2f(mpLowLevelGraphics->GetScreenSizeFloat().y * p[0]) / vTex;
		cVector2f vDir(sinf(p[1]), -cosf(p[1])), vOffset(p[3], p[4]);
		pProg->SetVec2f(U("avOffsetA"), (vOffset + vDir) * vScale);
		pProg->SetVec2f(U("avOffsetB"), vOffset * vScale);
		pProg->SetVec2f(U("avOffsetC"), (vOffset - vDir) * vScale);
		cColor vCol[3] = {Hue(fmodf(p[2], 360)), Hue(fmodf(p[2] + 120, 360)), Hue(fmodf(p[2] + 240, 360))};
		cColor sum = vCol[0] + vCol[1] + vCol[2];
		const char *vNames[] = {"avColorA", "avColorB", "avColorC"};
		for (int i = 0; i < 3; ++i)
			pProg->SetVec4f(U(vNames[i]), vCol[i].r / sum.r, vCol[i].g / sum.g, vCol[i].b / sum.b, vCol[i].a / sum.a);
	}
	else if (mlType == eRadialBlur)
	{
		pProg->SetFloat(U("afSize"), p[0]);
		pProg->SetFloat(U("afBlurStartDist"), p[2]);
		pProg->SetFloat(U("afAlpha"), p[1]);
	}
	else if (mlType == eImageFade)
	{
		mfT += fFrameTime;
		pProg->SetVec2f(U("avScreenSize"), vTex);
		pProg->SetFloat(U("afAmount"), p[0]);
		pProg->SetFloat(U("afT"), mfT);
		for (int i = 0; i < 3; ++i)
			c->SetTexture(i + 1, mvTextures[i]);
	}
	else if (mlType == eVideoDistortion)
	{
		pProg->SetVec2f(U("avScreenSize"), vTex);
		pProg->SetFloat(U("afAmount"), p[0]);
		pProg->SetFloat(U("afSeed"), p[1]);
		pProg->SetFloat(U("afLineDensity"), p[2]);
		pProg->SetFloat(U("afOffsetMul"), p[3]);
		pProg->SetVec2f(U("avScreenOffset"), cVector2f(p[4], p[5]));
		pProg->SetVec2f(U("avScreenBendAmount"), cVector2f(p[6], p[7]));
	}

	DrawQuad(0, 1, apIn, true);
	c->SetProgram(NULL);
	c->SetBlendMode(eMaterialBlendMode_None);
	c->SetTextureRange(NULL, 0);
	return apOut->GetColorBuffer(0)->ToTexture();
}

cPostEffectComposite *cSomaPostEffects::GetViewportComposite()
{
	if (gpViewportComposite == NULL)
		gpViewportComposite = gpSomaBase->mpEngine->GetGraphics()->CreatePostEffectComposite();
	return gpViewportComposite;
}

void cSomaPostEffects::RegisterNatives(asIScriptEngine *e)
{
	typedef cPostEffectComposite C;
	e->RegisterObjectMethod("cViewport", "cPostEffectComposite@ GetPostEffectComposite()",
							asFUNCTION(+[](asIScriptGeneric *g) { g->SetReturnAddress(GetViewportComposite()); }), asCALL_GENERIC);
	SOMA_METHOD(e, "cPostEffectComposite", "void AddPostEffect(iPostEffect@ apPostEffect, int alPrio)", +[](C *c, void *p, int l) {
		if (cSomaPostEffect *pEffect = Effect(p))
		{
			c->RemovePostEffect(pEffect);
			c->AddPostEffect(pEffect, -l);
		}
	});
	SOMA_METHOD(e, "cPostEffectComposite", "void RemovePostEffect(iPostEffect@ apPostEffect)",
				+[](C *c, void *p) { if (cSomaPostEffect *pEffect = Effect(p)) c->RemovePostEffect(pEffect); });
	SOMA_METHOD(e, "cPostEffectComposite", "iPostEffect@ GetPostEffect(int alIdx)", +[](C *c, int i) -> void * {
		return i >= 0 && i < c->GetPostEffectNum() ? c->GetPostEffect(i) : NULL;
	});
	SOMA_METHOD(e, "cPostEffectComposite", "iPostEffect@ GetPostEffectFromType(const tString&in asType)", +[](C *c, const tString &s) -> void * {
		for (int i = 0; i < c->GetPostEffectNum(); ++i)
			if (cSomaPostEffect *p = Effect(c->GetPostEffect(i)); p && p->msType == s)
				return p;
		return NULL;
	});
	SOMA_FUNC(e, "void cGraphics_DestroyPostEffect(iPostEffect@ apPostEffect)", +[](void *p) {
		if (cSomaPostEffect *pEffect = Effect(p))
		{
			GetViewportComposite()->RemovePostEffect(pEffect);
			delete pEffect;
		}
	});

	const char *vTypes[] = {"iPostEffect", "cPostEffect_ChromaticAberration", "cPostEffect_FXAA", "cPostEffect_ImageFadeFX",
							"cPostEffect_ImageTrail", "cPostEffect_RadialBlur", "cPostEffect_VideoDistortion"};
	for (const char *pType : vTypes)
	{
		SOMA_METHOD(e, pType, "void SetDisabled(bool abX)", +[](void *p, bool b) { if (cSomaPostEffect *x = Effect(p)) x->SetDisabled(b); });
		SOMA_METHOD(e, pType, "bool IsDisabled()", +[](void *p) { cSomaPostEffect *x = Effect(p); return x && x->IsDisabled(); });
		SOMA_METHOD(e, pType, "void SetActive(bool abX)", +[](void *p, bool b) { if (cSomaPostEffect *x = Effect(p)) x->SetActive(b); });
		SOMA_METHOD(e, pType, "bool IsActive()", +[](void *p) { cSomaPostEffect *x = Effect(p); return x && x->IsActive(); });
		if (strcmp(pType, "iPostEffect") != 0)
			SOMA_METHOD(e, pType, "void Reset()", +[](void *p) { if (cSomaPostEffect *x = Effect(p)) x->Reset(); });
	}

	typedef cSomaPostEffect P;
	SOMA_FUNC(e, "cPostEffect_ChromaticAberration@ cGraphics_CreatePostEffect_ChromaticAberration()", +[]() { return new P("ChromaticAberration"); });
	SOMA_FUNC(e, "cPostEffect_FXAA@ cGraphics_CreatePostEffect_FXAA()", +[]() { return new P("FXAA"); });
	SOMA_FUNC(e, "cPostEffect_ImageFadeFX@ cGraphics_CreatePostEffect_ImageFadeFX()", +[]() { return new P("ImageFadeFX"); });
	SOMA_FUNC(e, "cPostEffect_ImageTrail@ cGraphics_CreatePostEffect_ImageTrail()", +[]() { return new P("ImageTrail"); });
	SOMA_FUNC(e, "cPostEffect_RadialBlur@ cGraphics_CreatePostEffect_RadialBlur()", +[]() { return new P("RadialBlur"); });
	SOMA_FUNC(e, "cPostEffect_VideoDistortion@ cGraphics_CreatePostEffect_VideoDistortion()", +[]() { return new P("VideoDistortion"); });

	SOMA_METHOD(e, "cPostEffect_ChromaticAberration", "void SetParams(float afAmount, float afRotation, float afHue, const cVector2f&in avOffset)",
				+[](P *p, float a, float r, float h, const cVector2f &o) { p->Set({a, r, h, o.x, o.y}); });
	SOMA_METHOD(e, "cPostEffect_ChromaticAberration", "void GetParams(float &out afAmount, float &out afRotation, float &out afHue, cVector2f &out avOffset)",
				+[](P *p, float &a, float &r, float &h, cVector2f &o) { a = p->mfParams[0]; r = p->mfParams[1]; h = p->mfParams[2]; o = cVector2f(p->mfParams[3], p->mfParams[4]); });
	SOMA_METHOD(e, "cPostEffect_ImageFadeFX", "void SetParams(float afAmount, iTexture@ apFadeTexture, iTexture@ apColorTexture, iTexture@ apOffsetTexture)",
				+[](P *p, float a, iTexture *f, iTexture *c, iTexture *o) { p->Set({a}); p->mvTextures = {f, c, o}; });
	SOMA_METHOD(e, "cPostEffect_ImageFadeFX", "void GetParams(float &out afAmount)", +[](P *p, float &a) { a = p->mfParams[0]; });
	SOMA_METHOD(e, "cPostEffect_ImageTrail", "void SetParams(float afAmount)", +[](P *p, float a) { p->Set({a}); });
	SOMA_METHOD(e, "cPostEffect_ImageTrail", "void GetParams(float &out afAmount)", +[](P *p, float &a) { a = p->mfParams[0]; });
	SOMA_METHOD(e, "cPostEffect_RadialBlur", "void SetParams(float afSize, float afAlpha, float afBlurStartDist)",
				+[](P *p, float s, float a, float d) { p->Set({s, a, d}); });
	SOMA_METHOD(e, "cPostEffect_RadialBlur", "void GetParams(float &out afSize, float &out afAlpha, float &out afBlurStartDist)",
				+[](P *p, float &s, float &a, float &d) { s = p->mfParams[0]; a = p->mfParams[1]; d = p->mfParams[2]; });
	SOMA_METHOD(e, "cPostEffect_VideoDistortion",
				"void SetParams(float afAmount, float afRandomSeed, float afLineDensity, float afOffsetMul, const cVector2f&in avScreenOffset, const cVector2f&in avScreenBendAmount)",
				+[](P *p, float a, float s, float l, float m, const cVector2f &o, const cVector2f &b) { p->Set({a, s, l, m, o.x, o.y, b.x, b.y}); });
	SOMA_METHOD(e, "cPostEffect_VideoDistortion",
				"void GetParams(float &out afAmount, float&out afRandomSeed, float&out afLineDensity, float&out afOffsetMul, cVector2f&out avScreenOffset, cVector2f&out avScreenBendAmount)",
				+[](P *p, float &a, float &s, float &l, float &m, cVector2f &o, cVector2f &b) {
					a = p->mfParams[0]; s = p->mfParams[1]; l = p->mfParams[2]; m = p->mfParams[3];
					o = cVector2f(p->mfParams[4], p->mfParams[5]); b = cVector2f(p->mfParams[6], p->mfParams[7]);
				});
}
