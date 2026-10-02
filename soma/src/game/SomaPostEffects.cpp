#include "SomaPostEffects.h"
#include "SomaScriptBind.h"

#include <angelscript.h>
#include <algorithm>
#include <cstring>
#include <set>

namespace
{
	std::set<cSomaPostEffect *> gsetEffects;
	cSomaPostEffectComposite gViewportComposite;

	cSomaPostEffect *Effect(void *apObj)
	{
		cSomaPostEffect *pEffect = static_cast<cSomaPostEffect *>(apObj);
		return gsetEffects.count(pEffect) ? pEffect : NULL;
	}
}

cSomaPostEffect::cSomaPostEffect(const tString &asType) : msType(asType) { gsetEffects.insert(this); }
cSomaPostEffect::~cSomaPostEffect() { gsetEffects.erase(this); }

void cSomaPostEffect::Reset()
{
	mbActive = true;
	std::fill(mfParams, mfParams + 8, 0.0f);
	mvTextures.fill(NULL);
}

void cSomaPostEffectComposite::Add(cSomaPostEffect *apEffect, int alPrio)
{
	Remove(apEffect);
	auto it = std::find_if(mvEffects.begin(), mvEffects.end(), [&](const auto &e) { return e.first > alPrio; });
	mvEffects.insert(it, std::make_pair(alPrio, apEffect));
}

void cSomaPostEffectComposite::Remove(cSomaPostEffect *apEffect)
{
	std::erase_if(mvEffects, [&](const auto &e) { return e.second == apEffect; });
}

cSomaPostEffect *cSomaPostEffectComposite::FromType(const tString &asType)
{
	for (auto &e : mvEffects)
		if (e.second->msType == asType)
			return e.second;
	return NULL;
}

cSomaPostEffectComposite *cSomaPostEffectComposite::GetViewport() { return &gViewportComposite; }

void cSomaPostEffects::RegisterNatives(asIScriptEngine *e)
{
	typedef cSomaPostEffectComposite C;
	e->RegisterObjectMethod("cViewport", "cPostEffectComposite@ GetPostEffectComposite()",
							asFUNCTION(+[](asIScriptGeneric *g) { g->SetReturnAddress(C::GetViewport()); }), asCALL_GENERIC);
	SOMA_FUNC(e, "cPostEffectComposite@ cGraphics_CreatePostEffectComposite()", +[]() { return new C(); });
	SOMA_FUNC(e, "void cGraphics_DestroyPostEffectComposite(cPostEffectComposite@ apComposite)",
			  +[](C *p) { if (p != C::GetViewport()) delete p; });
	SOMA_METHOD(e, "cPostEffectComposite", "void AddPostEffect(iPostEffect@ apPostEffect, int alPrio)",
				+[](C *c, void *p, int l) { if (cSomaPostEffect *pEffect = Effect(p)) c->Add(pEffect, l); });
	SOMA_METHOD(e, "cPostEffectComposite", "void RemovePostEffect(iPostEffect@ apPostEffect)",
				+[](C *c, void *p) { if (cSomaPostEffect *pEffect = Effect(p)) c->Remove(pEffect); });
	SOMA_METHOD(e, "cPostEffectComposite", "int GetPostEffectNum()", +[](C *c) { return (int)c->mvEffects.size(); });
	SOMA_METHOD(e, "cPostEffectComposite", "iPostEffect@ GetPostEffect(int alIdx)", +[](C *c, int i) -> void * {
		return i >= 0 && i < (int)c->mvEffects.size() ? c->mvEffects[i].second : NULL;
	});
	SOMA_METHOD(e, "cPostEffectComposite", "iPostEffect@ GetPostEffectFromType(const tString&in asType)",
				+[](C *c, const tString &s) -> void * { return c->FromType(s); });
	SOMA_METHOD(e, "cPostEffectComposite", "bool HasActiveEffects()", +[](C *c) {
		for (auto &p : c->mvEffects)
			if (p.second->IsOn()) return true;
		return false;
	});
	SOMA_FUNC(e, "void cGraphics_DestroyPostEffect(iPostEffect@ apPostEffect)", +[](void *p) {
		if (cSomaPostEffect *pEffect = Effect(p))
		{
			gViewportComposite.Remove(pEffect);
			delete pEffect;
		}
	});

	const char *vTypes[] = {"iPostEffect", "cPostEffect_ChromaticAberration", "cPostEffect_FXAA", "cPostEffect_ImageFadeFX",
							"cPostEffect_ImageTrail", "cPostEffect_RadialBlur", "cPostEffect_VideoDistortion"};
	for (const char *pType : vTypes)
	{
		SOMA_METHOD(e, pType, "void SetDisabled(bool abX)", +[](void *p, bool b) { if (cSomaPostEffect *x = Effect(p)) x->mbDisabled = b; });
		SOMA_METHOD(e, pType, "bool IsDisabled()", +[](void *p) { cSomaPostEffect *x = Effect(p); return x && x->mbDisabled; });
		SOMA_METHOD(e, pType, "void SetActive(bool abX)", +[](void *p, bool b) { if (cSomaPostEffect *x = Effect(p)) x->mbActive = b; });
		SOMA_METHOD(e, pType, "bool IsActive()", +[](void *p) { cSomaPostEffect *x = Effect(p); return x && x->mbActive; });
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
