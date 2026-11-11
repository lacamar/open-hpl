//! `cSomaLuxVoiceHandler`: voice scene state machine, focus and scene volumes, subtitle timing and
//! layout, lip sync data. Script calls, sound entries and drawing stay C++; anything that can
//! re-enter the handler (script and completion callbacks) is handed back to the caller as an
//! event between steps.

use super::{std_max, std_min};
use crate::math::{clamp, fma, rand_rectl};
use crate::string::{int_to_string, to_bool, to_lower, token_to_float, token_to_int, wide_to_utf8};
use crate::xml::{DOC, Doc, NodeId, c_str};
use std::collections::{BTreeMap, BTreeSet};
use std::rc::Rc;

pub const VISEMES: usize = 17;

#[derive(Clone, Default)]
pub struct Sound {
    pub text: Vec<u8>,
    pub file: Vec<u8>,
    pub voice_offset: f32,
    pub text_offset: f32,
    pub end_padding: f32,
    pub volume: f32,
    pub effect: Vec<u8>,
    pub ends_after_effect: bool,
}

#[derive(Clone)]
pub struct Line {
    pub character: Vec<u8>,
    pub display_name: Vec<u8>,
    pub callback: Vec<u8>,
    pub char_volume: f32,
    pub entry_type: i32,
    pub source: Vec<u8>,
    pub min_dist: f32,
    pub max_dist: f32,
    pub world_space: bool,
    pub change_source: bool,
    pub prio: i32,
    pub play_once: bool,
    pub sounds: Vec<Sound>,
}

impl Default for Line {
    fn default() -> Self {
        Self {
            character: Vec::new(),
            display_name: Vec::new(),
            callback: Vec::new(),
            char_volume: 1.0,
            entry_type: 8,
            source: Vec::new(),
            min_dist: 0.0,
            max_dist: 0.0,
            world_space: false,
            change_source: false,
            prio: 0,
            play_once: false,
            sounds: Vec::new(),
        }
    }
}

pub struct Subject {
    pub name: Vec<u8>,
    pub scene: Vec<u8>,
    pub set: Vec<u8>,
    pub single_random_line: bool,
    pub last_line: i32,
    pub lines: Vec<Line>,
}

#[derive(Clone, Copy)]
pub struct LipFrame {
    pub start: i32,
    pub end: i32,
    pub w: [f32; VISEMES],
}

#[derive(Clone)]
struct Source {
    entity: Vec<u8>,
    min_dist: f32,
    max_dist: f32,
    use_3d: bool,
    listen_range: f32,
    min_freq: f32,
    max_freq: f32,
    freq_flags: i32,
}

#[derive(Clone, Copy)]
struct SceneVolume {
    volume: f32,
    goal: f32,
    speed: f32,
    focus: f32,
}

impl Default for SceneVolume {
    fn default() -> Self {
        Self {
            volume: 1.0,
            goal: 1.0,
            speed: 0.0,
            focus: 1.0,
        }
    }
}

type Entry = (usize, i32);

struct Playing {
    subject: usize,
    prio: i32,
    callback: Vec<u8>,
    on_done: u64,
    lines: Vec<i32>,
    line: usize,
    sound: usize,
    time: f32,
    step: i32,
    entry: usize,
    entry_id: i32,
    fallback: f32,
    text_delay: f32,
    subtitle: Vec<u8>,
    paused: bool,
    play_while_paused: bool,
    source_entity: Vec<u8>,
    source_min: f32,
    source_max: f32,
    listen_range: f32,
    min_freq: f32,
    max_freq: f32,
    source_dist: f32,
    freq_flags: i32,
    file: Vec<u8>,
    lip_entity: Vec<u8>,
    lipsync: Option<Rc<Vec<LipFrame>>>,
    effect: usize,
    effect_id: i32,
    old_voices: Vec<Entry>,
    old_effects: Vec<Entry>,
}

/// The sound handler, the map and the sound events behind the voice scenes.
pub trait VoiceHost {
    fn entry_valid(&mut self, entry: usize, id: i32) -> bool;
    fn entry_set_volume_mul(&mut self, entry: usize, vol: f32);
    fn entry_fade_out(&mut self, entry: usize, speed: f32);
    fn entry_set_paused(&mut self, entry: usize, on: bool);
    fn channel_set_position(&mut self, entry: usize, pos: [f32; 3]);
    fn channel_filter(&mut self, entry: usize, high: bool, gain: f32);
    fn channel_total(&mut self, entry: usize) -> f64;
    fn channel_elapsed(&mut self, entry: usize) -> f64;
    fn listener_pos(&mut self) -> [f32; 3];
    fn map_entity_pos(&mut self, name: &[u8]) -> Option<[f32; 3]>;
    fn map_current(&mut self) -> bool;
    fn file_exists(&mut self, file: &[u8]) -> bool;
    fn read_file(&mut self, file: &[u8]) -> Option<Vec<u8>>;
    fn play_3d(
        &mut self,
        file: &[u8],
        vol: f32,
        pos: [f32; 3],
        min: f32,
        max: f32,
        ty: i32,
    ) -> Option<Entry>;
    fn play_gui_stream(&mut self, file: &[u8], vol: f32, ty: i32) -> Option<Entry>;
    fn effect_play(&mut self, file: &[u8], vol: f32, ty: i32) -> Option<Entry>;
    fn effect_live(&mut self, effect: usize, id: i32) -> bool;
    fn effect_set_volume_mul(&mut self, effect: usize, vol: f32);
    fn effect_fade_out(&mut self, effect: usize, speed: f32);
    fn effect_set_paused(&mut self, effect: usize, on: bool);
    fn effect_is_playing(&mut self, effect: usize) -> bool;
    fn effect_total(&mut self, effect: usize) -> f32;
    fn effect_elapsed(&mut self, effect: usize) -> f32;
    fn played_has(&mut self, key: &[u8]) -> bool;
    fn played_insert(&mut self, key: &[u8]) -> bool;
    fn log(&mut self, text: &[u8]);
    fn error(&mut self, text: &[u8]);
    fn wrap_rows(&mut self, text: &[u8], width: f32, line_height: f32, size: [f32; 2]) -> usize;
}

pub enum UpdateEvent {
    Finish {
        callback: Vec<u8>,
        scene: Vec<u8>,
        name: Vec<u8>,
        on_done: u64,
    },
    Line {
        func: Vec<u8>,
        scene: Vec<u8>,
        name: Vec<u8>,
        line: i32,
        start: bool,
    },
}

pub struct StopEvent {
    pub on_done: u64,
}

/// One subtitle to draw: where its first row goes and how many rows it wraps to.
pub struct SubtitleDraw {
    pub text: Vec<u8>,
    pub alpha: f32,
    pub color: Option<[f32; 4]>,
    pub y: f32,
    pub rows: usize,
}

pub struct ImmediateSubtitle {
    pub rows: Vec<Vec<u8>>,
    pub name: Vec<u8>,
    pub color: [f32; 4],
    pub drawn_chars: i32,
}

#[derive(Default)]
pub struct Voices {
    subjects: Vec<Subject>,
    by_name: BTreeMap<Vec<u8>, usize>,
    text: BTreeMap<Vec<u8>, Vec<u8>>,
    loaded_sets: Vec<Vec<u8>>,
    playing: Vec<Playing>,
    sources: BTreeMap<Vec<u8>, Source>,
    scene_volumes: BTreeMap<Vec<u8>, SceneVolume>,
    scene_focus_prio: BTreeMap<Vec<u8>, i32>,
    focus_scene: Vec<u8>,
    forced_focus_scene: Vec<u8>,
    focus_timer: f32,
    immediate: Vec<ImmediateSubtitle>,
    speaking_callbacks: BTreeMap<Vec<u8>, Vec<u8>>,
    speaking: BTreeSet<Vec<u8>>,
    pub fade_alpha: f32,
    pub fade_goal: f32,
    fade_speed: f32,
    upd_i: usize,
    upd_dt: f32,
    upd_paused: bool,
}

/// `ReadTextFile`: `.voice` and some `.lang` files are UTF-16 with a BOM.
pub fn decode_text(s: &[u8]) -> Vec<u8> {
    let mut out: Vec<u8> = if s.starts_with(&[0xff, 0xfe]) {
        let w: Vec<u32> = s
            .get(2..)
            .unwrap_or_default()
            .chunks_exact(2)
            .map(|c| {
                u32::from(u16::from_le_bytes([
                    c.first().copied().unwrap_or(0),
                    c.get(1).copied().unwrap_or(0),
                ]))
            })
            .collect();
        wide_to_utf8(&w)
    } else if s.starts_with(&[0xef, 0xbb, 0xbf]) {
        s.get(3..).unwrap_or_default().to_vec()
    } else {
        s.to_vec()
    };
    let needle = b"encoding=\"utf-16\"";
    if let Some(p) = out.windows(needle.len()).position(|w| w == needle) {
        out.splice(p..p + needle.len(), *b"encoding=\"utf-8\"");
    }
    out
}

fn attr<'a>(doc: &'a Doc, e: NodeId, name: &[u8]) -> Option<&'a [u8]> {
    if name == b"_Text"
        && let Some(t) = doc.text(e)
    {
        return Some(t);
    }
    doc.attrs(e)
        .filter(|a| c_str(&a.name) == name)
        .last()
        .map(|a| c_str(&a.value))
}

fn a_str(doc: &Doc, e: NodeId, name: &[u8], default: &[u8]) -> Vec<u8> {
    attr(doc, e, name).unwrap_or(default).to_vec()
}

fn a_float(doc: &Doc, e: NodeId, name: &[u8], default: f32) -> f32 {
    attr(doc, e, name).map_or(default, token_to_float)
}

fn a_int(doc: &Doc, e: NodeId, name: &[u8], default: i32) -> i32 {
    attr(doc, e, name).map_or(default, token_to_int)
}

fn a_bool(doc: &Doc, e: NodeId, name: &[u8], default: bool) -> bool {
    attr(doc, e, name).map_or(default, to_bool)
}

fn kids(doc: &Doc, parent: Option<NodeId>, name: &[u8]) -> Vec<NodeId> {
    let mut v = Vec::new();
    let mut c = parent.and_then(|p| doc.first_child_element(p, Some(name)));
    while let Some(x) = c {
        v.push(x);
        c = doc.next_sibling_element(x, Some(name));
    }
    v
}

/// `ParseXml`: the document's root element when the text loads.
fn parse_root(text: &[u8]) -> Option<(Doc, NodeId)> {
    let mut doc = Doc::new();
    doc.parse(text);
    if doc.error() {
        return None;
    }
    let root = doc.first_child_element(DOC, None)?;
    Some((doc, root))
}

fn key(subject: &[u8], line: i32) -> Vec<u8> {
    [subject, b"#", &int_to_string(line, 0)].concat()
}

fn pad3(n: i32) -> Vec<u8> {
    format!("{n:03}").into_bytes()
}

impl Voices {
    pub fn new() -> Self {
        Self {
            fade_alpha: 0.0,
            fade_goal: 0.0,
            ..Self::default()
        }
    }

    /// `LoadLangFile` on file text.
    pub fn load_lang(&mut self, text: &[u8]) {
        let Some((doc, root)) = parse_root(text) else {
            return;
        };
        for cat in kids(&doc, Some(root), b"CATEGORY") {
            if to_lower(&a_str(&doc, cat, b"Name", b"")).starts_with(b"voices_") {
                for entry in kids(&doc, Some(cat), b"Entry") {
                    self.text.insert(
                        to_lower(&a_str(&doc, entry, b"Name", b"")),
                        a_str(&doc, entry, b"_Text", b""),
                    );
                }
            }
        }
    }

    /// `LoadVoiceFile` on file text: the number of subjects, `None` when the text did not load.
    pub fn load_voice(&mut self, text: &[u8], set: &[u8]) -> Option<usize> {
        let (doc, root) = parse_root(text)?;
        let chars_node = doc.first_child_element(root, Some(b"Characters"));
        let mut chars: BTreeMap<i32, Line> = BTreeMap::new();
        for p in kids(&doc, chars_node, b"Character") {
            let c = chars.entry(a_int(&doc, p, b"ID", -1)).or_default();
            c.character = a_str(&doc, p, b"Name", b"");
            c.display_name = a_str(&doc, p, b"DisplayName", &c.character.clone());
            c.char_volume = a_float(&doc, p, b"Volume", 1.0);
            c.entry_type = match a_str(&doc, p, b"EntryType", b"GUIWorld").as_slice() {
                b"World" => 1,
                b"WorldClean" => 2,
                b"GUI" => 4,
                _ => 8,
            };
            c.source = a_str(&doc, p, b"EntitySource", b"");
            c.min_dist = a_float(&doc, p, b"MinDist", 0.0);
            c.max_dist = a_float(&doc, p, b"MaxDist", 0.0);
            c.world_space = a_bool(&doc, p, b"WorldSpace", false);
        }
        let mut scenes: BTreeMap<i32, Vec<u8>> = BTreeMap::new();
        let scenes_node = doc.first_child_element(root, Some(b"Scenes"));
        for p in kids(&doc, scenes_node, b"Scene") {
            scenes.insert(a_int(&doc, p, b"ID", -1), a_str(&doc, p, b"Name", b""));
            self.scene_focus_prio.insert(
                a_str(&doc, p, b"Name", b""),
                a_int(&doc, p, b"FocusPrio", 0),
            );
        }
        let subjects_node = doc.first_child_element(root, Some(b"Subjects"));
        let mut count = 0;
        for sn in kids(&doc, subjects_node, b"Subject") {
            let scene = scenes
                .entry(a_int(&doc, sn, b"SceneId", -1))
                .or_default()
                .clone();
            let mut subject = Subject {
                name: a_str(&doc, sn, b"Name", b""),
                scene,
                set: set.to_vec(),
                single_random_line: a_bool(&doc, sn, b"UseSingleRandomLine", false),
                last_line: -1,
                lines: Vec::new(),
            };
            for ln in kids(&doc, Some(sn), b"Line") {
                let mut line = chars
                    .entry(a_int(&doc, ln, b"CharacterId", -1))
                    .or_default()
                    .clone();
                line.callback = a_str(&doc, ln, b"Callback", b"");
                line.prio = a_int(&doc, ln, b"Prio", 0);
                line.play_once = a_bool(&doc, ln, b"PlayOnce", false);
                if a_bool(&doc, ln, b"ChangeSource", false) {
                    line.source = a_str(&doc, ln, b"EntitySource", b"");
                    line.min_dist = a_float(&doc, ln, b"MinDist", 0.0);
                    line.max_dist = a_float(&doc, ln, b"MaxDist", 0.0);
                    line.change_source = true;
                }
                for sd in kids(&doc, Some(ln), b"Sound") {
                    line.sounds.push(Sound {
                        text: a_str(&doc, sd, b"Text", b""),
                        file: a_str(&doc, sd, b"FileName", b""),
                        voice_offset: a_float(&doc, sd, b"VoiceOffset", 0.0),
                        end_padding: a_float(&doc, sd, b"EndPadding", 0.0),
                        text_offset: a_float(&doc, sd, b"TextOffset", 0.0),
                        volume: a_float(&doc, sd, b"Volume", 1.0),
                        effect: a_str(&doc, sd, b"ExtraEffectFile", b""),
                        ends_after_effect: a_bool(&doc, sd, b"EndsAfterExtraEffect", false),
                    });
                }
                subject.lines.push(line);
            }
            self.subjects.push(subject);
            if let Some(s) = self.subjects.last() {
                self.by_name.insert(s.name.clone(), self.subjects.len() - 1);
            }
            count += 1;
        }
        Some(count)
    }

    /// True once per map: the caller then loads its `.voice` and `.lang`.
    pub fn mark_set(&mut self, set: &[u8]) -> bool {
        if self.loaded_sets.iter().any(|s| s == set) {
            return false;
        }
        self.loaded_sets.push(set.to_vec());
        true
    }

    pub fn subject_exists(&self, name: &[u8]) -> bool {
        self.by_name.contains_key(name)
    }

    pub fn any_active(&self) -> bool {
        !self.playing.is_empty()
    }

    fn subject(&self, i: usize) -> Option<&Subject> {
        self.subjects.get(i)
    }

    fn scene_of(&self, p: &Playing) -> &[u8] {
        self.subject(p.subject).map_or(&[], |s| &s.scene)
    }

    fn line_of(&self, p: &Playing) -> Option<&Line> {
        let l = *p.lines.get(p.line)?;
        self.subject(p.subject)?.lines.get(usize::try_from(l).ok()?)
    }

    fn sound_key(&self, subject: usize, line: usize, sound: usize) -> Vec<u8> {
        let Some(s) = self.subject(subject) else {
            return Vec::new();
        };
        let Some(l) = s.lines.get(line) else {
            return Vec::new();
        };
        if let Some(f) = l
            .sounds
            .get(sound)
            .map(|x| &x.file)
            .filter(|f| !f.is_empty())
        {
            return f.clone();
        }
        let mut k = [s.scene.as_slice(), b"_", &s.name, b"_"].concat();
        k.extend_from_slice(&pad3(line as i32 + 1));
        k.push(b'_');
        k.extend_from_slice(&l.character);
        k.push(b'_');
        k.extend_from_slice(&pad3(sound as i32 + 1));
        k
    }

    /// First half of `Play`: 1 when the scene must be stopped (and the caller then calls
    /// `play_start`), 0 when `Play` fails here, -1 for an unknown subject.
    pub fn play_check(&mut self, host: &mut dyn VoiceHost, subject: &[u8], prio: i32) -> i32 {
        let Some(&si) = self.by_name.get(subject) else {
            let mut m = b"Voice subject '".to_vec();
            m.extend_from_slice(subject);
            m.extend_from_slice(b"' could not be found in voicehandler\n");
            host.error(&m);
            return -1;
        };
        let scene = self.subject(si).map_or(&[][..], |s| &s.scene);
        let busy = self
            .playing
            .iter()
            .any(|q| self.scene_of(q) == scene && q.prio >= prio);
        i32::from(!busy)
    }

    pub fn scene_of_subject(&self, subject: &[u8]) -> Vec<u8> {
        self.by_name
            .get(subject)
            .and_then(|&i| self.subject(i))
            .map(|s| s.scene.clone())
            .unwrap_or_default()
    }

    /// Second half of `Play`, after the scene was stopped.
    #[allow(clippy::too_many_arguments)]
    pub fn play_start(
        &mut self,
        host: &mut dyn VoiceHost,
        subject: &[u8],
        line: i32,
        callback: &[u8],
        prio: i32,
        on_done: u64,
        play_while_paused: bool,
    ) -> bool {
        let Some(&si) = self.by_name.get(subject) else {
            return false;
        };
        let Some(sub) = self.subjects.get(si) else {
            return false;
        };
        let mut p = Playing {
            subject: si,
            prio,
            callback: callback.to_vec(),
            on_done,
            lines: Vec::new(),
            line: 0,
            sound: 0,
            time: 0.0,
            step: 0,
            entry: 0,
            entry_id: -1,
            fallback: 0.0,
            text_delay: 0.0,
            subtitle: Vec::new(),
            paused: false,
            play_while_paused,
            source_entity: Vec::new(),
            source_min: 0.0,
            source_max: 0.0,
            listen_range: -1.0,
            min_freq: 22000.0,
            max_freq: 22000.0,
            source_dist: 0.0,
            freq_flags: 0,
            file: Vec::new(),
            lip_entity: Vec::new(),
            lipsync: None,
            effect: 0,
            effect_id: -1,
            old_voices: Vec::new(),
            old_effects: Vec::new(),
        };
        let n = sub.lines.len() as i32;
        if n == 0 {
            return false;
        }
        let once: Vec<bool> = sub.lines.iter().map(|l| l.play_once).collect();
        let prios: Vec<i32> = sub.lines.iter().map(|l| l.prio).collect();
        let scene = sub.scene.clone();
        let once_at = |i: i32| {
            usize::try_from(i)
                .ok()
                .and_then(|i| once.get(i))
                .copied()
                .unwrap_or(false)
        };
        let prio_at = |i: i32| {
            usize::try_from(i)
                .ok()
                .and_then(|i| prios.get(i))
                .copied()
                .unwrap_or(0)
        };
        let name = sub.name.clone();
        let single = sub.single_random_line;
        let last_line = sub.last_line;
        if line >= 0 {
            if line >= n {
                return false;
            }
            p.lines.push(line);
        } else if single {
            let mut cand: Vec<i32> = Vec::new();
            for i in 0..n {
                if once_at(i) && host.played_has(&key(&name, i)) {
                    continue;
                }
                match cand.first() {
                    Some(&c) if prio_at(i) > prio_at(c) => cand = vec![i],
                    Some(&c) if prio_at(i) == prio_at(c) => cand.push(i),
                    Some(_) => {}
                    None => cand = vec![i],
                }
            }
            if cand.is_empty() {
                return false;
            }
            let m = cand.len() as i32;
            let mut r = rand_rectl(0, m - 1);
            let pick = |r: i32| {
                usize::try_from(r)
                    .ok()
                    .and_then(|r| cand.get(r))
                    .copied()
                    .unwrap_or(0)
            };
            if m > 2 && pick(r) == last_line {
                r = (r + 1).wrapping_rem(m);
            }
            let chosen = pick(r);
            if let Some(s) = self.subjects.get_mut(si) {
                s.last_line = chosen;
            }
            p.lines.push(chosen);
        } else {
            p.lines.push(0);
            let first_once = once_at(0);
            for i in 1..n {
                if !first_once || host.played_insert(&key(&name, i)) {
                    p.lines.push(i);
                }
            }
        }
        let first = p.lines.first().copied().unwrap_or(0);
        if once_at(first) && !host.played_insert(&key(&name, first)) {
            return false;
        }
        self.playing.push(p);
        let take = match self.scene_focus_prio.get(&self.focus_scene) {
            None => true,
            Some(&cur) => {
                let wanted = *self.scene_focus_prio.entry(scene.clone()).or_insert(0);
                cur < wanted
            }
        };
        if take {
            self.focus_scene = scene.clone();
            self.scene_volumes.entry(scene).or_default().focus = 1.0;
        }
        true
    }

    fn start_sound(&mut self, host: &mut dyn VoiceHost, idx: usize) {
        let Some(p) = self.playing.get(idx) else {
            return;
        };
        let (si, line_idx, sound_idx) = (
            p.subject,
            p.lines.get(p.line).copied().unwrap_or(0),
            p.sound,
        );
        let li = usize::try_from(line_idx).unwrap_or(0);
        let key_s = self.sound_key(si, li, sound_idx);
        let Some(sub) = self.subjects.get(si) else {
            return;
        };
        let Some(line) = sub.lines.get(li) else {
            return;
        };
        let Some(sound) = line.sounds.get(sound_idx) else {
            return;
        };
        let text = match self.text.get(&to_lower(&key_s)) {
            Some(t) if !t.is_empty() => t.clone(),
            _ => sound.text.clone(),
        };
        let subtitle = if text.is_empty() || line.display_name.is_empty() {
            text.clone()
        } else {
            [line.display_name.as_slice(), b": ", &text].concat()
        };
        let text_delay = std_max(sound.text_offset - sound.voice_offset, 0.0);
        let file = [b"voices/".as_slice(), &sub.set, b"/", &key_s, b".ogg"].concat();
        let scene = sub.scene.clone();
        let scene_vol = self.scene_volumes.entry(scene).or_default().volume;
        let volume = sound.volume * line.char_volume * scene_vol;
        let mut src = Source {
            entity: line.source.clone(),
            min_dist: line.min_dist,
            max_dist: line.max_dist,
            use_3d: line.world_space && !line.source.is_empty(),
            listen_range: -1.0,
            min_freq: 22000.0,
            max_freq: 22000.0,
            freq_flags: 0,
        };
        let shared = self
            .sources
            .get(&line.character)
            .filter(|_| !line.change_source);
        if let Some(s) = shared {
            src = s.clone();
            src.use_3d = src.use_3d && line.world_space;
        }
        let lip_entity = if shared.is_some() {
            src.entity.clone()
        } else {
            line.source.clone()
        };
        let (play_while_paused, entry_type) = (p.play_while_paused, line.entry_type);
        let ty = if play_while_paused { 4 } else { entry_type };
        let set = sub.set.clone();
        let src_pos = if src.use_3d && src.max_dist > 0.0 && host.map_current() {
            host.map_entity_pos(&src.entity)
        } else {
            None
        };
        let mut entry = None;
        let mut with_source = false;
        if !host.file_exists(&file) {
        } else if let Some(pos) = src_pos {
            entry = host.play_3d(&file, volume, pos, src.min_dist, src.max_dist, ty);
            with_source = true;
        } else {
            entry = host.play_gui_stream(&file, volume, ty);
        }
        let lipsync = if entry.is_some() && !lip_entity.is_empty() {
            let path = [b"voices/".as_slice(), &set, b"/lipsync/", &key_s, b".anno"].concat();
            host.read_file(&path)
                .and_then(|d| decode_lipsync(&d))
                .map(Rc::new)
        } else {
            None
        };
        let mut msg = b"SOMA voice: ".to_vec();
        msg.extend_from_slice(&key_s);
        if entry.is_none() {
            msg.extend_from_slice(b" (no audio)");
        }
        msg.push(b'\n');
        host.log(&msg);
        let Some(p) = self.playing.get_mut(idx) else {
            return;
        };
        p.subtitle = subtitle;
        p.text_delay = text_delay;
        p.file = file;
        p.entry = entry.map_or(0, |e| e.0);
        p.source_entity = Vec::new();
        if with_source {
            p.source_entity = src.entity.clone();
            p.source_min = src.min_dist;
            p.source_max = src.max_dist;
            p.listen_range = src.listen_range;
            p.min_freq = src.min_freq;
            p.max_freq = src.max_freq;
            p.freq_flags = src.freq_flags;
        }
        p.entry_id = entry.map_or(-1, |e| e.1);
        p.lip_entity = lip_entity;
        p.lipsync = lipsync;
        p.fallback = fma(0.09, text.len() as f32, 0.5);
    }

    fn stop_sound(&mut self, host: &mut dyn VoiceHost, idx: usize) {
        let Some(p) = self.playing.get_mut(idx) else {
            return;
        };
        p.old_voices.push((p.entry, p.entry_id));
        p.old_effects.push((p.effect, p.effect_id));
        for &(e, id) in &p.old_voices {
            if e != 0 && host.entry_valid(e, id) {
                host.entry_fade_out(e, 2.0);
            }
        }
        for &(e, id) in &p.old_effects {
            if e != 0 && host.effect_live(e, id) {
                host.effect_fade_out(e, 2.0);
            }
        }
        p.old_voices.clear();
        p.old_effects.clear();
        p.entry = 0;
        p.effect = 0;
    }

    /// One iteration of `Stop(scene)`: stops and removes the first playing of the scene and returns
    /// its completion token (0 for none); `None` when no playing of the scene is left.
    pub fn stop_next(&mut self, host: &mut dyn VoiceHost, scene: &[u8]) -> Option<StopEvent> {
        let i = self
            .playing
            .iter()
            .position(|p| self.scene_of(p) == scene)?;
        self.stop_sound(host, i);
        let p = self.playing.remove(i);
        Some(StopEvent { on_done: p.on_done })
    }

    pub fn stop_all(&mut self, host: &mut dyn VoiceHost) {
        for i in 0..self.playing.len() {
            self.stop_sound(host, i);
        }
        self.playing.clear();
    }

    pub fn reset_state(&mut self) {
        self.sources.clear();
        self.scene_volumes.clear();
        self.focus_scene.clear();
        self.forced_focus_scene.clear();
        self.focus_timer = 0.0;
        self.speaking_callbacks.clear();
        self.speaking.clear();
    }

    pub fn skip_current_line(&mut self, host: &mut dyn VoiceHost, scene: &[u8]) {
        for i in 0..self.playing.len() {
            if self
                .playing
                .get(i)
                .is_some_and(|p| self.scene_of(p) == scene)
            {
                self.stop_sound(host, i);
                if let Some(p) = self.playing.get_mut(i) {
                    p.subtitle.clear();
                    p.line += 1;
                    p.sound = 0;
                    p.step = 0;
                    p.time = 0.0;
                }
            }
        }
    }

    pub fn character_is_speaking(&self, name: &[u8]) -> bool {
        self.playing
            .iter()
            .any(|p| p.step == 1 && self.line_of(p).is_some_and(|l| l.character == name))
    }

    #[allow(clippy::too_many_arguments)]
    pub fn set_source(
        &mut self,
        character: &[u8],
        entity: &[u8],
        min: f32,
        max: f32,
        use_3d: bool,
        listen_range: f32,
        min_freq: f32,
        max_freq: f32,
        freq_flags: i32,
    ) {
        self.sources.insert(
            character.to_vec(),
            Source {
                entity: entity.to_vec(),
                min_dist: min,
                max_dist: max,
                use_3d,
                listen_range,
                min_freq,
                max_freq,
                freq_flags,
            },
        );
    }

    pub fn set_focus_scene(&mut self, host: &mut dyn VoiceHost, scene: &[u8]) {
        self.forced_focus_scene = scene.to_vec();
        if !scene.is_empty() && self.scene_focus_prio.contains_key(scene) {
            self.focus_scene = scene.to_vec();
        } else if scene.is_empty() {
            self.focus_scene.clear();
        } else {
            let mut m = b"Could not find voice scene '".to_vec();
            m.extend_from_slice(scene);
            m.extend_from_slice(b"'\n");
            host.error(&m);
        }
    }

    pub fn fade_scene_volume_to(&mut self, scene: &[u8], volume: f32, time: f32) {
        let v = self.scene_volumes.entry(scene.to_vec()).or_default();
        v.goal = volume;
        v.speed = if time > 0.0 {
            (volume - v.volume).abs() / time
        } else {
            0.0
        };
        if time <= 0.0 {
            v.volume = volume;
        }
    }

    pub fn fade_to(&mut self, goal: f32, time: f32) {
        self.fade_goal = goal;
        self.fade_speed = if time > 0.0 { 1.0 / time } else { 0.0 };
        if time <= 0.0 {
            self.fade_alpha = goal;
        }
    }

    pub fn is_fading(&self) -> bool {
        self.fade_alpha != self.fade_goal
    }

    fn update_focus(&mut self, dt: f32) {
        let (delay, out_mul, speed) = (2.0f32, 0.5f32, 0.5f32);
        let active = |this: &Self, s: &[u8]| this.playing.iter().any(|p| this.scene_of(p) == s);
        if self.playing.is_empty() || !self.forced_focus_scene.is_empty() {
            self.focus_timer = 0.0;
        } else if !active(self, &self.focus_scene) {
            if self.focus_timer < delay {
                self.focus_timer += dt;
            } else {
                self.focus_timer = 0.0;
                let mut best = -100000;
                let scenes: Vec<Vec<u8>> = self
                    .playing
                    .iter()
                    .map(|p| self.scene_of(p).to_vec())
                    .collect();
                for s in scenes {
                    let prio = *self.scene_focus_prio.entry(s.clone()).or_insert(0);
                    if prio > best {
                        best = prio;
                        self.focus_scene = s;
                    }
                }
            }
        }
        for (name, v) in &mut self.scene_volumes {
            let goal = if *name == self.focus_scene {
                1.0
            } else {
                out_mul
            };
            v.focus = if v.focus < goal {
                std_min(v.focus + speed * dt, goal)
            } else {
                std_max(v.focus - speed * dt, goal)
            };
        }
    }

    fn frozen(&self, p: &Playing) -> bool {
        self.upd_paused && !p.play_while_paused
    }

    /// `UpdateVoices` up to the per-scene loop; `update_step` then runs the loop.
    pub fn update_begin(&mut self, host: &mut dyn VoiceHost, dt: f32, game_paused: bool) {
        let step = if game_paused { 0.0 } else { dt };
        self.upd_paused = game_paused;
        self.upd_dt = dt;
        self.upd_i = 0;
        if self.fade_alpha < self.fade_goal {
            self.fade_alpha = std_min(fma(self.fade_speed, step, self.fade_alpha), self.fade_goal);
        } else if self.fade_alpha > self.fade_goal {
            self.fade_alpha = std_max(fma(-self.fade_speed, step, self.fade_alpha), self.fade_goal);
        }
        for v in self.scene_volumes.values_mut() {
            if v.volume < v.goal {
                v.volume = std_min(fma(v.speed, step, v.volume), v.goal);
            } else if v.volume > v.goal {
                v.volume = std_max(fma(-v.speed, step, v.volume), v.goal);
            }
        }
        self.update_focus(step);
        for i in 0..self.playing.len() {
            let Some(p) = self.playing.get(i) else {
                continue;
            };
            if self.frozen(p) {
                continue;
            }
            let vol = self
                .scene_volumes
                .get(self.scene_of(p))
                .map_or(1.0, |v| v.volume * v.focus);
            let Some(p) = self.playing.get_mut(i) else {
                continue;
            };
            let mut old = std::mem::take(&mut p.old_voices);
            old.retain(|&(e, id)| host.entry_valid(e, id));
            let mut old_fx = std::mem::take(&mut p.old_effects);
            old_fx.retain(|&(e, id)| host.effect_live(e, id));
            for &(e, _) in &old {
                host.entry_set_volume_mul(e, vol);
            }
            for &(e, _) in &old_fx {
                host.effect_set_volume_mul(e, vol);
            }
            p.old_voices = old;
            p.old_effects = old_fx;
            if p.effect != 0 && host.effect_live(p.effect, p.effect_id) {
                host.effect_set_volume_mul(p.effect, vol);
            }
            if p.entry == 0 || !host.entry_valid(p.entry, p.entry_id) {
                continue;
            }
            host.entry_set_volume_mul(p.entry, vol);
            let pos = if p.source_entity.is_empty() || !host.map_current() {
                None
            } else {
                host.map_entity_pos(&p.source_entity)
            };
            if let Some(pos) = pos {
                host.channel_set_position(p.entry, pos);
                let l = host.listener_pos();
                let (dx, dy, dz) = (pos[0] - l[0], pos[1] - l[1], pos[2] - l[2]);
                let dist = fma(dz, dz, fma(dx, dx, dy * dy)).sqrt();
                p.source_dist = dist;
                if p.freq_flags != 0 {
                    let t = clamp(
                        (dist - p.source_min) / (p.source_max - p.source_min + 0.01),
                        0.0,
                        1.0,
                    );
                    let cutoff = std_max(fma(p.max_freq - p.min_freq, t, p.min_freq), 1.0);
                    if p.freq_flags & 1 != 0 {
                        host.channel_filter(
                            p.entry,
                            false,
                            1.0 / (1.0 + super::powf(8000.0 / cutoff, 4.0)).sqrt(),
                        );
                    }
                    if p.freq_flags & 2 != 0 {
                        host.channel_filter(
                            p.entry,
                            true,
                            1.0 / (1.0 + super::powf(cutoff / 100.0, 4.0)).sqrt(),
                        );
                    }
                }
            }
        }
    }

    /// Runs the per-scene loop of `UpdateVoices` until a script or completion callback is due;
    /// the loop resumes on the next call. `None` when it ran to the end.
    pub fn update_step(&mut self, host: &mut dyn VoiceHost) -> Option<UpdateEvent> {
        let dt = self.upd_dt;
        loop {
            let i = self.upd_i;
            let p = self.playing.get(i)?;
            if p.paused || self.frozen(p) {
                self.upd_i += 1;
                continue;
            }
            if p.line >= p.lines.len() {
                let p = self.playing.remove(i);
                let sub = self.subjects.get(p.subject);
                return Some(UpdateEvent::Finish {
                    callback: p.callback,
                    scene: sub.map(|s| s.scene.clone()).unwrap_or_default(),
                    name: sub.map(|s| s.name.clone()).unwrap_or_default(),
                    on_done: p.on_done,
                });
            }
            let Some(line) = self.line_of(p) else {
                self.upd_i += 1;
                continue;
            };
            if p.sound >= line.sounds.len() {
                let line_no = p.lines.get(p.line).copied().unwrap_or(0);
                let func = line.callback.clone();
                if let Some(p) = self.playing.get_mut(i) {
                    p.line += 1;
                    p.sound = 0;
                    p.step = 0;
                    p.time = 0.0;
                }
                if !func.is_empty() {
                    return Some(self.line_event(i, line_no, func, false));
                }
                continue;
            }
            let Some(sound) = line.sounds.get(p.sound).cloned() else {
                self.upd_i += 1;
                continue;
            };
            let (entry_type, play_while_paused) = (line.entry_type, p.play_while_paused);
            let scene = self.scene_of(p).to_vec();
            if p.step == 0 && p.time == 0.0 && dt > 0.0 && !sound.effect.is_empty() {
                let ty = if play_while_paused { 4 } else { entry_type };
                let eff = host.effect_play(&sound.effect, sound.volume, ty);
                let vol = self.scene_volumes.get(&scene).map(|v| v.volume);
                if let Some(p) = self.playing.get_mut(i) {
                    p.old_effects.push((p.effect, p.effect_id));
                    p.effect = eff.map_or(0, |e| e.0);
                    p.effect_id = eff.map_or(-1, |e| e.1);
                    if let (Some(e), Some(v)) = (eff, vol) {
                        host.effect_set_volume_mul(e.0, v);
                    }
                }
            }
            let Some(p) = self.playing.get_mut(i) else {
                continue;
            };
            p.time += dt;
            let (step, time, sound_no) = (p.step, p.time, p.sound);
            if step == 0 && time >= sound.voice_offset {
                self.start_sound(host, i);
                let line_no = {
                    let Some(p) = self.playing.get_mut(i) else {
                        continue;
                    };
                    p.step = 1;
                    p.time = 0.0;
                    p.lines.get(p.line).copied().unwrap_or(0)
                };
                if sound_no == 0 {
                    self.upd_i += 1;
                    let func = self
                        .subjects
                        .get(self.playing.get(i).map_or(0, |p| p.subject))
                        .and_then(|s| usize::try_from(line_no).ok().and_then(|l| s.lines.get(l)))
                        .map(|l| l.callback.clone())
                        .unwrap_or_default();
                    if !func.is_empty() {
                        return Some(self.line_event(i, line_no, func, true));
                    }
                    continue;
                }
            } else if step == 1 {
                let Some(p) = self.playing.get(i) else {
                    continue;
                };
                let (entry, entry_id, effect, effect_id, fallback, ptime) = (
                    p.entry,
                    p.entry_id,
                    p.effect,
                    p.effect_id,
                    p.fallback,
                    p.time,
                );
                let playing = if sound.ends_after_effect {
                    effect != 0
                        && host.effect_live(effect, effect_id)
                        && host.effect_is_playing(effect)
                        && host.effect_total(effect) - host.effect_elapsed(effect)
                            >= sound.end_padding
                } else if entry != 0 {
                    host.entry_valid(entry, entry_id)
                        && host.channel_total(entry) - host.channel_elapsed(entry)
                            >= f64::from(sound.end_padding)
                } else {
                    ptime < fallback
                };
                if !playing && let Some(p) = self.playing.get_mut(i) {
                    p.old_voices.push((p.entry, p.entry_id));
                    p.entry = 0;
                    p.step = 2;
                    p.time = 0.0;
                }
            } else if step == 2
                && time >= std_max(-sound.end_padding, 0.0)
                && let Some(p) = self.playing.get_mut(i)
            {
                p.subtitle.clear();
                p.sound += 1;
                p.step = 0;
                p.time = 0.0;
            }
            self.upd_i += 1;
        }
    }

    fn line_event(&self, idx: usize, line_no: i32, func: Vec<u8>, start: bool) -> UpdateEvent {
        let sub = self.playing.get(idx).and_then(|p| self.subject(p.subject));
        UpdateEvent::Line {
            func,
            scene: sub.map(|s| s.scene.clone()).unwrap_or_default(),
            name: sub.map(|s| s.name.clone()).unwrap_or_default(),
            line: line_no,
            start,
        }
    }

    pub fn set_speaking_callback(&mut self, character: &[u8], func: &[u8]) {
        if func.is_empty() {
            self.speaking_callbacks.remove(character);
        } else {
            self.speaking_callbacks
                .insert(character.to_vec(), func.to_vec());
        }
    }

    /// `UpdateSpeakingCallbacks` up to the script calls: who started or stopped speaking.
    pub fn speaking_changes(&mut self) -> Vec<(Vec<u8>, bool)> {
        let now: BTreeSet<Vec<u8>> = self
            .playing
            .iter()
            .filter(|p| p.step == 1)
            .filter_map(|p| self.line_of(p))
            .map(|l| l.character.clone())
            .collect();
        let mut changed: Vec<(Vec<u8>, bool)> = Vec::new();
        for s in &now {
            if !self.speaking.contains(s) {
                changed.push((s.clone(), true));
            }
        }
        for s in &self.speaking {
            if !now.contains(s) {
                changed.push((s.clone(), false));
            }
        }
        self.speaking = now;
        changed
    }

    pub fn speaking_func(&self, character: &[u8]) -> Option<Vec<u8>> {
        self.speaking_callbacks.get(character).cloned()
    }

    /// After the script returned `keep` for `func`: drops the callback when it asked to go and was
    /// not replaced meanwhile.
    pub fn speaking_done(&mut self, character: &[u8], func: &[u8], keep: bool) {
        if !keep
            && self
                .speaking_callbacks
                .get(character)
                .is_some_and(|f| f == func)
        {
            self.speaking_callbacks.remove(character);
        }
    }

    pub fn add_subtitle(
        &mut self,
        rows: Vec<Vec<u8>>,
        name: &[u8],
        color: [f32; 4],
        drawn_chars: i32,
    ) {
        self.immediate.push(ImmediateSubtitle {
            rows,
            name: name.to_vec(),
            color,
            drawn_chars,
        });
    }

    /// The subtitles `OnDraw` puts on screen, the lowest first.
    pub fn subtitles(
        &mut self,
        host: &mut dyn VoiceHost,
        screen_h: f32,
        game_paused: bool,
    ) -> Vec<SubtitleDraw> {
        let immediate = std::mem::take(&mut self.immediate);
        let scale = screen_h / 768.0;
        let mut y = 700.0 * scale;
        let size = [26.0 * scale, 26.0 * scale];
        let width = 860.0 * scale;
        let mut out = Vec::new();
        for p in self.playing.iter().rev() {
            if p.subtitle.is_empty()
                || p.step == 0
                || (p.step == 1 && p.time < p.text_delay)
                || p.paused
                || (game_paused && !p.play_while_paused)
            {
                continue;
            }
            if p.listen_range >= 0.0
                && !p.source_entity.is_empty()
                && p.source_dist > p.listen_range
            {
                continue;
            }
            let alpha = if !p.source_entity.is_empty() && p.listen_range >= 0.0 {
                clamp(
                    1.0 - 6.0 * (p.source_dist - p.source_min) / std_max(p.source_max, 0.01),
                    0.2,
                    1.0,
                )
            } else {
                1.0
            };
            let rows = host.wrap_rows(&p.subtitle, width, size[1], size);
            y = fma(-size[1], (rows as u64).wrapping_sub(1) as f32, y);
            out.push(SubtitleDraw {
                text: p.subtitle.clone(),
                alpha,
                color: None,
                y,
                rows,
            });
            y = fma(-size[1], 1.3, y);
        }
        for sub in immediate {
            let mut text = if sub.name.is_empty() {
                Vec::new()
            } else {
                [sub.name.as_slice(), b": "].concat()
            };
            for (i, r) in sub.rows.iter().enumerate() {
                if i > 0 {
                    text.push(b' ');
                }
                text.extend_from_slice(r);
            }
            if let Ok(n) = usize::try_from(sub.drawn_chars)
                && n < text.len()
            {
                text.truncate(n);
            }
            let rows = host.wrap_rows(&text, width, size[1], size);
            y = fma(-size[1], (rows as u64).wrapping_sub(1) as f32, y);
            out.push(SubtitleDraw {
                text,
                alpha: sub.color[3],
                color: Some(sub.color),
                y,
                rows,
            });
            y = fma(-size[1], 1.3, y);
        }
        out
    }

    pub fn drop_immediate(&mut self) {
        self.immediate.clear();
    }

    pub fn subject_is_playing(&self, name: &[u8]) -> bool {
        self.playing
            .iter()
            .any(|p| self.subject(p.subject).is_some_and(|s| s.name == name))
    }

    pub fn scene_is_active(&self, scene: &[u8]) -> bool {
        self.playing.iter().any(|p| self.scene_of(p) == scene)
    }

    pub fn scene_involving_character_is_active(&self, name: &[u8]) -> bool {
        self.playing.iter().any(|p| {
            self.subject(p.subject)
                .is_some_and(|s| s.lines.iter().any(|l| l.character == name))
        })
    }

    pub fn subject_involves_character(&self, subject: &[u8], name: &[u8]) -> bool {
        self.by_name
            .get(subject)
            .and_then(|&i| self.subject(i))
            .is_some_and(|s| s.lines.iter().any(|l| l.character == name))
    }

    pub fn subject_line_count(&self, subject: &[u8]) -> i32 {
        self.by_name
            .get(subject)
            .and_then(|&i| self.subject(i))
            .map_or(0, |s| s.lines.len() as i32)
    }

    pub fn subject_scene(&self, subject: &[u8]) -> &[u8] {
        self.by_name
            .get(subject)
            .and_then(|&i| self.subject(i))
            .map_or(&[], |s| &s.scene)
    }

    /// `GetTimeLeftOnLastLineOfPlayingSubject`.
    pub fn time_left_on_last_line(&self, host: &mut dyn VoiceHost, subject: &[u8]) -> f32 {
        for p in &self.playing {
            let Some(sub) = self.subject(p.subject).filter(|s| s.name == subject) else {
                continue;
            };
            let Some(&l) = p.lines.get(p.line) else {
                continue;
            };
            let nsounds = usize::try_from(l)
                .ok()
                .and_then(|l| sub.lines.get(l))
                .map_or(0, |l| l.sounds.len());
            if l + 1 != sub.lines.len() as i32
                || p.sound + 1 != nsounds
                || p.entry == 0
                || !host.entry_valid(p.entry, p.entry_id)
            {
                return -1.0;
            }
            return (host.channel_total(p.entry) - host.channel_elapsed(p.entry)) as f32;
        }
        -1.0
    }

    /// The entry and file of the first playing whose current line is spoken by `character`.
    pub fn spectrum_by_character(&self, character: &[u8]) -> Option<(usize, i32, Vec<u8>)> {
        self.playing
            .iter()
            .find(|p| self.line_of(p).is_some_and(|l| l.character == character))
            .map(|p| (p.entry, p.entry_id, p.file.clone()))
    }

    pub fn spectrum_by_scene(&self, scene: &[u8]) -> Option<(usize, i32, Vec<u8>)> {
        self.playing
            .iter()
            .find(|p| self.scene_of(p) == scene)
            .map(|p| (p.entry, p.entry_id, p.file.clone()))
    }

    fn apply_pause(&self, host: &mut dyn VoiceHost, p: &Playing) {
        if p.entry != 0 && host.entry_valid(p.entry, p.entry_id) {
            host.entry_set_paused(p.entry, p.paused);
        }
        for &(e, id) in &p.old_voices {
            if host.entry_valid(e, id) {
                host.entry_set_paused(e, p.paused);
            }
        }
        if p.effect != 0 && host.effect_live(p.effect, p.effect_id) {
            host.effect_set_paused(p.effect, p.paused);
        }
        for &(e, id) in &p.old_effects {
            if e != 0 && host.effect_live(e, id) {
                host.effect_set_paused(e, p.paused);
            }
        }
    }

    pub fn set_paused(&mut self, host: &mut dyn VoiceHost, scene: &[u8], on: bool) {
        for i in 0..self.playing.len() {
            if self
                .playing
                .get(i)
                .is_some_and(|p| self.scene_of(p) == scene)
            {
                if let Some(p) = self.playing.get_mut(i) {
                    p.paused = on;
                }
                if let Some(p) = self.playing.get(i) {
                    self.apply_pause(host, p);
                }
            }
        }
    }

    pub fn set_paused_all(&mut self, host: &mut dyn VoiceHost, on: bool) {
        for p in &mut self.playing {
            p.paused = on;
        }
        for p in &self.playing {
            self.apply_pause(host, p);
        }
    }

    pub fn reapply_pauses(&self, host: &mut dyn VoiceHost) {
        for p in self.playing.iter().filter(|p| p.paused) {
            self.apply_pause(host, p);
        }
    }

    /// `UpdateLipsync`'s sampling: the viseme weights per speaking entity.
    pub fn lip_weights(&self, host: &mut dyn VoiceHost) -> BTreeMap<Vec<u8>, [f32; VISEMES]> {
        let mut out: BTreeMap<Vec<u8>, [f32; VISEMES]> = BTreeMap::new();
        for p in &self.playing {
            let Some(frames) = p.lipsync.as_ref().filter(|f| !f.is_empty()) else {
                continue;
            };
            if p.step != 1 || p.entry == 0 || !host.entry_valid(p.entry, p.entry_id) {
                continue;
            }
            let ms = (host.channel_elapsed(p.entry) * f64::from(1000.0f32) + 30.0) as f32;
            let at = frames.partition_point(|f| {
                ms.partial_cmp(&(f.start as f32)) != Some(std::cmp::Ordering::Less)
            });
            let (Some(first), Some(last)) = (frames.first(), frames.last()) else {
                continue;
            };
            let a = if at == 0 {
                first
            } else {
                frames.get(at - 1).unwrap_or(last)
            };
            let b = frames.get(at).unwrap_or(a);
            let t = if b.start > a.start {
                clamp((ms - a.start as f32) / (b.start - a.start) as f32, 0.0, 1.0)
            } else {
                0.0
            };
            let w = out.entry(p.lip_entity.clone()).or_insert([0.0; VISEMES]);
            for (wi, (bw, aw)) in w.iter_mut().zip(b.w.iter().zip(&a.w)) {
                *wi = clamp(fma(bw - aw, t, *aw), 0.0, 2.0);
            }
        }
        out
    }
}

/// Annosoft `.anno`: TEA-ECB over the first `ceil8(size/2)` bytes, then "ONNA", version and
/// frames of {start ms, end ms, n, n x {char[3] phoneme, f32 weight}}.
pub fn decode_lipsync(file: &[u8]) -> Option<Vec<LipFrame>> {
    const K: [u32; 4] = [0x8f81_2a90, 0x7182_3fad, 0x0317_fab7, 0xc385_6aa5];
    let mut d = file.to_vec();
    let enc = (d.len() >> 1).next_multiple_of(8).min(d.len() & !7);
    for block in d.get_mut(..enc)?.chunks_exact_mut(8) {
        let (lo, hi) = block.split_at_mut(4);
        let mut v = [
            u32::from_le_bytes((&*lo).try_into().ok()?),
            u32::from_le_bytes((&*hi).try_into().ok()?),
        ];
        let mut sum = 0xc6ef_3720u32;
        for _ in 0..32 {
            v[1] = v[1].wrapping_sub(
                ((v[0] << 4).wrapping_add(K[2]))
                    ^ v[0].wrapping_add(sum)
                    ^ ((v[0] >> 5).wrapping_add(K[3])),
            );
            v[0] = v[0].wrapping_sub(
                ((v[1] << 4).wrapping_add(K[0]))
                    ^ v[1].wrapping_add(sum)
                    ^ ((v[1] >> 5).wrapping_add(K[1])),
            );
            sum = sum.wrapping_sub(0x9e37_79b9);
        }
        lo.copy_from_slice(&v[0].to_le_bytes());
        hi.copy_from_slice(&v[1].to_le_bytes());
    }
    if d.len() < 12 || d.get(..4)? != b"ONNA" {
        return None;
    }
    let rd =
        |p: usize| -> Option<i32> { Some(i32::from_le_bytes(d.get(p..p + 4)?.try_into().ok()?)) };
    let nframes = rd(8)?;
    let mut out = Vec::new();
    let mut p = 12usize;
    let mut f = 0;
    while f < nframes && p + 12 <= d.len() {
        let mut fr = LipFrame {
            start: rd(p)?,
            end: rd(p + 4)?,
            w: [0.0; VISEMES],
        };
        let n = rd(p + 8)?;
        p += 12;
        let mut i = 0;
        while i < n && p + 7 <= d.len() {
            let w = f32::from_le_bytes(d.get(p + 3..p + 7)?.try_into().ok()?);
            let raw = d.get(p..p + 3)?;
            let name = raw.split(|&b| b == 0).next().unwrap_or(raw);
            if let Some(v) = viseme(name)
                && let Some(slot) = fr.w.get_mut(v - 1)
            {
                *slot += w;
            }
            i += 1;
            p += 7;
        }
        out.push(fr);
        f += 1;
    }
    Some(out)
}

fn viseme(phoneme: &[u8]) -> Option<usize> {
    Some(match phoneme {
        b"AA" => 1,
        b"AH" | b"h" => 2,
        b"AO" => 3,
        b"AW" | b"OW" => 4,
        b"OY" | b"UH" | b"UW" => 5,
        b"AE" | b"EH" => 6,
        b"AY" | b"IH" => 7,
        b"EY" => 8,
        b"IY" | b"y" => 9,
        b"ER" | b"r" => 10,
        b"l" => 11,
        b"w" => 12,
        b"b" | b"m" | b"p" => 13,
        b"DH" | b"NG" | b"TH" | b"ZH" | b"d" | b"g" | b"k" | b"n" | b"s" | b"t" | b"z" => 14,
        b"CH" | b"SH" | b"j" => 15,
        b"f" | b"v" => 16,
        b"x" => 17,
        _ => return None,
    })
}

/// `GetSpectrum`: |FFT| of the last `2n` samples at the 48 kHz mix rate, rect window, averaged
/// over the channels. `pcm` is interleaved.
pub fn spectrum(pcm: &[f32], channels: i32, rate: i32, elapsed: f64, num: i32) -> Vec<f32> {
    let mut out = vec![0.0f32; usize::try_from(num).unwrap_or(0)];
    if channels <= 0 {
        return out;
    }
    let n_total = 2 * num;
    let step = f64::from(rate) / 48000.0;
    let frames = pcm.len().checked_div(channels as usize).unwrap_or(0) as i64;
    let end = (elapsed * f64::from(rate)) as i64;
    let k2pi = std::f32::consts::TAU;
    for c in 0..channels {
        for k in 0..num {
            let (mut re, mut im) = (0.0f32, 0.0f32);
            for n in 0..n_total {
                let i = end - (f64::from(n_total - n) * step) as i64;
                if i < 0 || i >= frames {
                    continue;
                }
                let x = pcm
                    .get((i * i64::from(channels) + i64::from(c)) as usize)
                    .copied()
                    .unwrap_or(0.0);
                let a = k2pi * k as f32 * n as f32 / n_total as f32;
                re = fma(x, crate::math::cos(a), re);
                im = fma(-x, crate::math::sin(a), im);
            }
            if let Some(o) = out.get_mut(k as usize) {
                *o += fma(re, re, im * im).sqrt() * 2.0 / n_total as f32 / channels as f32;
            }
        }
    }
    out
}
