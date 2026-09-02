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
