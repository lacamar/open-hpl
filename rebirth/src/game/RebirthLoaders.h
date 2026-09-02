/*
 * Reverse-engineered entity/area "Type" loaders for Amnesia: Rebirth's own
 * .hpm maps.
 *
 * Booting a real Rebirth start map (01_00_intro.hpm) against a stock HPL2
 * cResources with no loaders registered (the state Phase 0 shipped in)
 * produces a long run of "no loader for type 'X' found" warnings in
 * hpl.log - the map's own static geometry and lights load fine (both use
 * generic, type-agnostic code paths in cWorldLoaderHpm), but every single
 * <Entity>/<Area> element is silently dropped because nothing recognizes
 * Rebirth's own Type names. Those names, observed directly from a real
 * boot log against real game data (this list is only what that one map
 * happens to reference - add more as later maps surface new names):
 *
 *   Entity types: StaticProp, Prop_Rigid, Prop_Grab, Prop_SwingDoor,
 *                 Prop_Readable, Prop_SketchbookReadable
 *   Area types:   PlayerStart, Trigger, Soundscape
 *
 * None of these are Dark Descent's own entity/area type names (compare
 * amnesia/src/game/LuxBase.cpp's AddEntityLoader/AddAreaLoader calls -
 * "Object", "SwingDoor", "Item" etc.) even where the concept obviously
 * matches (Prop_SwingDoor vs SwingDoor) - Rebirth's game-logic layer
 * renamed its own entity taxonomy, it isn't just missing registrations
 * for Dark Descent's existing classes.
 *
 * cEntityLoader_Object (HPL2/core/include/resources/EntityLoader_Object.h)
 * already implements the entire generic part of entity loading - parsing
 * the .ent XML's Bodies/Shapes/Joints/Mesh/Lights/etc. sections is
 * completely type-agnostic in HPL2's engine. BeforeLoad/AfterLoad are the
 * only two hooks a per-type loader must supply, and Dark Descent's own
 * loaders (cLuxPropLoader_*) only use them for *game-logic* behaviour
 * (interaction state, physics callbacks, keeping a cLuxProp_* wrapper
 * object alive) - none of which exists for Rebirth yet. So
 * cRebirthGenericEntityLoader below is intentionally the same "just load
 * the geometry/physics/lights, no gameplay" shape as Dark Descent's own
 * cLuxStaticPropLoader (amnesia/src/game/LuxStaticProp.cpp) - it turns a
 * Prop_Rigid or Prop_SwingDoor into inert-but-correctly-placed static
 * geometry, not an interactive prop or door.
 *
 * PlayerStart is the one case with an existing generic (non-game-specific)
 * engine hook: cWorld::CreateStartPos()/GetStartPosEntity() already exists
 * for exactly this ("a named camera/player spawn point a map declares"),
 * and cRebirthBase::InitTestMap() already calls GetStartPosEntity()
 * generically - see cLuxAreaNodeLoader_PlayerStart::Load()'s own
 * gpBase->mpCurrentMapLoading==NULL branch in
 * amnesia/src/game/LuxAreaNodes.cpp for the exact pattern this mirrors
 * (that branch exists there for the same "no game-logic layer yet"
 * reason relevant tools/scaffolding use it for).
 */

#ifndef REBIRTH_LOADERS_H
#define REBIRTH_LOADERS_H

#include "hpl.h"

using namespace hpl;

//----------------------------------------------

class cRebirthGenericEntityLoader : public cEntityLoader_Object
{
public:
	cRebirthGenericEntityLoader(const tString &asName);
	virtual ~cRebirthGenericEntityLoader(){}

protected:
	void BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars);
	void AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars);
};

//----------------------------------------------

// AreaType="PlayerStart" -> a real cStartPosEntity, so
// cWorld::GetStartPosEntity() (already called generically by
// cRebirthBase::InitTestMap()) can find it by name.
class cRebirthAreaLoader_PlayerStart : public iAreaLoader
{
public:
	cRebirthAreaLoader_PlayerStart(const tString &asName);
	virtual ~cRebirthAreaLoader_PlayerStart(){}

	void Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld);
};

//----------------------------------------------

// Catch-all no-op for gameplay-only area types with no engine-side meaning
// yet (Trigger, Soundscape) - registering these only silences "no area
// loader" warnings and lets cWorldLoaderHpm count them as handled; no
// behaviour is attached, and none should be inferred from this existing.
class cRebirthAreaLoader_Noop : public iAreaLoader
{
public:
	cRebirthAreaLoader_Noop(const tString &asName);
	virtual ~cRebirthAreaLoader_Noop(){}

	void Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld);
};

//----------------------------------------------

void RegisterRebirthLoaders(cResources *apResources);

//----------------------------------------------

#endif // REBIRTH_LOADERS_H
