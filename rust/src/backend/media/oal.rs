//! The OALWrapper calls the engine makes, behind a trait so the sound logic can run against a
//! recording fake. `RealOal` forwards to the one-line wrappers in `cshim/oal_shim.cpp`.

use core::ffi::{CStr, c_char, c_void};

/// An opaque OALWrapper object (sample, stream or effect).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Ptr(pub *mut c_void);

impl Ptr {
    pub const NULL: Ptr = Ptr(core::ptr::null_mut());

    pub fn is_null(self) -> bool {
        self.0.is_null()
    }
}

/// `OAL_FREE`.
pub const OAL_FREE: i32 = -3;

/// Order matches the switch in `ohpl_oal_reverb_set`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum Reverb {
    Density,
    Diffusion,
    EchoTime,
    EchoDepth,
    ModulationTime,
    ModulationDepth,
    Gain,
    GainHf,
    GainLf,
    ReflectionsDelay,
    ReflectionsGain,
    LateReverbGain,
    LateReverbDelay,
    HfReference,
    LfReference,
    AirAbsorptionGainHf,
    DecayTime,
    DecayHfRatio,
    DecayLfRatio,
    RoomRolloffFactor,
}

#[derive(Clone, Debug, PartialEq)]
pub struct InitParams {
    pub device_name: Vec<u8>,
    pub output_freq: i32,
    pub update_freq: i32,
    pub num_sources_hint: i32,
    pub min_mono_hint: i32,
    pub min_stereo_hint: i32,
    pub stream_buffer_size: i32,
    pub stream_buffer_count: i32,
    pub use_thread: bool,
    pub voice_management: bool,
    pub use_efx: bool,
}

pub trait Oal {
    fn sample_load(&self, path: &[u32]) -> Ptr;
    fn stream_load(&self, path: &[u32]) -> Ptr;
    fn sample_unload(&self, s: Ptr);
    fn stream_unload(&self, s: Ptr);
    fn sample_set_loop(&self, s: Ptr, l: bool);
    fn stream_set_loop(&self, s: Ptr, l: bool);
    fn sample_channels(&self, s: Ptr) -> i32;
    fn stream_channels(&self, s: Ptr) -> i32;
    fn sample_play(&self, src: i32, s: Ptr, vol: f32, paused: bool, prio: i32) -> i32;
    fn stream_play(&self, src: i32, s: Ptr, vol: f32, paused: bool) -> i32;

    fn source_stop(&self, s: i32);
    fn source_set_paused(&self, s: i32, p: bool);
    fn source_set_gain(&self, s: i32, g: f32);
    fn source_set_pitch(&self, s: i32, p: f32);
    fn source_set_loop(&self, s: i32, l: bool);
    fn source_set_attributes(&self, s: i32, pos: &[f32; 3], vel: &[f32; 3]);
    fn source_set_spatialize(&self, s: i32, v: bool);
    fn source_set_priority(&self, s: i32, p: u32);
    fn source_get_priority(&self, s: i32) -> u32;
    fn source_is_playing(&self, s: i32) -> bool;
    fn source_is_buffer_underrun(&self, s: i32) -> bool;
    fn source_get_elapsed(&self, s: i32) -> f64;
    fn source_get_total(&self, s: i32) -> f64;
    fn source_set_elapsed(&self, s: i32, t: f64);
    fn source_set_filter_type_bandpass(&self, s: i32);
    fn source_set_aux_send_slot(&self, s: i32, send: i32, slot: i32);
    fn source_set_filtering(&self, s: i32, on: bool, flags: i32);
    fn source_set_filter_gain(&self, s: i32, g: f32);
    fn source_set_filter_gain_hf(&self, s: i32, g: f32);
    fn source_set_filter_gain_lf(&self, s: i32, g: f32);
    fn source_set_aux_send_gain(&self, s: i32, g: f32);

    fn listener_set_attributes(
        &self,
        pos: &[f32; 3],
        vel: &[f32; 3],
        fwd: &[f32; 3],
        up: &[f32; 3],
    );
    fn listener_set_master_volume(&self, v: f32);

    fn init(&self, p: &InitParams) -> bool;
    fn close(&self);
    fn update(&self);
    fn set_rolloff_factor(&self, f: f32);
    fn set_distance_model_none(&self);
    fn setup_logging(&self, on: bool, name: &[u8]);
    fn info_device_name(&self) -> Vec<u8>;
    fn info_num_sources(&self) -> i32;
    fn info_efx_active(&self) -> bool;
    fn info_stream_buffer_count(&self) -> i32;
    fn info_stream_buffer_size(&self) -> i32;
    fn info_default_device(&self) -> Vec<u8>;
    fn info_output_devices(&self) -> Vec<Vec<u8>>;

    fn reverb_create(&self) -> Ptr;
    fn reverb_set(&self, e: Ptr, p: Reverb, v: f32);
    fn reverb_set_decay_hf_limit(&self, e: Ptr, v: i32);
    fn slot_attach(&self, slot: i32, effect: Ptr) -> bool;
    fn slot_set_gain(&self, slot: i32, g: f32);
    fn slot_set_auto_adjust(&self, slot: i32, v: bool);
}

type NameSink = unsafe extern "C" fn(*mut c_void, *const c_char);

unsafe extern "C" fn collect_name(ctx: *mut c_void, name: *const c_char) {
    // SAFETY: `ctx` is the `Vec<Vec<u8>>` passed to the shim and `name` is NUL-terminated.
    unsafe {
        if let (Some(v), false) = (ctx.cast::<Vec<Vec<u8>>>().as_mut(), name.is_null()) {
            v.push(CStr::from_ptr(name).to_bytes().to_vec());
        }
    }
}

// SAFETY (all blocks below): plain calls into the shim; pointers are valid for the call.
unsafe extern "C" {
    fn ohpl_oal_sample_load(path: *const u32, len: usize) -> *mut c_void;
    fn ohpl_oal_stream_load(path: *const u32, len: usize) -> *mut c_void;
    fn ohpl_oal_sample_unload(s: *mut c_void);
    fn ohpl_oal_stream_unload(s: *mut c_void);
    fn ohpl_oal_sample_set_loop(s: *mut c_void, l: bool);
    fn ohpl_oal_stream_set_loop(s: *mut c_void, l: bool);
    fn ohpl_oal_sample_channels(s: *mut c_void) -> i32;
    fn ohpl_oal_stream_channels(s: *mut c_void) -> i32;
    fn ohpl_oal_sample_play(src: i32, s: *mut c_void, vol: f32, paused: bool, prio: i32) -> i32;
    fn ohpl_oal_stream_play(src: i32, s: *mut c_void, vol: f32, paused: bool) -> i32;
    fn ohpl_oal_source_stop(s: i32);
    fn ohpl_oal_source_set_paused(s: i32, p: bool);
    fn ohpl_oal_source_set_gain(s: i32, g: f32);
    fn ohpl_oal_source_set_pitch(s: i32, p: f32);
    fn ohpl_oal_source_set_loop(s: i32, l: bool);
    fn ohpl_oal_source_set_attributes(s: i32, pos: *const f32, vel: *const f32);
    fn ohpl_oal_source_set_spatialize(s: i32, v: bool);
    fn ohpl_oal_source_set_priority(s: i32, p: u32);
    fn ohpl_oal_source_get_priority(s: i32) -> u32;
    fn ohpl_oal_source_is_playing(s: i32) -> bool;
    fn ohpl_oal_source_is_buffer_underrun(s: i32) -> bool;
    fn ohpl_oal_source_get_elapsed(s: i32) -> f64;
    fn ohpl_oal_source_get_total(s: i32) -> f64;
    fn ohpl_oal_source_set_elapsed(s: i32, t: f64);
    fn ohpl_oal_source_set_filter_type_bandpass(s: i32);
    fn ohpl_oal_source_set_aux_send_slot(s: i32, send: i32, slot: i32);
    fn ohpl_oal_source_set_filtering(s: i32, on: bool, flags: i32);
    fn ohpl_oal_source_set_filter_gain(s: i32, g: f32);
    fn ohpl_oal_source_set_filter_gain_hf(s: i32, g: f32);
    fn ohpl_oal_source_set_filter_gain_lf(s: i32, g: f32);
    fn ohpl_oal_source_set_aux_send_gain(s: i32, g: f32);
    fn ohpl_oal_listener_set_attributes(
        pos: *const f32,
        vel: *const f32,
        fwd: *const f32,
        up: *const f32,
    );
    fn ohpl_oal_listener_set_master_volume(v: f32);
    fn ohpl_oal_init(p: *const RawInit) -> bool;
    fn ohpl_oal_close();
    fn ohpl_oal_update();
    fn ohpl_oal_set_rolloff_factor(f: f32);
    fn ohpl_oal_set_distance_model_none();
    fn ohpl_oal_setup_logging(on: bool, name: *const c_char);
    fn ohpl_oal_info_device_name() -> *const c_char;
    fn ohpl_oal_info_num_sources() -> i32;
    fn ohpl_oal_info_efx_active() -> bool;
    fn ohpl_oal_info_stream_buffer_count() -> i32;
    fn ohpl_oal_info_stream_buffer_size() -> i32;
    fn ohpl_oal_info_default_device(sink: NameSink, ctx: *mut c_void);
    fn ohpl_oal_info_output_devices(sink: NameSink, ctx: *mut c_void);
    fn ohpl_oal_reverb_create() -> *mut c_void;
    fn ohpl_oal_reverb_set(e: *mut c_void, id: i32, v: f32);
    fn ohpl_oal_reverb_set_decay_hf_limit(e: *mut c_void, v: i32);
    fn ohpl_oal_slot_attach(slot: i32, effect: *mut c_void) -> bool;
    fn ohpl_oal_slot_set_gain(slot: i32, g: f32);
    fn ohpl_oal_slot_set_auto_adjust(slot: i32, v: bool);
}

/// Layout asserted in `cshim/layout_check.cpp`.
#[repr(C)]
struct RawInit {
    device_name: *const c_char,
    output_freq: i32,
    update_freq: i32,
    num_sources_hint: i32,
    min_mono_hint: i32,
    min_stereo_hint: i32,
    stream_buffer_size: i32,
    stream_buffer_count: i32,
    use_thread: u8,
    voice_management: u8,
    use_efx: u8,
}

fn cstr(b: &[u8]) -> Vec<u8> {
    let mut v: Vec<u8> = b.iter().copied().take_while(|&c| c != 0).collect();
    v.push(0);
    v
}

pub struct RealOal;

// SAFETY (all blocks below): shim calls with pointers valid for the call.
impl Oal for RealOal {
    fn sample_load(&self, path: &[u32]) -> Ptr {
        Ptr(unsafe { ohpl_oal_sample_load(path.as_ptr(), path.len()) })
    }
    fn stream_load(&self, path: &[u32]) -> Ptr {
        Ptr(unsafe { ohpl_oal_stream_load(path.as_ptr(), path.len()) })
    }
    fn sample_unload(&self, s: Ptr) {
        unsafe { ohpl_oal_sample_unload(s.0) }
    }
    fn stream_unload(&self, s: Ptr) {
        unsafe { ohpl_oal_stream_unload(s.0) }
    }
    fn sample_set_loop(&self, s: Ptr, l: bool) {
        unsafe { ohpl_oal_sample_set_loop(s.0, l) }
    }
    fn stream_set_loop(&self, s: Ptr, l: bool) {
        unsafe { ohpl_oal_stream_set_loop(s.0, l) }
    }
    fn sample_channels(&self, s: Ptr) -> i32 {
        unsafe { ohpl_oal_sample_channels(s.0) }
    }
    fn stream_channels(&self, s: Ptr) -> i32 {
        unsafe { ohpl_oal_stream_channels(s.0) }
    }
    fn sample_play(&self, src: i32, s: Ptr, vol: f32, paused: bool, prio: i32) -> i32 {
        unsafe { ohpl_oal_sample_play(src, s.0, vol, paused, prio) }
    }
    fn stream_play(&self, src: i32, s: Ptr, vol: f32, paused: bool) -> i32 {
        unsafe { ohpl_oal_stream_play(src, s.0, vol, paused) }
    }
    fn source_stop(&self, s: i32) {
        unsafe { ohpl_oal_source_stop(s) }
    }
    fn source_set_paused(&self, s: i32, p: bool) {
        unsafe { ohpl_oal_source_set_paused(s, p) }
    }
    fn source_set_gain(&self, s: i32, g: f32) {
        unsafe { ohpl_oal_source_set_gain(s, g) }
    }
    fn source_set_pitch(&self, s: i32, p: f32) {
        unsafe { ohpl_oal_source_set_pitch(s, p) }
    }
    fn source_set_loop(&self, s: i32, l: bool) {
        unsafe { ohpl_oal_source_set_loop(s, l) }
    }
    fn source_set_attributes(&self, s: i32, pos: &[f32; 3], vel: &[f32; 3]) {
        unsafe { ohpl_oal_source_set_attributes(s, pos.as_ptr(), vel.as_ptr()) }
    }
    fn source_set_spatialize(&self, s: i32, v: bool) {
        unsafe { ohpl_oal_source_set_spatialize(s, v) }
    }
    fn source_set_priority(&self, s: i32, p: u32) {
        unsafe { ohpl_oal_source_set_priority(s, p) }
    }
    fn source_get_priority(&self, s: i32) -> u32 {
        unsafe { ohpl_oal_source_get_priority(s) }
    }
    fn source_is_playing(&self, s: i32) -> bool {
        unsafe { ohpl_oal_source_is_playing(s) }
    }
    fn source_is_buffer_underrun(&self, s: i32) -> bool {
        unsafe { ohpl_oal_source_is_buffer_underrun(s) }
    }
    fn source_get_elapsed(&self, s: i32) -> f64 {
        unsafe { ohpl_oal_source_get_elapsed(s) }
    }
    fn source_get_total(&self, s: i32) -> f64 {
        unsafe { ohpl_oal_source_get_total(s) }
    }
    fn source_set_elapsed(&self, s: i32, t: f64) {
        unsafe { ohpl_oal_source_set_elapsed(s, t) }
    }
    fn source_set_filter_type_bandpass(&self, s: i32) {
        unsafe { ohpl_oal_source_set_filter_type_bandpass(s) }
    }
    fn source_set_aux_send_slot(&self, s: i32, send: i32, slot: i32) {
        unsafe { ohpl_oal_source_set_aux_send_slot(s, send, slot) }
    }
    fn source_set_filtering(&self, s: i32, on: bool, flags: i32) {
        unsafe { ohpl_oal_source_set_filtering(s, on, flags) }
    }
    fn source_set_filter_gain(&self, s: i32, g: f32) {
        unsafe { ohpl_oal_source_set_filter_gain(s, g) }
    }
    fn source_set_filter_gain_hf(&self, s: i32, g: f32) {
        unsafe { ohpl_oal_source_set_filter_gain_hf(s, g) }
    }
    fn source_set_filter_gain_lf(&self, s: i32, g: f32) {
        unsafe { ohpl_oal_source_set_filter_gain_lf(s, g) }
    }
    fn source_set_aux_send_gain(&self, s: i32, g: f32) {
        unsafe { ohpl_oal_source_set_aux_send_gain(s, g) }
    }
    fn listener_set_attributes(
        &self,
        pos: &[f32; 3],
        vel: &[f32; 3],
        fwd: &[f32; 3],
        up: &[f32; 3],
    ) {
        unsafe {
            ohpl_oal_listener_set_attributes(pos.as_ptr(), vel.as_ptr(), fwd.as_ptr(), up.as_ptr())
        }
    }
    fn listener_set_master_volume(&self, v: f32) {
        unsafe { ohpl_oal_listener_set_master_volume(v) }
    }
    fn init(&self, p: &InitParams) -> bool {
        let name = cstr(&p.device_name);
        let raw = RawInit {
            device_name: name.as_ptr().cast(),
            output_freq: p.output_freq,
            update_freq: p.update_freq,
            num_sources_hint: p.num_sources_hint,
            min_mono_hint: p.min_mono_hint,
            min_stereo_hint: p.min_stereo_hint,
            stream_buffer_size: p.stream_buffer_size,
            stream_buffer_count: p.stream_buffer_count,
            use_thread: u8::from(p.use_thread),
            voice_management: u8::from(p.voice_management),
            use_efx: u8::from(p.use_efx),
        };
        unsafe { ohpl_oal_init(&raw) }
    }
    fn close(&self) {
        unsafe { ohpl_oal_close() }
    }
    fn update(&self) {
        unsafe { ohpl_oal_update() }
    }
    fn set_rolloff_factor(&self, f: f32) {
        unsafe { ohpl_oal_set_rolloff_factor(f) }
    }
    fn set_distance_model_none(&self) {
        unsafe { ohpl_oal_set_distance_model_none() }
    }
    fn setup_logging(&self, on: bool, name: &[u8]) {
        let name = cstr(name);
        unsafe { ohpl_oal_setup_logging(on, name.as_ptr().cast()) }
    }
    fn info_device_name(&self) -> Vec<u8> {
        let p = unsafe { ohpl_oal_info_device_name() };
        if p.is_null() {
            Vec::new()
        } else {
            unsafe { CStr::from_ptr(p) }.to_bytes().to_vec()
        }
    }
    fn info_num_sources(&self) -> i32 {
        unsafe { ohpl_oal_info_num_sources() }
    }
    fn info_efx_active(&self) -> bool {
        unsafe { ohpl_oal_info_efx_active() }
    }
    fn info_stream_buffer_count(&self) -> i32 {
        unsafe { ohpl_oal_info_stream_buffer_count() }
    }
    fn info_stream_buffer_size(&self) -> i32 {
        unsafe { ohpl_oal_info_stream_buffer_size() }
    }
    fn info_default_device(&self) -> Vec<u8> {
        let mut v: Vec<Vec<u8>> = Vec::new();
        unsafe { ohpl_oal_info_default_device(collect_name, (&raw mut v).cast()) };
        v.into_iter().next().unwrap_or_default()
    }
    fn info_output_devices(&self) -> Vec<Vec<u8>> {
        let mut v: Vec<Vec<u8>> = Vec::new();
        unsafe { ohpl_oal_info_output_devices(collect_name, (&raw mut v).cast()) };
        v
    }
    fn reverb_create(&self) -> Ptr {
        Ptr(unsafe { ohpl_oal_reverb_create() })
    }
    fn reverb_set(&self, e: Ptr, p: Reverb, v: f32) {
        unsafe { ohpl_oal_reverb_set(e.0, p as i32, v) }
    }
    fn reverb_set_decay_hf_limit(&self, e: Ptr, v: i32) {
        unsafe { ohpl_oal_reverb_set_decay_hf_limit(e.0, v) }
    }
    fn slot_attach(&self, slot: i32, effect: Ptr) -> bool {
        unsafe { ohpl_oal_slot_attach(slot, effect.0) }
    }
    fn slot_set_gain(&self, slot: i32, g: f32) {
        unsafe { ohpl_oal_slot_set_gain(slot, g) }
    }
    fn slot_set_auto_adjust(&self, slot: i32, v: bool) {
        unsafe { ohpl_oal_slot_set_auto_adjust(slot, v) }
    }
}

const _: () =
    assert!(size_of::<RawInit>() == 40 && core::mem::offset_of!(RawInit, use_thread) == 36);
