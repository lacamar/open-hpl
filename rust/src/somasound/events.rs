//! FMOD Designer event emulation data: `.fdp` project parsing and the event store.
//! Mirrors `cSomaSoundEvents` (SomaSound.cpp); the files, banks and OpenAL stay C++.

use super::powf;
use crate::string::{file_ext, file_name, set_file_ext, split, to_lower, token_to_float};
use crate::xml::{DOC, Doc, NodeId};
use std::collections::{BTreeMap, BTreeSet};

pub const DSP_VOLUME: i32 = 0;
pub const DSP_EQ_GAIN: i32 = 1;
pub const DSP_LOWPASS: i32 = 2;
pub const DSP_HIGHPASS: i32 = 3;
pub const DSP_PITCH: i32 = 4;
pub const DSP_PAN_LEVEL: i32 = 5;
pub const DSP_REVERB_LEVEL: i32 = 6;

pub const ROLLOFF_LINEAR: i32 = 0;
pub const ROLLOFF_LINEAR_SQUARE: i32 = 1;
pub const ROLLOFF_LOG: i32 = 2;
pub const ROLLOFF_CUSTOM: i32 = 3;

pub const STEAL_OLDEST: i32 = 0;
pub const STEAL_NEWEST: i32 = 1;
pub const STEAL_QUIETEST: i32 = 2;
pub const JUST_FAIL: i32 = 3;
pub const JUST_FAIL_IF_QUIETEST: i32 = 4;

#[derive(Clone, Default)]
pub struct Wave {
    pub bank: Vec<u8>,
    pub sample: Vec<u8>,
}

#[derive(Clone, Default)]
pub struct Envelope {
    pub param: i32,
    pub dsp: i32,
    pub points: Vec<[f32; 2]>,
}

impl Envelope {
    pub fn eval(&self, x: f32) -> f32 {
        let (Some(first), Some(last)) = (self.points.first(), self.points.last()) else {
            return 1.0;
        };
        if x <= first[0] {
            return first[1];
        }
        for w in self.points.windows(2) {
            let (Some(a), Some(b)) = (w.first(), w.get(1)) else {
                continue;
            };
            if x <= b[0] {
                let t = if b[0] > a[0] {
                    (x - a[0]) / (b[0] - a[0])
                } else {
                    1.0
                };
                return crate::math::fma(b[1] - a[1], t, a[1]);
            }
        }
        last[1]
    }
}

#[derive(Clone)]
pub struct SoundDef {
    pub sequential: bool,
    pub volume: f32,
    pub volume_rand_db: f32,
    pub pitch: f32,
    pub pitch_rand: f32,
    pub spawn_min: f32,
    pub spawn_max: f32,
    pub spawn_count: i32,
    pub delay_min: f32,
    pub delay_max: f32,
    pub waves: Vec<Wave>,
    pub files: Vec<Vec<u8>>,
    pub last: i32,
}

impl Default for SoundDef {
    fn default() -> Self {
        Self {
            sequential: false,
            volume: 1.0,
            volume_rand_db: 0.0,
            pitch: 0.0,
            pitch_rand: 0.0,
            spawn_min: 0.0,
            spawn_max: 0.0,
            spawn_count: 1,
            delay_min: 0.0,
            delay_max: 0.0,
            waves: Vec::new(),
            files: Vec::new(),
            last: -1,
        }
    }
}

#[derive(Clone)]
pub struct LayerSound {
    pub def: i32,
    pub volume: f32,
    pub loop_mode: i32,
    pub x0: f32,
    pub x1: f32,
}

#[derive(Clone)]
pub struct Layer {
    pub param: i32,
    pub sounds: Vec<LayerSound>,
    pub envelopes: Vec<Envelope>,
}

impl Default for Layer {
    fn default() -> Self {
        Self {
            param: -1,
            sounds: Vec::new(),
            envelopes: Vec::new(),
        }
    }
}

#[derive(Clone, Default)]
pub struct Param {
    pub name: Vec<u8>,
    pub min: f32,
    pub max: f32,
    pub velocity: f32,
    pub seek: f32,
    pub sustain: f32,
    pub loop_mode: i32,
    pub builtin: i32,
}

#[derive(Clone)]
pub struct Event {
    pub name: Vec<u8>,
    pub volume: f32,
    pub volume_rand_db: f32,
    pub pitch: f32,
    pub pitch_rand: f32,
    pub is_3d: bool,
    pub pan_level: f32,
    pub reverb_gain: f32,
    pub one_shot: bool,
    pub min_dist: f32,
    pub max_dist: f32,
    pub rolloff: i32,
    pub max_playbacks: i32,
    pub max_behavior: i32,
    pub fade_in: f32,
    pub fade_out: f32,
    pub stream: bool,
    pub category_type: i32,
    pub params: Vec<Param>,
    pub layers: Vec<Layer>,
    pub defs: Vec<SoundDef>,
    pub data: usize,
    pub loaded: bool,
}

impl Default for Event {
    fn default() -> Self {
        Self {
            name: Vec::new(),
            volume: 1.0,
            volume_rand_db: 0.0,
            pitch: 0.0,
            pitch_rand: 0.0,
            is_3d: true,
            pan_level: 1.0,
            reverb_gain: 1.0,
            one_shot: true,
            min_dist: 1.0,
            max_dist: 20.0,
            rolloff: ROLLOFF_LINEAR,
            max_playbacks: 0,
            max_behavior: STEAL_OLDEST,
            fade_in: 0.0,
            fade_out: 0.0,
            stream: false,
            category_type: -1,
            params: Vec::new(),
            layers: Vec::new(),
            defs: Vec::new(),
            data: 0,
            loaded: false,
        }
    }
}

impl Event {
    pub fn has_files(&self) -> bool {
        self.defs
            .iter()
            .any(|d| d.files.iter().any(|f| !f.is_empty()))
    }
}

pub fn db_to_gain(db: f32) -> f32 {
    powf(10.0, db / 20.0)
}

/// The entry type an event plays under: its own category when it has one.
pub fn category_entry_type(category_type: i32, requested: i32) -> i32 {
    if category_type >= 0 {
        category_type
    } else {
        requested
    }
}

fn category_of(path: &[u8]) -> i32 {
    let lower = to_lower(path);
    let head = lower.split(|&b| b == b'/').next().unwrap_or_default();
    match head {
        b"world" => 1,
        b"worldclean" => 2,
        b"gui" => 4,
        b"guiworld" => 8,
        _ => -1,
    }
}

struct Xml<'a>(&'a Doc);

impl Xml<'_> {
    fn child(&self, e: Option<NodeId>, name: &[u8]) -> Option<NodeId> {
        e.and_then(|e| self.0.first_child_element(e, Some(name)))
    }

    fn text(&self, e: Option<NodeId>, name: &[u8], default: &[u8]) -> Vec<u8> {
        self.child(e, name)
            .and_then(|c| self.0.text(c))
            .unwrap_or(default)
            .to_vec()
    }

    /// `Num` in SomaSound.cpp: its default never applies, a missing child reads as "" and gives 0.
    fn num(&self, e: Option<NodeId>, name: &[u8]) -> f32 {
        token_to_float(&self.text(e, name, b""))
    }

    fn children(&self, e: NodeId, name: &[u8]) -> Vec<NodeId> {
        let mut v = Vec::new();
        let mut c = self.0.first_child_element(e, Some(name));
        while let Some(x) = c {
            v.push(x);
            c = self.0.next_sibling_element(x, Some(name));
        }
        v
    }
}

fn read_defs(x: &Xml, folder: NodeId, defs: &mut BTreeMap<Vec<u8>, SoundDef>) {
    for def in x.children(folder, b"sounddef") {
        let d = defs.entry(x.text(Some(def), b"name", b"")).or_default();
        d.sequential = x.text(Some(def), b"type", b"").starts_with(b"sequential");
        d.volume = db_to_gain(x.num(Some(def), b"volume_db"));
        d.volume_rand_db = x.num(Some(def), b"volume_randomization");
        d.pitch = x.num(Some(def), b"pitch");
        d.pitch_rand = x.num(Some(def), b"pitch_randomization");
        d.spawn_min = x.num(Some(def), b"spawntime_min") / 1000.0;
        d.spawn_max = x.num(Some(def), b"spawntime_max") / 1000.0;
        d.spawn_count = (x.num(Some(def), b"spawn_max") as i32).max(1);
        d.delay_min = x.num(Some(def), b"trigger_delay_min") / 1000.0;
        d.delay_max = x.num(Some(def), b"trigger_delay_max") / 1000.0;
        for wave in x.children(def, b"waveform") {
            let file = x.text(Some(wave), b"filename", b"");
            if !file.is_empty() {
                d.waves.push(Wave {
                    bank: x.text(Some(wave), b"soundbankname", b""),
                    sample: set_file_ext(file_name(&file), b""),
                });
            }
        }
    }
    for sub in x.children(folder, b"sounddeffolder") {
        read_defs(x, sub, defs);
    }
}

fn read_envelope(x: &Xml, env: NodeId, param_idx: &dyn Fn(&[u8]) -> i32) -> Option<Envelope> {
    let dsp = match x.text(Some(env), b"dsp_name", b"").as_slice() {
        b"Volume" => DSP_VOLUME,
        b"FMOD ParamEQ" => DSP_EQ_GAIN,
        b"FMOD Lowpass" | b"FMOD Lowpass Simple" => DSP_LOWPASS,
        b"FMOD Highpass" | b"FMOD Highpass Simple" => DSP_HIGHPASS,
        b"Pitch" => DSP_PITCH,
        b"3D Pan Level" => DSP_PAN_LEVEL,
        b"Reverb Level" => DSP_REVERB_LEVEL,
        _ => return None,
    };
    let want = if dsp == DSP_EQ_GAIN { 2 } else { 0 };
    if x.num(Some(env), b"dsp_paramindex") as i32 != want || x.text(Some(env), b"mute", b"") == b"1"
    {
        return None;
    }
    let mut e = Envelope {
        dsp,
        param: param_idx(&x.text(Some(env), b"controlparameter", b"")),
        points: Vec::new(),
    };
    for p in x.children(env, b"point") {
        let text = x.0.text(p).unwrap_or_default();
        let nums: Vec<&[u8]> = split(text, None, true).collect();
        if let (Some(a), Some(b)) = (nums.first(), nums.get(1)) {
            e.points.push([token_to_float(a), token_to_float(b)]);
        }
    }
    Some(e)
}

fn read_event(x: &Xml, ev_node: NodeId, path: &[u8], defs: &BTreeMap<Vec<u8>, SoundDef>) -> Event {
    let e = Some(ev_node);
    let mut ev = Event {
        name: [path, b"/", &x.text(e, b"name", b"")].concat(),
        volume: db_to_gain(x.num(e, b"volume_db")),
        volume_rand_db: x.num(e, b"volume_randomization"),
        pitch: x.num(e, b"pitch"),
        pitch_rand: x.num(e, b"pitch_randomization"),
        is_3d: x.text(e, b"mode", b"") == b"x_3d",
        pan_level: x.num(e, b"panlevel3d"),
        reverb_gain: powf(10.0, x.num(e, b"reverblevel_db") / 20.0),
        one_shot: x.text(e, b"oneshot", b"Yes") != b"No",
        min_dist: x.num(e, b"mindistance"),
        max_dist: x.num(e, b"maxdistance"),
        rolloff: match x.text(e, b"rolloff", b"").as_slice() {
            b"Linear" => ROLLOFF_LINEAR,
            b"LinearSquare" => ROLLOFF_LINEAR_SQUARE,
            b"Logarithmic" => ROLLOFF_LOG,
            b"Custom" => ROLLOFF_CUSTOM,
            _ => ROLLOFF_LOG,
        },
        max_playbacks: x.num(e, b"maxplaybacks") as i32,
        max_behavior: match x.text(e, b"maxplaybacks_behavior", b"").as_slice() {
            b"Steal_oldest" => STEAL_OLDEST,
            b"Steal_newest" => STEAL_NEWEST,
            b"Steal_quietest" => STEAL_QUIETEST,
            b"Just_fail" => JUST_FAIL,
            b"Just_fail_if_quietest" => JUST_FAIL_IF_QUIETEST,
            _ => STEAL_OLDEST,
        },
        fade_in: x.num(e, b"fadein_time") / 1000.0,
        fade_out: x.num(e, b"fadeout_time") / 1000.0,
        category_type: category_of(&x.text(e, b"category", b"")),
        ..Event::default()
    };

    for pn in x.children(ev_node, b"parameter") {
        let p = Some(pn);
        let name = x.text(p, b"name", b"");
        let sustain = x.child(p, b"sustainpoint");
        let builtin = match name.as_slice() {
            b"(distance)" => 1,
            b"(listener angle)" => 2,
            b"(event angle)" => 3,
            _ => 0,
        };
        ev.params.push(Param {
            min: x.num(p, b"rangemin"),
            max: x.num(p, b"rangemax"),
            velocity: x.num(p, b"velocity"),
            seek: x.num(p, b"seekspeed"),
            loop_mode: x.num(p, b"loopmode") as i32,
            sustain: match sustain {
                Some(s) if x.num(Some(s), b"enabled") != 0.0 => x.num(Some(s), b"value"),
                _ => -1.0,
            },
            builtin,
            name,
        });
    }
    let param_idx = |name: &[u8]| {
        ev.params
            .iter()
            .position(|p| p.name == name)
            .map_or(-1, |i| i as i32)
    };
    let mut local_defs: BTreeMap<Vec<u8>, i32> = BTreeMap::new();
    let mut ev_defs: Vec<SoundDef> = Vec::new();
    let mut layers = Vec::new();
    for ln in x.children(ev_node, b"layer") {
        if x.text(Some(ln), b"mute", b"") == b"1" {
            continue;
        }
        let mut layer = Layer {
            param: param_idx(&x.text(Some(ln), b"controlparameter", b"")),
            ..Layer::default()
        };
        for sn in x.children(ln, b"sound") {
            let name = x.text(Some(sn), b"name", b"");
            let Some(def) = defs.get(&name) else {
                continue;
            };
            let local = *local_defs.entry(name).or_insert_with(|| {
                ev_defs.push(def.clone());
                (ev_defs.len() - 1) as i32
            });
            let x0 = x.num(Some(sn), b"x");
            layer.sounds.push(LayerSound {
                def: local,
                volume: x.num(Some(sn), b"volume"),
                loop_mode: x.num(Some(sn), b"loopmode") as i32,
                x0,
                x1: x0 + x.num(Some(sn), b"width"),
            });
        }
        for en in x.children(ln, b"envelope") {
            if let Some(env) = read_envelope(x, en, &param_idx) {
                layer.envelopes.push(env);
            }
        }
        if !layer.sounds.is_empty() {
            layers.push(layer);
        }
    }
    ev.layers = layers;
    ev.defs = ev_defs;
    ev
}

#[derive(Default)]
pub struct Events {
    pub list: Vec<Event>,
    pub map: BTreeMap<Vec<u8>, usize>,
    data_events: BTreeMap<usize, usize>,
    projects: BTreeSet<Vec<u8>>,
}

impl Events {
    /// `msetProjects.insert(key).second`.
    pub fn mark_project(&mut self, key: &[u8]) -> bool {
        self.projects.insert(key.to_vec())
    }

    fn put(&mut self, key: Vec<u8>, ev: Event) {
        match self.map.get(&key).and_then(|&i| self.list.get_mut(i)) {
            Some(slot) => *slot = ev,
            None => {
                self.list.push(ev);
                self.map.insert(key, self.list.len() - 1);
            }
        }
    }

    fn read_group(
        &mut self,
        x: &Xml,
        group: NodeId,
        path: &[u8],
        defs: &BTreeMap<Vec<u8>, SoundDef>,
        count: &mut i32,
    ) {
        let mut nodes = x.children(group, b"event");
        for simple in x.children(group, b"simpleevent") {
            nodes.extend(x.children(simple, b"event"));
        }
        for n in nodes {
            let ev = read_event(x, n, path, defs);
            self.put(to_lower(&ev.name), ev);
            *count += 1;
        }
        for sub in x.children(group, b"eventgroup") {
            let p = [path, b"/", &x.text(Some(sub), b"name", b"")].concat();
            self.read_group(x, sub, &p, defs, count);
        }
    }

    /// Parses a project document into events; `None` when it did not load. Returns the project
    /// name and the number of events read.
    pub fn load_fdp(
        &mut self,
        data: &[u8],
        read_ok: bool,
        project: &[u8],
    ) -> Option<(Vec<u8>, i32)> {
        let mut doc = Doc::new();
        let loaded = doc.load(data, read_ok);
        let root = doc.first_child_element(DOC, None);
        let (true, Some(root)) = (loaded, root) else {
            return None;
        };
        let x = Xml(&doc);
        let mut defs = BTreeMap::new();
        read_defs(&x, root, &mut defs);
        let name = x.text(Some(root), b"name", project);
        let mut count = 0;
        self.read_group(&x, root, &name, &defs, &mut count);
        Some((name, count))
    }

    fn project_events<'a>(&'a self, key: &[u8]) -> impl Iterator<Item = &'a Event> + 'a {
        let prefix = [key, b"/"].concat();
        self.map
            .iter()
            .filter(move |(k, _)| k.starts_with(&prefix))
            .filter_map(|(_, &i)| self.list.get(i))
    }

    /// Banks and samples project `key`'s events need, both sorted.
    pub fn wanted(&self, key: &[u8]) -> BTreeMap<Vec<u8>, BTreeSet<Vec<u8>>> {
        let mut by_bank: BTreeMap<Vec<u8>, BTreeSet<Vec<u8>>> = BTreeMap::new();
        for ev in self.project_events(key) {
            for w in ev.defs.iter().flat_map(|d| &d.waves) {
                by_bank
                    .entry(w.bank.clone())
                    .or_default()
                    .insert(w.sample.clone());
            }
        }
        by_bank
    }

    /// Fills the file lists of project `key`'s events not loaded yet; `files` maps
    /// `bank__sample` to the cached file.
    pub fn assign_files(&mut self, key: &[u8], files: &BTreeMap<Vec<u8>, Vec<u8>>) {
        let prefix = [key, b"/"].concat();
        let idx: Vec<usize> = self
            .map
            .iter()
            .filter(|(k, _)| k.starts_with(&prefix))
            .map(|(_, &i)| i)
            .collect();
        for i in idx {
            let Some(ev) = self.list.get_mut(i).filter(|e| !e.loaded) else {
                continue;
            };
            for d in &mut ev.defs {
                for w in &d.waves {
                    let k = [w.bank.as_slice(), b"__", &w.sample].concat();
                    d.files.push(files.get(&k).cloned().unwrap_or_default());
                }
            }
            ev.loaded = true;
        }
    }

    /// The project part of an event name; `None` where `GetEvent` gives up before loading.
    pub fn project_of(name: &[u8]) -> Option<&[u8]> {
        if name.is_empty() || !file_ext(name).is_empty() {
            return None;
        }
        let slash = name.iter().position(|&b| b == b'/')?;
        name.get(..slash)
    }

    pub fn find(&self, name: &[u8]) -> Option<usize> {
        self.map.get(&to_lower(name)).copied()
    }

    /// `FileEvent`: the synthetic event playing one file.
    pub fn file_event(&mut self, file: &[u8], looped: bool, stream: bool) -> usize {
        let mut key = [b"file:".as_slice(), &to_lower(file)].concat();
        if looped {
            key.extend_from_slice(b":loop");
        }
        if stream {
            key.extend_from_slice(b":stream");
        }
        let idx = match self.map.get(&key) {
            Some(&i) => i,
            None => {
                self.list.push(Event::default());
                self.map.insert(key, self.list.len() - 1);
                self.list.len() - 1
            }
        };
        if let Some(ev) = self.list.get_mut(idx).filter(|e| !e.loaded) {
            ev.name = file.to_vec();
            ev.is_3d = false;
            ev.one_shot = !looped;
            ev.stream = stream;
            ev.defs = vec![SoundDef {
                files: vec![file.to_vec()],
                ..SoundDef::default()
            }];
            ev.layers = vec![Layer {
                param: -1,
                sounds: vec![LayerSound {
                    def: 0,
                    volume: 1.0,
                    loop_mode: i32::from(!looped),
                    x0: 0.0,
                    x1: 1.0,
                }],
                envelopes: Vec::new(),
            }];
            ev.loaded = true;
        }
        idx
    }

    pub fn set_data(&mut self, idx: usize, data: usize) {
        if let Some(ev) = self.list.get_mut(idx) {
            ev.data = data;
        }
        self.data_events.insert(data, idx);
    }

    pub fn event_of_data(&self, data: usize) -> Option<usize> {
        self.data_events.get(&data).copied()
    }
}
