//! The playing channel (`OalChan`) and the `Channel` trait the entry and music logic drives.

use crate::backend::media::oal::Oal;
use crate::backend::media::snd;
use crate::types::{Matrixf, Vector3f};
use std::ffi::c_void;

pub(super) trait Channel {
    fn play(&mut self);
    fn stop(&mut self);
    fn set_paused(&mut self, paused: bool);
    fn paused(&self) -> bool;
    fn is_playing(&mut self) -> bool;
    fn stop_used(&self) -> bool;
    fn speed(&self) -> f32;
    fn set_speed(&mut self, speed: f32);
    fn volume(&self) -> f32;
    fn set_volume(&mut self, volume: f32);
    fn looping(&self) -> bool;
    fn set_looping(&mut self, looping: bool);
    fn set_3d(&mut self, is_3d: bool);
    fn set_priority(&mut self, priority: i32);
    fn set_priority_modifier(&mut self, modifier: i32);
    fn set_affected_by_env(&mut self, affected: bool);
    fn position_is_relative(&self) -> bool;
    fn set_position_is_relative(&mut self, relative: bool);
    fn position(&self) -> Vector3f;
    fn set_position(&mut self, pos: Vector3f);
    fn rel_position(&self) -> Vector3f;
    fn set_rel_position(&mut self, pos: Vector3f);
    fn min_distance(&self) -> f32;
    fn set_min_distance(&mut self, dist: f32);
    fn max_distance(&self) -> f32;
    fn set_max_distance(&mut self, dist: f32);
    fn block_volume_mul(&self) -> f32;
    fn elapsed_time(&self) -> f64;
    fn total_time(&self) -> f64;
    fn set_elapsed_time(&mut self, time: f64);
    fn set_blockable(&mut self, _blockable: bool) {}
    fn set_block_volume_mul(&mut self, _mul: f32) {}
    fn set_filter(&mut self, _which: i32, _gain: f32) {}
}

pub(super) trait EntryHost {
    fn global_speed(&self, entry_type: u32) -> f32;
    fn global_volume(&self, entry_type: u32) -> f32;
    fn silent(&self) -> bool;
    fn is_blocked(&mut self, pos: Vector3f) -> bool;
    fn listener_position(&self) -> Vector3f;
    fn listener_matrix(&self) -> Matrixf;
}

/// A live `OalChan`; the playing source and the base-class state of the former `iSoundChannel`.
pub(super) struct CChannel<'a> {
    ch: &'a mut OalChan,
}

impl<'a> CChannel<'a> {
    /// # Safety
    /// `ch` is a live `OalChan` from `OalChan::into_raw` for the lifetime of the value.
    pub(super) unsafe fn new(ch: *mut c_void) -> Self {
        Self {
            // SAFETY: the contract above.
            ch: unsafe { &mut *ch.cast::<OalChan>() },
        }
    }
}

/// One playing OpenAL source (`cOpenALSoundChannel`): the L5 channel plus the C++ `iSoundData`
/// it plays, released through the host when the channel is freed.
pub struct OalChan {
    l5: snd::Channel,
    oal: &'static dyn Oal,
    data: *mut c_void,
    release: unsafe extern "C" fn(*mut c_void),
}

impl OalChan {
    pub(super) fn into_raw(
        oal: &'static dyn Oal,
        handle: i32,
        data: *mut c_void,
        release: unsafe extern "C" fn(*mut c_void),
    ) -> *mut c_void {
        let l5 = snd::Channel::new(oal, handle);
        Box::into_raw(Box::new(Self {
            l5,
            oal,
            data,
            release,
        }))
        .cast()
    }

    /// # Safety
    /// `ch` from `into_raw`, freed once.
    pub(super) unsafe fn free(ch: *mut c_void) {
        // SAFETY: caller contract.
        let c = unsafe { Box::from_raw(ch.cast::<Self>()) };
        let (data, release, oal) = (c.data, c.release, c.oal);
        c.l5.destroy(oal);
        // SAFETY: `release` is the host's `release_data`; `data` is the live `iSoundData`.
        unsafe { release(data) };
    }

    pub(super) fn data(&self) -> *mut c_void {
        self.data
    }

    /// Diagnostic read, codes as `OhplSndCh` in RustSound.h.
    pub(super) fn read(&self, what: i32) -> f64 {
        match what {
            0 => f64::from(self.l5.shadow.volume),
            1 => f64::from(self.l5.is_playing(self.oal)),
            2 => f64::from(self.l5.priority(self.oal) as i32),
            3 => self.l5.elapsed(self.oal),
            4 => self.l5.total(self.oal),
            5 => f64::from(self.l5.is_buffer_underrun(self.oal)),
            6 => f64::from(self.l5.shadow.paused),
            7 => f64::from(self.l5.shadow.looping),
            8 => f64::from(self.l5.shadow.is_3d),
            9 => f64::from(self.l5.shadow.position_relative),
            10 => f64::from(self.l5.shadow.position[0]),
            11 => f64::from(self.l5.shadow.position[1]),
            12 => f64::from(self.l5.shadow.position[2]),
            13 => f64::from(self.l5.shadow.rel_position[0]),
            14 => f64::from(self.l5.shadow.rel_position[1]),
            15 => f64::from(self.l5.shadow.rel_position[2]),
            16 => f64::from(self.l5.shadow.min_distance),
            _ => f64::from(self.l5.shadow.max_distance),
        }
    }
}

impl Channel for CChannel<'_> {
    fn play(&mut self) {
        self.set_paused(false);
        self.ch.l5.shadow.stop_used = false;
    }
    fn stop(&mut self) {
        self.ch.l5.stop(self.ch.oal);
        self.ch.l5.shadow.stop_used = true;
    }
    fn set_paused(&mut self, paused: bool) {
        self.ch.l5.shadow.paused = paused;
        self.ch.l5.set_paused(self.ch.oal, paused);
    }
    fn paused(&self) -> bool {
        self.ch.l5.shadow.paused
    }
    fn is_playing(&mut self) -> bool {
        self.ch.l5.is_playing(self.ch.oal)
    }
    fn stop_used(&self) -> bool {
        self.ch.l5.shadow.stop_used
    }
    fn speed(&self) -> f32 {
        self.ch.l5.shadow.speed
    }
    fn set_speed(&mut self, speed: f32) {
        self.ch.l5.shadow.speed = speed;
        self.ch.l5.set_pitch(self.ch.oal, speed);
    }
    fn volume(&self) -> f32 {
        self.ch.l5.shadow.volume
    }
    fn set_volume(&mut self, volume: f32) {
        self.ch.l5.shadow.volume = volume;
        self.ch.l5.set_volume(self.ch.oal, volume);
    }
    fn looping(&self) -> bool {
        self.ch.l5.shadow.looping
    }
    fn set_looping(&mut self, looping: bool) {
        self.ch.l5.shadow.looping = looping;
        self.ch.l5.set_loop(self.ch.oal, looping);
    }
    fn set_3d(&mut self, is_3d: bool) {
        let sh = &mut self.ch.l5.shadow;
        sh.is_3d = is_3d;
        let on = is_3d && !sh.position_relative;
        self.ch.l5.set_spatialize(self.ch.oal, on);
    }
    fn set_priority(&mut self, priority: i32) {
        let m = self.ch.l5.shadow.priority_modifier;
        self.ch.l5.set_priority(self.ch.oal, priority, m);
    }
    fn set_priority_modifier(&mut self, modifier: i32) {
        self.ch.l5.shadow.priority_modifier = modifier;
        let p = self.ch.l5.priority(self.ch.oal) as i32;
        self.set_priority(p);
    }
    fn set_affected_by_env(&mut self, affected: bool) {
        self.ch.l5.shadow.affected_by_env = affected;
        self.ch.l5.set_aux_send_slot(self.ch.oal, affected);
    }
    fn position_is_relative(&self) -> bool {
        self.ch.l5.shadow.position_relative
    }
    fn set_position_is_relative(&mut self, relative: bool) {
        let sh = &mut self.ch.l5.shadow;
        sh.position_relative = relative;
        let on = sh.is_3d && !relative;
        self.ch.l5.set_spatialize(self.ch.oal, on);
    }
    fn position(&self) -> Vector3f {
        let p = self.ch.l5.shadow.position;
        Vector3f {
            x: p[0],
            y: p[1],
            z: p[2],
        }
    }
    fn set_position(&mut self, pos: Vector3f) {
        let sh = &mut self.ch.l5.shadow;
        sh.position = [pos.x, pos.y, pos.z];
        let (p, v) = (sh.position, sh.velocity);
        self.ch.l5.set_attributes(self.ch.oal, &p, &v);
    }
    fn rel_position(&self) -> Vector3f {
        let p = self.ch.l5.shadow.rel_position;
        Vector3f {
            x: p[0],
            y: p[1],
            z: p[2],
        }
    }
    fn set_rel_position(&mut self, pos: Vector3f) {
        self.ch.l5.shadow.rel_position = [pos.x, pos.y, pos.z];
    }
    fn min_distance(&self) -> f32 {
        self.ch.l5.shadow.min_distance
    }
    fn set_min_distance(&mut self, dist: f32) {
        self.ch.l5.shadow.min_distance = dist;
    }
    fn max_distance(&self) -> f32 {
        self.ch.l5.shadow.max_distance
    }
    fn set_max_distance(&mut self, dist: f32) {
        self.ch.l5.shadow.max_distance = dist;
    }
    fn block_volume_mul(&self) -> f32 {
        self.ch.l5.shadow.block_volume_mul
    }
    fn elapsed_time(&self) -> f64 {
        self.ch.l5.elapsed(self.ch.oal)
    }
    fn total_time(&self) -> f64 {
        self.ch.l5.total(self.ch.oal)
    }
    fn set_elapsed_time(&mut self, time: f64) {
        self.ch.l5.set_elapsed(self.ch.oal, time);
    }
    fn set_blockable(&mut self, blockable: bool) {
        self.ch.l5.shadow.blockable = blockable;
    }
    fn set_block_volume_mul(&mut self, mul: f32) {
        self.ch.l5.shadow.block_volume_mul = mul;
    }
    fn set_filter(&mut self, which: i32, gain: f32) {
        match which {
            0 => self.ch.l5.set_filter_gain_hf(self.ch.oal, gain),
            1 => self.ch.l5.set_filter_gain_lf(self.ch.oal, gain),
            _ => self.ch.l5.set_env_send_gain(self.ch.oal, gain),
        }
    }
}
