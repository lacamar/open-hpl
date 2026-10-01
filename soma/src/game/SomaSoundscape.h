/*
 * cLuxSoundscapeHandler: Soundscape areas around the camera pick a background sound and fade named
 * sound entities in and out per level (Global, Area, Room, SubRoom); the areas around the player's
 * body give the sound prefix used by Sound_CreateAtEntity_UsePrefix and footsteps. Reverb areas pick the
 * global reverb (FMOD Ex presets, faded in the mB domain, sent to an EFX EAX reverb).
 */

#ifndef SOMA_SOUNDSCAPE_H
#define SOMA_SOUNDSCAPE_H

#include "hpl.h"

class asIScriptEngine;
class cSomaLuxEntity;
class cSomaLuxMap;
class cSomaSoundInstance;

using namespace hpl;

class cSomaSoundscape
{
public:
	static cSomaSoundscape *Get();
	static void RegisterNatives(asIScriptEngine *apEngine);

	void Update(cSomaLuxMap *apMap, float afTimeStep);
	void SetDefaultReverb(int alPreset, float afFadeTime);
	void SetPaused(bool abX);
	const tString &GetPrefix() { return msPrefix; }
	tString Describe();

private:
	struct cArea
	{
		cSomaLuxEntity *mpEnt;
		int mlLevel;
		int mlPrio;
		tString msBG;
		float mfBGVolume, mfBGFadeIn, mfBGTransSpeed, mfParentVolMul;
		std::vector<tString> mvIn, mvOut;
		float mfEntFadeIn, mfEntFadeOut;
		tString msPrefix;
		int mlPrefixPrio;
		bool mbUseReverb;
		int mlReverbPrio, mlReverbType;
		float mfReverbAmount, mfReverbFade;
		bool mbInside = false;
	};
	struct cLevel
	{
		cArea *mpArea = NULL;
		cSomaSoundInstance *mpBG = NULL;
		int mlBGId = -1;
		tString msBG;
		float mfBGVolume = 0, mfBGFade = 0;
		float mfParentVolMul = 1;
		std::vector<tString> mvIn, mvOut;
		float mfEntFadeIn = 1, mfEntFadeOut = 1;
	};
	static const int kLevels = 4;

	void Load(cSomaLuxMap *apMap);
	bool Inside(cArea &aArea, const cVector3f &avPos);
	bool BGLive(cLevel &aLevel);
	void StopBG(cLevel &aLevel, float afParentProd);
	void UpdateBG(int alLevel, cArea *apArea, float afParentProd);
	void UpdateEntities(int alLevel, cArea *apArea);
	void UpdateReverb(float afTimeStep);
	void ApplyReverb();
	void FadeEntities(const std::vector<tString> &avA, const std::vector<tString> &avB, float afTime, bool abIn);

	cSomaLuxMap *mpMap = NULL;
	cWorld *mpWorld = NULL;
	std::vector<cArea> mvAreas;
	std::vector<cArea *> mvActive;
	cLevel mLevels[kLevels];
	tString msPrefix;

	static const int kRevFields = 17;
	float mvRevCur[kRevFields] = {}, mvRevFrom[kRevFields] = {}, mvRevTo[kRevFields] = {};
	float mfRevT = 1, mfRevTime = 0;
	int mlRevType = -2;
	float mfRevAmount = -1;
	cArea *mpRevArea = NULL;
	int mlDefaultReverb = 0;
	float mfDefaultReverbFade = 1;
};

#endif // SOMA_SOUNDSCAPE_H
