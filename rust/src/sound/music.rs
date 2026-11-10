//! `cMusicHandler`: one main song crossfading against a queue of fading songs, with a lock and
//! per-file resume positions.
//!
//! Songs are C++ `cMusicEntry` objects (game code reads and writes their volumes), so the handler
//! holds raw pointers to their `Song` header and reads and writes its fields in place.

use super::channel::Channel;
use crate::math::fma;

#[repr(C)]
pub struct Song {
    pub stream: *mut std::ffi::c_void,
    pub max_volume: f32,
    pub volume: f32,
    pub volume_add: f32,
    pub looping: bool,
}

/// The C++ side of the handler: object lifetime, resource loading and the lock record.
pub(super) trait MusicHost {
    type Stream: Channel;
    /// A `cMusicEntry` with its defaults.
    fn new_song(&mut self) -> *mut Song;
    /// Destroys the song's stream and then the song.
    fn delete_song(&mut self, song: *mut Song);
    /// Loads `name` as a looping-or-not stream channel; `None` after logging the failure.
    fn create_stream(&mut self, name: &[u8], looping: bool) -> Option<*mut std::ffi::c_void>;
    fn set_song_name(&mut self, song: *mut Song, name: &[u8]);
    fn set_lock(&mut self, lock: *mut std::ffi::c_void, name: &[u8], volume: f32, looping: bool);
    /// # Safety
    /// `stream` is a live channel of this host.
    unsafe fn stream(&self, stream: *mut std::ffi::c_void) -> Self::Stream;
}

struct Slot {
    song: *mut Song,
    name: Vec<u8>,
}

struct Resume {
    name: Vec<u8>,
    pos: f64,
}

pub struct Music {
    main: Option<Slot>,
    fading: Vec<Slot>,
    lock: *mut std::ffi::c_void,
    paused: bool,
    volume_mul: f32,
    volume_mul_goal: f32,
    volume_mul_speed: f32,
    resume: Vec<Resume>,
}

macro_rules! field {
    ($p:expr, $f:ident) => {
        // SAFETY: slots only hold live songs: they are created by `new_song` and removed
        // before `delete_song`.
        unsafe { (*$p).$f }
    };
}

macro_rules! set_field {
    ($p:expr, $f:ident, $v:expr) => {{
        let value = $v;
        // SAFETY: as `field!`.
        unsafe { (*$p).$f = value }
    }};
}

impl Default for Music {
    fn default() -> Self {
        Self::new()
    }
}

impl Music {
    pub fn new() -> Self {
        Self {
            main: None,
            fading: Vec::new(),
            lock: std::ptr::null_mut(),
            paused: false,
            volume_mul: 1.0,
            volume_mul_goal: 1.0,
            volume_mul_speed: 1.0,
            resume: Vec::new(),
        }
    }

    pub fn main_song(&self) -> *mut Song {
        self.main.as_ref().map_or(std::ptr::null_mut(), |s| s.song)
    }

    pub fn volume_mul(&self) -> f32 {
        self.volume_mul
    }

    pub(super) fn destroy<H: MusicHost>(&mut self, host: &mut H) {
        if let Some(m) = self.main.take() {
            host.delete_song(m.song);
        }
        for s in self.fading.drain(..) {
            host.delete_song(s.song);
        }
        self.resume.clear();
    }

    fn stream<H: MusicHost>(host: &H, song: *mut Song) -> H::Stream {
        let raw = field!(song, stream);
        // SAFETY: a slot's song has a live stream (`load_and_start` sets it before the slot exists).
        unsafe { host.stream(raw) }
    }

    pub(super) fn play<H: MusicHost>(
        &mut self,
        host: &mut H,
        name: &[u8],
        volume: f32,
        fade_step: f32,
        looping: bool,
        resume: bool,
    ) -> bool {
        if !self.lock.is_null() {
            host.set_lock(self.lock, name, volume, looping);
            return true;
        }

        let playing = self.main.as_ref().is_some_and(|m| m.name == name);
        if !playing {
            if let Some(m) = &self.main {
                set_field!(m.song, volume_add, fade_step);
                self.fading.push(Slot {
                    song: m.song,
                    name: m.name.clone(),
                });
            }

            let mut i = 0;
            while let Some(s) = self.fading.get(i) {
                if s.name == name {
                    let song = s.song;
                    set_field!(song, volume, 0.0);
                    Self::stream(host, song).stop();
                    host.delete_song(song);
                    self.fading.remove(i);
                } else {
                    i += 1;
                }
            }

            let song = host.new_song();
            if !self.load_and_start(host, name, song, 0.0, looping, resume) {
                host.delete_song(song);
                self.main = None;
                return false;
            }
            self.main = Some(Slot {
                song,
                name: name.to_vec(),
            });
        } else if self
            .main
            .as_ref()
            .is_some_and(|m| field!(m.song, max_volume) == volume)
        {
            return true;
        }

        let Some(m) = &self.main else {
            return true;
        };
        set_field!(m.song, max_volume, volume);
        set_field!(m.song, looping, looping);
        if field!(m.song, max_volume) > field!(m.song, volume) {
            set_field!(m.song, volume_add, fade_step);
        } else {
            set_field!(m.song, volume_add, -fade_step);
        }
        true
    }

    pub(super) fn stop<H: MusicHost>(&mut self, host: &H, mut fade_step: f32) {
        let Some(m) = self.main.take() else {
            return;
        };
        if fade_step < 0.0 {
            fade_step = -fade_step;
        }
        set_field!(m.song, volume_add, fade_step);
        self.update_resume_entry(host, &m, fade_step);
        if fade_step == 0.0 {
            let mut stream = Self::stream(host, m.song);
            stream.set_volume(0.0);
            stream.stop();
            set_field!(m.song, volume, 0.0);
        }
        self.fading.push(m);
    }

    pub(super) fn set_paused<H: MusicHost>(&mut self, host: &H, paused: bool) {
        if let Some(m) = &self.main {
            Self::stream(host, m.song).set_paused(paused);
        }
        for s in &self.fading {
            Self::stream(host, s.song).set_paused(paused);
        }
        self.paused = paused;
    }

    pub fn lock(&mut self, lock: *mut std::ffi::c_void) {
        self.lock = lock;
    }

    pub fn fade_volume_mul(&mut self, dest: f32, speed: f32) {
        self.volume_mul_goal = dest;
        self.volume_mul_speed = speed;
    }

    pub fn set_volume_mul(&mut self, mul: f32) {
        self.volume_mul = mul;
        self.volume_mul_goal = mul;
    }

    pub fn reset_resume(&mut self) {
        self.resume.clear();
    }

    pub(super) fn update<H: MusicHost>(&mut self, host: &mut H, dt: f32) {
        if self.paused {
            return;
        }

        if self.volume_mul != self.volume_mul_goal {
            if self.volume_mul < self.volume_mul_goal {
                self.volume_mul = fma(dt, self.volume_mul_speed, self.volume_mul);
                if self.volume_mul > self.volume_mul_goal {
                    self.volume_mul = self.volume_mul_goal;
                }
            }
            if self.volume_mul > self.volume_mul_goal {
                self.volume_mul = fma(-self.volume_mul_speed, dt, self.volume_mul);
                if self.volume_mul < self.volume_mul_goal {
                    self.volume_mul = self.volume_mul_goal;
                }
            }
        }

        if let Some(m) = &self.main {
            let song = m.song;
            let mut stream = Self::stream(host, song);
            if !stream.is_playing() {
                host.delete_song(song);
                self.main = None;
            } else {
                set_field!(
                    song,
                    volume,
                    fma(field!(song, volume_add), dt, field!(song, volume))
                );
                if field!(song, volume_add) > 0.0 {
                    if field!(song, volume) >= field!(song, max_volume) {
                        set_field!(song, volume, field!(song, max_volume));
                    }
                } else if field!(song, volume) <= field!(song, max_volume) {
                    set_field!(song, volume, field!(song, max_volume));
                }
                let volume = field!(song, volume) * self.volume_mul;
                if stream.volume() != volume {
                    stream.set_volume(volume);
                }
            }
        }

        let mut i = 0;
        while let Some(s) = self.fading.get(i) {
            let song = s.song;
            set_field!(
                song,
                volume,
                fma(-field!(song, volume_add), dt, field!(song, volume))
            );
            let mut stream = Self::stream(host, song);
            if field!(song, volume) <= 0.0 {
                set_field!(song, volume, 0.0);
                stream.stop();
                host.delete_song(song);
                self.fading.remove(i);
            } else {
                stream.set_volume(field!(song, volume) * self.volume_mul);
                i += 1;
            }
        }
    }

    /// The resume position of `name`, created at 0 when absent.
    fn resume_pos(&mut self, name: &[u8]) -> Option<&mut f64> {
        let i = match self.resume.iter().position(|r| r.name == name) {
            Some(i) => i,
            None => {
                self.resume.push(Resume {
                    name: name.to_vec(),
                    pos: 0.0,
                });
                self.resume.len() - 1
            }
        };
        self.resume.get_mut(i).map(|r| &mut r.pos)
    }

    fn update_resume_entry<H: MusicHost>(&mut self, host: &H, slot: &Slot, fade_step: f32) {
        let stream = Self::stream(host, slot.song);
        let mut time_add = 0.0f32;
        if fade_step > 0.0 {
            time_add = stream.volume() / fade_step;
        }
        let elapsed = stream.elapsed_time();
        if let Some(pos) = self.resume_pos(&slot.name) {
            *pos = elapsed + f64::from(time_add);
        }
    }

    fn load_and_start<H: MusicHost>(
        &mut self,
        host: &mut H,
        name: &[u8],
        song: *mut Song,
        volume: f32,
        looping: bool,
        resume: bool,
    ) -> bool {
        let Some(raw) = host.create_stream(name, looping) else {
            return false;
        };
        host.set_song_name(song, name);
        set_field!(song, stream, raw);
        let mut stream = Self::stream(host, song);
        stream.set_volume(volume);

        if resume {
            let mut pos = self.resume_pos(name).map_or(0.0, |p| *p);
            if pos >= stream.total_time() {
                pos = 0.0;
            }
            stream.set_elapsed_time(pos);
        }

        stream.play();
        true
    }
}
