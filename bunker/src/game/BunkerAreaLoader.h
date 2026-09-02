/*
 * Real fix for the "start map has no StartPosEntity, using world origin"
 * gap noted in PORTING_NOTES.md's Rebirth/Bunker Phase 0 section.
 *
 * The Bunker's maps (like SOMA's/Rebirth's - see HPL2/core/sources/
 * resources/WorldLoaderHpm.cpp, the shared HPL3 split-track ".hpm" loader)
 * have no cStartPosEntity at all - that's an unrelated, HPL2-only concept
 * (cWorld::GetStartPosEntity()) that the split-track loader never
 * populates. Instead, player spawn points are plain map Areas of
 * AreaType="PlayerStart" (confirmed by inspecting a real install's
 * trenches.hpm_Area - e.g. `<Area Name="Start_Begin" WorldPos="2.2563
 * 0.978092 16.0246" ... AreaType="PlayerStart">`), exactly like Dark
 * Descent's own cLuxAreaNodeLoader_PlayerStart (amnesia/src/game/
 * LuxAreaNodes.h/.cpp) - same AreaType name, same iAreaLoader extension
 * point (cResources::AddAreaLoader/GetAreaLoader), just registered here
 * instead since amnesia/src/game/ is Dark Descent's own module. Without
 * *some* loader registered for "PlayerStart", cWorldLoaderHpm::CreateMapArea
 * silently drops every such Area (logged as "no area loader registered for
 * AreaType 'PlayerStart'") and InitTestMap() has nothing to resolve
 * main_init.cfg's <StartMap Pos="Start_Begin"/> against.
 *
 * Trimmed to Phase 0's actual need: no AI-node graph like Dark Descent's
 * cLuxNode_PlayerStart, just a name -> world transform table a caller can
 * look up after LoadWorld() returns.
 */

#ifndef BUNKER_AREA_LOADER_H
#define BUNKER_AREA_LOADER_H

#include "hpl.h"

using namespace hpl;

//----------------------------------------------

class cBunkerAreaLoader_PlayerStart : public iAreaLoader
{
public:
	cBunkerAreaLoader_PlayerStart(const tString &asName);

	void Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize,
			  const cMatrixf &a_mtxTransform, cWorld *apWorld);

	// Looks up a PlayerStart Area by its map-authored Name (e.g.
	// "Start_Begin", main_init.cfg's <StartMap Pos="..."/> value). Returns
	// false (aMtxOut untouched) if no such Area was seen during the last
	// LoadWorld() call, or it existed but was Active="false".
	static bool GetStartTransform(const tString &asMapStartName, cMatrixf &aMtxOut);

	// Called before each LoadWorld() so a later map's Areas don't leak into
	// an earlier map's namesake lookup - harmless for Phase 0 (only ever
	// loads one map per process) but keeps this correct if that changes.
	static void Clear();

private:
	static std::map<tString, cMatrixf> mmapPlayerStarts;
};

//----------------------------------------------

#endif // BUNKER_AREA_LOADER_H
