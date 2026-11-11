//! C ABI of the Amnesia save layer, mirrored in `amnesia/src/game/RustLuxSave.h`.

use super::env::{Env, Op};
use super::names::{self, Date, NameState};
use super::{engine, game, gamemap, map};
use std::ffi::c_void;

type Ctx = *mut c_void;
type Call = unsafe extern "C" fn(Ctx, u32, i32, i32) -> i32;
type Sink32 = unsafe extern "C" fn(Ctx, *const u32, usize);
type Translate = unsafe extern "C" fn(Ctx, *const u32, usize, Sink32, Ctx);

struct CEnv {
    ctx: Ctx,
    call: Call,
}

impl Env for CEnv {
    fn call(&mut self, op: Op, a: i32, b: i32) -> i32 {
        // SAFETY: the entry point's caller guarantees `call` accepts `ctx`.
        unsafe { (self.call)(self.ctx, op as u32, a, b) }
    }
}

macro_rules! walkers {
    ($($name:ident => $f:path),* $(,)?) => {
        $(
            /// # Safety
            /// `call` must accept `ctx` for every op of the walker.
            #[unsafe(no_mangle)]
            pub unsafe extern "C" fn $name(ctx: Ctx, call: Call) {
                $f(&mut CEnv { ctx, call });
            }
        )*
    };
}

walkers! {
    ohpl_ga6_light_from => engine::light_from,
    ohpl_ga6_light_to => engine::light_to,
    ohpl_ga6_sound_from => engine::sound_from,
    ohpl_ga6_sound_to => engine::sound_to,
    ohpl_ga6_ps_from => engine::ps_from,
    ohpl_ga6_ps_to => engine::ps_to,
    ohpl_ga6_joint_from => engine::joint_from,
    ohpl_ga6_joint_to => engine::joint_to,
    ohpl_ga6_mesh_from => engine::mesh_from,
    ohpl_ga6_mesh_to => engine::mesh_to,
    ohpl_ga6_rope_from => engine::rope_from,
    ohpl_ga6_rope_to => engine::rope_to,
    ohpl_ga6_effects_from => engine::effects_from,
    ohpl_ga6_effects_to => engine::effects_to,
    ohpl_ga6_music_from => game::music_from,
    ohpl_ga6_music_to => game::music_to,
    ohpl_ga6_insanity_to => game::insanity_to,
    ohpl_ga6_inventory_to => game::inventory_to,
    ohpl_ga6_hands_from => game::hands_from,
    ohpl_ga6_hands_to => game::hands_to,
    ohpl_ga6_player_from => game::player_from,
    ohpl_ga6_player_to => game::player_to,
    ohpl_ga6_map_from => map::map_from,
    ohpl_ga6_map_to => map::map_to,
    ohpl_ga6_gamemap_from => gamemap::gamemap_from,
    ohpl_ga6_gamemap_to => gamemap::gamemap_to,
    ohpl_ga6_entity_from => gamemap::entity_from,
    ohpl_ga6_entity_to => gamemap::entity_to,
    ohpl_ga6_enemy_to => gamemap::enemy_to,
}

fn units<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller of the entry point passes a live `n`-unit buffer.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller of the entry point passes a live `n`-byte buffer.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn push(sink: Sink32, ctx: Ctx, v: &[u32]) {
    // SAFETY: `sink` accepts `ctx`.
    unsafe { sink(ctx, v.as_ptr(), v.len()) }
}

/// # Safety
/// `st` and `date` are valid; buffers are live for their lengths; `sink` accepts `sctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga6_save_name(
    st: *mut NameState,
    prefix: *const u32,
    prefix_n: usize,
    map: *const u32,
    map_n: usize,
    date: *const Date,
    sink: Sink32,
    sctx: Ctx,
) {
    // SAFETY: valid pointers per the contract.
    let (Some(st), Some(date)) = (unsafe { st.as_mut() }, unsafe { date.as_ref() }) else {
        return;
    };
    let name = names::save_name(st, units(prefix, prefix_n), units(map, map_n), date);
    push(sink, sctx, &name);
}

/// # Safety
/// Buffers are live for their lengths; `translate` accepts `tctx`, `sink` accepts `sctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga6_proper_name(
    file: *const u32,
    file_n: usize,
    translate: Translate,
    tctx: Ctx,
    sink: Sink32,
    sctx: Ctx,
) {
    unsafe extern "C" fn collect(ctx: Ctx, p: *const u32, n: usize) {
        // SAFETY: `ctx` is the `Vec` handed to `translate` below.
        let out = unsafe { &mut *ctx.cast::<Vec<u32>>() };
        out.extend_from_slice(units(p, n));
    }
    let name = names::proper_name(units(file, file_n), &mut |tok| {
        let mut out: Vec<u32> = Vec::new();
        // SAFETY: `translate` accepts `tctx`; `collect` accepts the `Vec`.
        unsafe {
            translate(
                tctx,
                tok.as_ptr(),
                tok.len(),
                collect,
                (&raw mut out).cast(),
            );
        }
        out
    });
    push(sink, sctx, &name);
}

/// # Safety
/// `dates` is `n` readable dates.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga6_newest(dates: *const Date, n: usize) -> i32 {
    let d = if n == 0 || dates.is_null() {
        &[]
    } else {
        // SAFETY: per the contract.
        unsafe { std::slice::from_raw_parts(dates, n) }
    };
    names::newest(d)
        .and_then(|i| i32::try_from(i).ok())
        .unwrap_or(-1)
}

/// Writes the removal order into `out` (room for `n` entries) and returns its length.
///
/// # Safety
/// `dates` is `n` readable dates and `out` is `n` writable ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga6_rotation_victims(
    dates: *const Date,
    n: usize,
    max: i32,
    out: *mut i32,
) -> usize {
    if n == 0 || dates.is_null() || out.is_null() {
        return 0;
    }
    // SAFETY: per the contract.
    let (d, o) = unsafe {
        (
            std::slice::from_raw_parts(dates, n),
            std::slice::from_raw_parts_mut(out, n),
        )
    };
    let victims = names::rotation_victims(d, max);
    for (slot, v) in o.iter_mut().zip(&victims) {
        *slot = i32::try_from(*v).unwrap_or(-1);
    }
    victims.len().min(n)
}

/// # Safety
/// `file` is `n` readable units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga6_hardmode_file_ok(file: *const u32, n: usize) -> bool {
    names::hardmode_file_ok(units(file, n))
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OhplGa6Str {
    pub p: *const u8,
    pub n: usize,
}

/// # Safety
/// `names` is `n` readable strings whose buffers are live; `name` is `name_n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga6_find_map(
    names: *const OhplGa6Str,
    n: usize,
    name: *const u8,
    name_n: usize,
) -> i32 {
    let list: &[OhplGa6Str] = if n == 0 || names.is_null() {
        &[]
    } else {
        // SAFETY: per the contract.
        unsafe { std::slice::from_raw_parts(names, n) }
    };
    let v: Vec<&[u8]> = list.iter().map(|s| bytes(s.p, s.n)).collect();
    gamemap::find_map(&v, bytes(name, name_n))
        .and_then(|i| i32::try_from(i).ok())
        .unwrap_or(-1)
}
