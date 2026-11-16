//! `SomaSound.cpp` music: prioritised game music slots, dynamic tracks and their save text.

use super::dump;
use super::host::ops;
use crate::somascript::s2_as::{Frame, Soma, str_bytes};
use std::ffi::{c_char, c_int, c_void};

const MAX_PRIO: usize = 10;

unsafe extern "C" {
    fn snprintf(buf: *mut c_char, n: usize, fmt: *const c_char, ...) -> c_int;
}

/// `ostream::precision(9)` on a float.
fn g9(v: f32) -> String {
    let mut buf = [0u8; 40];
    // SAFETY: the buffer holds 40 bytes and the format takes one double.
    let n = unsafe {
        snprintf(
            buf.as_mut_ptr().cast(),
            buf.len(),
            c"%.9g".as_ptr(),
            f64::from(v),
        )
    };
    let n = usize::try_from(n).unwrap_or(0).min(buf.len() - 1);
    String::from_utf8_lossy(buf.get(..n).unwrap_or_default()).into_owned()
}

fn bits(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

#[derive(Clone, Default)]
struct Slot {
    file: Vec<u8>,
    volume: f32,
    looped: bool,
    resume: bool,
}

type Id = (u8, i32, i32);

#[derive(Clone)]
struct Track {
    id: Id,
    track_prio: i32,
    music_prio: i32,
    file: Vec<u8>,
    volume: f32,
    fade_in: f32,
    fade_out: f32,
}

impl Default for Track {
    fn default() -> Self {
        Self {
            id: (0, 0, 0),
            track_prio: 0,
            music_prio: -1,
            file: Vec::new(),
            volume: 0.0,
            fade_in: 0.0,
            fade_out: 0.0,
        }
    }
}

#[derive(Default)]
pub struct Music {
    slots: [Slot; MAX_PRIO + 1],
    current: i32,
    tracks: Vec<Track>,
    playing: Track,
}

fn slot_index(prio: i32) -> usize {
    prio.clamp(0, MAX_PRIO as i32) as usize
}

fn fade_speed(volume: f32, fade: f32) -> f32 {
    if fade > 0.0 { volume / fade } else { 100.0 }
}

impl Music {
    pub fn new() -> Self {
        Self {
            current: -1,
            ..Self::default()
        }
    }

    fn engine_play(file: &[u8], volume: f32, speed: f32, looped: bool, resume: bool) {
        if let Some(h) = ops() {
            // SAFETY: `file` is valid for the call.
            unsafe { (h.music_play)(file.as_ptr(), file.len(), volume, speed, looped, resume) };
        }
    }

    fn play_highest(&mut self) {
        for i in (0..=MAX_PRIO).rev() {
            let Some(m) = self.slots.get_mut(i) else {
                continue;
            };
            if m.file.is_empty() {
                continue;
            }
            if !m.looped {
                m.file.clear();
                continue;
            }
            Self::engine_play(&m.file, m.volume, 0.3, true, m.resume);
            self.current = i as i32;
            return;
        }
    }

    pub fn play(
        &mut self,
        file: &[u8],
        looped: bool,
        volume: f32,
        fade: f32,
        prio: i32,
        resume: bool,
    ) {
        let prio = prio.clamp(0, MAX_PRIO as i32);
        let line = format!(
            "{} {} {} {prio} {}{} cur={}",
            String::from_utf8_lossy(file),
            bits(volume),
            bits(fade),
            u8::from(looped),
            u8::from(resume),
            self.current
        );
        dump::text(b"MP", line.as_bytes());
        let cur = self.current;
        let Some(m) = self.slots.get_mut(prio as usize) else {
            return;
        };
        if m.file == file {
            return;
        }
        if cur <= prio {
            Self::engine_play(file, volume, fade_speed(volume, fade), looped, resume);
            self.current = prio;
        }
        *m = Slot {
            file: file.to_vec(),
            volume,
            looped,
            resume,
        };
    }

    pub fn stop(&mut self, fade: f32, prio: i32) {
        let prio = prio.clamp(0, MAX_PRIO as i32);
        dump::text(
            b"MS",
            format!("{} {prio} cur={}", bits(fade), self.current).as_bytes(),
        );
        let Some(m) = self.slots.get_mut(prio as usize) else {
            return;
        };
        if m.file.is_empty() {
            return;
        }
        m.file.clear();
        if prio != self.current {
            return;
        }
        let speed = fade_speed(m.volume, fade);
        if let Some(h) = ops() {
            // SAFETY: plain call.
            unsafe { (h.music_stop)(speed) };
        }
        self.current = -1;
        self.play_highest();
    }

    fn update_dynamic(&mut self) {
        let Some(h) = ops() else { return };
        self.tracks.retain(|t| {
            // SAFETY: plain call.
            t.id == (0, 0, 0) || unsafe { (h.entity_active)(t.id.0, t.id.1, t.id.2) }
        });
        let mut best: Option<&Track> = None;
        for t in &self.tracks {
            if best.is_none_or(|b| t.track_prio >= b.track_prio) {
                best = Some(t);
            }
        }
        if let Some(b) = best.cloned() {
            let cur = &self.playing;
            if b.id == cur.id && b.file == cur.file && b.volume == cur.volume {
                return;
            }
            if cur.music_prio >= 0 && cur.music_prio != b.music_prio {
                let prio = cur.music_prio;
                self.stop(b.fade_in, prio);
            }
            self.play(&b.file, true, b.volume, b.fade_in, b.music_prio, true);
            self.playing = b;
        } else if !self.playing.file.is_empty() {
            let (fade, prio) = (self.playing.fade_out, self.playing.music_prio);
            self.stop(fade, prio);
            self.playing = Track::default();
        }
    }

    pub fn update(&mut self) {
        self.update_dynamic();
        // SAFETY: plain call.
        if self.current < 0 || ops().is_some_and(|h| unsafe { (h.music_has_song)() }) {
            return;
        }
        if let Some(m) = self.slots.get_mut(slot_index(self.current)) {
            m.file.clear();
        }
        self.current = -1;
        self.play_highest();
    }

    pub fn serialize(&self) -> Vec<u8> {
        let mut o = String::new();
        for (i, m) in self
            .slots
            .iter()
            .enumerate()
            .filter(|(_, m)| !m.file.is_empty())
        {
            o += &format!(
                "M {i} {} {} {} {}\n",
                g9(m.volume),
                u8::from(m.looped),
                u8::from(m.resume),
                String::from_utf8_lossy(&m.file)
            );
        }
        for t in &self.tracks {
            o += &format!(
                "D {} {} {} {} {} {} {} {} {}\n",
                t.id.0,
                t.id.1,
                t.id.2,
                t.track_prio,
                t.music_prio,
                g9(t.volume),
                g9(t.fade_in),
                g9(t.fade_out),
                String::from_utf8_lossy(&t.file)
            );
        }
        o.into_bytes()
    }

    pub fn deserialize(&mut self, data: &[u8]) {
        self.slots = Default::default();
        self.tracks.clear();
        self.playing = Track::default();
        for line in data.split(|&b| b == b'\n') {
            let mut it = line.splitn(2, |&b| b == b' ');
            let (tag, rest) = (it.next().unwrap_or_default(), it.next().unwrap_or_default());
            match tag {
                b"M" => self.read_slot(rest),
                b"D" => self.read_track(rest),
                _ => {}
            }
        }
        if let Some(h) = ops() {
            // SAFETY: plain call.
            unsafe { (h.music_stop)(100.0) };
        }
        self.current = -1;
        self.play_highest();
    }

    fn read_slot(&mut self, rest: &[u8]) {
        let mut f = Fields(rest);
        let (Some(prio), Some(volume), Some(looped), Some(resume)) =
            (f.int(), f.float(), f.int(), f.int())
        else {
            return;
        };
        let file = f.rest().to_vec();
        if let Some(m) = usize::try_from(prio)
            .ok()
            .and_then(|p| self.slots.get_mut(p))
        {
            *m = Slot {
                file,
                volume,
                looped: looped != 0,
                resume: resume != 0,
            };
        }
    }

    fn read_track(&mut self, rest: &[u8]) {
        let mut f = Fields(rest);
        let mut t = Track::default();
        let (Some(a), Some(b), Some(c), Some(tp), Some(mp)) =
            (f.int(), f.int(), f.int(), f.int(), f.int())
        else {
            return;
        };
        t.id = (a as u8, b, c);
        t.track_prio = tp;
        t.music_prio = mp;
        t.volume = f.float().unwrap_or(0.0);
        t.fade_in = f.float().unwrap_or(0.0);
        t.fade_out = f.float().unwrap_or(0.0);
        t.file = f.rest().to_vec();
        self.tracks.push(t);
    }
}

struct Fields<'a>(&'a [u8]);

impl<'a> Fields<'a> {
    fn word(&mut self) -> Option<&'a str> {
        let s = self.0.trim_ascii_start();
        let n = s
            .iter()
            .position(u8::is_ascii_whitespace)
            .unwrap_or(s.len());
        let (w, rest) = s.split_at(n);
        self.0 = rest;
        (!w.is_empty())
            .then(|| std::str::from_utf8(w).ok())
            .flatten()
    }

    fn int(&mut self) -> Option<i32> {
        self.word()?.parse().ok()
    }

    fn float(&mut self) -> Option<f32> {
        self.word()?.parse().ok()
    }

    fn rest(&mut self) -> &'a [u8] {
        self.0.trim_ascii_start()
    }
}

static TAG: u8 = 0;

fn bytes<'a>(p: *mut c_void) -> &'a [u8] {
    // SAFETY: the declarations below pass `const tString &in`.
    unsafe { str_bytes(p) }
}

pub fn with<R>(f: impl FnOnce(&mut Music) -> R) -> R {
    f(&mut super::game::game().music)
}

pub(super) fn register(s: &Soma) {
    s.func("cLuxMusicHandler@ cLux_GetMusicHandler()", |fr: &Frame| {
        fr.ret_ref(std::ptr::from_ref(&TAG).cast_mut());
    });
    let t = "cLuxMusicHandler";
    s.method(
        t,
        "void Play(const tString &in asFile, bool abLoop,float afVolume, float afFreq, float afVolumeFadeTime, float afFreqFadeTime, int alPrio, bool abResume, bool abSpecialEffect)",
        |fr| {
            let (file, looped) = (bytes(fr.ptr_arg(0)).to_vec(), fr.arg::<bool>(1));
            let (vol, fade) = (fr.arg::<f32>(2), fr.arg::<f32>(4));
            let (prio, resume) = (fr.arg::<i32>(6), fr.arg::<bool>(7));
            with(|m| m.play(&file, looped, vol, fade, prio, resume));
        },
    );
    s.method(t, "void Stop(float afFadeTime, int alPrio)", |fr| {
        let (fade, prio) = (fr.arg::<f32>(0), fr.arg::<i32>(1));
        with(|m| m.stop(fade, prio));
    });
    s.method(
        t,
        "void AddDynamicTrack(tID a_idEntity, int alTrackPrio, int alMusicPrio, const tString&in asFile, float afVolume, float afFadeInTime, float afFadeOutTime)",
        |fr| {
            let id = read_id(fr.obj_arg(0));
            let t = Track {
                id,
                track_prio: fr.arg::<i32>(1),
                music_prio: fr.arg::<i32>(2),
                file: bytes(fr.ptr_arg(3)).to_vec(),
                volume: fr.arg::<f32>(4),
                fade_in: fr.arg::<f32>(5),
                fade_out: fr.arg::<f32>(6),
            };
            with(|m| match m.tracks.iter_mut().find(|x| x.id == id) {
                Some(x) => *x = t,
                None => m.tracks.push(t),
            });
        },
    );
    s.method(t, "void RemoveDynamicTrack(tID a_idEntity)", |fr| {
        let id = read_id(fr.obj_arg(0));
        with(|m| {
            if let Some(i) = m.tracks.iter().position(|x| x.id == id) {
                m.tracks.swap_remove(i);
            }
        });
    });
}

pub(super) fn read_id(p: *mut c_void) -> Id {
    if p.is_null() {
        return (0, 0, 0);
    }
    // SAFETY: a `tID` is 12 bytes: u8, pad, i32, i32.
    let b = unsafe { std::slice::from_raw_parts(p.cast::<u8>(), 12) };
    let w =
        |o: usize| i32::from_ne_bytes(std::array::from_fn(|k| b.get(o + k).copied().unwrap_or(0)));
    (b.first().copied().unwrap_or(0), w(4), w(8))
}

#[cfg(test)]
#[path = "music_tests.rs"]
mod tests;
