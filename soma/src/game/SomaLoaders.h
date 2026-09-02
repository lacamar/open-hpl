/*
 * Reverse-engineered entity/area "Type" loaders for SOMA's own .hpm maps.
 *
 * Mirrors rebirth/src/game/RebirthLoaders.h's approach almost exactly - see
 * that file for the full rationale of why a generic, type-agnostic loader
 * is the right shape here. Summary: booting a real SOMA map
 * (00_01_apartment.hpm) against a stock cResources with no loaders
 * registered produces a long run of "no loader for type/area type 'X'"
 * warnings in hpl.log - static geometry and lights load fine (cWorldLoaderHpm
 * handles those generically), but every <Entity>/<Area> element is silently
 * dropped because nothing recognizes SOMA's own Type/AreaType names.
 *
 * Unlike Rebirth, SOMA's per-map hpm data doesn't carry a Type attribute
 * directly on its <Entity> elements - each Entity instance only references a
 * FileIndex into a per-map file list of .ent paths, and the actual type name
 * lives inside that .ent file's own <UserDefinedVariables EntityType="..."/>
 * (confirmed by inspecting a real install's entities/urban/desk/
 * desk_paper_crumbled/desk_paper_crumbled.ent). So this list was collected
 * two ways: (1) a live headless boot of 00_01_apartment.hpm and a second map
 * (see PORTING_NOTES.md for which), reading the "no loader for type/area
 * type" warnings directly out of hpl.log, and (2) a full census -
 * `grep -rohE 'EntityType="[^"]+"' entities/` and `grep -ohE 'AreaType="[^"]+"'
 * maps/.../*.hpm_Area` - across the entire real SOMA install, which is more
 * complete than any handful of sampled maps could be on its own (a per-map
 * boot only exercises what that map happens to reference).
 *
 * Entity types (full census, entities/**\/*.ent's EntityType="..."):
 *   Agent_Anglerfish, Agent_Construct_Crawler, Agent_Construct_Worker,
 *   Agent_DeepseaSuit, Agent_Flesher, Agent_Humanoid, Agent_Humanoid_NPC,
 *   Agent_Infected_Robot, Agent_Puppet, Agent_Remade, Agent_Roomba,
 *   Agent_Swarm, Agent_SwimBot, Agent_Viperfish, Critter_CaveSpider,
 *   Critter_CrabSmall, Critter_CrabSpider, Critter_FishSmall,
 *   Critter_Nautilus, critter_wau_swarm_agent_fish, Prop_Button,
 *   Prop_ConstructLure, Prop_Datamine, Prop_EnergySource, Prop_Grab,
 *   Prop_HandheldTerminal, Prop_HudObject, Prop_Lamp, Prop_LevelDoor,
 *   Prop_Lever, Prop_Meter, Prop_MoveObject, Prop_MovingButton,
 *   Prop_OmniSlot, Prop_PhysicsSlideDoor, Prop_PlayerHands, Prop_Push,
 *   Prop_Readable, Prop_Rigid, Prop_Slide, Prop_SlideDoor, Prop_SwingDoor,
 *   Prop_Tear, Prop_Terminal, Prop_Tool, Prop_Wheel, StaticCollider,
 *   StaticProp
 *
 * Area types (full census, maps/*\/*\/*.hpm_Area's AreaType="..."):
 *   AgentRepel, AmbientLight, CameraAnimation, Climb, Crawl, Datamine,
 *   Description, Distortion, DoorwayTrigger, EyeTrackingZoom, Hide,
 *   InteractAux, Ladder, Liquid, MapTransfer, PathNode, PlayerStart, Sit,
 *   Soundscape, Sticky, Tool, Trigger, VisibilityArea, VisibilityPortal, Zoom
 *
 * Agent_ and Critter_ (monster/NPC AI) entities are included in the generic
 * loader registration below too, even though this Phase 0 scaffold has no
 * AI/scripting layer to drive them - same reasoning as Rebirth's
 * StaticProp/Prop_Rigid: cEntityLoader_Object's generic mesh/body/light
 * parsing is completely type-agnostic, so registering it for these just
 * means an Agent's mesh renders as inert static geometry (no movement, no
 * AI) instead of being silently dropped - strictly better than nothing, and
 * exactly as inert as every other entity type this scaffold loads.
 *
 * As with Rebirth, PlayerStart is the one AreaType with a real, generic
 * engine hook (cWorld::CreateStartPos()/GetStartPosEntity()) - everything
 * else gets the same no-op treatment as cRebirthAreaLoader_Noop, purely to
 * silence "no area loader" warnings.
 */

#ifndef SOMA_LOADERS_H
#define SOMA_LOADERS_H

#include "hpl.h"

using namespace hpl;

//----------------------------------------------

class cSomaGenericEntityLoader : public cEntityLoader_Object
{
public:
	cSomaGenericEntityLoader(const tString &asName);
	virtual ~cSomaGenericEntityLoader(){}

protected:
	void BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars);
	void AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars);
};

//----------------------------------------------

// AreaType="PlayerStart" -> a real cStartPosEntity, so
// cWorld::GetStartPosEntity() can find it by name (same mechanism Rebirth's
// cRebirthAreaLoader_PlayerStart uses - see cSomaBase::LoadMap()/InitTestMap()
// in SomaBase.cpp for how/whether this is queried).
class cSomaAreaLoader_PlayerStart : public iAreaLoader
{
public:
	cSomaAreaLoader_PlayerStart(const tString &asName);
	virtual ~cSomaAreaLoader_PlayerStart(){}

	void Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld);
};

//----------------------------------------------

// Catch-all no-op for gameplay-only area types with no engine-side meaning
// yet - registering these only silences "no area loader" warnings and lets
// cWorldLoaderHpm count them as handled; no behaviour is attached, and none
// should be inferred from this existing.
class cSomaAreaLoader_Noop : public iAreaLoader
{
public:
	cSomaAreaLoader_Noop(const tString &asName);
	virtual ~cSomaAreaLoader_Noop(){}

	void Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld);
};

//----------------------------------------------

void RegisterSomaLoaders(cResources *apResources);

//----------------------------------------------

#endif // SOMA_LOADERS_H
