//! `cLuxMusicHandler`: the owner of the priority slots, the enemy sets and the timers. The engine
//! music handler, the map's enemies and the player stay C++ behind [`Host`].

use super::host12::{Call, Host, code, log};
use super::music::{self, ENEMY_KINDS, Enemy, PlayOut, Slot, k};
use super::ops::Ops;
use std::collections::BTreeSet;

#[derive(Clone, Debug, Default, PartialEq)]
pub struct GameMusic {
    pub file: Vec<u8>,
    pub looped: bool,
    pub resume: bool,
    pub volume: f32,
    pub special: bool,
}

#[derive(Debug, Default)]
pub struct MusicHandler {
    pub max_prio: i32,
    pub volume_mul: f32,
    pub slots: Vec<GameMusic>,
    pub cur_prio: i32,
    pub enemies: [BTreeSet<usize>; ENEMY_KINDS],
    pub enemy: Enemy,
}

const ENEMY_TYPES: i32 = ENEMY_KINDS as i32;

impl MusicHandler {
    pub fn new(h: &dyn Host) -> Self {
        let max_prio = 8 + ENEMY_TYPES;
        let volume_mul = Call::new(h, code::CFG_F32)
            .s(0, b"Sound")
            .s(1, b"InGameMusicVolume")
            .f(0, 1.0)
            .run()
            .out[0];
        MusicHandler {
            max_prio,
            volume_mul,
            slots: vec![GameMusic::default(); (max_prio + 1) as usize],
            ..MusicHandler::default()
        }
    }

    pub fn reset(&mut self, h: &dyn Host) {
        for s in &mut self.slots {
            s.file.clear();
        }
        self.cur_prio = -1;
        self.enemies = Default::default();
        self.enemy = Enemy::default();
        Call::new(h, code::M_RESET_RESUME).run();
    }

    pub fn update(&mut self, h: &dyn Host, dt: f32) {
        let dead = Call::new(h, code::M_PLAYER_DEAD).run().flag();
        log(h, &format!("m dead {}", i32::from(dead)));
        if dead {
            return;
        }
        self.update_danger(h, dt);
        self.update_enemy(h, dt, 0);
        self.update_enemy(h, dt, 1);

        let no_song = self.cur_prio >= 0 && Call::new(h, code::M_SONG_NULL).run().flag();
        if self.cur_prio >= 0 {
            log(h, &format!("m nosong {}", i32::from(no_song)));
        }
        if music::song_over(self.cur_prio, no_song) {
            if let Some(s) = usize::try_from(self.cur_prio)
                .ok()
                .and_then(|i| self.slots.get_mut(i))
            {
                s.file.clear();
            }
            self.cur_prio = -1;
            self.play_highest(h);
        }
    }

    #[allow(clippy::too_many_arguments)]
    pub fn play(
        &mut self,
        h: &dyn Host,
        file: &[u8],
        looped: bool,
        volume: f32,
        fade: f32,
        prio: i32,
        resume: bool,
        special: bool,
    ) {
        let prio = music::prio(prio, self.max_prio);
        let Some(idx) = usize::try_from(prio).ok() else {
            return;
        };
        let Some(same_file) = self.slots.get(idx).map(|s| s.file == file) else {
            return;
        };
        let mut out = PlayOut::default();
        if !music::play(
            self.cur_prio,
            prio,
            same_file,
            volume,
            self.volume_mul,
            fade,
            &mut out,
        ) {
            return;
        }
        if out.play != 0 {
            Call::new(h, code::M_PLAY)
                .s(0, file)
                .f(0, out.volume)
                .f(1, out.fade_speed)
                .i(0, i32::from(looped))
                .i(1, i32::from(resume))
                .run();
        }
        self.cur_prio = out.cur_prio;
        if let Some(s) = self.slots.get_mut(idx) {
            s.special = special;
            s.looped = looped;
            s.resume = resume;
            s.volume = out.volume;
            s.file = file.to_vec();
        }
    }

    pub fn stop(&mut self, h: &dyn Host, fade: f32, prio: i32) {
        let prio = music::prio(prio, self.max_prio);
        let Some(idx) = usize::try_from(prio).ok() else {
            return;
        };
        let Some(slot) = self.slots.get(idx) else {
            return;
        };
        let has_file = !slot.file.is_empty();
        let out = music::stop(
            self.cur_prio,
            prio,
            has_file,
            if has_file { slot.volume } else { 0.0 },
            fade,
        );
        if out.clear != 0
            && let Some(s) = self.slots.get_mut(idx)
        {
            s.file.clear();
        }
        if out.stop != 0 {
            Call::new(h, code::M_STOP).f(0, out.fade_speed).run();
            self.cur_prio = -1;
            self.play_highest(h);
        }
    }

    pub fn enemy_set(&mut self, kind: usize) -> Option<&mut BTreeSet<usize>> {
        self.enemies.get_mut(kind)
    }

    pub fn on_map(&mut self, h: &dyn Host) {
        for i in 0..ENEMY_TYPES {
            if let Some(s) = self.enemy_set(i as usize) {
                s.clear();
            }
            self.stop(h, 2.0, 8 + i);
        }
    }

    fn update_danger(&mut self, h: &dyn Host, dt: f32) {
        let hard = Call::new(h, code::M_HARD_MODE).run().flag();
        let has_map = Call::new(h, code::M_HAS_MAP).run().flag();
        let mode = music::danger_begin(hard, &self.enemy, has_map);
        if mode == music::DANGER_RETURN {
            return;
        }
        let scan = Call::new(h, code::M_DANGER_SCAN).run();
        let found = !scan.sunk.danger.is_empty();
        let nonempty: Vec<u32> = scan
            .sunk
            .danger
            .iter()
            .map(|(_, m)| u32::from(!m.is_empty()))
            .collect();
        let prios: Vec<i32> = scan.sunk.danger.iter().map(|(p, _)| *p).collect();

        let mut ops = Ops::default();
        let mut idx = -1;
        if mode == music::DANGER_SCAN_IDLE {
            idx = music::pick_danger(&nonempty, &prios);
            music::danger_idle(&mut self.enemy, idx >= 0, dt, &mut ops);
        } else {
            music::danger_playing(&mut self.enemy, found, dt, &mut ops);
        }
        for op in ops.ops.iter().take(ops.n as usize) {
            match op.kind {
                k::PLAY_DANGER => {
                    let file = usize::try_from(idx)
                        .ok()
                        .and_then(|i| scan.sunk.danger.get(i))
                        .map(|(_, m)| m.clone())
                        .unwrap_or_default();
                    self.play(h, &file, true, 1.0, 6.0, 5, true, true);
                }
                k::STOP_DANGER => self.stop(h, 6.0, 5),
                _ => {}
            }
        }
    }

    fn update_enemy(&mut self, h: &dyn Host, dt: f32, t: usize) {
        let has_map = Call::new(h, code::M_HAS_MAP).run().flag();
        let set_empty = self.enemies.get(t).is_none_or(BTreeSet::is_empty);
        let mut ops = Ops::default();
        let mode = music::enemy_begin(&mut self.enemy, t, has_map, set_empty, dt, &mut ops);
        for op in ops.ops.iter().take(ops.n as usize) {
            if op.kind == k::STOP_ENEMY {
                self.stop(h, 5.0, op.i);
            }
        }
        if mode != music::ENEMY_SCAN {
            return;
        }
        let members: Vec<usize> = self
            .enemies
            .get(t)
            .map(|s| s.iter().copied().collect())
            .unwrap_or_default();
        let mut nonempty = Vec::new();
        let mut prios = Vec::new();
        let mut musics = Vec::new();
        for e in members {
            let r = Call::new(h, code::M_ENEMY_INFO)
                .p(0, e)
                .i(0, t as i32)
                .run();
            nonempty.push(u32::from(!r.string().is_empty()));
            prios.push(r.out_i[0]);
            musics.push(r.string());
        }
        let idx = music::pick_enemy(&nonempty, &prios);
        let Some(file) = usize::try_from(idx).ok().and_then(|i| musics.get(i)) else {
            return;
        };

        let mut ops = Ops::default();
        music::enemy_found(&mut self.enemy, t, &mut ops);
        for op in ops.ops.iter().take(ops.n as usize) {
            match op.kind {
                k::STOP_CLOSE => self.stop(h, 2.0, 5),
                k::STOP_LOWER => self.stop(h, 2.0, op.i),
                k::PLAY_ENEMY => self.play(h, file, true, 1.0, 1.5, op.i, true, true),
                _ => {}
            }
        }
    }

    fn play_highest(&mut self, h: &dyn Host) {
        let slots: Vec<Slot> = self
            .slots
            .iter()
            .map(|m| {
                let has = !m.file.is_empty();
                Slot {
                    has_file: u32::from(has),
                    looped: u32::from(has && m.looped),
                    resume: u32::from(has && m.resume),
                    volume: if has { m.volume } else { 0.0 },
                }
            })
            .collect();
        let mut ops = Ops::default();
        music::play_highest(&slots, self.max_prio, &mut ops);
        for op in ops.ops.iter().take(ops.n as usize) {
            let Some(idx) = usize::try_from(op.i).ok() else {
                continue;
            };
            if op.kind == k::PLAY {
                if let Some(m) = self.slots.get(idx) {
                    Call::new(h, code::M_PLAY)
                        .s(0, &m.file)
                        .f(0, m.volume)
                        .f(1, 0.3)
                        .i(0, 1)
                        .i(1, i32::from(m.resume))
                        .run();
                }
                self.cur_prio = op.i;
            } else if let Some(m) = self.slots.get_mut(idx) {
                m.file.clear();
            }
        }
    }
}
