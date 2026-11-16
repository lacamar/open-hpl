// Engine host ops of the script API (rust/src/luxscript/fns): each case wraps one call on a C++ game object.
// The op list mirrors rust/src/luxscript/fns/ops.rs (checked by a cargo test).

#include "LuxBase.h"
#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxMapHandler.h"
#include "LuxInputHandler.h"
#include "LuxMoveState_Normal.h"
#include "LuxSaveHandler.h"
#include "LuxPlayerState.h"
#include "LuxDebugHandler.h"
#include "LuxEffectHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxMessageHandler.h"
#include "LuxMusicHandler.h"
#include "LuxHintHandler.h"
#include "LuxInsanityHandler.h"
#include "LuxCompletionCountHandler.h"
#include "LuxGlobalDataHandler.h"
#include "LuxProgressLogHandler.h"
#include "LuxLoadScreenHandler.h"
#include "LuxCredits.h"
#include "LuxDemoEnd.h"
#include "LuxAchievementHandler.h"

#include "LuxProp_Object.h"
#include "LuxProp_SwingDoor.h"
#include "LuxProp_Lamp.h"
#include "LuxProp_Lever.h"
#include "LuxProp_Wheel.h"
#include "LuxProp_MoveObject.h"
#include "LuxProp_Item.h"
#include "LuxProp_LevelDoor.h"
#include "LuxProp_Button.h"
#include "LuxProp_NPC.h"
#include "LuxProp_MultiSlider.h"
#include "LuxEnemy_ManPig.h"
#include "LuxArea_Sticky.h"
#include "LuxEnemy.h"
#include "LuxInteractConnections.h"

#include "RustLuxFxA12a.h"
#include "RustLuxScriptA1.h"
#include "RustLuxScript.h"

static_assert(eLuxEntityType_Prop == 0 && eLuxEntityType_Area == 1 && eLuxEntityType_Enemy == 2 && eLuxEntityType_LastEnum == 5, "a1 entity types");
static_assert(eLuxPropType_Object == 0 && eLuxPropType_SwingDoor == 1 && eLuxPropType_Lever == 2 && eLuxPropType_Wheel == 3 &&
			  eLuxPropType_Lamp == 4 && eLuxPropType_MoveObject == 6 && eLuxPropType_Item == 7 && eLuxPropType_LevelDoor == 10 &&
			  eLuxPropType_Button == 11 && eLuxPropType_NPC == 14 && eLuxPropType_MultiSlider == 15, "a1 prop types");
static_assert(eLuxAreaType_Sticky == 4, "a1 area types");
static_assert(eLuxEnemyState_Wait == 2 && eLuxEnemyState_Search == 6 && eLuxEnemyState_Hunt == 9 && eLuxEnemyState_AttackMeleeShort == 12 &&
			  eLuxEnemyState_AttackMeleeLong == 13 && eLuxEnemyState_BreakDoor == 15, "a1 enemy states");
static_assert(eOhplA12aPost_RadialBlur == 0 && eOhplA12aPost_Sepia == 1 && eOhplA12aPost_ImageTrail == 2, "a1 post kinds");
static_assert(sizeof(cMatrixf) == 64, "a1 matrix");

static const eLuxAchievement gvA1Achievements[] =
{
	eLuxAchievement_Insanity, eLuxAchievement_Benefactor, eLuxAchievement_Survivor, eLuxAchievement_Sacrifice,
	eLuxAchievement_Quitter, eLuxAchievement_Egotist, eLuxAchievement_Altruist, eLuxAchievement_Vacillator,
	eLuxAchievement_Alchemist, eLuxAchievement_Pipeworker, eLuxAchievement_Restorer, eLuxAchievement_Masochist,
};

enum OhplA1Op
{
	TEMP_STR,
	WARNING,
	ERROR,
	MAP_EXISTS,
	MAP_ENTITY_TYPED,
	MAP_ENTITIES,
	MAP_ENTITY_NTH,
	ENT_IS_TYPE,
	ENT_NAME,
	MAP_ENTITY,
	MAP_VAR,
	GLOBAL_VAR,
	VAR_SET,
	LOG,
	DEBUG_MSG,
	PROG_LOG,
	DEBUG_SCRIPT_ON,
	MAP_ADD_TIMER,
	MAP_REMOVE_TIMER,
	MAP_TIMER_LEFT,
	CREDITS_SETUP,
	UPDATER_CONTAINER,
	CREDITS_KEY_PART,
	DEMO_END_EXISTS,
	AUTO_SAVE,
	MAP_CHECKPOINT,
	DEATH_HINT,
	CHANGE_MAP,
	CLEAR_SAVED_MAPS,
	DATA_CACHE,
	MAP_DISPLAY_NAME,
	LOAD_SCREEN,
	ACHIEVEMENT,
	HARD_MODE,
	GUI_SOUND_DATA,
	GUI_SOUND,
	HANDLE,
	TRANSLATE,
	ENT_BODY_NUM,
	ENT_BODY_POS,
	MAP_QUEST_COMPLETION,
	MAP_SET_QUESTS,
	INPUT_IN_INVENTORY,
	MAP_CREATE_ENTITY,
	ITEM_FIELD,
	MAP_DESTROY_ENTITY,
	MAP_USE_ITEM_ADD,
	MAP_USE_ITEM_REMOVE,
	P_ACTIVE,
	P_STATE_NORMAL,
	P_MOVE_STATE_NORMAL,
	P_CROUCH,
	P_BODY_AXES,
	P_BODY_ADD_FORCE,
	P_SCRIPT_CROSSHAIR,
	P_SET_FEET,
	P_FEET,
	P_SET_SANITY,
	P_ADD_SANITY,
	P_SANITY,
	P_SET_HEALTH,
	P_ADD_HEALTH,
	P_HEALTH,
	P_SET_LAMP_OIL,
	P_ADD_LAMP_OIL,
	P_LAMP_OIL,
	P_VELOCITY,
	P_MOVE_FORWARD,
	P_PERMA_DEATH_SOUND,
	P_SANITY_DRAIN_DISABLED,
	P_GIVE_SANITY_DAMAGE,
	P_LOWER_SANITY,
	P_GIVE_DAMAGE,
	P_FADE_PITCH,
	P_FADE_FOV,
	P_FADE_ASPECT,
	P_FADE_ROLL,
	P_HEAD_POS_ADD,
	ENT_HAS_MESH,
	ENT_MESH_CENTER,
	P_LOOK_AT_TARGET,
	P_LOOK_AT_ACTIVE,
	P_MOVE_SPEED_MUL,
	P_RUN_SPEED_MUL,
	P_LOOK_SPEED_MUL,
	P_JUMP_FORCE_MUL,
	P_JUMP_DISABLED,
	P_CROUCH_DISABLED,
	P_NO_FALL_DAMAGE,
	P_DRAG_FOOTSTEPS,
	MAP_PLAYER_START,
	P_PLACE_AT_START,
	LANTERN_ACTIVE_SET,
	LANTERN_ACTIVE,
	LANTERN_DISABLED,
	MAP_LANTERN_CALLBACK,
	DEATH_DISABLE_START_SOUND,
	P_IN_DARKNESS_FX,
	P_SPAWN_PS,
	P_FLASHBACK_ACTIVE,
	ENT_BODY,
	BODY_NAME,
	ENT_FULL_SAVE,
	ENT_ACTIVE,
	ENT_MESH_VISIBLE,
	ENT_TYPE,
	ENEMY_SET_FEET,
	BODY_SET_WORLD_POS,
	ENEMY_FEET,
	BODY_WORLD_POS,
	ENT_CROSSHAIR,
	AREA_MATRIX,
	BODY_MATRIX,
	BODY_SET_MATRIX,
	ENT_LOOK_AT_CB,
	ENT_INTERACT_CB,
	ENT_CALLBACK_FUNC,
	ENT_CONNECTION_CB,
	PROP_INTERACTION_DISABLED,
	ENT_COLLIDE,
	P_ADD_COLLIDE_CB,
	ENT_ADD_COLLIDE_CB,
	P_REMOVE_COLLIDE_CB,
	ENT_REMOVE_COLLIDE_CB,
	BODY_LOCAL_MATRIX,
	BODY_ADD_FORCE,
	BODY_ADD_IMPULSE,
	MAP_PHYS_BODY,
	MAP_PHYS_JOINT,
	JOINT_BREAK,
	BODY_SET_MASS,
	BODY_MASS,
	MAP_PHYS_ROPE,
	PROP_CONNECT_ROPE,
	PROP_CONNECT_MOVE_OBJECT,
	ENT_ADD_CONNECTION,
	PROP_PAUSE_ANIM,
	PROP_PLAY_ANIM,
	PROP_ANIM_POS,
	PROP_ANIM_SPEED,
	PROP_STOP_ANIM,
	PROP_PLAY_ANIMATION,
	PROP_EFFECTS_ACTIVE,
	PROP_STATIC_PHYSICS,
	PROP_STOP_MOVE,
	PROP_SET_HEALTH,
	PROP_RESET,
	LAMP_FADE_TO,
	LAMP_CAN_BE_LIT,
	LAMP_SET_LIT,
	BUTTON_CAN_OFF,
	BUTTON_CAN_ON,
	BUTTON_SWITCHED_ON,
	DOOR_OPEN_AMOUNT,
	DOOR_LOCKED,
	DOOR_CLOSED,
	DOOR_DISABLE_AUTO_CLOSE,
	LEVEL_DOOR_LOCKED,
	LEVEL_DOOR_SOUND,
	LEVEL_DOOR_TEXT,
	OBJECT_STUCK,
	WHEEL_ANGLE,
	WHEEL_STUCK,
	LEVER_STUCK,
	WHEEL_INTERACTION,
	LEVER_INTERACTION,
	MULTI_SLIDER_STUCK,
	MULTI_SLIDER_CB,
	NPC_AWAKE,
	NPC_FOLLOW,
	MOVE_OBJECT_STATE,
	MOVE_OBJECT_STATE_EXT,
	DOOR_GET_LOCKED,
	DOOR_GET_CLOSED,
	DOOR_GET_STATE,
	LEVER_GET_STATE,
	PROP_INTERACTED_WITH,
	PROP_HEALTH,
	PROP_FADE_IN_MESH,
	PROP_DISSOLVE,
	PROP_MAIN_BODY_MATRIX,
	AREA_POSITION,
	PROP_ROTATE,
	PROP_ADD_ATTACH,
	PROP_DESTROY_ATTACHED,
	AREA_ATTACH_TO_BODY,
	ENT_BONE,
	PROP_SET_PARENT_BONE,
	PROP_DETACH_BONE,
	STICKY_ALLOW,
	PROP_MAIN_BODY,
	STICKY_ATTACH_BODY,
	STICKY_DETACH,
	ENEMY_CHANGE_STATE,
	ENEMY_PLAYER_DISTANCE,
	ENEMY_STATE,
	ENEMY_SEEN,
	ENEMY_STATE_NAME,
	ENEMY_CLEAR_PATROL,
	ENEMY_PLAY_ANIM,
	ENEMY_SCRIPTED_ANIM,
	ENEMY_MESSAGE,
	ENEMY_ACTIVATION_DISTANCE,
	ENEMY_BLIND,
	ENEMY_DEAF,
	ENEMY_DISABLED,
	ENEMY_HALLUCINATION,
	ENEMY_FADE_TO_SMOKE,
	ENEMY_DISABLE_TRIGGERS,
	ENEMY_SANITY_DECREASE,
	ENEMY_SHOW_PLAYER_POS,
	ENEMY_HAS_NODES,
	ENEMY_NODE,
	ENEMY_ADD_PATROL,
	NODE_POSITION,
	ENT_AS_MANPIG,
	MANPIG_POSE,
	TESLA_FADE_DISABLED,
	TESLA_SOUND_DISABLED,
	TESLA_EASY_ESCAPE_DISABLED,
	TESLA_FORCE_SIGHTING,
	PRELOAD_PS,
	PRELOAD_SOUND,
	MAP_CREATE_PS,
	PS_AT_PLAYER,
	PS_SAVED,
	PS_MATRIX_FROM_ENTITY,
	PS_ATTACH_TO_ENTITY,
	PS_SETTINGS,
	PS_KILL,
	SOUND_FADE_IN,
	MAP_CREATE_SOUND,
	SOUND_SET_POS,
	SOUND_SAVED,
	SOUND_FORCE_GUI,
	SOUND_POS_FROM_ENTITY,
	SOUND_ATTACH_TO_ENTITY,
	MAP_SOUND,
	SOUND_PLAY,
	SOUNDS_STOP,
	MAP_LIGHT,
	LIGHT_VISIBLE,
	LIGHT_FLICKER,
	LIGHT_DIFFUSE,
	LIGHT_RADIUS,
	LIGHT_FADE,
	SOUND_GLOBAL_VALUE,
	SOUND_GLOBAL_FADE,
	WORLD_SKYBOX_ACTIVE,
	WORLD_SKYBOX_TEXTURE,
	WORLD_SKYBOX_COLOR,
	WORLD_FOG_ACTIVE,
	WORLD_FOG_COLOR,
	WORLD_FOG_PROPS,
};

static tString gvTempStr[3];
static tString gsTemp;
static tWString gsTranslated;
static std::vector<iLuxEntity*> gvEntities;
static cMatrixf gmtxTemp;
static float gvAxes[9];

static cLuxMap *CurMap() { return gpBase->mpMapHandler->GetCurrentMap(); }
static iLuxProp *Prop(const OhplA1Arg &a) { return static_cast<iLuxProp*>(const_cast<void*>(a.p)); }
static iLuxEntity *Ent(const OhplA1Arg &a) { return static_cast<iLuxEntity*>(const_cast<void*>(a.p)); }
static iLuxEnemy *Enemy(const OhplA1Arg &a) { return static_cast<iLuxEnemy*>(const_cast<void*>(a.p)); }
static iLuxArea *Area(const OhplA1Arg &a) { return static_cast<iLuxArea*>(const_cast<void*>(a.p)); }
static iPhysicsBody *Body(const OhplA1Arg &a) { return static_cast<iPhysicsBody*>(const_cast<void*>(a.p)); }
static cLuxEnemy_ManPig *ManPig(const OhplA1Arg &a) { return dynamic_cast<cLuxEnemy_ManPig*>(Ent(a)); }
static cParticleSystem *PS(const OhplA1Arg &a) { return static_cast<cParticleSystem*>(const_cast<void*>(a.p)); }
static cSoundEntity *Snd(const OhplA1Arg &a) { return static_cast<cSoundEntity*>(const_cast<void*>(a.p)); }
static iLight *Light(const OhplA1Arg &a) { return static_cast<iLight*>(const_cast<void*>(a.p)); }
static const cMatrixf &Mtx(const OhplA1Arg &a) { return *static_cast<const cMatrixf*>(a.p); }
static cVector3f Vec(const OhplA1Arg *a) { return cVector3f(a[0].F(), a[1].F(), a[2].F()); }
static float Comp(const cVector3f &v, int axis) { return axis == 0 ? v.x : (axis == 1 ? v.y : v.z); }
static cColor Col(const OhplA1Arg *a) { return cColor(a[0].F(), a[1].F(), a[2].F(), a[3].F()); }

extern "C" void OhplA1HostCall2(unsigned op, const OhplA1Arg *a, size_t n, OhplA1Arg *r)
{
	switch((OhplA1Op)op)
	{
	case LOG: Log("%s", OhplA1Str(a[0]).c_str()); break;
	case WARNING: Warning("%s", OhplA1Str(a[0]).c_str()); break;
	case ERROR: Error("%s", OhplA1Str(a[0]).c_str()); break;
	case DEBUG_MSG: gpBase->mpDebugHandler->AddMessage(cString::To16Char(OhplA1Str(a[0])), a[1].B()); break;
	case PROG_LOG: gpBase->mpProgressLogHandler->AddLog((eLuxProgressLogLevel)a[0].I(), OhplA1Str(a[1])); break;
	case DEBUG_SCRIPT_ON: r->SetB(gpBase->mpDebugHandler->GetScriptDebugOn()); break;
	case TEMP_STR: gvTempStr[a[0].I()] = OhplA1Str(a[1]); r->p = &gvTempStr[a[0].I()]; break;
	case MAP_EXISTS: r->SetB(CurMap() != NULL); break;
	case MAP_ADD_TIMER: CurMap()->AddTimer(OhplA1Str(a[0]), a[1].F(), OhplA1Str(a[2])); break;
	case MAP_REMOVE_TIMER: CurMap()->RemoveTimer(OhplA1Str(a[0])); break;
	case MAP_TIMER_LEFT:
	{
		float fCount;
		r->SetF(CurMap()->GetTimerCount(OhplA1Str(a[0]), &fCount) ? fCount : 0);
		break;
	}
	case MAP_VAR: { cLuxScriptVar *v = CurMap()->GetVar(OhplA1Str(a[0])); r->p = v ? &v->msVal : NULL; break; }
	case GLOBAL_VAR: { cLuxScriptVar *v = gpBase->mpGlobalDataHandler->GetVar(OhplA1Str(a[0])); r->p = v ? &v->msVal : NULL; break; }
	case VAR_SET: *static_cast<tString*>(const_cast<void*>(a[0].p)) = OhplA1Str(a[1]); break;
	case CREDITS_SETUP: gpBase->mpCredits->Setup(OhplA1Str(a[0]), a[1].B(), OhplA1Str(a[2]), OhplA1Str(a[3]), a[4].I()); break;
	case CREDITS_KEY_PART: gpBase->mpCredits->AddKeyPart(a[0].I()); break;
	case DEMO_END_EXISTS: r->SetB(gpBase->mpDemoEnd != NULL); break;
	case UPDATER_CONTAINER: gpBase->mpEngine->GetUpdater()->SetContainer(OhplA1Str(a[0])); break;
	case AUTO_SAVE: gpBase->mpSaveHandler->AutoSave(); break;
	case MAP_CHECKPOINT: CurMap()->SetCheckPoint(OhplA1Str(a[0]), OhplA1Str(a[1]), OhplA1Str(a[2])); break;
	case DEATH_HINT: gpBase->mpPlayer->GetHelperDeath()->SetHint(OhplA1Str(a[0]), OhplA1Str(a[1])); break;
	case DEATH_DISABLE_START_SOUND: gpBase->mpPlayer->GetHelperDeath()->DisableStartSound(); break;
	case CHANGE_MAP: gpBase->mpMapHandler->ChangeMap(OhplA1Str(a[0]), OhplA1Str(a[1]), OhplA1Str(a[2]), OhplA1Str(a[3])); break;
	case CLEAR_SAVED_MAPS: gpBase->mpMapHandler->ClearSaveMapCollection(); break;
	case DATA_CACHE: if(a[0].B()) gpBase->mpMapHandler->CreateDataCache(); else gpBase->mpMapHandler->DestroyDataCache(); break;
	case MAP_DISPLAY_NAME: CurMap()->SetDisplayNameEntry(OhplA1Str(a[0])); break;
	case LOAD_SCREEN: gpBase->mpLoadScreenHandler->SetupLoadText(OhplA1Str(a[0]), OhplA1Str(a[1]), a[2].I(), OhplA1Str(a[3])); break;
	case ACHIEVEMENT: gpBase->mpAchievementHandler->UnlockAchievement(gvA1Achievements[a[0].I()]); break;
	case HARD_MODE: r->SetB(gpBase->mbHardMode); break;
	case GUI_SOUND_DATA: gpBase->mpHelpFuncs->PlayGuiSoundData(OhplA1Str(a[0]), eSoundEntryType_Gui, a[1].F()); break;
	case GUI_SOUND: gpBase->mpEngine->GetSound()->GetSoundHandler()->PlayGui(OhplA1Str(a[0]), false, a[1].F()); break;
	case MAP_ENTITY_TYPED: r->p = CurMap()->GetEntityByName(OhplA1Str(a[0]), (eLuxEntityType)a[1].I(), a[2].I()); break;
	case MAP_ENTITY: r->p = CurMap()->GetEntityByName(OhplA1Str(a[0])); break;
	case MAP_ENTITIES:
	{
		gvEntities.clear();
		cLuxEntityIterator it = CurMap()->GetEntityIterator();
		while(it.HasNext()) gvEntities.push_back(it.Next());
		r->SetI((int)gvEntities.size());
		break;
	}
	case MAP_ENTITY_NTH: r->p = gvEntities[a[0].I()]; break;
	case ENT_IS_TYPE: r->SetB(LuxIsCorrectType(Ent(a[0]), (eLuxEntityType)a[1].I(), a[2].I())); break;
	case ENT_NAME: gsTemp = Ent(a[0])->GetName(); r->p = &gsTemp; break;
	case HANDLE:
		switch(a[0].I())
		{
		case 0: r->u = gpBase->mpEffectHandler->GetHandle(); break;
		case 1: r->u = gpBase->mpHintHandler->GetHandle(); break;
		case 2: r->u = gpBase->mpMessageHandler->GetHandle(); break;
		case 3: r->u = gpBase->mpInsanityHandler->GetHandle(); break;
		default: r->u = gpBase->mpMusicHandler->GetHandle(); break;
		}
		break;
	case TRANSLATE: gsTranslated = kTranslate(OhplA1Str(a[0]), OhplA1Str(a[1])); r->p = gsTranslated.data(); r->n = gsTranslated.size(); break;
	case INPUT_IN_INVENTORY: r->SetB(gpBase->mpInputHandler->GetState() == eLuxInputState_Inventory); break;
	case MAP_QUEST_COMPLETION: CurMap()->AddCompletionAmount(gpBase->mpCompletionCountHandler->mlQuestCompletionValue, a[0].F()); break;
	case MAP_SET_QUESTS: CurMap()->SetNumberOfQuests(a[0].I()); break;
	case MAP_USE_ITEM_ADD: CurMap()->AddUseItemCallback(OhplA1Str(a[0]), OhplA1Str(a[1]), OhplA1Str(a[2]), OhplA1Str(a[3]), a[4].B()); break;
	case MAP_USE_ITEM_REMOVE: CurMap()->RemoveUseItemCallback(OhplA1Str(a[0])); break;
	case MAP_CREATE_ENTITY:
		CurMap()->ResetLatestEntity();
		CurMap()->CreateEntity(OhplA1Str(a[0]), OhplA1Str(a[1]), Mtx(a[2]), 1);
		r->p = CurMap()->GetLatestEntity();
		break;
	case MAP_DESTROY_ENTITY: CurMap()->DestroyEntity(Ent(a[0])); break;
	case ITEM_FIELD:
	{
		cLuxProp_Item *pItem = static_cast<cLuxProp_Item*>(Prop(a[0]));
		switch(a[1].I())
		{
		case 0: r->SetI(pItem->GetItemType()); break;
		case 1: gsTemp = pItem->GetSubItemTypeName(); r->p = &gsTemp; break;
		case 2: gsTemp = pItem->GetImageFile(); r->p = &gsTemp; break;
		default: r->SetF(pItem->GetAmount()); break;
		}
		break;
	}

	// entities and bodies
	case ENT_BODY_NUM: r->SetI(Ent(a[0])->GetBodyNum()); break;
	case ENT_BODY: r->p = Ent(a[0])->GetBody(a[1].I()); break;
	case ENT_BODY_POS: r->SetF(Comp(Ent(a[0])->GetBody(0)->GetLocalPosition(), a[1].I())); break;
	case ENT_HAS_MESH: r->SetB(Ent(a[0])->GetMeshEntity() != NULL); break;
	case ENT_MESH_VISIBLE: Ent(a[0])->GetMeshEntity()->SetVisible(a[1].B()); break;
	case ENT_MESH_CENTER: r->SetF(Comp(Ent(a[0])->GetMeshEntity()->GetBoundingVolume()->GetWorldCenter(), a[1].I())); break;
	case ENT_ACTIVE: Ent(a[0])->SetActive(a[1].B()); break;
	case ENT_TYPE: r->SetI(Ent(a[0])->GetEntityType()); break;
	case ENT_FULL_SAVE: Ent(a[0])->SetFullGameSave(a[1].B()); break;
	case ENT_CROSSHAIR: Ent(a[0])->SetCustomFocusCrossHair((eLuxFocusCrosshair)a[1].I()); break;
	case ENT_LOOK_AT_CB: Ent(a[0])->SetPlayerLookAtCallback(OhplA1Str(a[1]), a[2].B()); break;
	case ENT_INTERACT_CB: Ent(a[0])->SetPlayerInteractCallback(OhplA1Str(a[1]), a[2].B()); break;
	case ENT_CALLBACK_FUNC: Ent(a[0])->SetCallbackFunc(OhplA1Str(a[1])); break;
	case ENT_CONNECTION_CB: Ent(a[0])->SetConnectionStateChangeCallback(OhplA1Str(a[1])); break;
	case ENT_COLLIDE: r->SetB(Ent(a[0])->CheckEntityCollision(Ent(a[1]), CurMap())); break;
	case ENT_ADD_COLLIDE_CB: Ent(a[0])->AddCollideCallback(Ent(a[1]), OhplA1Str(a[2]), a[3].B(), a[4].I()); break;
	case ENT_REMOVE_COLLIDE_CB: Ent(a[0])->RemoveCollideCallback(OhplA1Str(a[1])); break;
	case P_ADD_COLLIDE_CB: gpBase->mpPlayer->AddCollideCallback(Ent(a[0]), OhplA1Str(a[1]), a[2].B(), a[3].I()); break;
	case P_REMOVE_COLLIDE_CB: gpBase->mpPlayer->RemoveCollideCallback(OhplA1Str(a[0])); break;
	case ENT_ADD_CONNECTION: Ent(a[0])->AddConnection(OhplA1Str(a[1]), Ent(a[2]), a[3].B(), a[4].I(), OhplA1Str(a[5])); break;
	case ENT_BONE: r->p = Ent(a[0])->GetMeshEntity()->GetBoneStateFromName(OhplA1Str(a[1])); break;
	case BODY_NAME: gsTemp = Body(a[0])->GetName(); r->p = &gsTemp; break;
	case BODY_WORLD_POS: r->SetF(Comp(Body(a[0])->GetWorldPosition(), a[1].I())); break;
	case BODY_SET_WORLD_POS: Body(a[0])->SetWorldPosition(Vec(a + 1)); break;
	case BODY_MATRIX: gmtxTemp = Body(a[0])->GetWorldMatrix(); r->p = &gmtxTemp; break;
	case BODY_LOCAL_MATRIX: gmtxTemp = Body(a[0])->GetLocalMatrix(); r->p = &gmtxTemp; break;
	case BODY_SET_MATRIX: Body(a[0])->SetWorldMatrix(Mtx(a[1])); break;
	case BODY_ADD_FORCE: Body(a[0])->AddForce(Vec(a + 1)); break;
	case BODY_ADD_IMPULSE: Body(a[0])->AddImpulse(Vec(a + 1)); break;
	case BODY_SET_MASS: Body(a[0])->SetMass(a[1].F()); break;
	case BODY_MASS: r->SetF(Body(a[0])->GetMass()); break;
	case MAP_PHYS_BODY: r->p = CurMap()->GetPhysicsWorld()->GetBody(OhplA1Str(a[0])); break;
	case MAP_PHYS_JOINT: r->p = CurMap()->GetPhysicsWorld()->GetJoint(OhplA1Str(a[0])); break;
	case MAP_PHYS_ROPE: r->p = CurMap()->GetPhysicsWorld()->GetRope(OhplA1Str(a[0])); break;
	case JOINT_BREAK: static_cast<iPhysicsJoint*>(const_cast<void*>(a[0].p))->Break(); break;
	case AREA_MATRIX: gmtxTemp = Area(a[0])->GetBody()->GetWorldMatrix(); r->p = &gmtxTemp; break;
	case AREA_POSITION: r->SetF(Comp(Area(a[0])->GetPosition(), a[1].I())); break;
	case AREA_ATTACH_TO_BODY: Area(a[0])->AttachToBody(OhplA1Str(a[1]), a[2].I()); break;
	case STICKY_ALLOW: cLuxArea_Sticky::SetAllowAttachment(a[0].B()); break;
	case STICKY_ATTACH_BODY: static_cast<cLuxArea_Sticky*>(Ent(a[0]))->AttachBody(Body(a[1])); break;
	case STICKY_DETACH: static_cast<cLuxArea_Sticky*>(Ent(a[0]))->DetachBody(); break;

	// props
	case PROP_INTERACTION_DISABLED: Prop(a[0])->SetInteractionDisabled(a[1].B()); break;
	case PROP_PAUSE_ANIM: Prop(a[0])->PauseCurrentAnimation(a[1].F()); break;
	case PROP_PLAY_ANIM: Prop(a[0])->PlayCurrentAnimation(a[1].F(), a[2].B()); break;
	case PROP_ANIM_POS: Prop(a[0])->SetAnimationPosition(a[1].F()); break;
	case PROP_ANIM_SPEED: Prop(a[0])->SetAnimationSpeed(a[1].F()); break;
	case PROP_STOP_ANIM: Prop(a[0])->StopAnimation(); break;
	case PROP_PLAY_ANIMATION: Prop(a[0])->PlayAnimation(OhplA1Str(a[1]), a[2].F(), a[3].B(), OhplA1Str(a[4])); break;
	case PROP_EFFECTS_ACTIVE: Prop(a[0])->SetEffectsActive(a[1].B(), a[2].B()); break;
	case PROP_STATIC_PHYSICS: Prop(a[0])->SetStaticPhysics(a[1].B()); break;
	case PROP_STOP_MOVE: Prop(a[0])->StopMove(); break;
	case PROP_SET_HEALTH: Prop(a[0])->SetHealth(a[1].F()); break;
	case PROP_HEALTH: r->SetF(Prop(a[0])->GetHealth()); break;
	case PROP_RESET: Prop(a[0])->ResetProperties(); break;
	case PROP_INTERACTED_WITH: r->SetB(Prop(a[0])->IsInteractedWith()); break;
	case PROP_FADE_IN_MESH: Prop(a[0])->FadeInMeshEntity(a[1].F()); break;
	case PROP_DISSOLVE: CurMap()->AddDissolveEntity(Prop(a[0])->GetMeshEntity(), a[1].F()); break;
	case PROP_MAIN_BODY: r->p = Prop(a[0])->GetMainBody(); break;
	case PROP_MAIN_BODY_MATRIX: gmtxTemp = Prop(a[0])->GetMainBody()->GetWorldMatrix(); r->p = &gmtxTemp; break;
	case PROP_ROTATE: Prop(a[0])->RotateAtSpeed(a[1].F(), a[2].F(), cVector3f(a[3].F(), a[4].F(), a[5].F()), a[6].B(), a[7].B(), Vec(a + 8), Vec(a + 11)); break;
	case PROP_ADD_ATTACH: Prop(a[0])->AddAndAttachProp(OhplA1Str(a[1]), OhplA1Str(a[2]), Mtx(a[3])); break;
	case PROP_DESTROY_ATTACHED: r->SetB(Prop(a[0])->DestroyAttachedProp(OhplA1Str(a[1]))); break;
	case PROP_SET_PARENT_BONE: Prop(a[0])->SetParentBone(static_cast<cBoneState*>(const_cast<void*>(a[1].p)), Mtx(a[2]), OhplA1Str(a[3]), OhplA1Str(a[4])); break;
	case PROP_DETACH_BONE: Prop(a[0])->DetachFromParentBone(); break;
	case PROP_CONNECT_ROPE:
	{
		cLuxInteractConnection_Rope *pConnection = hplNew(cLuxInteractConnection_Rope, (OhplA1Str(a[2]), Prop(a[0]),
			static_cast<iPhysicsRope*>(const_cast<void*>(a[1].p)), a[3].F(), a[4].F(), a[5].F(), a[6].B(), a[7].I()));
		pConnection->SetInteractionOnly(a[8].B());
		Prop(a[0])->AddInteractConnection(pConnection);
		break;
	}
	case PROP_CONNECT_MOVE_OBJECT:
	{
		cLuxInteractConnection_MoveObject *pConnection = hplNew(cLuxInteractConnection_MoveObject, (OhplA1Str(a[2]), Prop(a[0]),
			static_cast<cLuxProp_MoveObject*>(Ent(a[1])), a[3].B(), a[4].I()));
		pConnection->SetInteractionOnly(a[5].B());
		Prop(a[0])->AddInteractConnection(pConnection);
		break;
	}
	case LAMP_FADE_TO: static_cast<cLuxProp_Lamp*>(Prop(a[0]))->FadeTo(a[1].F(), a[2].F(), a[3].F(), a[4].F(), a[5].F(), a[6].F()); break;
	case LAMP_CAN_BE_LIT: static_cast<cLuxProp_Lamp*>(Prop(a[0]))->SetCanBeLitByPlayer(a[1].B()); break;
	case LAMP_SET_LIT: static_cast<cLuxProp_Lamp*>(Prop(a[0]))->SetLit(a[1].B(), a[2].B()); break;
	case BUTTON_CAN_OFF: static_cast<cLuxProp_Button*>(Prop(a[0]))->SetCanBeSwitchedOff(a[1].B()); break;
	case BUTTON_CAN_ON: static_cast<cLuxProp_Button*>(Prop(a[0]))->SetCanBeSwitchedOn(a[1].B()); break;
	case BUTTON_SWITCHED_ON: static_cast<cLuxProp_Button*>(Prop(a[0]))->SetSwitchedOn(a[1].B(), a[2].B()); break;
	case DOOR_OPEN_AMOUNT: static_cast<cLuxProp_SwingDoor*>(Prop(a[0]))->SetOpenAmount(a[1].F(), a[2].F(), a[3].B()); break;
	case DOOR_LOCKED: static_cast<cLuxProp_SwingDoor*>(Prop(a[0]))->SetLocked(a[1].B(), a[2].B()); break;
	case DOOR_CLOSED: static_cast<cLuxProp_SwingDoor*>(Prop(a[0]))->SetClosed(a[1].B(), a[2].B()); break;
	case DOOR_DISABLE_AUTO_CLOSE: static_cast<cLuxProp_SwingDoor*>(Prop(a[0]))->SetDisableAutoClose(a[1].B()); break;
	case DOOR_GET_LOCKED: r->SetB(static_cast<cLuxProp_SwingDoor*>(Prop(a[0]))->GetLocked()); break;
	case DOOR_GET_CLOSED: r->SetB(static_cast<cLuxProp_SwingDoor*>(Prop(a[0]))->GetClosed()); break;
	case DOOR_GET_STATE: r->SetI(static_cast<cLuxProp_SwingDoor*>(Prop(a[0]))->GetDoorState()); break;
	case LEVEL_DOOR_LOCKED: static_cast<cLuxProp_LevelDoor*>(Prop(a[0]))->SetLocked(a[1].B()); break;
	case LEVEL_DOOR_SOUND: static_cast<cLuxProp_LevelDoor*>(Prop(a[0]))->SetLockedSound(OhplA1Str(a[1])); break;
	case LEVEL_DOOR_TEXT: static_cast<cLuxProp_LevelDoor*>(Prop(a[0]))->SetLockedText(OhplA1Str(a[1]), OhplA1Str(a[2])); break;
	case OBJECT_STUCK: static_cast<cLuxProp_Object*>(Prop(a[0]))->SetStuckState(a[1].I()); break;
	case WHEEL_ANGLE: static_cast<cLuxProp_Wheel*>(Prop(a[0]))->SetAngle(a[1].F(), a[2].B()); break;
	case WHEEL_STUCK: static_cast<cLuxProp_Wheel*>(Prop(a[0]))->SetStuckState(a[1].I(), a[2].B()); break;
	case WHEEL_INTERACTION: static_cast<cLuxProp_Wheel*>(Prop(a[0]))->SetInteractionDisablesStuck(a[1].B()); break;
	case LEVER_STUCK: static_cast<cLuxProp_Lever*>(Prop(a[0]))->SetStuckState(a[1].I(), a[2].B()); break;
	case LEVER_INTERACTION: static_cast<cLuxProp_Lever*>(Prop(a[0]))->SetInteractionDisablesStuck(a[1].B()); break;
	case LEVER_GET_STATE: r->SetI(static_cast<cLuxProp_Lever*>(Prop(a[0]))->GetLeverState()); break;
	case MULTI_SLIDER_STUCK: static_cast<cLuxProp_MultiSlider*>(Prop(a[0]))->SetStuckState(a[1].I(), a[2].B()); break;
	case MULTI_SLIDER_CB: static_cast<cLuxProp_MultiSlider*>(Prop(a[0]))->SetChangeStateCallback(OhplA1Str(a[1])); break;
	case NPC_AWAKE: static_cast<cLuxProp_NPC*>(Prop(a[0]))->SetAwake(a[1].B(), a[2].B()); break;
	case NPC_FOLLOW: static_cast<cLuxProp_NPC*>(Prop(a[0]))->SetFollowPlayer(a[1].B()); break;
	case MOVE_OBJECT_STATE: static_cast<cLuxProp_MoveObject*>(Prop(a[0]))->MoveToState(a[1].F()); break;
	case MOVE_OBJECT_STATE_EXT: static_cast<cLuxProp_MoveObject*>(Prop(a[0]))->MoveToState(a[1].F(), a[2].F(), a[3].F(), a[4].F(), a[5].B()); break;

	// enemies
	case ENEMY_SET_FEET: Enemy(a[0])->GetCharacterBody()->SetFeetPosition(Vec(a + 1)); break;
	case ENEMY_FEET: r->SetF(Comp(Enemy(a[0])->GetCharacterBody()->GetFeetPosition(), a[1].I())); break;
	case ENEMY_CHANGE_STATE: Enemy(a[0])->ChangeState((eLuxEnemyState)a[1].I()); break;
	case ENEMY_PLAYER_DISTANCE: r->SetF(Enemy(a[0])->GetPlayerDistance()); break;
	case ENEMY_STATE: r->SetI(Enemy(a[0])->GetCurrentEnemyState()); break;
	case ENEMY_SEEN: r->SetB(Enemy(a[0])->GetIsSeenByPlayer()); break;
	case ENEMY_STATE_NAME: r->p = &Enemy(a[0])->GetCurrentEnemyStateName(); break;
	case ENEMY_CLEAR_PATROL: Enemy(a[0])->ClearPatrolNodes(); break;
	case ENEMY_PLAY_ANIM: Enemy(a[0])->PlayAnim(OhplA1Str(a[1]), a[2].B(), a[3].F()); break;
	case ENEMY_SCRIPTED_ANIM: Enemy(a[0])->PlayScriptedAnimation(OhplA1Str(a[1]), a[2].B()); break;
	case ENEMY_MESSAGE: Enemy(a[0])->SendMessage(a[1].I() == 0 ? eLuxEnemyMessage_TimeOut : eLuxEnemyMessage_StopPatrolAnimation, a[2].F()); break;
	case ENEMY_ACTIVATION_DISTANCE: Enemy(a[0])->SetActivationDistance(a[1].F()); break;
	case ENEMY_BLIND: Enemy(a[0])->SetBlind(a[1].B()); break;
	case ENEMY_DEAF: Enemy(a[0])->SetDeaf(a[1].B()); break;
	case ENEMY_DISABLED: Enemy(a[0])->SetDisabled(a[1].B()); break;
	case ENEMY_HALLUCINATION: Enemy(a[0])->SetHallucination(a[1].B()); break;
	case ENEMY_FADE_TO_SMOKE: Enemy(a[0])->FadeToSmoke(a[1].B()); break;
	case ENEMY_DISABLE_TRIGGERS: Enemy(a[0])->SetDisableTriggers(a[1].B()); break;
	case ENEMY_SANITY_DECREASE: Enemy(a[0])->SetSanityDecreaseActive(a[1].B()); break;
	case ENEMY_SHOW_PLAYER_POS: Enemy(a[0])->ShowPlayerPosition(); break;
	case ENEMY_HAS_NODES: r->p = Enemy(a[0])->GetNodeContainer(); break;
	case ENEMY_NODE:
	{
		cAINodeContainer *pCont = Enemy(a[0])->GetNodeContainer();
		r->p = pCont ? pCont->GetNodeFromName(OhplA1Str(a[1])) : NULL;
		break;
	}
	case NODE_POSITION: r->SetF(Comp(static_cast<cAINode*>(const_cast<void*>(a[0].p))->GetPosition(), a[1].I())); break;
	case ENEMY_ADD_PATROL: Enemy(a[0])->AddPatrolNode(static_cast<cAINode*>(const_cast<void*>(a[1].p)), a[2].F(), OhplA1Str(a[3])); break;
	case ENT_AS_MANPIG: r->SetB(ManPig(a[0]) != NULL); break;
	case MANPIG_POSE: ManPig(a[0])->ChangePose((eLuxEnemyPoseType)a[1].I()); break;
	case TESLA_FADE_DISABLED: ManPig(a[0])->SetTeslaFadeDisabled(a[1].B()); break;
	case TESLA_SOUND_DISABLED: ManPig(a[0])->SetTeslaSoundDisabled(a[1].B()); break;
	case TESLA_EASY_ESCAPE_DISABLED: ManPig(a[0])->SetTeslaEasyEscapeDisabled(a[1].B()); break;
	case TESLA_FORCE_SIGHTING: ManPig(a[0])->ForceTeslaSighting(); break;

	// player
	case P_ACTIVE: gpBase->mpPlayer->SetActive(a[0].B()); break;
	case P_STATE_NORMAL: gpBase->mpPlayer->ChangeState(eLuxPlayerState_Normal); break;
	case P_MOVE_STATE_NORMAL: gpBase->mpPlayer->ChangeMoveState(eLuxMoveState_Normal); break;
	case P_CROUCH: static_cast<cLuxMoveState_Normal*>(gpBase->mpPlayer->GetMoveStateData(eLuxMoveState_Normal))->SetCrouch(a[0].B()); break;
	case P_BODY_AXES:
	{
		iCharacterBody *pBody = gpBase->mpPlayer->GetCharacterBody();
		cVector3f vForward = pBody->GetForward(), vRight = pBody->GetRight(), vUp = pBody->GetUp();
		gvAxes[0] = vForward.x; gvAxes[1] = vForward.y; gvAxes[2] = vForward.z;
		gvAxes[3] = vRight.x; gvAxes[4] = vRight.y; gvAxes[5] = vRight.z;
		gvAxes[6] = vUp.x; gvAxes[7] = vUp.y; gvAxes[8] = vUp.z;
		r->p = gvAxes;
		break;
	}
	case P_BODY_ADD_FORCE: gpBase->mpPlayer->GetCharacterBody()->AddForce(Vec(a)); break;
	case P_SCRIPT_CROSSHAIR: gpBase->mpPlayer->SetScriptShowFocusIconAndCrossHair(a[0].B()); break;
	case P_SET_FEET: gpBase->mpPlayer->GetCharacterBody()->SetFeetPosition(Vec(a)); break;
	case P_FEET: r->SetF(Comp(gpBase->mpPlayer->GetCharacterBody()->GetFeetPosition(), a[0].I())); break;
	case P_SET_SANITY: gpBase->mpPlayer->SetSanity(a[0].F()); break;
	case P_ADD_SANITY: gpBase->mpPlayer->AddSanity(a[0].F()); break;
	case P_SANITY: r->SetF(gpBase->mpPlayer->GetSanity()); break;
	case P_SET_HEALTH: gpBase->mpPlayer->SetHealth(a[0].F()); break;
	case P_ADD_HEALTH: gpBase->mpPlayer->AddHealth(a[0].F()); break;
	case P_HEALTH: r->SetF(gpBase->mpPlayer->GetHealth()); break;
	case P_SET_LAMP_OIL: gpBase->mpPlayer->SetLampOil(a[0].F()); break;
	case P_ADD_LAMP_OIL: gpBase->mpPlayer->AddLampOil(a[0].F()); break;
	case P_LAMP_OIL: r->SetF(gpBase->mpPlayer->GetLampOil()); break;
	case P_VELOCITY:
	{
		cVector3f v = gpBase->mpPlayer->GetCharacterBody()->GetVelocity(1.0f/60.0f);
		r->SetF(a[0].I() == 0 ? v.Length() : v.y);
		break;
	}
	case P_MOVE_FORWARD: gpBase->mpPlayer->GetCharacterBody()->Move(eCharDir_Forward, a[0].F()); break;
	case P_PERMA_DEATH_SOUND: gpBase->mpPlayer->SetCurrentPermaDeathSound(OhplA1Str(a[0])); break;
	case P_SANITY_DRAIN_DISABLED: gpBase->mpPlayer->SetSanityDrainDisabled(a[0].B()); break;
	case P_GIVE_SANITY_DAMAGE: gpBase->mpPlayer->GiveSanityDamage(a[0].F()); break;
	case P_LOWER_SANITY: gpBase->mpPlayer->LowerSanity(a[0].F(), a[1].B()); break;
	case P_GIVE_DAMAGE: gpBase->mpPlayer->GiveDamage(a[0].F(), 1, (eLuxDamageType)a[1].I(), a[2].B(), a[3].B()); break;
	case P_FADE_PITCH: gpBase->mpPlayer->FadePitchTo(a[0].F(), a[1].F(), a[2].F()); break;
	case P_FADE_FOV: gpBase->mpPlayer->FadeFOVMulTo(a[0].F(), a[1].F()); break;
	case P_FADE_ASPECT: gpBase->mpPlayer->FadeAspectMulTo(a[0].F(), a[1].F()); break;
	case P_FADE_ROLL: gpBase->mpPlayer->FadeRollTo(a[0].F(), a[1].F(), a[2].F()); break;
	case P_HEAD_POS_ADD: gpBase->mpPlayer->MoveHeadPosAdd(eLuxHeadPosAdd_Script, Vec(a), a[3].F(), a[4].F()); break;
	case P_LOOK_AT_TARGET: gpBase->mpPlayer->GetHelperLookAt()->SetTarget(Vec(a), a[3].F(), a[4].F(), OhplA1Str(a[5])); break;
	case P_LOOK_AT_ACTIVE: gpBase->mpPlayer->GetHelperLookAt()->SetActive(a[0].B()); break;
	case P_MOVE_SPEED_MUL: gpBase->mpPlayer->SetScriptMoveSpeedMul(a[0].F()); break;
	case P_RUN_SPEED_MUL: gpBase->mpPlayer->SetScriptRunSpeedMul(a[0].F()); break;
	case P_LOOK_SPEED_MUL: gpBase->mpPlayer->SetLookSpeedMul(a[0].F()); break;
	case P_JUMP_FORCE_MUL: gpBase->mpPlayer->SetScriptJumpForceMul(a[0].F()); break;
	case P_JUMP_DISABLED: gpBase->mpPlayer->SetJumpDisabled(a[0].B()); break;
	case P_CROUCH_DISABLED: gpBase->mpPlayer->SetCrouchDisabled(a[0].B()); break;
	case P_NO_FALL_DAMAGE: gpBase->mpPlayer->SetNoFallDamage(a[0].B()); break;
	case P_DRAG_FOOTSTEPS: gpBase->mpPlayer->SetUsesDragFootsteps(a[0].B()); break;
	case MAP_PLAYER_START: r->p = CurMap()->GetPlayerStart(OhplA1Str(a[0])); break;
	case P_PLACE_AT_START: gpBase->mpPlayer->PlaceAtStartNode(static_cast<cLuxNode_PlayerStart*>(const_cast<void*>(a[0].p))); break;
	case LANTERN_ACTIVE_SET: gpBase->mpPlayer->GetHelperLantern()->SetActive(a[0].B(), a[1].B()); break;
	case LANTERN_ACTIVE: r->SetB(gpBase->mpPlayer->GetHelperLantern()->IsActive()); break;
	case LANTERN_DISABLED: gpBase->mpPlayer->GetHelperLantern()->SetDisabled(a[0].B()); break;
	case MAP_LANTERN_CALLBACK: CurMap()->SetLanternLitCallback(OhplA1Str(a[0])); break;
	case P_IN_DARKNESS_FX: gpBase->mpPlayer->GetHelperInDarkness()->SetActive(a[0].B()); break;
	case P_SPAWN_PS: gpBase->mpPlayer->GetHelperSpawnPS()->Start(OhplA1Str(a[0])); break;
	case P_FLASHBACK_ACTIVE: r->SetB(gpBase->mpPlayer->GetHelperFlashback()->IsActive()); break;

	// world: sound, particle systems, lights, sky, fog
	case PRELOAD_PS: gpBase->mpEngine->GetResources()->GetParticleManager()->Preload(OhplA1Str(a[0])); break;
	case PRELOAD_SOUND: gpBase->mpEngine->GetResources()->GetSoundEntityManager()->Preload(OhplA1Str(a[0])); break;
	case MAP_CREATE_PS: r->p = CurMap()->GetWorld()->CreateParticleSystem(OhplA1Str(a[0]), OhplA1Str(a[1]), 1.0f); break;
	case PS_AT_PLAYER: PS(a[0])->SetPosition(gpBase->mpPlayer->GetCharacterBody()->GetPosition()); break;
	case PS_SAVED: PS(a[0])->SetIsSaved(a[1].B()); break;
	case PS_ATTACH_TO_ENTITY: Ent(a[1])->GetAttachEntity()->AddChild(PS(a[0])); break;
	case PS_MATRIX_FROM_ENTITY: PS(a[0])->SetMatrix(Ent(a[1])->GetAttachEntity()->GetWorldMatrix()); break;
	case PS_SETTINGS:
	{
		cParticleSystem *pPS = PS(a[0]);
		pPS->SetColor(Col(a + 1));
		pPS->SetFadeAtDistance(a[5].B());
		pPS->SetMinFadeDistanceEnd(a[6].F());
		pPS->SetMinFadeDistanceStart(a[7].F());
		pPS->SetMaxFadeDistanceStart(a[8].F());
		pPS->SetMaxFadeDistanceEnd(a[9].F());
		break;
	}
	case PS_KILL:
	{
		bool bFound = false;
		tString sName = OhplA1Str(a[0]);
		cParticleSystemIterator it = CurMap()->GetWorld()->GetParticleSystemIterator();
		while(it.HasNext())
		{
			cParticleSystem *pPS = it.Next();
			if(pPS->GetName() == sName)
			{
				if(a[1].B()) pPS->KillInstantly(); else pPS->Kill();
				bFound = true;
			}
		}
		r->SetB(bFound);
		break;
	}
	case MAP_CREATE_SOUND: r->p = CurMap()->GetWorld()->CreateSoundEntity(OhplA1Str(a[0]), OhplA1Str(a[1]), true); break;
	case SOUND_SET_POS: Snd(a[0])->SetPosition(Vec(a + 1)); break;
	case SOUND_SAVED: Snd(a[0])->SetIsSaved(a[1].B()); break;
	case SOUND_FADE_IN: Snd(a[0])->FadeIn(a[1].F()); break;
	case SOUND_FORCE_GUI: Snd(a[0])->SetForcePlayAsGUISound(true); break;
	case SOUND_ATTACH_TO_ENTITY: Ent(a[1])->GetAttachEntity()->AddChild(Snd(a[0])); break;
	case SOUND_POS_FROM_ENTITY: Snd(a[0])->SetPosition(Ent(a[1])->GetAttachEntity()->GetWorldPosition()); break;
	case MAP_SOUND: r->p = CurMap()->GetWorld()->GetSoundEntity(OhplA1Str(a[0])); break;
	case SOUND_PLAY: Snd(a[0])->Play(a[1].B()); break;
	case SOUNDS_STOP:
	{
		bool bFound = false;
		tString sName = OhplA1Str(a[0]);
		float fSpeed = a[1].F();
		cSoundEntityIterator it = CurMap()->GetWorld()->GetSoundEntityIterator();
		while(it.HasNext())
		{
			cSoundEntity *pSound = it.Next();
			if(pSound->GetName() == sName)
			{
				if(fSpeed <= 0) pSound->Stop(true); else pSound->FadeOut(fSpeed);
				bFound = true;
			}
		}
		r->SetB(bFound);
		break;
	}
	case MAP_LIGHT: r->p = CurMap()->GetWorld()->GetLight(OhplA1Str(a[0])); break;
	case LIGHT_VISIBLE: Light(a[0])->SetVisible(a[1].B()); break;
	case LIGHT_FLICKER: Light(a[0])->SetFlickerActive(a[1].B()); break;
	case LIGHT_DIFFUSE:
	{
		cColor c = Light(a[0])->GetDiffuseColor();
		r->SetF(a[1].I() == 0 ? c.r : (a[1].I() == 1 ? c.g : (a[1].I() == 2 ? c.b : c.a)));
		break;
	}
	case LIGHT_RADIUS: r->SetF(Light(a[0])->GetRadius()); break;
	case LIGHT_FADE: Light(a[0])->FadeTo(Col(a + 1), a[5].F(), a[6].F()); break;
	case SOUND_GLOBAL_VALUE:
	{
		cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		cMultipleSettingsHandler *pHandler = a[0].I() == 0 ? pSoundHandler->GetGlobalVolumeSettingsHandler() : pSoundHandler->GetGlobalSpeedSettingsHandler();
		r->SetF(pHandler->GetEntry(eLuxGlobalVolumeType_Script, true)->GetVal());
		break;
	}
	case SOUND_GLOBAL_FADE:
	{
		cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		if(a[0].I() == 0) pSoundHandler->FadeGlobalVolume(a[1].F(), a[2].F(), eSoundEntryType_World, eLuxGlobalVolumeType_Script, a[3].B());
		else pSoundHandler->FadeGlobalSpeed(a[1].F(), a[2].F(), eSoundEntryType_World, eLuxGlobalVolumeType_Script, a[3].B());
		break;
	}
	case WORLD_SKYBOX_ACTIVE: CurMap()->GetWorld()->SetSkyBoxActive(a[0].B()); break;
	case WORLD_SKYBOX_TEXTURE:
	{
		tString sTexture = OhplA1Str(a[0]);
		iTexture *pTexture = sTexture != "" ? gpBase->mpEngine->GetResources()->GetTextureManager()->CreateCubeMap(sTexture, true) : NULL;
		CurMap()->GetWorld()->SetSkyBox(pTexture, true);
		break;
	}
	case WORLD_SKYBOX_COLOR: CurMap()->GetWorld()->SetSkyBoxColor(Col(a)); break;
	case WORLD_FOG_ACTIVE: CurMap()->GetWorld()->SetFogActive(a[0].B()); break;
	case WORLD_FOG_COLOR: CurMap()->GetWorld()->SetFogColor(Col(a)); break;
	case WORLD_FOG_PROPS:
	{
		cWorld *pWorld = CurMap()->GetWorld();
		pWorld->SetFogStart(a[0].F());
		pWorld->SetFogEnd(a[1].F());
		pWorld->SetFogFalloffExp(a[2].F());
		pWorld->SetFogCulling(a[3].B());
		break;
	}
	}
}
