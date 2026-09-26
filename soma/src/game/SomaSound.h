/*
 * SOMA's FMOD Designer sound events on HPL2 sound entities. An event path ("project/group/event")
 * is resolved through the project's .fdp (event -> sound definitions -> waveforms and their banks);
 * the samples are extracted from the .fsb banks into the cache and the event becomes sound entity
 * data (volume, 3D distances, looping) that cWorld::CreateSoundEntity can use.
 */

#ifndef SOMA_SOUND_H
#define SOMA_SOUND_H

#include "hpl.h"

#include <angelscript.h>
#include <map>
#include <set>

using namespace hpl;

class cSomaSoundEvents
{
public:
	static cSomaSoundEvents *Get();

	// Sound entity data for an event path, NULL when it is not a known event (cSoundEntityManager resolver)
	cSoundEntityData *Resolve(const tString &asEvent);
	cSoundEntry *PlayGui(const tString &asEvent, float afVolume, int alEntryType);
	void PreloadProject(const tString &asProject);

	static void RegisterNatives(asIScriptEngine *apEngine);

private:
	struct cWave
	{
		tString msBank;
		tString msSample;
	};
	struct cEvent
	{
		float mfVolume = 1;
		bool mb3D = true;
		float mfMinDist = 1;
		float mfMaxDist = 20;
		bool mbLoop = false;
		std::vector<cWave> mvWaves;
		std::vector<tString> mvFiles;
		cSoundEntityData *mpData = NULL;
	};
	void LoadProject(const tString &asProject);
	std::map<tString, cEvent> mmapEvents;
	std::set<tString> msetProjects;
};

#endif // SOMA_SOUND_H
