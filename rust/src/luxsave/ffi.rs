//! C ABI of the Amnesia save layer, mirrored in `amnesia/src/game/RustLuxSave.h`.

use super::env::{Env, Op};
use super::handler::{HOp, Handler, Host};
use super::names::{self};
use super::{engine, game, gamemap, map};
use std::ffi::c_void;

type Ctx = *mut c_void;
type Call = unsafe extern "C" fn(Ctx, u32, i32, i32) -> i32;
type Sink32 = unsafe extern "C" fn(Ctx, *const u32, usize);
type HostCall = unsafe extern "C" fn(Ctx, u32, i64, *mut u32, usize) -> i64;
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

struct CHost {
    ctx: Ctx,
    call: HostCall,
}

impl Host for CHost {
    fn call(&mut self, op: HOp, a: i64, buf: &mut [u32]) -> i64 {
        // SAFETY: the entry point's caller guarantees `call` accepts `ctx` and `buf`.
        unsafe { (self.call)(self.ctx, op as u32, a, buf.as_mut_ptr(), buf.len()) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga6h_new(max_auto: i32) -> *mut Handler {
    Box::into_raw(Box::new(Handler::new(max_auto)))
}

/// # Safety
/// `h` comes from `ohpl_ga6h_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga6h_free(h: *mut Handler) {
    if !h.is_null() {
        // SAFETY: per the contract.
        drop(unsafe { Box::from_raw(h) });
    }
}

/// # Safety
/// `h` is live; `call` accepts `ctx`; `file` is `n` readable units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga6h_save_to_file(
    h: *mut Handler,
    ctx: Ctx,
    call: HostCall,
    file: *const u32,
    n: usize,
    snapshot: bool,
) {
    // SAFETY: valid `h` per the contract.
    if let Some(h) = unsafe { h.as_mut() } {
        h.save_to_file(&mut CHost { ctx, call }, units(file, n), snapshot);
    }
}

/// # Safety
/// `h` is live; `call` accepts `ctx`; `file` is `n` readable units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga6h_load_from_file(
    h: *mut Handler,
    ctx: Ctx,
    call: HostCall,
    file: *const u32,
    n: usize,
) {
    // SAFETY: valid `h` per the contract.
    if let Some(h) = unsafe { h.as_mut() } {
        h.load_from_file(&mut CHost { ctx, call }, units(file, n));
    }
}

/// `what`: 0 autosave, 1 hard-mode save, 2 autoload (reset logger), 3 autoload, 4 file exists.
///
/// # Safety
/// `h` is live; `call` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga6h_run(
    h: *mut Handler,
    ctx: Ctx,
    call: HostCall,
    what: u32,
) -> bool {
    // SAFETY: valid `h` per the contract.
    let Some(h) = (unsafe { h.as_mut() }) else {
        return false;
    };
    let host = &mut CHost { ctx, call };
    match what {
        0 => h.auto_save(host),
        1 => h.hardmode_save(host),
        2 => h.auto_load(host, true),
        3 => h.auto_load(host, false),
        _ => h.save_file_exists(host),
    }
}
