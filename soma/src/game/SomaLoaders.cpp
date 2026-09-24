/*
 * See SomaLoaders.h for scope notes.
 */

#include "SomaLoaders.h"

//////////////////////////////////////////////////////////////////////////
// GENERIC ENTITY LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cSomaGenericEntityLoader::cSomaGenericEntityLoader(const tString &asName) : cEntityLoader_Object(asName)
{
	mbLoadAsStatic = true;
	mbCreatesStaticEntity = true;
}

//-----------------------------------------------------------------------

void cSomaGenericEntityLoader::BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
}

//-----------------------------------------------------------------------

void cSomaGenericEntityLoader::AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	// Same instance-var handling as Rebirth's cRebirthGenericEntityLoader /
	// Dark Descent's cLuxStaticPropLoader - the only per-instance override
	// that's meaningful with no gameplay wrapper object to hand it to.
	if (apInstanceVars && mpEntity)
	{
		mpEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, apInstanceVars->GetVarBool("CastShadows", true));
	}

	// Real root cause of a severe, map-filling magenta/maroon corruption
	// (see PORTING_NOTES.md) initially mistaken for a resolution-dependent
	// light-volume bug: real SOMA .ent files mark physics-only, deliberately
	// invisible collision-blocker meshes with
	// <UserDefinedVariables><Var Name="ShowMesh" Value="false" /></...>
	// (confirmed live: entities/technical/block_box/block_box_bed.ent, the
	// exact entity filling most of the screen at 00_01_apartment.hpm's real
	// PlayerStartArea_1 - a bed's crouch-collision volume, never meant to be
	// player-visible in the real game). Dark Descent's own LuxProp.cpp reads
	// this same variable and calls SetVisible() on its wrapped entity
	// (LuxProp.cpp: "pProp->mbShowMesh = GetVarBool(\"ShowMesh\", true); if
	// (mpEntity) mpEntity->SetVisible(pProp->mbShowMesh);") - this port's own
	// SOMA entity loader had no equivalent at all (no gameplay "Prop" wrapper
	// object exists yet for SOMA, per this file's own header comment), so
	// every such collision-only mesh across every real SOMA map rendered
	// fully visible, using whatever raw diffuse texture the artist left on
	// it (block_box.dds is a solid, saturated placeholder color, by design
	// never meant to reach the screen). GetVarBool() here reads directly off
	// `this` (cEntityLoader_Object inherits cResourceVarsObject, and the
	// base class's Load() already calls LoadUserVariables(apRootElem) - the
	// .ent file's own <UserDefinedVariables>, not apInstanceVars's separate
	// per-map-placement <UserVariables> block above - right before calling
	// AfterLoad()), so no extra parsing is needed here.
	//
	// One real sibling gap found verifying this fix at other resolutions: a
	// second, smaller magenta patch remained visible even after the above -
	// entities/technical/block_box/block_box_static.ent, same "technical/
	// block_box" invisible-collision-volume family, but with EntityType=
	// "StaticCollider" and an EMPTY <UserDefinedVariables/> (no Var children
	// at all, confirmed by reading the real file) - GetVarBool("ShowMesh",
	// true) legitimately finds nothing and returns the default, which is
	// wrong specifically for this type: unlike "Prop_Rigid" (a real,
	// sometimes-visible prop that merely happens to default visible),
	// "StaticCollider" is BY DEFINITION collision-only - Dark Descent's own
	// LuxStaticProp-family loaders never register a visible mesh for this
	// type either. So the true no-authored-Var default depends on
	// msEntityType, not a single hardcoded bool.
	if (mpEntity)
	{
		bool bDefaultShowMesh = (msEntityType != "StaticCollider");
		mpEntity->SetVisible(GetVarBool("ShowMesh", bDefaultShowMesh));
	}

	// Map Active="false": script-activated later (e.g. the apartment's Legs), as iLuxProp::OnSetActive.
	// Bodies stay live: rejecting their contacts trips a Newton teardown crash (TASKS.md)
	if (mbActive) return;
	if (mpEntity)
	{
		mpEntity->SetActive(false);
		mpEntity->SetVisible(false);
	}
	for (size_t i = 0; i < mvLights.size(); ++i)
	{
		mvLights[i]->SetVisible(false);
		mvLights[i]->SetActive(false);
	}
	for (size_t i = 0; i < mvParticleSystems.size(); ++i)
	{
		if (mvParticleSystems[i] == NULL) continue;
		mvParticleSystems[i]->SetVisible(false);
		mvParticleSystems[i]->SetActive(false);
	}
	for (size_t i = 0; i < mvBillboards.size(); ++i)
	{
		mvBillboards[i]->SetActive(false);
		mvBillboards[i]->SetVisible(false);
	}
	for (size_t i = 0; i < mvBeams.size(); ++i)
	{
		mvBeams[i]->SetActive(false);
		mvBeams[i]->SetVisible(false);
	}
	for (size_t i = 0; i < mvSoundEntities.size(); ++i) mvSoundEntities[i]->Stop(false);
}

//////////////////////////////////////////////////////////////////////////
// PLAYERSTART AREA LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cSomaAreaLoader_PlayerStart::cSomaAreaLoader_PlayerStart(const tString &asName) : iAreaLoader(asName)
{
	mbCreatesStaticArea = true;
}

//-----------------------------------------------------------------------

void cSomaAreaLoader_PlayerStart::Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld)
{
	cStartPosEntity *pStartPos = apWorld->CreateStartPos(asName);
	pStartPos->SetMatrix(a_mtxTransform);
}

//////////////////////////////////////////////////////////////////////////
// NO-OP AREA LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cSomaAreaLoader_Noop::cSomaAreaLoader_Noop(const tString &asName) : iAreaLoader(asName)
{
	mbCreatesStaticArea = true;
}

//-----------------------------------------------------------------------

void cSomaAreaLoader_Noop::Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld)
{
}

//////////////////////////////////////////////////////////////////////////
// REGISTRATION
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void RegisterSomaLoaders(cResources *apResources)
{
	// Full census from entities/**/*.ent's EntityType="..." across a real
	// SOMA install - see SomaLoaders.h for how this was collected.
	static const char* apEntityTypeNames[] = {
		"Agent_Anglerfish",
		"Agent_Construct_Crawler",
		"Agent_Construct_Worker",
		"Agent_DeepseaSuit",
		"Agent_Flesher",
		"Agent_Humanoid",
		"Agent_Humanoid_NPC",
		"Agent_Infected_Robot",
		"Agent_Puppet",
		"Agent_Remade",
		"Agent_Roomba",
		"Agent_Swarm",
		"Agent_SwimBot",
		"Agent_Viperfish",
		"Critter_CaveSpider",
		"Critter_CrabSmall",
		"Critter_CrabSpider",
		"Critter_FishSmall",
		"Critter_Nautilus",
		"critter_wau_swarm_agent_fish",
		"Prop_Button",
		"Prop_ConstructLure",
		"Prop_Datamine",
		"Prop_EnergySource",
		"Prop_Grab",
		"Prop_HandheldTerminal",
		"Prop_HudObject",
		"Prop_Lamp",
		"Prop_LevelDoor",
		"Prop_Lever",
		"Prop_Meter",
		"Prop_MoveObject",
		"Prop_MovingButton",
		"Prop_OmniSlot",
		"Prop_PhysicsSlideDoor",
		"Prop_PlayerHands",
		"Prop_Push",
		"Prop_Readable",
		"Prop_Rigid",
		"Prop_Slide",
		"Prop_SlideDoor",
		"Prop_SwingDoor",
		"Prop_Tear",
		"Prop_Terminal",
		"Prop_Tool",
		"Prop_Wheel",
		"StaticCollider",
		"StaticProp",
	};
	for (size_t i = 0; i < sizeof(apEntityTypeNames) / sizeof(apEntityTypeNames[0]); ++i)
	{
		apResources->AddEntityLoader(hplNew(cSomaGenericEntityLoader, (apEntityTypeNames[i])));
	}

	apResources->AddAreaLoader(hplNew(cSomaAreaLoader_PlayerStart, ("PlayerStart")));

	// Full census from maps/*/*/*.hpm_Area's AreaType="..." across a real
	// SOMA install, minus PlayerStart (registered above).
	static const char* apNoopAreaTypeNames[] = {
		"AgentRepel",
		"AmbientLight",
		"CameraAnimation",
		"Climb",
		"Crawl",
		"Datamine",
		"Description",
		"Distortion",
		"DoorwayTrigger",
		"EyeTrackingZoom",
		"Hide",
		"InteractAux",
		"Ladder",
		"Liquid",
		"MapTransfer",
		"PathNode",
		"Sit",
		"Soundscape",
		"Sticky",
		"Tool",
		"Trigger",
		"VisibilityArea",
		"VisibilityPortal",
		"Zoom",
	};
	for (size_t i = 0; i < sizeof(apNoopAreaTypeNames) / sizeof(apNoopAreaTypeNames[0]); ++i)
	{
		apResources->AddAreaLoader(hplNew(cSomaAreaLoader_Noop, (apNoopAreaTypeNames[i])));
	}
}

//-----------------------------------------------------------------------
