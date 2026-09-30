#ifndef SOMA_TONE_MAPPING_H
#define SOMA_TONE_MAPPING_H

#include "hpl.h"

class asIScriptEngine;

using namespace hpl;

class cSomaToneMapping : public iUpdateable
{
public:
	cSomaToneMapping();

	static cSomaToneMapping *Get() { return mpInstance; }
	static void RegisterNatives(asIScriptEngine *apEngine);

	void OnMapLoaded(cWorld *apWorld);
	void Update(float afTimeStep);

	void FadeWorldExposure(float afX, float afTime) { mfWorldExposure = afX; mfWorldFadeTime = afTime; }
	void FadeWorldWhiteCut(float afX, float afTime) { mfWorldWhiteCut = afX; mfWorldFadeTime = afTime; }
	void FadeExposure(float afExposure, float afWhiteCut, float afTime);

	float mfKey;
	float mfGamma;
	float mfFilmGrainIntensity;
	float mfBrightPass;
	float mfBloomWidth;
	cColor mBloomTint;
	float mfBloomFalloff;
	bool mbBloomActive;
	bool mbFilmGrainActive;
	bool mbColorGradingActive;
	bool mbSRGB;

	float mfWorldExposure;
	float mfWorldWhiteCut;
	float mfWorldFadeTime;

	float mfExposure;
	float mfWhiteCut;
	float mfTransitionTime;

private:
	void Approach(float &afValue, float afGoal, float afTime, float afTimeStep);

	cWorld *mpWorld;
	static cSomaToneMapping *mpInstance;
};

#endif
