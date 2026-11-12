//! Op codes of the GA4c ports, mirrored in `RustLuxProp.h`. Codes marked "event" are re-entrant.

pub const ENT_CALLBACK: i32 = 200;
pub const ENT_INTERACT: i32 = 201;
pub const ENT_CLEAR_INTERACT: i32 = 202;
pub const ENT_LOOKAT_ENTER: i32 = 203;
pub const ENT_LOOKAT_LEAVE: i32 = 204;
pub const ENT_CONN_CHANGE_CB: i32 = 205;
pub const ENT_CONN_SEND: i32 = 206;
pub const ENT_CONN_SCRIPT: i32 = 207;

pub const CHEST_HINGE_MIN: i32 = 210;
pub const CHEST_HINGE_MAX: i32 = 211;
pub const CHEST_HINGE_STICKY: i32 = 212;
pub const CHEST_ADD_FORCE: i32 = 213;
pub const CHEST_PAY: i32 = 214;
pub const CHEST_UNLOCK: i32 = 215;
pub const CHEST_LOG: i32 = 216;
pub const CHEST_COMPLETION: i32 = 217;
pub const CHEST_SOUND: i32 = 218;

pub const ITEM_ADD: i32 = 220;
pub const ITEM_DESTROY: i32 = 221;
pub const ITEM_PICK_SOUND: i32 = 222;
pub const ITEM_PICK_MESSAGE: i32 = 223;
pub const ITEM_CALLBACK: i32 = 224;
pub const ITEM_FLASH: i32 = 225;

pub const NPC_ANIM: i32 = 230;
pub const NPC_BONES_RESIZE: i32 = 231;
pub const NPC_BONE_SET: i32 = 232;
pub const NPC_ERROR_NO_SKELETON: i32 = 233;
pub const NPC_ERROR_BONE_ARRAY: i32 = 234;
pub const NPC_ERROR_BONE: i32 = 235;
pub const NPC_BONE_ROTATE: i32 = 236;

pub const NPC_ANIM_AWAKE: i32 = 0;
pub const NPC_ANIM_SLEEP: i32 = 1;
pub const NPC_ANIM_IDLE: i32 = 2;
pub const NPC_ANIM_SLEEP_IDLE: i32 = 3;

pub const SLOT_EMOTION_DESC_CAT: i32 = 100;
pub const SLOT_EMOTION_DESC_ENTRY: i32 = 101;
pub const SLOT_EMOTION_TEXT_CAT: i32 = 102;
pub const SLOT_EMOTION_TEXT_ENTRY: i32 = 103;
pub const SLOT_EMOTION_SOUND: i32 = 104;
pub const SLOT_ITEM_IMAGE: i32 = 110;
pub const SLOT_ITEM_PICK_SOUND: i32 = 111;
pub const SLOT_ITEM_SUB_TYPE: i32 = 112;
pub const SLOT_ITEM_VAL: i32 = 113;
pub const SLOT_ITEM_EXTRA_VAL: i32 = 114;
pub const SLOT_NPC_AREA: i32 = 120;
pub const SLOT_HAND_ANIM_IDLE: i32 = 130;
pub const SLOT_HAND_ANIM_DRAW: i32 = 131;
pub const SLOT_HAND_ANIM_HOLSTER: i32 = 132;
pub const SLOT_HAND_BONE: i32 = 133;
pub const SLOT_HAND_MODEL: i32 = 134;
