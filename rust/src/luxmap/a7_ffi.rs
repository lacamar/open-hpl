//! C ABI of the A7 map/area ownership, mirrored in `amnesia/src/game/RustLuxMapA7.h`.
//! No `&mut` to a map is held across a call that can re-enter C++ (destructor thunks, callbacks).

use super::a7_area::{A7Area, AreaArena, Base, Pod};
use super::a7_map::{
    A7Map, Entity, K_DISSOLVE, K_ENTITY, K_LAMP, K_POS, K_START, K_USEITEM, Timer, UseItem,
    W_DOOMED,
};
use super::area::{ExamineIn, ExamineOut, StickyAttachOut};
use crate::obj::Foreign;
use crate::string_ffi::{Sink, emit, units};
use crate::types::{Matrixf, Vector3f};
use std::ffi::c_void;

/// `A7Map` plus the area arena that shares its lifetime.
#[derive(Default)]
pub struct Ctx {
    pub map: A7Map,
    pub areas: AreaArena,
}

type DropFn = unsafe extern "C" fn(*mut c_void);
type DissolveCb = unsafe extern "C" fn(*mut c_void, *mut c_void, f32, bool);
type PtrCb = unsafe extern "C" fn(*mut c_void, *mut c_void);

/// # Safety
/// `m` is a live context from `ohpl_a7_new`.
unsafe fn cx<'a>(m: *mut Ctx) -> &'a mut Ctx {
    // SAFETY: caller contract.
    unsafe { &mut *m }
}

macro_rules! mp {
    ($m:expr) => {
        // SAFETY: caller contract on `m`.
        unsafe { &mut cx($m).map }
    };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a7_new() -> *mut Ctx {
    Box::into_raw(Box::default())
}

/// # Safety
/// `m` came from `ohpl_a7_new`, is not used afterwards, and `ohpl_a7_teardown` has run.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_free(m: *mut Ctx) {
    if !m.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(m) });
    }
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_set_drop(m: *mut Ctx, kind: u32, f: DropFn) {
    let fr = Foreign {
        drop: Some(f),
        ..Foreign::default()
    };
    mp!(m).set_foreign(kind as usize, fr);
}

/// Runs the thunk for each address; the map is not borrowed while it runs.
fn drop_all(f: Foreign, ptrs: Vec<usize>) {
    for p in ptrs {
        // SAFETY: each address is a live object of the registered kind, dropped once.
        unsafe { f.destroy(p) };
    }
}

/// `~cLuxMap` order: timers, lamp connections, entities, start nodes, pos nodes, use-item
/// callbacks, dissolve entities.
///
/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_teardown(m: *mut Ctx) {
    mp!(m).timers_clear();
    let (lamps, f) = {
        let mm = mp!(m);
        (mm.lamp_take(), mm.foreign(K_LAMP))
    };
    drop_all(f, lamps);
    // SAFETY: `m` live.
    unsafe { ohpl_a7_destroy_entities(m) };
    for (take, kind) in [
        (A7Map::start_take as fn(&mut A7Map) -> Vec<usize>, K_START),
        (A7Map::pos_take, K_POS),
        (A7Map::use_items_take, K_USEITEM),
        (A7Map::dissolve_take, K_DISSOLVE),
    ] {
        let (v, f) = {
            let mm = mp!(m);
            (take(mm), mm.foreign(kind))
        };
        drop_all(f, v);
    }
}

/// `DestroyAllEntities` entity half: delete in list order with the deleting flag set, then clear
/// the lookups. The entities stay visible to lookups while their destructors run.
///
/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_destroy_entities(m: *mut Ctx) {
    let (ptrs, f) = {
        let mm = mp!(m);
        mm.set_deleting_all(true);
        (
            mm.list_snapshot(super::a7_map::W_ENTITIES),
            mm.foreign(K_ENTITY),
        )
    };
    drop_all(f, ptrs);
    let mm = mp!(m);
    mm.set_deleting_all(false);
    mm.after_destroy_all();
}

/// `DestroyAllEntities` head: timers and lamp connections go first.
///
/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_destroy_all(m: *mut Ctx) {
    mp!(m).timers_clear();
    let (lamps, f) = {
        let mm = mp!(m);
        (mm.lamp_take(), mm.foreign(K_LAMP))
    };
    drop_all(f, lamps);
    // SAFETY: `m` live.
    unsafe { ohpl_a7_destroy_entities(m) };
}

/// # Safety
/// `m` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_ent_add(
    m: *mut Ctx,
    p: *mut c_void,
    typed: *mut c_void,
    name: *const u8,
    name_n: usize,
    id: i32,
    is_enemy: bool,
    is_area: bool,
    is_sticky: bool,
) {
    // SAFETY: caller contract.
    let name = unsafe { units(name, name_n) };
    let e = Entity {
        p: p as usize,
        typed: typed as usize,
    };
    mp!(m).add_entity(e, name, id, is_enemy, is_area, is_sticky);
}

/// # Safety
/// `m` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_ent_find_name(
    m: *mut Ctx,
    name: *const u8,
    n: usize,
) -> *mut c_void {
    // SAFETY: caller contract.
    let name = unsafe { units(name, n) };
    mp!(m).find_name(name) as *mut c_void
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_ent_find_id(m: *mut Ctx, id: i32) -> *mut c_void {
    mp!(m).find_id(id) as *mut c_void
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_ent_exists(m: *mut Ctx, p: *const c_void) -> bool {
    mp!(m).exists(p as usize)
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_ent_latest(m: *mut Ctx) -> *mut c_void {
    mp!(m).latest() as *mut c_void
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_ent_reset_latest(m: *mut Ctx) {
    mp!(m).reset_latest();
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_ent_free_id(m: *mut Ctx) -> i32 {
    mp!(m).free_entity_id()
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_deleting_all(m: *mut Ctx) -> bool {
    mp!(m).deleting_all()
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_doom_push(m: *mut Ctx, p: *mut c_void) {
    mp!(m).doom_push(p as usize);
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_doom_clear(m: *mut Ctx) {
    mp!(m).list_clear(W_DOOMED);
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_ent_unlink(
    m: *mut Ctx,
    p: *mut c_void,
    typed: *mut c_void,
    is_enemy: bool,
    is_area: bool,
    is_sticky: bool,
) {
    let e = Entity {
        p: p as usize,
        typed: typed as usize,
    };
    mp!(m).unlink_entity(e, is_enemy, is_area, is_sticky);
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_cur_begin(m: *mut Ctx, which: u32) -> u64 {
    mp!(m).cursor_begin(which)
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_cur_get(m: *mut Ctx, which: u32, cur: u64) -> *mut c_void {
    mp!(m).cursor_get(which, cur) as *mut c_void
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_cur_next(m: *mut Ctx, which: u32, cur: u64) -> u64 {
    mp!(m).cursor_next(which, cur)
}

/// `-1` when unchanged, else the new icon state.
///
/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_commentary_sync(m: *mut Ctx, show: bool) -> i32 {
    mp!(m).commentary_sync(show).map_or(-1, i32::from)
}

// timers

/// # Safety
/// `m` live; texts valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_timer_add(
    m: *mut Ctx,
    name: *const u8,
    name_n: usize,
    time: f32,
    func: *const u8,
    func_n: usize,
) {
    // SAFETY: caller contract.
    let (name, func) = unsafe { (units(name, name_n), units(func, func_n)) };
    mp!(m).timer_add(name, time, func);
}

/// Loads a saved timer verbatim.
///
/// # Safety
/// `m` live; texts valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_timer_push(
    m: *mut Ctx,
    name: *const u8,
    name_n: usize,
    func: *const u8,
    func_n: usize,
    count: f32,
    destroy_me: bool,
) {
    // SAFETY: caller contract.
    let (name, func) = unsafe { (units(name, name_n), units(func, func_n)) };
    mp!(m).timer_push_raw(Timer {
        name: name.to_vec(),
        func: func.to_vec(),
        count,
        destroy_me,
    });
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_timers_clear(m: *mut Ctx) {
    mp!(m).timers_clear();
}

/// # Safety
/// `m` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_timer_remove(m: *mut Ctx, name: *const u8, n: usize) {
    // SAFETY: caller contract.
    let name = unsafe { units(name, n) };
    mp!(m).timer_remove(name);
}

/// # Safety
/// `m` live; `name` valid; `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_timer_get(
    m: *mut Ctx,
    name: *const u8,
    n: usize,
    out: *mut f32,
) -> bool {
    // SAFETY: caller contract.
    let name = unsafe { units(name, n) };
    match mp!(m).timer_get(name) {
        Some(c) => {
            // SAFETY: caller contract.
            unsafe { *out = c };
            true
        }
        None => false,
    }
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_timers_begin(m: *mut Ctx, step: f32) {
    mp!(m).timers_begin(step);
}

/// Next due timer at or after `*cursor`; its name and function leave through the sinks.
///
/// # Safety
/// `m` live; `cursor` writable; sinks valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_timer_poll(
    m: *mut Ctx,
    cursor: *mut usize,
    sink: Sink<u8>,
    name_ctx: *mut c_void,
    func_ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let cur = unsafe { &mut *cursor };
    let hit = mp!(m).timer_poll(cur);
    match hit {
        Some((n, f)) => {
            // SAFETY: caller contract.
            unsafe {
                emit(sink, name_ctx, &n);
                emit(sink, func_ctx, &f);
            }
            true
        }
        None => false,
    }
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_timer_finish(m: *mut Ctx, cursor: usize) {
    mp!(m).timer_finish(cursor);
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_timers_end(m: *mut Ctx) {
    mp!(m).timers_end();
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_timer_len(m: *mut Ctx) -> usize {
    mp!(m).timer_len()
}

/// # Safety
/// `m` live; sinks and out-params valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_timer_at(
    m: *mut Ctx,
    i: usize,
    sink: Sink<u8>,
    name_ctx: *mut c_void,
    func_ctx: *mut c_void,
    count: *mut f32,
    destroy_me: *mut bool,
) -> bool {
    let t = mp!(m).timer_at(i).cloned();
    let Some(t) = t else { return false };
    // SAFETY: caller contract.
    unsafe {
        emit(sink, name_ctx, &t.name);
        emit(sink, func_ctx, &t.func);
        *count = t.count;
        *destroy_me = t.destroy_me;
    }
    true
}

// use-item callbacks

/// # Safety
/// `m` live; texts valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_use_add(
    m: *mut Ctx,
    p: *mut c_void,
    name: *const u8,
    name_n: usize,
    item: *const u8,
    item_n: usize,
    entity: *const u8,
    entity_n: usize,
) {
    // SAFETY: caller contract.
    let (name, item, entity) = unsafe {
        (
            units(name, name_n),
            units(item, item_n),
            units(entity, entity_n),
        )
    };
    mp!(m).use_item_add(UseItem {
        ptr: p as usize,
        name: name.to_vec(),
        item: item.to_vec(),
        entity: entity.to_vec(),
    });
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_use_len(m: *mut Ctx) -> usize {
    mp!(m).use_item_len()
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_use_at(m: *mut Ctx, i: usize) -> *mut c_void {
    mp!(m).use_item_at(i).map_or(0, |u| u.ptr) as *mut c_void
}

/// Unlinks the entry with the name and deletes its object; false when none matches.
///
/// # Safety
/// `m` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_use_remove_name(m: *mut Ctx, name: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    let name = unsafe { units(name, n) };
    let (hit, f) = {
        let mm = mp!(m);
        (mm.use_item_remove_name(name), mm.foreign(K_USEITEM))
    };
    hit.is_some_and(|p| {
        drop_all(f, vec![p]);
        true
    })
}

/// # Safety
/// `m` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_use_remove_ptr(
    m: *mut Ctx,
    p: *mut c_void,
    name: *const u8,
    n: usize,
) {
    // SAFETY: caller contract.
    let name = unsafe { units(name, n) };
    let (hit, f) = {
        let mm = mp!(m);
        (
            mm.use_item_remove_ptr(p as usize, name),
            mm.foreign(K_USEITEM),
        )
    };
    if let Some(p) = hit {
        drop_all(f, vec![p]);
    }
}

/// # Safety
/// `m` live; texts valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_use_find(
    m: *mut Ctx,
    item: *const u8,
    item_n: usize,
    entity: *const u8,
    entity_n: usize,
) -> *mut c_void {
    // SAFETY: caller contract.
    let (item, entity) = unsafe { (units(item, item_n), units(entity, entity_n)) };
    mp!(m).use_item_find(item, entity) as *mut c_void
}

/// Deletes every callback object, in order.
///
/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_use_clear(m: *mut Ctx) {
    let (v, f) = {
        let mm = mp!(m);
        (mm.use_items_take(), mm.foreign(K_USEITEM))
    };
    drop_all(f, v);
}

// dissolve entities

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_dissolve_add(m: *mut Ctx, obj: *mut c_void, time: f32) {
    mp!(m).dissolve_add(obj as usize, time);
}

/// One update pass: `cb(ctx, obj, alpha, remove)` per entity in order; a removed one is deleted
/// right after its callback.
///
/// # Safety
/// `m` live; `cb` valid for every object.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_dissolve_update(
    m: *mut Ctx,
    step: f32,
    cb: DissolveCb,
    ctx: *mut c_void,
) {
    let (out, f) = {
        let mm = mp!(m);
        (mm.dissolve_step(step), mm.foreign(K_DISSOLVE))
    };
    for (obj, alpha, remove) in out {
        // SAFETY: `obj` is a live dissolve object; the map is not borrowed.
        unsafe { cb(ctx, obj as *mut c_void, alpha, remove) };
        if remove {
            drop_all(f, vec![obj]);
        }
    }
}

// lamp light connections

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_lamp_add(m: *mut Ctx, conn: *mut c_void, light: *const c_void) {
    mp!(m).lamp_add(conn as usize, light as usize);
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_lamp_find(m: *mut Ctx, light: *const c_void) -> *mut c_void {
    mp!(m).lamp_find(light as usize) as *mut c_void
}

/// # Safety
/// `m` live; `cb` valid for every connection.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_lamp_each(m: *mut Ctx, cb: PtrCb, ctx: *mut c_void) {
    for conn in mp!(m).lamp_snapshot() {
        // SAFETY: live connection; the map is not borrowed.
        unsafe { cb(ctx, conn as *mut c_void) };
    }
}

// start / pos nodes

/// # Safety
/// `m` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_start_add(m: *mut Ctx, p: *mut c_void, name: *const u8, n: usize) {
    // SAFETY: caller contract.
    let name = unsafe { units(name, n) };
    mp!(m).start_add(p as usize, name);
}

/// # Safety
/// `m` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_start_find(m: *mut Ctx, name: *const u8, n: usize) -> *mut c_void {
    // SAFETY: caller contract.
    let name = unsafe { units(name, n) };
    mp!(m).start_find(name) as *mut c_void
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_start_first(m: *mut Ctx) -> *mut c_void {
    mp!(m).start_first() as *mut c_void
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_start_num(m: *mut Ctx) -> usize {
    mp!(m).start_num()
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_start_at(m: *mut Ctx, i: usize) -> *mut c_void {
    mp!(m).start_at(i) as *mut c_void
}

/// # Safety
/// `m` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_pos_add(m: *mut Ctx, p: *mut c_void, name: *const u8, n: usize) {
    // SAFETY: caller contract.
    let name = unsafe { units(name, n) };
    mp!(m).pos_add(p as usize, name);
}

/// # Safety
/// `m` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_pos_find(m: *mut Ctx, name: *const u8, n: usize) -> *mut c_void {
    // SAFETY: caller contract.
    let name = unsafe { units(name, n) };
    mp!(m).pos_find(name) as *mut c_void
}

// areas

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_new(m: *mut Ctx, kind: i32) -> *mut A7Area {
    // SAFETY: caller contract.
    unsafe { cx(m) }
        .areas
        .alloc(kind)
        .unwrap_or(std::ptr::null_mut())
}

/// # Safety
/// `m` live; `a` from `ohpl_a7_area_new` on `m`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_free(m: *mut Ctx, a: *mut A7Area) {
    if !a.is_null() {
        // SAFETY: caller contract.
        unsafe { cx(m).areas.free(a) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a7_pod_size(kind: i32) -> usize {
    Pod::size_of(kind)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a7_base_size() -> usize {
    std::mem::size_of::<Base>()
}

/// # Safety
/// `a` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_base(a: *mut A7Area) -> *mut Base {
    // SAFETY: caller contract.
    unsafe { (*a).base_ptr() }
}

/// # Safety
/// `a` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_pod(a: *mut A7Area) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { (*a).pod_ptr() }
}

/// `AttachToBody` success: stores the offset in the base block.
///
/// # Safety
/// `a` live; vectors readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_attach(
    a: *mut A7Area,
    body: *const Vector3f,
    parent: *const Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { (*a).attach(*body, *parent) };
}

/// # Safety
/// `a` live; vectors readable; `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_follow(
    a: *mut A7Area,
    parent: *const Vector3f,
    body: *const Vector3f,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { *out = (*a).follow(*parent, *body) };
}

/// # Safety
/// `a` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_can_interact(a: *mut A7Area, has_callback: bool) -> bool {
    // SAFETY: caller contract.
    unsafe { (*a).can_interact(has_callback) }
}

/// # Safety
/// `a` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_on_interact(a: *mut A7Area, has_callback: bool) -> bool {
    // SAFETY: caller contract.
    unsafe { (*a).on_interact_script(has_callback) }
}

/// Crosshair of Script/Examine/Sign/Ladder areas; `tbl` is default, grab, ladder, base answer.
///
/// # Safety
/// `a` live; `tbl` readable for 4.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_crosshair(
    a: *mut A7Area,
    has_callback: bool,
    custom: i32,
    tbl: *const [i32; 4],
) -> i32 {
    // SAFETY: caller contract.
    unsafe { (*a).crosshair(has_callback, custom, &*tbl) }
}

/// # Safety
/// `a` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_tick(a: *mut A7Area, step: f32, engine_step: f32) -> bool {
    // SAFETY: caller contract.
    unsafe { (*a).tick(step, engine_step) }
}

/// # Safety
/// `a` live; `i`, `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_examine(
    a: *mut A7Area,
    i: *const ExamineIn,
    sanity: f32,
    out: *mut ExamineOut,
) {
    // SAFETY: caller contract.
    unsafe { *out = (*a).examine_interact(*i, sanity) };
}

/// # Safety
/// `a` live; `world` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_ladder_setup(
    a: *mut A7Area,
    world: *const Matrixf,
    pos_y: f32,
    size_y: f32,
) {
    // SAFETY: caller contract.
    unsafe { (*a).ladder_setup(&*world, pos_y, size_y) };
}

/// # Safety
/// `a` live; vectors valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_ladder_start(
    a: *mut A7Area,
    area_pos: *const Vector3f,
    char_size: *const Vector3f,
    char_y: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { *out = (*a).ladder_start(*area_pos, *char_size, char_y) };
}

/// # Safety
/// `a` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_liquid_setup(a: *mut A7Area, pos_y: f32, size_y: f32) {
    // SAFETY: caller contract.
    unsafe { (*a).liquid_setup(pos_y, size_y) };
}

/// # Safety
/// `a` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_sticky_can_detach(a: *mut A7Area) -> bool {
    // SAFETY: caller contract.
    unsafe { (*a).sticky_can_detach() }
}

/// `entity_saved`: -1 when the body has no entity.
///
/// # Safety
/// `a` live; `start` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_sticky_attach(
    a: *mut A7Area,
    start: *const Matrixf,
    gravity: bool,
    mass: f32,
    entity_saved: i32,
) {
    let saved = (entity_saved >= 0).then_some(entity_saved != 0);
    // SAFETY: caller contract.
    unsafe { (*a).sticky_attach(*start, gravity, mass, saved) };
}

/// # Safety
/// `a` live; inputs readable; `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_sticky_step(
    a: *mut A7Area,
    area_world: *const Matrixf,
    area_pos: *const Vector3f,
    mass_centre: *const Vector3f,
    attached_pos: *const Vector3f,
    step: f32,
    out: *mut StickyAttachOut,
) -> bool {
    // SAFETY: caller contract.
    let r = unsafe { (*a).sticky_step(*area_world, *area_pos, *mass_centre, *attached_pos, step) };
    match r {
        Some(o) => {
            // SAFETY: caller contract.
            unsafe { *out = o };
            true
        }
        None => false,
    }
}
