use super::log::Level;
use super::oal::{InitParams, Oal, Ptr, Reverb};
use super::snd::{Channel, Data, Device, InitArgs, Low, enumerate_devices, first_default};
use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::path::Path;

fn golden(name: &str) -> std::path::PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("tests/golden/L5")
        .join(name)
}

fn f(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn d(v: f64) -> String {
    format!("{:016x}", v.to_bits())
}

fn v3(v: &[f32; 3]) -> String {
    format!("{},{},{}", f(v[0]), f(v[1]), f(v[2]))
}

fn b(v: bool) -> &'static str {
    if v { "1" } else { "0" }
}

fn p(v: Ptr) -> String {
    if v.is_null() {
        "null".into()
    } else {
        format!("#{}", v.0 as usize)
    }
}

fn w(path: &[u32]) -> String {
    path.iter()
        .map(|&c| char::from_u32(c).unwrap_or('?'))
        .collect()
}

/// The recording OAL of `gen/oal_fake.cpp`, line for line.
struct Fake {
    trace: RefCell<Vec<String>>,
    devices: Vec<Vec<u8>>,
    default: Vec<u8>,
    fail_inits: Cell<i32>,
    play_fail: Cell<bool>,
    next_id: Cell<usize>,
    next_handle: Cell<i32>,
    channels: RefCell<HashMap<usize, i32>>,
}

impl Fake {
    fn new() -> Self {
        Self {
            trace: RefCell::new(Vec::new()),
            devices: vec![
                b"Alpha Speaker".to_vec(),
                b"Beta Headset".to_vec(),
                b"Gamma HDMI".to_vec(),
            ],
            default: b"Beta".to_vec(),
            fail_inits: Cell::new(0),
            play_fail: Cell::new(false),
            next_id: Cell::new(1),
            next_handle: Cell::new(100),
            channels: RefCell::new(HashMap::new()),
        }
    }

    fn t(&self, s: String) {
        self.trace.borrow_mut().push(s);
    }

    fn id(&self) -> usize {
        let i = self.next_id.get();
        self.next_id.set(i + 1);
        i
    }

    fn load(&self, kind: &str, path: &[u32], missing: &str) -> Ptr {
        self.t(format!("{kind}_Load {}", w(path)));
        if w(path) == missing {
            return Ptr::NULL;
        }
        let id = self.id();
        self.channels
            .borrow_mut()
            .insert(id, if w(path).contains("stereo") { 2 } else { 1 });
        Ptr(id as *mut _)
    }

    fn handle(&self) -> i32 {
        if self.play_fail.get() {
            return -1;
        }
        let h = self.next_handle.get();
        self.next_handle.set(h + 1);
        h
    }
}

impl Oal for Fake {
    fn sample_load(&self, path: &[u32]) -> Ptr {
        self.load("Sample", path, "missing.wav")
    }
    fn stream_load(&self, path: &[u32]) -> Ptr {
        self.load("Stream", path, "missing.ogg")
    }
    fn sample_unload(&self, s: Ptr) {
        self.t(format!("Sample_Unload {}", p(s)));
    }
    fn stream_unload(&self, s: Ptr) {
        self.t(format!("Stream_Unload {}", p(s)));
    }
    fn sample_set_loop(&self, s: Ptr, l: bool) {
        self.t(format!("Sample_SetLoop {} {}", p(s), b(l)));
    }
    fn stream_set_loop(&self, s: Ptr, l: bool) {
        self.t(format!("Stream_SetLoop {} {}", p(s), b(l)));
    }
    fn sample_channels(&self, s: Ptr) -> i32 {
        self.t(format!("Sample_GetChannels {}", p(s)));
        self.channels
            .borrow()
            .get(&(s.0 as usize))
            .copied()
            .unwrap_or(0)
    }
    fn stream_channels(&self, s: Ptr) -> i32 {
        self.t(format!("Stream_GetChannels {}", p(s)));
        self.channels
            .borrow()
            .get(&(s.0 as usize))
            .copied()
            .unwrap_or(0)
    }
    fn sample_play(&self, src: i32, s: Ptr, vol: f32, paused: bool, prio: i32) -> i32 {
        self.t(format!(
            "Sample_Play {src} {} {} {} {prio}",
            p(s),
            f(vol),
            b(paused)
        ));
        self.handle()
    }
    fn stream_play(&self, src: i32, s: Ptr, vol: f32, paused: bool) -> i32 {
        self.t(format!(
            "Stream_Play {src} {} {} {}",
            p(s),
            f(vol),
            b(paused)
        ));
        self.handle()
    }
    fn source_stop(&self, s: i32) {
        self.t(format!("Source_Stop {s}"));
    }
    fn source_set_paused(&self, s: i32, v: bool) {
        self.t(format!("Source_SetPaused {s} {}", b(v)));
    }
    fn source_set_gain(&self, s: i32, g: f32) {
        self.t(format!("Source_SetGain {s} {}", f(g)));
    }
    fn source_set_pitch(&self, s: i32, g: f32) {
        self.t(format!("Source_SetPitch {s} {}", f(g)));
    }
    fn source_set_loop(&self, s: i32, l: bool) {
        self.t(format!("Source_SetLoop {s} {}", b(l)));
    }
    fn source_set_attributes(&self, s: i32, pos: &[f32; 3], vel: &[f32; 3]) {
        self.t(format!("Source_SetAttributes {s} {} {}", v3(pos), v3(vel)));
    }
    fn source_set_spatialize(&self, s: i32, v: bool) {
        self.t(format!("Source_SetSpatialize {s} {}", b(v)));
    }
    fn source_set_priority(&self, s: i32, pr: u32) {
        self.t(format!("Source_SetPriority {s} {pr}"));
    }
    fn source_get_priority(&self, s: i32) -> u32 {
        self.t(format!("Source_GetPriority {s}"));
        3
    }
    fn source_is_playing(&self, s: i32) -> bool {
        self.t(format!("Source_IsPlaying {s}"));
        true
    }
    fn source_is_buffer_underrun(&self, s: i32) -> bool {
        self.t(format!("Source_IsBufferUnderrun {s}"));
        false
    }
    fn source_get_elapsed(&self, s: i32) -> f64 {
        self.t(format!("Source_GetElapsedTime {s}"));
        0.5
    }
    fn source_get_total(&self, s: i32) -> f64 {
        self.t(format!("Source_GetTotalTime {s}"));
        2.0
    }
    fn source_set_elapsed(&self, s: i32, t: f64) {
        self.t(format!("Source_SetElapsedTime {s} {}", d(t)));
    }
    fn source_set_filter_type_bandpass(&self, s: i32) {
        self.t(format!("Source_SetFilterType {s} 2"));
    }
    fn source_set_aux_send_slot(&self, s: i32, send: i32, slot: i32) {
        self.t(format!("Source_SetAuxSendSlot {s} {send} {slot}"));
    }
    fn source_set_filtering(&self, s: i32, on: bool, flags: i32) {
        self.t(format!("Source_SetFiltering {s} {} {flags}", b(on)));
    }
    fn source_set_filter_gain(&self, s: i32, g: f32) {
        self.t(format!("Source_SetFilterGain {s} {}", f(g)));
    }
    fn source_set_filter_gain_hf(&self, s: i32, g: f32) {
        self.t(format!("Source_SetFilterGainHF {s} {}", f(g)));
    }
    fn source_set_filter_gain_lf(&self, s: i32, g: f32) {
        self.t(format!("Source_SetFilterGainLF {s} {}", f(g)));
    }
    fn source_set_aux_send_gain(&self, s: i32, g: f32) {
        self.t(format!("Source_SetAuxSendGain {s} {}", f(g)));
    }
    fn listener_set_attributes(
        &self,
        pos: &[f32; 3],
        vel: &[f32; 3],
        fwd: &[f32; 3],
        up: &[f32; 3],
    ) {
        self.t(format!(
            "Listener_SetAttributes {} {} {} {}",
            v3(pos),
            v3(vel),
            v3(fwd),
            v3(up)
        ));
    }
    fn listener_set_master_volume(&self, v: f32) {
        self.t(format!("Listener_SetMasterVolume {}", f(v)));
    }
    fn init(&self, pr: &InitParams) -> bool {
        let ok = self.fail_inits.get() <= 0;
        if !ok {
            self.fail_inits.set(self.fail_inits.get() - 1);
        }
        self.t(format!(
            "Init '{}' {} {} {} {} {} {} {} {}{}{} -> {}",
            String::from_utf8_lossy(&pr.device_name),
            pr.output_freq,
            pr.update_freq,
            pr.num_sources_hint,
            pr.min_mono_hint,
            pr.min_stereo_hint,
            pr.stream_buffer_size,
            pr.stream_buffer_count,
            b(pr.use_thread),
            b(pr.voice_management),
            b(pr.use_efx),
            b(ok)
        ));
        ok
    }
    fn close(&self) {
        self.t("Close".into());
    }
    fn update(&self) {
        self.t("Update".into());
    }
    fn set_rolloff_factor(&self, v: f32) {
        self.t(format!("SetRollOffFactor {}", f(v)));
    }
    fn set_distance_model_none(&self) {
        self.t("SetDistanceModel 6".into());
    }
    fn setup_logging(&self, on: bool, name: &[u8]) {
        self.t(format!(
            "SetupLogging {} 0 3 {}",
            b(on),
            String::from_utf8_lossy(name)
        ));
    }
    fn info_device_name(&self) -> Vec<u8> {
        self.t("Info_GetDeviceName".into());
        self.devices.get(1).cloned().unwrap_or_default()
    }
    fn info_num_sources(&self) -> i32 {
        self.t("Info_GetNumSources".into());
        32
    }
    fn info_efx_active(&self) -> bool {
        self.t("Info_IsEFXActive".into());
        false
    }
    fn info_stream_buffer_count(&self) -> i32 {
        self.t("Info_GetStreamBufferCount".into());
        4
    }
    fn info_stream_buffer_size(&self) -> i32 {
        self.t("Info_GetStreamBufferSize".into());
        8192
    }
    fn info_default_device(&self) -> Vec<u8> {
        self.t("Info_GetDefaultOutputDevice".into());
        self.default.clone()
    }
    fn info_output_devices(&self) -> Vec<Vec<u8>> {
        self.t("Info_GetOutputDevices".into());
        self.devices.clone()
    }
    fn reverb_create(&self) -> Ptr {
        self.t("Reverb_Create".into());
        Ptr(self.id() as *mut _)
    }
    fn reverb_set(&self, e: Ptr, pr: Reverb, v: f32) {
        self.t(format!("Reverb_Set{pr:?} {} {}", p(e), f(v)));
    }
    fn reverb_set_decay_hf_limit(&self, e: Ptr, v: i32) {
        self.t(format!("Reverb_SetDecayHFLimit {} {}", p(e), v as u8));
    }
    fn slot_attach(&self, slot: i32, e: Ptr) -> bool {
        self.t(format!("Slot_Attach {slot} {}", p(e)));
        true
    }
    fn slot_set_gain(&self, slot: i32, g: f32) {
        self.t(format!("Slot_SetGain {slot} {}", f(g)));
    }
    fn slot_set_auto_adjust(&self, slot: i32, v: bool) {
        self.t(format!("Slot_SetAutoAdjust {slot} {}", b(v)));
    }
}

fn wide(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn logger(fake: &Fake) -> impl FnMut(Level, &[u8]) + '_ {
    move |level, msg| {
        let prefix = match level {
            Level::Log => "",
            Level::Warning => "WARNING: ",
            Level::Error => "ERROR: ",
            Level::Fatal => "FATAL ERROR: ",
        };
        let text = format!("{prefix}{}", String::from_utf8_lossy(msg)).replace('\n', "\\n");
        fake.t(format!("LOG {} {text}", level as i32));
    }
}

/// `iSoundChannel` base-class state the C++ shell passes down.
struct ChannelShell {
    ch: Channel,
    relative: bool,
    three_d: bool,
    pos: [f32; 3],
    vel: [f32; 3],
    modifier: i32,
}

fn channel_ops(o: &Fake, mut c: ChannelShell) {
    c.ch.set_paused(o, false);
    c.ch.set_pitch(o, 1.5);
    c.ch.set_volume(o, 0.8);
    c.ch.set_loop(o, true);
    c.three_d = true;
    c.ch.set_spatialize(o, c.three_d && !c.relative);
    c.relative = true;
    c.ch.set_spatialize(o, c.three_d && !c.relative);
    c.pos = [1.0, 2.0, 3.0];
    c.ch.set_attributes(o, &c.pos, &c.vel);
    c.vel = [4.0, 5.0, 6.0];
    c.ch.set_attributes(o, &c.pos, &c.vel);
    c.relative = false;
    c.ch.set_spatialize(o, c.three_d && !c.relative);
    c.ch.set_paused(o, true);
    c.ch.is_playing(o);
    c.ch.is_buffer_underrun(o);
    c.ch.elapsed(o);
    c.ch.total(o);
    c.ch.set_elapsed(o, -1.0);
    c.ch.set_elapsed(o, 1.5);
    c.ch.set_elapsed(o, 5.0);
    c.ch.set_priority(o, 7, c.modifier);
    c.modifier = 300;
    let current = c.ch.priority(o) as i32;
    c.ch.set_priority(o, current, c.modifier);
    c.ch.priority(o);
    c.ch.set_aux_send_slot(o, true);
    c.ch.set_aux_send_slot(o, false);
    c.ch.set_filtering(o, true, 3);
    c.ch.set_filter_gain(o, 0.5);
    c.ch.set_filter_gain_hf(o, 0.25);
    c.ch.set_filter_gain_lf(o, 0.125);
    c.ch.set_env_send_gain(o, 0.75);
    c.ch.stop(o);
    c.ch.set_volume(o, 0.1);
    c.ch.destroy(o);
}

fn new_channel(o: &Fake, h: i32) -> ChannelShell {
    ChannelShell {
        ch: Channel::new(o, h),
        relative: false,
        three_d: false,
        pos: [0.0; 3],
        vel: [0.0; 3],
        modifier: 0,
    }
}

struct Devices {
    all: Vec<Device>,
}

fn filtered(all: &[Device], filter: &str) -> Vec<Device> {
    let lf = filter.to_lowercase();
    all.iter()
        .filter(|d| {
            String::from_utf8_lossy(&d.name)
                .to_lowercase()
                .contains(&lf)
        })
        .cloned()
        .collect()
}

fn init_variant(
    o: &Fake,
    devs: &mut Devices,
    label: &str,
    dev: i32,
    fails: i32,
    filter: &str,
    env: Option<&[u8]>,
) {
    o.t(format!("== {label}"));
    o.fail_inits.set(fails);
    let mut low = Low::new();
    let mut log = logger(o);
    Low::init_prelude(false, &mut log);
    devs.all = enumerate_devices(o);
    let valid = filtered(&devs.all, filter);
    let args = InitArgs {
        device_id: dev,
        use_env_audio: false,
        max_channels: 32,
        stream_update_freq: 100,
        use_threading: true,
        use_voice_management: true,
        max_mono_hint: 16,
        max_stereo_hint: 8,
        stream_buffer_size: 8192,
        stream_buffer_count: 4,
        enable_log: false,
    };
    let r = low.init(o, &args, &devs.all, &valid, env, &mut log);
    if r.initialized {
        low.set_volume(o, 1.0);
    }
    low.close(o);
}

fn scenario() -> Vec<String> {
    let o = Fake::new();
    let mut devs = Devices { all: Vec::new() };
    o.t("== main".into());
    {
        let mut low = Low::new();
        let mut log = logger(&o);
        Low::init_prelude(true, &mut log);
        devs.all = enumerate_devices(&o);
        let valid = filtered(&devs.all, "");
        let args = InitArgs {
            device_id: -1,
            use_env_audio: false,
            max_channels: 32,
            stream_update_freq: 100,
            use_threading: true,
            use_voice_management: true,
            max_mono_hint: 16,
            max_stereo_hint: 8,
            stream_buffer_size: 8192,
            stream_buffer_count: 4,
            enable_log: true,
        };
        low.init(&o, &args, &devs.all, &valid, None, &mut log);
        low.set_volume(&o, 1.0);
        low.set_volume(&o, 0.5);
        let up = [0.0, 1.0, 0.0];
        low.set_listener(
            &o,
            &[1.0, 2.0, 3.0],
            &[0.5, 0.0, 0.0],
            &[-0.0, -0.0, -1.0],
            &up,
        );
        low.set_listener(
            &o,
            &[4.0, 5.0, 6.0],
            &[0.5, 0.0, 0.0],
            &[-0.0, -0.0, -1.0],
            &up,
        );
        low.set_rolloff(&o, 0.7);
        low.update(&o);
        assert_eq!(low.set_env_volume(&o, 0.5), None);
        low.set_environment(&o, None);
        low.fade_environment(&o, None, None, 0.5);

        o.t("== sample".into());
        let mut d1 = Data::new(false, false);
        d1.create_from_file(&o, &wide("snd/a_stereo.wav"), &mut log);
        d1.is_stereo(&o);
        let h = d1.create_channel(&o, 5).unwrap_or(-1);
        channel_ops(&o, new_channel(&o, h));
        let h = d1.create_channel(&o, 9).unwrap_or(-1);
        new_channel(&o, h).ch.destroy(&o);
        d1.release(&o);

        o.t("== stream".into());
        let mut d2 = Data::new(true, true);
        d2.create_from_file(&o, &wide("music/m.ogg"), &mut log);
        d2.is_stereo(&o);
        let h = d2.create_channel(&o, 1).unwrap_or(-1);
        channel_ops(&o, new_channel(&o, h));
        o.play_fail.set(true);
        let none = d2.create_channel(&o, 1);
        o.play_fail.set(false);
        o.t(if none.is_some() {
            "channel"
        } else {
            "no channel"
        }
        .into());
        d2.release(&o);

        o.t("== missing".into());
        let mut d3 = Data::new(false, false);
        let ok3 = d3.create_from_file(&o, &wide("missing.wav"), &mut log);
        d3.release(&o);
        let mut d4 = Data::new(true, false);
        let ok4 = d4.create_from_file(&o, &wide("missing.ogg"), &mut log);
        d4.release(&o);
        o.t(if ok3 || ok4 { "loaded" } else { "both null" }.into());
        low.close(&o);
    }
    init_variant(&o, &mut devs, "fail1", -1, 1, "", None);
    init_variant(&o, &mut devs, "fail2", 1, 2, "", None);
    init_variant(&o, &mut devs, "fail3", -2, 3, "", None);
    init_variant(&o, &mut devs, "fail4", 0, 4, "", None);
    init_variant(&o, &mut devs, "fail5", 0, 5, "", None);
    init_variant(&o, &mut devs, "filter", -1, 0, "gam", None);
    init_variant(&o, &mut devs, "nofilter", -1, 0, "zzz", None);
    init_variant(&o, &mut devs, "env", 2, 0, "", Some(b"Gamma HDMI"));
    o.trace.into_inner()
}

#[test]
fn oal_trace_matches_cpp() {
    let want = std::fs::read_to_string(golden("oal_trace.txt")).unwrap();
    let got = scenario().join("\n") + "\n";
    if want != got {
        for (i, (a, b)) in want.lines().zip(got.lines()).enumerate() {
            assert_eq!(a, b, "line {}", i + 1);
        }
        assert_eq!(want.lines().count(), got.lines().count());
    }
}

#[test]
fn first_default_prefers_valid() {
    let dev = |id, default| Device {
        id,
        name: vec![],
        default,
    };
    let all = [dev(0, false), dev(1, true)];
    assert_eq!(first_default(&all, &[]).map(|d| d.id), Some(1));
    assert_eq!(first_default(&all, &all[..1]).map(|d| d.id), Some(0));
    assert_eq!(first_default(&[], &[]), None);
}

fn fixture(name: &str) -> std::path::PathBuf {
    golden("fixtures").join(name)
}

fn env_line(name: &str) -> String {
    let rel = format!("fixtures/{name}");
    let Some(l) = std::fs::read(fixture(name))
        .ok()
        .and_then(|d| super::env::parse(&d))
    else {
        return format!("{rel} none");
    };
    let e = l.params;
    let fl = [
        e.density,
        e.diffusion,
        e.gain,
        e.gain_hf,
        e.gain_lf,
        e.decay_time,
        e.decay_hf_ratio,
        e.decay_lf_ratio,
        e.reflections_gain,
        e.reflections_delay,
        e.reflections_pan[0],
        e.reflections_pan[1],
        e.reflections_pan[2],
        e.late_reverb_gain,
        e.late_reverb_delay,
        e.late_reverb_pan[0],
        e.late_reverb_pan[1],
        e.late_reverb_pan[2],
        e.echo_time,
        e.echo_depth,
        e.modulation_time,
        e.modulation_depth,
        e.air_absorption_gain_hf,
        e.hf_reference,
        e.lf_reference,
        e.room_rolloff_factor,
    ];
    let hex: Vec<String> = fl.iter().map(|&v| f(v)).collect();
    format!(
        "{rel} name={} {} {}",
        String::from_utf8_lossy(&l.name),
        hex.join(" "),
        e.decay_hf_limit
    )
}

#[test]
fn env_files_match_cpp() {
    let got: Vec<String> = ["env_cave.xml", "env_fmod.xml", "missing.xml"]
        .iter()
        .map(|n| env_line(n))
        .collect();
    let want = std::fs::read_to_string(golden("env.txt")).unwrap();
    assert_eq!(want.lines().collect::<Vec<_>>(), got);
}

fn font_dump(name: &str) -> String {
    use super::font::{Error, parse};
    let rel = format!("fixtures/{name}");
    let Ok(data) = std::fs::read(fixture(name)) else {
        return format!("{rel} open\n");
    };
    match parse(&data) {
        Err(Error::Parse) => format!("{rel} parse\n"),
        Err(Error::Lacks) => format!("{rel} lacks\n"),
        Err(Error::Unreasonable(id)) => format!("{rel} unreasonable {id}\n"),
        Ok(font) => {
            let h = font.header;
            let mut out = format!(
                "{rel} ok lh={} base={} h={} r={} largest={} count={}\n",
                h.line_height,
                h.base,
                f(h.height),
                f(h.size_ratio_x),
                font.largest_id,
                font.glyph_count
            );
            for pg in &font.pages {
                out += &format!(" page {}\n", String::from_utf8_lossy(pg));
            }
            for g in &font.glyphs {
                out += &format!(
                    " char {} {} {} {} {} {} {} {} {} st={}\n",
                    g.id,
                    g.x,
                    g.y,
                    g.width,
                    g.height,
                    g.x_offset,
                    g.y_offset,
                    g.advance,
                    g.page,
                    g.status as i32
                );
            }
            out
        }
    }
}

#[test]
fn font_files_match_cpp() {
    let got: String = [
        "font_sample.fnt",
        "font_lacks.fnt",
        "font_big.fnt",
        "font_missing.fnt",
    ]
    .iter()
    .map(|n| font_dump(n))
    .collect();
    assert_eq!(std::fs::read_to_string(golden("font.txt")).unwrap(), got);
}

fn install_root() -> Option<std::path::PathBuf> {
    let r = std::env::var_os("OHPL_L5_ROOT")
        .map(std::path::PathBuf::from)
        .or_else(|| {
            std::env::var_os("HOME")
                .map(|h| std::path::PathBuf::from(h).join(".local/share/Steam/steamapps/common"))
        })?;
    r.is_dir().then_some(r)
}

fn fnv(mut h: u64, bytes: &[u8]) -> u64 {
    for &b in bytes {
        h ^= u64::from(b);
        h = h.wrapping_mul(0x0000_0100_0000_01b3);
    }
    h
}

fn bitmap_hash(b: &super::bitmap::Bitmap) -> u64 {
    let meta = [
        b.width,
        b.height,
        b.depth,
        b.bytes_per_pixel,
        b.format as i32,
        i32::from(b.compressed),
        b.images,
        b.mips,
    ];
    let mut h = 0xcbf2_9ce4_8422_2325u64;
    for m in meta {
        h = fnv(h, &m.to_le_bytes());
    }
    for plane in &b.planes {
        h = fnv(h, &(plane.len() as i32).to_le_bytes());
        h = fnv(h, plane);
    }
    h
}

/// Replays the sampled census recorded from the C++ loaders; needs the installs (read-only).
#[test]
fn bitmaps_match_cpp_census() {
    let Some(root) = install_root() else {
        return;
    };
    let golden_lines = std::fs::read_to_string(golden("bitmaps.txt")).unwrap();
    let mut log = |_: Level, _: &[u8]| {};
    let (mut checked, mut missing) = (0, 0);
    for line in golden_lines.lines() {
        let (rel, want) = line.rsplit_once(' ').unwrap();
        let path = root.join(rel);
        if !path.exists() {
            missing += 1;
            continue;
        }
        let wide: Vec<u32> = path.to_string_lossy().chars().map(u32::from).collect();
        let bm = if rel.to_lowercase().ends_with(".dds") {
            super::bitmap::load_dds(&wide, false, &mut log)
        } else {
            super::bitmap::load_misc(&wide, &mut log)
        };
        let got = bm.map_or("none".to_string(), |b| format!("{:016x}", bitmap_hash(&b)));
        assert_eq!(want, got, "{rel}");
        checked += 1;
    }
    eprintln!("bitmaps checked {checked}, absent {missing}");
}

#[test]
fn wavs_match_cpp_census() {
    let Some(root) = install_root() else {
        return;
    };
    for line in std::fs::read_to_string(golden("wav.txt")).unwrap().lines() {
        let (rel, want) = line.rsplit_once(' ').unwrap();
        let Ok(data) = std::fs::read(root.join(rel)) else {
            continue;
        };
        let got = super::audio::decode_wav(&data).map_or("none".to_string(), |p| {
            format!("{:016x}", super::audio::hash(&p))
        });
        assert_eq!(want, got, "{rel}");
    }
}

#[test]
fn wav_rejects_what_oalwrapper_rejects() {
    use super::audio::decode_wav;
    let mut w = b"RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x02\0\x44\xac\0\0\0\0\0\0\0\0\x10\0data\x04\0\0\0abcd".to_vec();
    let ok = decode_wav(&w).unwrap();
    assert_eq!(
        (
            ok.channels,
            ok.frequency,
            ok.bits,
            ok.samples,
            ok.data.as_slice()
        ),
        (2, 44100, 16, 1, &b"abcd"[..])
    );
    assert!(decode_wav(&w[..w.len() - 1]).is_none());
    w[20] = 3;
    assert!(decode_wav(&w).is_none());
    assert!(decode_wav(b"RIFX").is_none());
}
