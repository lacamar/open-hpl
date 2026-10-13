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
	void FadeGrading(const tString &asName, float afTime);
	void SetGradingLayer(int alPrio, const tString &asName, float afAlpha);
	void RemoveGradingLayer(const tString &asName);

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

	cWorld *mpWorld;
	iTexture *mpGradingTexture;
	iTexture *mpGradingTarget;
	float mfGradingBlend;
	float mfGradingFadeTime;
	struct cGradingLayer
	{
		int mlPrio;
		tString msName;
		iTexture *mpTexture;
		float mfAlpha;
	};
	std::vector<cGradingLayer> mvGradingLayers;
	static cSomaToneMapping *mpInstance;
};

#endif
