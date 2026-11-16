#ifndef SOMA_TONE_MAPPING_H
#define SOMA_TONE_MAPPING_H

#include "hpl.h"

class asIScriptEngine;

using namespace hpl;

// Shell of the Rust tone mapping object (rust/src/somaui/s8b_tone.rs)
class cSomaToneMapping : public iUpdateable
{
public:
	cSomaToneMapping();

	static cSomaToneMapping *Get() { return mpInstance; }
	static void RegisterNatives(asIScriptEngine *apEngine);
	static void InstallHost();
	static bool WorldExposure(float *apExposure, float *apWhiteCut);
	static void FadeExposure(float afExposure, float afWhiteCut, float afTime);

	void OnMapLoaded(cWorld *apWorld);
	void Update(float afTimeStep);

private:
	static cSomaToneMapping *mpInstance;
};

#endif
