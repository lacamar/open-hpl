//! `cSomaSoundInstance`: one playing FMOD Designer event. The sound handler and the entity
//! behind the instance are reached through [`Host`].

use super::events::{
    DSP_EQ_GAIN, DSP_HIGHPASS, DSP_LOWPASS, DSP_PAN_LEVEL, DSP_PITCH, DSP_REVERB_LEVEL, DSP_VOLUME,
    Event, Events, ROLLOFF_LINEAR, ROLLOFF_LINEAR_SQUARE, ROLLOFF_LOG, STEAL_NEWEST, STEAL_OLDEST,
    STEAL_QUIETEST, category_entry_type, db_to_gain,
};
use super::{fmodf, powf, std_max, std_min};
use crate::math::{self, cross3, fma, rand_rectf, rand_rectl, v3};
use crate::string::to_lower;
use crate::types::Vector3f;

pub const PLAY_3D: u8 = 0;
pub const PLAY_GUI_STREAM: u8 = 1;
pub const PLAY_GUI: u8 = 2;

pub struct PlayReq<'a> {
    pub kind: u8,
    pub file: &'a [u8],
    pub looped: bool,
    pub pos: [f32; 3],
    pub entry_type: i32,
    pub stream: bool,
}

#[derive(Clone, Copy, Default)]
pub struct Listener {
    pub pos: [f32; 3],
    pub fwd: [f32; 3],
    pub up: [f32; 3],
}

#[derive(Clone, Copy, Default)]
pub struct EntityInfo {
    pub world_pos: [f32; 3],
    pub min_dist: f32,
    pub max_dist: f32,
    pub volume: f32,
}

#[derive(Clone, Copy)]
pub enum Filter {
    Hf,
    Lf,
    Send,
}

/// The sound handler, its entries and channels, the listener and the entity.
pub trait Host {
    fn valid(&mut self, entry: usize, id: i32) -> bool;
    fn play(&mut self, req: &PlayReq) -> Option<(usize, i32)>;
    fn stop(&mut self, entry: usize);
    fn pause(&mut self, entry: usize, on: bool);
    fn total_time(&mut self, entry: usize) -> f64;
    fn set_looping(&mut self, entry: usize, on: bool);
    fn set_filter(&mut self, entry: usize, which: Filter, gain: f32);
    fn set_position(&mut self, entry: usize, pos: [f32; 3]);
    fn set_default_volume(&mut self, entry: usize, vol: f32);
    fn set_default_speed(&mut self, entry: usize, speed: f32);
    fn listener(&mut self) -> Listener;
    fn entity(&mut self, entity: usize) -> EntityInfo;
}

/// Scalar state, shared with C++ (`OhplGs5InstState`).
#[repr(C)]
pub struct State {
    pub event: usize,
    pub entity: usize,
    pub id: i32,
    pub entry_type: i32,
    pub pos: [f32; 3],
    pub rand_gain: f32,
    pub rand_pitch: f32,
    pub volume: f32,
    pub volume_mul: f32,
    pub volume_mul_dest: f32,
    pub volume_mul_speed: f32,
    pub speed_mul: f32,
    pub speed_mul_dest: f32,
    pub speed_mul_speed: f32,
    pub fade: f32,
    pub fade_dest: f32,
    pub fade_speed: f32,
    pub start_fails: i32,
    pub time: f32,
    pub audibility: f32,
    pub stop_at_fade_end: bool,
    pub started: bool,
    pub stopped: bool,
    pub paused: bool,
    pub stop_disabled: bool,
    pub play_3d: bool,
    pub has_entity: bool,
}

pub struct Voice {
    pub entry: usize,
    pub entry_id: i32,
    pub layer: i32,
    pub sound: i32,
    pub gain: f32,
    pub speed: f32,
    pub looped: bool,
    pub gain_hf: f32,
    pub gain_lf: f32,
    pub send_gain: f32,
}

#[derive(Clone, Copy, Default)]
struct Slot {
    in_range: bool,
    triggered: bool,
    delay: f32,
    spawn_timer: f32,
}

/// `st` first: C++ keeps a pointer to it.
#[repr(C)]
pub struct Inst {
    pub st: State,
    pub voices: Vec<Voice>,
    slots: Vec<Vec<Slot>>,
    pub param_value: Vec<f32>,
    param_target: Vec<f32>,
}

#[derive(Default)]
pub struct Bank {
    pub events: Events,
    insts: Vec<Option<Box<Inst>>>,
    order: Vec<usize>,
    next_id: i32,
}

fn rand_db(rand_db: f32) -> f32 {
    if rand_db < 0.0 {
        db_to_gain(rand_rectf(rand_db, 0.0))
    } else {
        1.0
    }
}

fn rand_range(min: f32, max: f32) -> f32 {
    if max > min { rand_rectf(min, max) } else { min }
}

fn approach(x: &mut f32, dest: f32, speed: f32, dt: f32) {
    if *x < dest {
        *x = std_min(dest, *x + speed * dt);
    } else {
        *x = std_max(dest, *x - speed * dt);
    }
}

fn vec3(a: [f32; 3]) -> Vector3f {
    v3(a[0], a[1], a[2])
}

/// `x*x + y*y + z*z` as gcc contracts it.
fn sqr_len(x: f32, y: f32, z: f32) -> f32 {
    fma(z, z, fma(x, x, y * y))
}

const MINUS_ONE: f32 = -1.0;

const PAN_Q: [f32; 16] = [
    0.0, 0.0897, 0.175, 0.263, 0.346, 0.429, 0.510, 0.585, 0.656, 0.725, 0.789, 0.846, 0.895,
    0.938, 0.973, 1.0,
];

impl Inst {
    fn source_pos(&self, ent: &EntityInfo) -> [f32; 3] {
        if self.st.has_entity {
            ent.world_pos
        } else {
            self.st.pos
        }
    }

    fn param_norm(&self, ev: &Event, idx: usize) -> f32 {
        let (Some(p), Some(v)) = (ev.params.get(idx), self.param_value.get(idx)) else {
            return 0.0;
        };
        if p.max > p.min {
            (v - p.min) / (p.max - p.min)
        } else {
            0.0
        }
    }

    /// `PanPos`: where OpenAL Soft's stereo panpot gives FMOD's constant power L/R ratio.
    fn pan_pos(&self, ev: &Event, ent: &EntityInfo, lis: &Listener, pan_mul: f32) -> [f32; 3] {
        let pos = self.source_pos(ent);
        let dir = [
            pos[0] - lis.pos[0],
            pos[1] - lis.pos[1],
            pos[2] - lis.pos[2],
        ];
        let dist = sqr_len(dir[0], dir[1], dir[2]).sqrt();
        if dist < 1e-3 {
            return pos;
        }
        let fwd = lis.fwd.map(|x| x * MINUS_ONE);
        let right = cross3(vec3(fwd), vec3(lis.up));
        let pan = math::dot3(vec3(dir), right) / dist * ev.pan_level * pan_mul;
        let abs = std_min(pan.abs(), 1.0);
        let l = ((1.0 - abs) / 2.0).sqrt();
        let r = ((1.0 + abs) / 2.0).sqrt();
        let q = (r - l) / (r + l);
        let mut i = 1usize;
        while i < 15 && PAN_Q.get(i).is_some_and(|&v| v < q) {
            i += 1;
        }
        let (lo, hi) = (
            PAN_Q.get(i - 1).copied().unwrap_or(0.0),
            PAN_Q.get(i).copied().unwrap_or(1.0),
        );
        let angle = math::to_rad(4.0 * ((i - 1) as f32 + (q - lo) / (hi - lo)));
        let (c, s) = (math::cos(angle), math::sin(angle));
        let k = s * if pan < 0.0 { -1.0 } else { 1.0 };
        let mut out = [0.0; 3];
        for n in 0..3 {
            let (f, rt, l0) = (
                [fwd[0], fwd[1], fwd[2]].get(n).copied().unwrap_or(0.0),
                [right.x, right.y, right.z].get(n).copied().unwrap_or(0.0),
                lis.pos.get(n).copied().unwrap_or(0.0),
            );
            if let Some(o) = out.get_mut(n) {
                *o = fma(fma(rt, k, f * c), dist, l0);
            }
        }
        out
    }

    fn listener_distance(&self, ent: &EntityInfo, lis: &Listener) -> f32 {
        let p = self.source_pos(ent);
        let (dx, dy, dz) = (p[0] - lis.pos[0], p[1] - lis.pos[1], p[2] - lis.pos[2]);
        sqr_len(dx, dy, dz).sqrt()
    }

    fn distance_gain(&self, ev: &Event, ent: &EntityInfo, dist: f32) -> f32 {
        let (min, max) = if self.st.has_entity {
            (ent.min_dist, ent.max_dist)
        } else {
            (ev.min_dist, ev.max_dist)
        };
        if dist <= min {
            return 1.0;
        }
        match ev.rolloff {
            ROLLOFF_LINEAR | ROLLOFF_LINEAR_SQUARE => {
                let t = if max > min {
                    math::clamp(1.0 - (dist - min) / (max - min), 0.0, 1.0)
                } else {
                    0.0
                };
                if ev.rolloff == ROLLOFF_LINEAR {
                    t
                } else {
                    t * t
                }
            }
            ROLLOFF_LOG => min / std_min(dist, std_max(max, min)),
            _ => 1.0,
        }
    }

    fn stop_now(&mut self, host: &mut dyn Host) {
        for v in &self.voices {
            if host.valid(v.entry, v.entry_id) {
                host.stop(v.entry);
            }
        }
        self.voices.clear();
        self.st.stopped = true;
        self.st.audibility = 0.0;
    }

    fn stop(&mut self, host: &mut dyn Host, ev: &Event) {
        if self.st.stop_disabled || self.st.stopped {
            return;
        }
        if ev.fade_out > 0.0 {
            self.st.stopped = true;
            self.st.stop_at_fade_end = true;
            self.st.fade_dest = 0.0;
            self.st.fade_speed = -1.0 / ev.fade_out;
            return;
        }
        self.stop_now(host);
    }
}

impl Inst {
    fn start(&mut self, bank: &mut Bank, host: &mut dyn Host, ev: &mut Event) {
        if ev.max_playbacks > 0 {
            let same: Vec<usize> = bank
                .order
                .iter()
                .copied()
                .filter(|&o| {
                    bank.inst(o).is_some_and(|p| {
                        p.st.event == self.st.event && p.st.started && !p.st.stopped
                    })
                })
                .collect();
            if same.len() as i32 >= ev.max_playbacks {
                let audibility = |o: usize| bank.inst(o).map_or(0.0, |p| p.st.audibility);
                let steal = match ev.max_behavior {
                    STEAL_OLDEST => same.first().copied(),
                    STEAL_NEWEST => same.last().copied(),
                    STEAL_QUIETEST => {
                        let mut s = same.first().copied();
                        for &o in &same {
                            if s.is_some_and(|c| audibility(o) < audibility(c)) {
                                s = Some(o);
                            }
                        }
                        s
                    }
                    _ => None,
                };
                let Some(steal) = steal else {
                    self.st.start_fails = self.st.start_fails.wrapping_add(1);
                    let stop = ev.one_shot || !self.st.has_entity || self.st.start_fails > 201;
                    self.st.started = stop;
                    self.st.stopped = stop;
                    return;
                };
                bank.stop_now(host, steal);
            }
        }
        self.st.started = true;
        self.st.stopped = false;
        self.st.stop_at_fade_end = false;
        self.st.time = 0.0;
        self.st.rand_gain = rand_db(ev.volume_rand_db);
        self.st.rand_pitch = rand_rectf(-ev.pitch_rand, ev.pitch_rand);
        self.slots = ev
            .layers
            .iter()
            .map(|l| vec![Slot::default(); l.sounds.len()])
            .collect();
        if ev.fade_in > 0.0 {
            self.st.fade = 0.0;
            self.st.fade_dest = 1.0;
            self.st.fade_speed = 1.0 / ev.fade_in;
        }
    }

    fn start_voice(
        &mut self,
        host: &mut dyn Host,
        ev: &mut Event,
        at: (usize, usize),
        looped: bool,
        scene: (&Listener, &EntityInfo),
    ) -> bool {
        let (layer, sound) = at;
        let Some(def_idx) = ev
            .layers
            .get(layer)
            .and_then(|l| l.sounds.get(sound))
            .and_then(|s| usize::try_from(s.def).ok())
        else {
            return false;
        };
        let Some(def) = ev.defs.get_mut(def_idx) else {
            return false;
        };
        let num = def.files.len() as i32;
        if num == 0 {
            return false;
        }
        let idx = if def.sequential {
            def.last.wrapping_add(1).wrapping_rem(num)
        } else {
            let mut i = rand_rectl(0, num - 1);
            if num > 1 && i == def.last {
                i = (i + 1 + rand_rectl(0, num - 2)).wrapping_rem(num);
            }
            i
        };
        def.last = idx;
        let file = usize::try_from(idx)
            .ok()
            .and_then(|i| def.files.get(i))
            .cloned()
            .unwrap_or_default();
        if file.is_empty() {
            return false;
        }
        let (vol_rand, pitch, pitch_rand) = (def.volume_rand_db, def.pitch, def.pitch_rand);
        let (kind, pos) = if self.st.play_3d {
            (PLAY_3D, self.pan_pos(ev, scene.1, scene.0, 1.0))
        } else if ev.stream {
            (PLAY_GUI_STREAM, [0.0, 0.0, 1.0])
        } else {
            (PLAY_GUI, [0.0, 0.0, 1.0])
        };
        let Some((entry, entry_id)) = host.play(&PlayReq {
            kind,
            file: &file,
            looped,
            pos,
            entry_type: self.st.entry_type,
            stream: ev.stream,
        }) else {
            return false;
        };
        self.voices.push(Voice {
            entry,
            entry_id,
            layer: layer as i32,
            sound: sound as i32,
            gain: rand_db(vol_rand),
            speed: powf(2.0, pitch + rand_rectf(-pitch_rand, pitch_rand)),
            looped,
            gain_hf: 1.0,
            gain_lf: 1.0,
            send_gain: 1.0,
        });
        true
    }

    fn update(&mut self, bank: &mut Bank, host: &mut dyn Host, ev: &mut Event, dt: f32) {
        if self.st.paused {
            return;
        }
        if !self.st.started {
            if !self.st.has_entity || self.st.stopped {
                return;
            }
            self.start(bank, host, ev);
            if !self.st.started {
                return;
            }
        }
        if self.st.stopped && self.voices.is_empty() {
            return;
        }
        self.st.time += dt;
        if self.st.fade_speed != 0.0 {
            approach(
                &mut self.st.fade,
                self.st.fade_dest,
                self.st.fade_speed.abs(),
                dt,
            );
            if self.st.fade == self.st.fade_dest {
                self.st.fade_speed = 0.0;
                if self.st.stop_at_fade_end {
                    self.stop_now(host);
                    self.st.fade = 1.0;
                    self.st.fade_dest = 1.0;
                    self.st.stop_at_fade_end = false;
                    return;
                }
            }
        }
        approach(
            &mut self.st.volume_mul,
            self.st.volume_mul_dest,
            self.st.volume_mul_speed,
            dt,
        );
        approach(
            &mut self.st.speed_mul,
            self.st.speed_mul_dest,
            self.st.speed_mul_speed,
            dt,
        );

        let ent = if self.st.has_entity {
            host.entity(self.st.entity)
        } else {
            EntityInfo::default()
        };
        let lis = if self.st.play_3d {
            host.listener()
        } else {
            Listener::default()
        };
        let dist = if self.st.play_3d {
            self.listener_distance(&ent, &lis)
        } else {
            0.0
        };
        for i in 0..self.param_value.len() {
            let Some(p) = ev.params.get(i).cloned() else {
                continue;
            };
            let Some(value) = self.param_value.get(i).copied() else {
                continue;
            };
            let new = if p.builtin == 1 {
                math::clamp(dist, p.min, p.max)
            } else if p.builtin != 0 {
                p.min
            } else if p.velocity != 0.0 && !self.st.stopped {
                let range = p.max - p.min;
                let mut x = fma(p.velocity * range, dt, value);
                let sustain = fma(p.sustain, range, p.min);
                if p.sustain >= 0.0 && value <= sustain && x > sustain {
                    x = sustain;
                }
                if x > p.max {
                    if p.loop_mode == 0 && range > 0.0 {
                        x = p.min + fmodf(x - p.min, range);
                    } else {
                        x = p.max;
                        if p.loop_mode == 2 {
                            self.stop(host, ev);
                        }
                    }
                }
                x
            } else {
                value
            };
            if let Some(s) = self.param_value.get_mut(i) {
                *s = new;
            }
        }

        let mut base =
            ev.volume * self.st.rand_gain * self.st.volume * self.st.volume_mul * self.st.fade;
        if self.st.has_entity {
            base *= ent.volume;
        }
        if self.st.play_3d {
            base *= self.distance_gain(ev, &ent, dist);
        }
        let event_speed = powf(2.0, ev.pitch + self.st.rand_pitch) * self.st.speed_mul;

        let mut i = 0;
        while let Some(v) = self.voices.get(i) {
            if host.valid(v.entry, v.entry_id) {
                i += 1;
            } else {
                self.voices.remove(i);
            }
        }

        let nl = ev.layers.len();
        let mut pending = false;
        let mut gains: Vec<Vec<f32>> = vec![Vec::new(); nl];
        let mut filter = vec![[1.0f32; 2]; nl];
        let mut speed = vec![1.0f32; nl];
        let mut pan = vec![1.0f32; nl];
        let mut send = vec![ev.reverb_gain; nl];
        for l in 0..nl {
            let Some(layer) = ev.layers.get(l).cloned() else {
                continue;
            };
            let x = if layer.param >= 0 {
                usize::try_from(layer.param).map_or(0.0, |p| self.param_norm(ev, p))
            } else {
                0.0
            };
            let mut layer_gain = base;
            for env in &layer.envelopes {
                let y = env.eval(if env.param >= 0 {
                    usize::try_from(env.param).map_or(0.0, |p| self.param_norm(ev, p))
                } else {
                    x
                });
                let cutoff = 10.0 * powf(2200.0, y);
                match env.dsp {
                    DSP_VOLUME => layer_gain *= y,
                    DSP_EQ_GAIN => {
                        if let Some(f) = filter.get_mut(l) {
                            f[0] *= std_min(fma(2.95, y, 0.05), 1.0);
                        }
                    }
                    DSP_LOWPASS => {
                        if let Some(f) = filter.get_mut(l) {
                            f[0] /= (1.0 + powf(8000.0 / cutoff, 4.0)).sqrt();
                        }
                    }
                    DSP_HIGHPASS => {
                        if let Some(f) = filter.get_mut(l) {
                            f[1] /= (1.0 + powf(cutoff / 100.0, 4.0)).sqrt();
                        }
                    }
                    DSP_PITCH => {
                        if let Some(s) = speed.get_mut(l) {
                            *s *= powf(2.0, (y - 0.5) * 8.0);
                        }
                    }
                    DSP_PAN_LEVEL => {
                        if let Some(s) = pan.get_mut(l) {
                            *s *= y;
                        }
                    }
                    DSP_REVERB_LEVEL => {
                        if let Some(s) = send.get_mut(l) {
                            *s *= y;
                        }
                    }
                    _ => {}
                }
            }
            let mut layer_gains = vec![0.0f32; layer.sounds.len()];
            for (s, snd) in layer.sounds.iter().enumerate() {
                let Some(def) = usize::try_from(snd.def).ok().and_then(|d| ev.defs.get(d)) else {
                    continue;
                };
                let (def_volume, delay_min, delay_max, spawn_max, spawn_min, spawn_count) = (
                    def.volume,
                    def.delay_min,
                    def.delay_max,
                    def.spawn_max,
                    def.spawn_min,
                    def.spawn_count,
                );
                let gain = layer_gain * snd.volume * def_volume;
                if let Some(g) = layer_gains.get_mut(s) {
                    *g = gain;
                }
                let in_range = (!self.st.stopped || self.st.stop_at_fade_end)
                    && (layer.param < 0 || (x >= snd.x0 && (x < snd.x1 || snd.x1 >= 0.999)));
                let Some(slot) = self.slots.get_mut(l).and_then(|r| r.get_mut(s)) else {
                    continue;
                };
                if in_range && !slot.in_range {
                    slot.triggered = false;
                    slot.delay = rand_range(delay_min, delay_max);
                    slot.spawn_timer = 0.0;
                }
                slot.in_range = in_range;

                let audible = gain > 1e-4 || !self.st.play_3d;
                let mut voices = 0;
                for v in &mut self.voices {
                    if v.layer == l as i32 && v.sound == s as i32 {
                        voices += 1;
                        if in_range && v.looped && !audible {
                            host.stop(v.entry);
                        } else if !in_range && v.looped {
                            if snd.loop_mode == 2 {
                                host.set_looping(v.entry, false);
                                v.looped = false;
                            } else {
                                host.stop(v.entry);
                            }
                        }
                    }
                }
                if !in_range {
                    continue;
                }
                let Some(slot) = self.slots.get_mut(l).and_then(|r| r.get_mut(s)) else {
                    continue;
                };
                if slot.delay > 0.0 {
                    slot.delay -= dt;
                    pending = true;
                    continue;
                }
                let (triggered, timer) = (slot.triggered, slot.spawn_timer);
                if snd.loop_mode != 1 {
                    if voices == 0 && audible {
                        self.start_voice(host, ev, (l, s), true, (&lis, &ent));
                    }
                    pending = true;
                } else if spawn_max > 0.0 {
                    let timer = timer - dt;
                    if let Some(slot) = self.slots.get_mut(l).and_then(|r| r.get_mut(s)) {
                        slot.spawn_timer = timer;
                    }
                    if timer <= 0.0 && voices < spawn_count && audible {
                        self.start_voice(host, ev, (l, s), false, (&lis, &ent));
                        if let Some(slot) = self.slots.get_mut(l).and_then(|r| r.get_mut(s)) {
                            slot.spawn_timer = rand_range(spawn_min, spawn_max);
                        }
                    }
                    pending = true;
                } else if !triggered {
                    if let Some(slot) = self.slots.get_mut(l).and_then(|r| r.get_mut(s)) {
                        slot.triggered = true;
                    }
                    self.start_voice(host, ev, (l, s), false, (&lis, &ent));
                }
            }
            if let Some(g) = gains.get_mut(l) {
                *g = layer_gains;
            }
        }

        self.st.audibility = 0.0;
        let positions: Vec<[f32; 3]> = if self.st.play_3d {
            (0..nl)
                .map(|l| self.pan_pos(ev, &ent, &lis, pan.get(l).copied().unwrap_or(1.0)))
                .collect()
        } else {
            vec![[0.0; 3]; nl]
        };
        let mut i = 0;
        while i < self.voices.len() {
            let Some(v) = self.voices.get_mut(i) else {
                break;
            };
            if !host.valid(v.entry, v.entry_id) {
                self.voices.remove(i);
                continue;
            }
            let (l, s) = (v.layer as usize, v.sound as usize);
            let gain = gains.get(l).and_then(|g| g.get(s)).copied().unwrap_or(0.0) * v.gain;
            host.set_default_volume(v.entry, gain);
            host.set_default_speed(
                v.entry,
                v.speed * event_speed * speed.get(l).copied().unwrap_or(1.0),
            );
            let f = filter.get(l).copied().unwrap_or([1.0, 1.0]);
            if (f[0] - v.gain_hf).abs() > 0.01 || (f[1] - v.gain_lf).abs() > 0.01 {
                v.gain_hf = f[0];
                v.gain_lf = f[1];
                host.set_filter(v.entry, Filter::Hf, f[0]);
                host.set_filter(v.entry, Filter::Lf, f[1]);
            }
            let sd = send.get(l).copied().unwrap_or(1.0);
            if (sd - v.send_gain).abs() > 0.01 {
                v.send_gain = sd;
                host.set_filter(v.entry, Filter::Send, sd);
            }
            if self.st.play_3d {
                host.set_position(v.entry, positions.get(l).copied().unwrap_or_default());
            }
            self.st.audibility += gain;
            i += 1;
        }

        if !self.st.stopped
            && (ev.one_shot || ev.params.is_empty())
            && !pending
            && self.voices.is_empty()
        {
            self.st.stopped = true;
        }
    }
}

impl Bank {
    pub fn inst(&self, h: usize) -> Option<&Inst> {
        self.insts.get(h).and_then(|i| i.as_deref())
    }

    pub fn inst_mut(&mut self, h: usize) -> Option<&mut Inst> {
        self.insts.get_mut(h).and_then(|i| i.as_deref_mut())
    }

    /// `cSomaSoundInstance` constructor; returns the handle.
    pub fn create(
        &mut self,
        event: usize,
        entity: usize,
        has_entity: bool,
        requested_type: i32,
    ) -> usize {
        let ev = self.events.list.get(event);
        if self.next_id == 0 {
            self.next_id = 1;
        }
        let id = self.next_id;
        self.next_id = self.next_id.wrapping_add(1);
        let params: Vec<f32> = ev
            .map(|e| e.params.iter().map(|p| p.min).collect())
            .unwrap_or_default();
        let inst = Box::new(Inst {
            st: State {
                event,
                entity,
                id,
                entry_type: category_entry_type(ev.map_or(-1, |e| e.category_type), requested_type),
                pos: [0.0, 0.0, 1.0],
                rand_gain: 1.0,
                rand_pitch: 0.0,
                volume: 1.0,
                volume_mul: 1.0,
                volume_mul_dest: 1.0,
                volume_mul_speed: 0.0,
                speed_mul: 1.0,
                speed_mul_dest: 1.0,
                speed_mul_speed: 0.0,
                fade: 1.0,
                fade_dest: 1.0,
                fade_speed: 0.0,
                start_fails: 0,
                time: 0.0,
                audibility: 0.0,
                stop_at_fade_end: false,
                started: false,
                stopped: false,
                paused: false,
                stop_disabled: false,
                play_3d: has_entity && ev.is_some_and(|e| e.is_3d),
                has_entity,
            },
            voices: Vec::new(),
            slots: Vec::new(),
            param_target: params.clone(),
            param_value: params,
        });
        let h = match self.insts.iter().position(Option::is_none) {
            Some(i) => {
                if let Some(s) = self.insts.get_mut(i) {
                    *s = Some(inst);
                }
                i
            }
            None => {
                self.insts.push(Some(inst));
                self.insts.len() - 1
            }
        };
        self.order.push(h);
        h
    }

    /// The destructor: stops the voices and forgets the instance.
    pub fn destroy(&mut self, host: &mut dyn Host, h: usize) {
        if let Some(inst) = self.insts.get_mut(h).and_then(Option::take) {
            for v in &inst.voices {
                if host.valid(v.entry, v.entry_id) {
                    host.stop(v.entry);
                }
            }
        }
        self.order.retain(|&o| o != h);
    }

    /// Runs `f` on instance `h` and its event, both moved out of the bank so `f` can reach the
    /// other instances.
    fn with<R>(
        &mut self,
        h: usize,
        f: impl FnOnce(&mut Bank, &mut Inst, &mut Event) -> R,
    ) -> Option<R> {
        let mut inst = self.insts.get_mut(h).and_then(Option::take)?;
        let ei = inst.st.event;
        let mut ev = self
            .events
            .list
            .get_mut(ei)
            .map(std::mem::take)
            .unwrap_or_default();
        let r = f(self, &mut inst, &mut ev);
        if let Some(slot) = self.events.list.get_mut(ei) {
            *slot = ev;
        }
        if let Some(s) = self.insts.get_mut(h) {
            *s = Some(inst);
        }
        Some(r)
    }

    pub fn play(&mut self, host: &mut dyn Host, h: usize) {
        let Some(inst) = self.inst(h) else { return };
        if inst.st.started && !inst.st.stopped {
            return;
        }
        self.stop_now(host, h);
        self.with(h, |bank, i, ev| i.start(bank, host, ev));
    }

    pub fn update(&mut self, host: &mut dyn Host, h: usize, dt: f32) {
        self.with(h, |bank, i, ev| i.update(bank, host, ev, dt));
    }

    pub fn stop_now(&mut self, host: &mut dyn Host, h: usize) {
        if let Some(i) = self.inst_mut(h) {
            i.stop_now(host);
        }
    }

    pub fn stop(&mut self, host: &mut dyn Host, h: usize) {
        self.with(h, |_, i, ev| i.stop(host, ev));
    }

    pub fn fade_out(&mut self, host: &mut dyn Host, h: usize, speed: f32) {
        let Some(i) = self.inst_mut(h) else { return };
        if i.st.stopped || i.st.stop_disabled {
            return;
        }
        if speed <= 0.0 {
            i.stop_now(host);
            return;
        }
        i.st.stopped = true;
        i.st.stop_at_fade_end = true;
        i.st.fade_dest = 0.0;
        i.st.fade_speed = -speed;
    }

    pub fn fade_in_to(&mut self, host: &mut dyn Host, h: usize, volume_mul: f32, speed: f32) {
        if self.inst(h).is_some_and(|i| !i.st.started || i.st.stopped) {
            self.play(host, h);
        }
        let Some(i) = self.inst_mut(h) else { return };
        i.st.stop_at_fade_end = false;
        if speed <= 0.0 {
            i.st.fade = volume_mul;
            i.st.fade_dest = volume_mul;
            i.st.fade_speed = 0.0;
            return;
        }
        i.st.fade = 0.0;
        i.st.fade_dest = volume_mul;
        i.st.fade_speed = speed;
    }

    pub fn fade_volume_mul_to(&mut self, h: usize, dest: f32, speed: f32) {
        let Some(i) = self.inst_mut(h) else { return };
        if speed <= 0.0 {
            i.st.volume_mul = dest;
            i.st.volume_mul_dest = dest;
            return;
        }
        i.st.volume_mul_dest = dest;
        i.st.volume_mul_speed = speed.abs();
    }

    pub fn fade_speed_mul_to(&mut self, h: usize, dest: f32, speed: f32) {
        let Some(i) = self.inst_mut(h) else { return };
        if speed <= 0.0 {
            i.st.speed_mul = dest;
            i.st.speed_mul_dest = dest;
            return;
        }
        i.st.speed_mul_dest = dest;
        i.st.speed_mul_speed = speed.abs();
    }

    pub fn param_index(&self, h: usize, name: &[u8]) -> i32 {
        let Some(ev) = self.inst(h).and_then(|i| self.events.list.get(i.st.event)) else {
            return -1;
        };
        let want = to_lower(name);
        ev.params
            .iter()
            .position(|p| to_lower(&p.name) == want)
            .map_or(-1, |i| i as i32)
    }

    pub fn set_param(&mut self, h: usize, idx: i32, value: f32) {
        let Some(i) = self.insts.get_mut(h).and_then(|i| i.as_deref_mut()) else {
            return;
        };
        let Some(ev) = self.events.list.get(i.st.event) else {
            return;
        };
        let Some(k) = usize::try_from(idx)
            .ok()
            .filter(|&k| k < i.param_value.len())
        else {
            return;
        };
        let Some(p) = ev.params.get(k) else { return };
        let v = math::clamp(value, p.min, p.max);
        if let Some(s) = i.param_value.get_mut(k) {
            *s = v;
        }
        if let Some(s) = i.param_target.get_mut(k) {
            *s = v;
        }
    }

    pub fn set_paused(&mut self, host: &mut dyn Host, h: usize, on: bool) {
        let Some(i) = self.inst_mut(h) else { return };
        i.st.paused = on;
        for v in &i.voices {
            if host.valid(v.entry, v.entry_id) {
                host.pause(v.entry, on);
            }
        }
    }

    pub fn total_time(&self, host: &mut dyn Host, h: usize) -> f32 {
        let mut total = 0.0f32;
        for v in self
            .inst(h)
            .map(|i| i.voices.as_slice())
            .unwrap_or_default()
        {
            if host.valid(v.entry, v.entry_id) {
                total = std_max(total, host.total_time(v.entry) as f32);
            }
        }
        total
    }
}
