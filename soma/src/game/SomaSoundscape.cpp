#include "SomaSoundscape.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxPlayer.h"
#include "SomaScriptBind.h"
#include "SomaSound.h"

#include "impl/OpenALSoundEnvironment.h"

#include <algorithm>
#include <cstring>

namespace
{
char gHandlerTag;

const unsigned char *U(const tString &s) { return (const unsigned char *)s.data(); }

void SinkString(void *apCtx, const unsigned char *apData, size_t alLen) { static_cast<tString *>(apCtx)->assign((const char *)apData, alLen); }

cSomaSoundInstance *Bg(size_t e) { return (cSomaSoundInstance *)e; }

struct cScapeHost
{
	static bool BgLive(void *, size_t e, int id) { return cSomaSoundEvents::Get()->IsLive(Bg(e), id); }
	static void BgFadeOut(void *, size_t e, float f) { Bg(e)->FadeOut(f); }
	static void BgFadeVolumeMulTo(void *, size_t e, float d, float s) { Bg(e)->FadeVolumeMulTo(d, s); }
	static bool BgPlay(void *, const unsigned char *n, size_t l, size_t *pe, int *pid)
	{
		cSomaSoundInstance *p = cSomaSoundEvents::Get()->PlayGui(tString((const char *)n, l), 1.0f, eSoundEntryType_World, true);
		if (p == NULL)
			return false;
		*pe = (size_t)p;
		*pid = p->GetId();
		return true;
	}
	static void BgSetVolumeMul(void *, size_t e, float f) { Bg(e)->SetVolumeMul(f); }
	static void BgFadeIn(void *, size_t e, float s) { Bg(e)->FadeInTo(1, s); }
	static void BgSetPaused(void *, size_t e, bool b) { Bg(e)->SetPaused(b); }
	static bool WorldPresent(void *) { return cSomaSoundscape::Get()->GetWorld() != NULL; }
	static void EntityFade(void *, const unsigned char *n, size_t l, bool abIn, float afSpeed)
	{
		cSoundEntity *pSound = cSomaSoundscape::Get()->GetWorld()->GetSoundEntity(tString((const char *)n, l));
		if (pSound == NULL)
			return;
		if (abIn)
			pSound->FadeIn(afSpeed);
		else
			pSound->FadeOut(afSpeed);
	}
	static void Area(void *, size_t e, bool *pActive, float *o)
	{
		cSomaLuxEntity *p = (cSomaLuxEntity *)e;
		*pActive = p->mbActive;
		cMatrixf m = p->GetMatrix();
		for (int r = 0; r < 4; ++r)
			for (int c = 0; c < 4; ++c)
				o[r * 4 + c] = m.m[r][c];
		o[16] = p->mvSize.x;
		o[17] = p->mvSize.y;
		o[18] = p->mvSize.z;
	}
	static bool CameraPos(void *, float *o)
	{
		cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
		cCamera *pCam = pPlayer ? pPlayer->GetCamera() : NULL;
		if (pCam == NULL)
			return false;
		cVector3f v = pCam->GetPosition();
		o[0] = v.x; o[1] = v.y; o[2] = v.z;
		return true;
	}
	static bool BodyPos(void *, float *o)
	{
		cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
		iCharacterBody *pBody = pPlayer ? pPlayer->GetCharacterBody() : NULL;
		if (pBody == NULL)
			return false;
		cVector3f v = pBody->GetPosition();
		o[0] = v.x; o[1] = v.y; o[2] = v.z;
		return true;
	}
	// FMOD Ex reverb properties to EFX EAX reverb
	static void ApplyReverb(void *, const float *p)
	{
		static cOpenALSoundEnvironment env;
		env.SetGain(p[0]);
		env.SetGainHF(p[1]);
		env.SetGainLF(p[2]);
		env.SetDecayTime(p[3]);
		env.SetDecayHFRatio(p[4]);
		env.SetDecayLFRatio(p[5]);
		env.SetReflectionsGain(p[6]);
		env.SetReflectionsDelay(p[7]);
		env.SetLateReverbGain(p[8]);
		env.SetLateReverbDelay(p[9]);
		env.SetModulationTime(p[10]);
		env.SetModulationDepth(p[11]);
		env.SetHFReference(p[12]);
		env.SetLFReference(p[13]);
		env.SetDiffusion(p[14]);
		env.SetDensity(p[15]);
		env.SetEchoTime(0.25f);
		env.SetEchoDepth(0);
		env.SetAirAbsorptionGainHF(0.994f);
		env.SetRoomRolloffFactor(0);
		env.SetDecayHFLimit(0);
		gpSomaBase->mpEngine->GetSound()->GetLowLevel()->SetSoundEnvironment(&env);
	}
};

const OhplGs5ScapeHost *SHost()
{
	static const OhplGs5ScapeHost host = {NULL, cScapeHost::BgLive, cScapeHost::BgFadeOut, cScapeHost::BgFadeVolumeMulTo, cScapeHost::BgPlay,
										  cScapeHost::BgSetVolumeMul, cScapeHost::BgFadeIn, cScapeHost::BgSetPaused, cScapeHost::WorldPresent,
										  cScapeHost::EntityFade, cScapeHost::Area, cScapeHost::CameraPos, cScapeHost::BodyPos, cScapeHost::ApplyReverb};
	return &host;
}
} // namespace

cSomaSoundscape::cSomaSoundscape() : mpScape(ohpl_gs5_scape_new()) {}

cSomaSoundscape *cSomaSoundscape::Get()
{
	static cSomaSoundscape gInstance;
	return &gInstance;
}

void cSomaSoundscape::Load(cSomaLuxMap *apMap)
{
	mpMap = apMap;
	mpWorld = apMap ? apMap->GetWorld() : NULL;
	std::vector<OhplGs5AreaDef> vDefs;
	struct cStrings
	{
		tString msLevel, msBG, msIn, msOut, msPrefix, msReverb;
	};
	std::vector<cStrings> vStrings;
	if (apMap)
	{
		for (cSomaLuxEntity *pEnt : apMap->GetEntities())
		{
			if (pEnt->meType != eSomaLuxEntityType_Area || pEnt->msClassName != "Soundscape")
				continue;
			cResourceVarsObject &v = pEnt->mInstanceVars;
			vStrings.push_back({v.GetVarString("Level", ""), v.GetVarString("BG_Sound", ""), v.GetVarString("SoundEnt_InNames", ""),
								v.GetVarString("SoundEnt_OutNames", ""), v.GetVarString("SoundPrefix", ""), v.GetVarString("ReverbType", "Off")});
			const cStrings &s = vStrings.back();
			OhplGs5AreaDef d;
			d.ent = (size_t)pEnt;
			d.level = {U(s.msLevel), s.msLevel.size()};
			d.prio = v.GetVarInt("Prio", 0);
			d.bg = {U(s.msBG), s.msBG.size()};
			d.bg_volume = v.GetVarFloat("BG_Volume", 1);
			d.bg_fade_in = v.GetVarFloat("BG_FadeInTime", 1);
			d.bg_trans_speed = v.GetVarFloat("BG_FadeTransitionSpeed", 1);
			d.parent_vol_mul = v.GetVarFloat("BG_ParentVolMul", 1);
			d.in_names = {U(s.msIn), s.msIn.size()};
			d.out_names = {U(s.msOut), s.msOut.size()};
			d.ent_fade_in = v.GetVarFloat("SoundEnt_FadeInTime", 1);
			d.ent_fade_out = v.GetVarFloat("SoundEnt_FadeOutTime", 1);
			d.prefix = {U(s.msPrefix), s.msPrefix.size()};
			d.prefix_prio = v.GetVarInt("SoundPrefixPrio", 0);
			d.use_reverb = v.GetVarBool("UseReverb", false);
			d.reverb_prio = v.GetVarInt("ReverbPrio", 0);
			d.reverb_type = {U(s.msReverb), s.msReverb.size()};
			d.reverb_amount = v.GetVarFloat("ReverbAmount", 1);
			d.reverb_fade = v.GetVarFloat("ReverbFadeTime", 1);
			vDefs.push_back(d);
		}
	}
	// pointers into vStrings must not move while the defs are read
	for (size_t i = 0; i < vDefs.size(); ++i)
	{
		const cStrings &s = vStrings[i];
		vDefs[i].level = {U(s.msLevel), s.msLevel.size()};
		vDefs[i].bg = {U(s.msBG), s.msBG.size()};
		vDefs[i].in_names = {U(s.msIn), s.msIn.size()};
		vDefs[i].out_names = {U(s.msOut), s.msOut.size()};
		vDefs[i].prefix = {U(s.msPrefix), s.msPrefix.size()};
		vDefs[i].reverb_type = {U(s.msReverb), s.msReverb.size()};
	}
	ohpl_gs5_scape_load(mpScape, vDefs.data(), vDefs.size());
	if (apMap)
		Log("SOMA soundscape: %d areas\n", (int)vDefs.size());
}

void cSomaSoundscape::Update(cSomaLuxMap *apMap, float afTimeStep)
{
	if (apMap != mpMap)
		Load(apMap);
	ohpl_gs5_scape_update(mpScape, SHost(), afTimeStep);
	size_t lLen = 0;
	const unsigned char *pPrefix = ohpl_gs5_scape_prefix(mpScape, &lLen);
	msPrefix.assign((const char *)pPrefix, lLen);
}

void cSomaSoundscape::SetDefaultReverb(int alPreset, float afFadeTime)
{
	ohpl_gs5_scape_set_default_reverb(mpScape, alPreset, afFadeTime);
}

void cSomaSoundscape::SetPaused(bool abX)
{
	ohpl_gs5_scape_set_paused(mpScape, SHost(), abX);
}

tString cSomaSoundscape::Describe()
{
	OhplGs5ScapeInfo info;
	ohpl_gs5_scape_info(mpScape, &info);
	size_t lLen = 0;
	tString s = "prefix=" + msPrefix + " reverb=";
	if (info.rev_type >= 0)
	{
		const unsigned char *p = ohpl_gs5_reverb_name(info.rev_type, &lLen);
		s += tString((const char *)p, lLen);
	}
	else
		s += "-";
	s += "*" + cString::ToString(info.rev_amount);
	if (info.rev_has_area)
		s += "(" + ((cSomaLuxEntity *)info.rev_area_ent)->msName + ")";
	s += " room=" + cString::ToString(info.rev_room);
	for (int i = 0; i < 4; ++i)
	{
		size_t lEnt = 0;
		bool bArea = false, bBg = false;
		tString sBg;
		ohpl_gs5_scape_level(mpScape, i, &lEnt, &bArea, &bBg, SinkString, &sBg);
		if (bArea || bBg)
			s += " L" + cString::ToString(i) + "=" + (bArea ? ((cSomaLuxEntity *)lEnt)->msName : tString("-")) + ":" + sBg;
	}
	return s;
}

void cSomaSoundscape::RegisterNatives(asIScriptEngine *e)
{
	SOMA_FUNC(e, "cLuxSoundscapeHandler@ cLux_GetSoundscapeHandler()", +[]() { return (void *)&gHandlerTag; });
	SOMA_FUNC(e, "void cLux_SetupDefaultGlobalReverb(eSoundReverbPreset aType, float afFadeTime)", +[](int alType, float afFade) { Get()->SetDefaultReverb(alType, afFade); });
	SOMA_METHOD(e, "cLuxSoundscapeHandler", "const tString& GetCurrentSoundPrefix()", +[](void *) -> const tString & { return Get()->GetPrefix(); });
}
