//! `cSomaSoundscape`: soundscape areas, their background loops, sound entity fades, the sound
//! prefix and the reverb preset blending. Entities, the player and the sound events stay C++.

use super::presets::{NAMES, PRESETS};
use super::{powf, std_min};
use crate::math::{clamp, fma, matrix_inverse, matrix_mul_vec, v3};
use crate::string::split;
use crate::types::Matrixf;

pub const LEVELS: usize = 4;
pub const REV_FIELDS: usize = 17;

/// The `Soundscape` area entity's variables as the map holds them.
#[derive(Default)]
pub struct AreaDef {
    pub ent: usize,
    pub level: Vec<u8>,
    pub prio: i32,
    pub bg: Vec<u8>,
    pub bg_volume: f32,
    pub bg_fade_in: f32,
    pub bg_trans_speed: f32,
    pub parent_vol_mul: f32,
    pub in_names: Vec<u8>,
    pub out_names: Vec<u8>,
    pub ent_fade_in: f32,
    pub ent_fade_out: f32,
    pub prefix: Vec<u8>,
    pub prefix_prio: i32,
    pub use_reverb: bool,
    pub reverb_prio: i32,
    pub reverb_type: Vec<u8>,
    pub reverb_amount: f32,
    pub reverb_fade: f32,
}

struct Area {
    ent: usize,
    level: usize,
    prio: i32,
    bg: Vec<u8>,
    bg_volume: f32,
    bg_fade_in: f32,
    bg_trans_speed: f32,
    parent_vol_mul: f32,
    in_names: Vec<Vec<u8>>,
    out_names: Vec<Vec<u8>>,
    ent_fade_in: f32,
    ent_fade_out: f32,
    prefix: Vec<u8>,
    prefix_prio: i32,
    use_reverb: bool,
    reverb_prio: i32,
    reverb_type: i32,
    reverb_amount: f32,
    reverb_fade: f32,
    inside: bool,
}

struct Level {
    area: Option<usize>,
    bg: usize,
    bg_id: i32,
    bg_name: Vec<u8>,
    bg_volume: f32,
    bg_fade: f32,
    parent_vol_mul: f32,
    in_names: Vec<Vec<u8>>,
    out_names: Vec<Vec<u8>>,
    ent_fade_in: f32,
    ent_fade_out: f32,
}

impl Default for Level {
    fn default() -> Self {
        Self {
            area: None,
            bg: 0,
            bg_id: -1,
            bg_name: Vec::new(),
            bg_volume: 0.0,
            bg_fade: 0.0,
            parent_vol_mul: 1.0,
            in_names: Vec::new(),
            out_names: Vec::new(),
            ent_fade_in: 1.0,
            ent_fade_out: 1.0,
        }
    }
}

#[derive(Clone, Copy, Default)]
pub struct AreaState {
    pub active: bool,
    pub matrix: [f32; 16],
    pub size: [f32; 3],
}

/// The area entities, the player, the sound events and the OpenAL environment.
pub trait ScapeHost {
    fn bg_live(&mut self, bg: usize, id: i32) -> bool;
    fn bg_fade_out(&mut self, bg: usize, speed: f32);
    fn bg_fade_volume_mul_to(&mut self, bg: usize, dest: f32, speed: f32);
    fn bg_play(&mut self, name: &[u8]) -> Option<(usize, i32)>;
    fn bg_set_volume_mul(&mut self, bg: usize, vol: f32);
    fn bg_fade_in(&mut self, bg: usize, speed: f32);
    fn bg_set_paused(&mut self, bg: usize, on: bool);
    fn world_present(&mut self) -> bool;
    fn entity_fade(&mut self, name: &[u8], fade_in: bool, speed: f32);
    fn area(&mut self, ent: usize) -> AreaState;
    fn camera_pos(&mut self) -> Option<[f32; 3]>;
    fn body_pos(&mut self) -> Option<[f32; 3]>;
    fn apply_reverb(&mut self, env: &ReverbEnv);
}

/// The EFX values `ApplyReverb` sets.
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct ReverbEnv {
    pub gain: f32,
    pub gain_hf: f32,
    pub gain_lf: f32,
    pub decay_time: f32,
    pub decay_hf_ratio: f32,
    pub decay_lf_ratio: f32,
    pub reflections_gain: f32,
    pub reflections_delay: f32,
    pub late_gain: f32,
    pub late_delay: f32,
    pub mod_time: f32,
    pub mod_depth: f32,
    pub hf_ref: f32,
    pub lf_ref: f32,
    pub diffusion: f32,
    pub density: f32,
}

pub struct Scape {
    areas: Vec<Area>,
    active: Vec<usize>,
    levels: [Level; LEVELS],
    pub prefix: Vec<u8>,
    pub rev_cur: [f32; REV_FIELDS],
    rev_from: [f32; REV_FIELDS],
    rev_to: [f32; REV_FIELDS],
    rev_t: f32,
    rev_time: f32,
    pub rev_type: i32,
    pub rev_amount: f32,
    pub rev_area: Option<usize>,
    default_reverb: i32,
    default_reverb_fade: f32,
}

impl Default for Scape {
    fn default() -> Self {
        let mut s = Self {
            areas: Vec::new(),
            active: Vec::new(),
            levels: Default::default(),
            prefix: Vec::new(),
            rev_cur: [0.0; REV_FIELDS],
            rev_from: [0.0; REV_FIELDS],
            rev_to: [0.0; REV_FIELDS],
            rev_t: 1.0,
            rev_time: 0.0,
            rev_type: -2,
            rev_amount: -1.0,
            rev_area: None,
            default_reverb: 0,
            default_reverb_fade: 1.0,
        };
        s.reset_reverb();
        s
    }
}

pub fn level_from_string(s: &[u8]) -> usize {
    match s {
        [d @ b'0'..=b'3', b':', ..] => usize::from(d - b'0'),
        _ => 2,
    }
}

pub fn reverb_from_string(s: &[u8]) -> i32 {
    NAMES
        .iter()
        .position(|n| n.as_bytes() == s)
        .map_or(0, |i| i as i32)
}

fn fade_speed(time: f32) -> f32 {
    if time > 0.0 { 1.0 / time } else { 1000.0 }
}

fn names(s: &[u8]) -> Vec<Vec<u8>> {
    split(s, None, true).map(<[u8]>::to_vec).collect()
}

fn mb(x: f32, max: f32) -> f32 {
    clamp(powf(10.0, x.round() / 2000.0), 0.0, max)
}

/// `ApplyReverb`: FMOD Ex reverb properties to EFX EAX reverb.
pub fn reverb_env(p: &[f32; REV_FIELDS]) -> ReverbEnv {
    ReverbEnv {
        gain: mb(p[1], 1.0),
        gain_hf: mb(p[2], 1.0),
        gain_lf: mb(p[3], 1.0),
        decay_time: clamp(p[4], 0.1, 20.0),
        decay_hf_ratio: clamp(p[5], 0.1, 2.0),
        decay_lf_ratio: clamp(p[6], 0.1, 2.0),
        reflections_gain: mb(p[7], 3.16),
        reflections_delay: clamp(p[8], 0.0, 0.3),
        late_gain: mb(p[9], 10.0),
        late_delay: clamp(p[10], 0.0, 0.1),
        mod_time: clamp(p[11], 0.04, 4.0),
        mod_depth: clamp(p[12], 0.0, 1.0),
        hf_ref: clamp(p[13], 1000.0, 20000.0),
        lf_ref: clamp(p[14], 20.0, 1000.0),
        diffusion: p[15] / 100.0,
        density: p[16] / 100.0,
    }
}

impl Scape {
    fn reset_reverb(&mut self) {
        self.rev_type = -2;
        self.rev_area = None;
        self.default_reverb = 0;
        self.default_reverb_fade = 1.0;
    }

    /// `Load`: forgets the map's areas and reads the new ones (none for no map).
    pub fn load(&mut self, defs: &[AreaDef]) {
        self.areas.clear();
        self.active.clear();
        for l in &mut self.levels {
            l.area = None;
            l.in_names.clear();
            l.out_names.clear();
        }
        self.reset_reverb();
        for d in defs {
            self.areas.push(Area {
                ent: d.ent,
                level: level_from_string(&d.level),
                prio: d.prio,
                bg: d.bg.clone(),
                bg_volume: d.bg_volume,
                bg_fade_in: d.bg_fade_in,
                bg_trans_speed: d.bg_trans_speed,
                parent_vol_mul: d.parent_vol_mul,
                in_names: names(&d.in_names),
                out_names: names(&d.out_names),
                ent_fade_in: d.ent_fade_in,
                ent_fade_out: d.ent_fade_out,
                prefix: if d.prefix == b"[None]" {
                    Vec::new()
                } else {
                    d.prefix.clone()
                },
                prefix_prio: d.prefix_prio,
                use_reverb: d.use_reverb,
                reverb_prio: d.reverb_prio,
                reverb_type: reverb_from_string(&d.reverb_type),
                reverb_amount: d.reverb_amount,
                reverb_fade: d.reverb_fade,
                inside: false,
            });
        }
    }

    pub fn area_count(&self) -> usize {
        self.areas.len()
    }

    fn inside(host: &mut dyn ScapeHost, ent: usize, pos: [f32; 3]) -> bool {
        let st = host.area(ent);
        let mut m = Matrixf::default();
        for (row, chunk) in m.m.iter_mut().zip(st.matrix.chunks_exact(4)) {
            row.copy_from_slice(chunk);
        }
        let local = matrix_mul_vec(&matrix_inverse(&m), v3(pos[0], pos[1], pos[2]));
        let l = [local.x, local.y, local.z];
        l.iter()
            .zip(&st.size)
            .all(|(l, s)| l.abs().partial_cmp(&(s * 0.5)) != Some(std::cmp::Ordering::Greater))
    }

    fn bg_live(&self, host: &mut dyn ScapeHost, level: usize) -> bool {
        self.levels
            .get(level)
            .is_some_and(|l| l.bg != 0 && host.bg_live(l.bg, l.bg_id))
    }

    fn stop_bg(&mut self, host: &mut dyn ScapeHost, level: usize) {
        if self.bg_live(host, level)
            && let Some(l) = self.levels.get(level)
        {
            host.bg_fade_out(l.bg, fade_speed(l.bg_fade));
        }
        if let Some(l) = self.levels.get_mut(level) {
            l.bg = 0;
            l.bg_name.clear();
            l.parent_vol_mul = 1.0;
        }
    }

    /// The old background fades out over the new area's fade-in time.
    fn update_bg(
        &mut self,
        host: &mut dyn ScapeHost,
        level: usize,
        area: Option<usize>,
        parent_prod: f32,
    ) {
        let Some(a) = area
            .and_then(|a| self.areas.get(a))
            .filter(|a| !a.bg.is_empty())
        else {
            self.stop_bg(host, level);
            return;
        };
        let (bg, bg_volume, fade_in, trans_speed) =
            (a.bg.clone(), a.bg_volume, a.bg_fade_in, a.bg_trans_speed);
        let target = bg_volume * parent_prod;
        let live = self.bg_live(host, level);
        let Some(l) = self.levels.get_mut(level) else {
            return;
        };
        if bg == l.bg_name && live {
            if bg_volume == l.bg_volume {
                host.bg_fade_volume_mul_to(l.bg, target, trans_speed);
            } else {
                host.bg_fade_volume_mul_to(
                    l.bg,
                    target,
                    if fade_in > 0.0 {
                        target / fade_in
                    } else {
                        10000.0
                    },
                );
            }
        } else {
            if live {
                host.bg_fade_out(l.bg, fade_speed(fade_in));
            }
            let played = host.bg_play(&bg);
            l.bg = played.map_or(0, |p| p.0);
            l.bg_id = played.map_or(-1, |p| p.1);
            if let Some((p, _)) = played {
                host.bg_set_volume_mul(p, target);
                host.bg_fade_in(p, if fade_in > 0.0 { 1.0 / fade_in } else { 0.0 });
            }
        }
        l.bg_name = bg;
        l.bg_volume = bg_volume;
        l.bg_fade = fade_in;
    }

    fn fade_entities(
        host: &mut dyn ScapeHost,
        a: &[Vec<u8>],
        b: &[Vec<u8>],
        time: f32,
        fade_in: bool,
    ) {
        if !host.world_present() {
            return;
        }
        for name in a {
            if b.contains(name) {
                continue;
            }
            host.entity_fade(name, fade_in, fade_speed(time));
        }
    }

    fn update_entities(&mut self, host: &mut dyn ScapeHost, level: usize, area: Option<usize>) {
        let Some(l) = self.levels.get(level) else {
            return;
        };
        let Some(a) = area.and_then(|a| self.areas.get(a)) else {
            Self::fade_entities(host, &l.out_names, &[], l.ent_fade_in, true);
            Self::fade_entities(host, &l.in_names, &[], l.ent_fade_out, false);
            if let Some(l) = self.levels.get_mut(level) {
                l.in_names.clear();
                l.out_names.clear();
            }
            return;
        };
        Self::fade_entities(host, &l.in_names, &a.in_names, a.ent_fade_out, false);
        Self::fade_entities(host, &a.in_names, &l.in_names, a.ent_fade_in, true);
        Self::fade_entities(host, &l.out_names, &a.out_names, a.ent_fade_in, true);
        Self::fade_entities(host, &a.out_names, &l.out_names, a.ent_fade_out, false);
        let (inn, out, fi, fo) = (
            a.in_names.clone(),
            a.out_names.clone(),
            a.ent_fade_in,
            a.ent_fade_out,
        );
        if let Some(l) = self.levels.get_mut(level) {
            l.in_names = inn;
            l.out_names = out;
            l.ent_fade_in = fi;
            l.ent_fade_out = fo;
        }
    }

    pub fn update(&mut self, host: &mut dyn ScapeHost, dt: f32) {
        self.update_reverb(host, dt);
        let Some(cam) = host.camera_pos() else { return };
        let body = host.body_pos();

        self.prefix.clear();
        let mut prefix_prio = -9_999_999;
        for i in 0..self.areas.len() {
            let Some(a) = self.areas.get(i) else { continue };
            let ent = a.ent;
            let state = host.area(ent);
            let is_inside = state.active && Self::inside(host, ent, cam);
            let Some(a) = self.areas.get_mut(i) else {
                continue;
            };
            if is_inside != a.inside {
                a.inside = is_inside;
                if is_inside {
                    self.active.push(i);
                } else if let Some(p) = self.active.iter().position(|&x| x == i) {
                    self.active.remove(p);
                }
            }
            let Some(a) = self.areas.get(i) else { continue };
            if let Some(b) = body
                && state.active
                && a.prefix_prio > prefix_prio
                && Self::inside(host, a.ent, b)
            {
                prefix_prio = a.prefix_prio;
                self.prefix = a.prefix.clone();
            }
        }

        // first area taken, later ones only on strictly higher prio
        let mut rev: Option<usize> = None;
        for &i in &self.active {
            let Some(a) = self.areas.get(i) else { continue };
            if a.use_reverb
                && rev.is_none_or(|r| {
                    self.areas
                        .get(r)
                        .is_some_and(|r| a.reverb_prio > r.reverb_prio)
                })
            {
                rev = Some(i);
            }
        }
        let rev_area = rev.and_then(|r| self.areas.get(r));
        let ty = rev_area.map_or(self.default_reverb, |a| a.reverb_type);
        let amount = rev_area.map_or(1.0, |a| a.reverb_amount);
        if ty != self.rev_type || amount != self.rev_amount || rev != self.rev_area {
            let first = self.rev_type == -2;
            self.rev_type = ty;
            self.rev_amount = amount;
            self.rev_area = rev;
            let preset = usize::try_from(ty)
                .ok()
                .and_then(|t| PRESETS.get(t))
                .copied()
                .unwrap_or([0.0; REV_FIELDS]);
            self.rev_to = preset;
            for (to, p) in self.rev_to.iter_mut().zip(&preset).skip(1).take(3) {
                *to = clamp(fma(p + 10000.0, amount, -10000.0), -10000.0, 0.0);
            }
            self.rev_from = self.rev_cur;
            self.rev_time = if first {
                0.0
            } else {
                rev_area.map_or(self.default_reverb_fade, |a| a.reverb_fade)
            };
            self.rev_t = 0.0;
        }

        let mut best: [Option<usize>; LEVELS] = [None; LEVELS];
        let mut best_prio = [-100_000i32; LEVELS];
        for &i in &self.active {
            let Some(a) = self.areas.get(i) else { continue };
            if let (Some(bp), Some(b)) = (best_prio.get_mut(a.level), best.get_mut(a.level))
                && *bp < a.prio
            {
                *bp = a.prio;
                *b = Some(i);
            }
        }
        let mut same = true;
        for (i, b) in best.iter().enumerate() {
            let live = self.bg_live(host, i);
            let area = self.levels.get(i).and_then(|l| l.area);
            same = same && *b == area && (b.is_some() || !live);
        }
        if same {
            return;
        }
        for (l, b) in self.levels.iter_mut().zip(&best) {
            l.parent_vol_mul = b
                .and_then(|a| self.areas.get(a))
                .map_or(1.0, |a| a.parent_vol_mul);
        }
        for (i, b) in best.iter().enumerate() {
            let mut prod = 1.0f32;
            for l in self.levels.iter().skip(i + 1) {
                prod *= l.parent_vol_mul;
            }
            self.update_bg(host, i, *b, prod);
        }
        for (i, b) in best.iter().enumerate() {
            self.update_entities(host, i, *b);
            if let Some(l) = self.levels.get_mut(i) {
                l.area = *b;
            }
        }
    }

    pub fn set_default_reverb(&mut self, preset: i32, fade: f32) {
        self.default_reverb = clamp(preset as f32, 0.0, 41.0) as i32;
        self.default_reverb_fade = fade;
    }

    fn update_reverb(&mut self, host: &mut dyn ScapeHost, dt: f32) {
        if self.rev_t >= 1.0 {
            return;
        }
        self.rev_t = if self.rev_time <= 0.0001 {
            1.0
        } else {
            std_min(self.rev_t + dt / self.rev_time, 1.0)
        };
        for ((cur, to), from) in self
            .rev_cur
            .iter_mut()
            .zip(&self.rev_to)
            .zip(&self.rev_from)
        {
            *cur = fma(to - from, self.rev_t, *from);
        }
        host.apply_reverb(&reverb_env(&self.rev_cur));
    }

    pub fn set_paused(&self, host: &mut dyn ScapeHost, on: bool) {
        for i in 0..LEVELS {
            if self.bg_live(host, i)
                && let Some(l) = self.levels.get(i)
            {
                host.bg_set_paused(l.bg, on);
            }
        }
    }

    /// Per level: (area index, background instance live-or-not pointer, background name).
    pub fn level_info(&self, level: usize) -> Option<(Option<usize>, usize, &[u8])> {
        self.levels
            .get(level)
            .map(|l| (l.area, l.bg, l.bg_name.as_slice()))
    }

    pub fn area_ent(&self, area: usize) -> usize {
        self.areas.get(area).map_or(0, |a| a.ent)
    }
}
