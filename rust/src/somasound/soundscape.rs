//! `SomaSoundscape.cpp`: the map's soundscape areas, their background loops, entity fades, sound
//! prefix and reverb, and the script handler.

use super::game::game;
use super::glue;
use super::host::{AreaDefC, Engine, Str, ops};
use super::scape::{AreaDef, AreaState, ReverbEnv, Scape, ScapeHost};
use crate::somascript::s2_as::{Frame, Soma};
use crate::sound::own::{ent_fade_in, ent_fade_out, entity_of};
use std::ffi::c_void;

const ENTRY_WORLD: i32 = 1;

#[derive(Default)]
pub struct Soundscape {
    pub scape: Scape,
    map: usize,
    world: usize,
    prefix: Vec<u8>,
}

struct Host;

fn handle(bg: usize) -> Option<usize> {
    game().shell_of(bg).map(|s| s.h)
}

impl ScapeHost for Host {
    fn bg_live(&mut self, bg: usize, id: i32) -> bool {
        game().live(bg, id)
    }

    fn bg_fade_out(&mut self, bg: usize, speed: f32) {
        if let Some(h) = handle(bg) {
            game().bank.fade_out(&mut Engine, h, speed);
        }
    }

    fn bg_fade_volume_mul_to(&mut self, bg: usize, dest: f32, speed: f32) {
        if let Some(h) = handle(bg) {
            game().bank.fade_volume_mul_to(h, dest, speed);
        }
    }

    fn bg_play(&mut self, name: &[u8]) -> Option<(usize, i32)> {
        let shell = glue::play_gui(name, 1.0, ENTRY_WORLD, true, false);
        let id = game().shell_of(shell)?.id;
        Some((shell, id))
    }

    fn bg_set_volume_mul(&mut self, bg: usize, vol: f32) {
        if let Some(i) = handle(bg).and_then(|h| game().bank.inst_mut(h)) {
            i.st.volume_mul = vol;
            i.st.volume_mul_dest = vol;
        }
    }

    fn bg_fade_in(&mut self, bg: usize, speed: f32) {
        if let Some(h) = handle(bg) {
            game().bank.fade_in_to(&mut Engine, h, 1.0, speed);
        }
    }

    fn bg_set_paused(&mut self, bg: usize, on: bool) {
        if let Some(h) = handle(bg) {
            game().bank.set_paused(&mut Engine, h, on);
        }
    }

    fn world_present(&mut self) -> bool {
        game().soundscape.world != 0
    }

    fn entity_fade(&mut self, name: &[u8], fade_in: bool, speed: f32) {
        let (w, Some(o)) = (game().soundscape.world, ops()) else {
            return;
        };
        // SAFETY: `w` is the current map's world; `name` is valid for the call.
        let ent = unsafe { (o.world_sound_by_name)(w, name.as_ptr(), name.len()) };
        if ent != 0 {
            let e = entity_of(ent);
            // SAFETY: `e` is the record of a live sound entity of that world.
            if !e.is_null() {
                unsafe {
                    if fade_in {
                        ent_fade_in(e, speed);
                    } else {
                        ent_fade_out(e, speed);
                    }
                }
            }
        }
    }

    fn area(&mut self, ent: usize) -> AreaState {
        let mut st = AreaState::default();
        let mut b = [0.0f32; 19];
        if let Some(o) = ops() {
            // SAFETY: `ent` is an area entity of the current map; `b` holds 19 floats.
            unsafe { (o.area_state)(ent, &mut st.active, b.as_mut_ptr()) };
        }
        st.matrix.copy_from_slice(b.get(..16).unwrap_or(&[0.0; 16]));
        st.size.copy_from_slice(b.get(16..).unwrap_or(&[0.0; 3]));
        st
    }

    fn camera_pos(&mut self) -> Option<[f32; 3]> {
        let mut b = [0.0f32; 3];
        // SAFETY: `b` holds three floats.
        ops()
            .filter(|o| unsafe { (o.camera_pos)(b.as_mut_ptr()) })
            .map(|_| b)
    }

    fn body_pos(&mut self) -> Option<[f32; 3]> {
        let mut b = [0.0f32; 3];
        // SAFETY: `b` holds three floats.
        ops()
            .filter(|o| unsafe { (o.body_pos)(b.as_mut_ptr()) })
            .map(|_| b)
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
        if let Some(o) = ops() {
            // SAFETY: `v` holds the 16 reverb floats.
            unsafe { (o.apply_reverb)(v.as_ptr()) };
        }
    }
}

fn text(s: &Str) -> Vec<u8> {
    // SAFETY: the host fills each string for the duration of the callback.
    unsafe { crate::string_ffi::units(s.p, s.n) }.to_vec()
}

unsafe extern "C" fn collect(ctx: *mut c_void, d: *const AreaDefC) {
    // SAFETY: `ctx` is the vector below; `d` is valid for the callback.
    let (v, d) = unsafe { (&mut *ctx.cast::<Vec<AreaDef>>(), &*d) };
    v.push(AreaDef {
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
    });
}

fn load(map: usize) {
    let mut defs: Vec<AreaDef> = Vec::new();
    let mut world = 0;
    if map != 0
        && let Some(o) = ops()
    {
        // SAFETY: `map` is the current map; `defs` outlives the call.
        unsafe {
            world = (o.scape_world)(map);
            (o.scape_areas)(map, collect, std::ptr::from_mut(&mut defs).cast());
        }
    }
    let s = &mut game().soundscape;
    s.map = map;
    s.world = world;
    s.scape.load(&defs);
    if map != 0 {
        super::host::log(format!("SOMA soundscape: {} areas\n", defs.len()).as_bytes());
    }
}

pub fn update(map: usize, dt: f32) {
    if map != game().soundscape.map {
        load(map);
    }
    let mut sc = std::mem::take(&mut game().soundscape.scape);
    sc.update(&mut Host, dt);
    let s = &mut game().soundscape;
    s.scape = sc;
    s.prefix = s.scape.prefix.clone();
}

pub fn forget(map: usize) {
    if map == game().soundscape.map {
        load(0);
    }
}

pub fn set_paused(on: bool) {
    let sc = std::mem::take(&mut game().soundscape.scape);
    sc.set_paused(&mut Host, on);
    game().soundscape.scape = sc;
}

fn ent_name(ent: usize) -> Vec<u8> {
    unsafe extern "C" fn sink(ctx: *mut c_void, p: *const u8, n: usize) {
        // SAFETY: `ctx` is the vector below; the host passes `n` valid bytes.
        unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(std::slice::from_raw_parts(p, n)) }
    }
    let mut v = Vec::new();
    if let Some(o) = ops() {
        // SAFETY: `v` outlives the call.
        unsafe { (o.ent_name)(ent, sink, std::ptr::from_mut(&mut v).cast()) };
    }
    v
}

fn ts(v: f32) -> String {
    String::from_utf8_lossy(&crate::string::float_to_string(v, 20, false)).into_owned()
}

pub fn describe() -> Vec<u8> {
    let (sc, prefix) = (&game().soundscape.scape, &game().soundscape.prefix);
    let mut s = format!("prefix={} reverb=", String::from_utf8_lossy(prefix));
    if sc.rev_type >= 0 {
        s += usize::try_from(sc.rev_type)
            .ok()
            .and_then(|i| super::presets::NAMES.get(i))
            .copied()
            .unwrap_or("");
    } else {
        s += "-";
    }
    s += &format!("*{}", ts(sc.rev_amount));
    if let Some(a) = sc.rev_area {
        s += &format!("({})", String::from_utf8_lossy(&ent_name(sc.area_ent(a))));
    }
    s += &format!(" room={}", ts(sc.rev_cur.get(1).copied().unwrap_or(0.0)));
    for i in 0..4 {
        let Some((area, bg, name)) = sc.level_info(i) else {
            continue;
        };
        if area.is_some() || bg != 0 {
            let a = area.map_or_else(
                || "-".to_owned(),
                |a| String::from_utf8_lossy(&ent_name(sc.area_ent(a))).into_owned(),
            );
            s += &format!(" L{i}={a}:{}", String::from_utf8_lossy(name));
        }
    }
    s.into_bytes()
}

static TAG: u8 = 0;

pub(super) fn register(s: &Soma) {
    s.func(
        "cLuxSoundscapeHandler@ cLux_GetSoundscapeHandler()",
        |fr: &Frame| {
            fr.ret_ref(std::ptr::from_ref(&TAG).cast_mut());
        },
    );
    s.func(
        "void cLux_SetupDefaultGlobalReverb(eSoundReverbPreset aType, float afFadeTime)",
        |fr| {
            let (t, f) = (fr.arg::<i32>(0), fr.arg::<f32>(1));
            game().soundscape.scape.set_default_reverb(t, f);
        },
    );
    s.method(
        "cLuxSoundscapeHandler",
        "const tString& GetCurrentSoundPrefix()",
        |fr| {
            let prefix = game().soundscape.prefix.clone();
            fr.ret_ref(
                ops()
                    .map_or(std::ptr::null(), |o| {
                        // SAFETY: `prefix` is valid for the call.
                        unsafe { (o.prefix_ref)(prefix.as_ptr(), prefix.len()) }
                    })
                    .cast_mut(),
            );
        },
    );
}
