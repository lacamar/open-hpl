//! Engine services the effect handler reaches: sound entries, GUI gfx and fonts, the game HUD,
//! configs, the player, the map and the post effects. C++ fills one [`HostTable`]
//! (`amnesia/src/game/RustLuxFxA12a.h`); tests use a recording fake. No host call may be made
//! while a handler borrow is held, since scripts can re-enter the handler.

use super::dump;
use super::postfx::Op as PostOp;
use super::renderer::{Op as RendOp, Outline};
use crate::gui::direct;
use crate::types::{Color, Vector3f};
use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

pub type Ctx = *mut c_void;
type Sink = unsafe extern "C" fn(Ctx, *const u8, usize);
type Sink32 = unsafe extern "C" fn(Ctx, *const u32, usize);

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    pub p: *const u8,
    pub n: usize,
}

fn bytes(s: &[u8]) -> Bytes {
    Bytes {
        p: s.as_ptr(),
        n: s.len(),
    }
}

pub const PLAYER_FOV: i32 = 0;
pub const PLAYER_ACTIVE: i32 = 1;
pub const PLAYER_SHAKE: i32 = 2;
pub const PLAYER_SHAKE_CLEAR: i32 = 3;
pub const MAP_ICON: i32 = 0;
pub const MAP_ENEMY_RESET: i32 = 1;
pub const MAP_SCRIPT: i32 = 2;
pub const POST_PARAMS: i32 = 0;
pub const POST_ACTIVE: i32 = 1;
pub const POST_RESET: i32 = 2;
pub const POST_RADIAL: i32 = 0;
pub const POST_SEPIA: i32 = 1;
pub const POST_TRAIL: i32 = 2;
pub const COMMENTARY_RESET: i32 = 0;
pub const COMMENTARY_START: i32 = 1;
pub const COMMENTARY_STOP: i32 = 2;

#[repr(C)]
pub struct HostTable {
    pub snd_valid: unsafe extern "C" fn(usize, i32) -> i32,
    pub snd_play_gui: unsafe extern "C" fn(Bytes, *mut i32) -> usize,
    pub snd_play_3d: unsafe extern "C" fn(Bytes, *const f32, f32, f32, *mut i32) -> usize,
    pub snd_fade_out: unsafe extern "C" fn(usize, f32),
    pub snd_stop: unsafe extern "C" fn(usize),
    pub snd_pause: unsafe extern "C" fn(usize, i32),
    pub snd_volmul: unsafe extern "C" fn(usize, f32),
    pub snd_elapsed: unsafe extern "C" fn(usize) -> f64,
    pub snd_set_elapsed: unsafe extern "C" fn(usize, f64),
    pub snd_commentary: unsafe extern "C" fn(i32),
    pub snd_reset_world: unsafe extern "C" fn(),
    pub gui_sound: unsafe extern "C" fn(Bytes),
    pub translate: unsafe extern "C" fn(Bytes, Bytes, Sink32, Ctx),
    pub wrap: unsafe extern "C" fn(usize, f32, f32, *const f32, *const u32, usize, Sink32, Ctx),
    pub font_load: unsafe extern "C" fn() -> usize,
    pub font_destroy: unsafe extern "C" fn(usize),
    pub hud: unsafe extern "C" fn(*mut f32),
    pub draw_font:
        unsafe extern "C" fn(usize, *const f32, *const f32, *const f32, i32, *const u32, usize),
    pub cfg_f32: unsafe extern "C" fn(Bytes, Bytes, f32) -> f32,
    pub cfg_vec2: unsafe extern "C" fn(Bytes, Bytes, f32, *mut f32),
    pub cfg_str: unsafe extern "C" fn(Bytes, Bytes, Bytes, Sink, Ctx),
    pub cfg_color: unsafe extern "C" fn(Bytes, Bytes, *mut f32),
    pub subtitles: unsafe extern "C" fn() -> i32,
    pub game_time: unsafe extern "C" fn() -> f64,
    pub player: unsafe extern "C" fn(i32, f32, f32, f32),
    pub map: unsafe extern "C" fn(i32, i32, Bytes),
    pub post: unsafe extern "C" fn(i32, i32, f32, f32),
    pub r_reflection: unsafe extern "C" fn(usize) -> i32,
    pub r_collides: unsafe extern "C" fn(usize, usize) -> i32,
    pub r_outline: unsafe extern "C" fn(usize, usize, *mut Outline),
    pub r_clip: unsafe extern "C" fn(usize, *const f32, *const f32, *mut i32),
    pub r_replay:
        unsafe extern "C" fn(usize, *const usize, usize, *const RendOp, usize, *const i32),
    pub p_replay: unsafe extern "C" fn(usize, usize, usize, *const PostOp, usize),
    pub dump: unsafe extern "C" fn(i32, f32) -> usize,
}

static HOST: AtomicPtr<HostTable> = AtomicPtr::new(std::ptr::null_mut());

/// # Safety
/// `t` outlives the program and every function in it is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12a_set_host(t: *const HostTable) {
    HOST.store(t.cast_mut(), Ordering::Release);
}

/// Size and position of the virtual HUD: start position, then size.
#[derive(Clone, Copy, Default)]
pub struct Hud {
    pub start: [f32; 3],
    pub size: [f32; 2],
}

/// `font` 0 is the game's default font.
pub trait Fx {
    fn snd_valid(&self, entry: usize, id: i32) -> bool;
    fn snd_play_gui(&self, file: &[u8]) -> (usize, i32);
    fn snd_play_3d(&self, file: &[u8], pos: [f32; 3], min: f32, max: f32) -> (usize, i32);
    fn snd_fade_out(&self, entry: usize, speed: f32);
    fn snd_stop(&self, entry: usize);
    fn snd_pause(&self, entry: usize, on: bool);
    fn snd_volmul(&self, entry: usize, v: f32);
    fn snd_elapsed(&self, entry: usize) -> f64;
    fn snd_set_elapsed(&self, entry: usize, t: f64);
    fn snd_commentary(&self, mode: i32);
    fn snd_reset_world(&self);
    fn gui_sound(&self, name: &[u8]);
    fn translate(&self, cat: &[u8], entry: &[u8]) -> Vec<u32>;
    fn wrap(
        &self,
        font: usize,
        width: f32,
        line: f32,
        size: [f32; 2],
        text: &[u32],
    ) -> Vec<Vec<u32>>;
    fn font_load(&self) -> usize;
    fn font_destroy(&self, font: usize);
    fn gfx_rect(&self, additive: bool) -> usize;
    fn hud(&self) -> Hud;
    /// `set` 0 is the game HUD set.
    fn draw_gfx(&self, set: usize, gfx: usize, pos: [f32; 3], size: [f32; 2], col: [f32; 4]);
    #[allow(clippy::too_many_arguments)]
    fn draw_font(
        &self,
        font: usize,
        pos: [f32; 3],
        size: [f32; 2],
        col: [f32; 4],
        center: bool,
        text: &[u32],
    );
    fn cfg_f32(&self, sec: &[u8], key: &[u8], def: f32) -> f32;
    fn cfg_vec2(&self, sec: &[u8], key: &[u8], def: f32) -> [f32; 2];
    fn cfg_str(&self, sec: &[u8], key: &[u8], def: &[u8]) -> Vec<u8>;
    fn cfg_color(&self, sec: &[u8], key: &[u8]) -> [f32; 4];
    fn subtitles(&self) -> bool;
    fn game_time(&self) -> f64;
    fn player_fov(&self, mul: f32, speed: f32);
    fn player_active(&self, on: bool);
    /// `None` clears the shake offset.
    fn player_shake(&self, v: Option<[f32; 3]>);
    fn map_icon(&self, id: i32);
    fn map_enemy_reset(&self);
    fn map_script(&self, name: &[u8]);
    fn post(&self, kind: i32, op: i32, a: f32, b: f32);
    fn r_reflection(&self, fns: usize) -> bool;
    fn r_collides(&self, fns: usize, obj: usize) -> bool;
    fn r_outline(&self, fns: usize, obj: usize) -> Outline;
    /// Scissor rectangle (x, y, w, h) of a bounding box on screen.
    fn r_clip(&self, fns: usize, min: Vector3f, max: Vector3f) -> [i32; 4];
    fn r_replay(&self, fns: usize, objs: &[usize], ops: &[RendOp], clip: [i32; 4]);
    fn p_replay(&self, effect: usize, input: usize, fb: usize, ops: &[PostOp]);
}

pub struct HostFx(&'static HostTable);

pub fn host() -> Option<HostFx> {
    // SAFETY: `ohpl_a12a_set_host` stored a table that lives for the whole run, or null.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }.map(HostFx)
}

unsafe extern "C" fn push_bytes(ctx: Ctx, p: *const u8, n: usize) {
    // SAFETY: `ctx` is the `Vec` handed to the host call; the host passes `n` readable bytes.
    let out = unsafe { &mut *ctx.cast::<Vec<u8>>() };
    out.clear();
    if !p.is_null() {
        out.extend_from_slice(unsafe { std::slice::from_raw_parts(p, n) });
    }
}

unsafe extern "C" fn push_units(ctx: Ctx, p: *const u32, n: usize) {
    // SAFETY: as `push_bytes`.
    let out = unsafe { &mut *ctx.cast::<Vec<u32>>() };
    out.clear();
    if !p.is_null() {
        out.extend_from_slice(unsafe { std::slice::from_raw_parts(p, n) });
    }
}

unsafe extern "C" fn push_row(ctx: Ctx, p: *const u32, n: usize) {
    // SAFETY: `ctx` is the row list handed to `wrap`.
    let out = unsafe { &mut *ctx.cast::<Vec<Vec<u32>>>() };
    let row = if p.is_null() {
        Vec::new()
    } else {
        unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
    };
    out.push(row);
}

// SAFETY (all blocks below): every pointer passed is to a live local or slice for the duration
// of the call, and the table's functions accept them.
const MAT_ADDITIVE: i32 = 3;
const MAT_MODULATIVE: i32 = 4;

impl HostFx {
    /// One dump line, when `OHPL_A12A_DUMP` is set.
    fn log(&self, line: impl FnOnce() -> String) {
        if dump::enabled() {
            dump::log(&line());
        }
    }

    /// One render-path dump line (frame 10 on).
    fn rlog(&self, line: impl FnOnce() -> String) {
        dump::rlog(line);
    }

    /// Dump driver queries and the insanity post effect switch.
    pub fn dump(&self, op: i32, arg: f32) -> usize {
        // SAFETY: the table's function takes plain values.
        unsafe { (self.0.dump)(op, arg) }
    }
}

impl Fx for HostFx {
    fn snd_valid(&self, entry: usize, id: i32) -> bool {
        let r = unsafe { (self.0.snd_valid)(entry, id) != 0 };
        self.log(|| format!("snd_valid {} => {}", u8::from(entry != 0), u8::from(r)));
        r
    }

    fn snd_play_gui(&self, file: &[u8]) -> (usize, i32) {
        let mut id = -1;
        let e = unsafe { (self.0.snd_play_gui)(bytes(file), &raw mut id) };
        self.log(|| format!("snd_play_gui {} => {}", dump::escb(file), u8::from(e != 0)));
        (e, id)
    }

    fn snd_play_3d(&self, file: &[u8], pos: [f32; 3], min: f32, max: f32) -> (usize, i32) {
        let mut id = -1;
        let e = unsafe { (self.0.snd_play_3d)(bytes(file), pos.as_ptr(), min, max, &raw mut id) };
        self.log(|| {
            format!(
                "snd_play_3d {} {} {} {} => {}",
                dump::escb(file),
                dump::fs(&pos),
                dump::hf(min),
                dump::hf(max),
                u8::from(e != 0)
            )
        });
        (e, id)
    }

    fn snd_fade_out(&self, entry: usize, speed: f32) {
        self.log(|| format!("snd_fade_out 1 {}", dump::hf(speed)));
        unsafe { (self.0.snd_fade_out)(entry, speed) }
    }

    fn snd_stop(&self, entry: usize) {
        self.log(|| "snd_stop 1".into());
        unsafe { (self.0.snd_stop)(entry) }
    }

    fn snd_pause(&self, entry: usize, on: bool) {
        self.log(|| format!("snd_pause 1 {}", u8::from(on)));
        unsafe { (self.0.snd_pause)(entry, i32::from(on)) }
    }

    fn snd_volmul(&self, entry: usize, v: f32) {
        self.log(|| format!("snd_volmul 1 {}", dump::hf(v)));
        unsafe { (self.0.snd_volmul)(entry, v) }
    }

    fn snd_elapsed(&self, entry: usize) -> f64 {
        let d = unsafe { (self.0.snd_elapsed)(entry) };
        self.log(|| format!("snd_elapsed 1 => {}", dump::hd(d)));
        d
    }

    fn snd_set_elapsed(&self, entry: usize, t: f64) {
        self.log(|| format!("snd_set_elapsed 1 {}", dump::hd(t)));
        unsafe { (self.0.snd_set_elapsed)(entry, t) }
    }

    fn snd_commentary(&self, mode: i32) {
        self.log(|| format!("snd_commentary {mode}"));
        unsafe { (self.0.snd_commentary)(mode) }
    }

    fn snd_reset_world(&self) {
        self.log(|| "snd_reset_world".into());
        unsafe { (self.0.snd_reset_world)() }
    }

    fn gui_sound(&self, name: &[u8]) {
        self.log(|| format!("gui_sound {}", dump::escb(name)));
        unsafe { (self.0.gui_sound)(bytes(name)) }
    }

    fn translate(&self, cat: &[u8], entry: &[u8]) -> Vec<u32> {
        let mut out: Vec<u32> = Vec::new();
        unsafe {
            (self.0.translate)(
                bytes(cat),
                bytes(entry),
                push_units,
                std::ptr::from_mut(&mut out).cast(),
            )
        };
        self.log(|| {
            format!(
                "translate {} {} => {}",
                dump::escb(cat),
                dump::escb(entry),
                dump::esc(&out)
            )
        });
        out
    }

    fn wrap(
        &self,
        font: usize,
        width: f32,
        line: f32,
        size: [f32; 2],
        text: &[u32],
    ) -> Vec<Vec<u32>> {
        let mut rows: Vec<Vec<u32>> = Vec::new();
        unsafe {
            (self.0.wrap)(
                font,
                width,
                line,
                size.as_ptr(),
                text.as_ptr(),
                text.len(),
                push_row,
                std::ptr::from_mut(&mut rows).cast(),
            )
        };
        self.log(|| {
            let r: Vec<String> = rows.iter().map(|r| dump::esc(r)).collect();
            format!(
                "wrap {} {} {} {} {} => {}",
                u8::from(font != 0),
                dump::hf(width),
                dump::hf(line),
                dump::fs(&size),
                dump::esc(text),
                r.join("|")
            )
        });
        rows
    }

    fn font_load(&self) -> usize {
        let f = unsafe { (self.0.font_load)() };
        self.log(|| format!("font_load => {}", u8::from(f != 0)));
        f
    }

    fn font_destroy(&self, font: usize) {
        self.log(|| "font_destroy".into());
        unsafe { (self.0.font_destroy)(font) }
    }

    fn gfx_rect(&self, additive: bool) -> usize {
        let g = direct::gfx_rect(
            direct::WHITE,
            if additive {
                MAT_ADDITIVE
            } else {
                MAT_MODULATIVE
            },
            true,
        );
        self.log(|| format!("gfx_rect {} => 1", if additive { "add" } else { "mod" }));
        g
    }

    fn hud(&self) -> Hud {
        let mut o = [0.0f32; 5];
        unsafe { (self.0.hud)(o.as_mut_ptr()) };
        self.log(|| format!("hud => {}", dump::fs(&o)));
        Hud {
            start: [o[0], o[1], o[2]],
            size: [o[3], o[4]],
        }
    }

    fn draw_gfx(&self, set: usize, gfx: usize, pos: [f32; 3], size: [f32; 2], col: [f32; 4]) {
        self.log(|| {
            format!(
                "draw_gfx 1 {} {} {}",
                dump::fs(&pos),
                dump::fs(&size),
                dump::fs(&col)
            )
        });
        let set = if set == 0 { direct::hud_set() } else { set };
        let c = Color {
            r: col[0],
            g: col[1],
            b: col[2],
            a: col[3],
        };
        direct::draw_gfx(set, gfx, pos, size, c, direct::MAT_LAST, 0.0);
    }

    fn draw_font(
        &self,
        font: usize,
        pos: [f32; 3],
        size: [f32; 2],
        col: [f32; 4],
        center: bool,
        text: &[u32],
    ) {
        self.log(|| {
            format!(
                "draw_font {} {} {} {} {} {}",
                u8::from(font != 0),
                dump::fs(&pos),
                dump::fs(&size),
                dump::fs(&col),
                u8::from(center),
                dump::esc(text)
            )
        });
        unsafe {
            (self.0.draw_font)(
                font,
                pos.as_ptr(),
                size.as_ptr(),
                col.as_ptr(),
                i32::from(center),
                text.as_ptr(),
                text.len(),
            )
        }
    }

    fn cfg_f32(&self, sec: &[u8], key: &[u8], def: f32) -> f32 {
        let f = unsafe { (self.0.cfg_f32)(bytes(sec), bytes(key), def) };
        self.log(|| {
            format!(
                "cfg_f32 {} {} => {}",
                dump::escb(sec),
                dump::escb(key),
                dump::hf(f)
            )
        });
        f
    }

    fn cfg_vec2(&self, sec: &[u8], key: &[u8], def: f32) -> [f32; 2] {
        let mut o = [0.0; 2];
        unsafe { (self.0.cfg_vec2)(bytes(sec), bytes(key), def, o.as_mut_ptr()) };
        self.log(|| {
            format!(
                "cfg_vec2 {} {} => {}",
                dump::escb(sec),
                dump::escb(key),
                dump::fs(&o)
            )
        });
        o
    }

    fn cfg_str(&self, sec: &[u8], key: &[u8], def: &[u8]) -> Vec<u8> {
        let mut out: Vec<u8> = Vec::new();
        unsafe {
            (self.0.cfg_str)(
                bytes(sec),
                bytes(key),
                bytes(def),
                push_bytes,
                std::ptr::from_mut(&mut out).cast(),
            )
        };
        self.log(|| {
            format!(
                "cfg_str {} {} => {}",
                dump::escb(sec),
                dump::escb(key),
                dump::escb(&out)
            )
        });
        out
    }

    fn cfg_color(&self, sec: &[u8], key: &[u8]) -> [f32; 4] {
        let mut o = [0.0; 4];
        unsafe { (self.0.cfg_color)(bytes(sec), bytes(key), o.as_mut_ptr()) };
        self.log(|| {
            format!(
                "cfg_color {} {} => {}",
                dump::escb(sec),
                dump::escb(key),
                dump::fs(&o)
            )
        });
        o
    }

    fn subtitles(&self) -> bool {
        let b = unsafe { (self.0.subtitles)() != 0 };
        self.log(|| format!("subtitles => {}", u8::from(b)));
        b
    }

    fn game_time(&self) -> f64 {
        self.log(|| "game_time".into());
        unsafe { (self.0.game_time)() }
    }

    fn player_fov(&self, mul: f32, speed: f32) {
        self.log(|| format!("player_fov {} {}", dump::hf(mul), dump::hf(speed)));
        unsafe { (self.0.player)(PLAYER_FOV, mul, speed, 0.0) }
    }

    fn player_active(&self, on: bool) {
        self.log(|| format!("player_active {}", u8::from(on)));
        unsafe { (self.0.player)(PLAYER_ACTIVE, f32::from(u8::from(on)), 0.0, 0.0) }
    }

    fn player_shake(&self, v: Option<[f32; 3]>) {
        match v {
            Some([x, y, z]) => {
                self.log(|| "player_shake".into());
                unsafe { (self.0.player)(PLAYER_SHAKE, x, y, z) }
            }
            None => {
                self.log(|| "player_shake_clear".into());
                unsafe { (self.0.player)(PLAYER_SHAKE_CLEAR, 0.0, 0.0, 0.0) }
            }
        }
    }

    fn map_icon(&self, id: i32) {
        self.log(|| format!("map_icon {id}"));
        unsafe { (self.0.map)(MAP_ICON, id, bytes(&[])) }
    }

    fn map_enemy_reset(&self) {
        self.log(|| "map_enemy_reset".into());
        unsafe { (self.0.map)(MAP_ENEMY_RESET, 0, bytes(&[])) }
    }

    fn map_script(&self, name: &[u8]) {
        self.log(|| format!("map_script {}", dump::escb(name)));
        unsafe { (self.0.map)(MAP_SCRIPT, 0, bytes(name)) }
    }

    fn post(&self, kind: i32, op: i32, a: f32, b: f32) {
        self.log(|| {
            let name = ["params", "active", "reset"]
                .get(op as usize)
                .copied()
                .unwrap_or("");
            format!("post {kind} {name} {} {}", dump::hf(a), dump::hf(b))
        });
        unsafe { (self.0.post)(kind, op, a, b) }
    }

    fn r_reflection(&self, fns: usize) -> bool {
        let b = unsafe { (self.0.r_reflection)(fns) != 0 };
        self.rlog(|| format!("r_reflection => {}", u8::from(b)));
        b
    }

    fn r_collides(&self, fns: usize, obj: usize) -> bool {
        let b = unsafe { (self.0.r_collides)(fns, obj) != 0 };
        self.rlog(|| format!("r_collides => {}", u8::from(b)));
        b
    }

    fn r_outline(&self, fns: usize, obj: usize) -> Outline {
        let mut o = Outline::default();
        unsafe { (self.0.r_outline)(fns, obj, &raw mut o) };
        self.rlog(|| {
            let v3 = |v: &Vector3f| dump::fs(&[v.x, v.y, v.z]);
            let world: Vec<f32> = o.world.m.iter().flatten().copied().collect();
            format!(
                "r_outline => {} {} {} {} {} {} {} {}",
                o.collides,
                o.has_alpha,
                v3(&o.min),
                v3(&o.max),
                v3(&o.local_min),
                v3(&o.local_max),
                v3(&o.local_center),
                dump::fs(&world)
            )
        });
        o
    }

    fn r_clip(&self, fns: usize, min: Vector3f, max: Vector3f) -> [i32; 4] {
        let mn = [min.x, min.y, min.z];
        let mx = [max.x, max.y, max.z];
        let mut r = [0i32; 4];
        unsafe { (self.0.r_clip)(fns, mn.as_ptr(), mx.as_ptr(), r.as_mut_ptr()) };
        self.rlog(|| format!("r_clip => {} {} {} {}", r[0], r[1], r[2], r[3]));
        r
    }

    fn r_replay(&self, fns: usize, objs: &[usize], ops: &[RendOp], clip: [i32; 4]) {
        for op in ops {
            self.rlog(|| format!("R {} {} {} {}", op.kind, op.i, op.j, dump::fs(&op.f)));
        }
        unsafe {
            (self.0.r_replay)(
                fns,
                objs.as_ptr(),
                objs.len(),
                ops.as_ptr(),
                ops.len(),
                clip.as_ptr(),
            )
        }
    }

    fn p_replay(&self, effect: usize, input: usize, fb: usize, ops: &[PostOp]) {
        for op in ops {
            self.rlog(|| {
                let masked = op.kind == super::postfx::op::FLOAT && (op.b == 1 || op.b == 3);
                let f0 = if masked {
                    "~".to_string()
                } else {
                    dump::hf(op.f[0])
                };
                format!(
                    "P {} {} {} {} {}",
                    op.kind,
                    op.a,
                    op.b,
                    f0,
                    dump::hf(op.f[1])
                )
            });
        }
        unsafe { (self.0.p_replay)(effect, input, fb, ops.as_ptr(), ops.len()) }
    }
}
