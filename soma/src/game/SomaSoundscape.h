#ifndef SOMA_SOUNDSCAPE_H
#define SOMA_SOUNDSCAPE_H

#include "hpl.h"
#include "RustSomaSound.h"

class asIScriptEngine;
class cSomaLuxEntity;
class cSomaLuxMap;

using namespace hpl;

class cSomaSoundscape
{
public:
	static cSomaSoundscape *Get();
	static void RegisterNatives(asIScriptEngine *apEngine);

	void Update(cSomaLuxMap *apMap, float afTimeStep);
	void Forget(cSomaLuxMap *apMap)
	{
		if (apMap == mpMap)
			Load(NULL);
	}
	void SetDefaultReverb(int alPreset, float afFadeTime);
	void SetPaused(bool abX);
	const tString &GetPrefix() { return msPrefix; }
	tString Describe();
	cWorld *GetWorld() { return mpWorld; }

private:
	cSomaSoundscape();
	void Load(cSomaLuxMap *apMap);

	OhplGs5Scape *mpScape;
	cSomaLuxMap *mpMap = NULL;
	cWorld *mpWorld = NULL;
	tString msPrefix;
};

#endif // SOMA_SOUNDSCAPE_H
