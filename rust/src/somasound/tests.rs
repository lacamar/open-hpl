use super::events::{Envelope, Events, category_entry_type, db_to_gain};
use super::fsb;
use super::instance::{Bank, EntityInfo, Filter, Host, Listener, PLAY_3D, PLAY_GUI, PlayReq};
use super::scape::{
    AreaDef, AreaState, ReverbEnv, Scape, ScapeHost, level_from_string, reverb_env,
    reverb_from_string,
};
use super::voice::{UpdateEvent, VoiceHost, Voices, decode_lipsync, decode_text, spectrum};
use std::collections::BTreeMap;

const FDP: &[u8] = br#"<project>
<name>Test</name>
<sounddeffolder>
<name>f</name>
<sounddef><name>d1</name><type>sequential</type><volume_db>-6</volume_db>
<waveform><filename>dir/a.wav</filename><soundbankname>bank</soundbankname></waveform>
<waveform><filename>b.wav</filename><soundbankname>bank</soundbankname></waveform></sounddef>
<sounddef><name>d1</name><waveform><filename>c.wav</filename><soundbankname>bank</soundbankname></waveform></sounddef>
</sounddeffolder>
<eventgroup><name>grp</name>
<event><name>Ev</name><mode>x_3d</mode><category>GuiWorld/Sub</category><oneshot>No</oneshot>
<maxplaybacks>1</maxplaybacks><maxplaybacks_behavior>Steal_newest</maxplaybacks_behavior>
<parameter><name>Speed</name><rangemin>0</rangemin><rangemax>10</rangemax><velocity>0.1</velocity>
<sustainpoint><enabled>1</enabled><value>0.5</value></sustainpoint></parameter>
<layer><controlparameter>Speed</controlparameter>
<sound><name>d1</name><volume>1</volume><loopmode>0</loopmode><x>0</x><width>1</width></sound>
<sound><name>none</name></sound>
<envelope><dsp_name>Volume</dsp_name><dsp_paramindex>0</dsp_paramindex><controlparameter>Speed</controlparameter>
<point>0 0.25</point><point>1,1</point></envelope></layer></event>
</eventgroup></project>"#;

fn loaded() -> Events {
    let mut ev = Events::default();
    assert_eq!(ev.load_fdp(FDP, true, b"Test"), Some((b"Test".to_vec(), 1)));
    ev
}

#[test]
fn fdp_event_fields_and_defaults() {
    let ev = loaded();
    let e = &ev.list[ev.find(b"TEST/grp/ev").unwrap()];
    assert_eq!(e.name, b"Test/grp/Ev");
    assert!(e.is_3d && !e.one_shot);
    assert_eq!(e.category_type, 8);
    assert_eq!(e.max_behavior, 1);
    // missing numeric children read as 0, whatever the defaults say
    assert_eq!((e.pan_level, e.min_dist, e.max_dist), (0.0, 0.0, 0.0));
    assert_eq!(e.params[0].max, 10.0);
    assert_eq!(e.params[0].sustain, 0.5);
    // the duplicate sounddef accumulates its waves
    assert_eq!(e.defs.len(), 1);
    assert_eq!(e.defs[0].waves.len(), 3);
    // ... and its later fields overwrite the earlier ones
    assert!(!e.defs[0].sequential);
    assert_eq!(e.defs[0].volume, db_to_gain(0.0));
    assert_eq!(e.layers[0].sounds.len(), 1);
    assert_eq!(e.layers[0].sounds[0].volume, 1.0);
    assert_eq!(
        e.layers[0].envelopes[0].points,
        vec![[0.0, 0.25], [1.0, 1.0]]
    );
}

#[test]
fn fdp_bank_plan_and_files() {
    let mut ev = loaded();
    let wanted = ev.wanted(b"test");
    assert_eq!(wanted.len(), 1);
    assert_eq!(wanted[b"bank".as_slice()].len(), 3);
    let mut files = BTreeMap::new();
    files.insert(b"bank__a".to_vec(), b"bank__a.ogg".to_vec());
    ev.assign_files(b"test", &files);
    let e = &ev.list[ev.find(b"test/grp/ev").unwrap()];
    assert!(e.loaded && e.has_files());
    assert_eq!(
        e.defs[0].files,
        vec![b"bank__a.ogg".to_vec(), vec![], vec![]]
    );
    // loaded events keep their files
    ev.assign_files(b"test", &BTreeMap::new());
    assert_eq!(ev.list[0].defs[0].files.len(), 3);
}

#[test]
fn project_of_and_file_events() {
    assert_eq!(Events::project_of(b"Proj/a/b"), Some(&b"Proj"[..]));
    assert_eq!(Events::project_of(b"noslash"), None);
    assert_eq!(Events::project_of(b"a/b.ogg"), None);
    assert_eq!(Events::project_of(b""), None);
    let mut ev = Events::default();
    let a = ev.file_event(b"x/Y.ogg", true, false);
    assert_eq!(a, ev.file_event(b"x/y.OGG", true, false));
    assert_ne!(a, ev.file_event(b"x/y.ogg", false, false));
    let e = &ev.list[a];
    assert!(!e.one_shot && e.has_files() && !e.is_3d);
    assert_eq!(e.layers[0].sounds[0].loop_mode, 0);
}

#[test]
fn envelope_eval_interpolates() {
    let e = Envelope {
        param: -1,
        dsp: 0,
        points: vec![[0.2, 1.0], [0.6, 0.0]],
    };
    assert_eq!(e.eval(0.0), 1.0);
    assert_eq!(e.eval(0.4), 0.5);
    assert_eq!(e.eval(0.9), 0.0);
    assert_eq!(Envelope::default().eval(0.3), 1.0);
}

#[test]
fn event_category_decides_the_entry_type() {
    assert_eq!(category_entry_type(8, 1), 8);
    assert_eq!(category_entry_type(-1, 4), 4);
}

#[derive(Default)]
struct Rec {
    next: usize,
    alive: Vec<usize>,
    log: Vec<String>,
    listener: Listener,
    entity: EntityInfo,
}

impl Host for Rec {
    fn valid(&mut self, entry: usize, _id: i32) -> bool {
        self.alive.contains(&entry)
    }
    fn play(&mut self, req: &PlayReq) -> Option<(usize, i32)> {
        self.next += 1;
        self.alive.push(self.next);
        self.log.push(format!(
            "play {} {} {}",
            req.kind,
            String::from_utf8_lossy(req.file),
            req.entry_type
        ));
        Some((self.next, self.next as i32))
    }
    fn stop(&mut self, entry: usize) {
        self.alive.retain(|&e| e != entry);
        self.log.push(format!("stop {entry}"));
    }
    fn pause(&mut self, entry: usize, on: bool) {
        self.log.push(format!("pause {entry} {on}"));
    }
    fn total_time(&mut self, _: usize) -> f64 {
        3.5
    }
    fn set_looping(&mut self, entry: usize, on: bool) {
        self.log.push(format!("loop {entry} {on}"));
    }
    fn set_filter(&mut self, entry: usize, which: Filter, gain: f32) {
        self.log
            .push(format!("filter {entry} {} {gain}", which as i32));
    }
    fn set_position(&mut self, _: usize, _: [f32; 3]) {}
    fn set_default_volume(&mut self, entry: usize, vol: f32) {
        self.log.push(format!("vol {entry} {vol}"));
    }
    fn set_default_speed(&mut self, _: usize, _: f32) {}
    fn listener(&mut self) -> Listener {
        self.listener
    }
    fn entity(&mut self, _: usize) -> EntityInfo {
        self.entity
    }
}

fn bank_with_files() -> Bank {
    let mut bank = Bank::default();
    bank.events = loaded();
    let mut files = BTreeMap::new();
    for s in ["a", "b", "c"] {
        files.insert(
            format!("bank__{s}").into_bytes(),
            format!("bank__{s}.ogg").into_bytes(),
        );
    }
    bank.events.assign_files(b"test", &files);
    bank
}

#[test]
fn instance_plays_and_advances_the_parameter_to_its_sustain_point() {
    let mut bank = bank_with_files();
    let mut host = Rec::default();
    let h = bank.create(0, 0, false, 1);
    assert_eq!(bank.inst(h).unwrap().st.entry_type, 8);
    bank.play(&mut host, h);
    bank.update(&mut host, h, 1.0);
    assert!(
        host.log
            .iter()
            .any(|l| l.starts_with(&format!("play {PLAY_GUI} bank__")))
    );
    let v = bank.inst(h).unwrap().param_value[0];
    assert!(v > 0.0 && v <= 5.0);
    bank.update(&mut host, h, 100.0);
    assert_eq!(bank.inst(h).unwrap().param_value[0], 5.0);
    assert_eq!(bank.param_index(h, b"SPEED"), 0);
    bank.set_param(h, 0, 99.0);
    assert_eq!(bank.inst(h).unwrap().param_value[0], 10.0);
}

#[test]
fn instance_3d_play_uses_the_pan_position() {
    let mut bank = bank_with_files();
    let mut host = Rec {
        listener: Listener {
            pos: [0.0; 3],
            fwd: [0.0, 0.0, -1.0],
            up: [0.0, 1.0, 0.0],
        },
        ..Rec::default()
    };
    bank.events.list[0].min_dist = 1.0;
    bank.events.list[0].max_dist = 20.0;
    let h = bank.create(0, 0, false, 1);
    bank.inst_mut(h).unwrap().st.play_3d = true;
    bank.inst_mut(h).unwrap().st.pos = [3.0, 0.0, -4.0];
    bank.play(&mut host, h);
    bank.update(&mut host, h, 0.5);
    assert!(
        host.log
            .iter()
            .any(|l| l.starts_with(&format!("play {PLAY_3D} ")))
    );
}

#[test]
fn max_playbacks_steals_the_newest() {
    let mut bank = bank_with_files();
    let mut host = Rec::default();
    let a = bank.create(0, 0, false, 1);
    bank.play(&mut host, a);
    assert!(bank.inst(a).unwrap().st.started);
    let b = bank.create(0, 0, false, 1);
    bank.play(&mut host, b);
    // newest of the running instances is a: it stops, b runs
    assert!(bank.inst(a).unwrap().st.stopped);
    assert!(bank.inst(b).unwrap().st.started && !bank.inst(b).unwrap().st.stopped);
}

#[test]
fn fade_out_stops_at_the_end_and_pause_reaches_the_voices() {
    let mut bank = bank_with_files();
    let mut host = Rec::default();
    let h = bank.create(0, 0, false, 1);
    bank.play(&mut host, h);
    bank.update(&mut host, h, 0.1);
    bank.set_paused(&mut host, h, true);
    assert!(
        host.log
            .iter()
            .any(|l| l.starts_with("pause ") && l.ends_with("true"))
    );
    bank.set_paused(&mut host, h, false);
    bank.fade_out(&mut host, h, 0.5);
    bank.update(&mut host, h, 1.0);
    assert!(!bank.inst(h).unwrap().voices.is_empty());
    bank.update(&mut host, h, 1.0);
    assert!(bank.inst(h).unwrap().voices.is_empty());
    assert_eq!(bank.inst(h).unwrap().st.fade, 1.0);
    bank.destroy(&mut host, h);
    assert!(bank.inst(h).is_none());
}

const VOICE: &[u8] = br#"<Voice>
<Characters>
<Character ID="1" Name="Cath" DisplayName="Catherine" Volume="0.5" EntryType="World" EntitySource="catEnt" MinDist="1" MaxDist="9" WorldSpace="true"/>
<Character ID="2" Name="Simon"/>
</Characters>
<Scenes><Scene ID="1" Name="SceneA" FocusPrio="2"/><Scene ID="2" Name="SceneB"/></Scenes>
<Subjects>
<Subject Name="Hello" SceneId="1"><Line CharacterId="1" Callback="onLine" Prio="1"><Sound Text="Hi there" FileName="" VoiceOffset="0.1" Volume="2"/></Line>
<Line CharacterId="2" PlayOnce="true"><Sound Text="Bye" EndPadding="-0.2"/></Line></Subject>
<Subject Name="Rand" SceneId="2" UseSingleRandomLine="true"><Line CharacterId="1" Prio="1"><Sound Text="one"/></Line><Line CharacterId="2" Prio="3"><Sound Text="two"/></Line></Subject>
</Subjects></Voice>"#;

#[derive(Default)]
struct VRec {
    played: Vec<Vec<u8>>,
    log: Vec<String>,
    files: bool,
}

impl VoiceHost for VRec {
    fn entry_valid(&mut self, _: usize, _: i32) -> bool {
        false
    }
    fn entry_set_volume_mul(&mut self, e: usize, v: f32) {
        self.log.push(format!("evol {e} {v}"));
    }
    fn entry_fade_out(&mut self, _: usize, _: f32) {}
    fn entry_set_paused(&mut self, _: usize, _: bool) {}
    fn channel_set_position(&mut self, _: usize, _: [f32; 3]) {}
    fn channel_filter(&mut self, _: usize, _: bool, _: f32) {}
    fn channel_total(&mut self, _: usize) -> f64 {
        1.0
    }
    fn channel_elapsed(&mut self, _: usize) -> f64 {
        0.0
    }
    fn listener_pos(&mut self) -> [f32; 3] {
        [0.0; 3]
    }
    fn map_entity_pos(&mut self, _: &[u8]) -> Option<[f32; 3]> {
        None
    }
    fn map_current(&mut self) -> bool {
        false
    }
    fn file_exists(&mut self, _: &[u8]) -> bool {
        self.files
    }
    fn read_file(&mut self, _: &[u8]) -> Option<Vec<u8>> {
        None
    }
    fn play_3d(
        &mut self,
        _: &[u8],
        _: f32,
        _: [f32; 3],
        _: f32,
        _: f32,
        _: i32,
    ) -> Option<(usize, i32)> {
        None
    }
    fn play_gui_stream(&mut self, _: &[u8], _: f32, _: i32) -> Option<(usize, i32)> {
        None
    }
    fn effect_play(&mut self, _: &[u8], _: f32, _: i32) -> Option<(usize, i32)> {
        None
    }
    fn effect_live(&mut self, _: usize, _: i32) -> bool {
        false
    }
    fn effect_set_volume_mul(&mut self, _: usize, _: f32) {}
    fn effect_fade_out(&mut self, _: usize, _: f32) {}
    fn effect_set_paused(&mut self, _: usize, _: bool) {}
    fn effect_is_playing(&mut self, _: usize) -> bool {
        false
    }
    fn effect_total(&mut self, _: usize) -> f32 {
        0.0
    }
    fn effect_elapsed(&mut self, _: usize) -> f32 {
        0.0
    }
    fn played_has(&mut self, key: &[u8]) -> bool {
        self.played.iter().any(|k| k == key)
    }
    fn played_insert(&mut self, key: &[u8]) -> bool {
        if self.played_has(key) {
            return false;
        }
        self.played.push(key.to_vec());
        true
    }
    fn log(&mut self, text: &[u8]) {
        self.log.push(String::from_utf8_lossy(text).into_owned());
    }
    fn error(&mut self, text: &[u8]) {
        self.log
            .push(format!("error {}", String::from_utf8_lossy(text)));
    }
    fn wrap_rows(&mut self, text: &[u8], _: f32, _: f32, _: [f32; 2]) -> usize {
        text.len().div_ceil(10)
    }
}

fn run_events(v: &mut Voices, host: &mut VRec, dt: f32) -> Vec<String> {
    v.update_begin(host, dt, false);
    let mut out = Vec::new();
    while let Some(e) = v.update_step(host) {
        out.push(match e {
            UpdateEvent::Finish {
                callback,
                scene,
                name,
                on_done,
            } => format!(
                "finish {} {} {} {on_done}",
                String::from_utf8_lossy(&callback),
                String::from_utf8_lossy(&scene),
                String::from_utf8_lossy(&name)
            ),
            UpdateEvent::Line {
                func, line, start, ..
            } => {
                format!("line {} {line} {start}", String::from_utf8_lossy(&func))
            }
        });
    }
    out
}

#[test]
fn voice_scene_runs_its_lines_with_the_reading_time_fallback() {
    let mut v = Voices::new();
    let mut host = VRec::default();
    assert_eq!(v.load_voice(VOICE, b"set"), Some(2));
    assert_eq!(v.play_check(&mut host, b"Hello", 0), 1);
    assert!(v.play_start(&mut host, b"Hello", -1, b"done", 0, 7, false));
    assert!(v.scene_is_active(b"SceneA"));
    assert_eq!(v.play_check(&mut host, b"Hello", 0), 0);
    assert_eq!(v.play_check(&mut host, b"Nope", 0), -1);
    let mut all = Vec::new();
    for _ in 0..200 {
        all.extend(run_events(&mut v, &mut host, 0.1));
        if !v.any_active() {
            break;
        }
    }
    assert!(all.contains(&"line onLine 0 true".to_string()));
    assert!(all.iter().any(|e| e == "finish done SceneA Hello 7"));
    assert!(
        host.log
            .iter()
            .any(|l| l.contains("SceneA_Hello_001_Cath_001 (no audio)"))
    );
}

#[test]
fn voice_play_once_and_single_random_line_selection() {
    let mut v = Voices::new();
    let mut host = VRec::default();
    v.load_voice(VOICE, b"set");
    // the engine tests the first line's PlayOnce for every queued line: line 0 is not PlayOnce
    assert!(v.play_start(&mut host, b"Hello", -1, b"", 0, 0, false));
    assert!(host.played.is_empty());
    assert!(v.stop_next(&mut host, b"SceneA").is_some());
    // the highest Prio line of a single-random subject wins
    assert!(v.play_start(&mut host, b"Rand", -1, b"", 0, 0, false));
    assert_eq!(v.subject_line_count(b"Rand"), 2);
    assert!(v.character_is_speaking(b"Simon") || !v.character_is_speaking(b"Cath"));
    assert!(v.play_start(&mut host, b"Hello", 1, b"", 5, 0, false));
    assert!(!v.play_start(&mut host, b"Hello", 1, b"", 5, 0, false));
}

#[test]
fn voice_subtitle_layout_walks_up_the_screen() {
    let mut v = Voices::new();
    let mut host = VRec::default();
    v.load_voice(VOICE, b"set");
    v.add_subtitle(
        vec![b"abc".to_vec(), b"defg".to_vec()],
        b"N",
        [1.0, 0.0, 0.0, 0.5],
        4,
    );
    let d = v.subtitles(&mut host, 768.0, false);
    assert_eq!(d.len(), 1);
    assert_eq!(d[0].text, b"N: a");
    assert_eq!(d[0].alpha, 0.5);
    assert_eq!(d[0].rows, 1);
    assert_eq!(d[0].y, 700.0);
    assert!(v.subtitles(&mut host, 768.0, false).is_empty());
}

#[test]
fn voice_text_decoding_and_lipsync() {
    let mut utf16 = vec![0xff, 0xfe];
    for c in "<a encoding=\"utf-16\"/>".encode_utf16() {
        utf16.extend_from_slice(&c.to_le_bytes());
    }
    assert_eq!(decode_text(&utf16), b"<a encoding=\"utf-8\"/>");
    assert_eq!(decode_text(b"\xef\xbb\xbfxy"), b"xy");

    // build an .anno: header, one frame, one phoneme; encrypt the first half with TEA
    let mut d = Vec::new();
    d.extend_from_slice(b"ONNA");
    d.extend_from_slice(&1i32.to_le_bytes());
    d.extend_from_slice(&1i32.to_le_bytes());
    d.extend_from_slice(&10i32.to_le_bytes());
    d.extend_from_slice(&50i32.to_le_bytes());
    d.extend_from_slice(&1i32.to_le_bytes());
    d.extend_from_slice(b"AA\0");
    d.extend_from_slice(&0.75f32.to_le_bytes());
    d.extend_from_slice(&[0; 5]);
    let k = [0x8f81_2a90u32, 0x7182_3fad, 0x0317_fab7, 0xc385_6aa5];
    let enc = (d.len() / 2).next_multiple_of(8).min(d.len() / 8 * 8);
    for b in d[..enc].chunks_exact_mut(8) {
        let mut v = [
            u32::from_le_bytes(b[..4].try_into().unwrap()),
            u32::from_le_bytes(b[4..].try_into().unwrap()),
        ];
        let mut sum = 0u32;
        for _ in 0..32 {
            sum = sum.wrapping_add(0x9e37_79b9);
            v[0] = v[0].wrapping_add(
                ((v[1] << 4).wrapping_add(k[0]))
                    ^ v[1].wrapping_add(sum)
                    ^ ((v[1] >> 5).wrapping_add(k[1])),
            );
            v[1] = v[1].wrapping_add(
                ((v[0] << 4).wrapping_add(k[2]))
                    ^ v[0].wrapping_add(sum)
                    ^ ((v[0] >> 5).wrapping_add(k[3])),
            );
        }
        b[..4].copy_from_slice(&v[0].to_le_bytes());
        b[4..].copy_from_slice(&v[1].to_le_bytes());
    }
    let frames = decode_lipsync(&d).unwrap();
    assert_eq!((frames[0].start, frames[0].end), (10, 50));
    assert_eq!(frames[0].w[0], 0.75);
    assert!(decode_lipsync(b"short").is_none());
}

#[test]
fn spectrum_of_a_sine_peaks_at_its_bin() {
    let pcm: Vec<f32> = (0..4096).map(|i| (i as f32 * 0.4).sin()).collect();
    let s = spectrum(&pcm, 1, 48000, 0.05, 32);
    let peak = s
        .iter()
        .cloned()
        .enumerate()
        .max_by(|a, b| a.1.total_cmp(&b.1))
        .unwrap()
        .0;
    assert!(peak > 0 && s[peak] > 0.1);
}

#[derive(Default)]
struct SRec {
    log: Vec<String>,
    areas: Vec<AreaState>,
    cam: [f32; 3],
    live: Vec<(usize, i32)>,
    next: usize,
}

impl ScapeHost for SRec {
    fn bg_live(&mut self, bg: usize, id: i32) -> bool {
        self.live.contains(&(bg, id))
    }
    fn bg_fade_out(&mut self, bg: usize, speed: f32) {
        self.log.push(format!("fadeout {bg} {speed}"));
    }
    fn bg_fade_volume_mul_to(&mut self, bg: usize, dest: f32, speed: f32) {
        self.log.push(format!("volto {bg} {dest} {speed}"));
    }
    fn bg_play(&mut self, name: &[u8]) -> Option<(usize, i32)> {
        self.next += 1;
        self.live.push((self.next, self.next as i32));
        self.log
            .push(format!("play {}", String::from_utf8_lossy(name)));
        Some((self.next, self.next as i32))
    }
    fn bg_set_volume_mul(&mut self, bg: usize, v: f32) {
        self.log.push(format!("vol {bg} {v}"));
    }
    fn bg_fade_in(&mut self, bg: usize, speed: f32) {
        self.log.push(format!("fadein {bg} {speed}"));
    }
    fn bg_set_paused(&mut self, bg: usize, on: bool) {
        self.log.push(format!("paused {bg} {on}"));
    }
    fn world_present(&mut self) -> bool {
        true
    }
    fn entity_fade(&mut self, name: &[u8], fade_in: bool, speed: f32) {
        self.log.push(format!(
            "ent {} {fade_in} {speed}",
            String::from_utf8_lossy(name)
        ));
    }
    fn area(&mut self, ent: usize) -> AreaState {
        self.areas[ent]
    }
    fn camera_pos(&mut self) -> Option<[f32; 3]> {
        Some(self.cam)
    }
    fn body_pos(&mut self) -> Option<[f32; 3]> {
        Some(self.cam)
    }
    fn apply_reverb(&mut self, env: &ReverbEnv) {
        self.log.push(format!("reverb {}", env.decay_time));
    }
}

fn area_def(ent: usize, level: &str, bg: &str) -> AreaDef {
    AreaDef {
        ent,
        level: level.into(),
        bg: bg.into(),
        bg_volume: 0.8,
        bg_fade_in: 2.0,
        bg_trans_speed: 1.0,
        parent_vol_mul: 1.0,
        in_names: b"a b,c".to_vec(),
        ent_fade_in: 1.0,
        ent_fade_out: 1.0,
        prefix: b"[None]".to_vec(),
        reverb_type: b"Cave".to_vec(),
        reverb_amount: 1.0,
        reverb_fade: 1.0,
        use_reverb: true,
        ..AreaDef::default()
    }
}

fn unit_area(centre: f32) -> AreaState {
    let mut m = [0.0; 16];
    for i in 0..4 {
        m[i * 4 + i] = 1.0;
    }
    m[3] = centre;
    AreaState {
        active: true,
        matrix: m,
        size: [2.0, 2.0, 2.0],
    }
}

#[test]
fn soundscape_enters_an_area_plays_its_background_and_blends_the_reverb() {
    let mut s = Scape::default();
    s.load(&[area_def(0, "2:x", "amb/loop")]);
    let mut host = SRec {
        areas: vec![unit_area(0.0)],
        ..SRec::default()
    };
    host.cam = [10.0, 0.0, 0.0];
    s.update(&mut host, 0.1);
    assert!(host.log.is_empty());
    host.cam = [0.5, 0.0, 0.0];
    s.update(&mut host, 0.1);
    assert!(host.log.iter().any(|l| l == "play amb/loop"));
    assert!(
        host.log
            .iter()
            .any(|l| l == "ent a false 1" || l == "ent a true 1")
    );
    assert_eq!(s.rev_type, reverb_from_string(b"Cave"));
    assert!(host.log.iter().any(|l| l.starts_with("reverb")));
    assert!(s.prefix.is_empty());
    host.cam = [10.0, 0.0, 0.0];
    s.update(&mut host, 0.1);
    assert!(host.log.iter().any(|l| l.starts_with("fadeout 1 0.5")));
}

#[test]
fn soundscape_names_and_env_conversion() {
    assert_eq!(level_from_string(b"3:abc"), 3);
    assert_eq!(level_from_string(b"4:abc"), 2);
    assert_eq!(level_from_string(b"1"), 2);
    assert_eq!(reverb_from_string(b"Cave"), 9);
    assert_eq!(reverb_from_string(b"Unknown"), 0);
    let off = reverb_env(&super::presets::PRESETS[0]);
    assert_eq!(off.gain, 1e-5);
    assert_eq!(off.decay_time, 1.0);
    assert!(reverb_env(&super::presets::PRESETS[9]).decay_time > 1.0);
}

#[test]
fn fsb_glue_decisions() {
    assert!(fsb::mode_supported(2) && fsb::mode_supported(7) && fsb::mode_supported(15));
    assert!(!fsb::mode_supported(6));
    assert_eq!(fsb::cached_state(true, true), 1);
    assert_eq!(fsb::cached_state(false, true), 2);
    assert_eq!(fsb::cached_state(false, false), 0);
    assert_eq!(fsb::output_name(b"p__", b"s", 15, 2), b"p__s.ogg");
    assert_eq!(fsb::output_name(b"p__", b"s", 15, 6), b"p__s.wav");
    assert_eq!(fsb::output_name(b"p__", b"s", 2, 1), b"p__s.wav");
    assert_eq!(fsb::wanted_index(b"b", &[b"a", b"b", b"b"]), Some(1));
    assert_eq!(fsb::wanted_index(b"z", &[b"a"]), None);
    let a = [1.0f32, 0.5, -2.0];
    let b = [0.25f32, 0.5, 0.5];
    let mut out = Vec::new();
    fsb::downmix_block(&[&a, &b], &[1.0, 1.0], &[0.0, 1.0], 3, &mut out);
    assert_eq!(out, vec![32767, 8191, 32767, 16383, -32768, 16383]);
}
