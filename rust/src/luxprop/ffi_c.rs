//! C ABI of the GA4c ports, mirrored in `amnesia/src/game/RustLuxProp.h`: the rest of the entity
//! base, chest, emotion stone, item, NPC, static prop and the hand-object base.
#![allow(clippy::missing_safety_doc, clippy::too_many_arguments)]

use super::entc::{self, CollideHost, ConnHost, EntityCb, LookHost};
use super::entity::LookIn;
use super::ffi::{Sink, VarsRef};
use super::ffi_b::LosRef;
use super::handc;
use super::object::MoveData;
use super::ops::{Op, Ops, Pod};
use super::propsc::Skeleton;
use super::propsc::{self, ItemAmount, ItemHost, ItemState, NpcBone, NpcHost, NpcParams, NpcState};
use crate::types::{Matrixf, Vector3f};
use std::ffi::c_void;

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<EntityCb>() == 12);
    assert!(size_of::<ItemAmount>() == 8);
    assert!(size_of::<ItemState>() == 8);
    assert!(size_of::<NpcParams>() == 12);
    assert!(size_of::<NpcState>() == 12);
    assert!(size_of::<NpcBone>() == 20);
};

fn rd<'a, T>(p: *const T) -> &'a T {
    // SAFETY: caller contract.
    unsafe { &*p }
}

fn vr<'a>(p: *const VarsRef) -> &'a VarsRef {
    // SAFETY: caller contract.
    unsafe { &*p }
}

fn rw<'a, T>(p: *mut T) -> &'a mut T {
    // SAFETY: caller contract.
    unsafe { &mut *p }
}

fn pod<T>(p: *mut T) -> Pod<T> {
    // SAFETY: caller contract: `p` is live state.
    unsafe { Pod::from_ptr(p) }
}

fn sl<'a, T>(p: *const T, n: i32) -> &'a [T] {
    match usize::try_from(n) {
        // SAFETY: caller contract: `p` holds `n` elements.
        Ok(n) if n > 0 && !p.is_null() => unsafe { std::slice::from_raw_parts(p, n) },
        _ => &[],
    }
}

fn with_ops<R>(sink: &Sink, f: impl FnOnce(&mut Ops) -> R) -> R {
    let (ctx, push) = (sink.ctx, sink.push);
    let mut fwd = move |op: &Op| {
        // SAFETY: the sink stays valid for the call.
        unsafe { push(ctx, op) }
    };
    f(&mut Ops(&mut fwd))
}

// ---- entity ----

#[repr(C)]
pub struct LookHostRef {
    pub ctx: *mut c_void,
    pub body_num: unsafe extern "C" fn(*mut c_void) -> i32,
    pub body: unsafe extern "C" fn(*mut c_void, i32, *mut LookIn) -> bool,
}

impl LookHost for &LookHostRef {
    fn body_num(&mut self) -> i32 {
        // SAFETY: the C++ side keeps the table alive for the call.
        unsafe { (self.body_num)(self.ctx) }
    }
    fn body(&mut self, i: i32) -> Option<LookIn> {
        let mut out = LookIn::default();
        // SAFETY: as above.
        unsafe { (self.body)(self.ctx, i, &mut out) }.then_some(out)
    }
}

#[repr(C)]
pub struct ConnHostRef {
    pub ctx: *mut c_void,
    pub conn_num: unsafe extern "C" fn(*mut c_void) -> i32,
    pub conn: unsafe extern "C" fn(*mut c_void, i32, *mut i32, *mut bool),
    pub has_callback: unsafe extern "C" fn(*mut c_void, i32) -> bool,
}

impl ConnHost for &ConnHostRef {
    fn conn_num(&mut self) -> i32 {
        // SAFETY: the C++ side keeps the table alive for the call.
        unsafe { (self.conn_num)(self.ctx) }
    }
    fn conn(&mut self, i: i32) -> (i32, bool) {
        let (mut used, mut invert) = (0, false);
        // SAFETY: as above.
        unsafe { (self.conn)(self.ctx, i, &mut used, &mut invert) };
        (used, invert)
    }
    fn conn_has_callback(&mut self, i: i32) -> bool {
        // SAFETY: as above.
        unsafe { (self.has_callback)(self.ctx, i) }
    }
}

#[repr(C)]
pub struct CollideHostRef {
    pub ctx: *mut c_void,
    pub body_num: unsafe extern "C" fn(*mut c_void) -> i32,
    pub bv_intersects: unsafe extern "C" fn(*mut c_void, i32) -> bool,
    pub shape_collides: unsafe extern "C" fn(*mut c_void, i32) -> bool,
}

impl CollideHost for &CollideHostRef {
    fn body_num(&mut self) -> i32 {
        // SAFETY: the C++ side keeps the table alive for the call.
        unsafe { (self.body_num)(self.ctx) }
    }
    fn bv_intersects(&mut self, i: i32) -> bool {
        // SAFETY: as above.
        unsafe { (self.bv_intersects)(self.ctx, i) }
    }
    fn shape_collides(&mut self, i: i32) -> bool {
        // SAFETY: as above.
        unsafe { (self.shape_collides)(self.ctx, i) }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_ent_run_callback(has: bool, sink: *const Sink) {
    with_ops(rd(sink), |o| entc::run_callback(has, o));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_ent_run_interact(
    cb: *mut EntityCb,
    has: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |o| entc::run_interact(pod(cb), has, o));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_ent_set_active(active: *mut bool, x: bool) -> bool {
    entc::set_active(rw(active), x)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_ent_attach(sound: bool, attach_entity: bool, attach: bool) -> i32 {
    entc::play_sound_attach(sound, attach_entity, attach)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_ent_look_at(
    cb: *mut EntityCb,
    has_callback: bool,
    dt: f32,
    host: *const LookHostRef,
    los: *const LosRef,
    sink: *const Sink,
) {
    with_ops(rd(sink), |o| {
        entc::update_look_at(pod(cb), has_callback, dt, &mut rd(host), &mut rd(los), o);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_ent_conn_change(
    state: i32,
    has_change_callback: bool,
    host: *const ConnHostRef,
    sink: *const Sink,
) {
    with_ops(rd(sink), |o| {
        entc::connection_state_change(state, has_change_callback, &mut rd(host), o);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_ent_collides(host: *const CollideHostRef) -> bool {
    entc::collides_with_player(&mut rd(host))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_ent_preload_has_doc(doc: bool) -> bool {
    entc::preload_has_doc(doc)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_ent_preload_has_mesh(mesh: bool) -> bool {
    entc::preload_has_mesh(mesh)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_ent_connection_missing(found: bool) -> bool {
    entc::connection_missing(found)
}

// ---- chest ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_chest_load(v: *const VarsRef, out: *mut MoveData) {
    *rw(out) = propsc::chest_load(vr(v));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_chest_load_instance(v: *const VarsRef) -> i32 {
    propsc::chest_load_instance(vr(v))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_chest_crosshair(locked: bool) -> i32 {
    propsc::chest_crosshair(locked)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_chest_interact(locked: bool, coins_needed: i32, coins: i32) -> i32 {
    propsc::chest_interact(locked, coins_needed, coins)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_chest_press(yes: bool, coins_needed: i32, sink: *const Sink) {
    with_ops(rd(sink), |o| propsc::chest_press(yes, coins_needed, o));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_chest_set_locked(
    locked: *mut bool,
    new: bool,
    max_angles: *const f32,
    n: i32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |o| {
        propsc::chest_set_locked(pod(locked), new, sl(max_angles, n), o);
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_chest_conn_locked(state: i32) -> bool {
    propsc::chest_conn_locked(state)
}

// ---- emotion stone ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_emotion_load_instance(v: *const VarsRef) {
    propsc::emotion_load_instance(vr(v));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_emotion_has_text(cat_empty: bool, entry_empty: bool) -> bool {
    propsc::emotion_has_text(cat_empty, entry_empty)
}

// ---- item ----

#[repr(C)]
pub struct ItemHostRef {
    pub ctx: *mut c_void,
    pub show_pickup: unsafe extern "C" fn(*mut c_void) -> bool,
    pub sub_mesh_num: unsafe extern "C" fn(*mut c_void) -> i32,
    pub sub_mesh_visible: unsafe extern "C" fn(*mut c_void, i32) -> bool,
}

impl ItemHost for &ItemHostRef {
    fn show_pickup(&mut self) -> bool {
        // SAFETY: the C++ side keeps the table alive for the call.
        unsafe { (self.show_pickup)(self.ctx) }
    }
    fn sub_mesh_num(&mut self) -> i32 {
        // SAFETY: as above.
        unsafe { (self.sub_mesh_num)(self.ctx) }
    }
    fn sub_mesh_visible(&mut self, i: i32) -> bool {
        // SAFETY: as above.
        unsafe { (self.sub_mesh_visible)(self.ctx, i) }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_item_load(v: *const VarsRef, kind: i32, out: *mut ItemAmount) {
    *rw(out) = propsc::item_load(vr(v), kind);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_item_load_instance(
    v: *const VarsRef,
    kind: i32,
    out: *mut ItemAmount,
) {
    *rw(out) = propsc::item_load_instance(vr(v), kind);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_item_can_interact(destroy_me: bool) -> bool {
    propsc::item_can_interact(destroy_me)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_item_interact(
    s: *mut ItemState,
    host: *const ItemHostRef,
    sink: *const Sink,
) {
    with_ops(rd(sink), |o| {
        propsc::item_interact(pod(s), &mut rd(host), o)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_item_update(
    s: *mut ItemState,
    dt: f32,
    camera: *const Vector3f,
    body: *const Vector3f,
    host: *const ItemHostRef,
    sink: *const Sink,
) {
    with_ops(rd(sink), |o| {
        propsc::item_update(pod(s), dt, *rd(camera), *rd(body), &mut rd(host), o);
    });
}

// ---- NPC ----

#[repr(C)]
pub struct SkeletonRef {
    pub ctx: *mut c_void,
    pub present: unsafe extern "C" fn(*mut c_void) -> bool,
    pub bone_index: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> i32,
    pub bone_world: unsafe extern "C" fn(*mut c_void, i32, *mut Matrixf),
}

impl Skeleton for &SkeletonRef {
    fn present(&mut self) -> bool {
        // SAFETY: the C++ side keeps the table alive for the call.
        unsafe { (self.present)(self.ctx) }
    }
    fn bone_index(&mut self, name: &[u8]) -> i32 {
        // SAFETY: as above.
        unsafe { (self.bone_index)(self.ctx, name.as_ptr(), name.len()) }
    }
    fn bone_world(&mut self, idx: i32) -> Matrixf {
        let mut out = Matrixf::default();
        // SAFETY: as above.
        unsafe { (self.bone_world)(self.ctx, idx, &mut out) };
        out
    }
}

#[repr(C)]
pub struct NpcHostRef {
    pub ctx: *mut c_void,
    pub check_area: unsafe extern "C" fn(*mut c_void) -> bool,
    pub mesh_world: unsafe extern "C" fn(*mut c_void, *mut Matrixf),
    pub mesh_position: unsafe extern "C" fn(*mut c_void, *mut Vector3f),
    pub player_position: unsafe extern "C" fn(*mut c_void, *mut Vector3f),
}

impl NpcHost for &NpcHostRef {
    fn check_area_collision(&mut self) -> bool {
        // SAFETY: the C++ side keeps the table alive for the call.
        unsafe { (self.check_area)(self.ctx) }
    }
    fn mesh_world(&mut self) -> Matrixf {
        let mut out = Matrixf::default();
        // SAFETY: as above.
        unsafe { (self.mesh_world)(self.ctx, &mut out) };
        out
    }
    fn mesh_position(&mut self) -> Vector3f {
        let mut out = Vector3f::default();
        // SAFETY: as above.
        unsafe { (self.mesh_position)(self.ctx, &mut out) };
        out
    }
    fn player_position(&mut self) -> Vector3f {
        let mut out = Vector3f::default();
        // SAFETY: as above.
        unsafe { (self.player_position)(self.ctx, &mut out) };
        out
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_npc_state_new(s: *mut NpcState) {
    *rw(s) = NpcState::new();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_npc_load(
    v: *const VarsRef,
    skel: *const SkeletonRef,
    out: *mut NpcParams,
    sink: *const Sink,
) {
    with_ops(rd(sink), |o| {
        *rw(out) = propsc::npc_load(vr(v), &mut rd(skel), o);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_npc_load_instance(v: *const VarsRef) -> bool {
    propsc::npc_load_instance(vr(v))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_npc_set_awake(
    s: *mut NpcState,
    awake: bool,
    effects: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |o| {
        propsc::npc_set_awake(pod(s), awake, effects, o)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_npc_update_wake(
    s: *mut NpcState,
    non_loop_anim: i32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |o| {
        propsc::npc_update_wake(pod(s), non_loop_anim, o)
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_npc_after_world_load(area_name_empty: bool) -> i32 {
    propsc::npc_after_world_load(area_name_empty)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_npc_area_missing(found: bool) -> bool {
    propsc::npc_area_missing(found)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_npc_update_area(
    s: *mut NpcState,
    has_area: bool,
    dt: f32,
    host: *const NpcHostRef,
) {
    propsc::npc_update_area(pod(s), has_area, dt, &mut rd(host));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_npc_update_head(
    s: *mut NpcState,
    p: *const NpcParams,
    bones: *const NpcBone,
    n: i32,
    dt: f32,
    host: *const NpcHostRef,
    sink: *const Sink,
) {
    with_ops(rd(sink), |o| {
        propsc::npc_update_head(pod(s), rd(p), sl(bones, n), dt, &mut rd(host), o);
    });
}

// ---- static prop and hand objects ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_static_cast_shadows(v: *const VarsRef) -> bool {
    propsc::static_cast_shadows(vr(v))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_hand_load_settings(v: *const VarsRef, out: *mut Matrixf) {
    *rw(out) = handc::load_settings(vr(v));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_hand_load_main(v: *const VarsRef) {
    handc::load_main(vr(v));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_hand_adopt_mesh(has_mesh: bool) -> bool {
    handc::adopt_mesh(has_mesh)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_hand_model_inverse(local: *const Matrixf, out: *mut Matrixf) {
    *rw(out) = handc::model_inverse(rd(local));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4c_hand_child_local(
    inv: *const Matrixf,
    child: *const Matrixf,
    out: *mut Matrixf,
) {
    *rw(out) = handc::child_local(rd(inv), rd(child));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4c_hand_create_needed(has_entity: bool) -> bool {
    handc::create_needed(has_entity)
}
