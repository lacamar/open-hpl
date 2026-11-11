//! C ABI of the soundscapes and the FSB glue, mirrored in `soma/src/game/RustSomaSound.h`.

use super::ffi_voice::OhplGs5Str;
use super::fsb;
use super::scape::{AreaDef, AreaState, ReverbEnv, Scape, ScapeHost};
use crate::string_ffi::units;
use std::ffi::c_void;

type Ctx = *mut c_void;

#[repr(C)]
pub struct OhplGs5AreaDef {
    pub ent: usize,
    pub level: OhplGs5Str,
    pub prio: i32,
    pub bg: OhplGs5Str,
    pub bg_volume: f32,
    pub bg_fade_in: f32,
    pub bg_trans_speed: f32,
    pub parent_vol_mul: f32,
    pub in_names: OhplGs5Str,
    pub out_names: OhplGs5Str,
    pub ent_fade_in: f32,
    pub ent_fade_out: f32,
    pub prefix: OhplGs5Str,
    pub prefix_prio: i32,
    pub use_reverb: bool,
    pub reverb_prio: i32,
    pub reverb_type: OhplGs5Str,
    pub reverb_amount: f32,
    pub reverb_fade: f32,
}

#[repr(C)]
pub struct OhplGs5ScapeHost {
    pub ctx: Ctx,
    pub bg_live: unsafe extern "C" fn(Ctx, usize, i32) -> bool,
    pub bg_fade_out: unsafe extern "C" fn(Ctx, usize, f32),
    pub bg_fade_volume_mul_to: unsafe extern "C" fn(Ctx, usize, f32, f32),
    pub bg_play: unsafe extern "C" fn(Ctx, *const u8, usize, *mut usize, *mut i32) -> bool,
    pub bg_set_volume_mul: unsafe extern "C" fn(Ctx, usize, f32),
    pub bg_fade_in: unsafe extern "C" fn(Ctx, usize, f32),
    pub bg_set_paused: unsafe extern "C" fn(Ctx, usize, bool),
    pub world_present: unsafe extern "C" fn(Ctx) -> bool,
    pub entity_fade: unsafe extern "C" fn(Ctx, *const u8, usize, bool, f32),
    /// active, 16 matrix floats, 3 size floats
    pub area: unsafe extern "C" fn(Ctx, usize, *mut bool, *mut f32),
    pub camera_pos: unsafe extern "C" fn(Ctx, *mut f32) -> bool,
    pub body_pos: unsafe extern "C" fn(Ctx, *mut f32) -> bool,
    pub apply_reverb: unsafe extern "C" fn(Ctx, *const f32),
}

struct CHost<'a>(&'a OhplGs5ScapeHost);

// SAFETY (all calls below): the entry point's caller guarantees the table accepts its `ctx`.
impl ScapeHost for CHost<'_> {
    fn bg_live(&mut self, bg: usize, id: i32) -> bool {
        unsafe { (self.0.bg_live)(self.0.ctx, bg, id) }
    }
    fn bg_fade_out(&mut self, bg: usize, speed: f32) {
        unsafe { (self.0.bg_fade_out)(self.0.ctx, bg, speed) }
    }
    fn bg_fade_volume_mul_to(&mut self, bg: usize, dest: f32, speed: f32) {
        unsafe { (self.0.bg_fade_volume_mul_to)(self.0.ctx, bg, dest, speed) }
    }
    fn bg_play(&mut self, name: &[u8]) -> Option<(usize, i32)> {
        let (mut e, mut id) = (0usize, 0i32);
        unsafe { (self.0.bg_play)(self.0.ctx, name.as_ptr(), name.len(), &mut e, &mut id) }
            .then_some((e, id))
    }
    fn bg_set_volume_mul(&mut self, bg: usize, vol: f32) {
        unsafe { (self.0.bg_set_volume_mul)(self.0.ctx, bg, vol) }
    }
    fn bg_fade_in(&mut self, bg: usize, speed: f32) {
        unsafe { (self.0.bg_fade_in)(self.0.ctx, bg, speed) }
    }
    fn bg_set_paused(&mut self, bg: usize, on: bool) {
        unsafe { (self.0.bg_set_paused)(self.0.ctx, bg, on) }
    }
    fn world_present(&mut self) -> bool {
        unsafe { (self.0.world_present)(self.0.ctx) }
    }
    fn entity_fade(&mut self, name: &[u8], fade_in: bool, speed: f32) {
        unsafe { (self.0.entity_fade)(self.0.ctx, name.as_ptr(), name.len(), fade_in, speed) }
    }
    fn area(&mut self, ent: usize) -> AreaState {
        let mut st = AreaState::default();
        let mut b = [0.0f32; 19];
        unsafe { (self.0.area)(self.0.ctx, ent, &mut st.active, b.as_mut_ptr()) };
        st.matrix.copy_from_slice(b.get(..16).unwrap_or(&[0.0; 16]));
        st.size.copy_from_slice(b.get(16..).unwrap_or(&[0.0; 3]));
        st
    }
    fn camera_pos(&mut self) -> Option<[f32; 3]> {
        let mut b = [0.0f32; 3];
        unsafe { (self.0.camera_pos)(self.0.ctx, b.as_mut_ptr()) }.then_some(b)
    }
    fn body_pos(&mut self) -> Option<[f32; 3]> {
        let mut b = [0.0f32; 3];
        unsafe { (self.0.body_pos)(self.0.ctx, b.as_mut_ptr()) }.then_some(b)
    }
    fn apply_reverb(&mut self, env: &ReverbEnv) {
        let v = [
            env.gain,
            env.gain_hf,
            env.gain_lf,
            env.decay_time,
            env.decay_hf_ratio,
            env.decay_lf_ratio,
            env.reflections_gain,
            env.reflections_delay,
            env.late_gain,
            env.late_delay,
            env.mod_time,
            env.mod_depth,
            env.hf_ref,
            env.lf_ref,
            env.diffusion,
            env.density,
        ];
        unsafe { (self.0.apply_reverb)(self.0.ctx, v.as_ptr()) }
    }
}

/// # Safety
/// `s` is a live handle.
unsafe fn scape<'a>(s: *mut Scape) -> &'a mut Scape {
    // SAFETY: caller contract.
    unsafe { &mut *s }
}

unsafe fn text(s: &OhplGs5Str) -> Vec<u8> {
    // SAFETY: caller contract of the calling entry point.
    unsafe { units(s.p, s.n) }.to_vec()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs5_scape_new() -> *mut Scape {
    Box::into_raw(Box::default())
}

/// # Safety
/// `s` came from `ohpl_gs5_scape_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_scape_free(s: *mut Scape) {
    if !s.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(s) });
    }
}

/// `Load`: forgets the current areas and reads `count` new ones.
///
/// # Safety
/// `s` is live; `defs` holds `count` definitions whose strings are readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_scape_load(
    s: *mut Scape,
    defs: *const OhplGs5AreaDef,
    count: usize,
) {
    let mut v = Vec::new();
    for i in 0..count {
        // SAFETY: caller contract.
        let d = unsafe { &*defs.add(i) };
        // SAFETY: caller contract.
        v.push(unsafe {
            AreaDef {
                ent: d.ent,
                level: text(&d.level),
                prio: d.prio,
                bg: text(&d.bg),
                bg_volume: d.bg_volume,
                bg_fade_in: d.bg_fade_in,
                bg_trans_speed: d.bg_trans_speed,
                parent_vol_mul: d.parent_vol_mul,
                in_names: text(&d.in_names),
                out_names: text(&d.out_names),
                ent_fade_in: d.ent_fade_in,
                ent_fade_out: d.ent_fade_out,
                prefix: text(&d.prefix),
                prefix_prio: d.prefix_prio,
                use_reverb: d.use_reverb,
                reverb_prio: d.reverb_prio,
                reverb_type: text(&d.reverb_type),
                reverb_amount: d.reverb_amount,
                reverb_fade: d.reverb_fade,
            }
        });
    }
    // SAFETY: caller contract.
    unsafe { scape(s).load(&v) }
}

/// # Safety
/// `s` is live; `host` is a valid table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_scape_update(
    s: *mut Scape,
    host: *const OhplGs5ScapeHost,
    dt: f32,
) {
    // SAFETY: caller contract.
    let mut h = CHost(unsafe { &*host });
    // SAFETY: caller contract.
    unsafe { scape(s).update(&mut h, dt) }
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_scape_set_default_reverb(s: *mut Scape, preset: i32, fade: f32) {
    // SAFETY: caller contract.
    unsafe { scape(s).set_default_reverb(preset, fade) }
}

/// # Safety
/// `s` is live; `host` is a valid table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_scape_set_paused(
    s: *mut Scape,
    host: *const OhplGs5ScapeHost,
    on: bool,
) {
    // SAFETY: caller contract.
    let mut h = CHost(unsafe { &*host });
    // SAFETY: caller contract.
    unsafe { scape(s).set_paused(&mut h, on) }
}

/// The current sound prefix.
///
/// # Safety
/// `s` is live; the returned pointer is valid until the next `update`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_scape_prefix(s: *mut Scape, len: *mut usize) -> *const u8 {
    // SAFETY: caller contract.
    let p = unsafe { &scape(s).prefix };
    // SAFETY: caller contract.
    unsafe { *len = p.len() };
    p.as_ptr()
}

#[repr(C)]
pub struct OhplGs5ScapeInfo {
    pub rev_type: i32,
    pub rev_amount: f32,
    pub rev_room: f32,
    pub rev_area_ent: usize,
    pub rev_has_area: bool,
}

/// # Safety
/// `s` is live; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_scape_info(s: *mut Scape, out: *mut OhplGs5ScapeInfo) {
    // SAFETY: caller contract.
    let sc = unsafe { scape(s) };
    // SAFETY: caller contract.
    unsafe {
        out.write(OhplGs5ScapeInfo {
            rev_type: sc.rev_type,
            rev_amount: sc.rev_amount,
            rev_room: sc.rev_cur.get(1).copied().unwrap_or(0.0),
            rev_area_ent: sc.rev_area.map_or(0, |a| sc.area_ent(a)),
            rev_has_area: sc.rev_area.is_some(),
        });
    }
}

/// Level `i`: whether it has an area (with its entity) and a background, and the background name.
///
/// # Safety
/// `s` is live; `ent` is writable; `name` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_scape_level(
    s: *mut Scape,
    i: usize,
    ent: *mut usize,
    has_area: *mut bool,
    has_bg: *mut bool,
    name: unsafe extern "C" fn(Ctx, *const u8, usize),
    ctx: Ctx,
) {
    // SAFETY: caller contract.
    let sc = unsafe { scape(s) };
    let Some((area, bg, nm)) = sc.level_info(i) else {
        return;
    };
    // SAFETY: caller contract.
    unsafe {
        *has_area = area.is_some();
        *ent = area.map_or(0, |a| sc.area_ent(a));
        *has_bg = bg != 0;
        name(ctx, nm.as_ptr(), nm.len());
    }
}

/// Name of EAX reverb preset `i`, empty past the table.
///
/// # Safety
/// `len` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_reverb_name(i: i32, len: *mut usize) -> *const u8 {
    let n = usize::try_from(i)
        .ok()
        .and_then(|i| super::presets::NAMES.get(i))
        .copied()
        .unwrap_or("");
    // SAFETY: caller contract.
    unsafe { *len = n.len() };
    n.as_ptr()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs5_fsb_mode_supported(mode: u32) -> bool {
    fsb::mode_supported(mode)
}

/// 0 extract, 1 cached `.ogg`, 2 cached `.wav`.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs5_fsb_cached_state(has_ogg: bool, has_wav: bool) -> u8 {
    fsb::cached_state(has_ogg, has_wav)
}

/// # Safety
/// `prefix` and `sample` are readable for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_fsb_output_name(
    prefix: *const u8,
    pn: usize,
    sample: *const u8,
    sn: usize,
    mode: u32,
    channels: i32,
    sink: unsafe extern "C" fn(Ctx, *const u8, usize),
    ctx: Ctx,
) {
    // SAFETY: caller contract.
    let n = fsb::output_name(
        unsafe { units(prefix, pn) },
        unsafe { units(sample, sn) },
        mode,
        channels,
    );
    // SAFETY: caller contract.
    unsafe { sink(ctx, n.as_ptr(), n.len()) };
}

/// The index of the first of `count` wanted names equal to `sample`, -1 for none.
///
/// # Safety
/// `wanted` holds `count` strings.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_fsb_wanted_index(
    sample: *const u8,
    sn: usize,
    wanted: *const OhplGs5Str,
    count: usize,
) -> i64 {
    let mut names: Vec<&[u8]> = Vec::new();
    for i in 0..count {
        // SAFETY: caller contract.
        let w = unsafe { &*wanted.add(i) };
        // SAFETY: caller contract.
        names.push(unsafe { units(w.p, w.n) });
    }
    // SAFETY: caller contract.
    fsb::wanted_index(unsafe { units(sample, sn) }, &names).map_or(-1, |i| i as i64)
}

/// Mixes `n` frames of `ch` planar channels (`planes[c][i]`) to interleaved stereo 16 bit, appended
/// to `out` (which must hold `2 * n` shorts).
///
/// # Safety
/// `planes` holds `ch` pointers to `n` floats; `left`/`right` hold `ch` floats; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_fsb_downmix_block(
    planes: *const *const f32,
    ch: usize,
    n: usize,
    left: *const f32,
    right: *const f32,
    out: *mut i16,
) {
    let mut p: Vec<&[f32]> = Vec::new();
    for c in 0..ch {
        // SAFETY: caller contract.
        p.push(unsafe { units(*planes.add(c), n) });
    }
    let mut v = Vec::with_capacity(2 * n);
    // SAFETY: caller contract.
    fsb::downmix_block(
        &p,
        unsafe { units(left, ch) },
        unsafe { units(right, ch) },
        n,
        &mut v,
    );
    // SAFETY: caller contract.
    unsafe { out.copy_from_nonoverlapping(v.as_ptr(), v.len()) };
}
