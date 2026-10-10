#include "SomaSound.h"
#include "SomaBase.h"
#include "SomaFsb.h"
#include "SomaScriptBind.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "impl/tinyXML/tinyxml.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>

typedef cSomaSoundEvents::cEvent cEvent;

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

float DbToGain(float afDb) { return std::pow(10.0f, afDb / 20.0f); }
float RandDb(float afRandDb) { return afRandDb < 0 ? DbToGain(cMath::RandRectf(afRandDb, 0)) : 1.0f; }
float RandRange(float afMin, float afMax) { return afMax > afMin ? cMath::RandRectf(afMin, afMax) : afMin; }
} // namespace

cSomaSoundEvents *cSomaSoundEvents::Get()
{
	static cSomaSoundEvents events;
	return &events;
}

static tString Text(TiXmlElement *apElem, const char *apChild, const tString &asDefault = "")
{
	TiXmlElement *pChild = apElem ? apElem->FirstChildElement(apChild) : NULL;
	return pChild && pChild->GetText() ? tString(pChild->GetText()) : asDefault;
}

static float Num(TiXmlElement *apElem, const char *apChild, float afDefault = 0)
{
	return cString::ToFloat(Text(apElem, apChild).c_str(), afDefault);
}

static std::vector<TiXmlElement *> Children(TiXmlElement *apElem, const char *apName)
{
	std::vector<TiXmlElement *> v;
	for (TiXmlElement *p = apElem->FirstChildElement(apName); p; p = p->NextSiblingElement(apName))
		v.push_back(p);
	return v;
}

float cSomaSoundEvents::cEnvelope::Eval(float afX) const
{
	if (mvPoints.empty())
		return 1;
	if (afX <= mvPoints.front().x)
		return mvPoints.front().y;
	for (size_t i = 1; i < mvPoints.size(); ++i)
		if (afX <= mvPoints[i].x)
		{
			const cVector2f &a = mvPoints[i - 1], &b = mvPoints[i];
			float t = b.x > a.x ? (afX - a.x) / (b.x - a.x) : 1;
			return a.y + (b.y - a.y) * t;
		}
	return mvPoints.back().y;
}

bool cSomaSoundEvents::cEvent::HasFiles() const
{
	for (const cSoundDef &d : mvDefs)
		for (const tString &f : d.mvFiles)
			if (f != "")
				return true;
	return false;
}

void cSomaSoundEvents::LoadProject(const tString &asProject)
{
	tString sKey = cString::ToLowerCase(asProject);
	if (msetProjects.insert(sKey).second == false)
		return;
	cResources *pRes = gpSomaBase->mpEngine->GetResources();
	TiXmlDocument doc;
	FILE *pFile = cPlatform::OpenFile(pRes->GetFileSearcher()->GetFilePath(asProject + ".fdp"), _W("rb"));
	bool bLoaded = pFile && doc.LoadFile(pFile);
	if (pFile)
		fclose(pFile);
	TiXmlElement *pDoc = doc.RootElement();
	if (bLoaded == false || pDoc == NULL)
	{
		Warning("SOMA sound: no event project '%s'\n", asProject.c_str());
		return;
	}

	std::map<tString, cSoundDef> mapDefs;
	std::function<void(TiXmlElement *)> readDefs = [&](TiXmlElement *apFolder) {
		for (TiXmlElement *pDef : Children(apFolder, "sounddef"))
		{
			cSoundDef &d = mapDefs[Text(pDef, "name")];
			tString sType = Text(pDef, "type");
			d.mbSequential = sType.compare(0, 10, "sequential") == 0;
			d.mfVolume = DbToGain(Num(pDef, "volume_db"));
			d.mfVolumeRandDb = Num(pDef, "volume_randomization");
			d.mfPitch = Num(pDef, "pitch");
			d.mfPitchRand = Num(pDef, "pitch_randomization");
			d.mfSpawnMin = Num(pDef, "spawntime_min") / 1000.0f;
			d.mfSpawnMax = Num(pDef, "spawntime_max") / 1000.0f;
			d.mlSpawnCount = std::max(1, (int)Num(pDef, "spawn_max", 1));
			d.mfDelayMin = Num(pDef, "trigger_delay_min") / 1000.0f;
			d.mfDelayMax = Num(pDef, "trigger_delay_max") / 1000.0f;
			for (TiXmlElement *pWave : Children(pDef, "waveform"))
			{
				tString sFile = Text(pWave, "filename");
				if (sFile != "")
					d.mvWaves.push_back(cWave{Text(pWave, "soundbankname"), cString::SetFileExt(cString::GetFileName(sFile), "")});
			}
		}
		for (TiXmlElement *pSub : Children(apFolder, "sounddeffolder"))
			readDefs(pSub);
	};
	readDefs(pDoc);

	static const std::map<tString, int> mapBehavior = {
		{"Steal_oldest", eMaxBehavior_StealOldest}, {"Steal_newest", eMaxBehavior_StealNewest}, {"Steal_quietest", eMaxBehavior_StealQuietest},
		{"Just_fail", eMaxBehavior_JustFail}, {"Just_fail_if_quietest", eMaxBehavior_JustFailIfQuietest}};
	static const std::map<tString, int> mapRolloff = {
		{"Linear", eRolloff_Linear}, {"LinearSquare", eRolloff_LinearSquare}, {"Logarithmic", eRolloff_Log}, {"Custom", eRolloff_Custom}};

	tString sProject = Text(pDoc, "name", asProject);
	int lCount = 0;
	std::function<void(TiXmlElement *, const tString &)> readGroup = [&](TiXmlElement *apGroup, const tString &asPath) {
		std::vector<TiXmlElement *> vEvents = Children(apGroup, "event");
		for (TiXmlElement *pSimple : Children(apGroup, "simpleevent"))
			for (TiXmlElement *pEvent : Children(pSimple, "event"))
				vEvents.push_back(pEvent);
		for (TiXmlElement *pEvent : vEvents)
		{
			cEvent ev;
			ev.msName = asPath + "/" + Text(pEvent, "name");
			ev.mfVolume = DbToGain(Num(pEvent, "volume_db"));
			ev.mfVolumeRandDb = Num(pEvent, "volume_randomization");
			ev.mfPitch = Num(pEvent, "pitch");
			ev.mfPitchRand = Num(pEvent, "pitch_randomization");
			ev.mb3D = Text(pEvent, "mode") == "x_3d";
			ev.mfPanLevel = Num(pEvent, "panlevel3d", 1);
			ev.mbOneShot = Text(pEvent, "oneshot", "Yes") != "No";
			ev.mfMinDist = Num(pEvent, "mindistance", 1);
			ev.mfMaxDist = Num(pEvent, "maxdistance", 20);
			auto rolloff = mapRolloff.find(Text(pEvent, "rolloff"));
			ev.mlRolloff = rolloff != mapRolloff.end() ? rolloff->second : eRolloff_Log;
			ev.mlMaxPlaybacks = (int)Num(pEvent, "maxplaybacks");
			auto behavior = mapBehavior.find(Text(pEvent, "maxplaybacks_behavior"));
			ev.mlMaxBehavior = behavior != mapBehavior.end() ? behavior->second : eMaxBehavior_StealOldest;
			ev.mfFadeIn = Num(pEvent, "fadein_time") / 1000.0f;
			ev.mfFadeOut = Num(pEvent, "fadeout_time") / 1000.0f;

			for (TiXmlElement *pParam : Children(pEvent, "parameter"))
			{
				cParam p;
				p.msName = Text(pParam, "name");
				p.mfMin = Num(pParam, "rangemin");
				p.mfMax = Num(pParam, "rangemax", 1);
				p.mfVelocity = Num(pParam, "velocity");
				p.mfSeek = Num(pParam, "seekspeed");
				p.mlLoopMode = (int)Num(pParam, "loopmode");
				p.mlBuiltin = p.msName == "(distance)" ? 1 : p.msName == "(listener angle)" ? 2 : p.msName == "(event angle)" ? 3 : 0;
				ev.mvParams.push_back(p);
			}
			auto paramIdx = [&](const tString &asName) {
				for (size_t i = 0; i < ev.mvParams.size(); ++i)
					if (ev.mvParams[i].msName == asName)
						return (int)i;
				return -1;
			};

			std::map<tString, int> mapLocalDefs;
			for (TiXmlElement *pLayer : Children(pEvent, "layer"))
			{
				if (Text(pLayer, "mute") == "1")
					continue;
				cLayer layer;
				layer.mlParam = paramIdx(Text(pLayer, "controlparameter"));
				for (TiXmlElement *pSound : Children(pLayer, "sound"))
				{
					auto def = mapDefs.find(Text(pSound, "name"));
					if (def == mapDefs.end())
						continue;
					auto local = mapLocalDefs.find(def->first);
					if (local == mapLocalDefs.end())
					{
						local = mapLocalDefs.insert({def->first, (int)ev.mvDefs.size()}).first;
						ev.mvDefs.push_back(def->second);
					}
					cLayerSound s;
					s.mlDef = local->second;
					s.mfVolume = Num(pSound, "volume", 1);
					s.mlLoopMode = (int)Num(pSound, "loopmode", 1);
					s.mfX0 = Num(pSound, "x");
					s.mfX1 = s.mfX0 + Num(pSound, "width", 1);
					layer.mvSounds.push_back(s);
				}
				for (TiXmlElement *pEnv : Children(pLayer, "envelope"))
				{
					static const std::map<tString, int> mapDsp = {{"Volume", eDsp_Volume}, {"FMOD ParamEQ", eDsp_EqGain}, {"FMOD Lowpass", eDsp_Lowpass},
						{"FMOD Lowpass Simple", eDsp_Lowpass}, {"FMOD Highpass", eDsp_Highpass}, {"FMOD Highpass Simple", eDsp_Highpass}, {"Pitch", eDsp_Pitch}};
					auto dsp = mapDsp.find(Text(pEnv, "dsp_name"));
					int lParamIdx = (int)Num(pEnv, "dsp_paramindex");
					if (dsp == mapDsp.end() || lParamIdx != (dsp->second == eDsp_EqGain ? 2 : 0) || Text(pEnv, "mute") == "1")
						continue;
					cEnvelope env;
					env.mlDsp = dsp->second;
					env.mlParam = paramIdx(Text(pEnv, "controlparameter"));
					for (TiXmlElement *pPoint : Children(pEnv, "point"))
					{
						tStringVec vNums;
						cString::GetStringVec(pPoint->GetText() ? pPoint->GetText() : "", vNums, NULL);
						if (vNums.size() >= 2)
							env.mvPoints.push_back(cVector2f(cString::ToFloat(vNums[0].c_str(), 0), cString::ToFloat(vNums[1].c_str(), 1)));
					}
					layer.mvEnvelopes.push_back(env);
				}
				if (layer.mvSounds.empty() == false)
					ev.mvLayers.push_back(layer);
			}
			mmapEvents[cString::ToLowerCase(ev.msName)] = ev;
			++lCount;
		}
		for (TiXmlElement *pSub : Children(apGroup, "eventgroup"))
			readGroup(pSub, asPath + "/" + Text(pSub, "name"));
	};
	readGroup(pDoc, sProject);

	// One pass per bank: banks are large and read whole
	tWString sCacheDir = cSomaFsb::GetCacheDir(_W("events-v4"));
	std::map<tString, std::set<tString>> mapByBank;
	for (auto &it : mmapEvents)
		if (it.first.compare(0, sKey.size() + 1, sKey + "/") == 0)
			for (const cSoundDef &d : it.second.mvDefs)
				for (const cWave &w : d.mvWaves)
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
		if (it.first.compare(0, sKey.size() + 1, sKey + "/") == 0 && it.second.mbLoaded == false)
		{
			for (cSoundDef &d : it.second.mvDefs)
				for (const cWave &w : d.mvWaves)
				{
					auto f = mapFiles.find(w.msBank + "__" + w.msSample);
					d.mvFiles.push_back(f != mapFiles.end() ? f->second : "");
				}
			it.second.mbLoaded = true;
		}
	cFileSearcher *pSearcher = pRes->GetFileSearcher();
	if (std::any_of(mapFiles.begin(), mapFiles.end(), [&](auto &f) { return pSearcher->GetFilePath(f.second) == _W(""); }))
		pRes->AddResourceDir(sCacheDir, false);
	Log("SOMA sound: %d events in project '%s', %d samples\n", lCount, sProject.c_str(), (int)mapFiles.size());
}

void cSomaSoundEvents::PreloadProject(const tString &asProject)
{
	LoadProject(asProject);
}

cEvent *cSomaSoundEvents::GetEvent(const tString &asName)
{
	if (asName.empty() || cString::GetFileExt(asName) != "")
		return NULL;
	tString sKey = cString::ToLowerCase(asName);
	size_t lSlash = sKey.find('/');
	if (lSlash == tString::npos)
		return NULL;
	LoadProject(asName.substr(0, lSlash));
	auto it = mmapEvents.find(sKey);
	return it == mmapEvents.end() ? NULL : &it->second;
}

cEvent *cSomaSoundEvents::FileEvent(const tString &asFile, bool abLoop, bool abStream)
{
	tString sKey = "file:" + cString::ToLowerCase(asFile) + (abLoop ? ":loop" : "") + (abStream ? ":stream" : "");
	cEvent &ev = mmapEvents[sKey];
	if (ev.mbLoaded)
		return &ev;
	ev.msName = asFile;
	ev.mb3D = false;
	ev.mbOneShot = abLoop == false;
	ev.mbStream = abStream;
	ev.mvDefs.resize(1);
	ev.mvDefs[0].mvFiles.push_back(asFile);
	ev.mvLayers.resize(1);
	ev.mvLayers[0].mvSounds.resize(1);
	ev.mvLayers[0].mvSounds[0].mlDef = 0;
	ev.mvLayers[0].mvSounds[0].mlLoopMode = abLoop ? 0 : 1;
	ev.mbLoaded = true;
	return &ev;
}

cSoundEntityData *cSomaSoundEvents::Resolve(const tString &asEvent)
{
	cEvent *pEvent = GetEvent(asEvent);
	if (pEvent == NULL)
		return NULL;
	if (pEvent->mpData)
		return pEvent->mpData;
	if (pEvent->HasFiles() == false)
	{
		if (pEvent->mvDefs.empty() == false)
			Warning("SOMA sound: event '%s' has no playable samples\n", asEvent.c_str());
		return NULL;
	}

	cResources *pRes = gpSomaBase->mpEngine->GetResources();
	cSoundEntityData *pData = hplNew(cSoundEntityData, (cString::ToLowerCase(asEvent), pRes, gpSomaBase->mpEngine->GetSound()));
	for (const cSoundDef &d : pEvent->mvDefs)
		for (const tString &f : d.mvFiles)
			if (f != "")
				pData->AddSoundName(f, eSoundEntityType_Main);
	pData->SetVolume(1);
	pData->SetMinDistance(pEvent->mfMinDist);
	pData->SetMaxDistance(pEvent->mfMaxDist);
	pData->SetLoop(pEvent->mbOneShot == false);
	pData->SetUse3D(pEvent->mb3D);
	pData->SetStream(false);
	pData->SetFadeStart(false);
	pData->SetFadeStop(false);
	pEvent->mpData = pData;
	mmapDataEvents[pData] = pEvent;

	static bool bFactory = false;
	if (bFactory == false)
	{
		bFactory = true;
		cSoundEntity::SetEventFactory(+[](cSoundEntity *apEntity) -> iSoundEntityEvent * {
			auto it = Get()->mmapDataEvents.find(apEntity->GetData());
			if (it == Get()->mmapDataEvents.end())
				return NULL;
			return hplNew(cSomaSoundInstance, (it->second, apEntity->GetName(), apEntity, eSoundEntryType_World));
		});
	}
	return pData;
}

cSomaSoundInstance *cSomaSoundEvents::PlayGui(const tString &asEvent, float afVolume, int alEntryType, bool abLoop, bool abStream)
{
	cEvent *pEvent = GetEvent(asEvent);
	if (pEvent == NULL && cString::GetFileExt(asEvent) == "" && asEvent.find('/') != tString::npos)
		return NULL;
	if (pEvent == NULL)
		pEvent = FileEvent(asEvent, abLoop, abStream);
	if (pEvent->HasFiles() == false)
		return NULL;
	cSomaSoundInstance *pInst = hplNew(cSomaSoundInstance, (pEvent, pEvent->msName, NULL, (eSoundEntryType)alEntryType));
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
	cEvent *pEvent = GetEvent(asEvent);
	if (pEvent == NULL || pEvent->HasFiles() == false)
		return NULL;
	cSomaSoundInstance *pInst = hplNew(cSomaSoundInstance, (pEvent, pEvent->msName, NULL, (eSoundEntryType)alEntryType));
	pInst->SetVolume(afVolume);
	pInst->SetPosition(avPos);
	pInst->mb3DPlay = pEvent->mb3D;
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

cSomaSoundInstance::cSomaSoundInstance(cEvent *apEvent, const tString &asName, cSoundEntity *apEntity, eSoundEntryType aType)
	: mpEvent(apEvent), msName(asName), mpEntity(apEntity), mType(aType)
{
	cSomaSoundEvents *pEvents = cSomaSoundEvents::Get();
	mlId = pEvents->mlNextId++;
	pEvents->mlstInstances.push_back(this);
	pEvents->mmapLive[this] = mlId;
	if (pEvents->mbUpdaterAdded == false)
	{
		pEvents->mbUpdaterAdded = true;
		gpSomaBase->mpEngine->GetUpdater()->AddGlobalUpdate(hplNew(cSomaSoundUpdater, ()));
	}
	mb3DPlay = mpEntity && mpEvent->mb3D;
	for (const cSomaSoundEvents::cParam &p : mpEvent->mvParams)
		mvParamValue.push_back(p.mfMin);
	mvParamTarget = mvParamValue;
}

cSomaSoundInstance::~cSomaSoundInstance()
{
	for (cVoice &v : mvVoices)
		if (Handler()->IsValid(v.mpEntry, v.mlEntryId))
			v.mpEntry->Stop();
	cSomaSoundEvents *pEvents = cSomaSoundEvents::Get();
	pEvents->mlstInstances.remove(this);
	pEvents->mmapLive.erase(this);
}

void cSomaSoundInstance::Play()
{
	if (mbStarted && mbStopped == false)
		return;
	StopNow();
	Start();
}

void cSomaSoundInstance::Start()
{
	cSomaSoundEvents *pEvents = cSomaSoundEvents::Get();
	if (mpEvent->mlMaxPlaybacks > 0)
	{
		std::vector<cSomaSoundInstance *> vSame;
		for (cSomaSoundInstance *p : pEvents->mlstInstances)
			if (p != this && p->mpEvent == mpEvent && p->mbStarted && p->mbStopped == false)
				vSame.push_back(p);
		if ((int)vSame.size() >= mpEvent->mlMaxPlaybacks)
		{
			cSomaSoundInstance *pSteal = NULL;
			switch (mpEvent->mlMaxBehavior)
			{
			case cSomaSoundEvents::eMaxBehavior_StealOldest: pSteal = vSame.front(); break;
			case cSomaSoundEvents::eMaxBehavior_StealNewest: pSteal = vSame.back(); break;
			case cSomaSoundEvents::eMaxBehavior_StealQuietest:
				pSteal = vSame.front();
				for (cSomaSoundInstance *p : vSame)
					if (p->mfAudibility < pSteal->mfAudibility)
						pSteal = p;
				break;
			default: break;
			}
			if (pSteal == NULL)
			{
				mbStarted = true;
				mbStopped = true;
				return;
			}
			pSteal->StopNow();
		}
	}
	mbStarted = true;
	mbStopped = false;
	mbStopAtFadeEnd = false;
	mfTime = 0;
	mfRandGain = RandDb(mpEvent->mfVolumeRandDb);
	mfRandPitch = cMath::RandRectf(-mpEvent->mfPitchRand, mpEvent->mfPitchRand);
	mvSlots.assign(mpEvent->mvLayers.size(), std::vector<cSlot>());
	for (size_t i = 0; i < mpEvent->mvLayers.size(); ++i)
		mvSlots[i].resize(mpEvent->mvLayers[i].mvSounds.size());
	if (mpEvent->mfFadeIn > 0)
	{
		mfFade = 0;
		mfFadeDest = 1;
		mfFadeSpeed = 1.0f / mpEvent->mfFadeIn;
	}
}

void cSomaSoundInstance::StopNow()
{
	for (cVoice &v : mvVoices)
		if (Handler()->IsValid(v.mpEntry, v.mlEntryId))
			v.mpEntry->Stop();
	mvVoices.clear();
	mbStopped = true;
	mfAudibility = 0;
}

void cSomaSoundInstance::Stop(bool abPlayEnd)
{
	if (mbStopDisabled || mbStopped)
		return;
	if (mpEvent->mfFadeOut > 0)
	{
		mbStopped = true;
		mbStopAtFadeEnd = true;
		mfFadeDest = 0;
		mfFadeSpeed = -1.0f / mpEvent->mfFadeOut;
		return;
	}
	StopNow();
}

void cSomaSoundInstance::FadeOut(float afSpeed)
{
	if (mbStopped || mbStopDisabled)
		return;
	if (afSpeed <= 0)
	{
		StopNow();
		return;
	}
	mbStopped = true;
	mbStopAtFadeEnd = true;
	mfFadeDest = 0;
	mfFadeSpeed = -afSpeed;
}

void cSomaSoundInstance::FadeInTo(float afVolumeMul, float afSpeed)
{
	if (mbStarted == false || mbStopped)
		Play();
	mbStopAtFadeEnd = false;
	if (afSpeed <= 0)
	{
		mfFade = mfFadeDest = afVolumeMul;
		mfFadeSpeed = 0;
		return;
	}
	mfFade = 0;
	mfFadeDest = afVolumeMul;
	mfFadeSpeed = afSpeed;
}

void cSomaSoundInstance::FadeVolumeMulTo(float afDest, float afSpeed)
{
	if (afSpeed <= 0)
	{
		SetVolumeMul(afDest);
		return;
	}
	mfVolumeMulDest = afDest;
	mfVolumeMulSpeed = std::fabs(afSpeed);
}

void cSomaSoundInstance::FadeSpeedMulTo(float afDest, float afSpeed)
{
	if (afSpeed <= 0)
	{
		SetSpeedMul(afDest);
		return;
	}
	mfSpeedMulDest = afDest;
	mfSpeedMulSpeed = std::fabs(afSpeed);
}

int cSomaSoundInstance::ParamIndex(const tString &asName)
{
	for (size_t i = 0; i < mpEvent->mvParams.size(); ++i)
		if (cString::ToLowerCase(mpEvent->mvParams[i].msName) == cString::ToLowerCase(asName))
			return (int)i;
	return -1;
}

void cSomaSoundInstance::SetParam(int alIdx, float afValue)
{
	if (alIdx < 0 || alIdx >= (int)mvParamValue.size())
		return;
	const cSomaSoundEvents::cParam &p = mpEvent->mvParams[alIdx];
	mvParamValue[alIdx] = mvParamTarget[alIdx] = cMath::Clamp(afValue, p.mfMin, p.mfMax);
}

float cSomaSoundInstance::GetParamValue(int alIdx)
{
	return alIdx >= 0 && alIdx < (int)mvParamValue.size() ? mvParamValue[alIdx] : 0;
}

const cSomaSoundEvents::cParam *cSomaSoundInstance::GetParamDef(int alIdx)
{
	return alIdx >= 0 && alIdx < (int)mpEvent->mvParams.size() ? &mpEvent->mvParams[alIdx] : NULL;
}

void cSomaSoundInstance::SetPaused(bool abX)
{
	mbPaused = abX;
	for (cVoice &v : mvVoices)
		if (Handler()->IsValid(v.mpEntry, v.mlEntryId))
			v.mpEntry->SetPaused(abX);
}

float cSomaSoundInstance::GetTotalTime()
{
	float fTotal = 0;
	for (cVoice &v : mvVoices)
		if (Handler()->IsValid(v.mpEntry, v.mlEntryId))
			fTotal = std::max(fTotal, (float)v.mpEntry->GetChannel()->GetTotalTime());
	return fTotal;
}

float cSomaSoundInstance::ParamNorm(int alIdx)
{
	const cSomaSoundEvents::cParam &p = mpEvent->mvParams[alIdx];
	return p.mfMax > p.mfMin ? (mvParamValue[alIdx] - p.mfMin) / (p.mfMax - p.mfMin) : 0;
}

cVector3f cSomaSoundInstance::SourcePos()
{
	return mpEntity ? mpEntity->GetWorldPosition() : mvPos;
}

// FMOD Ex stereo pans by constant power on sin(azimuth) with no rear or elevation cue; move the
// source to where OpenAL Soft's stereo panpot gives the same L/R ratio.
// ponytail: table measured from OpenAL Soft 1.24 panpot (4 deg steps); HRTF/UHJ output differs.
cVector3f cSomaSoundInstance::PanPos()
{
	static const float vQ[] = {0, .0897f, .175f, .263f, .346f, .429f, .510f, .585f, .656f, .725f, .789f, .846f, .895f, .938f, .973f, 1};
	iLowLevelSound *pLow = gpSomaBase->mpEngine->GetSound()->GetLowLevel();
	cVector3f vPos = SourcePos(), vListener = pLow->GetListenerPosition();
	cVector3f vDir = vPos - vListener;
	float fDist = vDir.Length();
	if (fDist < 1e-3f)
		return vPos;
	cVector3f vFwd = pLow->GetListenerForward() * -1, vRight = cMath::Vector3Cross(vFwd, pLow->GetListenerUp());
	float fPan = cMath::Vector3Dot(vDir, vRight) / fDist * mpEvent->mfPanLevel;
	float fAbs = std::min(std::fabs(fPan), 1.0f);
	float fL = std::sqrt((1 - fAbs) / 2), fR = std::sqrt((1 + fAbs) / 2), fQ = (fR - fL) / (fR + fL);
	size_t i = 1;
	while (i < 15 && vQ[i] < fQ)
		++i;
	float fAngle = cMath::ToRad(4 * (i - 1 + (fQ - vQ[i - 1]) / (vQ[i] - vQ[i - 1])));
	return vListener + (vFwd * std::cos(fAngle) + vRight * (std::sin(fAngle) * (fPan < 0 ? -1 : 1))) * fDist;
}

float cSomaSoundInstance::ListenerDistance()
{
	return cMath::Vector3Dist(gpSomaBase->mpEngine->GetSound()->GetLowLevel()->GetListenerPosition(), SourcePos());
}

float cSomaSoundInstance::DistanceGain(float afDist)
{
	float fMin = mpEntity ? mpEntity->GetMinDistance() : mpEvent->mfMinDist;
	float fMax = mpEntity ? mpEntity->GetMaxDistance() : mpEvent->mfMaxDist;
	if (afDist <= fMin)
		return 1;
	switch (mpEvent->mlRolloff)
	{
	case cSomaSoundEvents::eRolloff_Linear:
	case cSomaSoundEvents::eRolloff_LinearSquare:
	{
		float t = fMax > fMin ? cMath::Clamp(1 - (afDist - fMin) / (fMax - fMin), 0.0f, 1.0f) : 0;
		return mpEvent->mlRolloff == cSomaSoundEvents::eRolloff_Linear ? t : t * t;
	}
	case cSomaSoundEvents::eRolloff_Log: return fMin / std::min(afDist, std::max(fMax, fMin));
	default: return 1;
	}
}

bool cSomaSoundInstance::StartVoice(int alLayer, int alSound, bool abLoop)
{
	const cSomaSoundEvents::cLayerSound &snd = mpEvent->mvLayers[alLayer].mvSounds[alSound];
	cSomaSoundEvents::cSoundDef &def = mpEvent->mvDefs[snd.mlDef];
	int lNum = (int)def.mvFiles.size();
	if (lNum == 0)
		return false;
	int lIdx;
	if (def.mbSequential)
		lIdx = (def.mlLast + 1) % lNum;
	else
	{
		lIdx = cMath::RandRectl(0, lNum - 1);
		if (lNum > 1 && lIdx == def.mlLast)
			lIdx = (lIdx + 1 + cMath::RandRectl(0, lNum - 2)) % lNum;
	}
	def.mlLast = lIdx;
	const tString &sFile = def.mvFiles[lIdx];
	if (sFile == "")
		return false;

	cSoundEntry *pEntry;
	if (mb3DPlay)
		pEntry = Handler()->Play(sFile, abLoop, 0, PanPos(), 1e5f, 2e5f, mType, false, true, 0, mpEvent->mbStream);
	else if (mpEvent->mbStream)
		pEntry = Handler()->PlayGuiStream(sFile, abLoop, 0, cVector3f(0, 0, 1), mType);
	else
		pEntry = Handler()->PlayGui(sFile, abLoop, 0, cVector3f(0, 0, 1), mType);
	if (pEntry == NULL)
		return false;
	cVoice v;
	v.mpEntry = pEntry;
	v.mlEntryId = pEntry->GetId();
	v.mlLayer = alLayer;
	v.mlSound = alSound;
	v.mfGain = RandDb(def.mfVolumeRandDb);
	v.mfSpeed = std::pow(2.0f, def.mfPitch + cMath::RandRectf(-def.mfPitchRand, def.mfPitchRand));
	v.mbLoop = abLoop;
	mvVoices.push_back(v);
	return true;
}

void cSomaSoundInstance::Update(float afTimeStep)
{
	if (mbStarted == false)
	{
		if (mpEntity == NULL || mbStopped)
			return;
		Start();
	}
	if (mbPaused || (gpSomaBase->mbScriptGamePaused && (mType & 11)))
		return;
	if (mbStopped && mvVoices.empty())
		return;
	mfTime += afTimeStep;

	auto approach = [afTimeStep](float &afX, float afDest, float afSpeed) {
		if (afX < afDest)
			afX = std::min(afDest, afX + afSpeed * afTimeStep);
		else
			afX = std::max(afDest, afX - afSpeed * afTimeStep);
	};
	if (mfFadeSpeed != 0)
	{
		approach(mfFade, mfFadeDest, std::fabs(mfFadeSpeed));
		if (mfFade == mfFadeDest)
		{
			mfFadeSpeed = 0;
			if (mbStopAtFadeEnd)
			{
				StopNow();
				mfFade = mfFadeDest = 1;
				mbStopAtFadeEnd = false;
				return;
			}
		}
	}
	approach(mfVolumeMul, mfVolumeMulDest, mfVolumeMulSpeed);
	approach(mfSpeedMul, mfSpeedMulDest, mfSpeedMulSpeed);

	float fDist = mb3DPlay ? ListenerDistance() : 0;
	for (size_t i = 0; i < mvParamValue.size(); ++i)
	{
		const cSomaSoundEvents::cParam &p = mpEvent->mvParams[i];
		if (p.mlBuiltin == 1)
			mvParamValue[i] = cMath::Clamp(fDist, p.mfMin, p.mfMax);
		else if (p.mlBuiltin)
			mvParamValue[i] = p.mfMin;
		else if (p.mfVelocity != 0 && mbStopped == false)
		{
			float fRange = p.mfMax - p.mfMin;
			float fX = mvParamValue[i] + p.mfVelocity * afTimeStep;
			if (fX > p.mfMax)
			{
				if (p.mlLoopMode == 0 && fRange > 0)
					fX = p.mfMin + std::fmod(fX - p.mfMin, fRange);
				else
				{
					fX = p.mfMax;
					if (p.mlLoopMode == 2)
						Stop(false);
				}
			}
			mvParamValue[i] = fX;
		}
	}

	float fBase = mpEvent->mfVolume * mfRandGain * mfVolume * mfVolumeMul * mfFade;
	if (mpEntity)
		fBase *= mpEntity->GetVolume();
	if (mb3DPlay)
		fBase *= DistanceGain(fDist);
	float fEventSpeed = std::pow(2.0f, mpEvent->mfPitch + mfRandPitch) * mfSpeedMul;

	for (size_t i = 0; i < mvVoices.size();)
	{
		if (Handler()->IsValid(mvVoices[i].mpEntry, mvVoices[i].mlEntryId))
			++i;
		else
			mvVoices.erase(mvVoices.begin() + i);
	}

	bool bPending = false;
	std::vector<std::vector<float>> vGain(mpEvent->mvLayers.size());
	std::vector<cVector2f> vFilter(mpEvent->mvLayers.size(), cVector2f(1));
	std::vector<float> vSpeed(mpEvent->mvLayers.size(), 1);
	for (size_t l = 0; l < mpEvent->mvLayers.size(); ++l)
	{
		const cSomaSoundEvents::cLayer &layer = mpEvent->mvLayers[l];
		float fX = layer.mlParam >= 0 ? ParamNorm(layer.mlParam) : 0;
		float fLayerGain = fBase;
		for (const cSomaSoundEvents::cEnvelope &env : layer.mvEnvelopes)
		{
			float fY = env.Eval(env.mlParam >= 0 ? ParamNorm(env.mlParam) : fX);
			// EFX only has shelves (HF at 5 kHz, LF at 250 Hz): take each FMOD filter's 12 dB/oct
			// response at 8 kHz / 100 Hz. ponytail: no EQ boost, centre or bandwidth.
			float fCutoff = 10 * std::pow(2200.0f, fY);
			switch (env.mlDsp)
			{
			case cSomaSoundEvents::eDsp_Volume: fLayerGain *= fY; break;
			case cSomaSoundEvents::eDsp_EqGain: vFilter[l].x *= std::min(0.05f + 2.95f * fY, 1.0f); break;
			case cSomaSoundEvents::eDsp_Lowpass: vFilter[l].x /= std::sqrt(1 + std::pow(8000 / fCutoff, 4.0f)); break;
			case cSomaSoundEvents::eDsp_Highpass: vFilter[l].y /= std::sqrt(1 + std::pow(fCutoff / 100, 4.0f)); break;
			case cSomaSoundEvents::eDsp_Pitch: vSpeed[l] *= std::pow(2.0f, (fY - 0.5f) * 8); break;
			}
		}
		vGain[l].resize(layer.mvSounds.size());
		for (size_t s = 0; s < layer.mvSounds.size(); ++s)
		{
			const cSomaSoundEvents::cLayerSound &snd = layer.mvSounds[s];
			const cSomaSoundEvents::cSoundDef &def = mpEvent->mvDefs[snd.mlDef];
			float fGain = fLayerGain * snd.mfVolume * def.mfVolume;
			vGain[l][s] = fGain;
			cSlot &slot = mvSlots[l][s];
			bool bInRange = (mbStopped == false || mbStopAtFadeEnd) && (layer.mlParam < 0 || (fX >= snd.mfX0 && (fX < snd.mfX1 || snd.mfX1 >= 0.999f)));
			if (bInRange && slot.mbInRange == false)
			{
				slot.mbTriggered = false;
				slot.mfDelay = RandRange(def.mfDelayMin, def.mfDelayMax);
				slot.mfSpawnTimer = 0;
			}
			slot.mbInRange = bInRange;

			bool bAudible = fGain > 1e-4f || mb3DPlay == false;
			int lVoices = 0;
			for (cVoice &v : mvVoices)
				if (v.mlLayer == (int)l && v.mlSound == (int)s)
				{
					++lVoices;
					if (bInRange && v.mbLoop && bAudible == false)
						v.mpEntry->Stop();
					else if (bInRange == false && v.mbLoop)
					{
						if (snd.mlLoopMode == 2)
						{
							v.mpEntry->GetChannel()->SetLooping(false);
							v.mbLoop = false;
						}
						else
							v.mpEntry->Stop();
					}
				}
			if (bInRange == false)
				continue;
			if (slot.mfDelay > 0)
			{
				slot.mfDelay -= afTimeStep;
				bPending = true;
				continue;
			}
			if (snd.mlLoopMode != 1)
			{
				if (lVoices == 0 && bAudible)
					StartVoice((int)l, (int)s, true);
				bPending = true;
			}
			else if (def.mfSpawnMax > 0)
			{
				slot.mfSpawnTimer -= afTimeStep;
				// a spawn blocked by spawn_max fires when a voice frees up
				if (slot.mfSpawnTimer <= 0 && lVoices < def.mlSpawnCount && bAudible)
				{
					StartVoice((int)l, (int)s, false);
					slot.mfSpawnTimer = RandRange(def.mfSpawnMin, def.mfSpawnMax);
				}
				bPending = true;
			}
			else if (slot.mbTriggered == false)
			{
				slot.mbTriggered = true;
				StartVoice((int)l, (int)s, false);
			}
		}
	}

	mfAudibility = 0;
	cVector3f vPos = mb3DPlay ? PanPos() : cVector3f(0);
	for (size_t i = 0; i < mvVoices.size();)
	{
		cVoice &v = mvVoices[i];
		if (Handler()->IsValid(v.mpEntry, v.mlEntryId) == false)
		{
			mvVoices.erase(mvVoices.begin() + i);
			continue;
		}
		float fGain = vGain[v.mlLayer][v.mlSound] * v.mfGain;
		v.mpEntry->SetDefaultVolume(fGain);
		v.mpEntry->SetDefaultSpeed(v.mfSpeed * fEventSpeed * vSpeed[v.mlLayer]);
		const cVector2f &vF = vFilter[v.mlLayer];
		if (std::fabs(vF.x - v.mfGainHF) > 0.01f || std::fabs(vF.y - v.mfGainLF) > 0.01f)
		{
			v.mfGainHF = vF.x;
			v.mfGainLF = vF.y;
			v.mpEntry->GetChannel()->SetFilterGainHF(vF.x);
			v.mpEntry->GetChannel()->SetFilterGainLF(vF.y);
		}
		if (mb3DPlay)
			v.mpEntry->GetChannel()->SetPosition(vPos);
		mfAudibility += fGain;
		++i;
	}

	// oneshot=No events without parameters still finish (menu_glitch, maxplaybacks 1)
	if (mbStopped == false && (mpEvent->mbOneShot || mpEvent->mvParams.empty()) && bPending == false && mvVoices.empty())
		mbStopped = true;
}

tString cSomaSoundInstance::Describe()
{
	char buf[256];
	snprintf(buf, sizeof(buf), "%s voices=%d aud=%.3f vol=%.2f mul=%.2f fade=%.2f%s%s", msName.c_str(), (int)mvVoices.size(), mfAudibility, mfVolume,
			 mfVolumeMul, mfFade, mpEntity ? " entity" : "", mbStopped ? " stopped" : "");
	tString s = buf;
	for (const cVoice &v : mvVoices)
		if (v.mfGainHF < 1 || v.mfGainLF < 1)
			s += " hf=" + cString::ToString(v.mfGainHF) + " lf=" + cString::ToString(v.mfGainLF);
	for (size_t i = 0; i < mvParamValue.size(); ++i)
		s += " " + mpEvent->mvParams[i].msName + "=" + cString::ToString(mvParamValue[i]);
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
	SOMA_METHOD(e, "cSoundEntry", "float GetMinDistance()", +[](Inst *p) { return Live(p) ? p->GetEvent()->mfMinDist : 0.0f; });
	SOMA_METHOD(e, "cSoundEntry", "float GetMaxDistance()", +[](Inst *p) { return Live(p) ? p->GetEvent()->mfMaxDist : 0.0f; });
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
			p->FadeInTo(vol, speed);
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
