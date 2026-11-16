#include "SomaSound.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxPlayer.h"
#include "SomaLuxVoice.h"
#include "SomaScriptRuntime.h"

#include "impl/OpenALSoundEnvironment.h"

namespace
{
cSoundHandler *Handler() { return gpSomaBase->mpEngine->GetSound()->GetSoundHandler(); }
cMusicHandler *Music() { return gpSomaBase->mpEngine->GetSound()->GetMusicHandler(); }
cSoundEntity *Ent(size_t e) { return (cSoundEntity *)e; }
cSomaSoundInstance *Shell(size_t e) { return (cSomaSoundInstance *)e; }
tString Str(const unsigned char *p, size_t n) { return tString((const char *)p, n); }

class cSomaSoundUpdater : public iUpdateable
{
public:
	cSomaSoundUpdater() : iUpdateable("SomaSoundEvents") {}
	void Update(float afTimeStep) { ohpl_s7_update(afTimeStep, gpSomaBase->mbScriptGamePaused); }
	void OnPauseUpdate(float afTimeStep) { ohpl_s7_pause_update(afTimeStep, gpSomaBase->mbScriptGamePaused); }
};

bool Valid(size_t e, int id) { return Handler()->IsValid((cSoundEntry *)e, id); }

bool Play(const OhplGs5PlayReq *r, size_t *pEntry, int *pId)
{
	tString sFile((const char *)r->file, r->file_len);
	cVector3f vPos(r->pos[0], r->pos[1], r->pos[2]);
	cSoundEntry *p;
	if (r->kind == 0)
		p = Handler()->Play(sFile, r->looped, 0, vPos, 1e5f, 2e5f, (eSoundEntryType)r->entry_type, false, true, 0, r->stream);
	else if (r->kind == 1)
		p = Handler()->PlayGuiStream(sFile, r->looped, 0, vPos, (eSoundEntryType)r->entry_type);
	else
		p = Handler()->PlayGui(sFile, r->looped, 0, vPos, (eSoundEntryType)r->entry_type);
	if (p == NULL)
		return false;
	*pEntry = (size_t)p;
	*pId = p->GetId();
	return true;
}

void SetFilter(size_t e, int which, float f)
{
	iSoundChannel *pCh = ((cSoundEntry *)e)->GetChannel();
	if (which == 0)
		pCh->SetFilterGainHF(f);
	else if (which == 1)
		pCh->SetFilterGainLF(f);
	else
		pCh->SetEnvSendGain(f);
}

void Listener(float *o)
{
	iLowLevelSound *pLow = gpSomaBase->mpEngine->GetSound()->GetLowLevel();
	cVector3f p = pLow->GetListenerPosition(), f = pLow->GetListenerForward(), u = pLow->GetListenerUp();
	o[0] = p.x; o[1] = p.y; o[2] = p.z; o[3] = f.x; o[4] = f.y; o[5] = f.z; o[6] = u.x; o[7] = u.y; o[8] = u.z;
}

void EntityInfo(size_t e, float *o)
{
	cSoundEntity *p = Ent(e);
	cVector3f v = p->GetWorldPosition();
	o[0] = v.x; o[1] = v.y; o[2] = v.z;
	o[3] = p->GetMinDistance();
	o[4] = p->GetMaxDistance();
	o[5] = p->GetVolume();
}

bool FindFile(const unsigned char *n, size_t l, void (*sink)(void *, const unsigned char *, size_t), void *ctx)
{
	tString s = cString::To8Char(gpSomaBase->mpEngine->GetResources()->GetFileSearcher()->GetFilePath(Str(n, l)));
	sink(ctx, (const unsigned char *)s.data(), s.size());
	return s.empty() == false;
}

size_t DataNew(const unsigned char *n, size_t l)
{
	cResources *pRes = gpSomaBase->mpEngine->GetResources();
	return (size_t)hplNew(cSoundEntityData, (Str(n, l), pRes, gpSomaBase->mpEngine->GetSound()));
}

void DataSetup(size_t d, float afMin, float afMax, bool abLoop, bool ab3D)
{
	cSoundEntityData *p = (cSoundEntityData *)d;
	p->SetVolume(1);
	p->SetMinDistance(afMin);
	p->SetMaxDistance(afMax);
	p->SetLoop(abLoop);
	p->SetUse3D(ab3D);
	p->SetStream(false);
	p->SetFadeStart(false);
	p->SetFadeStop(false);
}

size_t ShellNew(size_t alEvent, size_t alEntity, int alType)
{
	tString sName;
	ohpl_s7_event_name(alEvent, [](void *c, const unsigned char *p, size_t n) { static_cast<tString *>(c)->assign((const char *)p, n); }, &sName);
	return (size_t)hplNew(cSomaSoundInstance, (alEvent, sName, (cSoundEntity *)alEntity, (eSoundEntryType)alType));
}

const void *ShellString(size_t e, unsigned which, const unsigned char *p, size_t n)
{
	static tString sEmpty;
	if (which == 0)
		return &Shell(e)->msName;
	if (which == 1)
	{
		static tString sParam;
		sParam.assign((const char *)p, n);
		return &sParam;
	}
	return &sEmpty;
}

const void *ShellPositionRef(size_t e)
{
	static cVector3f vZero(0);
	return e ? (const void *)&Shell(e)->mvPos : (const void *)&vZero;
}

void ShellSetPosition(size_t e, const float *v) { Shell(e)->mvPos = cVector3f(v[0], v[1], v[2]); }

size_t WorldCreateSound(size_t w, const unsigned char *n, size_t nl, const unsigned char *f, size_t fl, bool abRemove)
{
	return (size_t)((cWorld *)w)->CreateSoundEntity(Str(n, nl), Str(f, fl), abRemove);
}

size_t WorldSoundByCreation(size_t w, int alID)
{
	cSoundEntityIterator it = ((cWorld *)w)->GetSoundEntityIterator();
	while (it.HasNext())
	{
		cSoundEntity *p = it.Next();
		if (p->GetCreationID() == alID)
			return (size_t)p;
	}
	return 0;
}

void ObjectId(size_t p, unsigned char *o)
{
	cSomaID id = SomaObjectID((void *)p, "cSoundEntity");
	memcpy(o, &id, sizeof(id));
}

void SeFadeMain(size_t e, float afVol, float afSpeed)
{
	cSoundEntity *o = Ent(e);
	o->Play(false);
	if (cSoundEntry *pEntry = o->GetSoundEntry(eSoundEntityType_Main, false))
	{
		if (afSpeed > 0)
			pEntry->FadeIn(afVol, afSpeed);
		else
			pEntry->SetVolumeMul(afVol);
	}
}

float SeProp(size_t e, unsigned op, float f)
{
	cSoundEntity *o = Ent(e);
	switch (op)
	{
	case 0: o->SetMinDistance(f); break;
	case 1: o->SetMaxDistance(f); break;
	case 2: return o->GetMinDistance();
	case 3: return o->GetMaxDistance();
	}
	return 0;
}

void SeResetProps(size_t e)
{
	cSoundEntity *o = Ent(e);
	if (o->GetData())
	{
		o->SetMinDistance(o->GetData()->GetMinDistance());
		o->SetMaxDistance(o->GetData()->GetMaxDistance());
		o->SetVolume(o->GetData()->GetVolume());
	}
}

size_t ScapeWorld(size_t m) { return (size_t)((cSomaLuxMap *)m)->GetWorld(); }

void ScapeAreas(size_t m, void (*sink)(void *, const OhplGs5AreaDef *), void *ctx)
{
	for (cSomaLuxEntity *pEnt : ((cSomaLuxMap *)m)->GetEntities())
	{
		if (pEnt->meType != eSomaLuxEntityType_Area || pEnt->msClassName != "Soundscape")
			continue;
		cResourceVarsObject &v = pEnt->mInstanceVars;
		tString sLevel = v.GetVarString("Level", ""), sBg = v.GetVarString("BG_Sound", ""), sIn = v.GetVarString("SoundEnt_InNames", ""),
				sOut = v.GetVarString("SoundEnt_OutNames", ""), sPrefix = v.GetVarString("SoundPrefix", ""), sReverb = v.GetVarString("ReverbType", "Off");
		auto S = [](const tString &s) { return OhplGs5Str{(const unsigned char *)s.data(), s.size()}; };
		OhplGs5AreaDef d;
		d.ent = (size_t)pEnt;
		d.level = S(sLevel);
		d.prio = v.GetVarInt("Prio", 0);
		d.bg = S(sBg);
		d.bg_volume = v.GetVarFloat("BG_Volume", 1);
		d.bg_fade_in = v.GetVarFloat("BG_FadeInTime", 1);
		d.bg_trans_speed = v.GetVarFloat("BG_FadeTransitionSpeed", 1);
		d.parent_vol_mul = v.GetVarFloat("BG_ParentVolMul", 1);
		d.in_names = S(sIn);
		d.out_names = S(sOut);
		d.ent_fade_in = v.GetVarFloat("SoundEnt_FadeInTime", 1);
		d.ent_fade_out = v.GetVarFloat("SoundEnt_FadeOutTime", 1);
		d.prefix = S(sPrefix);
		d.prefix_prio = v.GetVarInt("SoundPrefixPrio", 0);
		d.use_reverb = v.GetVarBool("UseReverb", false);
		d.reverb_prio = v.GetVarInt("ReverbPrio", 0);
		d.reverb_type = S(sReverb);
		d.reverb_amount = v.GetVarFloat("ReverbAmount", 1);
		d.reverb_fade = v.GetVarFloat("ReverbFadeTime", 1);
		sink(ctx, &d);
	}
}

void AreaState(size_t e, bool *pActive, float *o)
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

bool CameraPos(float *o)
{
	cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
	cCamera *pCam = pPlayer ? pPlayer->GetCamera() : NULL;
	if (pCam == NULL)
		return false;
	cVector3f v = pCam->GetPosition();
	o[0] = v.x; o[1] = v.y; o[2] = v.z;
	return true;
}

bool BodyPos(float *o)
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
void ApplyReverb(const float *p)
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

const void *PrefixRef(const unsigned char *p, size_t n)
{
	static tString sPrefix;
	sPrefix.assign((const char *)p, n);
	return &sPrefix;
}

int ScriptCall(const unsigned char *d, size_t dn, const OhplS7CallArg *a, size_t n, bool abRet)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap == NULL || pMap->GetScript() == NULL || cSomaScriptRuntime::Get() == NULL)
		return -1;
	std::vector<tString> vStr(n);
	for (size_t i = 0; i < n; ++i)
		if (a[i].kind == 0)
			vStr[i] = Str(a[i].s.p, a[i].s.n);
	int lRet = 0;
	cSomaScriptRuntime::Get()->Call(pMap->GetScript(), Str(d, dn), [&](asIScriptContext *c) {
		for (size_t i = 0; i < n; ++i)
		{
			if (a[i].kind == 0)
				c->SetArgObject((asUINT)i, &vStr[i]);
			else if (a[i].kind == 1)
				c->SetArgByte((asUINT)i, a[i].i != 0);
			else
				c->SetArgDWord((asUINT)i, a[i].i);
		}
	}, [&](asIScriptContext *c) { lRet = c->GetReturnByte() != 0; });
	return abRet ? lRet : 0;
}

bool VoicePlay(const unsigned char *s, size_t n, uint64_t token, void (*done)(uint64_t))
{
	return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->Play(Str(s, n), -1, "", 100, [token, done]() { done(token); });
}

const OhplS7Host kHost = {
	Valid,
	Play,
	[](size_t e) { ((cSoundEntry *)e)->Stop(); },
	[](size_t e, bool b) { ((cSoundEntry *)e)->SetPaused(b); },
	[](size_t e) { return (double)((cSoundEntry *)e)->GetChannel()->GetTotalTime(); },
	[](size_t e, bool b) { ((cSoundEntry *)e)->GetChannel()->SetLooping(b); },
	SetFilter,
	[](size_t e, const float *v) { ((cSoundEntry *)e)->GetChannel()->SetPosition(cVector3f(v[0], v[1], v[2])); },
	[](size_t e, float f) { ((cSoundEntry *)e)->SetDefaultVolume(f); },
	[](size_t e, float f) { ((cSoundEntry *)e)->SetDefaultSpeed(f); },
	Listener,
	EntityInfo,
	[](float v, unsigned types, int id) { return Handler()->SetGlobalVolume(v, types, id); },
	[](int id) {
		cMultipleSettingsHandler::cGSEntry *p = Handler()->GetGlobalVolumeSettingsHandler()->GetEntry(id, false);
		return p ? p->GetVal() : 1.0f;
	},
	[](float v, float speed, unsigned types, int id, bool destroy) { return Handler()->FadeGlobalVolume(v, speed, types, id, destroy); },
	[](float v, unsigned types, int id) { return Handler()->SetGlobalSpeed(v, types, id); },
	[](int id) {
		cMultipleSettingsHandler::cGSEntry *p = Handler()->GetGlobalSpeedSettingsHandler()->GetEntry(id, false);
		return p ? p->GetVal() : 1.0f;
	},
	[](float v, float speed, unsigned types, int id, bool destroy) { return Handler()->FadeGlobalSpeed(v, speed, types, id, destroy); },
	[](unsigned types, float speed) { Handler()->FadeOutAll(types, speed, false); },
	[](const unsigned char *f, size_t n, float vol, float speed, bool loop, bool resume) { Music()->Play(Str(f, n), vol, speed, loop, resume); },
	[](float speed) { Music()->Stop(speed); },
	[]() { return Music()->GetCurrentSong() != NULL; },
	[](float d, float s) { Music()->FadeVolumeMul(d, s); },
	[]() { return Music()->GetVolumeMul(); },
	[](uint8_t a, int32_t b, int32_t c) {
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		cSomaID id;
		id.mA = a;
		id.mB = b;
		id.mC = c;
		cSomaLuxEntity *p = pMap ? pMap->GetEntity(id) : NULL;
		return p != NULL && p->mbActive;
	},
	FindFile,
	[](const unsigned char *p, size_t n) { gpSomaBase->mpEngine->GetResources()->AddResourceDir(cString::To16Char(Str(p, n)), false); },
	DataNew,
	[](size_t d, const unsigned char *f, size_t n) { ((cSoundEntityData *)d)->AddSoundName(Str(f, n), eSoundEntityType_Main); },
	DataSetup,
	[]() { gpSomaBase->mpEngine->GetUpdater()->AddGlobalUpdate(hplNew(cSomaSoundUpdater, ())); },
	[]() {
		cSoundEntity::SetEventFactory(+[](cSoundEntity *apEntity) -> iSoundEntityEvent * {
			int64_t lIdx = ohpl_s7_event_of_data((size_t)apEntity->GetData());
			if (lIdx < 0)
				return NULL;
			return hplNew(cSomaSoundInstance, ((size_t)lIdx, apEntity->GetName(), apEntity, eSoundEntryType_World));
		});
	},
	[]() {
		if (gpSomaBase && gpSomaBase->mpEngine)
			gpSomaBase->mpEngine->GetResources()->GetSoundEntityManager()->SetCustomResolver(
				+[](const tString &asName) { return (cSoundEntityData *)ohpl_s7_resolve((const unsigned char *)asName.data(), asName.size()); });
	},
	ShellString,
	ShellPositionRef,
	ShellNew,
	[](size_t e) { hplDelete(Shell(e)); },
	[](size_t e) { return Shell(e)->GetHandle(); },
	ShellSetPosition,
	WorldCreateSound,
	WorldSoundByCreation,
	ObjectId,
	[](size_t e) { return (size_t)Ent(e)->GetEvent(); },
	[](size_t e, bool in, float speed) {
		if (in)
			Ent(e)->FadeIn(speed);
		else
			Ent(e)->FadeOut(speed);
	},
	SeFadeMain,
	[](size_t e) { return Ent(e)->GetData() ? (int)Ent(e)->GetData()->GetLoop() : -1; },
	SeProp,
	SeResetProps,
	[](const unsigned char *p, size_t n) { Log("%s", Str(p, n).c_str()); },
	[](const unsigned char *p, size_t n) { Warning("%s", Str(p, n).c_str()); },
	ScapeWorld,
	ScapeAreas,
	[](size_t w, const unsigned char *n, size_t l) { return (size_t)((cWorld *)w)->GetSoundEntity(Str(n, l)); },
	AreaState,
	CameraPos,
	BodyPos,
	ApplyReverb,
	[](size_t e, OhplS7Sink sink, void *ctx) { sink(ctx, (const unsigned char *)((cSomaLuxEntity *)e)->msName.data(), ((cSomaLuxEntity *)e)->msName.size()); },
	PrefixRef,
	[]() { return cSomaLuxVoiceHandler::Get() != NULL; },
	VoicePlay,
	[](const unsigned char *s, size_t n) { return cSomaLuxVoiceHandler::Get()->GetSubjectLineNumber(Str(s, n)); },
	[]() { cSomaLuxVoiceHandler::Get()->StopAll(); },
	[](const unsigned char *s, size_t n) { return cSomaLuxVoiceHandler::Get()->SceneInvolvingCharacterIsActive(Str(s, n)); },
	[](const unsigned char *s, size_t n, const unsigned char *c, size_t cn) { return cSomaLuxVoiceHandler::Get()->SubjectInvolvesCharacter(Str(s, n), Str(c, cn)); },
	ScriptCall,
};

struct cInstall
{
	cInstall() { ohpl_s7_install_host(&kHost); }
} gInstall;
} // namespace
