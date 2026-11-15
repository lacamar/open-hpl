//! `cLuxEffectHandler` and its ten effects (`LuxEffectHandler.cpp`): fade, flash, sanity gain
//! flash, voices, image trail, screen shake, sepia, radial blur, emotion flash and commentary. The
//! handler lives in an arena; host calls never run under a borrow because scripts re-enter.

use super::effects::{self, Amount, Emotion, Fade, Flash, Shake, k as ek};
use super::host::{Fx, POST_ACTIVE, POST_PARAMS, POST_RADIAL, POST_RESET, POST_SEPIA, POST_TRAIL};
use super::ops::Ops;
use super::voice::{self, ACTION_FINALIZE, ACTION_RETURN, AddIn, TextsIn, UpdateIn, k as vk};
use crate::obj::{Arena, Handle};
use std::cell::RefCell;
use std::collections::VecDeque;

#[derive(Clone, Debug, Default, PartialEq)]
pub struct VoiceData {
    pub texts: [Vec<u32>; 7],
    pub delays: [f32; 7],
    pub voice_file: Vec<u8>,
    pub effect_file: Vec<u8>,
    pub use_pos: bool,
    pub pos: [f32; 3],
    pub min: f32,
    pub max: f32,
    pub priority: i32,
    pub current_time: f64,
    pub interrupted_at: f64,
}

pub struct Voice {
    pub active: bool,
    pub paused: bool,
    pub over_cb: Vec<u8>,
    pub list: Vec<VoiceData>,
    pub voice_entry: usize,
    pub voice_id: i32,
    pub effect_entry: usize,
    pub effect_id: i32,
    pub rows: Vec<Vec<u32>>,
    pub queue: VecDeque<(Vec<u32>, f32)>,
    pub vol_mul: f32,
    pub font_size: [f32; 2],
    pub row_width: f32,
}

pub struct Handler {
    pub fade: Fade,
    pub fade_gfx: usize,
    pub flash: Flash,
    pub flash_gfx: usize,
    pub sanity: Flash,
    pub sanity_gfx: usize,
    pub sanity_color: [f32; 4],
    pub sanity_sound: Vec<u8>,
    pub sanity_in: f32,
    pub sanity_out: f32,
    pub voice: Voice,
    pub trail: Amount,
    pub shake_active: bool,
    pub shakes: Vec<Shake>,
    pub sepia: Amount,
    pub radial: Amount,
    pub radial_start: f32,
    pub emotion: Emotion,
    pub emotion_gfx: usize,
    pub emotion_font: usize,
    pub emotion_rows: Vec<Vec<u32>>,
    pub commentary_active: bool,
    pub commentary_entry: usize,
    pub commentary_id: i32,
    pub commentary_icon: i32,
    pub paused: bool,
}

const FONT_SIZE: [f32; 2] = [20.0, 20.0];
const SECTION_PLAYER: &[u8] = b"Player_General";

fn hud_pos(fx: &dyn Fx) -> ([f32; 3], [f32; 2]) {
    let h = fx.hud();
    (
        [h.start[0] + 0.0, h.start[1] + 0.0, h.start[2] + 3.2f32],
        h.size,
    )
}

fn push_script_name(name: &[u8]) -> Vec<u8> {
    let mut v = name.to_vec();
    v.extend_from_slice(b"()");
    v
}

impl Handler {
    /// Creation order matches the C++ constructors (gfx, configs, resets) so host calls line up.
    pub fn new(fx: &dyn Fx) -> Self {
        let fade_gfx = fx.gfx_rect(false);
        let flash_gfx = fx.gfx_rect(true);
        let sanity_gfx = fx.gfx_rect(true);
        let sanity_color = fx.cfg_color(SECTION_PLAYER, b"SanityGain_Color");
        let sanity_sound = fx.cfg_str(SECTION_PLAYER, b"SanityGain_Sound", b"");
        let sanity_in = fx.cfg_f32(SECTION_PLAYER, b"SanityGain_FadeInTime", 0.0);
        let sanity_out = fx.cfg_f32(SECTION_PLAYER, b"SanityGain_FadeOutTime", 0.0);
        let font_size = fx.cfg_vec2(b"Effects", b"VoiceTextFontSize", 1.0);
        let row_width = fx.cfg_f32(b"Effects", b"VoiceTextRowWidth", 1.0);
        let emotion_gfx = fx.gfx_rect(true);
        let mut h = Self {
            fade: Fade::default(),
            fade_gfx,
            flash: Flash::default(),
            flash_gfx,
            sanity: Flash::default(),
            sanity_gfx,
            sanity_color,
            sanity_sound,
            sanity_in,
            sanity_out,
            voice: Voice {
                active: false,
                paused: false,
                over_cb: Vec::new(),
                list: Vec::new(),
                voice_entry: 0,
                voice_id: -1,
                effect_entry: 0,
                effect_id: -1,
                rows: Vec::new(),
                queue: VecDeque::new(),
                vol_mul: 1.0,
                font_size,
                row_width,
            },
            trail: Amount::default(),
            shake_active: false,
            shakes: Vec::new(),
            sepia: Amount::default(),
            radial: Amount::default(),
            radial_start: 0.0,
            emotion: Emotion::default(),
            emotion_gfx,
            emotion_font: 0,
            emotion_rows: Vec::new(),
            commentary_active: false,
            commentary_entry: 0,
            commentary_id: -1,
            commentary_icon: -1,
            paused: false,
        };
        effects::fade_reset(&mut h.fade);
        effects::flash_reset(&mut h.flash);
        h.sanity.alpha = 0.0;
        // constructor of the commentary effect resets it
        fx.snd_commentary(super::host::COMMENTARY_RESET);
        h
    }
}

pub struct Ctx<'a> {
    pub arena: &'a RefCell<Arena<Handler>>,
    pub h: Handle<Handler>,
    pub fx: &'a dyn Fx,
}

impl Ctx<'_> {
    pub fn with<R>(&self, f: impl FnOnce(&mut Handler) -> R) -> Option<R> {
        let mut a = self.arena.try_borrow_mut().ok()?;
        a.get_mut(self.h).map(f)
    }

    fn get<R: Default>(&self, f: impl FnOnce(&Handler) -> R) -> R {
        self.with(|s| f(s)).unwrap_or_default()
    }

    fn valid(&self, entry: usize, id: i32) -> bool {
        self.fx.snd_valid(entry, id)
    }

    // ------------------------------------------------------------ fade, flash

    pub fn fade_in(&self, t: f32) {
        self.with(|s| effects::fade_in(&mut s.fade, t));
    }

    pub fn fade_out(&self, t: f32) {
        self.with(|s| effects::fade_out(&mut s.fade, t));
    }

    pub fn fade_direct(&self, x: f32) {
        self.with(|s| effects::fade_direct(&mut s.fade, x));
    }

    pub fn is_fading(&self) -> bool {
        self.get(|s| effects::fade_is_fading(&s.fade))
    }

    pub fn flash_start(&self, a: f32, b: f32, c: f32) {
        self.with(|s| effects::flash_start(&mut s.flash, a, b, c));
    }

    pub fn sanity_start(&self) {
        let Some((sound, i, o)) =
            self.with(|s| (s.sanity_sound.clone(), s.sanity_in, s.sanity_out))
        else {
            return;
        };
        if !sound.is_empty() {
            self.fx.gui_sound(&sound);
        }
        self.with(|s| effects::sanity_flash_start(&mut s.sanity, i, o));
    }

    // ------------------------------------------------------------ amount effects

    fn amount_kind(s: &mut Handler, kind: i32) -> &mut Amount {
        match kind {
            POST_SEPIA => &mut s.sepia,
            POST_TRAIL => &mut s.trail,
            _ => &mut s.radial,
        }
    }

    pub fn amount_fade_to(&self, kind: i32, goal: f32, speed: f32) {
        self.with(|s| {
            let a = Self::amount_kind(s, kind);
            a.goal = goal;
            a.speed = speed;
            a.active = 1;
        });
        self.fx.post(kind, POST_ACTIVE, 1.0, 0.0);
    }

    pub fn radial_start_dist(&self, d: f32) {
        let Some(size) = self.with(|s| {
            s.radial_start = d;
            s.radial.amount
        }) else {
            return;
        };
        self.fx.post(POST_RADIAL, POST_PARAMS, size, d);
    }

    fn amount_update(&self, kind: i32, dt: f32) {
        let mut ops = Ops::default();
        let Some(start) = self.with(|s| {
            effects::amount_update(Self::amount_kind(s, kind), dt, &mut ops);
            s.radial_start
        }) else {
            return;
        };
        for op in ops.ops.iter().take(ops.n as usize) {
            if op.kind == ek::AMOUNT_PARAMS {
                let b = if kind == POST_RADIAL { start } else { 0.0 };
                self.fx.post(kind, POST_PARAMS, op.a, b);
            } else {
                self.fx.post(kind, POST_ACTIVE, 0.0, 0.0);
            }
        }
    }

    fn amount_reset(&self, kind: i32) {
        self.with(|s| {
            let a = Self::amount_kind(s, kind);
            a.amount = 0.0;
            a.goal = 0.0;
            if kind == POST_RADIAL {
                s.radial_start = 0.0;
            }
        });
        self.fx.post(kind, POST_RESET, 0.0, 0.0);
        self.fx.post(kind, POST_ACTIVE, 0.0, 0.0);
    }

    // ------------------------------------------------------------ shake

    pub fn shake_start(&self, amount: f32, time: f32, fade_in: f32, fade_out: f32) {
        self.with(|s| {
            s.shakes.push(Shake {
                max_size: amount,
                size: amount,
                time,
                fade_in_time: fade_in,
                max_fade_in_time: fade_in,
                fade_out_time: fade_out,
                max_fade_out_time: fade_out,
            });
            s.shake_active = true;
        });
    }

    fn shake_update(&self, dt: f32) {
        let mut ops = Ops::default();
        self.with(|s| {
            let (kept, active) = effects::shake_update(&mut s.shakes, dt, &mut ops);
            s.shakes.truncate(kept);
            if !active {
                s.shake_active = false;
            }
        });
        for op in ops.ops.iter().take(ops.n as usize) {
            if op.kind == ek::SHAKE_END {
                self.fx.player_shake(None);
            } else {
                self.fx.player_shake(Some([op.a, op.b, op.c]));
            }
        }
    }

    // ------------------------------------------------------------ emotion flash

    pub fn load_fonts(&self) {
        let f = self.fx.font_load();
        self.with(|s| s.emotion_font = f);
    }

    pub fn clear_fonts(&self) {
        let f = self.get(|s| s.emotion_font);
        if f != 0 {
            self.fx.font_destroy(f);
        }
        self.with(|s| s.emotion_font = 0);
    }

    pub fn emotion_start(&self, cat: &[u8], entry: &[u8], sound: &[u8]) {
        self.with(|s| {
            s.emotion.active = 1;
            s.emotion.step = 0;
            s.emotion.alpha = 0.0;
        });
        self.fx.gui_sound(sound);
        let text = self.fx.translate(cat, entry);
        let font = self.get(|s| s.emotion_font);
        let rows = self.fx.wrap(font, 500.0, FONT_SIZE[1], FONT_SIZE, &text);
        self.with(|s| {
            s.emotion_rows = rows;
            effects::emotion_start(&mut s.emotion, text.len());
        });
        self.set_player_paused(true);
        self.fx.player_fov(0.5, 0.5);
        self.amount_fade_to(POST_RADIAL, 0.15, 3.0);
        self.radial_start_dist(0.6);
        self.fx.map_enemy_reset();
    }

    fn emotion_update(&self, dt: f32) {
        let mut ops = Ops::default();
        self.with(|s| effects::emotion_update(&mut s.emotion, dt, &mut ops));
        for op in ops.ops.iter().take(ops.n as usize) {
            match op.kind {
                ek::EMO_FOV_OUT => self.fx.player_fov(1.0, 0.33),
                ek::EMO_UNPAUSE => self.set_player_paused(false),
                ek::EMO_RADIAL_OUT => self.amount_fade_to(POST_RADIAL, 0.0, 1.0),
                _ => {}
            }
        }
    }

    pub fn emotion_action(&self, pressed: bool) {
        self.with(|s| effects::emotion_action(&mut s.emotion, pressed));
    }

    fn emotion_draw(&self) {
        let Some((alpha, text_alpha, gfx, font, rows)) = self.with(|s| {
            (
                s.emotion.alpha,
                s.emotion.text_alpha,
                s.emotion_gfx,
                s.emotion_font,
                s.emotion_rows.clone(),
            )
        }) else {
            return;
        };
        let (pos, size) = hud_pos(self.fx);
        self.fx
            .draw_gfx(0, gfx, pos, size, [alpha, alpha, alpha, 1.0]);
        if text_alpha > 0.0 {
            for r in voice::emotion_rows(rows.len(), FONT_SIZE[1]) {
                let text = rows.get(r.row as usize).map_or(&[][..], Vec::as_slice);
                self.fx.draw_font(
                    font,
                    [r.x, r.y, r.z],
                    FONT_SIZE,
                    [0.0, 0.0, 0.0, text_alpha],
                    r.align == voice::ALIGN_CENTER,
                    text,
                );
            }
        }
    }

    // ------------------------------------------------------------ player pause

    pub fn set_player_paused(&self, on: bool) {
        self.with(|s| s.paused = on);
        self.fx.player_active(!on);
    }

    // ------------------------------------------------------------ commentary

    pub fn commentary_start(&self, icon: i32, file: &[u8]) {
        let (entry, icon_old) = self.get(|s| (s.commentary_entry, s.commentary_icon));
        if entry != 0 {
            self.commentary_stop();
            self.fx.map_icon(icon_old);
        }
        self.with(|s| s.commentary_icon = icon);
        let (e, id) = self.fx.snd_play_gui(file);
        self.with(|s| s.commentary_entry = e);
        if e != 0 {
            self.with(|s| {
                s.commentary_id = id;
                s.commentary_active = true;
            });
            self.fx.snd_commentary(super::host::COMMENTARY_START);
            self.voice_set_volume(0.1);
        }
    }

    pub fn commentary_stop(&self) {
        let Some((active, entry, id)) =
            self.with(|s| (s.commentary_active, s.commentary_entry, s.commentary_id))
        else {
            return;
        };
        if !active {
            return;
        }
        if self.valid(entry, id) {
            self.fx.snd_fade_out(entry, 1.0);
        }
        self.with(|s| s.commentary_entry = 0);
        self.fx.snd_commentary(super::host::COMMENTARY_STOP);
        self.voice_set_volume(1.0);
        self.with(|s| s.commentary_active = false);
    }

    fn commentary_update(&self) {
        let (entry, id, icon) =
            self.get(|s| (s.commentary_entry, s.commentary_id, s.commentary_icon));
        if self.valid(entry, id) {
            return;
        }
        self.fx.map_icon(icon);
        self.with(|s| s.commentary_entry = 0);
        self.fx.snd_commentary(super::host::COMMENTARY_STOP);
        self.voice_set_volume(1.0);
        self.with(|s| s.commentary_active = false);
    }

    fn commentary_reset(&self) {
        let (entry, id) = self.get(|s| (s.commentary_entry, s.commentary_id));
        if entry != 0 && self.valid(entry, id) {
            self.fx.snd_stop(entry);
        }
        self.fx.snd_commentary(super::host::COMMENTARY_RESET);
        self.with(|s| {
            s.commentary_entry = 0;
            s.commentary_icon = -1;
        });
    }

    // ------------------------------------------------------------ voices

    pub fn voice_set_volume(&self, mul: f32) {
        let Some((ve, vid, ee, eid)) = self.with(|s| {
            s.voice.vol_mul = mul;
            (
                s.voice.voice_entry,
                s.voice.voice_id,
                s.voice.effect_entry,
                s.voice.effect_id,
            )
        }) else {
            return;
        };
        if ve != 0 && self.valid(ve, vid) {
            self.fx.snd_volmul(ve, mul);
        }
        if ee != 0 && self.valid(ee, eid) {
            self.fx.snd_volmul(ee, mul);
        }
    }

    pub fn voice_stop(&self, speed: f32) {
        let (ve, vid, ee, eid) = self.get(|s| {
            (
                s.voice.voice_entry,
                s.voice.voice_id,
                s.voice.effect_entry,
                s.voice.effect_id,
            )
        });
        if self.valid(ve, vid) {
            self.fx.snd_fade_out(ve, speed);
        }
        self.with(|s| s.voice.voice_entry = 0);
        if self.valid(ee, eid) {
            self.fx.snd_fade_out(ee, speed);
        }
        self.with(|s| {
            s.voice.effect_entry = 0;
            s.voice.list.clear();
            s.voice.active = false;
        });
    }

    #[allow(clippy::too_many_arguments)]
    pub fn voice_add(
        &self,
        voice_file: &[u8],
        effect_file: &[u8],
        cat: &[u8],
        entries: [&[u8]; 7],
        delays: [f32; 7],
        use_pos: bool,
        pos: [f32; 3],
        min: f32,
        max: f32,
        priority: i32,
        remove_interrupted: bool,
    ) {
        let mut data = VoiceData {
            voice_file: voice_file.to_vec(),
            effect_file: effect_file.to_vec(),
            use_pos,
            pos,
            min,
            max,
            priority,
            current_time: 0.0,
            interrupted_at: -1.0,
            ..VoiceData::default()
        };
        for (n, entry) in entries.iter().enumerate() {
            if !cat.is_empty() && !entry.is_empty() {
                if let Some(t) = data.texts.get_mut(n) {
                    *t = self.fx.translate(cat, entry);
                }
                if let (Some(d), Some(src)) = (data.delays.get_mut(n), delays.get(n)) {
                    *d = *src;
                }
            }
        }
        let Some((active, front, ve, vid, ee, eid)) = self.with(|s| {
            (
                s.voice.active,
                s.voice.list.first().map(|v| v.priority),
                s.voice.voice_entry,
                s.voice.voice_id,
                s.voice.effect_entry,
                s.voice.effect_id,
            )
        }) else {
            return;
        };
        let add_in = AddIn {
            active: u32::from(active),
            list_empty: u32::from(front.is_none()),
            front_priority: front.unwrap_or(0),
            priority,
            remove_interrupted: u32::from(remove_interrupted),
            voice_valid: u32::from(self.valid(ve, vid)),
            effect_valid: u32::from(self.valid(ee, eid)),
        };
        let mut ops = Ops::default();
        voice::add(&add_in, &mut ops);
        for op in ops.ops.iter().take(ops.n as usize) {
            match op.kind {
                vk::FADE_VOICE => {
                    self.fx.snd_fade_out(ve, 0.5);
                    let t = self.fx.snd_elapsed(ve);
                    self.with(|s| {
                        if let Some(f) = s.voice.list.first_mut() {
                            f.current_time = t;
                        }
                    });
                    data.interrupted_at = self.fx.game_time();
                }
                vk::VOICE_NULL => {
                    self.with(|s| s.voice.voice_entry = 0);
                }
                vk::FADE_EFFECT => self.fx.snd_fade_out(ee, 0.5),
                vk::EFFECT_NULL => {
                    self.with(|s| s.voice.effect_entry = 0);
                }
                vk::POP_FRONT => {
                    self.with(|s| {
                        if !s.voice.list.is_empty() {
                            s.voice.list.remove(0);
                        }
                    });
                }
                vk::PUSH_FRONT => {
                    self.with(|s| s.voice.list.insert(0, data.clone()));
                }
                vk::PUSH_BACK => {
                    self.with(|s| s.voice.list.push(data.clone()));
                }
                _ => {}
            }
        }
        self.with(|s| s.voice.active = true);
    }

    fn voice_set_paused(&self, pause: bool) {
        let Some((active, paused, ve, vid, ee, eid)) = self.with(|s| {
            (
                s.voice.active,
                s.voice.paused,
                s.voice.voice_entry,
                s.voice.voice_id,
                s.voice.effect_entry,
                s.voice.effect_id,
            )
        }) else {
            return;
        };
        if !active || paused == pause {
            return;
        }
        self.with(|s| s.voice.paused = pause);
        if ve != 0 && self.valid(ve, vid) {
            self.fx.snd_pause(ve, pause);
        }
        if ee != 0 && self.valid(ee, eid) {
            self.fx.snd_pause(ee, pause);
        }
    }

    pub fn voice_pause(&self) {
        self.voice_set_paused(true);
    }

    pub fn voice_unpause(&self) {
        self.voice_set_paused(false);
    }

    pub fn voice_reset(&self) {
        let (ve, vid, ee, eid) = self.get(|s| {
            (
                s.voice.voice_entry,
                s.voice.voice_id,
                s.voice.effect_entry,
                s.voice.effect_id,
            )
        });
        if ve != 0 && self.valid(ve, vid) {
            self.fx.snd_stop(ve);
        }
        self.with(|s| s.voice.voice_entry = 0);
        if ee != 0 && self.valid(ee, eid) {
            self.fx.snd_stop(ee);
        }
        self.with(|s| {
            let v = &mut s.voice;
            v.effect_entry = 0;
            v.rows.clear();
            v.queue.clear();
            v.list.clear();
            v.over_cb.clear();
            v.paused = false;
            v.vol_mul = 1.0;
        });
    }

    pub fn voice_done_playing(&self) -> bool {
        self.get(|s| s.voice.list.is_empty() && s.voice.voice_entry == 0)
    }

    fn voice_wrap(&self, text: &[u32]) -> Vec<Vec<u32>> {
        let (w, fs) = self.get(|s| (s.voice.row_width, s.voice.font_size));
        self.fx.wrap(0, w, fs[1] + 2.0f32, fs, text)
    }

    fn voice_update(&self, dt: f32) {
        // subtitle queue
        let due = self
            .with(|s| {
                let mut delays: Vec<f32> = s.voice.queue.iter().map(|q| q.1).collect();
                let due = voice::text_tick(&mut delays, dt);
                for (q, d) in s.voice.queue.iter_mut().zip(delays) {
                    q.1 = d;
                }
                due
            })
            .unwrap_or(0);
        for _ in 0..due {
            let text = self.get(|s| {
                s.voice
                    .queue
                    .front()
                    .map(|q| q.0.clone())
                    .unwrap_or_default()
            });
            let rows = self.voice_wrap(&text);
            self.with(|s| {
                s.voice.rows = rows;
                s.voice.queue.pop_front();
            });
        }

        let Some((vol, ve, vid, ee, eid, empty)) = self.with(|s| {
            (
                s.voice.vol_mul,
                s.voice.voice_entry,
                s.voice.voice_id,
                s.voice.effect_entry,
                s.voice.effect_id,
                s.voice.list.is_empty(),
            )
        }) else {
            return;
        };
        let upd = UpdateIn {
            volume_mul: vol,
            voice_nonnull: u32::from(ve != 0),
            voice_valid: u32::from(self.valid(ve, vid)),
            effect_nonnull: u32::from(ee != 0),
            effect_valid: u32::from(self.valid(ee, eid)),
            list_empty: u32::from(empty),
        };
        let mut ops = Ops::default();
        let action = voice::update_a(&upd, &mut ops);
        for op in ops.ops.iter().take(ops.n as usize) {
            if op.kind == vk::VOL_VOICE {
                self.fx.snd_volmul(ve, op.a);
            } else if op.kind == vk::VOL_EFFECT {
                self.fx.snd_volmul(ee, op.a);
            }
        }
        if action == ACTION_RETURN {
            return;
        }
        if action == ACTION_FINALIZE {
            let (cb, pre) = self.get(|s| (s.voice.over_cb.clone(), s.voice.vol_mul));
            self.voice_reset();
            self.with(|s| {
                s.voice.vol_mul = pre;
                s.voice.active = false;
            });
            if !cb.is_empty() {
                self.fx.map_script(&push_script_name(&cb));
            }
            return;
        }

        let Some(v) = self.with(|s| {
            s.voice.list.sort_by_key(|v| std::cmp::Reverse(v.priority));
            s.voice.list.first().cloned()
        }) else {
            return;
        };
        let Some(v) = v else { return };
        let start = voice::start_time(v.current_time, v.interrupted_at, self.fx.game_time());

        let play = |file: &[u8]| {
            let (e, id) = if v.use_pos {
                self.fx.snd_play_3d(file, v.pos, v.min, v.max)
            } else {
                self.fx.snd_play_gui(file)
            };
            if e != 0 && start > 0.0 {
                self.fx.snd_set_elapsed(e, start);
            }
            (e, id)
        };
        let (e, id) = play(&v.voice_file);
        self.with(|s| {
            s.voice.voice_entry = e;
            if e != 0 {
                s.voice.voice_id = id;
            }
        });
        if !v.effect_file.is_empty() {
            let (e, id) = play(&v.effect_file);
            self.with(|s| {
                s.voice.effect_entry = e;
                if e != 0 {
                    s.voice.effect_id = id;
                }
            });
        }

        let (list_len, vn, en) = self.get(|s| {
            (
                s.voice.list.len() as u32,
                s.voice.voice_entry == 0,
                s.voice.effect_entry == 0,
            )
        });
        let mut texts = TextsIn {
            nonempty_mask: 0,
            delays: v.delays,
            list_size: list_len,
            voice_null: u32::from(vn),
            effect_null: u32::from(en),
        };
        for (n, t) in v.texts.iter().enumerate() {
            if !t.is_empty() {
                texts.nonempty_mask |= 1u32 << n;
            }
        }
        let mut ops = Ops::default();
        let over = voice::update_b(&texts, &mut ops);
        for op in ops.ops.iter().take(ops.n as usize) {
            match op.kind {
                vk::CLEAR_TEXT => {
                    self.with(|s| {
                        s.voice.rows.clear();
                        s.voice.queue.clear();
                    });
                }
                vk::WRAP_NOW => {
                    let rows = self.voice_wrap(v.texts.first().map_or(&[][..], Vec::as_slice));
                    self.with(|s| s.voice.rows = rows);
                }
                vk::QUEUE => {
                    let text = v.texts.get(op.i as usize).cloned().unwrap_or_default();
                    self.with(|s| s.voice.queue.push_back((text, op.a)));
                }
                _ => {}
            }
        }
        self.with(|s| {
            if !s.voice.list.is_empty() {
                s.voice.list.remove(0);
            }
        });
        if over {
            self.voice_reset();
            let cb = self.get(|s| s.voice.over_cb.clone());
            self.with(|s| s.voice.active = false);
            if !cb.is_empty() {
                self.fx.map_script(&push_script_name(&cb));
            }
        }
    }

    fn voice_draw(&self) {
        if !self.fx.subtitles() {
            return;
        }
        let Some((rows, fs, w)) =
            self.with(|s| (s.voice.rows.clone(), s.voice.font_size, s.voice.row_width))
        else {
            return;
        };
        if rows.is_empty() {
            return;
        }
        for r in voice::voice_rows(rows.len(), w, fs[1]) {
            let text = rows.get(r.row as usize).map_or(&[][..], Vec::as_slice);
            self.fx
                .draw_font(0, [r.x, r.y, r.z], fs, [1.0, 1.0, 1.0, 1.0], false, text);
        }
    }

    // ------------------------------------------------------------ handler

    fn draw_flash(&self, gfx: usize, alpha: f32) {
        let (pos, size) = hud_pos(self.fx);
        self.fx
            .draw_gfx(0, gfx, pos, size, [alpha, alpha, alpha, 1.0]);
    }

    pub fn sanity_draw(&self, set: usize) {
        let (gfx, a, c) = self.get(|s| (s.sanity_gfx, s.sanity.alpha, s.sanity_color));
        let (pos, size) = hud_pos(self.fx);
        self.fx.draw_gfx(
            set,
            gfx,
            pos,
            size,
            [c[0] * a, c[1] * a, c[2] * a, c[3] * a],
        );
    }

    pub fn sanity_update(&self, dt: f32) {
        self.with(|s| effects::flash_update(&mut s.sanity, dt));
    }

    pub fn update(&self, dt: f32) {
        let active = |f: fn(&Handler) -> bool| self.get(f);
        if active(|s| s.fade.active != 0) {
            self.with(|s| effects::fade_update(&mut s.fade, dt));
        }
        if active(|s| s.flash.active != 0) {
            self.with(|s| effects::flash_update(&mut s.flash, dt));
        }
        if active(|s| s.sanity.active != 0) {
            self.sanity_update(dt);
        }
        if active(|s| s.voice.active) {
            self.voice_update(dt);
        }
        if active(|s| s.trail.active != 0) {
            self.amount_update(POST_TRAIL, dt);
        }
        if active(|s| s.shake_active) {
            self.shake_update(dt);
        }
        if active(|s| s.sepia.active != 0) {
            self.amount_update(POST_SEPIA, dt);
        }
        if active(|s| s.radial.active != 0) {
            self.amount_update(POST_RADIAL, dt);
        }
        if active(|s| s.emotion.active != 0) {
            self.emotion_update(dt);
        }
        if active(|s| s.commentary_active) {
            self.commentary_update();
        }
    }

    pub fn draw(&self) {
        let active = |f: fn(&Handler) -> bool| self.get(f);
        if active(|s| s.fade.active != 0) {
            let alpha = self.get(|s| s.fade.alpha);
            if alpha > 0.0 || alpha.is_nan() {
                let (pos, size) = hud_pos(self.fx);
                let v = 1.0f32 - alpha;
                let gfx = self.get(|s| s.fade_gfx);
                self.fx.draw_gfx(0, gfx, pos, size, [v, v, v, 1.0]);
            }
        }
        if active(|s| s.flash.active != 0) {
            let (gfx, a) = self.get(|s| (s.flash_gfx, s.flash.alpha));
            self.draw_flash(gfx, a);
        }
        if active(|s| s.sanity.active != 0) {
            self.sanity_draw(0);
        }
        if active(|s| s.voice.active) {
            self.voice_draw();
        }
        if active(|s| s.emotion.active != 0) {
            self.emotion_draw();
        }
    }

    pub fn do_action(&self, pressed: bool) {
        if self.get(|s| s.emotion.active != 0) {
            self.emotion_action(pressed);
        }
    }

    /// Reset of every effect in creation order, each then set inactive.
    pub fn reset(&self) {
        self.with(|s| {
            effects::fade_reset(&mut s.fade);
            s.fade.active = 0;
            effects::flash_reset(&mut s.flash);
            s.flash.active = 0;
            s.sanity.alpha = 0.0;
            s.sanity.active = 0;
        });
        self.voice_reset();
        self.with(|s| s.voice.active = false);
        self.amount_reset(POST_TRAIL);
        self.with(|s| s.trail.active = 0);
        self.with(|s| {
            s.shakes.clear();
            s.shake_active = false;
        });
        self.amount_reset(POST_SEPIA);
        self.with(|s| s.sepia.active = 0);
        self.amount_reset(POST_RADIAL);
        self.with(|s| s.radial.active = 0);
        self.with(|s| s.emotion.active = 0);
        self.commentary_reset();
        self.with(|s| s.commentary_active = false);
        self.fx.snd_reset_world();
        self.with(|s| s.paused = false);
    }

    pub fn on_map_leave(&self) {
        self.amount_fade_to(POST_SEPIA, 0.0, 1.0);
        self.amount_fade_to(POST_RADIAL, 0.0, 1.0);
        if self.get(|s| s.commentary_active) {
            self.commentary_stop();
        }
    }

    pub fn state(&self) -> State {
        self.get(|s| State {
            fade: s.fade,
            flash: s.flash,
            sanity: s.sanity,
            trail: s.trail,
            sepia: s.sepia,
            radial: s.radial,
            radial_start: s.radial_start,
            emotion: s.emotion,
            shake_active: u32::from(s.shake_active),
            shake_count: s.shakes.len() as u32,
            voice_active: u32::from(s.voice.active),
            voice_paused: u32::from(s.voice.paused),
            voice_count: s.voice.list.len() as u32,
            voice_rows: s.voice.rows.len() as u32,
            voice_queue: s.voice.queue.len() as u32,
            voice_entries: u32::from(s.voice.voice_entry != 0)
                | (u32::from(s.voice.effect_entry != 0) << 1),
            voice_vol: s.voice.vol_mul,
            commentary_active: u32::from(s.commentary_active),
            commentary_icon: s.commentary_icon,
            player_paused: u32::from(s.paused),
        })
    }

    /// Writes the saved fields back as the C++ save loader did, without any side effects.
    pub fn set_state(&self, st: &State) {
        self.with(|s| {
            s.fade = st.fade;
            s.trail = st.trail;
            s.sepia = st.sepia;
            s.radial = st.radial;
            s.radial_start = st.radial_start;
            s.voice.active = st.voice_active != 0;
        });
    }
}

/// Flat copy of the effect state for the dump hook and the save game.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct State {
    pub fade: Fade,
    pub flash: Flash,
    pub sanity: Flash,
    pub trail: Amount,
    pub sepia: Amount,
    pub radial: Amount,
    pub radial_start: f32,
    pub emotion: Emotion,
    pub shake_active: u32,
    pub shake_count: u32,
    pub voice_active: u32,
    pub voice_paused: u32,
    pub voice_count: u32,
    pub voice_rows: u32,
    pub voice_queue: u32,
    pub voice_entries: u32,
    pub voice_vol: f32,
    pub commentary_active: u32,
    pub commentary_icon: i32,
    pub player_paused: u32,
}
