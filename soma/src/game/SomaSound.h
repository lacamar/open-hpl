#ifndef SOMA_SOUND_H
#define SOMA_SOUND_H

#include "hpl.h"
#include "RustSomaSound.h"

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

	struct cParam
	{
		tString msName;
		float mfMin = 0, mfMax = 1;
		float mfVelocity = 0;
		float mfSeek = 0;
		float mfSustain = -1;
		int mlLoopMode = 0;
		int mlBuiltin = 0;
	};

	OhplGs5Bank *GetBank() { return mpBank; }

private:
	cSomaSoundEvents();
	void LoadProject(const tString &asProject);
	int GetEvent(const tString &asName);

	OhplGs5Bank *mpBank;
	std::list<cSomaSoundInstance *> mlstInstances;
	std::map<cSomaSoundInstance *, int> mmapLive;
	bool mbUpdaterAdded = false;
};

class cSomaSoundInstance : public iSoundEntityEvent
{
public:
	cSomaSoundInstance(size_t alEvent, const tString &asName, cSoundEntity *apEntity, eSoundEntryType aType);
	~cSomaSoundInstance();

	void Play();
	void Stop(bool abPlayEnd);
	void FadeIn(float afSpeed) { FadeInTo(1, afSpeed); }
	void FadeOut(float afSpeed);
	bool IsStopped();
	void Update(float afTimeStep);

	void FadeInTo(float afVolumeMul, float afSpeed);
	void StopNow();
	void FadeVolumeMulTo(float afDest, float afSpeed);
	void SetVolumeMul(float afX) { mpS->volume_mul = mpS->volume_mul_dest = afX; }
	float GetVolumeMul() { return mpS->volume_mul; }
	void FadeSpeedMulTo(float afDest, float afSpeed);
	void SetSpeedMul(float afX) { mpS->speed_mul = mpS->speed_mul_dest = afX; }
	float GetSpeedMul() { return mpS->speed_mul; }
	void SetVolume(float afX) { mpS->volume = afX; }
	float GetVolume() { return mpS->volume; }
	void SetParam(int alIdx, float afValue) override;
	void SetParam(const tString &asName, float afValue) override { SetParam(ParamIndex(asName), afValue); }
	int ParamIndex(const tString &asName);
	float GetParamValue(int alIdx);
	int GetParamNum();
	const cSomaSoundEvents::cParam *GetParamDef(int alIdx);
	void SetPaused(bool abX);
	bool GetPaused() { return mpS->paused; }
	bool IsPlaying() { return mpS->stopped == false; }
	void SetPosition(const cVector3f &avPos) { mvPos = avPos; mpS->pos[0] = avPos.x; mpS->pos[1] = avPos.y; mpS->pos[2] = avPos.z; }
	const cVector3f &GetPosition() { return mvPos; }
	float GetElapsedTime() { return mpS->time; }
	float GetTotalTime();
	float GetAudibility() { return mpS->audibility; }
	bool Is3D() { return mEvent.is_3d; }
	bool IsOneShot() { return mEvent.one_shot; }
	float GetMinDist() { return mEvent.min_dist; }
	float GetMaxDist() { return mEvent.max_dist; }
	eSoundEntryType GetType() { return (eSoundEntryType)mpS->entry_type; }
	void SetStopDisabled(bool abX) { mpS->stop_disabled = abX; }
	bool GetStopDisabled() { return mpS->stop_disabled; }
	bool IsActive() { return mpS->stopped == false; }

	const tString &GetName() { return msName; }
	int GetId() { return mlId; }
	tString Describe();

private:
	friend class cSomaSoundEvents;
	friend class cSomaSaveState;

	// SomaSave reads these two
	struct cStateFloat
	{
		OhplGs5InstState *const *mppS;
		float OhplGs5InstState::*mpField;
		operator float() const { return (*mppS)->*mpField; }
	};

	size_t mlH;
	OhplGs5InstState *mpS;
	cStateFloat mfFadeDest{&mpS, &OhplGs5InstState::fade_dest};
	cStateFloat mfVolumeMulDest{&mpS, &OhplGs5InstState::volume_mul_dest};
	OhplGs5EventInfo mEvent;
	tString msName;
	int mlId;
	cSoundEntity *mpEntity;
	cVector3f mvPos = cVector3f(0, 0, 1);
	cSomaSoundEvents::cParam mParamCache;
};

tString SomaSerializeMusic();
void SomaDeserializeMusic(const tString &asData);

#endif // SOMA_SOUND_H
