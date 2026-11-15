#include "SomaSound.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"

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
};

struct cInstall
{
	cInstall() { ohpl_s7_install_host(&kHost); }
} gInstall;
} // namespace
