//! `cOpenALSoundData`, `cOpenALSoundChannel` and `cLowLevelSoundOpenAL`: the OpenAL calls and
//! the objects they hang off. The C++ shells keep the `iSoundData`/`iSoundChannel`/
//! `iLowLevelSound` base-class state and forward here.

use super::env::{EnvParams, blend};
use super::log::{Level, Sink};
use super::oal::{InitParams, OAL_FREE, Oal, Ptr, Reverb};
use crate::string::{first_pos, wide_to_utf8};

pub struct Data {
    stream: bool,
    loop_stream: bool,
    sample: Ptr,
    strm: Ptr,
}

impl Data {
    pub fn new(stream: bool, loop_stream: bool) -> Self {
        Self {
            stream,
            loop_stream,
            sample: Ptr::NULL,
            strm: Ptr::NULL,
        }
    }

    pub fn create_from_file(&mut self, oal: &dyn Oal, path: &[u32], log: Sink) -> bool {
        if self.stream {
            self.strm = oal.stream_load(path);
            if self.strm.is_null() {
                let mut m = b"Couldn't load sound stream '".to_vec();
                m.extend(wide_to_utf8(path));
                m.extend(b"'\n");
                log(Level::Error, &m);
                return false;
            }
            oal.stream_set_loop(self.strm, self.loop_stream);
        } else {
            self.sample = oal.sample_load(path);
            if self.sample.is_null() {
                let mut m = b"Couldn't load sound data '".to_vec();
                m.extend(wide_to_utf8(path));
                m.extend(b"'\n");
                log(Level::Error, &m);
                return false;
            }
            oal.sample_set_loop(self.sample, true);
        }
        true
    }

    /// The source handle of a new paused channel, `None` when the data is unloaded or no source is free.
    pub fn create_channel(&self, oal: &dyn Oal, priority: i32) -> Option<i32> {
        if self.sample.is_null() && self.strm.is_null() {
            return None;
        }
        let handle = if self.stream {
            oal.stream_play(OAL_FREE, self.strm, 1.0, true)
        } else {
            oal.sample_play(OAL_FREE, self.sample, 1.0, true, priority)
        };
        (handle != -1).then_some(handle)
    }

    /// A stream is asked even when it never loaded, as the original did.
    pub fn is_stereo(&self, oal: &dyn Oal) -> bool {
        if self.stream {
            return oal.stream_channels(self.strm) == 2;
        }
        if !self.sample.is_null() {
            return oal.sample_channels(self.sample) == 2;
        }
        false
    }

    pub fn release(self, oal: &dyn Oal) {
        if self.stream {
            if !self.strm.is_null() {
                oal.stream_unload(self.strm);
            }
        } else if !self.sample.is_null() {
            oal.sample_unload(self.sample);
        }
    }
}

pub struct Channel {
    handle: i32,
}

impl Channel {
    pub fn new(oal: &dyn Oal, handle: i32) -> Self {
        let zero = [0.0; 3];
        oal.source_set_attributes(handle, &zero, &zero);
        oal.source_set_filter_type_bandpass(handle);
        oal.source_set_spatialize(handle, false);
        Self { handle }
    }

    pub fn destroy(self, oal: &dyn Oal) {
        if self.handle >= 0 {
            oal.source_stop(self.handle);
        }
    }

    pub fn stop(&mut self, oal: &dyn Oal) {
        oal.source_stop(self.handle);
        self.handle = -1;
    }

    pub fn set_paused(&self, oal: &dyn Oal, p: bool) {
        oal.source_set_paused(self.handle, p);
    }

    pub fn set_pitch(&self, oal: &dyn Oal, v: f32) {
        oal.source_set_pitch(self.handle, v);
    }

    pub fn set_volume(&self, oal: &dyn Oal, v: f32) {
        oal.source_set_gain(self.handle, v);
    }

    pub fn set_loop(&self, oal: &dyn Oal, l: bool) {
        oal.source_set_loop(self.handle, l);
    }

    pub fn set_spatialize(&self, oal: &dyn Oal, v: bool) {
        oal.source_set_spatialize(self.handle, v);
    }

    pub fn set_attributes(&self, oal: &dyn Oal, pos: &[f32; 3], vel: &[f32; 3]) {
        oal.source_set_attributes(self.handle, pos, vel);
    }

    pub fn is_playing(&self, oal: &dyn Oal) -> bool {
        oal.source_is_playing(self.handle)
    }

    pub fn is_buffer_underrun(&self, oal: &dyn Oal) -> bool {
        oal.source_is_buffer_underrun(self.handle)
    }

    pub fn elapsed(&self, oal: &dyn Oal) -> f64 {
        oal.source_get_elapsed(self.handle)
    }

    pub fn total(&self, oal: &dyn Oal) -> f64 {
        oal.source_get_total(self.handle)
    }

    /// Clamps into `[0, total)`; the total is asked once, twice when the time is past it.
    pub fn set_elapsed(&self, oal: &dyn Oal, mut t: f64) {
        if t < 0.0 {
            t = 0.0;
        }
        if t > self.total(oal) {
            t = self.total(oal) - 0.0001;
        }
        oal.source_set_elapsed(self.handle, t);
    }

    /// `alX + modifier`, capped at 255; a negative sum wraps as the unsigned argument did.
    pub fn set_priority(&self, oal: &dyn Oal, x: i32, modifier: i32) {
        let mut p = x.wrapping_add(modifier);
        if p > 255 {
            p = 255;
        }
        oal.source_set_priority(self.handle, p as u32);
    }

    pub fn priority(&self, oal: &dyn Oal) -> u32 {
        oal.source_get_priority(self.handle)
    }

    pub fn set_aux_send_slot(&self, oal: &dyn Oal, affected: bool) {
        oal.source_set_aux_send_slot(self.handle, 0, if affected { 0 } else { -1 });
    }

    pub fn set_filtering(&self, oal: &dyn Oal, on: bool, flags: i32) {
        oal.source_set_filtering(self.handle, on, flags);
    }

    pub fn set_filter_gain(&self, oal: &dyn Oal, g: f32) {
        oal.source_set_filter_gain(self.handle, g);
    }

    pub fn set_filter_gain_hf(&self, oal: &dyn Oal, g: f32) {
        oal.source_set_filter_gain_hf(self.handle, g);
    }

    pub fn set_filter_gain_lf(&self, oal: &dyn Oal, g: f32) {
        oal.source_set_filter_gain_lf(self.handle, g);
    }

    pub fn set_env_send_gain(&self, oal: &dyn Oal, g: f32) {
        oal.source_set_aux_send_gain(self.handle, g);
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Device {
    pub id: i32,
    pub name: Vec<u8>,
    pub default: bool,
}

/// `PopulateAvailableSoundDevices`: the default flag goes to the first device whose name contains
/// the default device name; the default name is only asked while none is flagged.
pub fn enumerate_devices(oal: &dyn Oal) -> Vec<Device> {
    let mut found = false;
    oal.info_output_devices()
        .into_iter()
        .enumerate()
        .map(|(i, name)| {
            let mut default = false;
            if !found && first_pos(&name, &oal.info_default_device()) != -1 {
                found = true;
                default = true;
            }
            Device {
                id: i as i32,
                name,
                default,
            }
        })
        .collect()
}

/// First default device of `valid`, else its first entry.
pub fn first_valid_default(valid: &[Device]) -> Option<&Device> {
    valid.iter().find(|d| d.default).or_else(|| valid.first())
}

/// `GetFirstDefaultDevice`: the valid pick, else the first default of `all`, else its first entry.
pub fn first_default<'a>(all: &'a [Device], valid: &'a [Device]) -> Option<&'a Device> {
    first_valid_default(valid)
        .or_else(|| all.iter().find(|d| d.default))
        .or_else(|| all.first())
}

pub struct InitArgs {
    pub device_id: i32,
    pub use_env_audio: bool,
    pub max_channels: i32,
    pub stream_update_freq: i32,
    pub use_threading: bool,
    pub use_voice_management: bool,
    pub max_mono_hint: i32,
    pub max_stereo_hint: i32,
    pub stream_buffer_size: i32,
    pub stream_buffer_count: i32,
    pub enable_log: bool,
}

pub struct InitResult {
    pub initialized: bool,
    pub device_id: i32,
}

pub struct Low {
    initialized: bool,
    env_enabled: bool,
    effect: Ptr,
}

fn say(log: &mut dyn FnMut(Level, &[u8]), level: Level, parts: &[&[u8]]) {
    log(level, &parts.concat());
}

impl Default for Low {
    fn default() -> Self {
        Self {
            initialized: false,
            env_enabled: false,
            effect: Ptr::NULL,
        }
    }
}

impl Low {
    pub fn new() -> Self {
        Self::default()
    }

    /// The opening lines of `Init`, written before the device lists are asked for.
    pub fn init_prelude(enable_log: bool, log: Sink) {
        log(Level::Log, b" Initializing OpenAL\n");
        if enable_log {
            log(Level::Log, b"  Sound logging enabled\n");
        }
        log(Level::Log, b"  Available OpenAL devices:\n");
    }

    pub fn env_enabled(&self) -> bool {
        self.env_enabled
    }

    pub fn close(&self, oal: &dyn Oal) {
        if self.initialized {
            oal.close();
        }
    }

    pub fn update(&self, oal: &dyn Oal) {
        oal.update();
    }

    pub fn set_listener(
        &self,
        oal: &dyn Oal,
        pos: &[f32; 3],
        vel: &[f32; 3],
        fwd: &[f32; 3],
        up: &[f32; 3],
    ) {
        oal.listener_set_attributes(pos, vel, fwd, up);
    }

    pub fn set_rolloff(&self, oal: &dyn Oal, f: f32) {
        oal.set_rolloff_factor(f);
    }

    pub fn set_volume(&self, oal: &dyn Oal, v: f32) {
        oal.listener_set_master_volume(v);
    }

    /// The clamped volume when the effect slot took it, `None` while environmental audio is off.
    pub fn set_env_volume(&self, oal: &dyn Oal, v: f32) -> Option<f32> {
        if !self.env_enabled {
            return None;
        }
        let v = v.clamp(0.0, 1.0);
        oal.slot_set_gain(0, v);
        Some(v)
    }

    /// Mirrors `cLowLevelSoundOpenAL::Init`. `all` and `valid` are the base class's available
    /// and filtered lists. With no device at all the original indexed an empty vector; that case
    /// leaves the name empty here.
    pub fn init(
        &mut self,
        oal: &dyn Oal,
        a: &InitArgs,
        all: &[Device],
        valid: &[Device],
        env_device: Option<&[u8]>,
        log: Sink,
    ) -> InitResult {
        let log_sounds = a.enable_log;
        let mut p = InitParams {
            device_name: Vec::new(),
            output_freq: 44100,
            update_freq: a.stream_update_freq,
            num_sources_hint: a.max_channels,
            min_mono_hint: a.max_mono_hint,
            min_stereo_hint: a.max_stereo_hint,
            stream_buffer_size: a.stream_buffer_size,
            stream_buffer_count: a.stream_buffer_count,
            use_thread: a.use_threading,
            voice_management: a.use_voice_management,
            use_efx: a.use_env_audio,
        };

        if all.is_empty() {
            log(
                Level::Fatal,
                b"   No sound devices available! Check your OpenAL installation\n",
            );
        }
        for d in all {
            say(
                log,
                Level::Log,
                &[b"   ", d.id.to_string().as_bytes(), b". ", &d.name],
            );
            log(
                Level::Log,
                if d.default {
                    b"(OpenAL default)\n"
                } else {
                    b"\n"
                },
            );
        }

        let mut id = a.device_id;
        if valid.is_empty() {
            log(
                Level::Warning,
                b"  No valid sound devices available! Using implementation default\n",
            );
            id = -2;
        }

        let by_id = usize::try_from(id).ok().and_then(|i| all.get(i));
        if let Some(d) = by_id {
            p.device_name = d.name.clone();
        } else {
            if id == -1 {
                match first_valid_default(valid) {
                    Some(d) => {
                        id = d.id;
                        p.device_name = d.name.clone();
                    }
                    None => id = -2,
                }
            }
            if id == -2
                && let Some(d) = first_default(all, valid)
            {
                id = d.id;
                p.device_name = d.name.clone();
            }
        }

        if let Some(n) = env_device {
            p.device_name = n.to_vec();
        }

        say(
            log,
            Level::Log,
            &[b"  Trying to open device '", &p.device_name, b"'... "],
        );
        let mut file: Vec<u8> = b"HPL_OpenAL_".to_vec();
        file.extend(
            p.device_name
                .iter()
                .map(|&c| if c == b' ' { b'_' } else { c }),
        );
        oal.setup_logging(log_sounds, &file);

        self.initialized = oal.init(&p);

        if !self.initialized {
            log(
                Level::Log,
                b"Failed\n  Finding first device matching filter that works... ",
            );
            for d in valid {
                p.device_name = d.name.clone();
                self.initialized = oal.init(&p);
                if self.initialized {
                    id = d.id;
                    say(log, Level::Log, &[b"  Device name: ", &d.name, b"\n"]);
                    break;
                }
            }
        }

        if !self.initialized {
            log(Level::Log, b"Failed\n  Trying with default device... ");
            for d in all {
                if !d.default {
                    continue;
                }
                p.device_name = d.name.clone();
                self.initialized = oal.init(&p);
                if self.initialized {
                    id = d.id;
                    say(log, Level::Log, &[b"  Device name: ", &d.name, b"\n"]);
                    break;
                }
            }
        }

        if !self.initialized {
            log(
                Level::Log,
                b"Failed\n  Trying to let your OpenAL driver pick the device... ",
            );
            p.device_name.clear();
            self.initialized = oal.init(&p);
            if self.initialized {
                let picked = oal.info_device_name();
                for d in all {
                    if first_pos(&d.name, &picked) != -1 {
                        id = d.id;
                        break;
                    }
                }
                say(log, Level::Log, &[b"  Device name: ", &picked, b"\n"]);
            }
        }

        if !self.initialized {
            log(Level::Fatal, b"Failed! Check your OpenAL installation\n");
            return InitResult {
                initialized: false,
                device_id: id,
            };
        }

        log(Level::Log, b"Success!\n");
        oal.set_distance_model_none();
        say(
            log,
            Level::Log,
            &[
                b"  Number of mono sources: ",
                oal.info_num_sources().to_string().as_bytes(),
                b"\n",
            ],
        );
        say(
            log,
            Level::Log,
            &[
                b"  Streaming setup: ",
                oal.info_stream_buffer_count().to_string().as_bytes(),
                b" Buffers x ",
                oal.info_stream_buffer_size().to_string().as_bytes(),
                b" bytes each\n",
            ],
        );

        if cfg!(windows) && a.use_env_audio {
            self.env_enabled = oal.info_efx_active();
            self.effect = oal.reverb_create();
            if self.env_enabled && !self.effect.is_null() {
                oal.slot_attach(0, self.effect);
                oal.slot_set_auto_adjust(0, false);
                let _ = self.set_env_volume(oal, 1.0);
                log(Level::Log, b"  Setting up Environmental Audio...Success.\n");
            } else {
                log(Level::Log, b"  Setting up Environmental Audio...Failed.\n");
            }
        }

        let zero = [0.0; 3];
        oal.listener_set_attributes(&zero, &zero, &[0.0, 0.0, 1.0], &[0.0, 1.0, 0.0]);

        InitResult {
            initialized: true,
            device_id: id,
        }
    }

    /// `None` env resets every reverb parameter to zero.
    pub fn set_environment(&self, oal: &dyn Oal, env: Option<&EnvParams>) {
        if !self.env_enabled {
            return;
        }
        let e = self.effect;
        let z = EnvParams::default();
        let env_ref = env.unwrap_or(&z);
        for (p, v) in [
            (Reverb::Density, env_ref.density),
            (Reverb::Diffusion, env_ref.diffusion),
            (Reverb::EchoTime, env_ref.echo_time),
            (Reverb::EchoDepth, env_ref.echo_depth),
            (Reverb::ModulationTime, env_ref.modulation_time),
            (Reverb::ModulationDepth, env_ref.modulation_depth),
            (Reverb::Gain, env_ref.gain),
            (Reverb::GainHf, env_ref.gain_hf),
            (Reverb::GainLf, env_ref.gain_lf),
            (Reverb::ReflectionsDelay, env_ref.reflections_delay),
            (Reverb::ReflectionsGain, env_ref.reflections_gain),
            (Reverb::LateReverbGain, env_ref.late_reverb_gain),
            (Reverb::LateReverbDelay, env_ref.late_reverb_delay),
            (Reverb::HfReference, env_ref.hf_reference),
            (Reverb::LfReference, env_ref.lf_reference),
            (Reverb::AirAbsorptionGainHf, env_ref.air_absorption_gain_hf),
            (Reverb::DecayTime, env_ref.decay_time),
            (Reverb::DecayHfRatio, env_ref.decay_hf_ratio),
            (Reverb::DecayLfRatio, env_ref.decay_lf_ratio),
        ] {
            oal.reverb_set(e, p, v);
        }
        oal.reverb_set_decay_hf_limit(e, env_ref.decay_hf_limit);
        oal.reverb_set(e, Reverb::RoomRolloffFactor, env_ref.room_rolloff_factor);
        if env.is_some() {
            oal.slot_attach(0, e);
        }
    }

    pub fn fade_environment(
        &self,
        oal: &dyn Oal,
        src: Option<&EnvParams>,
        dst: Option<&EnvParams>,
        t: f32,
    ) {
        if !self.env_enabled {
            return;
        }
        let t = t.clamp(0.0, 1.0);
        if let Some(e) = blend(src, dst, t) {
            self.set_environment(oal, Some(&e));
        }
    }
}
