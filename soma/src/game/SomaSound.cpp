#include "SomaSound.h"
#include "SomaBase.h"
#include "SomaFsb.h"
#include "SomaScriptBind.h"
#include "SomaLuxEntity.h"

#include <cmath>
#include <functional>

cSomaSoundEvents *cSomaSoundEvents::Get()
{
	static cSomaSoundEvents events;
	return &events;
}

// Element text is exposed as the "_Text" attribute
static tString Text(cXmlElement *apElem, const char *apChild, const tString &asDefault = "")
{
	cXmlElement *pChild = apElem ? apElem->GetFirstElement(apChild) : NULL;
	return pChild ? pChild->GetAttributeString("_Text", asDefault) : asDefault;
}

static std::vector<cXmlElement *> Children(cXmlElement *apElem, const char *apName)
{
	std::vector<cXmlElement *> v;
	cXmlNodeListIterator it = apElem->GetChildIterator();
	while (it.HasNext())
		if (cXmlElement *p = it.Next()->ToElement())
			if (p->GetValue() == apName)
				v.push_back(p);
	return v;
}

void cSomaSoundEvents::LoadProject(const tString &asProject)
{
	tString sKey = cString::ToLowerCase(asProject);
	if (msetProjects.insert(sKey).second == false)
		return;
	cResources *pRes = gpSomaBase->mpEngine->GetResources();
	iXmlDocument *pDoc = pRes->LoadXmlDocument(asProject + ".fdp");
	if (pDoc == NULL)
	{
		Warning("SOMA sound: no event project '%s'\n", asProject.c_str());
		return;
	}

	// Sound definitions: name -> waveforms
	std::map<tString, std::vector<cWave>> mapDefs;
	std::function<void(cXmlElement *)> readDefs = [&](cXmlElement *apFolder) {
		for (cXmlElement *pDef : Children(apFolder, "sounddef"))
		{
			std::vector<cWave> &v = mapDefs[Text(pDef, "name")];
			for (cXmlElement *pWave : Children(pDef, "waveform"))
			{
				tString sFile = Text(pWave, "filename");
				if (sFile != "")
					v.push_back(cWave{Text(pWave, "soundbankname"), cString::SetFileExt(cString::GetFileName(sFile), "")});
			}
		}
		for (cXmlElement *pSub : Children(apFolder, "sounddeffolder"))
			readDefs(pSub);
	};
	readDefs(pDoc);

	tString sProject = Text(pDoc, "name", asProject);
	int lCount = 0;
	std::function<void(cXmlElement *, const tString &)> readGroup = [&](cXmlElement *apGroup, const tString &asPath) {
		std::vector<cXmlElement *> vEvents = Children(apGroup, "event");
		for (cXmlElement *pSimple : Children(apGroup, "simpleevent"))
			for (cXmlElement *pEvent : Children(pSimple, "event"))
				vEvents.push_back(pEvent);
		for (cXmlElement *pEvent : vEvents)
		{
			cEvent ev;
			ev.mfVolume = std::pow(10.0f, cString::ToFloat(Text(pEvent, "volume_db", "0").c_str(), 0) / 20.0f);
			ev.mb3D = Text(pEvent, "mode") == "x_3d";
			ev.mfMinDist = cString::ToFloat(Text(pEvent, "mindistance", "1").c_str(), 1);
			ev.mfMaxDist = cString::ToFloat(Text(pEvent, "maxdistance", "20").c_str(), 20);
			ev.mbLoop = Text(pEvent, "oneshot", "Yes") == "No";
			for (cXmlElement *pLayer : Children(pEvent, "layer"))
				for (cXmlElement *pSound : Children(pLayer, "sound"))
				{
					auto it = mapDefs.find(Text(pSound, "name"));
					if (it != mapDefs.end())
						ev.mvWaves.insert(ev.mvWaves.end(), it->second.begin(), it->second.end());
				}
			mmapEvents[cString::ToLowerCase(asPath + "/" + Text(pEvent, "name"))] = ev;
			++lCount;
		}
		for (cXmlElement *pSub : Children(apGroup, "eventgroup"))
			readGroup(pSub, asPath + "/" + Text(pSub, "name"));
	};
	readGroup(pDoc, sProject);
	pRes->DestroyXmlDocument(pDoc);

	// One pass per bank: banks are large and read whole
	tWString sCacheDir = cSomaFsb::GetCacheDir(_W("events"));
	std::map<tString, std::set<tString>> mapByBank;
	for (auto &it : mmapEvents)
		if (it.first.compare(0, sKey.size() + 1, sKey + "/") == 0)
			for (const cWave &w : it.second.mvWaves)
				mapByBank[w.msBank].insert(w.msSample);
	std::map<tString, tString> mapFiles;
	for (auto &bank : mapByBank)
	{
		std::map<tString, tString> mapBankFiles;
		cSomaFsb::ExtractSamples(pRes, bank.first + ".fsb", sCacheDir, bank.first + "__", std::vector<tString>(bank.second.begin(), bank.second.end()), mapBankFiles);
		for (auto &f : mapBankFiles)
			mapFiles[bank.first + "__" + f.first] = f.second;
	}
	for (auto &it : mmapEvents)
		if (it.first.compare(0, sKey.size() + 1, sKey + "/") == 0 && it.second.mvFiles.empty())
			for (const cWave &w : it.second.mvWaves)
			{
				auto f = mapFiles.find(w.msBank + "__" + w.msSample);
				if (f != mapFiles.end())
					it.second.mvFiles.push_back(f->second);
			}
	// The file searcher indexes a directory when it is added
	pRes->AddResourceDir(sCacheDir, false);
	Log("SOMA sound: %d events in project '%s', %d samples\n", lCount, sProject.c_str(), (int)mapFiles.size());
}

void cSomaSoundEvents::PreloadProject(const tString &asProject)
{
	LoadProject(asProject);
}

cSoundEntityData *cSomaSoundEvents::Resolve(const tString &asEvent)
{
	if (asEvent.empty() || cString::GetFileExt(asEvent) != "")
		return NULL;
	tString sKey = cString::ToLowerCase(asEvent);
	size_t lSlash = sKey.find('/');
	if (lSlash == tString::npos)
		return NULL;
	LoadProject(asEvent.substr(0, lSlash));
	auto it = mmapEvents.find(sKey);
	if (it == mmapEvents.end())
		return NULL;
	cEvent &ev = it->second;
	if (ev.mpData)
		return ev.mpData;

	cResources *pRes = gpSomaBase->mpEngine->GetResources();
	if (ev.mvFiles.empty())
	{
		if (ev.mvWaves.empty() == false)
			Warning("SOMA sound: event '%s' has no playable samples\n", asEvent.c_str());
		return NULL;
	}

	cSoundEntityData *pData = hplNew(cSoundEntityData, (sKey, pRes, gpSomaBase->mpEngine->GetSound()));
	for (const tString &f : ev.mvFiles)
		pData->AddSoundName(f, eSoundEntityType_Main);
	pData->SetVolume(ev.mfVolume);
	pData->SetMinDistance(ev.mfMinDist);
	pData->SetMaxDistance(ev.mfMaxDist);
	pData->SetLoop(ev.mbLoop);
	pData->SetUse3D(ev.mb3D);
	pData->SetStream(false);
	pData->SetFadeStart(false);
	pData->SetFadeStop(false);
	ev.mpData = pData;
	return pData;
}

cSoundEntry *cSomaSoundEvents::PlayGui(const tString &asEvent, float afVolume, int alEntryType)
{
	cSoundHandler *pHandler = gpSomaBase->mpEngine->GetSound()->GetSoundHandler();
	if (cString::GetFileExt(asEvent) != "")
		return pHandler->PlayGui(asEvent, false, afVolume, cVector3f(0, 0, 1), (eSoundEntryType)alEntryType);
	if (Resolve(asEvent) == NULL)
		return NULL;
	cEvent &ev = mmapEvents[cString::ToLowerCase(asEvent)];
	const tString &sFile = ev.mvFiles[cMath::RandRectl(0, (int)ev.mvFiles.size() - 1)];
	return pHandler->PlayGui(sFile, ev.mbLoop, afVolume * ev.mfVolume, cVector3f(0, 0, 1), (eSoundEntryType)alEntryType);
}

//---------------------------------------

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

void RegisterMusicNatives(asIScriptEngine *e)
{
	typedef const tString &S;
	SOMA_FUNC(e, "cLuxMusicHandler@ cLux_GetMusicHandler()", +[]() { return (void *)&gMusicTag; });
	SOMA_METHOD(e, "cLuxMusicHandler",
				"void Play(const tString &in asFile, bool abLoop,float afVolume, float afFreq, float afVolumeFadeTime, float afFreqFadeTime, int alPrio, bool abResume, bool abSpecialEffect)",
				+[](void *, S f, bool loop, float vol, float, float fade, float, int prio, bool resume, bool) {
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
				});
	SOMA_METHOD(e, "cLuxMusicHandler", "void Stop(float afFadeTime, int alPrio)", +[](void *, float fade, int prio) {
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
	});
}
} // namespace

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
	SOMA_METHOD(e, "cSoundEntity", "void FadeIn(float afSpeed, float afTargetVol)", +[](cSoundEntity *o, float speed, float vol) {
		o->Play(false);
		if (cSoundEntry *pEntry = o->GetSoundEntry(eSoundEntityType_Main, false))
		{
			if (speed > 0)
				pEntry->FadeIn(vol, speed);
			else
				pEntry->SetVolumeMul(vol);
		}
	});
	SOMA_METHOD(e, "cSoundEntity", "bool IsOneShot()", +[](cSoundEntity *o) { return o->GetData() == NULL || o->GetData()->GetLoop() == false; });
	SOMA_METHOD(e, "cWorld", "cSoundEntity@ CreateSoundEntityEx(const tString &in asName,const tString &in asSoundDataFile, bool abRemoveWhenOver, bool abNonBlockLoad)",
				+[](cWorld *w, S n, S file, bool remove, bool) -> cSoundEntity * {
					return (file.empty() ? NULL : w->CreateSoundEntity(n, file, remove));
				});
	SOMA_METHOD(e, "cWorld", "tID CreateSoundEntityID(const tString &in asName,const tString &in asSoundDataFile, bool abRemoveWhenOver)",
				+[](cWorld *w, S n, S file, bool remove) {
					return SomaObjectID((file.empty() ? NULL : w->CreateSoundEntity(n, file, remove)), "cSoundEntity");
				});
	SOMA_METHOD(e, "cWorld", "tID CreateSoundEntityExID(const tString &in asName,const tString &in asSoundDataFile, bool abRemoveWhenOver, bool abNonBlockLoad)",
				+[](cWorld *w, S n, S file, bool remove, bool) {
					return SomaObjectID((file.empty() ? NULL : w->CreateSoundEntity(n, file, remove)), "cSoundEntity");
				});
	SOMA_FUNC(e, "bool cLux_PlayGuiSoundData(const tString&in asName, eSoundEntryType aDestType, float afVolMul, bool abSkipPreviousRandom)",
			  +[](S n, int type, float vol, bool) { return cSomaSoundEvents::Get()->PlayGui(n, vol, type) != NULL; });
	SOMA_FUNC(e, "bool cLux_PlayGuiSoundDataEx(const tString&in asName, eSoundEntryType aDestType, float afVolMul, bool abSkipPreviousRandom, cLuxSoundExtraData @apExtraData)",
			  +[](S n, int type, float vol, bool, char *pExtra) {
				  cSoundEntry *pEntry = cSomaSoundEvents::Get()->PlayGui(n, vol, type);
				  if (pExtra)
					  *(cSoundEntry **)(pExtra + 32) = pEntry;
				  return pEntry != NULL;
			  });
	SOMA_FUNC(e, "void cSound_FadeMusicVolumeMul(float afDest, float afSpeed)",
			  +[](float d, float sp) { gpSomaBase->mpEngine->GetSound()->GetMusicHandler()->FadeVolumeMul(d, sp); });
	SOMA_FUNC(e, "float cSound_GetMusicVolumeMul()", +[]() { return gpSomaBase->mpEngine->GetSound()->GetMusicHandler()->GetVolumeMul(); });
	RegisterMusicNatives(e);
	SOMA_FUNC(e, "cSoundEntry@ cSound_GetEntry(const tString&in asName)", +[](S n) -> cSoundEntry * {
		for (cSoundEntry *p : *gpSomaBase->mpEngine->GetSound()->GetSoundHandler()->GetEntryList())
			if (p->GetName() == n)
				return p;
		return NULL;
	});
	SOMA_METHOD(e, "cSoundEntry", "void Stop(bool abPlayEnd)", +[](cSoundEntry *p, bool) { p->Stop(); });
	SOMA_METHOD(e, "cSoundEntry", "void FadeIn(float afVolumeMul,float afSpeed)", +[](cSoundEntry *p, float v, float sp) { p->FadeIn(v, sp); });
	SOMA_METHOD(e, "cSoundEntry", "float GetVolumeMul()", +[](cSoundEntry *p) { return p->GetVolumeMul(); });
	SOMA_FUNC(e, "cSoundEntry@ cSound_PlayGui(const tString&in asName, bool abLoop, float afVolume, const cVector3f&in avPos, eSoundEntryType aEntryType)",
			  +[](S n, bool, float vol, const cVector3f &, int type) { return cSomaSoundEvents::Get()->PlayGui(n, vol, type); });
	SOMA_FUNC(e, "cSoundEntry@ cSound_PlayGuiStream(const tString&in asFileName, bool abLoop, float afVolume, const cVector3f&in avPos, eSoundEntryType aEntryType)",
			  +[](S n, bool loop, float vol, const cVector3f &pos, int type) {
				  return gpSomaBase->mpEngine->GetSound()->GetSoundHandler()->PlayGuiStream(n, loop, vol, pos, (eSoundEntryType)type);
			  });
	SOMA_FUNC(e, "void cSound_PreloadProject(const tString&in asName, bool abNonBlockingLoad)", +[](S n, bool) { cSomaSoundEvents::Get()->PreloadProject(n); });
	SOMA_FUNC(e, "void cSound_PreloadGroup(const tString&in asInternalPath, bool abNonBlockingLoad, bool abSubGroups)", +[](S, bool, bool) {});
	SOMA_FUNC(e, "int cSound_SetGlobalVolume(float afVolume, uint aAffectedTypes, int alId)", +[](float v, asUINT types, int id) {
		return gpSomaBase->mpEngine->GetSound()->GetSoundHandler()->SetGlobalVolume(v, types, id);
	});
	SOMA_FUNC(e, "float cSound_GetGlobalVolumeFromId(int alId)", +[](int id) {
		cMultipleSettingsHandler *h = gpSomaBase->mpEngine->GetSound()->GetSoundHandler()->GetGlobalVolumeSettingsHandler();
		cMultipleSettingsHandler::cGSEntry *pEntry = h->GetEntry(id, false);
		return pEntry ? pEntry->GetVal() : 1.0f;
	});
	SOMA_FUNC(e, "int cSound_FadeGlobalVolume(float afDestVolume, float afSpeed, uint aAffectedTypes, int alId, bool abDestroyIdAtDest)",
			  +[](float v, float speed, asUINT types, int id, bool destroy) {
				  return gpSomaBase->mpEngine->GetSound()->GetSoundHandler()->FadeGlobalVolume(v, speed, types, id, destroy);
			  });
	SOMA_FUNC(e, "void cSound_FadeOutAll(uint aTypes, float afFadeSpeed, bool abDisableStop)",
			  +[](asUINT types, float speed, bool) { gpSomaBase->mpEngine->GetSound()->GetSoundHandler()->FadeOutAll(types, speed, false); });
}
