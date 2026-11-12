//! C ABI of the sound classes, declared in `HPL2/core/include/impl/RustL5.h`. Handles are
//! `Box`es owned by the C++ shell that created them.

use super::env::{self, EnvParams};
use super::log::{self, Level, c_bytes};
use super::oal::RealOal;
use super::snd::{Channel, Data, Device, InitArgs, Low, enumerate_devices, first_default};
use crate::string_ffi::units;
use core::ffi::{c_char, c_void};
use std::ffi::OsStr;
use std::os::unix::ffi::OsStrExt;

fn emit(level: Level, msg: &[u8]) {
    log::to_callback(level, msg);
}

/// # Safety
/// `p` is a live handle from the matching `_new`.
unsafe fn r<'a, T>(p: *mut T) -> Option<&'a mut T> {
    // SAFETY: caller contract.
    unsafe { p.as_mut() }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l5_data_new(stream: bool, loop_stream: bool) -> *mut Data {
    Box::into_raw(Box::new(Data::new(stream, loop_stream)))
}

/// # Safety
/// `d` is live; `path` holds `len` code points.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_data_create(d: *mut Data, path: *const u32, len: usize) -> bool {
    // SAFETY: caller contract.
    let (Some(d), path) = (unsafe { r(d) }, unsafe { units(path, len) }) else {
        return false;
    };
    d.create_from_file(&RealOal, path, &mut emit)
}

/// The new source handle, -1 for none.
///
/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_data_channel(d: *mut Data, priority: i32) -> i32 {
    // SAFETY: caller contract.
    unsafe { r(d) }
        .and_then(|d| d.create_channel(&RealOal, priority))
        .unwrap_or(-1)
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_data_is_stereo(d: *mut Data) -> bool {
    // SAFETY: caller contract.
    unsafe { r(d) }.is_some_and(|d| d.is_stereo(&RealOal))
}

/// # Safety
/// `d` from `ohpl_l5_data_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_data_free(d: *mut Data) {
    if !d.is_null() {
        // SAFETY: caller contract.
        unsafe { Box::from_raw(d) }.release(&RealOal);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l5_chan_new(handle: i32) -> *mut Channel {
    Box::into_raw(Box::new(Channel::new(&RealOal, handle)))
}

/// # Safety
/// `c` from `ohpl_l5_chan_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_chan_free(c: *mut Channel) {
    if !c.is_null() {
        // SAFETY: caller contract.
        unsafe { Box::from_raw(c) }.destroy(&RealOal);
    }
}

macro_rules! chan_fn {
    ($name:ident($($a:ident: $t:ty),*) $body:expr) => {
        /// # Safety
        /// `c` is live.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(c: *mut Channel $(, $a: $t)*) {
            // SAFETY: caller contract.
            if let Some(c) = unsafe { r(c) } {
                let f: fn(&mut Channel $(, $t)*) = $body;
                f(c $(, $a)*);
            }
        }
    };
}

chan_fn!(ohpl_l5_chan_stop() | c | c.stop(&RealOal));
chan_fn!(ohpl_l5_chan_set_paused(p: bool) |c, p| c.set_paused(&RealOal, p));
chan_fn!(ohpl_l5_chan_set_pitch(v: f32) |c, v| c.set_pitch(&RealOal, v));
chan_fn!(ohpl_l5_chan_set_volume(v: f32) |c, v| c.set_volume(&RealOal, v));
chan_fn!(ohpl_l5_chan_set_loop(l: bool) |c, l| c.set_loop(&RealOal, l));
chan_fn!(ohpl_l5_chan_set_spatialize(v: bool) |c, v| c.set_spatialize(&RealOal, v));
chan_fn!(ohpl_l5_chan_set_elapsed(t: f64) |c, t| c.set_elapsed(&RealOal, t));
chan_fn!(ohpl_l5_chan_set_priority(x: i32, modifier: i32) |c, x, m| c.set_priority(&RealOal, x, m));
chan_fn!(ohpl_l5_chan_set_aux_slot(affected: bool) |c, a| c.set_aux_send_slot(&RealOal, a));
chan_fn!(ohpl_l5_chan_set_filtering(on: bool, flags: i32) |c, on, f| c.set_filtering(&RealOal, on, f));
chan_fn!(ohpl_l5_chan_set_filter_gain(g: f32) |c, g| c.set_filter_gain(&RealOal, g));
chan_fn!(ohpl_l5_chan_set_filter_gain_hf(g: f32) |c, g| c.set_filter_gain_hf(&RealOal, g));
chan_fn!(ohpl_l5_chan_set_filter_gain_lf(g: f32) |c, g| c.set_filter_gain_lf(&RealOal, g));
chan_fn!(ohpl_l5_chan_set_env_send_gain(g: f32) |c, g| c.set_env_send_gain(&RealOal, g));

/// # Safety
/// `c` is live; `pos` and `vel` point to three floats each.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_chan_set_attributes(
    c: *mut Channel,
    pos: *const f32,
    vel: *const f32,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(c), Some(p), Some(v)) = (
            r(c),
            pos.cast::<[f32; 3]>().as_ref(),
            vel.cast::<[f32; 3]>().as_ref(),
        ) {
            c.set_attributes(&RealOal, p, v);
        }
    }
}

/// # Safety
/// `c` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_chan_is_playing(c: *mut Channel) -> bool {
    // SAFETY: caller contract.
    unsafe { r(c) }.is_some_and(|c| c.is_playing(&RealOal))
}

/// # Safety
/// `c` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_chan_is_underrun(c: *mut Channel) -> bool {
    // SAFETY: caller contract.
    unsafe { r(c) }.is_some_and(|c| c.is_buffer_underrun(&RealOal))
}

/// # Safety
/// `c` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_chan_elapsed(c: *mut Channel) -> f64 {
    // SAFETY: caller contract.
    unsafe { r(c) }.map_or(0.0, |c| c.elapsed(&RealOal))
}

/// # Safety
/// `c` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_chan_total(c: *mut Channel) -> f64 {
    // SAFETY: caller contract.
    unsafe { r(c) }.map_or(0.0, |c| c.total(&RealOal))
}

/// # Safety
/// `c` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_chan_priority(c: *mut Channel) -> i32 {
    // SAFETY: caller contract.
    unsafe { r(c) }.map_or(0, |c| c.priority(&RealOal) as i32)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l5_low_new() -> *mut Low {
    Box::into_raw(Box::new(Low::new()))
}

/// # Safety
/// `l` from `ohpl_l5_low_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_low_free(l: *mut Low) {
    if !l.is_null() {
        // SAFETY: caller contract.
        unsafe { Box::from_raw(l) }.close(&RealOal);
    }
}

/// # Safety
/// `l` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_low_update(l: *mut Low) {
    // SAFETY: caller contract.
    if let Some(l) = unsafe { r(l) } {
        l.update(&RealOal);
    }
}

/// # Safety
/// `l` is live; each vector points to three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_low_set_listener(
    l: *mut Low,
    pos: *const f32,
    vel: *const f32,
    fwd: *const f32,
    up: *const f32,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(l), Some(p), Some(v), Some(f), Some(u)) = (
            r(l),
            pos.cast::<[f32; 3]>().as_ref(),
            vel.cast::<[f32; 3]>().as_ref(),
            fwd.cast::<[f32; 3]>().as_ref(),
            up.cast::<[f32; 3]>().as_ref(),
        ) {
            l.set_listener(&RealOal, p, v, f, u);
        }
    }
}

/// # Safety
/// `l` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_low_set_rolloff(l: *mut Low, f: f32) {
    // SAFETY: caller contract.
    if let Some(l) = unsafe { r(l) } {
        l.set_rolloff(&RealOal, f);
    }
}

/// # Safety
/// `l` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_low_set_volume(l: *mut Low, v: f32) {
    // SAFETY: caller contract.
    if let Some(l) = unsafe { r(l) } {
        l.set_volume(&RealOal, v);
    }
}

/// Writes the clamped volume and returns true when the effect slot took it.
///
/// # Safety
/// `l` is live; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_low_set_env_volume(l: *mut Low, v: f32, out: *mut f32) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match (
            r(l).and_then(|l| l.set_env_volume(&RealOal, v)),
            out.as_mut(),
        ) {
            (Some(x), Some(o)) => {
                *o = x;
                true
            }
            _ => false,
        }
    }
}

/// # Safety
/// `l` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_low_env_enabled(l: *mut Low) -> bool {
    // SAFETY: caller contract.
    unsafe { r(l) }.is_some_and(|l| l.env_enabled())
}

#[repr(C)]
pub struct OhplL5Device {
    id: i32,
    name: *const u8,
    name_len: usize,
    is_default: bool,
}

#[repr(C)]
pub struct OhplL5InitArgs {
    device_id: i32,
    use_env_audio: bool,
    max_channels: i32,
    stream_update_freq: i32,
    use_threading: bool,
    use_voice_management: bool,
    max_mono_hint: i32,
    max_stereo_hint: i32,
    stream_buffer_size: i32,
    stream_buffer_count: i32,
    enable_log: bool,
}

#[repr(C)]
pub struct OhplL5InitResult {
    initialized: bool,
    device_id: i32,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l5_low_prelude(enable_log: bool) {
    Low::init_prelude(enable_log, &mut emit);
}

type DeviceSink = unsafe extern "C" fn(*mut c_void, i32, *const u8, usize, bool);

/// `PopulateAvailableSoundDevices`: calls `sink` once per OpenAL output device.
///
/// # Safety
/// `sink` accepts `ctx` and the name pointer for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_devices(sink: DeviceSink, ctx: *mut c_void) {
    for d in enumerate_devices(&RealOal) {
        // SAFETY: caller contract.
        unsafe { sink(ctx, d.id, d.name.as_ptr(), d.name.len(), d.default) };
    }
}

/// # Safety
/// `p` points to `n` devices whose names are valid for their lengths.
unsafe fn devices(p: *const OhplL5Device, n: usize) -> Vec<Device> {
    // SAFETY: caller contract.
    unsafe { units(p, n) }
        .iter()
        .map(|d| Device {
            id: d.id,
            // SAFETY: caller contract.
            name: unsafe { units(d.name, d.name_len) }.to_vec(),
            default: d.is_default,
        })
        .collect()
}

/// The id of `GetFirstDefaultDevice` among `all`, -1 when there is none.
///
/// # Safety
/// Device arrays as in `ohpl_l5_low_init`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_first_default(
    all: *const OhplL5Device,
    n_all: usize,
    valid: *const OhplL5Device,
    n_valid: usize,
) -> i32 {
    // SAFETY: caller contract.
    let (all, valid) = unsafe { (devices(all, n_all), devices(valid, n_valid)) };
    first_default(&all, &valid).map_or(-1, |d| d.id)
}

/// # Safety
/// `l` is live; `args` is readable; the device arrays are valid for their counts and `env_device`
/// is NUL-terminated or null (`OPENHPL_SOUND_DEVICE`).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_low_init(
    l: *mut Low,
    args: *const OhplL5InitArgs,
    all: *const OhplL5Device,
    n_all: usize,
    valid: *const OhplL5Device,
    n_valid: usize,
    env_device: *const c_char,
) -> OhplL5InitResult {
    let fail = OhplL5InitResult {
        initialized: false,
        device_id: -2,
    };
    // SAFETY: caller contract.
    let (Some(l), Some(a)) = (unsafe { r(l) }, unsafe { args.as_ref() }) else {
        return fail;
    };
    // SAFETY: caller contract.
    let (all, valid) = unsafe { (devices(all, n_all), devices(valid, n_valid)) };
    let env = (!env_device.is_null()).then(|| {
        // SAFETY: caller contract.
        unsafe { c_bytes(env_device) }
    });
    let args = InitArgs {
        device_id: a.device_id,
        use_env_audio: a.use_env_audio,
        max_channels: a.max_channels,
        stream_update_freq: a.stream_update_freq,
        use_threading: a.use_threading,
        use_voice_management: a.use_voice_management,
        max_mono_hint: a.max_mono_hint,
        max_stereo_hint: a.max_stereo_hint,
        stream_buffer_size: a.stream_buffer_size,
        stream_buffer_count: a.stream_buffer_count,
        enable_log: a.enable_log,
    };
    let res = l.init(&RealOal, &args, &all, &valid, env, &mut emit);
    OhplL5InitResult {
        initialized: res.initialized,
        device_id: res.device_id,
    }
}

/// # Safety
/// `l` is live; `env` is null or points to a parameter set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_low_set_env(l: *mut Low, env: *const EnvParams) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(l) = r(l) {
            l.set_environment(&RealOal, env.as_ref());
        }
    }
}

/// # Safety
/// `l` is live; `src` and `dst` are null or point to parameter sets.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_low_fade_env(
    l: *mut Low,
    src: *const EnvParams,
    dst: *const EnvParams,
    t: f32,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(l) = r(l) {
            l.fade_environment(&RealOal, src.as_ref(), dst.as_ref(), t);
        }
    }
}

type NameSink = unsafe extern "C" fn(*mut c_void, *const u8, usize);

/// Parses a sound environment file into `out`, handing its name to `sink`.
///
/// # Safety
/// `path` is NUL-terminated; `out` is writable; `sink` accepts `ctx` and the name.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_env_load(
    path: *const c_char,
    out: *mut EnvParams,
    sink: NameSink,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let path = OsStr::from_bytes(unsafe { c_bytes(path) });
    let Some(loaded) = std::fs::read(path).ok().and_then(|d| env::parse(&d)) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        if let Some(o) = out.as_mut() {
            *o = loaded.params;
        }
        sink(ctx, loaded.name.as_ptr(), loaded.name.len());
    }
    true
}

const _: () = assert!(
    size_of::<OhplL5Device>() == 32
        && size_of::<OhplL5InitArgs>() == 40
        && core::mem::offset_of!(OhplL5InitArgs, enable_log) == 36
        && size_of::<OhplL5InitResult>() == 8
);
