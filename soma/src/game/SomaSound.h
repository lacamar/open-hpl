#ifndef SOMA_SOUND_H
#define SOMA_SOUND_H

#include "hpl.h"
#include "RustSomaSound.h"

#include <angelscript.h>
#include <list>

using namespace hpl;

// SOMA numbering: HPL2's eSoundEntryType_Gui (2) is WorldClean
const eSoundEntryType eSomaSoundEntryType_Gui = (eSoundEntryType)4;

class cSomaSoundInstance;

class cSomaSoundEvents
{
	friend class cSomaSoundInstance;

public:
	static cSomaSoundEvents *Get()
	{
		static cSomaSoundEvents events;
		return &events;
	}

	cSomaSoundInstance *PlayGui(const tString &asEvent, float afVolume, int alEntryType, bool abLoop = false, bool abStream = false)
	{
		return (cSomaSoundInstance *)ohpl_s7_play_gui((const unsigned char *)asEvent.data(), asEvent.size(), afVolume, alEntryType, abLoop, abStream);
	}
	void PreloadProject(const tString &asProject) { ohpl_s7_preload((const unsigned char *)asProject.data(), asProject.size()); }
	void FadeOutAll(tFlag aTypes, float afSpeed) { ohpl_s7_fade_out_all(aTypes, afSpeed); }
	cSomaSoundInstance *FindInstance(const tString &asName) { return (cSomaSoundInstance *)ohpl_s7_find_instance((const unsigned char *)asName.data(), asName.size()); }
	bool IsLive(cSomaSoundInstance *apInstance, int alId = -1) { return ohpl_s7_is_live((size_t)apInstance, alId); }
	const std::list<cSomaSoundInstance *> &GetInstances() { return mlstInstances; }

	static void RegisterNatives(asIScriptEngine *apEngine) { ohpl_s7_register_natives(apEngine); }

private:
	std::list<cSomaSoundInstance *> mlstInstances;
};

class cSomaSoundInstance : public iSoundEntityEvent
{
public:
	cSomaSoundInstance(size_t alEvent, const tString &asName, cSoundEntity *apEntity, eSoundEntryType aType) : msName(asName)
	{
		mlH = ohpl_s7_inst_new((size_t)this, alEvent, (const unsigned char *)asName.data(), asName.size(), (size_t)apEntity, apEntity != NULL, (int)aType);
		mpS = ohpl_s7_inst_state(mlH);
		cSomaSoundEvents::Get()->mlstInstances.push_back(this);
	}
	~cSomaSoundInstance()
	{
		ohpl_s7_inst_destroy(mlH);
		cSomaSoundEvents::Get()->mlstInstances.remove(this);
	}

	void Play() { ohpl_s7_inst_play(mlH); }
	void Stop(bool abPlayEnd) { ohpl_s7_inst_stop(mlH); }
	void FadeIn(float afSpeed) { FadeInTo(1, afSpeed); }
	void FadeOut(float afSpeed) { ohpl_s7_inst_fade_out(mlH, afSpeed); }
	bool IsStopped() { return ohpl_s7_inst_is_stopped(mlH); }
	void Update(float afTimeStep) { ohpl_s7_inst_update(mlH, afTimeStep); }

	void FadeInTo(float afVolumeMul, float afSpeed) { ohpl_s7_inst_fade_in_to(mlH, afVolumeMul, afSpeed); }
	void StopNow() { ohpl_s7_inst_stop_now(mlH); }
	void FadeVolumeMulTo(float afDest, float afSpeed) { ohpl_s7_inst_fade_volume_mul_to(mlH, afDest, afSpeed); }
	void SetVolumeMul(float afX) { mpS->volume_mul = mpS->volume_mul_dest = afX; }
	float GetVolumeMul() { return mpS->volume_mul; }
	void FadeSpeedMulTo(float afDest, float afSpeed) { ohpl_s7_inst_fade_speed_mul_to(mlH, afDest, afSpeed); }
	void SetSpeedMul(float afX) { mpS->speed_mul = mpS->speed_mul_dest = afX; }
	float GetSpeedMul() { return mpS->speed_mul; }
	void SetVolume(float afX) { mpS->volume = afX; }
	float GetVolume() { return mpS->volume; }
	void SetParam(int alIdx, float afValue) override { ohpl_s7_inst_set_param(mlH, alIdx, afValue); }
	void SetParam(const tString &asName, float afValue) override { SetParam(ParamIndex(asName), afValue); }
	int ParamIndex(const tString &asName) { return ohpl_s7_inst_param_index(mlH, (const unsigned char *)asName.data(), asName.size()); }
	float GetParamValue(int alIdx) { return ohpl_s7_inst_param_value(mlH, alIdx); }
	int GetParamNum() { return ohpl_s7_inst_param_count(mlH); }
	void SetPaused(bool abX) { ohpl_s7_inst_set_paused(mlH, abX); }
	bool GetPaused() { return mpS->paused; }
	bool IsPlaying() { return mpS->stopped == false; }
	void SetPosition(const cVector3f &avPos) { mvPos = avPos; ohpl_s7_inst_set_position((size_t)this, mlH, &avPos.x); }
	const cVector3f &GetPosition() { return mvPos; }
	float GetElapsedTime() { return mpS->time; }
	float GetTotalTime() { return ohpl_s7_inst_total_time(mlH); }
	float GetAudibility() { return mpS->audibility; }
	eSoundEntryType GetType() { return (eSoundEntryType)mpS->entry_type; }
	void SetStopDisabled(bool abX) { mpS->stop_disabled = abX; }
	bool GetStopDisabled() { return mpS->stop_disabled; }
	bool IsActive() { return mpS->stopped == false; }

	const tString &GetName() { return msName; }
	int GetId() { return mpS->id; }
	size_t GetHandle() { return mlH; }
	tString Describe()
	{
		tString s;
		ohpl_s7_inst_describe(mlH, [](void *c, const unsigned char *p, size_t n) { static_cast<tString *>(c)->assign((const char *)p, n); }, &s);
		return s;
	}

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
	tString msName;
	cVector3f mvPos = cVector3f(0, 0, 1);
};

inline tString SomaSerializeMusic()
{
	tString s;
	ohpl_s7_music_serialize([](void *c, const unsigned char *p, size_t n) { static_cast<tString *>(c)->assign((const char *)p, n); }, &s);
	return s;
}

inline void SomaDeserializeMusic(const tString &asData) { ohpl_s7_music_deserialize((const unsigned char *)asData.data(), asData.size()); }

#endif // SOMA_SOUND_H
