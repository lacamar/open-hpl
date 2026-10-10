#ifndef SOMA_SOUND_H
#define SOMA_SOUND_H

#include "hpl.h"

#include <angelscript.h>
#include <map>
#include <set>

using namespace hpl;

// SOMA numbering: HPL2's eSoundEntryType_Gui (2) is WorldClean
const eSoundEntryType eSomaSoundEntryType_Gui = (eSoundEntryType)4;

class cSomaSoundInstance;

class cSomaSoundEvents
{
	friend class cSomaSoundInstance;

public:
	static cSomaSoundEvents *Get();

	cSoundEntityData *Resolve(const tString &asEvent);
	cSomaSoundInstance *PlayGui(const tString &asEvent, float afVolume, int alEntryType, bool abLoop = false, bool abStream = false);
	cSomaSoundInstance *Play3D(const tString &asEvent, float afVolume, const cVector3f &avPos, int alEntryType);
	void PreloadProject(const tString &asProject);
	void Update(float afTimeStep);
	void FadeOutAll(tFlag aTypes, float afSpeed);

	cSomaSoundInstance *FindInstance(const tString &asName);
	bool IsLive(cSomaSoundInstance *apInstance, int alId = -1);
	const std::list<cSomaSoundInstance *> &GetInstances() { return mlstInstances; }

	static void RegisterNatives(asIScriptEngine *apEngine);

	struct cWave
	{
		tString msBank;
		tString msSample;
	};
	enum eDsp { eDsp_Volume, eDsp_EqGain, eDsp_Lowpass, eDsp_Highpass };
	struct cEnvelope
	{
		int mlParam = -1;
		int mlDsp = eDsp_Volume;
		std::vector<cVector2f> mvPoints;
		float Eval(float afX) const;
	};
	struct cSoundDef
	{
		bool mbSequential = false;
		float mfVolume = 1;
		float mfVolumeRandDb = 0;
		float mfPitch = 0;
		float mfPitchRand = 0;
		float mfSpawnMin = 0;
		float mfSpawnMax = 0;
		int mlSpawnCount = 1;
		float mfDelayMin = 0;
		float mfDelayMax = 0;
		std::vector<cWave> mvWaves;
		std::vector<tString> mvFiles;
		int mlLast = -1;
	};
	struct cLayerSound
	{
		int mlDef = -1;
		float mfVolume = 1;
		int mlLoopMode = 1;
		float mfX0 = 0, mfX1 = 1;
	};
	struct cLayer
	{
		int mlParam = -1;
		std::vector<cLayerSound> mvSounds;
		std::vector<cEnvelope> mvEnvelopes;
	};
	struct cParam
	{
		tString msName;
		float mfMin = 0, mfMax = 1;
		float mfVelocity = 0;
		float mfSeek = 0;
		int mlLoopMode = 0;
		int mlBuiltin = 0;
	};
	enum eRolloff { eRolloff_Linear, eRolloff_LinearSquare, eRolloff_Log, eRolloff_Custom };
	enum eMaxBehavior { eMaxBehavior_StealOldest, eMaxBehavior_StealNewest, eMaxBehavior_StealQuietest, eMaxBehavior_JustFail, eMaxBehavior_JustFailIfQuietest };
	struct cEvent
	{
		tString msName;
		float mfVolume = 1;
		float mfVolumeRandDb = 0;
		float mfPitch = 0;
		float mfPitchRand = 0;
		bool mb3D = true;
		float mfPanLevel = 1;
		bool mbOneShot = true;
		float mfMinDist = 1;
		float mfMaxDist = 20;
		int mlRolloff = eRolloff_Linear;
		int mlMaxPlaybacks = 0;
		int mlMaxBehavior = eMaxBehavior_StealOldest;
		float mfFadeIn = 0;
		float mfFadeOut = 0;
		bool mbStream = false;
		std::vector<cParam> mvParams;
		std::vector<cLayer> mvLayers;
		std::vector<cSoundDef> mvDefs;
		cSoundEntityData *mpData = NULL;
		bool mbLoaded = false;
		bool HasFiles() const;
	};

private:
	void LoadProject(const tString &asProject);
	cEvent *GetEvent(const tString &asName);
	cEvent *FileEvent(const tString &asFile, bool abLoop, bool abStream);

	std::map<tString, cEvent> mmapEvents;
	std::map<cSoundEntityData *, cEvent *> mmapDataEvents;
	std::set<tString> msetProjects;
	std::list<cSomaSoundInstance *> mlstInstances;
	std::map<cSomaSoundInstance *, int> mmapLive;
	tWString msCacheDir;
	bool mbUpdaterAdded = false;
	int mlNextId = 1;
};

class cSomaSoundInstance : public iSoundEntityEvent
{
public:
	typedef cSomaSoundEvents::cEvent cEvent;
	cSomaSoundInstance(cEvent *apEvent, const tString &asName, cSoundEntity *apEntity, eSoundEntryType aType);
	~cSomaSoundInstance();

	void Play();
	void Stop(bool abPlayEnd);
	void FadeIn(float afSpeed) { FadeInTo(1, afSpeed); }
	void FadeOut(float afSpeed);
	bool IsStopped() { return mbStopped && mvVoices.empty(); }
	void Update(float afTimeStep);

	void FadeInTo(float afVolumeMul, float afSpeed);
	void StopNow();
	void FadeVolumeMulTo(float afDest, float afSpeed);
	void SetVolumeMul(float afX) { mfVolumeMul = mfVolumeMulDest = afX; }
	float GetVolumeMul() { return mfVolumeMul; }
	void FadeSpeedMulTo(float afDest, float afSpeed);
	void SetSpeedMul(float afX) { mfSpeedMul = mfSpeedMulDest = afX; }
	float GetSpeedMul() { return mfSpeedMul; }
	void SetVolume(float afX) { mfVolume = afX; }
	float GetVolume() { return mfVolume; }
	void SetParam(int alIdx, float afValue);
	void SetParam(const tString &asName, float afValue) { SetParam(ParamIndex(asName), afValue); }
	int ParamIndex(const tString &asName);
	float GetParamValue(int alIdx);
	int GetParamNum() { return (int)mvParamValue.size(); }
	const cSomaSoundEvents::cParam *GetParamDef(int alIdx);
	void SetPaused(bool abX);
	bool GetPaused() { return mbPaused; }
	bool IsPlaying() { return mbStopped == false; }
	void SetPosition(const cVector3f &avPos) { mvPos = avPos; }
	const cVector3f &GetPosition() { return mvPos; }
	float GetElapsedTime() { return mfTime; }
	float GetTotalTime();
	float GetAudibility() { return mfAudibility; }
	bool Is3D() { return mpEvent->mb3D; }
	bool IsOneShot() { return mpEvent->mbOneShot; }
	eSoundEntryType GetType() { return mType; }
	void SetStopDisabled(bool abX) { mbStopDisabled = abX; }
	bool GetStopDisabled() { return mbStopDisabled; }
	bool IsActive() { return mbStopped == false; }

	const tString &GetName() { return msName; }
	int GetId() { return mlId; }
	cEvent *GetEvent() { return mpEvent; }
	tString Describe();

private:
	friend class cSomaSoundEvents;
	friend class cSomaSaveState;
	struct cVoice
	{
		cSoundEntry *mpEntry;
		int mlEntryId;
		int mlLayer, mlSound;
		float mfGain;
		float mfSpeed;
		bool mbLoop;
		float mfGainHF = 1, mfGainLF = 1;
	};
	struct cSlot
	{
		bool mbInRange = false;
		bool mbTriggered = false;
		float mfDelay = 0;
		float mfSpawnTimer = 0;
	};
	void Start();
	bool StartVoice(int alLayer, int alSound, bool abLoop);
	float ParamNorm(int alIdx);
	float DistanceGain(float afDist);
	cVector3f SourcePos();
	cVector3f PanPos();
	float ListenerDistance();

	cEvent *mpEvent;
	tString msName;
	int mlId;
	cSoundEntity *mpEntity;
	eSoundEntryType mType;
	cVector3f mvPos = cVector3f(0, 0, 1);
	std::vector<float> mvParamValue, mvParamTarget;
	std::vector<cVoice> mvVoices;
	std::vector<std::vector<cSlot>> mvSlots;
	float mfRandGain = 1, mfRandPitch = 0;
	float mfVolume = 1;
	float mfVolumeMul = 1, mfVolumeMulDest = 1, mfVolumeMulSpeed = 0;
	float mfSpeedMul = 1, mfSpeedMulDest = 1, mfSpeedMulSpeed = 0;
	float mfFade = 1, mfFadeDest = 1, mfFadeSpeed = 0;
	bool mbStopAtFadeEnd = false;
	bool mbStarted = false, mbStopped = false, mbPaused = false, mbStopDisabled = false;
	bool mb3DPlay = false;
	float mfTime = 0;
	float mfAudibility = 0;
};

tString SomaSerializeMusic();
void SomaDeserializeMusic(const tString &asData);

#endif // SOMA_SOUND_H
