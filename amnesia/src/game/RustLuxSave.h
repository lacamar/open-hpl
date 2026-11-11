#ifndef LUX_RUST_SAVE_H
#define LUX_RUST_SAVE_H

// C ABI of rust/src/luxsave/ffi.rs. A walker owns the order and decisions of a save/load step;
// every engine call, entity creation and plain field copy is one op performed by the caller's
// dispatch function (ctx, op, a, b) -> int. Op numbers are checked against the Rust enum.

#include <cstddef>
#include <cstdint>

enum
{
	eOhplGa6Ent_Prop = 0,
	eOhplGa6Ent_Enemy = 2,
	eOhplGa6Ent_Other = 255,
};

enum
{
	eOhplGa6Prop_Object = 0,
	eOhplGa6Prop_SwingDoor = 1,
	eOhplGa6Prop_Lamp = 4,
	eOhplGa6Prop_Item = 7,
	eOhplGa6Prop_Chest = 8,
	eOhplGa6Prop_LevelDoor = 10,
	eOhplGa6Prop_Other = 255,
};

enum
{
	eOhplGa6Joint_Ball = 0,
	eOhplGa6Joint_Hinge = 1,
	eOhplGa6Joint_Slider = 2,
	eOhplGa6Joint_Screw = 3,
	eOhplGa6Joint_Other = 255,
};

enum
{
	eOhplGa6Vol_Normal = 0,
	eOhplGa6Vol_Skipped = 1,
};

enum
{
	eOhplGa6Set_Attack = 0,
	eOhplGa6Set_Search = 1,
};

enum
{
	eOhplGa6Kind_Volume = 0,
	eOhplGa6Kind_Speed = 1,
};

enum
{
	eOhplGa6Flag_Saved = 1,
	eOhplGa6Flag_NoEntityParent = 2,
	eOhplGa6Flag_NoParent = 4,
};

enum
{
	eOhplGa6List_Disabled = 0,
	eOhplGa6List_LockedDoors = 1,
	eOhplGa6List_LockedLevelDoors = 2,
	eOhplGa6List_ActiveItems = 3,
	eOhplGa6List_OpenChests = 4,
	eOhplGa6List_UnbrokenContainers = 5,
	eOhplGa6List_BrokenWithActiveItem = 6,
	eOhplGa6List_UnlitLamps = 7,
};

enum
{
	eOhplGa6Func_Look = 0,
	eOhplGa6Func_Interact = 1,
	eOhplGa6Func_Callback = 2,
	eOhplGa6Func_StateChange = 3,
};

enum eOhplGa6Op
{
	eOhplGa6Op_LightHasParent = 1,
	eOhplGa6Op_LightFromBase = 2,
	eOhplGa6Op_LightIsFading = 3,
	eOhplGa6Op_LightFlickerActive = 4,
	eOhplGa6Op_LightWriteDest = 5,
	eOhplGa6Op_LightColorFlickerOn = 6,
	eOhplGa6Op_LightColorCurrent = 7,
	eOhplGa6Op_LightFlickerProps = 8,
	eOhplGa6Op_LightSetActiveVisible = 9,
	eOhplGa6Op_LightSetColorFlicker = 10,
	eOhplGa6Op_SoundFromHead = 11,
	eOhplGa6Op_SoundIsStopped = 12,
	eOhplGa6Op_SoundIsFadingOut = 13,
	eOhplGa6Op_SoundSetStopped = 14,
	eOhplGa6Op_SoundFromTail = 15,
	eOhplGa6Op_SoundToHead = 16,
	eOhplGa6Op_SoundSavedStopped = 17,
	eOhplGa6Op_SoundStop = 18,
	eOhplGa6Op_SoundToTail = 19,
	eOhplGa6Op_PsPresent = 20,
	eOhplGa6Op_PsFromNull = 21,
	eOhplGa6Op_PsFromHead = 22,
	eOhplGa6Op_PsLiveEmitterN = 23,
	eOhplGa6Op_PsResizeEmitters = 24,
	eOhplGa6Op_PsEmitterDying = 25,
	eOhplGa6Op_PsEmitterDead = 26,
	eOhplGa6Op_PsSetEmitter = 27,
	eOhplGa6Op_PsToHead = 28,
	eOhplGa6Op_PsSavedEmitterN = 29,
	eOhplGa6Op_PsSavedEmitterActive = 30,
	eOhplGa6Op_PsKillEmitter = 31,
	eOhplGa6Op_JointExists = 32,
	eOhplGa6Op_JointSetBroken = 33,
	eOhplGa6Op_JointFromBase = 34,
	eOhplGa6Op_JointLiveCtrlN = 35,
	eOhplGa6Op_JointCtrlFrom = 36,
	eOhplGa6Op_JointType = 37,
	eOhplGa6Op_JointFromLimits = 38,
	eOhplGa6Op_JointSavedCtrlN = 39,
	eOhplGa6Op_JointFindCtrl = 40,
	eOhplGa6Op_JointCtrlTo = 41,
	eOhplGa6Op_JointToLimits = 42,
	eOhplGa6Op_MeshFromBase = 43,
	eOhplGa6Op_MeshLiveAnimN = 44,
	eOhplGa6Op_MeshResizeAnims = 45,
	eOhplGa6Op_MeshAnimFrom = 46,
	eOhplGa6Op_MeshLiveSubN = 47,
	eOhplGa6Op_MeshResizeSubs = 48,
	eOhplGa6Op_MeshSubFrom = 49,
	eOhplGa6Op_MeshToBase = 50,
	eOhplGa6Op_MeshSavedAnimN = 51,
	eOhplGa6Op_MeshAnimTo = 52,
	eOhplGa6Op_MeshSavedSubN = 53,
	eOhplGa6Op_MeshSubTo = 54,
	eOhplGa6Op_RopeFromBase = 55,
	eOhplGa6Op_RopeLivePartN = 56,
	eOhplGa6Op_RopePartFrom = 57,
	eOhplGa6Op_RopeToBase = 58,
	eOhplGa6Op_RopeSavedPartN = 59,
	eOhplGa6Op_RopePartTo = 60,
	eOhplGa6Op_RopeFindStartBody = 61,
	eOhplGa6Op_RopeAttachStart = 62,
	eOhplGa6Op_RopeFindEndBody = 63,
	eOhplGa6Op_RopeAttachEnd = 64,
	eOhplGa6Op_EffectFromBase = 65,
	eOhplGa6Op_EffectVolumeCount = 66,
	eOhplGa6Op_EffectResizeVolumes = 67,
	eOhplGa6Op_EffectVolumeKind = 68,
	eOhplGa6Op_EffectEntryPresent = 69,
	eOhplGa6Op_EffectEntryFrom = 70,
	eOhplGa6Op_EffectEntryInactive = 71,
	eOhplGa6Op_EffectToBase = 72,
	eOhplGa6Op_EffectSavedCount = 73,
	eOhplGa6Op_EffectEntryActive = 74,
	eOhplGa6Op_EffectEntryApply = 75,
	eOhplGa6Op_EffectEntryDestroy = 76,
	eOhplGa6Op_MapFromHead = 77,
	eOhplGa6Op_MapLightN = 78,
	eOhplGa6Op_MapLightFlags = 79,
	eOhplGa6Op_MapLightFrom = 80,
	eOhplGa6Op_MapSoundN = 81,
	eOhplGa6Op_MapSoundFlags = 82,
	eOhplGa6Op_MapSoundLoop = 83,
	eOhplGa6Op_MapSoundFrom = 84,
	eOhplGa6Op_MapPsN = 85,
	eOhplGa6Op_MapPsFlags = 86,
	eOhplGa6Op_MapPsDying = 87,
	eOhplGa6Op_MapPsFrom = 88,
	eOhplGa6Op_MapEntN = 89,
	eOhplGa6Op_MapEntSaved = 90,
	eOhplGa6Op_MapEntDestroyMe = 91,
	eOhplGa6Op_MapEntFrom = 92,
	eOhplGa6Op_MapFromTail = 93,
	eOhplGa6Op_MapToHead = 94,
	eOhplGa6Op_MapSkyIsNone = 95,
	eOhplGa6Op_MapSkyNull = 96,
	eOhplGa6Op_MapSkyLoad = 97,
	eOhplGa6Op_MapToFog = 98,
	eOhplGa6Op_MapSavedLightN = 99,
	eOhplGa6Op_MapFindLight = 100,
	eOhplGa6Op_MapLightTo = 101,
	eOhplGa6Op_MapLightWarn = 102,
	eOhplGa6Op_MapSavedSoundN = 103,
	eOhplGa6Op_MapCreateSound = 104,
	eOhplGa6Op_MapSoundTo = 105,
	eOhplGa6Op_MapSoundWarn = 106,
	eOhplGa6Op_MapSavedPsN = 107,
	eOhplGa6Op_MapCreatePs = 108,
	eOhplGa6Op_MapPsTo = 109,
	eOhplGa6Op_MapPsWarn = 110,
	eOhplGa6Op_MapLoadBegin = 111,
	eOhplGa6Op_MapSavedEntN = 112,
	eOhplGa6Op_MapCreateEntity = 113,
	eOhplGa6Op_MapEntLoad = 114,
	eOhplGa6Op_MapLoadEnd = 115,
	eOhplGa6Op_MapSavedRopeN = 116,
	eOhplGa6Op_MapRopeCreate = 117,
	eOhplGa6Op_MapRopeTo = 118,
	eOhplGa6Op_MapAfterLoad = 119,
	eOhplGa6Op_MapVarsTimers = 120,
	eOhplGa6Op_MapLoadedN = 121,
	eOhplGa6Op_MapSetupEntity = 122,
	eOhplGa6Op_MapLoadedActive = 123,
	eOhplGa6Op_MapLoadedUpdate = 124,
	eOhplGa6Op_MapUseItemCallbacks = 125,
	eOhplGa6Op_GmFromHead = 126,
	eOhplGa6Op_GmSnapshot = 127,
	eOhplGa6Op_GmEntN = 128,
	eOhplGa6Op_GmEntFullSave = 129,
	eOhplGa6Op_GmEntFullFrom = 130,
	eOhplGa6Op_GmEntHasCollide = 131,
	eOhplGa6Op_GmEntConnNum = 132,
	eOhplGa6Op_GmEntSaveEntity = 133,
	eOhplGa6Op_GmEntHasFunc = 134,
	eOhplGa6Op_GmEntAddFunc = 135,
	eOhplGa6Op_GmEntActive = 136,
	eOhplGa6Op_GmEntAddDisabled = 137,
	eOhplGa6Op_GmEntType = 138,
	eOhplGa6Op_GmEntSaveEnemy = 139,
	eOhplGa6Op_GmPropHasAttached = 140,
	eOhplGa6Op_GmPropInteractConnNum = 141,
	eOhplGa6Op_GmPropType = 142,
	eOhplGa6Op_GmLocked = 143,
	eOhplGa6Op_GmAddList = 144,
	eOhplGa6Op_GmItemContainerId = 145,
	eOhplGa6Op_GmObjectHasContained = 146,
	eOhplGa6Op_GmLampLit = 147,
	eOhplGa6Op_GmListN = 148,
	eOhplGa6Op_GmListGet = 149,
	eOhplGa6Op_GmToHead = 150,
	eOhplGa6Op_GmToVarsCallbacksTimers = 151,
	eOhplGa6Op_GmSavedEntityN = 152,
	eOhplGa6Op_GmSavedEntityFound = 153,
	eOhplGa6Op_GmToEntity = 154,
	eOhplGa6Op_GmSavedEntityError = 155,
	eOhplGa6Op_GmSavedEnemyN = 156,
	eOhplGa6Op_GmSavedEnemyFound = 157,
	eOhplGa6Op_GmToEnemy = 158,
	eOhplGa6Op_GmSavedEnemyError = 159,
	eOhplGa6Op_GmPlayerCallbacksTo = 160,
	eOhplGa6Op_GmEntClearFuncs = 161,
	eOhplGa6Op_GmFuncN = 162,
	eOhplGa6Op_GmFuncFound = 163,
	eOhplGa6Op_GmFuncApply = 164,
	eOhplGa6Op_GmSavedLightN = 165,
	eOhplGa6Op_GmSavedSoundN = 166,
	eOhplGa6Op_GmSoundId = 167,
	eOhplGa6Op_GmFindSound = 168,
	eOhplGa6Op_GmSoundWarn = 169,
	eOhplGa6Op_GmSavedPsN = 170,
	eOhplGa6Op_GmPsId = 171,
	eOhplGa6Op_GmFindPs = 172,
	eOhplGa6Op_GmLivePsN = 173,
	eOhplGa6Op_GmLivePsFlags = 174,
	eOhplGa6Op_GmLivePsId = 175,
	eOhplGa6Op_GmLivePsKill = 176,
	eOhplGa6Op_GmFullN = 177,
	eOhplGa6Op_GmFullFind = 178,
	eOhplGa6Op_GmFullCreate = 179,
	eOhplGa6Op_GmFullLoad = 180,
	eOhplGa6Op_GmFullId = 181,
	eOhplGa6Op_GmEntId = 182,
	eOhplGa6Op_GmEntDestroy = 183,
	eOhplGa6Op_GmUpdateDestroyed = 184,
	eOhplGa6Op_GmEntSetActive = 185,
	eOhplGa6Op_GmSetLocked = 186,
	eOhplGa6Op_GmSetLevelDoorLocked = 187,
	eOhplGa6Op_GmEntSaved = 188,
	eOhplGa6Op_GmObjectClear = 189,
	eOhplGa6Op_GmSetChestLocked = 190,
	eOhplGa6Op_GmSetLampLit = 191,
	eOhplGa6Op_GeFromId = 192,
	eOhplGa6Op_GeIsProp = 193,
	eOhplGa6Op_GeFromProp = 194,
	eOhplGa6Op_GeFromConns = 195,
	eOhplGa6Op_GeFromCollide = 196,
	eOhplGa6Op_GeToAttached = 197,
	eOhplGa6Op_GeSavedInteractN = 198,
	eOhplGa6Op_GeCreateInteract = 199,
	eOhplGa6Op_GeAddInteract = 200,
	eOhplGa6Op_GeToConns = 201,
	eOhplGa6Op_GeToCollide = 202,
	eOhplGa6Op_GenToHead = 203,
	eOhplGa6Op_GenHasPathFinder = 204,
	eOhplGa6Op_GenSavedNodeN = 205,
	eOhplGa6Op_GenFindNode = 206,
	eOhplGa6Op_GenAddNode = 207,
	eOhplGa6Op_MusicHasSong = 208,
	eOhplGa6Op_MusicFromSong = 209,
	eOhplGa6Op_MusicFromNoSong = 210,
	eOhplGa6Op_MusicFromMisc = 211,
	eOhplGa6Op_MusicFromEntries = 212,
	eOhplGa6Op_MusicEnemyN = 213,
	eOhplGa6Op_MusicEnemyId = 214,
	eOhplGa6Op_MusicAddAttackerId = 215,
	eOhplGa6Op_MusicHasSavedSong = 216,
	eOhplGa6Op_MusicPlay = 217,
	eOhplGa6Op_MusicHasCurrentSong = 218,
	eOhplGa6Op_MusicApplySong = 219,
	eOhplGa6Op_MusicToMisc = 220,
	eOhplGa6Op_MusicLiveEntryN = 221,
	eOhplGa6Op_MusicSavedEntryN = 222,
	eOhplGa6Op_MusicEntryTo = 223,
	eOhplGa6Op_MusicSavedIdN = 224,
	eOhplGa6Op_MusicSavedId = 225,
	eOhplGa6Op_MusicFindEnemy = 226,
	eOhplGa6Op_MusicInsertEnemy = 227,
	eOhplGa6Op_MusicWarn = 228,
	eOhplGa6Op_InsToCount = 229,
	eOhplGa6Op_InsSavedEventN = 230,
	eOhplGa6Op_InsLiveEventN = 231,
	eOhplGa6Op_InsEventTo = 232,
	eOhplGa6Op_InsClearSets = 233,
	eOhplGa6Op_InsSavedSetN = 234,
	eOhplGa6Op_InsInsertSet = 235,
	eOhplGa6Op_InvToDisabled = 236,
	eOhplGa6Op_InvEquippedIndex = 237,
	eOhplGa6Op_InvEquip = 238,
	eOhplGa6Op_InvEquipNone = 239,
	eOhplGa6Op_InvToItems = 240,
	eOhplGa6Op_InvToCallbacks = 241,
	eOhplGa6Op_HandsHasObject = 242,
	eOhplGa6Op_HandsFromObject = 243,
	eOhplGa6Op_HandsFromNoObject = 244,
	eOhplGa6Op_HandsFromRest = 245,
	eOhplGa6Op_HandsToState = 246,
	eOhplGa6Op_HandsHasSavedObject = 247,
	eOhplGa6Op_HandsFindObject = 248,
	eOhplGa6Op_HandsHasCurrentObject = 249,
	eOhplGa6Op_HandsAttachObject = 250,
	eOhplGa6Op_HandsToRest = 251,
	eOhplGa6Op_HandsIdleCondition = 252,
	eOhplGa6Op_HandsIdleAfterLoad = 253,
	eOhplGa6Op_PlayerStateSaved = 254,
	eOhplGa6Op_PlayerFromState = 255,
	eOhplGa6Op_PlayerFromNoState = 256,
	eOhplGa6Op_PlayerFromBlock = 257,
	eOhplGa6Op_PlayerToStateBefore = 258,
	eOhplGa6Op_PlayerHasStateData = 259,
	eOhplGa6Op_PlayerLoadStateBefore = 260,
	eOhplGa6Op_PlayerChangeState = 261,
	eOhplGa6Op_PlayerLoadStateAfter = 262,
	eOhplGa6Op_PlayerToBlock1 = 263,
	eOhplGa6Op_PlayerSpawnPsActive = 264,
	eOhplGa6Op_PlayerSpawnPsStart = 265,
	eOhplGa6Op_PlayerSpawnPsStop = 266,
	eOhplGa6Op_PlayerToBlock2 = 267,
	eOhplGa6Op_PlayerHeadPosAddsMatch = 268,
	eOhplGa6Op_PlayerToHeadPosAdds = 269,
	eOhplGa6Op_PlayerSavedTerrorN = 270,
	eOhplGa6Op_PlayerSavedTerrorId = 271,
	eOhplGa6Op_PlayerFindEnemy = 272,
	eOhplGa6Op_PlayerInsertTerror = 273,
	eOhplGa6Op_PlayerWarnTerror = 274,
	eOhplGa6Op_PlayerToBlock3 = 275,
};

struct OhplGa6Date
{
	int seconds, minutes, hours, month_day, month, year;
};

struct OhplGa6NameState
{
	int count;
	OhplGa6Date latest;
};

struct OhplGa6Str
{
	const unsigned char *p;
	size_t n;
};

typedef int (*OhplGa6Call)(void *ctx, unsigned op, int a, int b);
typedef void (*OhplGa6Sink32)(void *ctx, const unsigned *p, size_t n);
typedef void (*OhplGa6Translate)(void *ctx, const unsigned *p, size_t n, OhplGa6Sink32 sink, void *sctx);

extern "C"
{
void ohpl_ga6_light_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_light_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_sound_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_sound_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_ps_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_ps_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_joint_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_joint_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_mesh_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_mesh_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_rope_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_rope_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_effects_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_effects_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_music_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_music_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_insanity_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_inventory_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_hands_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_hands_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_player_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_player_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_map_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_map_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_gamemap_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_gamemap_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_entity_from(void *ctx, OhplGa6Call call);
void ohpl_ga6_entity_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_enemy_to(void *ctx, OhplGa6Call call);
void ohpl_ga6_save_name(OhplGa6NameState *st, const unsigned *prefix, size_t prefix_n, const unsigned *map, size_t map_n, const OhplGa6Date *date, OhplGa6Sink32 sink, void *sctx);
void ohpl_ga6_proper_name(const unsigned *file, size_t n, OhplGa6Translate translate, void *tctx, OhplGa6Sink32 sink, void *sctx);
int ohpl_ga6_newest(const OhplGa6Date *dates, size_t n);
size_t ohpl_ga6_rotation_victims(const OhplGa6Date *dates, size_t n, int max, int *out);
bool ohpl_ga6_hardmode_file_ok(const unsigned *file, size_t n);
int ohpl_ga6_find_map(const OhplGa6Str *names, size_t n, const unsigned char *name, size_t name_n);
}

#endif // LUX_RUST_SAVE_H
