#include "SomaSound.h"
#include "SomaBase.h"
#include "SomaFsb.h"
#include "SomaScriptBind.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>

namespace
{
cSoundHandler *Handler() { return gpSomaBase->mpEngine->GetSound()->GetSoundHandler(); }
void UpdateGameMusic();

class cSomaSoundUpdater : public iUpdateable
{
public:
	cSomaSoundUpdater() : iUpdateable("SomaSoundEvents") {}
	void Update(float afTimeStep)
	{
		cSomaSoundEvents::Get()->Update(afTimeStep);
		UpdateGameMusic();
	}
	void OnPauseUpdate(float afTimeStep) { cSomaSoundEvents::Get()->Update(afTimeStep); }
};

const unsigned char *U(const tString &s) { return (const unsigned char *)s.data(); }

void SinkString(void *apCtx, const unsigned char *apData, size_t alLen) { static_cast<tString *>(apCtx)->assign((const char *)apData, alLen); }

struct cSoundHost
{
	static bool Valid(void *, size_t e, int id) { return Handler()->IsValid((cSoundEntry *)e, id); }
	static bool Play(void *, const OhplGs5PlayReq *r, size_t *pEntry, int *pId)
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
	static void Stop(void *, size_t e) { ((cSoundEntry *)e)->Stop(); }
	static void Pause(void *, size_t e, bool b) { ((cSoundEntry *)e)->SetPaused(b); }
	static double TotalTime(void *, size_t e) { return ((cSoundEntry *)e)->GetChannel()->GetTotalTime(); }
	static void SetLooping(void *, size_t e, bool b) { ((cSoundEntry *)e)->GetChannel()->SetLooping(b); }
	static void SetFilter(void *, size_t e, int which, float f)
	{
		iSoundChannel *pCh = ((cSoundEntry *)e)->GetChannel();
		if (which == 0)
			pCh->SetFilterGainHF(f);
		else if (which == 1)
			pCh->SetFilterGainLF(f);
		else
			pCh->SetEnvSendGain(f);
	}
	static void SetPosition(void *, size_t e, const float *v) { ((cSoundEntry *)e)->GetChannel()->SetPosition(cVector3f(v[0], v[1], v[2])); }
	static void SetDefaultVolume(void *, size_t e, float f) { ((cSoundEntry *)e)->SetDefaultVolume(f); }
	static void SetDefaultSpeed(void *, size_t e, float f) { ((cSoundEntry *)e)->SetDefaultSpeed(f); }
	static void Listener(void *, float *o)
	{
		iLowLevelSound *pLow = gpSomaBase->mpEngine->GetSound()->GetLowLevel();
		cVector3f p = pLow->GetListenerPosition(), f = pLow->GetListenerForward(), u = pLow->GetListenerUp();
		o[0] = p.x; o[1] = p.y; o[2] = p.z; o[3] = f.x; o[4] = f.y; o[5] = f.z; o[6] = u.x; o[7] = u.y; o[8] = u.z;
	}
	static void Entity(void *, size_t e, float *o)
	{
		cSoundEntity *p = (cSoundEntity *)e;
		cVector3f v = p->GetWorldPosition();
		o[0] = v.x; o[1] = v.y; o[2] = v.z;
		o[3] = p->GetMinDistance();
		o[4] = p->GetMaxDistance();
		o[5] = p->GetVolume();
	}
};

const OhplGs5Host *Host()
{
	static const OhplGs5Host host = {NULL, cSoundHost::Valid, cSoundHost::Play, cSoundHost::Stop, cSoundHost::Pause, cSoundHost::TotalTime,
									 cSoundHost::SetLooping, cSoundHost::SetFilter, cSoundHost::SetPosition, cSoundHost::SetDefaultVolume,
									 cSoundHost::SetDefaultSpeed, cSoundHost::Listener, cSoundHost::Entity};
	return &host;
}

OhplGs5Bank *Bank() { return cSomaSoundEvents::Get()->GetBank(); }

tString EventName(OhplGs5Bank *apBank, size_t alEvent)
{
	tString s;
	ohpl_gs5_ev_name(apBank, alEvent, SinkString, &s);
	return s;
}
} // namespace

cSomaSoundEvents::cSomaSoundEvents() : mpBank(ohpl_gs5_bank_new()) {}

cSomaSoundEvents *cSomaSoundEvents::Get()
{
	static cSomaSoundEvents events;
	return &events;
}

void cSomaSoundEvents::LoadProject(const tString &asProject)
{
	tString sKey = cString::ToLowerCase(asProject);
	if (ohpl_gs5_ev_mark_project(mpBank, U(sKey), sKey.size()) == false)
		return;
	cResources *pRes = gpSomaBase->mpEngine->GetResources();
	tWString sFdp = pRes->GetFileSearcher()->GetFilePath(asProject + ".fdp");
	FILE *pFile = cPlatform::OpenFile(sFdp, _W("rb"));
	std::vector<unsigned char> vData;
	bool bReadOk = true;
	if (pFile)
	{
		fseek(pFile, 0, SEEK_END);
		long lLength = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);
		if (lLength > 0)
		{
			vData.resize(lLength);
			bReadOk = fread(vData.data(), lLength, 1, pFile) == 1;
		}
		fclose(pFile);
	}
	tString sProject;
	int lCount = 0;
	if (pFile == NULL || ohpl_gs5_ev_load_fdp(mpBank, vData.data(), vData.size(), bReadOk, U(asProject), asProject.size(), SinkString, &sProject, &lCount) == false)
	{
		Warning("SOMA sound: no event project '%s'\n", asProject.c_str());
		return;
	}

	// One pass per bank: banks are large and read whole
	tWString sCacheDir = cSomaFsb::GetCacheDir(_W("events-v4"));
	std::map<tString, std::set<tString>> mapByBank;
	struct cWanted
	{
		std::map<tString, std::set<tString>> *mpMap;
	} wanted = {&mapByBank};
	ohpl_gs5_ev_wanted(mpBank, U(sKey), sKey.size(), +[](void *c, const unsigned char *b, size_t bn, const unsigned char *s, size_t sn) {
		(*static_cast<cWanted *>(c)->mpMap)[tString((const char *)b, bn)].insert(tString((const char *)s, sn));
	}, &wanted);
	std::map<tString, tString> mapFiles;
	for (auto &bank : mapByBank)
	{
		std::map<tString, tString> mapBankFiles;
		// Banks in different dirs can share a name: prefer the one next to the project
		tWString sBank = pRes->GetFileSearcher()->GetFilePath(bank.first + ".fsb");
		tWString sLocal = cString::GetFilePathW(sFdp) + cString::To16Char(bank.first + ".fsb");
		tString sPrefix = bank.first + "__";
		if (sLocal != sBank && cPlatform::FileExists(sLocal))
		{
			sBank = sLocal;
			sPrefix = sKey + "__" + sPrefix;
		}
		cSomaFsb::ExtractSamples(sBank, sCacheDir, sPrefix, std::vector<tString>(bank.second.begin(), bank.second.end()), mapBankFiles);
		for (auto &f : mapBankFiles)
			mapFiles[bank.first + "__" + f.first] = f.second;
	}
	std::vector<OhplGs5FilePair> vPairs;
	for (auto &f : mapFiles)
		vPairs.push_back({U(f.first), f.first.size(), U(f.second), f.second.size()});
	ohpl_gs5_ev_assign(mpBank, U(sKey), sKey.size(), vPairs.data(), vPairs.size());
	cFileSearcher *pSearcher = pRes->GetFileSearcher();
	if (std::any_of(mapFiles.begin(), mapFiles.end(), [&](auto &f) { return pSearcher->GetFilePath(f.second) == _W(""); }))
		pRes->AddResourceDir(sCacheDir, false);
	Log("SOMA sound: %d events in project '%s', %d samples\n", lCount, sProject.c_str(), (int)mapFiles.size());
}

void cSomaSoundEvents::PreloadProject(const tString &asProject)
{
	LoadProject(asProject);
}

int cSomaSoundEvents::GetEvent(const tString &asName)
{
	int64_t lProject = ohpl_gs5_ev_project_len(U(asName), asName.size());
	if (lProject < 0)
		return -1;
	LoadProject(asName.substr(0, lProject));
	return (int)ohpl_gs5_ev_find(mpBank, U(asName), asName.size());
}

cSoundEntityData *cSomaSoundEvents::Resolve(const tString &asEvent)
{
	int lEvent = GetEvent(asEvent);
	if (lEvent < 0)
		return NULL;
	OhplGs5EventInfo info;
	ohpl_gs5_ev_info(mpBank, lEvent, &info);
	if (info.data)
		return (cSoundEntityData *)info.data;
	if (info.has_files == false)
	{
		if (info.has_defs)
			Warning("SOMA sound: event '%s' has no playable samples\n", asEvent.c_str());
		return NULL;
	}

	cResources *pRes = gpSomaBase->mpEngine->GetResources();
	cSoundEntityData *pData = hplNew(cSoundEntityData, (cString::ToLowerCase(asEvent), pRes, gpSomaBase->mpEngine->GetSound()));
	ohpl_gs5_ev_files(mpBank, lEvent, +[](void *c, const unsigned char *f, size_t n) {
		static_cast<cSoundEntityData *>(c)->AddSoundName(tString((const char *)f, n), eSoundEntityType_Main);
	}, pData);
	pData->SetVolume(1);
	pData->SetMinDistance(info.min_dist);
	pData->SetMaxDistance(info.max_dist);
	pData->SetLoop(info.one_shot == false);
	pData->SetUse3D(info.is_3d);
	pData->SetStream(false);
	pData->SetFadeStart(false);
	pData->SetFadeStop(false);
	ohpl_gs5_ev_set_data(mpBank, lEvent, (size_t)pData);

	static bool bFactory = false;
	if (bFactory == false)
	{
		bFactory = true;
		cSoundEntity::SetEventFactory(+[](cSoundEntity *apEntity) -> iSoundEntityEvent * {
			int64_t lIdx = ohpl_gs5_ev_of_data(Bank(), (size_t)apEntity->GetData());
			if (lIdx < 0)
				return NULL;
			return hplNew(cSomaSoundInstance, ((size_t)lIdx, apEntity->GetName(), apEntity, eSoundEntryType_World));
		});
	}
	return pData;
}

cSomaSoundInstance *cSomaSoundEvents::PlayGui(const tString &asEvent, float afVolume, int alEntryType, bool abLoop, bool abStream)
{
	int lEvent = GetEvent(asEvent);
	if (lEvent < 0 && cString::GetFileExt(asEvent) == "" && asEvent.find('/') != tString::npos)
		return NULL;
	if (lEvent < 0)
		lEvent = (int)ohpl_gs5_ev_file_event(mpBank, U(asEvent), asEvent.size(), abLoop, abStream);
	OhplGs5EventInfo info;
	ohpl_gs5_ev_info(mpBank, lEvent, &info);
	if (info.has_files == false)
		return NULL;
	cSomaSoundInstance *pInst = hplNew(cSomaSoundInstance, ((size_t)lEvent, EventName(mpBank, lEvent), NULL, (eSoundEntryType)alEntryType));
	pInst->SetVolume(afVolume);
	pInst->Play();
	if (pInst->IsActive() == false)
	{
		hplDelete(pInst);
		return NULL;
	}
	return pInst;
}

cSomaSoundInstance *cSomaSoundEvents::Play3D(const tString &asEvent, float afVolume, const cVector3f &avPos, int alEntryType)
{
	int lEvent = GetEvent(asEvent);
	OhplGs5EventInfo info;
	if (lEvent < 0 || ohpl_gs5_ev_info(mpBank, lEvent, &info) == false || info.has_files == false)
		return NULL;
	cSomaSoundInstance *pInst = hplNew(cSomaSoundInstance, ((size_t)lEvent, EventName(mpBank, lEvent), NULL, (eSoundEntryType)alEntryType));
	pInst->SetVolume(afVolume);
	pInst->SetPosition(avPos);
	pInst->mpS->play_3d = info.is_3d;
	pInst->Play();
	if (pInst->IsActive() == false)
	{
		hplDelete(pInst);
		return NULL;
	}
	return pInst;
}

void cSomaSoundEvents::Update(float afTimeStep)
{
	std::vector<cSomaSoundInstance *> vGui;
	for (cSomaSoundInstance *p : mlstInstances)
		if (p->mpEntity == NULL)
			vGui.push_back(p);
	for (cSomaSoundInstance *p : vGui)
	{
		p->Update(afTimeStep);
		if (p->IsStopped())
			hplDelete(p);
	}
}

void cSomaSoundEvents::FadeOutAll(tFlag aTypes, float afSpeed)
{
	std::vector<cSomaSoundInstance *> v;
	for (cSomaSoundInstance *p : mlstInstances)
		if (p->mpEntity == NULL && (p->GetType() & aTypes))
			v.push_back(p);
	for (cSomaSoundInstance *p : v)
		p->FadeOut(afSpeed);
}

cSomaSoundInstance *cSomaSoundEvents::FindInstance(const tString &asName)
{
	for (cSomaSoundInstance *p : mlstInstances)
		if (p->mpEntity == NULL && p->IsActive() && cString::ToLowerCase(p->GetName()) == cString::ToLowerCase(asName))
			return p;
	return NULL;
}

bool cSomaSoundEvents::IsLive(cSomaSoundInstance *apInstance, int alId)
{
	auto it = mmapLive.find(apInstance);
	return it != mmapLive.end() && (alId < 0 || it->second == alId);
}

cSomaSoundInstance::cSomaSoundInstance(size_t alEvent, const tString &asName, cSoundEntity *apEntity, eSoundEntryType aType)
	: msName(asName), mpEntity(apEntity)
{
	cSomaSoundEvents *pEvents = cSomaSoundEvents::Get();
	mlH = ohpl_gs5_inst_new(pEvents->mpBank, alEvent, (size_t)apEntity, apEntity != NULL, (int)aType);
	mpS = ohpl_gs5_inst_state(pEvents->mpBank, mlH);
	ohpl_gs5_ev_info(pEvents->mpBank, alEvent, &mEvent);
	mlId = mpS->id;
	pEvents->mlstInstances.push_back(this);
	pEvents->mmapLive[this] = mlId;
	if (pEvents->mbUpdaterAdded == false)
	{
		pEvents->mbUpdaterAdded = true;
		gpSomaBase->mpEngine->GetUpdater()->AddGlobalUpdate(hplNew(cSomaSoundUpdater, ()));
	}
}

cSomaSoundInstance::~cSomaSoundInstance()
{
	cSomaSoundEvents *pEvents = cSomaSoundEvents::Get();
	ohpl_gs5_inst_destroy(pEvents->mpBank, Host(), mlH);
	pEvents->mlstInstances.remove(this);
	pEvents->mmapLive.erase(this);
}

void cSomaSoundInstance::Play() { ohpl_gs5_inst_play(Bank(), Host(), mlH); }
void cSomaSoundInstance::StopNow() { ohpl_gs5_inst_stop_now(Bank(), Host(), mlH); }
void cSomaSoundInstance::Stop(bool abPlayEnd) { ohpl_gs5_inst_stop(Bank(), Host(), mlH); }
void cSomaSoundInstance::FadeOut(float afSpeed) { ohpl_gs5_inst_fade_out(Bank(), Host(), mlH, afSpeed); }
void cSomaSoundInstance::FadeInTo(float afVolumeMul, float afSpeed) { ohpl_gs5_inst_fade_in_to(Bank(), Host(), mlH, afVolumeMul, afSpeed); }
void cSomaSoundInstance::FadeVolumeMulTo(float afDest, float afSpeed) { ohpl_gs5_inst_fade_volume_mul_to(Bank(), mlH, afDest, afSpeed); }
void cSomaSoundInstance::FadeSpeedMulTo(float afDest, float afSpeed) { ohpl_gs5_inst_fade_speed_mul_to(Bank(), mlH, afDest, afSpeed); }
void cSomaSoundInstance::Update(float afTimeStep) { ohpl_gs5_inst_update(Bank(), Host(), mlH, afTimeStep); }
void cSomaSoundInstance::SetPaused(bool abX) { ohpl_gs5_inst_set_paused(Bank(), Host(), mlH, abX); }
void cSomaSoundInstance::SetParam(int alIdx, float afValue) { ohpl_gs5_inst_set_param(Bank(), mlH, alIdx, afValue); }
float cSomaSoundInstance::GetParamValue(int alIdx) { return ohpl_gs5_inst_param_value(Bank(), mlH, alIdx); }
int cSomaSoundInstance::GetParamNum() { return ohpl_gs5_inst_param_count(Bank(), mlH); }
float cSomaSoundInstance::GetTotalTime() { return ohpl_gs5_inst_total_time(Bank(), Host(), mlH); }
bool cSomaSoundInstance::IsStopped() { return mpS->stopped && ohpl_gs5_inst_voice(Bank(), mlH, 0, NULL) == 0; }

int cSomaSoundInstance::ParamIndex(const tString &asName)
{
	return ohpl_gs5_inst_param_index(Bank(), mlH, U(asName), asName.size());
}

const cSomaSoundEvents::cParam *cSomaSoundInstance::GetParamDef(int alIdx)
{
	OhplGs5ParamDef d;
	if (ohpl_gs5_inst_param_def(Bank(), mlH, alIdx, &d, SinkString, &mParamCache.msName) == false)
		return NULL;
	mParamCache.mfMin = d.min;
	mParamCache.mfMax = d.max;
	mParamCache.mfVelocity = d.velocity;
	mParamCache.mfSeek = d.seek;
	mParamCache.mfSustain = d.sustain;
	mParamCache.mlLoopMode = d.loop_mode;
	mParamCache.mlBuiltin = d.builtin;
	return &mParamCache;
}

tString cSomaSoundInstance::Describe()
{
	int lVoices = ohpl_gs5_inst_voice(Bank(), mlH, 0, NULL);
	char buf[256];
	snprintf(buf, sizeof(buf), "%s voices=%d aud=%.3f vol=%.2f mul=%.2f fade=%.2f%s%s", msName.c_str(), lVoices, mpS->audibility, mpS->volume,
			 mpS->volume_mul, mpS->fade, mpEntity ? " entity" : "", mpS->stopped ? " stopped" : "");
	tString s = buf;
	std::vector<std::array<float, 3>> vVoices(lVoices);
	for (int i = 0; i < lVoices; ++i)
		ohpl_gs5_inst_voice(Bank(), mlH, i, vVoices[i].data());
	for (auto &v : vVoices)
		if (v[0] < 1 || v[1] < 1)
			s += " hf=" + cString::ToString(v[0]) + " lf=" + cString::ToString(v[1]);
	for (auto &v : vVoices)
		if (v[2] < 1)
			s += " send=" + cString::ToString(v[2]);
	for (int i = 0; i < GetParamNum(); ++i)
		s += " " + GetParamDef(i)->msName + "=" + cString::ToString(GetParamValue(i));
	return s;
}

namespace
{
struct cGameMusic
{
	tString msFile;
	float mfVolume = 0;
	bool mbLoop = false;
	bool mbResume = false;
};

const int kMaxMusicPrio = 10;
cGameMusic gvGameMusic[kMaxMusicPrio + 1];
int glCurrentMusicPrio = -1;
char gMusicTag;

cMusicHandler *MusicHandler() { return gpSomaBase->mpEngine->GetSound()->GetMusicHandler(); }

void PlayHighestMusic()
{
	for (int i = kMaxMusicPrio; i >= 0; --i)
	{
		cGameMusic &m = gvGameMusic[i];
		if (m.msFile.empty())
			continue;
		if (m.mbLoop == false)
		{
			m.msFile = "";
			continue;
		}
		MusicHandler()->Play(m.msFile, m.mfVolume, 0.3f, true, m.mbResume);
		glCurrentMusicPrio = i;
		return;
	}
}

void PlayMusic(const tString &f, bool loop, float vol, float fade, int prio, bool resume)
{
	prio = cMath::Clamp(prio, 0, kMaxMusicPrio);
	cGameMusic &m = gvGameMusic[prio];
	if (m.msFile == f)
		return;
	if (glCurrentMusicPrio <= prio)
	{
		MusicHandler()->Play(f, vol, fade > 0 ? vol / fade : 100.0f, loop, resume);
		glCurrentMusicPrio = prio;
	}
	m.msFile = f;
	m.mfVolume = vol;
	m.mbLoop = loop;
	m.mbResume = resume;
}

void StopMusic(float fade, int prio)
{
	prio = cMath::Clamp(prio, 0, kMaxMusicPrio);
	cGameMusic &m = gvGameMusic[prio];
	if (m.msFile.empty())
		return;
	m.msFile = "";
	if (prio != glCurrentMusicPrio)
		return;
	MusicHandler()->Stop(fade > 0 ? m.mfVolume / fade : 100.0f);
	glCurrentMusicPrio = -1;
	PlayHighestMusic();
}

struct cDynamicTrack
{
	cSomaID mID;
	int mlTrackPrio = 0, mlMusicPrio = -1;
	tString msFile;
	float mfVolume = 0, mfFadeIn = 0, mfFadeOut = 0;
};
std::vector<cDynamicTrack> gvDynamicTracks;
cDynamicTrack gCurrentDynamicTrack;

// cLuxMusicHandler::VariableUpdate (Rebirth binary)
void UpdateDynamicTracks()
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	std::erase_if(gvDynamicTracks, [pMap](const cDynamicTrack &t) {
		if (t.mID == cSomaID())
			return false;
		cSomaLuxEntity *p = pMap ? pMap->GetEntity(t.mID) : NULL;
		return p == NULL || p->mbActive == false;
	});
	const cDynamicTrack *pBest = NULL;
	for (const cDynamicTrack &t : gvDynamicTracks)
		if (pBest == NULL || t.mlTrackPrio >= pBest->mlTrackPrio)
			pBest = &t;
	cDynamicTrack &cur = gCurrentDynamicTrack;
	if (pBest)
	{
		if (pBest->mID == cur.mID && pBest->msFile == cur.msFile && pBest->mfVolume == cur.mfVolume)
			return;
		if (cur.mlMusicPrio >= 0 && cur.mlMusicPrio != pBest->mlMusicPrio)
			StopMusic(pBest->mfFadeIn, cur.mlMusicPrio);
		PlayMusic(pBest->msFile, true, pBest->mfVolume, pBest->mfFadeIn, pBest->mlMusicPrio, true);
		cur = *pBest;
	}
	else if (cur.msFile.empty() == false)
	{
		StopMusic(cur.mfFadeOut, cur.mlMusicPrio);
		cur = cDynamicTrack();
	}
}

void UpdateGameMusic()
{
	UpdateDynamicTracks();
	if (glCurrentMusicPrio < 0 || MusicHandler()->GetCurrentSong())
		return;
	gvGameMusic[glCurrentMusicPrio].msFile = "";
	glCurrentMusicPrio = -1;
	PlayHighestMusic();
}

void RegisterMusicNatives(asIScriptEngine *e)
{
	typedef const tString &S;
	SOMA_FUNC(e, "cLuxMusicHandler@ cLux_GetMusicHandler()", +[]() { return (void *)&gMusicTag; });
	SOMA_METHOD(e, "cLuxMusicHandler",
				"void Play(const tString &in asFile, bool abLoop,float afVolume, float afFreq, float afVolumeFadeTime, float afFreqFadeTime, int alPrio, bool abResume, bool abSpecialEffect)",
				+[](void *, S f, bool loop, float vol, float, float fade, float, int prio, bool resume, bool) { PlayMusic(f, loop, vol, fade, prio, resume); });
	SOMA_METHOD(e, "cLuxMusicHandler", "void Stop(float afFadeTime, int alPrio)", +[](void *, float fade, int prio) { StopMusic(fade, prio); });
	SOMA_METHOD(e, "cLuxMusicHandler",
				"void AddDynamicTrack(tID a_idEntity, int alTrackPrio, int alMusicPrio, const tString&in asFile, float afVolume, float afFadeInTime, float afFadeOutTime)",
				+[](void *, cSomaID id, int trackPrio, int musicPrio, S f, float vol, float fadeIn, float fadeOut) {
					auto it = std::find_if(gvDynamicTracks.begin(), gvDynamicTracks.end(),
										   [&](const cDynamicTrack &t) { return t.mID == id; });
					cDynamicTrack &t = it != gvDynamicTracks.end() ? *it : gvDynamicTracks.emplace_back();
					t = {id, trackPrio, musicPrio, f, vol, fadeIn, fadeOut};
				});
	SOMA_METHOD(e, "cLuxMusicHandler", "void RemoveDynamicTrack(tID a_idEntity)", +[](void *, cSomaID id) {
		auto it = std::find_if(gvDynamicTracks.begin(), gvDynamicTracks.end(), [&](const cDynamicTrack &t) { return t.mID == id; });
		if (it != gvDynamicTracks.end())
		{
			*it = gvDynamicTracks.back();
			gvDynamicTracks.pop_back();
		}
	});
}
} // namespace

// cLuxMusicHandler::Scriptable_SaveToBuffer: music slots and dynamic tracks
tString SomaSerializeMusic()
{
	std::ostringstream o;
	o.precision(9);
	for (int i = 0; i <= kMaxMusicPrio; ++i)
		if (gvGameMusic[i].msFile.empty() == false)
			o << "M " << i << ' ' << gvGameMusic[i].mfVolume << ' ' << gvGameMusic[i].mbLoop << ' ' << gvGameMusic[i].mbResume << ' '
			  << gvGameMusic[i].msFile << '\n';
	for (const cDynamicTrack &t : gvDynamicTracks)
		o << "D " << (int)t.mID.mA << ' ' << t.mID.mB << ' ' << t.mID.mC << ' ' << t.mlTrackPrio << ' ' << t.mlMusicPrio << ' ' << t.mfVolume
		  << ' ' << t.mfFadeIn << ' ' << t.mfFadeOut << ' ' << t.msFile << '\n';
	return o.str();
}

void SomaDeserializeMusic(const tString &asData)
{
	for (cGameMusic &m : gvGameMusic)
		m = cGameMusic();
	gvDynamicTracks.clear();
	gCurrentDynamicTrack = cDynamicTrack();
	std::istringstream in(asData);
	tString sLine;
	while (std::getline(in, sLine))
	{
		std::istringstream l(sLine);
		char c = 0;
		l >> c;
		if (c == 'M')
		{
			int lPrio = -1;
			cGameMusic m;
			l >> lPrio >> m.mfVolume >> m.mbLoop >> m.mbResume >> std::ws;
			std::getline(l, m.msFile);
			if (lPrio >= 0 && lPrio <= kMaxMusicPrio)
				gvGameMusic[lPrio] = m;
		}
		else if (c == 'D')
		{
			cDynamicTrack t;
			int lA = 0;
			l >> lA >> t.mID.mB >> t.mID.mC >> t.mlTrackPrio >> t.mlMusicPrio >> t.mfVolume >> t.mfFadeIn >> t.mfFadeOut >> std::ws;
			t.mID.mA = (uint8_t)lA;
			std::getline(l, t.msFile);
			gvDynamicTracks.push_back(t);
		}
	}
	MusicHandler()->Stop(100.0f);
	glCurrentMusicPrio = -1;
	PlayHighestMusic();
}

typedef cSomaSoundInstance Inst;

static Inst *Live(Inst *p) { return cSomaSoundEvents::Get()->IsLive(p) ? p : NULL; }

static Inst *EntityEvent(cSoundEntity *o) { return o ? (Inst *)o->GetEvent() : NULL; }

static void RegisterEntryNatives(asIScriptEngine *e)
{
	typedef const tString &S;
	static tString sEmpty;
	SOMA_METHOD(e, "cSoundEntry", "const tString& GetName()", +[](Inst *p) -> const tString & { return Live(p) ? p->GetName() : sEmpty; });
	SOMA_METHOD(e, "cSoundEntry", "eSoundEntryType GetType()", +[](Inst *p) { return Live(p) ? (int)p->GetType() : 0; });
	SOMA_METHOD(e, "cSoundEntry", "int GetId()", +[](Inst *p) { return Live(p) ? p->GetId() : -1; });
	SOMA_METHOD(e, "cSoundEntry", "bool IsFirstTime()", +[](Inst *p) { return Live(p) && p->GetElapsedTime() == 0; });
	SOMA_METHOD(e, "cSoundEntry", "void SetPosition(const cVector3f&in avPosition)", +[](Inst *p, const cVector3f &v) {
		if (Live(p))
			p->SetPosition(v);
	});
	SOMA_METHOD(e, "cSoundEntry", "const cVector3f& GetPosition()", +[](Inst *p) -> const cVector3f & {
		static cVector3f vZero(0);
		return Live(p) ? p->GetPosition() : vZero;
	});
	SOMA_METHOD(e, "cSoundEntry", "bool IsPlaying()", +[](Inst *p) { return Live(p) && p->IsPlaying(); });
	SOMA_METHOD(e, "cSoundEntry", "float GetElapsedTime()", +[](Inst *p) { return Live(p) ? p->GetElapsedTime() : 0.0f; });
	SOMA_METHOD(e, "cSoundEntry", "float GetTotalTime()", +[](Inst *p) { return Live(p) ? p->GetTotalTime() : 0.0f; });
	SOMA_METHOD(e, "cSoundEntry", "float GetAudibility()", +[](Inst *p) { return Live(p) ? p->GetAudibility() : 0.0f; });
	SOMA_METHOD(e, "cSoundEntry", "void SetParam(const tString &in asName, float afValue)", +[](Inst *p, S n, float v) {
		if (Live(p))
			p->SetParam(n, v);
	});
	SOMA_METHOD(e, "cSoundEntry", "void SetParam(int alIdx, float afValue)", +[](Inst *p, int i, float v) {
		if (Live(p))
			p->SetParam(i, v);
	});
	SOMA_METHOD(e, "cSoundEntry", "int GetParamNum()", +[](Inst *p) { return Live(p) ? p->GetParamNum() : 0; });
	SOMA_METHOD(e, "cSoundEntry", "float GetParamValue(int alIdx)", +[](Inst *p, int i) { return Live(p) ? p->GetParamValue(i) : 0.0f; });
	SOMA_METHOD(e, "cSoundEntry", "float GetParamMin(int alIdx)", +[](Inst *p, int i) {
		const cSomaSoundEvents::cParam *d = Live(p) ? p->GetParamDef(i) : NULL;
		return d ? d->mfMin : 0.0f;
	});
	SOMA_METHOD(e, "cSoundEntry", "float GetParamMax(int alIdx)", +[](Inst *p, int i) {
		const cSomaSoundEvents::cParam *d = Live(p) ? p->GetParamDef(i) : NULL;
		return d ? d->mfMax : 0.0f;
	});
	SOMA_METHOD(e, "cSoundEntry", "const tString& GetParamName(int alIdx)", +[](Inst *p, int i) -> const tString & {
		const cSomaSoundEvents::cParam *d = Live(p) ? p->GetParamDef(i) : NULL;
		return d ? d->msName : sEmpty;
	});
	SOMA_METHOD(e, "cSoundEntry", "void SetPaused(bool abX)", +[](Inst *p, bool x) {
		if (Live(p))
			p->SetPaused(x);
	});
	SOMA_METHOD(e, "cSoundEntry", "bool GetPaused()", +[](Inst *p) { return Live(p) && p->GetPaused(); });
	SOMA_METHOD(e, "cSoundEntry", "void SetVolume(float afX)", +[](Inst *p, float x) {
		if (Live(p))
			p->SetVolume(x);
	});
	SOMA_METHOD(e, "cSoundEntry", "float GetVolume()", +[](Inst *p) { return Live(p) ? p->GetVolume() : 0.0f; });
	SOMA_METHOD(e, "cSoundEntry", "void SetSpeed(float afX)", +[](Inst *p, float x) {
		if (Live(p))
			p->SetSpeedMul(x);
	});
	SOMA_METHOD(e, "cSoundEntry", "float GetSpeed()", +[](Inst *p) { return Live(p) ? p->GetSpeedMul() : 1.0f; });
	SOMA_METHOD(e, "cSoundEntry", "float GetVolumeMul()", +[](Inst *p) { return Live(p) ? p->GetVolumeMul() : 0.0f; });
	SOMA_METHOD(e, "cSoundEntry", "float GetSpeedMul()", +[](Inst *p) { return Live(p) ? p->GetSpeedMul() : 1.0f; });
	SOMA_METHOD(e, "cSoundEntry", "float GetMinDistance()", +[](Inst *p) { return Live(p) ? p->GetMinDist() : 0.0f; });
	SOMA_METHOD(e, "cSoundEntry", "float GetMaxDistance()", +[](Inst *p) { return Live(p) ? p->GetMaxDist() : 0.0f; });
	SOMA_METHOD(e, "cSoundEntry", "bool Is3D()", +[](Inst *p) { return Live(p) && p->Is3D(); });
	SOMA_METHOD(e, "cSoundEntry", "bool IsOneShot()", +[](Inst *p) { return Live(p) && p->IsOneShot(); });
	SOMA_METHOD(e, "cSoundEntry", "bool IsVirtual()", +[](Inst *p) { return Live(p) && p->GetAudibility() < 1e-4f; });
	SOMA_METHOD(e, "cSoundEntry", "void Stop(bool abPlayEnd)", +[](Inst *p, bool end) {
		if (Live(p))
			p->Stop(end);
	});
	SOMA_METHOD(e, "cSoundEntry", "void SetVolumeMul(float afMul)", +[](Inst *p, float x) {
		if (Live(p))
			p->SetVolumeMul(x);
	});
	SOMA_METHOD(e, "cSoundEntry", "void SetSpeedMul(float afMul)", +[](Inst *p, float x) {
		if (Live(p))
			p->SetSpeedMul(x);
	});
	SOMA_METHOD(e, "cSoundEntry", "void FadeVolumeMulTo(float afDestMul, float afSpeed)", +[](Inst *p, float d, float sp) {
		if (Live(p))
			p->FadeVolumeMulTo(d, sp);
	});
	SOMA_METHOD(e, "cSoundEntry", "void FadeSpeedMulTo(float afDestMul, float afSpeed)", +[](Inst *p, float d, float sp) {
		if (Live(p))
			p->FadeSpeedMulTo(d, sp);
	});
	SOMA_METHOD(e, "cSoundEntry", "void FadeOut(float afSpeed)", +[](Inst *p, float sp) {
		if (Live(p))
			p->FadeOut(sp);
	});
	SOMA_METHOD(e, "cSoundEntry", "void FadeIn(float afVolumeMul,float afSpeed)", +[](Inst *p, float v, float sp) {
		if (Live(p))
			p->FadeInTo(v, sp);
	});
	SOMA_METHOD(e, "cSoundEntry", "void SetStopDisabled(bool abX)", +[](Inst *p, bool x) {
		if (Live(p))
			p->SetStopDisabled(x);
	});
	SOMA_METHOD(e, "cSoundEntry", "bool GetStopDisabled()", +[](Inst *p) { return Live(p) && p->GetStopDisabled(); });
	SOMA_FUNC(e, "bool cSound_IsValid(cSoundEntry @apEntry, int alID)",
			  +[](Inst *p, int id) { return cSomaSoundEvents::Get()->IsLive(p, id) && p->IsActive(); });
	SOMA_FUNC(e, "cSoundEntry@ cSound_GetEntry(const tString&in asName)", +[](S n) { return cSomaSoundEvents::Get()->FindInstance(n); });

	SOMA_METHOD(e, "cSoundEntity", "void FadeIn(float afSpeed, float afTargetVol)", +[](cSoundEntity *o, float speed, float vol) {
		if (Inst *p = EntityEvent(o))
		{
			o->FadeIn(speed);
			p->FadeInTo(1, speed);
			if (speed > 0)
			{
				p->SetVolumeMul(0);
				p->FadeVolumeMulTo(vol, speed);
			}
			else
				p->SetVolumeMul(vol);
			return;
		}
		o->Play(false);
		if (cSoundEntry *pEntry = o->GetSoundEntry(eSoundEntityType_Main, false))
		{
			if (speed > 0)
				pEntry->FadeIn(vol, speed);
			else
				pEntry->SetVolumeMul(vol);
		}
	});
	SOMA_METHOD(e, "cSoundEntity", "void FadeOut(float afSpeed)", +[](cSoundEntity *o, float speed) { o->FadeOut(speed); });
	SOMA_METHOD(e, "cSoundEntity", "bool IsOneShot()", +[](cSoundEntity *o) {
		if (Inst *p = EntityEvent(o))
			return p->IsOneShot();
		return o->GetData() == NULL || o->GetData()->GetLoop() == false;
	});
	SOMA_METHOD(e, "cSoundEntity", "void SetParam(int alIdx, float afValue)", +[](cSoundEntity *o, int i, float v) {
		if (Inst *p = EntityEvent(o))
			p->SetParam(i, v);
	});
	SOMA_METHOD(e, "cSoundEntity", "void SetParam(const tString&in asName, float afValue)", +[](cSoundEntity *o, S n, float v) {
		if (Inst *p = EntityEvent(o))
			p->SetParam(n, v);
	});
	SOMA_METHOD(e, "cSoundEntity", "float GetParam(int alIdx)", +[](cSoundEntity *o, int i) {
		Inst *p = EntityEvent(o);
		return p ? p->GetParamValue(i) : 0.0f;
	});
	SOMA_METHOD(e, "cSoundEntity", "float GetParam(const tString&in asName)", +[](cSoundEntity *o, S n) {
		Inst *p = EntityEvent(o);
		return p ? p->GetParamValue(p->ParamIndex(n)) : 0.0f;
	});
	SOMA_METHOD(e, "cSoundEntity", "cSoundEntry@ GetSoundEntry(bool abCheckEntryValidity)", +[](cSoundEntity *o, bool) { return EntityEvent(o); });
	SOMA_METHOD(e, "cSoundEntity", "void FadeVolumeMul(float afDest, float afSpeed)", +[](cSoundEntity *o, float d, float sp) {
		if (Inst *p = EntityEvent(o))
			p->FadeVolumeMulTo(d, sp);
	});
	SOMA_METHOD(e, "cSoundEntity", "void FadeSpeedMul(float afDest, float afSpeed)", +[](cSoundEntity *o, float d, float sp) {
		if (Inst *p = EntityEvent(o))
			p->FadeSpeedMulTo(d, sp);
	});
	SOMA_METHOD(e, "cSoundEntity", "float GetElapsedTime()", +[](cSoundEntity *o) {
		Inst *p = EntityEvent(o);
		return p ? p->GetElapsedTime() : 0.0f;
	});
	SOMA_METHOD(e, "cSoundEntity", "void SetCustomMinDistance(float afX)", +[](cSoundEntity *o, float x) { o->SetMinDistance(x); });
	SOMA_METHOD(e, "cSoundEntity", "void SetCustomMaxDistance(float afX)", +[](cSoundEntity *o, float x) { o->SetMaxDistance(x); });
	SOMA_METHOD(e, "cSoundEntity", "float GetCustomMinDistance()", +[](cSoundEntity *o) { return o->GetMinDistance(); });
	SOMA_METHOD(e, "cSoundEntity", "float GetCustomMaxDistance()", +[](cSoundEntity *o) { return o->GetMaxDistance(); });
	SOMA_METHOD(e, "cSoundEntity", "void SetUseCustomProperties(bool abX)", +[](cSoundEntity *o, bool x) {
		if (x == false && o->GetData())
		{
			o->SetMinDistance(o->GetData()->GetMinDistance());
			o->SetMaxDistance(o->GetData()->GetMaxDistance());
			o->SetVolume(o->GetData()->GetVolume());
		}
	});
}

void cSomaSoundEvents::RegisterNatives(asIScriptEngine *e)
{
	if (gpSomaBase && gpSomaBase->mpEngine)
		gpSomaBase->mpEngine->GetResources()->GetSoundEntityManager()->SetCustomResolver(
			+[](const tString &asName) { return cSomaSoundEvents::Get()->Resolve(asName); });
	typedef const tString &S;
	SOMA_METHOD(e, "cWorld", "cSoundEntity@ CreateSoundEntity(const tString&in asName, const tString&in asSoundEntity, bool abRemoveWhenOver)",
				+[](cWorld *w, S n, S file, bool remove) -> cSoundEntity * {
					return (file.empty() ? NULL : w->CreateSoundEntity(n, file, remove));
				});
	SOMA_METHOD(e, "cWorld", "cSoundEntity@ CreateSoundEntityEx(const tString &in asName,const tString &in asSoundDataFile, bool abRemoveWhenOver, bool abNonBlockLoad)",
				+[](cWorld *w, S n, S file, bool remove, bool) -> cSoundEntity * {
					return (file.empty() ? NULL : w->CreateSoundEntity(n, file, remove));
				});
	SOMA_METHOD(e, "cWorld", "tID CreateSoundEntityID(const tString &in asName,const tString &in asSoundDataFile, bool abRemoveWhenOver)",
				+[](cWorld *w, S n, S file, bool remove) {
					return SomaObjectID((file.empty() ? NULL : w->CreateSoundEntity(n, file, remove)), "cSoundEntity");
				});
	SOMA_METHOD(e, "cWorld", "cSoundEntity@ GetSoundEntityFromCreationID(int alID)", +[](cWorld *w, int id) -> cSoundEntity * {
		cSoundEntityIterator it = w->GetSoundEntityIterator();
		while (it.HasNext())
		{
			cSoundEntity *p = it.Next();
			if (p->GetCreationID() == id)
				return p;
		}
		return NULL;
	});
	SOMA_METHOD(e, "cWorld", "tID CreateSoundEntityExID(const tString &in asName,const tString &in asSoundDataFile, bool abRemoveWhenOver, bool abNonBlockLoad)",
				+[](cWorld *w, S n, S file, bool remove, bool) {
					return SomaObjectID((file.empty() ? NULL : w->CreateSoundEntity(n, file, remove)), "cSoundEntity");
				});
	RegisterEntryNatives(e);
	SOMA_FUNC(e, "bool cLux_PlayGuiSoundData(const tString&in asName, eSoundEntryType aDestType, float afVolMul, bool abSkipPreviousRandom)",
			  +[](S n, int type, float vol, bool) { return cSomaSoundEvents::Get()->PlayGui(n, vol, type) != NULL; });
	SOMA_FUNC(e, "bool cLux_PlayGuiSoundDataEx(const tString&in asName, eSoundEntryType aDestType, float afVolMul, bool abSkipPreviousRandom, cLuxSoundExtraData @apExtraData)",
			  +[](S n, int type, float vol, bool, char *pExtra) {
				  Inst *pInst = cSomaSoundEvents::Get()->PlayGui(n, vol, type);
				  if (pExtra)
					  *(Inst **)(pExtra + 32) = pInst;
				  return pInst != NULL;
			  });
	SOMA_FUNC(e, "void cSound_FadeMusicVolumeMul(float afDest, float afSpeed)",
			  +[](float d, float sp) { gpSomaBase->mpEngine->GetSound()->GetMusicHandler()->FadeVolumeMul(d, sp); });
	SOMA_FUNC(e, "float cSound_GetMusicVolumeMul()", +[]() { return gpSomaBase->mpEngine->GetSound()->GetMusicHandler()->GetVolumeMul(); });
	RegisterMusicNatives(e);
	SOMA_FUNC(e, "cSoundEntry@ cSound_PlayGui(const tString&in asName, bool abLoop, float afVolume, const cVector3f&in avPos, eSoundEntryType aEntryType)",
			  +[](S n, bool loop, float vol, const cVector3f &, int type) { return cSomaSoundEvents::Get()->PlayGui(n, vol, type, loop); });
	SOMA_FUNC(e, "cSoundEntry@ cSound_PlayGuiStream(const tString&in asFileName, bool abLoop, float afVolume, const cVector3f&in avPos, eSoundEntryType aEntryType)",
			  +[](S n, bool loop, float vol, const cVector3f &, int type) { return cSomaSoundEvents::Get()->PlayGui(n, vol, type, loop, true); });
	SOMA_FUNC(e, "cSoundEntry@ cSound_PlaySoundEntityGui(const tString&in asName,bool abLoop,float afVolume, eSoundEntryType aEntryType, const cVector3f&in avPos)",
			  +[](S n, bool loop, float vol, int type, const cVector3f &) { return cSomaSoundEvents::Get()->PlayGui(n, vol, type, loop); });
	SOMA_FUNC(e, "cSoundEntry@ cSound_PlaySoundEvent(const tString&in asInternalPath,float afVolume,const cVector3f&in avPos,const cVector3f&in avOrientation, bool abNonBlockLoad)",
			  +[](S n, float vol, const cVector3f &pos, const cVector3f &, bool) { return cSomaSoundEvents::Get()->Play3D(n, vol, pos, eSoundEntryType_World); });
	SOMA_FUNC(e, "cSoundEntry@ cSound_Play3D(const tString&in asName,bool abLoop,float afVolume,const cVector3f&in avPos, float afMinDist,float afMaxDist, eSoundEntryType aEntryType, bool abRelative, int alPriorityModifier, bool abStream, bool abNonBlockedLoad)",
			  +[](S n, bool loop, float vol, const cVector3f &pos, float, float, int type, bool, int, bool, bool) {
				  Inst *p = cSomaSoundEvents::Get()->Play3D(n, vol, pos, type);
				  return p ? p : cSomaSoundEvents::Get()->PlayGui(n, vol, type, loop);
			  });
	SOMA_FUNC(e, "void cSound_PreloadProject(const tString&in asName, bool abNonBlockingLoad)", +[](S n, bool) { cSomaSoundEvents::Get()->PreloadProject(n); });
	SOMA_FUNC(e, "void cSound_PreloadGroup(const tString&in asInternalPath, bool abNonBlockingLoad, bool abSubGroups)", +[](S, bool, bool) {});
	SOMA_FUNC(e, "int cSound_SetGlobalVolume(float afVolume, uint aAffectedTypes, int alId)", +[](float v, asUINT types, int id) {
		return Handler()->SetGlobalVolume(v, types, id);
	});
	SOMA_FUNC(e, "float cSound_GetGlobalVolumeFromId(int alId)", +[](int id) {
		cMultipleSettingsHandler::cGSEntry *pEntry = Handler()->GetGlobalVolumeSettingsHandler()->GetEntry(id, false);
		return pEntry ? pEntry->GetVal() : 1.0f;
	});
	SOMA_FUNC(e, "int cSound_FadeGlobalVolume(float afDestVolume, float afSpeed, uint aAffectedTypes, int alId, bool abDestroyIdAtDest)",
			  +[](float v, float speed, asUINT types, int id, bool destroy) { return Handler()->FadeGlobalVolume(v, speed, types, id, destroy); });
	SOMA_FUNC(e, "int cSound_SetGlobalSpeed(float afSpeed, uint aAffectedTypes, int alId)", +[](float v, asUINT types, int id) {
		return Handler()->SetGlobalSpeed(v, types, id);
	});
	SOMA_FUNC(e, "float cSound_GetGlobalSpeedFromId(int alId)", +[](int id) {
		cMultipleSettingsHandler::cGSEntry *pEntry = Handler()->GetGlobalSpeedSettingsHandler()->GetEntry(id, false);
		return pEntry ? pEntry->GetVal() : 1.0f;
	});
	SOMA_FUNC(e, "int cSound_FadeGlobalSpeed(float afDestSpeed, float afSpeed, uint aAffectedTypes, int alId, bool abDestroyIdAtDest)",
			  +[](float v, float speed, asUINT types, int id, bool destroy) { return Handler()->FadeGlobalSpeed(v, speed, types, id, destroy); });
	SOMA_FUNC(e, "void cSound_FadeOutAll(uint aTypes, float afFadeSpeed, bool abDisableStop)", +[](asUINT types, float speed, bool) {
		cSomaSoundEvents::Get()->FadeOutAll(types, speed);
		Handler()->FadeOutAll(types, speed, false);
	});
}
