#include "SomaLuxVoice.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaImGui.h"
#include "SomaLuxEntity.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"
#include "SomaSound.h"

#include "impl/scriptarray.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <vorbis/vorbisfile.h>
#include <sstream>

cSomaImGui *SomaHudImGui();

cSomaLuxVoiceHandler *cSomaLuxVoiceHandler::mpInstance = NULL;

void SomaMapScriptCall(const tString &asDecl, const std::function<void(asIScriptContext *)> &aSetArgs)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap && pMap->GetScript() && cSomaScriptRuntime::Get())
		cSomaScriptRuntime::Get()->Call(pMap->GetScript(), asDecl, aSetArgs);
}

namespace
{
const unsigned char *U(const tString &s) { return (const unsigned char *)s.data(); }

void SinkString(void *apCtx, const unsigned char *apData, size_t alLen) { static_cast<tString *>(apCtx)->assign((const char *)apData, alLen); }

cEngine *E() { return cSomaLuxVoiceHandler::Get()->GetEngine(); }
cSoundHandler *H() { return E()->GetSound()->GetSoundHandler(); }

struct cVoiceHost
{
	static bool EntryValid(void *, size_t e, int id) { return H()->IsValid((cSoundEntry *)e, id); }
	static void EntrySetVolumeMul(void *, size_t e, float f) { ((cSoundEntry *)e)->SetVolumeMul(f); }
	static void EntryFadeOut(void *, size_t e, float f) { ((cSoundEntry *)e)->FadeOut(f); }
	static void EntrySetPaused(void *, size_t e, bool b) { ((cSoundEntry *)e)->SetPaused(b); }
	static void ChannelSetPosition(void *, size_t e, const float *v) { ((cSoundEntry *)e)->GetChannel()->SetPosition(cVector3f(v[0], v[1], v[2])); }
	static void ChannelFilter(void *, size_t e, bool abHigh, float f)
	{
		if (abHigh)
			((cSoundEntry *)e)->GetChannel()->SetFilterGainLF(f);
		else
			((cSoundEntry *)e)->GetChannel()->SetFilterGainHF(f);
	}
	static double ChannelTotal(void *, size_t e) { return ((cSoundEntry *)e)->GetChannel()->GetTotalTime(); }
	static double ChannelElapsed(void *, size_t e) { return ((cSoundEntry *)e)->GetChannel()->GetElapsedTime(); }
	static void ListenerPos(void *, float *o)
	{
		cVector3f v = E()->GetSound()->GetLowLevel()->GetListenerPosition();
		o[0] = v.x; o[1] = v.y; o[2] = v.z;
	}
	static bool MapEntityPos(void *, const unsigned char *n, size_t l, float *o)
	{
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		cSomaLuxEntity *p = pMap ? pMap->GetEntity(tString((const char *)n, l)) : NULL;
		if (p == NULL)
			return false;
		cVector3f v = p->GetPosition();
		o[0] = v.x; o[1] = v.y; o[2] = v.z;
		return true;
	}
	static bool MapCurrent(void *) { return cSomaLuxMap::GetCurrent() != NULL; }
	static bool FileExists(void *, const unsigned char *f, size_t l)
	{
		return E()->GetResources()->GetFileSearcher()->GetFilePath(tString((const char *)f, l)) != _W("");
	}
	static bool ReadFile(void *, const unsigned char *f, size_t l, void (*sink)(void *, const unsigned char *, size_t), void *ctx)
	{
		std::ifstream file(cString::To8Char(E()->GetResources()->GetFileSearcher()->GetFilePath(tString((const char *)f, l))).c_str(), std::ios::binary);
		if (!file)
			return false;
		std::string d((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		sink(ctx, (const unsigned char *)d.data(), d.size());
		return true;
	}
	static bool Play3D(void *, const unsigned char *f, size_t l, float vol, const float *pos, float mn, float mx, int type, size_t *pe, int *pid)
	{
		cSoundEntry *p = H()->Play3D(tString((const char *)f, l), false, vol, cVector3f(pos[0], pos[1], pos[2]), mn, mx, (eSoundEntryType)type, false, 0, true);
		if (p == NULL)
			return false;
		*pe = (size_t)p;
		*pid = p->GetId();
		return true;
	}
	static bool PlayGuiStream(void *, const unsigned char *f, size_t l, float vol, int type, size_t *pe, int *pid)
	{
		cSoundEntry *p = H()->PlayGuiStream(tString((const char *)f, l), false, vol, cVector3f(0, 0, 1), (eSoundEntryType)type);
		if (p == NULL)
			return false;
		*pe = (size_t)p;
		*pid = p->GetId();
		return true;
	}
	static bool EffectPlay(void *, const unsigned char *f, size_t l, float vol, int type, size_t *pe, int *pid)
	{
		cSomaSoundInstance *p = cSomaSoundEvents::Get()->PlayGui(tString((const char *)f, l), vol, type, false, true);
		if (p == NULL)
			return false;
		*pe = (size_t)p;
		*pid = p->GetId();
		return true;
	}
	static bool EffectLive(void *, size_t e, int id) { return cSomaSoundEvents::Get()->IsLive((cSomaSoundInstance *)e, id); }
	static void EffectSetVolumeMul(void *, size_t e, float f) { ((cSomaSoundInstance *)e)->SetVolumeMul(f); }
	static void EffectFadeOut(void *, size_t e, float f) { ((cSomaSoundInstance *)e)->FadeOut(f); }
	static void EffectSetPaused(void *, size_t e, bool b) { ((cSomaSoundInstance *)e)->SetPaused(b); }
	static bool EffectIsPlaying(void *, size_t e) { return ((cSomaSoundInstance *)e)->IsPlaying(); }
	static float EffectTotal(void *, size_t e) { return ((cSomaSoundInstance *)e)->GetTotalTime(); }
	static float EffectElapsed(void *, size_t e) { return ((cSomaSoundInstance *)e)->GetElapsedTime(); }
	static bool PlayedHas(void *, const unsigned char *k, size_t l)
	{
		return cSomaLuxVoiceHandler::Get()->msetPlayedLines.count(tString((const char *)k, l)) > 0;
	}
	static bool PlayedInsert(void *, const unsigned char *k, size_t l)
	{
		return cSomaLuxVoiceHandler::Get()->msetPlayedLines.insert(tString((const char *)k, l)).second;
	}
	static void LogText(void *, const unsigned char *t, size_t l) { Log("%s", tString((const char *)t, l).c_str()); }
	static void ErrorText(void *, const unsigned char *t, size_t l) { Error("%s", tString((const char *)t, l).c_str()); }
	static size_t WrapRows(void *, const unsigned char *t, size_t l, float afWidth, float afLineHeight, const float *s)
	{
		tWStringVec vRows;
		cSomaLuxVoiceHandler::Get()->GetFont()->GetWordWrapRows(afWidth, afLineHeight, cVector2f(s[0], s[1]), cString::To16Char(tString((const char *)t, l)), &vRows);
		cSomaLuxVoiceHandler::Get()->mvRowCache.push_back(vRows);
		return vRows.size();
	}
};

const OhplGs5VoiceHost *VHost()
{
	static const OhplGs5VoiceHost host = {NULL, cVoiceHost::EntryValid, cVoiceHost::EntrySetVolumeMul, cVoiceHost::EntryFadeOut, cVoiceHost::EntrySetPaused,
										  cVoiceHost::ChannelSetPosition, cVoiceHost::ChannelFilter, cVoiceHost::ChannelTotal, cVoiceHost::ChannelElapsed,
										  cVoiceHost::ListenerPos, cVoiceHost::MapEntityPos, cVoiceHost::MapCurrent, cVoiceHost::FileExists,
										  cVoiceHost::ReadFile, cVoiceHost::Play3D, cVoiceHost::PlayGuiStream, cVoiceHost::EffectPlay,
										  cVoiceHost::EffectLive, cVoiceHost::EffectSetVolumeMul, cVoiceHost::EffectFadeOut, cVoiceHost::EffectSetPaused,
										  cVoiceHost::EffectIsPlaying, cVoiceHost::EffectTotal, cVoiceHost::EffectElapsed, cVoiceHost::PlayedHas,
										  cVoiceHost::PlayedInsert, cVoiceHost::LogText, cVoiceHost::ErrorText, cVoiceHost::WrapRows};
	return &host;
}
} // namespace

cSomaLuxVoiceHandler::cSomaLuxVoiceHandler(cEngine *apEngine) : iUpdateable("SomaLuxVoice"), mpEngine(apEngine), mpV(ohpl_gs5_voice_new())
{
	mpInstance = this;
	cGui *pGui = mpEngine->GetGui();
	mpGuiSet = pGui->CreateSet("SomaVoiceSubtitles", pGui->CreateSkin("gui_default.skin"));
	cViewport *pViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, false);
	pViewport->AddGuiSet(mpGuiSet);
	mpFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_medium_bold.fnt");
	mpFadeGfx = pGui->CreateGfxFilledRect(cColor(0, 1), eGuiMaterial_Alpha);
	LoadVoiceFile("global_voice.voice", "global");
	LoadLangFile("global_voice.lang");
}

cSomaLuxVoiceHandler::~cSomaLuxVoiceHandler()
{
	if (mpInstance == this)
		mpInstance = NULL;
	ohpl_gs5_voice_free(mpV);
}

// .voice and some .lang files are UTF-16 with a BOM
bool cSomaLuxVoiceHandler::ReadText(const tString &asFile, tString &asOut)
{
	tWString sPath = mpEngine->GetResources()->GetFileSearcher()->GetFilePath(asFile);
	if (sPath == _W(""))
		return false;
	std::ifstream file(cString::To8Char(sPath).c_str(), std::ios::binary);
	if (!file)
		return false;
	std::stringstream ss;
	ss << file.rdbuf();
	std::string s = ss.str();
	ohpl_gs5_decode_text((const unsigned char *)s.data(), s.size(), SinkString, &asOut);
	return true;
}

void cSomaLuxVoiceHandler::LoadLangFile(const tString &asFile)
{
	tString sData;
	if (ReadText(asFile, sData))
		ohpl_gs5_voice_load_lang(mpV, U(sData), sData.size());
}

bool cSomaLuxVoiceHandler::LoadVoiceFile(const tString &asFile, const tString &asSet)
{
	tString sData;
	int64_t lCount = ReadText(asFile, sData) ? ohpl_gs5_voice_load_voice(mpV, U(sData), sData.size(), U(asSet), asSet.size()) : -1;
	if (lCount < 0)
	{
		Warning("SOMA voice: could not load '%s'\n", asFile.c_str());
		return false;
	}
	Log("SOMA voice: %d subjects from '%s'\n", (int)lCount, asFile.c_str());
	return true;
}

void cSomaLuxVoiceHandler::LoadMapFile(const tString &asHpmPath, const tString &asMapName)
{
	if (ohpl_gs5_voice_mark_set(mpV, U(asMapName), asMapName.size()) == false)
		return;
	LoadVoiceFile(asMapName + ".voice", asMapName);
	LoadLangFile(asMapName + ".lang");
}

void cSomaLuxVoiceHandler::Reset()
{
	StopAll();
	msetLipEntities.clear();
	ohpl_gs5_voice_reset_state(mpV);
}

std::function<void()> cSomaLuxVoiceHandler::TakeDone(uint64_t alToken)
{
	std::function<void()> f;
	auto it = mmapOnDone.find(alToken);
	if (it != mmapOnDone.end())
	{
		f = it->second;
		mmapOnDone.erase(it);
	}
	return f;
}

bool cSomaLuxVoiceHandler::Play(const tString &asSubject, int alLine, const tString &asCallback, int alPrio, const std::function<void()> &aOnDone,
								bool abPlayWhilePaused)
{
	if (ohpl_gs5_voice_play_check(mpV, VHost(), U(asSubject), asSubject.size(), alPrio) <= 0)
		return false;
	tString sScene;
	ohpl_gs5_voice_scene_of(mpV, U(asSubject), asSubject.size(), SinkString, &sScene);
	StopScene(sScene);
	uint64_t lToken = 0;
	if (aOnDone)
	{
		lToken = mlNextToken++;
		mmapOnDone[lToken] = aOnDone;
	}
	bool bOk = ohpl_gs5_voice_play_start(mpV, VHost(), U(asSubject), asSubject.size(), alLine, U(asCallback), asCallback.size(), alPrio, lToken, abPlayWhilePaused);
	if (bOk == false && lToken)
		mmapOnDone.erase(lToken);
	return bOk;
}

void cSomaLuxVoiceHandler::StopScene(const tString &asScene)
{
	for (;;)
	{
		int64_t lToken = ohpl_gs5_voice_stop_next(mpV, VHost(), U(asScene), asScene.size());
		if (lToken < 0)
			break;
		if (lToken)
		{
			std::function<void()> onDone = TakeDone(lToken);
			if (onDone)
				onDone();
		}
	}
}

void cSomaLuxVoiceHandler::Stop(const tString &asScene)
{
	StopScene(asScene);
}

void cSomaLuxVoiceHandler::StopAll()
{
	ohpl_gs5_voice_stop_all(mpV, VHost());
	mmapOnDone.clear();
}

void cSomaLuxVoiceHandler::SkipCurrentLine(const tString &asScene)
{
	ohpl_gs5_voice_skip_line(mpV, VHost(), U(asScene), asScene.size());
}

bool cSomaLuxVoiceHandler::CharacterIsSpeaking(const tString &asName)
{
	return ohpl_gs5_voice_character_is_speaking(mpV, U(asName), asName.size());
}

void cSomaLuxVoiceHandler::SetSource(const tString &asCharacter, const tString &asEntity, float afMinDist, float afMaxDist, bool abUse3D,
									 float afListenRange, float afMinFreq, float afMaxFreq, int alFreqFlags)
{
	ohpl_gs5_voice_set_source(mpV, U(asCharacter), asCharacter.size(), U(asEntity), asEntity.size(), afMinDist, afMaxDist, abUse3D, afListenRange, afMinFreq,
							  afMaxFreq, alFreqFlags);
}

void cSomaLuxVoiceHandler::SetFocusScene(const tString &asScene)
{
	ohpl_gs5_voice_set_focus_scene(mpV, VHost(), U(asScene), asScene.size());
}

void cSomaLuxVoiceHandler::FadeSceneVolumeTo(const tString &asScene, float afVolume, float afTime)
{
	ohpl_gs5_voice_fade_scene_volume(mpV, U(asScene), asScene.size(), afVolume, afTime);
}

void cSomaLuxVoiceHandler::FadeTo(float afGoal, float afTime)
{
	ohpl_gs5_voice_fade_to(mpV, afGoal, afTime);
}

bool cSomaLuxVoiceHandler::IsFading() { return ohpl_gs5_voice_is_fading(mpV); }
float cSomaLuxVoiceHandler::GetFadeAlpha() { return ohpl_gs5_voice_fade_alpha(mpV); }
bool cSomaLuxVoiceHandler::AnySceneIsActive() { return ohpl_gs5_voice_any_active(mpV); }

void cSomaLuxVoiceHandler::UpdateVoices(float afTimeStep)
{
	const bool bGamePaused = gpSomaBase->mbScriptGamePaused;
	ohpl_gs5_voice_update_begin(mpV, VHost(), afTimeStep, bGamePaused);
	for (;;)
	{
		struct cOne
		{
			cSomaLuxVoiceHandler *mpSelf;
		} one = {this};
		bool bMore = ohpl_gs5_voice_update_step(mpV, VHost(), +[](void *apCtx, const OhplGs5VoiceEvent *e) {
			cOne *p = static_cast<cOne *>(apCtx);
			tString sFunc((const char *)e->func, e->func_len), sScene((const char *)e->scene, e->scene_len), sName((const char *)e->name, e->name_len);
			if (e->kind == 0)
			{
				// the script may stop everything; the finished scene's completion still runs
				std::function<void()> onDone = e->on_done ? p->mpSelf->TakeDone(e->on_done) : std::function<void()>();
				if (sFunc != "")
					SomaMapScriptCall("void " + sFunc + "(const tString&in, const tString&in)", [&](asIScriptContext *c) {
						c->SetArgObject(0, &sScene);
						c->SetArgObject(1, &sName);
					});
				if (onDone)
					onDone();
			}
			else
				SomaMapScriptCall("void " + sFunc + "(const tString&in, const tString&in, int, bool)", [&](asIScriptContext *c) {
					c->SetArgObject(0, &sScene);
					c->SetArgObject(1, &sName);
					c->SetArgDWord(2, e->line);
					c->SetArgByte(3, e->start);
				});
		}, &one);
		if (bMore == false)
			break;
	}
	if (bGamePaused)
		return;
	UpdateLipsync();
	UpdateSpeakingCallbacks();
}

void cSomaLuxVoiceHandler::SetSpeakingCallback(const tString &asCharacter, const tString &asFunc)
{
	ohpl_gs5_voice_set_speaking_callback(mpV, U(asCharacter), asCharacter.size(), U(asFunc), asFunc.size());
}

void cSomaLuxVoiceHandler::UpdateSpeakingCallbacks()
{
	std::vector<std::pair<tString, bool>> vChanged;
	ohpl_gs5_voice_speaking_changes(mpV, +[](void *c, const unsigned char *n, size_t l, bool b) {
		static_cast<std::vector<std::pair<tString, bool>> *>(c)->push_back({tString((const char *)n, l), b});
	}, &vChanged);
	for (auto &it : vChanged)
	{
		tString sFunc;
		if (ohpl_gs5_voice_speaking_func(mpV, U(it.first), it.first.size(), SinkString, &sFunc) == false)
			continue;
		bool bKeep = true;
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		if (pMap && pMap->GetScript() && cSomaScriptRuntime::Get())
			cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "bool " + sFunc + "(const tString&in, bool)", [&](asIScriptContext *c) {
				c->SetArgObject(0, &it.first);
				c->SetArgByte(1, it.second);
			}, [&](asIScriptContext *c) { bKeep = c->GetReturnByte() != 0; });
		ohpl_gs5_voice_speaking_done(mpV, U(it.first), it.first.size(), U(sFunc), sFunc.size(), bKeep);
	}
}

void cSomaLuxVoiceHandler::AddSubtitle(const std::vector<tString> &avRows, const tString &asName, const cColor &aColor, int alDrawnChars)
{
	std::vector<OhplGs5Str> vRows;
	for (const tString &s : avRows)
		vRows.push_back({U(s), s.size()});
	float vColor[4] = {aColor.r, aColor.g, aColor.b, aColor.a};
	ohpl_gs5_voice_add_subtitle(mpV, vRows.data(), vRows.size(), U(asName), asName.size(), vColor, alDrawnChars);
}

void cSomaLuxVoiceHandler::OnDraw(float afFrameTime)
{
	// cLuxFadeEffect::OnDraw: on the HUD set at z 0.9, so HUD ImGui (credits) draws over it
	float fFadeAlpha = GetFadeAlpha();
	if (fFadeAlpha > 0 && cSomaLuxMap::GetCurrent())
	{
		cGuiSet *pHud = SomaHudImGui()->GetSet();
		cVector2f vSize = pHud->GetVirtualSize();
		cVector2f vPos = pHud->GetVirtualSizeOffset() * -1.0f - vSize * 0.125f;
		pHud->DrawGfx(mpFadeGfx, cVector3f(vPos.x, vPos.y, 0.9f), vSize * 1.25f, cColor(0, fFadeAlpha));
	}
	if (mpFont == NULL || (gpSomaBase && gpSomaBase->GetConfig()->mbShowSubtitles == false))
	{
		ohpl_gs5_voice_drop_immediate(mpV);
		return;
	}
	cVector2f vScreen = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
	mpGuiSet->SetVirtualSize(vScreen, -1000, 1000, cVector2f(0));
	// game.cfg <Voice FontSize="26" TextY="700" MaxTextWidth="860"> on a 768 line canvas
	float fScale = vScreen.y / 768.0f;
	cVector2f vSize(26 * fScale, 26 * fScale);
	struct cDraw
	{
		cSomaLuxVoiceHandler *mpSelf;
		cVector2f mvScreen, mvSize;
		float mfScale;
		size_t mlNext;
	} draw = {this, vScreen, vSize, fScale, 0};
	mvRowCache.clear();
	ohpl_gs5_voice_subtitles(mpV, VHost(), vScreen.y, gpSomaBase->mbScriptGamePaused, +[](void *apCtx, const unsigned char *t, size_t l, float afAlpha, bool abColor,
																						const float *apColor, float afY, size_t alRows) {
		cDraw *d = static_cast<cDraw *>(apCtx);
		const tWStringVec &vRows = d->mpSelf->mvRowCache[d->mlNext++];
		cColor col = abColor ? cColor(apColor[0], apColor[1], apColor[2], apColor[3]) : cColor(1, afAlpha);
		for (size_t r = 0; r < vRows.size(); ++r)
		{
			cVector3f vPos(d->mvScreen.x * 0.5f, afY + d->mvSize.y * (float)r, 5);
			d->mpSelf->mpGuiSet->DrawFont(vRows[r], d->mpSelf->mpFont, vPos + cVector3f(d->mfScale, d->mfScale, -0.1f), d->mvSize, cColor(0, afAlpha),
										  eFontAlign_Center);
			d->mpSelf->mpGuiSet->DrawFont(vRows[r], d->mpSelf->mpFont, vPos, d->mvSize, col, eFontAlign_Center);
		}
	}, &draw);
}

bool cSomaLuxVoiceHandler::SubjectExists(const tString &asSubject)
{
	return ohpl_gs5_voice_subject_exists(mpV, U(asSubject), asSubject.size());
}

// .anno (Annosoft) decoding and the viseme sampling are in Rust; the animation states stay here
void cSomaLuxVoiceHandler::UpdateLipsync()
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	typedef std::array<float, 17> tVisemes;
	std::map<tString, tVisemes> mapWeights;
	ohpl_gs5_voice_lip_weights(mpV, VHost(), +[](void *c, const unsigned char *n, size_t l, const float *w) {
		tVisemes v;
		std::copy(w, w + 17, v.begin());
		(*static_cast<std::map<tString, tVisemes> *>(c))[tString((const char *)n, l)] = v;
	}, &mapWeights);
	std::set<tString> setAll = msetLipEntities;
	for (auto &it : mapWeights)
		setAll.insert(it.first);
	msetLipEntities.clear();
	for (const tString &sName : setAll)
	{
		cSomaLuxEntity *pEnt = pMap ? pMap->GetEntity(sName) : NULL;
		if (pEnt == NULL || pEnt->mpMesh == NULL)
			continue;
		auto itW = mapWeights.find(sName);
		for (int i = 0; i < 17; ++i)
		{
			cAnimationState *pState = pEnt->mpMesh->GetAnimationStateFromName("Visemes_17_" + cString::ToString(i + 1));
			if (pState == NULL)
				continue;
			if (itW == mapWeights.end())
			{
				pState->FadeOut(0.1f);
				continue;
			}
			pState->SetLayer(true);
			pState->SetActive(true);
			pState->SetPaused(true);
			pState->SetFadeStep(0);
			pState->SetTimePosition(0);
			pState->SetWeight(itW->second[i]);
		}
		if (itW != mapWeights.end())
			msetLipEntities.insert(sName);
	}
}

bool cSomaLuxVoiceHandler::LoadPcm(const tString &asFile)
{
	if (mPcm.msFile == asFile)
		return mPcm.mlChannels > 0;
	mPcm = cPcm();
	mPcm.msFile = asFile;
	OggVorbis_File vf;
	if (ov_fopen(cString::To8Char(mpEngine->GetResources()->GetFileSearcher()->GetFilePath(asFile)).c_str(), &vf) != 0)
		return false;
	mPcm.mlChannels = ov_info(&vf, -1)->channels;
	mPcm.mlRate = ov_info(&vf, -1)->rate;
	int lSection = 0;
	float **pBuf;
	for (long n; (n = ov_read_float(&vf, &pBuf, 4096, &lSection)) > 0;)
		for (long i = 0; i < n; ++i)
			for (int c = 0; c < mPcm.mlChannels; ++c)
				mPcm.mvData.push_back(pBuf[c][i]);
	ov_clear(&vf);
	return true;
}

// cLuxVoiceHandler::GetSpectrumFromScene -> FMOD Channel::getSpectrum
void cSomaLuxVoiceHandler::GetSpectrum(bool abByScene, const tString &asKey, std::vector<float> &avOut, int alNum)
{
	size_t lEntry = 0;
	int lId = 0;
	tString sFile;
	if (ohpl_gs5_voice_spectrum_target(mpV, abByScene, U(asKey), asKey.size(), &lEntry, &lId, SinkString, &sFile) == false)
		return;
	avOut.assign(alNum, 0.0f);
	if (lEntry == 0 || cVoiceHost::EntryValid(NULL, lEntry, lId) == false || LoadPcm(sFile) == false)
		return;
	ohpl_gs5_spectrum(mPcm.mvData.data(), mPcm.mvData.size(), mPcm.mlChannels, mPcm.mlRate, cVoiceHost::ChannelElapsed(NULL, lEntry), alNum, avOut.data());
}

void cSomaLuxVoiceHandler::GetSpectrumFromSpeakingCharacter(const tString &asName, std::vector<float> &avOut, int alNum)
{
	GetSpectrum(false, asName, avOut, alNum);
}

void cSomaLuxVoiceHandler::GetSpectrumFromScene(const tString &asScene, std::vector<float> &avOut, int alNum)
{
	GetSpectrum(true, asScene, avOut, alNum);
}

bool cSomaLuxVoiceHandler::SubjectIsPlaying(const tString &asName)
{
	return ohpl_gs5_voice_subject_is_playing(mpV, U(asName), asName.size());
}

float cSomaLuxVoiceHandler::GetTimeLeftOnLastLineOfPlayingSubject(const tString &asSubject)
{
	return ohpl_gs5_voice_time_left(mpV, VHost(), U(asSubject), asSubject.size());
}

bool cSomaLuxVoiceHandler::SceneIsActive(const tString &asScene)
{
	return ohpl_gs5_voice_scene_is_active(mpV, U(asScene), asScene.size());
}

bool cSomaLuxVoiceHandler::SceneInvolvingCharacterIsActive(const tString &asName)
{
	return ohpl_gs5_voice_scene_involving_character_is_active(mpV, U(asName), asName.size());
}

bool cSomaLuxVoiceHandler::SubjectInvolvesCharacter(const tString &asSubject, const tString &asName)
{
	return ohpl_gs5_voice_subject_involves(mpV, U(asSubject), asSubject.size(), U(asName), asName.size());
}

int cSomaLuxVoiceHandler::GetSubjectLineNumber(const tString &asSubject)
{
	return ohpl_gs5_voice_subject_line_count(mpV, U(asSubject), asSubject.size());
}

const tString &cSomaLuxVoiceHandler::GetSubjectSceneName(const tString &asSubject)
{
	msSceneCache.clear();
	ohpl_gs5_voice_subject_scene(mpV, U(asSubject), asSubject.size(), SinkString, &msSceneCache);
	return msSceneCache;
}

void cSomaLuxVoiceHandler::SetPaused(const tString &asScene, bool abX)
{
	ohpl_gs5_voice_set_paused(mpV, VHost(), U(asScene), asScene.size(), abX);
}

void cSomaLuxVoiceHandler::SetPausedAll(bool abX)
{
	ohpl_gs5_voice_set_paused_all(mpV, VHost(), abX);
}

// the game's ResumeAll unpauses every entry
void cSomaLuxVoiceHandler::ReapplyPauses()
{
	ohpl_gs5_voice_reapply_pauses(mpV, VHost());
}

cSomaLuxDialogHandler *cSomaLuxDialogHandler::Get()
{
	static cSomaLuxDialogHandler handler;
	return &handler;
}

void cSomaLuxDialogHandler::Begin(const tString &asName)
{
	mBuilding = cDialog();
	mBuilding.msName = asName;
	mBuilding.msCallback = msPendingCallback;
	msPendingCallback = "";
}

void cSomaLuxDialogHandler::AddBranch(const tString &asName, const tString &asNext)
{
	cBranch b;
	b.msName = asName;
	b.msNext = asNext;
	mBuilding.mvBranches.push_back(b);
}

void cSomaLuxDialogHandler::AddSubject(const tString &asSubject, const tString &asCallback)
{
	if (mBuilding.mvBranches.empty())
		AddBranch("", "");
	mBuilding.mvBranches.back().mvItems.push_back(cItem{asSubject, -1, asCallback});
}

void cSomaLuxDialogHandler::AddPause(float afTime, const tString &asCallback)
{
	if (mBuilding.mvBranches.empty())
		AddBranch("", "");
	mBuilding.mvBranches.back().mvItems.push_back(cItem{"", afTime, asCallback});
}

void cSomaLuxDialogHandler::AddResponseOption(const tString &asEntry, const tString &asBranch, int alId, const tString &asCallback)
{
	if (mBuilding.mvBranches.empty() == false)
		mBuilding.mvBranches.back().mvOptions.push_back(cOption{asEntry, asBranch, alId, asCallback});
}

// cLuxDialogHandler::AddBranchEvent: attaches to the last added subject
void cSomaLuxDialogHandler::AddBranchEvent(int alType, float afVar, const tString &asVar, const tString &asNewBranch, bool abOnlyEnd)
{
	if (mBuilding.mvBranches.empty() || mBuilding.mvBranches.back().mvItems.empty())
		return;
	mBuilding.mvBranches.back().mvItems.back().mvEvents.push_back(cEvent{alType, (int)lroundf(afVar), asVar, asNewBranch, abOnlyEnd});
}

cSomaLuxDialogHandler::cBranch *cSomaLuxDialogHandler::FindBranch(cDialog &aD, const tString &asName, int *apIdx)
{
	for (size_t i = 0; i < aD.mvBranches.size(); ++i)
		if (aD.mvBranches[i].msName == asName)
		{
			*apIdx = (int)i;
			return &aD.mvBranches[i];
		}
	return NULL;
}

void cSomaLuxDialogHandler::End(const tString &asStartBranch)
{
	std::shared_ptr<cDialog> pD(new cDialog(mBuilding));
	mBuilding = cDialog();
	if (pD->mvBranches.empty())
		return;
	pD->mlBranch = 0;
	if (asStartBranch != "")
		FindBranch(*pD, asStartBranch, &pD->mlBranch);
	pD->mlItem = -1;
	mvActive.push_back(pD);
	NextItem(*pD);
}

static void ItemCallback(const tString &asFunc, const tString &asSubject, bool abStart)
{
	if (asFunc == "")
		return;
	SomaMapScriptCall("void " + asFunc + "(const tString&in, bool)", [&](asIScriptContext *c) {
		c->SetArgObject(0, (void *)&asSubject);
		c->SetArgByte(1, abStart);
	});
}

void cSomaLuxDialogHandler::StartItem(cDialog &aD)
{
	cItem &item = aD.mvBranches[aD.mlBranch].mvItems[aD.mlItem];
	ItemCallback(item.msCallback, item.msSubject, true);
	if (item.mfPause >= 0)
	{
		aD.mfPauseLeft = item.mfPause;
		aD.mbWaiting = false;
		return;
	}
	aD.mbWaiting = true;
	std::weak_ptr<cDialog> wD;
	for (auto &p : mvActive)
		if (p.get() == &aD)
			wD = p;
	bool bOk = cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->Play(item.msSubject, -1, "", 100, [this, wD]() {
		if (auto p = wD.lock())
			p->mbWaiting = false;
	});
	if (bOk == false)
		aD.mbWaiting = false;
}

void cSomaLuxDialogHandler::NextItem(cDialog &aD)
{
	while (aD.mbDone == false)
	{
		cBranch &b = aD.mvBranches[aD.mlBranch];
		if (aD.mlItem >= 0 && aD.mlItem < (int)b.mvItems.size())
		{
			cItem &prev = b.mvItems[aD.mlItem];
			ItemCallback(prev.msCallback, prev.msSubject, false);
			if (CheckEndEvents(aD))
				continue;
		}
		++aD.mlItem;
		if (aD.mlItem < (int)b.mvItems.size())
		{
			StartItem(aD);
			return;
		}
		EndBranch(aD);
		if (aD.mbDone)
			return;
	}
}

void cSomaLuxDialogHandler::EndBranch(cDialog &aD)
{
	cBranch &b = aD.mvBranches[aD.mlBranch];
	tString sNext = b.msNext;
	if (b.mvOptions.empty() == false)
	{
		// No dialog UI yet: take the first option
		const cOption &opt = b.mvOptions[0];
		Log("SOMA dialog: auto-selected option '%s'\n", opt.msEntry.c_str());
		if (opt.msCallback != "")
			SomaMapScriptCall("void " + opt.msCallback + "(const tString&in, const tString&in, int)", [&](asIScriptContext *c) {
				c->SetArgObject(0, &b.msName);
				c->SetArgObject(1, (void *)&opt.msEntry);
				c->SetArgDWord(2, opt.mlId);
			});
		sNext = opt.msBranch;
	}
	int lIdx = -1;
	if (sNext != "" && FindBranch(aD, sNext, &lIdx))
	{
		aD.mlBranch = lIdx;
		aD.mlItem = -1;
		return;
	}
	Finish(aD);
}

void cSomaLuxDialogHandler::Finish(cDialog &aD)
{
	aD.mbDone = true;
	if (aD.msCallback != "")
		SomaMapScriptCall("void " + aD.msCallback + "(const tString&in)", [&](asIScriptContext *c) { c->SetArgObject(0, &aD.msName); });
}

// cLuxDialogInstance::CheckBranchEvent; OutOfRange/PlayerNotLooking are unused by the game scripts
bool cSomaLuxDialogHandler::CheckEvent(cDialog &aD, const cItem &aItem, const cEvent &aE)
{
	int lVar = mmapVars.count(aE.msVar) ? mmapVars[aE.msVar] : 0;
	switch (aE.mlType)
	{
	case 2: mmapVars[aE.msVar] = aE.mlVal; return false;
	case 3: mmapVars[aE.msVar] += aE.mlVal; return false;
	case 4: return lVar == aE.mlVal;
	case 5: return lVar < aE.mlVal;
	case 6: return lVar > aE.mlVal;
	case 7:
	{
		bool bRet = false;
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		if (aE.msVar == "" || pMap == NULL || pMap->GetScript() == NULL)
			return false;
		const tString &sBranch = aD.mvBranches[aD.mlBranch].msName;
		int lLine = cSomaLuxVoiceHandler::Get() ? cSomaLuxVoiceHandler::Get()->GetSubjectLineNumber(aItem.msSubject) - 1 : -1;
		cSomaScriptRuntime::Get()->Call(
			pMap->GetScript(), "bool " + aE.msVar + "(const tString&in, const tString&in, int, const tString&in)",
			[&](asIScriptContext *c) {
				c->SetArgObject(0, (void *)&sBranch);
				c->SetArgObject(1, (void *)&aItem.msSubject);
				c->SetArgDWord(2, lLine);
				c->SetArgObject(3, (void *)&aE.msNewBranch);
			},
			[&](asIScriptContext *c) { bRet = c->GetReturnByte() != 0; });
		return bRet;
	}
	}
	return false;
}

// cLuxDialogInstance::CheckEvents(true): a firing event jumps to its branch, or ends the dialog
bool cSomaLuxDialogHandler::CheckEndEvents(cDialog &aD)
{
	const cItem item = aD.mvBranches[aD.mlBranch].mvItems[aD.mlItem];
	for (const cEvent &e : item.mvEvents)
	{
		if (e.mbOnlyEnd == false || CheckEvent(aD, item, e) == false)
			continue;
		int lIdx = -1;
		if (e.msNewBranch != "" && FindBranch(aD, e.msNewBranch, &lIdx))
		{
			aD.mlBranch = lIdx;
			aD.mlItem = -1;
		}
		else
			Finish(aD);
		return true;
	}
	return false;
}

void cSomaLuxDialogHandler::Update(float afTimeStep)
{
	std::vector<std::shared_ptr<cDialog>> vActive = mvActive;
	for (auto &p : vActive)
	{
		if (p->mbDone)
			continue;
		if (p->mfPauseLeft > 0)
		{
			p->mfPauseLeft -= afTimeStep;
			if (p->mfPauseLeft <= 0)
				NextItem(*p);
		}
		else if (p->mbWaiting == false)
			NextItem(*p);
	}
	for (size_t i = 0; i < mvActive.size();)
		if (mvActive[i]->mbDone)
			mvActive.erase(mvActive.begin() + i);
		else
			++i;
}

void cSomaLuxDialogHandler::Stop(const tString &asName)
{
	for (auto &p : mvActive)
		if (p->msName == asName)
			p->mbDone = true;
}

void cSomaLuxDialogHandler::StopAll()
{
	for (auto &p : mvActive)
		p->mbDone = true;
	if (cSomaLuxVoiceHandler::Get())
		cSomaLuxVoiceHandler::Get()->StopAll();
}

bool cSomaLuxDialogHandler::CharacterIsActive(const tString &asName)
{
	cSomaLuxVoiceHandler *pVoice = cSomaLuxVoiceHandler::Get();
	if (pVoice == NULL)
		return false;
	if (pVoice->SceneInvolvingCharacterIsActive(asName))
		return true;
	// Pauses between subjects still count
	for (auto &p : mvActive)
		if (p->mbDone == false)
			for (const cBranch &b : p->mvBranches)
				for (const cItem &item : b.mvItems)
					if (item.msSubject != "" && pVoice->SubjectInvolvesCharacter(item.msSubject, asName))
						return true;
	return false;
}

// Natives: both handlers are singletons, any non-null pointer identifies them

static int gVoiceTag, gDialogTag;

void SomaRegisterFadeNatives(asIScriptEngine *e)
{
	const char *T = "cLuxEffectHandler";
	SOMA_METHOD(e, T, "void FadeIn(float afTime)", +[](void *, float t) { if (cSomaLuxVoiceHandler::Get()) cSomaLuxVoiceHandler::Get()->FadeTo(0, t); });
	SOMA_METHOD(e, T, "void FadeOut(float afTime)", +[](void *, float t) { if (cSomaLuxVoiceHandler::Get()) cSomaLuxVoiceHandler::Get()->FadeTo(1, t); });
	SOMA_METHOD(e, T, "bool IsFading()", +[](void *) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->IsFading(); });
	SOMA_METHOD(e, T, "float GetFadeAlpha()", +[](void *) { return cSomaLuxVoiceHandler::Get() ? cSomaLuxVoiceHandler::Get()->GetFadeAlpha() : 0.0f; });
}

void cSomaLuxVoiceHandler::RegisterNatives(asIScriptEngine *e)
{
	typedef const tString &S;
	auto V = []() { return cSomaLuxVoiceHandler::Get(); };
	(void)V;
	SOMA_FUNC(e, "cLuxVoiceHandler@ cLux_GetVoiceHandler()", +[]() { return (void *)&gVoiceTag; });
	SOMA_FUNC(e, "cLuxDialogHandler@ cLux_GetDialogHandler()", +[]() { return (void *)&gDialogTag; });
	SomaRegisterFadeNatives(e);
	const char *T = "cLuxVoiceHandler";
#define VH if (cSomaLuxVoiceHandler::Get()) cSomaLuxVoiceHandler::Get()
	SOMA_METHOD(e, T, "bool Play(const tString &in asSubject, int alSpecificLine, const tString &in asCallback, int alPrio)",
				+[](void *, S s, int l, S cb, int prio) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->Play(s, l, cb, prio); });
	// Rebirth
	SOMA_METHOD(e, T, "bool Play(const tString &in asSubject, int alSpecificLine, const tString &in asCallback, int alPrio, bool abPlayWhilePaused)",
				+[](void *, S s, int l, S cb, int prio, bool pwp) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->Play(s, l, cb, prio, {}, pwp); });
	SOMA_METHOD(e, T, "void AddSubtitle(const array<tWString> &in avText, const tWString &in asCharacterName, const cColor &in aColor, int alDrawnChars, bool abVoiceSoundMissing, bool abIsPlayer = false, bool abPlayWhilePaused = false, bool abPositional = false, const cVector3f &in avPosition = 0)",
				+[](void *, CScriptArray &rows, const tWString &name, const cColor &c, int drawn, bool, bool, bool, bool, const cVector3f &) {
					std::vector<tString> v;
					for (asUINT i = 0; i < rows.GetSize(); ++i)
						v.push_back(cString::To8Char(*(const tWString *)rows.At(i)));
					VH->AddSubtitle(v, cString::To8Char(name), c, drawn);
				});
	SOMA_METHOD(e, T, "void AdvanceFromCurrentSound(const tString&in asScene)", +[](void *, S s) { VH->SkipCurrentLine(s); });
	SOMA_METHOD(e, T, "float GetTimeLeftOnLastLineOfPlayingSubject(const tString&in asName)",
				+[](void *, S s) { return cSomaLuxVoiceHandler::Get() ? cSomaLuxVoiceHandler::Get()->GetTimeLeftOnLastLineOfPlayingSubject(s) : -1.0f; });
	SOMA_METHOD(e, T, "int GetSubjectLineNumber(const tString &in asSubject)",
				+[](void *, S s) { return cSomaLuxVoiceHandler::Get() ? cSomaLuxVoiceHandler::Get()->GetSubjectLineNumber(s) : 0; });
	SOMA_METHOD(e, T, "const tString& GetSubjectSceneName(const tString &in asSubject)", +[](void *, S s) -> const tString & {
		static tString sEmpty;
		return cSomaLuxVoiceHandler::Get() ? cSomaLuxVoiceHandler::Get()->GetSubjectSceneName(s) : sEmpty;
	});
	SOMA_METHOD(e, T, "bool SubjectExists(const tString &in asSubject)", +[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->SubjectExists(s); });
	SOMA_METHOD(e, T, "void StopAll()", +[](void *) { VH->StopAll(); });
	SOMA_METHOD(e, T, "void Stop(const tString&in asScene)", +[](void *, S s) { VH->Stop(s); });
	SOMA_METHOD(e, T, "void SkipCurrentLine(const tString&in asScene)", +[](void *, S s) { VH->SkipCurrentLine(s); });
	SOMA_METHOD(e, T, "void SkipCurrentSound(const tString&in asScene)", +[](void *, S s) { VH->SkipCurrentLine(s); });
	SOMA_METHOD(e, T, "void SetPaused(const tString&in asScene, bool abX)", +[](void *, S s, bool b) { VH->SetPaused(s, b); });
	SOMA_METHOD(e, T, "void SetPausedAll(bool abX)", +[](void *, bool b) { VH->SetPausedAll(b); });
	SOMA_METHOD(e, T, "bool CharacterIsSpeaking(const tString&in asName)", +[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->CharacterIsSpeaking(s); });
	SOMA_METHOD(e, T, "void AddCharacterSpeakingCallback(const tString&in asCharacter, const tString&in asCallback)", +[](void *, S c, S f) { VH->SetSpeakingCallback(c, f); });
	SOMA_METHOD(e, T, "void RemoveCharacterSpeakingCallback(const tString&in asCharacter)", +[](void *, S c) { VH->SetSpeakingCallback(c, ""); });
	SOMA_METHOD(e, T, "void GetSpectrumFromSpeakingCharacter(const tString&in asCharacter, array<float>&out aDestArray, int alNumSamples=64)",
				+[](void *, S s, CScriptArray &arr, int n) {
					std::vector<float> v;
					VH->GetSpectrumFromSpeakingCharacter(s, v, n);
					arr.Resize((asUINT)v.size());
					for (asUINT i = 0; i < v.size(); ++i)
						*(float *)arr.At(i) = v[i];
				});
	SOMA_METHOD(e, T, "void GetSpectrumFromScene(const tString&in asScene, array<float>&out aDestArray, int alNumSamples=64)",
				+[](void *, S s, CScriptArray &arr, int n) {
					std::vector<float> v;
					VH->GetSpectrumFromScene(s, v, n);
					arr.Resize((asUINT)v.size());
					for (asUINT i = 0; i < v.size(); ++i)
						*(float *)arr.At(i) = v[i];
				});
	SOMA_METHOD(e, T, "bool SubjectIsPlaying(const tString&in asName)", +[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->SubjectIsPlaying(s); });
	SOMA_METHOD(e, T, "bool SceneIsActive(const tString&in asScene)", +[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->SceneIsActive(s); });
	SOMA_METHOD(e, T, "bool SceneInvolvingCharacterIsActive(const tString&in asCharacter)",
				+[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->SceneInvolvingCharacterIsActive(s); });
	SOMA_METHOD(e, T, "void FadeSceneVolumeTo(const tString&in asScene, float afVolume, float afTime)", +[](void *, S s, float v, float t) { VH->FadeSceneVolumeTo(s, v, t); });
	SOMA_METHOD(e, T, "void SetFocusScene(const tString&in asScene)", +[](void *, S s) { VH->SetFocusScene(s); });
	SOMA_METHOD(e, "cLuxMap",
				"void SetVoiceSource(const tString &in asCharacter, const tString &in asEntityName, float afMinDistance, float afMaxDistance, bool abUse3D, float afMaxPlayerListeningRange, float afMinFreq = 22000, float afMaxFreq = 22000, uint aFrequencyFlags = 0)",
				+[](void *, S c, S ent, float fMin, float fMax, bool b3D, float fRange, float fMinF, float fMaxF, asUINT lFlags) { VH->SetSource(c, ent, fMin, fMax, b3D, fRange, fMinF, fMaxF, (int)lFlags); });
	SOMA_METHOD(e, T, "bool AnySceneIsActive()", +[](void *) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->AnySceneIsActive(); });
#undef VH
}

void cSomaLuxDialogHandler::RegisterNatives(asIScriptEngine *e)
{
	typedef const tString &S;
	typedef cSomaLuxDialogHandler D;
	const char *T = "cLuxDialogHandler";
	SOMA_METHOD(e, T, "void Begin(const tString &in asName)", +[](void *, S s) { D::Get()->Begin(s); });
	SOMA_METHOD(e, T, "void End(const tString&in asStartBranch)", +[](void *, S s) { D::Get()->End(s); });
	SOMA_METHOD(e, T, "void SetCallbackFunc(const tString&in asFunc)", +[](void *, S s) { D::Get()->SetCallbackFunc(s); });
	SOMA_METHOD(e, T, "void AddBranch(const tString&in asName, const tString&in asNextBranch)", +[](void *, S n, S next) { D::Get()->AddBranch(n, next); });
	SOMA_METHOD(e, T, "void AddBranchSubject(const tString&in asSubject, const tString&in asCallback)", +[](void *, S s, S cb) { D::Get()->AddSubject(s, cb); });
	SOMA_METHOD(e, T, "void AddBranchPause(float afTime, const tString&in asCallback)", +[](void *, float t, S cb) { D::Get()->AddPause(t, cb); });
	SOMA_METHOD(e, T, "void AddResponseOption(const tString &in asEntry, const tString&in asBranch,int alId, const tString&in asCallback)",
				+[](void *, S entry, S branch, int id, S cb) { D::Get()->AddResponseOption(entry, branch, id, cb); });
	SOMA_METHOD(e, T, "void AddBranchEvent(eLuxDialogBranchEvent aType, float afVar, const tString&in asVar, const tString&in asNewBranch, bool abOnlyCheckEndOfSubject)",
				+[](void *, int t, float f, S var, S branch, bool b) { D::Get()->AddBranchEvent(t, f, var, branch, b); });
	SOMA_METHOD(e, T, "bool CharacterIsActive(const tString&in asName)", +[](void *, S s) { return D::Get()->CharacterIsActive(s); });
	SOMA_METHOD(e, T, "void Stop(const tString &in asName)", +[](void *, S s) { D::Get()->Stop(s); });
	SOMA_METHOD(e, T, "void StopAll()", +[](void *) { D::Get()->StopAll(); });
	SOMA_METHOD(e, T, "void SetVar(const tString&in asName, int alX)", +[](void *, S s, int x) { D::Get()->mmapVars[s] = x; });
	SOMA_METHOD(e, T, "void IncVar(const tString&in asName, int alX)", +[](void *, S s, int x) { D::Get()->mmapVars[s] += x; });
	SOMA_METHOD(e, T, "int GetVar(const tString&in asName)", +[](void *, S s) { return D::Get()->mmapVars[s]; });
}
