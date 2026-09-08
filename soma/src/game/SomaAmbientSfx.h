/*
 * Real map-authored ambient/environmental sound entities (car honking,
 * distant dogs, seagulls, a fridge hum, a DVD player idle loop, a
 * ventilation loop, ...) - see SomaAmbientSfx.cpp for the full root-cause
 * writeup and PORTING_NOTES.md for the live-verified fix this session.
 *
 * Mirrors cSomaMenuSfx's shape (EnsureCached() once at boot, self-contained,
 * reads only the user's own real SOMA install, writes only to the per-user
 * cache dir) but for a different, unrelated real data source: SOMA's own
 * .hpm_Sound per-map sidecar track (loaded by the existing, already-working
 * cWorldLoaderHpm::LoadSoundsTrack()/cEngineFileLoading::LoadSound(), which
 * this file's own comment shows was never the gap) rather than
 * script-invoked Sound_PlayGui() event names.
 */

#ifndef SOMA_AMBIENT_SFX_H
#define SOMA_AMBIENT_SFX_H

#include "hpl.h"

using namespace hpl;

class cSomaAmbientSfx
{
public:
	// Extracts the real FMOD-banked ambient samples this port's known real
	// maps reference and synthesizes matching real .snt sound-entity sidecar
	// files (+ their real extracted audio) into a per-user cache resource
	// dir, so cSoundEntityManager::CreateSoundEntity() - which already runs
	// unmodified for every real .hpm_Sound entity - finds a real, playable
	// resource where today it silently fails. Idempotent/cheap on repeat
	// calls (checks the cache first); call once before any map load, same
	// spot RegisterSomaLoaders() already runs from in cSomaBase::Init().
	static void EnsureCached(cResources *apResources);
};

#endif // SOMA_AMBIENT_SFX_H
