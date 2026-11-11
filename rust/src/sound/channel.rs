//! The seam to the C++ `iSoundChannel` and the sound handler: Rust never owns either, it calls
//! back through function tables, so the OpenAL backend and the game layers stay C++.

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
}

pub(super) trait EntryHost {
    fn global_speed(&self, entry_type: u32) -> f32;
    fn global_volume(&self, entry_type: u32) -> f32;
    fn silent(&self) -> bool;
    fn is_blocked(&mut self, pos: Vector3f) -> bool;
    fn listener_position(&self) -> Vector3f;
    fn listener_matrix(&self) -> Matrixf;
}

type Fn0<R = ()> = unsafe extern "C" fn(*mut c_void) -> R;
type Fn1<A, R = ()> = unsafe extern "C" fn(*mut c_void, A) -> R;

/// Calls on one `iSoundChannel`; every entry takes the channel pointer first.
#[repr(C)]
pub struct ChannelOps {
    pub play: Fn0,
    pub stop: Fn0,
    pub set_paused: Fn1<bool>,
    pub paused: Fn0<bool>,
    pub is_playing: Fn0<bool>,
    pub stop_used: Fn0<bool>,
    pub speed: Fn0<f32>,
    pub set_speed: Fn1<f32>,
    pub volume: Fn0<f32>,
    pub set_volume: Fn1<f32>,
    pub looping: Fn0<bool>,
    pub set_looping: Fn1<bool>,
    pub set_3d: Fn1<bool>,
    pub set_priority: Fn1<i32>,
    pub set_priority_modifier: Fn1<i32>,
    pub set_affected_by_env: Fn1<bool>,
    pub position_is_relative: Fn0<bool>,
    pub set_position_is_relative: Fn1<bool>,
    pub position: Fn1<*mut Vector3f>,
    pub set_position: Fn1<*const Vector3f>,
    pub rel_position: Fn1<*mut Vector3f>,
    pub set_rel_position: Fn1<*const Vector3f>,
    pub min_distance: Fn0<f32>,
    pub set_min_distance: Fn1<f32>,
    pub max_distance: Fn0<f32>,
    pub set_max_distance: Fn1<f32>,
    pub block_volume_mul: Fn0<f32>,
    pub elapsed_time: Fn0<f64>,
    pub total_time: Fn0<f64>,
    pub set_elapsed_time: Fn1<f64>,
}

/// Calls on the owning `cSoundHandler`; every entry takes the handler pointer first.
#[repr(C)]
pub struct HostOps {
    pub global_speed: Fn1<u32, f32>,
    pub global_volume: Fn1<u32, f32>,
    pub silent: Fn0<bool>,
    pub is_blocked: Fn1<*const Vector3f, bool>,
    pub listener_position: Fn1<*mut Vector3f>,
    pub listener_matrix: Fn1<*mut Matrixf>,
}

/// A live `iSoundChannel` and its function table.
pub(super) struct CChannel<'a> {
    ops: &'a ChannelOps,
    ch: *mut c_void,
}

impl<'a> CChannel<'a> {
    /// # Safety
    /// `ch` stays a valid channel for `ops` for the lifetime of the value.
    pub(super) unsafe fn new(ops: &'a ChannelOps, ch: *mut c_void) -> Self {
        Self { ops, ch }
    }
}

macro_rules! call {
    ($s:expr, $f:ident $(, $a:expr)*) => {
        // SAFETY: `CChannel::new` contract: `ch` is valid for the table that was paired with it.
        unsafe { ($s.ops.$f)($s.ch $(, $a)*) }
    };
}

fn read_vec(f: impl FnOnce(*mut Vector3f)) -> Vector3f {
    let mut v = Vector3f::default();
    f(&raw mut v);
    v
}

impl Channel for CChannel<'_> {
    fn play(&mut self) {
        call!(self, play)
    }
    fn stop(&mut self) {
        call!(self, stop)
    }
    fn set_paused(&mut self, paused: bool) {
        call!(self, set_paused, paused)
    }
    fn paused(&self) -> bool {
        call!(self, paused)
    }
    fn is_playing(&mut self) -> bool {
        call!(self, is_playing)
    }
    fn stop_used(&self) -> bool {
        call!(self, stop_used)
    }
    fn speed(&self) -> f32 {
        call!(self, speed)
    }
    fn set_speed(&mut self, speed: f32) {
        call!(self, set_speed, speed)
    }
    fn volume(&self) -> f32 {
        call!(self, volume)
    }
    fn set_volume(&mut self, volume: f32) {
        call!(self, set_volume, volume)
    }
    fn looping(&self) -> bool {
        call!(self, looping)
    }
    fn set_looping(&mut self, looping: bool) {
        call!(self, set_looping, looping)
    }
    fn set_3d(&mut self, is_3d: bool) {
        call!(self, set_3d, is_3d)
    }
    fn set_priority(&mut self, priority: i32) {
        call!(self, set_priority, priority)
    }
    fn set_priority_modifier(&mut self, modifier: i32) {
        call!(self, set_priority_modifier, modifier)
    }
    fn set_affected_by_env(&mut self, affected: bool) {
        call!(self, set_affected_by_env, affected)
    }
    fn position_is_relative(&self) -> bool {
        call!(self, position_is_relative)
    }
    fn set_position_is_relative(&mut self, relative: bool) {
        call!(self, set_position_is_relative, relative)
    }
    fn position(&self) -> Vector3f {
        read_vec(|out| call!(self, position, out))
    }
    fn set_position(&mut self, pos: Vector3f) {
        call!(self, set_position, &raw const pos)
    }
    fn rel_position(&self) -> Vector3f {
        read_vec(|out| call!(self, rel_position, out))
    }
    fn set_rel_position(&mut self, pos: Vector3f) {
        call!(self, set_rel_position, &raw const pos)
    }
    fn min_distance(&self) -> f32 {
        call!(self, min_distance)
    }
    fn set_min_distance(&mut self, dist: f32) {
        call!(self, set_min_distance, dist)
    }
    fn max_distance(&self) -> f32 {
        call!(self, max_distance)
    }
    fn set_max_distance(&mut self, dist: f32) {
        call!(self, set_max_distance, dist)
    }
    fn block_volume_mul(&self) -> f32 {
        call!(self, block_volume_mul)
    }
    fn elapsed_time(&self) -> f64 {
        call!(self, elapsed_time)
    }
    fn total_time(&self) -> f64 {
        call!(self, total_time)
    }
    fn set_elapsed_time(&mut self, time: f64) {
        call!(self, set_elapsed_time, time)
    }
}

/// A live `cSoundHandler` and its function table.
pub(super) struct CHost<'a> {
    ops: &'a HostOps,
    handler: *mut c_void,
}

impl<'a> CHost<'a> {
    /// # Safety
    /// `handler` stays a valid handler for `ops` for the lifetime of the value.
    pub(super) unsafe fn new(ops: &'a HostOps, handler: *mut c_void) -> Self {
        Self { ops, handler }
    }
}

macro_rules! host_call {
    ($s:expr, $f:ident $(, $a:expr)*) => {
        // SAFETY: `CHost::new` contract: `handler` is valid for the table that was paired with it.
        unsafe { ($s.ops.$f)($s.handler $(, $a)*) }
    };
}

impl EntryHost for CHost<'_> {
    fn global_speed(&self, entry_type: u32) -> f32 {
        host_call!(self, global_speed, entry_type)
    }
    fn global_volume(&self, entry_type: u32) -> f32 {
        host_call!(self, global_volume, entry_type)
    }
    fn silent(&self) -> bool {
        host_call!(self, silent)
    }
    fn is_blocked(&mut self, pos: Vector3f) -> bool {
        host_call!(self, is_blocked, &raw const pos)
    }
    fn listener_position(&self) -> Vector3f {
        let mut v = Vector3f::default();
        host_call!(self, listener_position, &raw mut v);
        v
    }
    fn listener_matrix(&self) -> Matrixf {
        let mut m = Matrixf::default();
        host_call!(self, listener_matrix, &raw mut m);
        m
    }
}
