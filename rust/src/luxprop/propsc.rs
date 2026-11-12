//! `cLuxProp_Chest`, `_EmotionStone`, `_Item`, `_NPC` and the static-prop loader: `.ent` variable
//! parsing, lock/pickup/wake state machines and the NPC head tracking. Joints, animations, bones,
//! inventory and messages stay in C++ as ops.

use super::object::{MoveData, load_rotate};
use super::opc as op_c;
use super::ops::{Op, Ops, Pod};
use super::vars::{Vars, put_var};
use crate::math::{
    TWO_PI, angle_distance, clamp, dot3, fma, matrix_inverse, matrix_mul_vec, matrix_quaternion,
    normalize3, quat_from_angle_axis, to_rad, v3, vector3_angle,
};
use crate::string;
use crate::types::{Matrixf, Quaternion, Vector3f};

pub const CROSSHAIR_DEFAULT: i32 = 0;
pub const CROSSHAIR_GRAB: i32 = 1;

// ---- chest ----

pub fn chest_load(v: &dyn Vars) -> MoveData {
    let mut d = MoveData::default();
    load_rotate(v, &mut d);
    d
}

pub fn chest_load_instance(v: &dyn Vars) -> i32 {
    v.int("CoinsNeeded", 0)
}

pub fn chest_crosshair(locked: bool) -> i32 {
    if locked {
        CROSSHAIR_GRAB
    } else {
        CROSSHAIR_DEFAULT
    }
}

pub const CHEST_NO_INTERACT: i32 = 0;
pub const CHEST_NO_COINS: i32 = 1;
pub const CHEST_QUESTION: i32 = 2;

/// `OnInteract`: which message the locked chest shows.
pub fn chest_interact(locked: bool, coins_needed: i32, coins: i32) -> i32 {
    if !locked {
        CHEST_NO_INTERACT
    } else if coins < coins_needed {
        CHEST_NO_COINS
    } else {
        CHEST_QUESTION
    }
}

/// `cLuxChestMessageCallback::OnPress`.
pub fn chest_press(yes: bool, coins_needed: i32, ops: &mut Ops) {
    if !yes {
        return;
    }
    ops.push(Op::new(op_c::CHEST_PAY).i(coins_needed.wrapping_neg()));
    ops.event(Op::new(op_c::CHEST_UNLOCK));
    ops.push(Op::new(op_c::CHEST_LOG));
    ops.push(Op::new(op_c::CHEST_COMPLETION));
    ops.push(Op::new(op_c::CHEST_SOUND));
}

/// `SetLocked`: the hinge limits per joint (`max_angles` is the open angle of each hinge).
pub fn chest_set_locked(locked: Pod<bool>, new: bool, max_angles: &[f32], ops: &mut Ops) {
    if *locked.r() == new {
        return;
    }
    *locked.m() = new;
    for (i, &max) in (0..).zip(max_angles) {
        if new {
            ops.push(Op::new(op_c::CHEST_HINGE_MIN).i(i).f(to_rad(0.0)));
            ops.push(Op::new(op_c::CHEST_HINGE_MAX).i(i).f(to_rad(2.0)));
        } else {
            ops.push(Op::new(op_c::CHEST_HINGE_MAX).i(i).f(max));
            ops.push(Op::new(op_c::CHEST_HINGE_MIN).i(i).f(max - to_rad(2.0)));
        }
        ops.push(Op::new(op_c::CHEST_HINGE_STICKY).i(i));
        ops.push(Op::new(op_c::CHEST_ADD_FORCE).i(i));
    }
}

/// `OnConnectionStateChange`: the lock state to set.
pub fn chest_conn_locked(state: i32) -> bool {
    state > 0
}

// ---- emotion stone ----

pub fn emotion_load_instance(v: &dyn Vars) {
    put_var(v, op_c::SLOT_EMOTION_DESC_CAT, "DescCat");
    put_var(v, op_c::SLOT_EMOTION_DESC_ENTRY, "DescEntry");
    put_var(v, op_c::SLOT_EMOTION_TEXT_CAT, "TextCat");
    put_var(v, op_c::SLOT_EMOTION_TEXT_ENTRY, "TextEntry");
    put_var(v, op_c::SLOT_EMOTION_SOUND, "Sound");
}

/// `GetFocusText`: does the stone have a description to show?
pub fn emotion_has_text(cat_empty: bool, entry_empty: bool) -> bool {
    !(cat_empty || entry_empty)
}

// ---- item ----

pub const KIND_COINS: i32 = 1;
pub const KIND_HEALTH: i32 = 2;
pub const KIND_SANITY: i32 = 3;
pub const KIND_LAMP_OIL: i32 = 4;
pub const KIND_NOTE: i32 = 5;
pub const KIND_DIARY: i32 = 6;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ItemAmount {
    pub set: bool,
    pub amount: f32,
}

/// `LoadVariables`: the strings go to their slots, the amount is the kind's variable.
pub fn item_load(v: &dyn Vars, kind: i32) -> ItemAmount {
    put_var(v, op_c::SLOT_ITEM_IMAGE, "ImageFile");
    v.put(
        op_c::SLOT_ITEM_PICK_SOUND,
        &v.string("PickSound", b"Generic"),
    );
    put_var(v, op_c::SLOT_ITEM_SUB_TYPE, "SubItemTypeName");
    let name = match kind {
        KIND_COINS => "CoinAmount",
        KIND_HEALTH | KIND_SANITY => "HealAmount",
        KIND_LAMP_OIL => "OilAmount",
        _ => return ItemAmount::default(),
    };
    ItemAmount {
        set: true,
        amount: v.float(name, 0.0),
    }
}

/// `LoadInstanceVariables`.
pub fn item_load_instance(v: &dyn Vars, kind: i32) -> ItemAmount {
    let custom = v.string("CustomSubItemTypeName", b"");
    if !custom.is_empty() {
        v.put(op_c::SLOT_ITEM_SUB_TYPE, &custom);
    }
    let mut out = ItemAmount::default();
    if kind == KIND_NOTE {
        put_var(v, op_c::SLOT_ITEM_VAL, "NoteText");
        let mut amount = if v.boolean("OpenNoteInJournal", true) {
            1.0f32
        } else {
            -1.0f32
        };
        amount *= if v.boolean("UseNarration", false) {
            2.0f32
        } else {
            1.0f32
        };
        out = ItemAmount { set: true, amount };
    } else if kind == KIND_DIARY {
        put_var(v, op_c::SLOT_ITEM_VAL, "DiaryText");
        put_var(v, op_c::SLOT_ITEM_EXTRA_VAL, "DiaryCallback");
    }
    out
}

pub fn item_can_interact(destroy_me: bool) -> bool {
    !destroy_me
}

/// What `AddItem` reports back, written by C++ into the prop.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ItemState {
    pub remove_prop: bool,
    pub flash_alpha: f32,
}

pub trait ItemHost {
    fn show_pickup(&mut self) -> bool;
    fn sub_mesh_num(&mut self) -> i32;
    fn sub_mesh_visible(&mut self, i: i32) -> bool;
}

/// `OnInteract`: add to the inventory, and when it took the item remove the prop.
pub fn item_interact(s: Pod<ItemState>, host: &mut dyn ItemHost, ops: &mut Ops) {
    s.m().remove_prop = true;
    ops.event(Op::new(op_c::ITEM_ADD));
    if s.r().remove_prop {
        ops.event(Op::new(op_c::ITEM_DESTROY));
        ops.push(Op::new(op_c::ITEM_PICK_SOUND));
        if host.show_pickup() {
            ops.push(Op::new(op_c::ITEM_PICK_MESSAGE));
        }
        ops.event(Op::new(op_c::ITEM_CALLBACK));
    }
}

/// `UpdatePropSpecific`: the flash fades in within four metres of the player.
pub fn item_update(
    s: Pod<ItemState>,
    dt: f32,
    camera: Vector3f,
    body: Vector3f,
    host: &mut dyn ItemHost,
    ops: &mut Ops,
) {
    let dx = body.x - camera.x;
    let dz = body.z - camera.z;
    let dist_sqr = fma(dz, dz, dx * dx);
    if dist_sqr < 4.0f32 * 4.0f32 {
        s.m().flash_alpha += dt;
        if s.r().flash_alpha > 1.0 {
            s.m().flash_alpha = 1.0;
        }
    } else {
        s.m().flash_alpha -= dt;
        if s.r().flash_alpha < 0.0 {
            s.m().flash_alpha = 0.0;
        }
    }
    if s.r().flash_alpha > 0.0 {
        let mut i = 0;
        while i < host.sub_mesh_num() {
            if host.sub_mesh_visible(i) {
                ops.push(Op::new(op_c::ITEM_FLASH).i(i).f(s.r().flash_alpha));
            }
            i += 1;
        }
    }
}

// ---- NPC ----

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct NpcParams {
    pub head_move_speed_mul: f32,
    pub head_move_max_speed: f32,
    pub max_head_angle: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct NpcState {
    pub head_angle: f32,
    pub in_area_count: f32,
    pub player_in_area: bool,
    pub awake: bool,
    pub playing_wake_anim: bool,
    pub follow_player: bool,
}

impl NpcState {
    pub fn new() -> Self {
        Self {
            head_angle: 0.0,
            in_area_count: 0.0,
            player_in_area: false,
            awake: true,
            playing_wake_anim: false,
            follow_player: true,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct NpcBone {
    pub idx: i32,
    pub mul: f32,
    pub rot_vec: Vector3f,
}

pub trait Skeleton {
    fn present(&mut self) -> bool;
    fn bone_index(&mut self, name: &[u8]) -> i32;
    fn bone_world(&mut self, idx: i32) -> Matrixf;
}

/// `LoadVariables`: the head-move bone table is announced through ops, the numbers returned.
pub fn npc_load(v: &dyn Vars, skel: &mut dyn Skeleton, ops: &mut Ops) -> NpcParams {
    let names_raw = v.string("MoveHeadBones", b"");
    let names: Vec<&[u8]> = string::split(&names_raw, None, true).collect();
    let muls_raw = v.string("MoveHeadBoneMuls", b"");
    let muls: Vec<f32> = string::split(&muls_raw, None, true)
        .map(string::token_to_float)
        .collect();
    if !skel.present() {
        ops.push(Op::new(op_c::NPC_ERROR_NO_SKELETON));
    } else if names.is_empty() || names.len() != muls.len() {
        ops.push(Op::new(op_c::NPC_ERROR_BONE_ARRAY));
    } else {
        ops.push(Op::new(op_c::NPC_BONES_RESIZE).i(i32::try_from(names.len()).unwrap_or(i32::MAX)));
        for (i, (name, &mul)) in (0..).zip(names.iter().zip(&muls)) {
            let idx = skel.bone_index(name);
            if idx < -1 {
                ops.push(Op::new(op_c::NPC_ERROR_BONE).i(i).j(idx));
                continue;
            }
            let inv = matrix_inverse(&skel.bone_world(idx));
            let mut rot = matrix_mul_vec(&inv, v3(0.0, 1.0, 0.0));
            normalize3(&mut rot);
            ops.push(Op::new(op_c::NPC_BONE_SET).i(i).j(idx).f(mul).v(rot));
        }
    }
    NpcParams {
        head_move_speed_mul: v.float("HeadMoveSpeedMul", 0.0),
        head_move_max_speed: v.float("HeadMoveMaxSpeed", 0.0),
        max_head_angle: to_rad(v.float("MaxHeadAngle", 0.0)),
    }
}

/// `LoadInstanceVariables`: the area name goes to its slot, the result is the initial awake flag.
pub fn npc_load_instance(v: &dyn Vars) -> bool {
    put_var(v, op_c::SLOT_NPC_AREA, "FollowPlayerArea");
    v.boolean("Awake", true)
}

fn npc_anim(anim: i32, fade: f32, looped: bool, ops: &mut Ops) {
    ops.push(Op::new(op_c::NPC_ANIM).i(anim).j(i32::from(looped)).f(fade));
}

/// `SetAwake`.
pub fn npc_set_awake(s: Pod<NpcState>, awake: bool, effects: bool, ops: &mut Ops) {
    if s.r().awake == awake {
        return;
    }
    s.m().awake = awake;
    if effects {
        let anim = if awake {
            op_c::NPC_ANIM_AWAKE
        } else {
            op_c::NPC_ANIM_SLEEP
        };
        npc_anim(anim, 0.3, false, ops);
        s.m().playing_wake_anim = true;
    } else {
        let anim = if awake {
            op_c::NPC_ANIM_IDLE
        } else {
            op_c::NPC_ANIM_SLEEP_IDLE
        };
        npc_anim(anim, 0.05, true, ops);
    }
}

pub fn npc_update_wake(s: Pod<NpcState>, non_loop_anim: i32, ops: &mut Ops) {
    if !s.r().playing_wake_anim {
        return;
    }
    if non_loop_anim < 0 {
        s.m().playing_wake_anim = false;
        let anim = if s.r().awake {
            op_c::NPC_ANIM_IDLE
        } else {
            op_c::NPC_ANIM_SLEEP_IDLE
        };
        npc_anim(anim, 0.3, true, ops);
    }
}

pub const AREA_FIND: i32 = 1;
pub const AREA_WARN_EMPTY: i32 = 2;

/// `AfterWorldLoad`: warn about a missing area name, otherwise look the area up.
pub fn npc_after_world_load(area_name_empty: bool) -> i32 {
    if area_name_empty {
        AREA_WARN_EMPTY
    } else {
        AREA_FIND
    }
}

/// `AfterWorldLoad` after the lookup: true reports the missing area.
pub fn npc_area_missing(found: bool) -> bool {
    !found
}

pub trait NpcHost {
    fn check_area_collision(&mut self) -> bool;
    fn mesh_world(&mut self) -> Matrixf;
    fn mesh_position(&mut self) -> Vector3f;
    fn player_position(&mut self) -> Vector3f;
}

/// `UpdateCheckPlayerIsInArea`: the collision test runs twice a second.
pub fn npc_update_area(s: Pod<NpcState>, has_area: bool, dt: f32, host: &mut dyn NpcHost) {
    if !has_area {
        return;
    }
    s.m().in_area_count -= dt;
    if s.r().in_area_count > 0.0 {
        return;
    }
    s.m().in_area_count = 0.5;
    let in_area = host.check_area_collision();
    s.m().player_in_area = in_area;
}

fn flat(mut v: Vector3f) -> Vector3f {
    v.y = 0.0;
    normalize3(&mut v);
    v
}

/// `UpdateHeadMovement`: turn towards the player, then rotate the head bones.
pub fn npc_update_head(
    s: Pod<NpcState>,
    p: &NpcParams,
    bones: &[NpcBone],
    dt: f32,
    host: &mut dyn NpcHost,
    ops: &mut Ops,
) {
    let mut wanted = 0.0f32;
    if s.r().player_in_area && s.r().awake && s.r().follow_player {
        let inv = matrix_inverse(&host.mesh_world());
        let right = flat(v3(inv.m[0][0], inv.m[0][1], inv.m[0][2]));
        let fwd = flat(v3(inv.m[2][0], inv.m[2][1], inv.m[2][2]));
        let (player, mesh) = (host.player_position(), host.mesh_position());
        let to_player = flat(v3(player.x - mesh.x, player.y - mesh.y, player.z - mesh.z));
        wanted = vector3_angle(fwd, to_player);
        if dot3(right, to_player) < 0.0 {
            wanted = -wanted;
        }
        wanted = clamp(wanted, -p.max_head_angle, p.max_head_angle);
    }
    if (s.r().head_angle - wanted).abs() > 0.0001f32 {
        let dist = angle_distance(s.r().head_angle, wanted, TWO_PI);
        let speed = clamp(
            p.head_move_speed_mul * dist,
            -p.head_move_max_speed,
            p.head_move_max_speed,
        );
        let head = fma(speed, dt, s.r().head_angle);
        s.m().head_angle = head;
    }
    for b in bones {
        if b.idx < 0 {
            continue;
        }
        let mut q = Quaternion::default();
        quat_from_angle_axis(&mut q, s.r().head_angle * b.mul, b.rot_vec);
        ops.push(
            Op::new(op_c::NPC_BONE_ROTATE)
                .i(b.idx)
                .m(matrix_quaternion(&q)),
        );
    }
}

// ---- static prop ----

/// `cLuxStaticPropLoader::AfterLoad`: whether the mesh casts shadows.
pub fn static_cast_shadows(v: &dyn Vars) -> bool {
    v.boolean("CastShadows", true)
}
