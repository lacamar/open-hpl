#include "SomaLuxVoice.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaImGui.h"
#include "SomaLuxEntity.h"
#include "SomaSound.h"
#include "SomaScriptRuntime.h"

#include "impl/RustEasArray.h"
#include <fstream>


cSomaLuxVoiceHandler *cSomaLuxVoiceHandler::mpInstance = NULL;

void SomaMapScriptCall(const tString &asDecl, const std::function<void(asIScriptContext *)> &aSetArgs)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap && pMap->GetScript() && cSomaScriptRuntime::Get())
		cSomaScriptRuntime::Get()->Call(pMap->GetScript(), asDecl, aSetArgs);
}

namespace
{
cEngine *E() { return cSomaLuxVoiceHandler::Get()->GetEngine(); }
cSoundHandler *H() { return E()->GetSound()->GetSoundHandler(); }
tString Str(const unsigned char *p, size_t n) { return tString((const char *)p, n); }
void Vec3(float *o, const cVector3f &v)
{
	o[0] = v.x;
	o[1] = v.y;
	o[2] = v.z;
}

bool PlayOut(cSoundEntry *p, size_t *pe, int *pid)
{
	if (p == NULL)
		return false;
	*pe = (size_t)p;
	*pid = p->GetId();
	return true;
}

bool EffectOut(cSomaSoundInstance *p, size_t *pe, int *pid)
{
	if (p == NULL)
		return false;
	*pe = (size_t)p;
	*pid = p->GetId();
	return true;
}

const OhplS5bVoiceHost kHost = {
	[](size_t e, int id) { return H()->IsValid((cSoundEntry *)e, id); },
	[](float *o) { Vec3(o, E()->GetSound()->GetLowLevel()->GetListenerPosition()); },
	[](const unsigned char *n, size_t l, float *o) {
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		cSomaLuxEntity *p = pMap ? pMap->GetEntity(Str(n, l)) : NULL;
		if (p == NULL)
			return false;
		Vec3(o, p->GetPosition());
		return true;
	},
	[]() { return cSomaLuxMap::GetCurrent() != NULL; },
	[](const unsigned char *f, size_t l) { return E()->GetResources()->GetFileSearcher()->GetFilePath(Str(f, l)) != _W(""); },
	[](const unsigned char *f, size_t l, void (*sink)(void *, const unsigned char *, size_t), void *ctx) {
		std::ifstream file(cString::To8Char(E()->GetResources()->GetFileSearcher()->GetFilePath(Str(f, l))).c_str(), std::ios::binary);
		if (!file)
			return false;
		std::string d((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		sink(ctx, (const unsigned char *)d.data(), d.size());
		return true;
	},
	[](const unsigned char *f, size_t l, float vol, const float *pos, float mn, float mx, int type, size_t *pe, int *pid) {
		return PlayOut(H()->Play3D(Str(f, l), false, vol, cVector3f(pos[0], pos[1], pos[2]), mn, mx, (eSoundEntryType)type, false, 0, true), pe, pid);
	},
	[](const unsigned char *f, size_t l, float vol, int type, size_t *pe, int *pid) {
		return PlayOut(H()->PlayGuiStream(Str(f, l), false, vol, cVector3f(0, 0, 1), (eSoundEntryType)type), pe, pid);
	},
	[](const unsigned char *f, size_t l, float vol, int type, size_t *pe, int *pid) {
		return EffectOut(cSomaSoundEvents::Get()->PlayGui(Str(f, l), vol, type, false, true), pe, pid);
	},
	[](size_t e, int id) { return cSomaSoundEvents::Get()->IsLive((cSomaSoundInstance *)e, id); },
	[](size_t e, float f) { ((cSomaSoundInstance *)e)->SetVolumeMul(f); },
	[](size_t e, float f) { ((cSomaSoundInstance *)e)->FadeOut(f); },
	[](size_t e, bool b) { ((cSomaSoundInstance *)e)->SetPaused(b); },
	[](size_t e) { return ((cSomaSoundInstance *)e)->IsPlaying(); },
	[](size_t e) { return ((cSomaSoundInstance *)e)->GetTotalTime(); },
	[](size_t e) { return ((cSomaSoundInstance *)e)->GetElapsedTime(); },
	[](const unsigned char *t, size_t l) { Error("%s", Str(t, l).c_str()); },
	[](const unsigned char *t, size_t l, float afWidth, float afLineHeight, const float *s, void (*sink)(void *, const unsigned int *, size_t), void *ctx) {
		tWStringVec vRows;
		cSomaLuxVoiceHandler::Get()->mpFont->GetWordWrapRows(afWidth, afLineHeight, cVector2f(s[0], s[1]), cString::To16Char(Str(t, l)), &vRows);
		for (const tWString &r : vRows)
			sink(ctx, (const unsigned int *)r.data(), r.size());
	},
	// cLuxFadeEffect::OnDraw: on the HUD set at z 0.9, so HUD ImGui (credits) draws over it
	[](float afAlpha) {
		cGuiSet *pHud = SomaHudImGui()->GetSet();
		cVector2f vSize = pHud->GetVirtualSize();
		cVector2f vPos = pHud->GetVirtualSizeOffset() * -1.0f - vSize * 0.125f;
		pHud->DrawGfx(cSomaLuxVoiceHandler::Get()->mpFadeGfx, cVector3f(vPos.x, vPos.y, 0.9f), vSize * 1.25f, cColor(0, afAlpha));
	},
	[]() { return cSomaLuxVoiceHandler::Get()->mpFont != NULL && (gpSomaBase == NULL || gpSomaBase->GetConfig()->mbShowSubtitles); },
	[](float *o) {
		cVector2f vScreen = E()->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
		cSomaLuxVoiceHandler::Get()->mpGuiSet->SetVirtualSize(vScreen, -1000, 1000, cVector2f(0));
		o[0] = vScreen.x;
		o[1] = vScreen.y;
	},
	[](const unsigned int *t, size_t l, const float *p, const float *s, const float *c) {
		cSomaLuxVoiceHandler *pSelf = cSomaLuxVoiceHandler::Get();
		pSelf->mpGuiSet->DrawFont(tWString((const wchar_t *)t, l), pSelf->mpFont, cVector3f(p[0], p[1], p[2]), cVector2f(s[0], s[1]), cColor(c[0], c[1], c[2], c[3]),
								  eFontAlign_Center);
	},
	[]() { return gpSomaBase->mbScriptGamePaused; },
	// the viseme animation states of an entity's mesh; false when it has none
	[](const unsigned char *n, size_t l, const float *w) {
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		cSomaLuxEntity *pEnt = pMap ? pMap->GetEntity(Str(n, l)) : NULL;
		if (pEnt == NULL || pEnt->mpMesh == NULL)
			return false;
		for (int i = 0; i < 17; ++i)
		{
			cAnimationState *pState = pEnt->mpMesh->GetAnimationStateFromName("Visemes_17_" + cString::ToString(i + 1));
			if (pState == NULL)
				continue;
			if (w == NULL)
			{
				pState->FadeOut(0.1f);
				continue;
			}
			pState->SetLayer(true);
			pState->SetActive(true);
			pState->SetPaused(true);
			pState->SetFadeStep(0);
			pState->SetTimePosition(0);
			pState->SetWeight(w[i]);
		}
		return true;
	},
	[](const unsigned char *d, size_t dn, const OhplS7CallArg *a, size_t n, int lDefault) {
		return OhplMapScriptCall(d, dn, a, n, lDefault >= 0, lDefault < 0 ? 0 : lDefault);
	},
	[](const void *a) { return (unsigned)((const OhplEasArray *)a)->GetSize(); },
	[](const void *a, unsigned i) { return ((OhplEasArray *)a)->At(i); },
	[](void *a, unsigned n) { ((OhplEasArray *)a)->Resize(n); },
	[](const unsigned char *p, size_t n) -> const void * {
		static tString sRef;
		sRef.assign((const char *)p, n);
		return &sRef;
	},
};
} // namespace

cSomaLuxVoiceHandler::cSomaLuxVoiceHandler(cEngine *apEngine) : iUpdateable("SomaLuxVoice"), mpEngine(apEngine)
{
	mpInstance = this;
	ohpl_s5b_voice_install_host(&kHost);
	cGui *pGui = mpEngine->GetGui();
	mpGuiSet = pGui->CreateSet("SomaVoiceSubtitles", pGui->CreateSkin("gui_default.skin"));
	cViewport *pViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, false);
	pViewport->AddGuiSet(mpGuiSet);
	mpFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_medium_bold.fnt");
	mpFadeGfx = pGui->CreateGfxFilledRect(cColor(0, 1), eGuiMaterial_Alpha);
	ohpl_s5b_voice_create();
}

cSomaLuxVoiceHandler::~cSomaLuxVoiceHandler()
{
	if (mpInstance == this)
		mpInstance = NULL;
	ohpl_s5b_voice_destroy();
}

void cSomaLuxVoiceHandler::UpdateVoices(float afTimeStep) { ohpl_s5b_voice_update(afTimeStep, gpSomaBase->mbScriptGamePaused); }

std::set<tString> cSomaLuxVoiceHandler::PlayedLines()
{
	std::set<tString> set;
	for (size_t i = 0, n = ohpl_s5b_voice_played_count(); i < n; ++i)
		ohpl_s5b_voice_played_at(i, +[](void *c, const unsigned char *p, size_t l) { static_cast<std::set<tString> *>(c)->insert(Str(p, l)); }, &set);
	return set;
}

void cSomaLuxVoiceHandler::SetPlayedLines(const std::set<tString> &asetLines)
{
	ohpl_s5b_voice_clear_played();
	for (const tString &s : asetLines)
		ohpl_s5b_voice_played_insert((const unsigned char *)s.data(), s.size());
}
