//! S5: `cSomaLuxInputHandler` (`SomaLuxPlayer.cpp`) on the Rust path: the action table, gamepad
//! profiles, per-frame edge latching, the mouse-look smoothing and the `cLuxInputHandler` natives.
//! The C++ object stays as a shell; `cInput` and the script engine are reached through `InHost`.

use super::game::{self, Edge};
use super::player;
use crate::string;
use std::cell::RefCell;
use std::collections::VecDeque;
use std::ffi::c_void;

pub const MAX_ACTIONS: usize = 256;

#[derive(Clone, Debug, PartialEq)]
pub struct Action {
    pub name: Vec<u8>,
    pub id: i32,
    pub axis: i32,
    pub mul: f32,
    pub analog_id: i32,
    pub gamepad: bool,
}

impl Action {
    pub fn plain(name: &[u8], id: i32) -> Self {
        Self {
            name: name.to_vec(),
            id,
            axis: -1,
            mul: 0.0,
            analog_id: -1,
            gamepad: false,
        }
    }
}

#[derive(Default, Clone, Debug, PartialEq)]
pub struct Preset {
    pub name: Vec<u8>,
    pub actions: Vec<i32>,
    pub bindings: Vec<Vec<u8>>,
    pub analog: Vec<bool>,
}

#[derive(Default, Clone, Debug, PartialEq)]
pub struct Profile {
    pub name: Vec<u8>,
    pub prefix: Vec<u8>,
    pub buttons: Vec<Vec<u8>>,
    pub axes: Vec<Vec<u8>>,
    pub dpad: Vec<u32>,
    pub presets: Vec<Preset>,
}

/// What the input handler needs from `cInput`, the game and the script engine.
pub trait InHost {
    fn action_exists_id(&self, id: i32) -> bool;
    fn action_exists_name(&self, name: &[u8]) -> bool;
    fn action_destroy_id(&self, id: i32);
    fn action_destroy_name(&self, name: &[u8]);
    fn action_create(&self, name: &[u8], id: i32);
    fn action_create_mouse(&self, name: &[u8], id: i32, button: i32);
    fn action_id_of(&self, name: &[u8]) -> Option<i32>;
    fn action_triggered(&self, id: i32) -> bool;
    fn add_key(&self, id: i32, key: &[u8]);
    fn add_button(&self, id: i32, button: &[u8]);
    fn key_down(&self, i: i32) -> bool;
    fn key_name(&self, i: i32) -> Vec<u8>;
    fn button_down(&self, i: i32) -> bool;
    fn button_name(&self, i: i32) -> Vec<u8>;
    fn warn(&self, text: &[u8]);
    fn log_actions(&self, count: usize);
    fn call(&self, decl: &[u8]);
    fn call_key_config(&self);
    fn broadcast_action(&self, id: i32, pressed: bool);
    fn broadcast_analog(&self, id: i32, v: [f32; 3]);
    /// Unprocessed mouse motion of the frame.
    fn mouse_rel(&self) -> (i32, i32);
    fn screen_height(&self) -> f32;
    /// `(invert, smooth, mouse sensitivity, gamepad sensitivity)` of the user config, if loaded.
    fn user_config(&self) -> Option<(bool, bool, f32, f32)>;
    fn arr_size(&self, array: *const c_void) -> usize;
    fn arr_at(&self, array: *const c_void, i: usize) -> *const c_void;
    /// A `const tString&` the script can hold for the call, holding `text`.
    fn str_ref(&self, text: &[u8]) -> *mut c_void;
    fn engine_action_name(&self, id: i32) -> Option<Vec<u8>>;
    fn engine_action_binds(&self, id: i32) -> Vec<u8>;
    fn dump(&self, line: &[u8]);
}

pub struct Input {
    pub(super) actions: Vec<Action>,
    pub(super) profiles: Vec<Profile>,
    pub(super) gamepad_profile: Vec<u8>,
    pub(super) gamepad_preset: Vec<u8>,
    pub(super) down: [bool; MAX_ACTIONS],
    pub(super) prev_down: [bool; MAX_ACTIONS],
    pub(super) prev_keys: Vec<bool>,
    pub(super) prev_buttons: Vec<bool>,
    pub(super) latest_key: Vec<u8>,
    pub(super) invert_mouse: bool,
    pub(super) smooth_mouse: bool,
    pub(super) mouse_sensitivity: f32,
    pub(super) gamepad_sensitivity: f32,
    pub(super) max_smooth: i32,
    pub(super) prev_smooth_mul: f32,
    pub(super) smooth: VecDeque<(f32, f32)>,
}

thread_local! {
    static INPUT: RefCell<Option<Input>> = const { RefCell::new(None) };
}

pub(super) fn st<R>(f: impl FnOnce(&mut Input) -> R) -> Option<R> {
    INPUT
        .try_with(|c| {
            let mut b = c.try_borrow_mut().ok()?;
            b.as_mut().map(f)
        })
        .ok()
        .flatten()
}

impl Input {
    fn edge(&self, id: i32) -> Edge {
        let ok = usize::try_from(id).ok().filter(|&i| i < MAX_ACTIONS);
        let at = |a: &[bool; MAX_ACTIONS]| ok.is_some_and(|i| a.get(i).copied().unwrap_or(false));
        game::action_edge(id, MAX_ACTIONS as i32, at(&self.down), at(&self.prev_down))
    }

    fn profile(&self, name: &[u8]) -> Option<usize> {
        self.profiles.iter().position(|p| p.name == name)
    }

    fn preset(&self) -> Option<&Preset> {
        let p = self.profiles.get(self.profile(&self.gamepad_profile)?)?;
        p.presets
            .iter()
            .rev()
            .find(|p| p.name == self.gamepad_preset)
    }

    pub fn action_name(&self, id: i32, gamepad: bool) -> Vec<u8> {
        let mut any: Option<&Action> = None;
        for a in self.actions.iter().filter(|a| a.id == id) {
            if a.gamepad == gamepad {
                return a.name.clone();
            }
            any = Some(a);
        }
        any.map(|a| a.name.clone()).unwrap_or_default()
    }

    fn actions_for_control(
        &self,
        profile: &[u8],
        preset: &[u8],
        control: &[u8],
        out: &mut Vec<u8>,
    ) {
        let Some(p) = self.profile(profile).and_then(|i| self.profiles.get(i)) else {
            return;
        };
        for pr in p.presets.iter().filter(|x| x.name == preset) {
            for (i, &id) in pr.actions.iter().enumerate() {
                if pr.bindings.get(i).is_some_and(|b| b == control) {
                    let analog = pr.analog.get(i).copied().unwrap_or(false);
                    out.extend_from_slice(&self.action_name(id, analog));
                    out.push(b'/');
                }
            }
            if !out.is_empty() {
                out.pop();
            }
        }
    }

    fn layout(&self, input: &[u8]) -> (bool, Vec<u8>, Vec<u8>) {
        let low = string::to_lower(input);
        let parts: Vec<&[u8]> = string::split(&low, Some(b"."), true).collect();
        let part = |i: usize| parts.get(i).copied().unwrap_or_default();
        let profile = (parts.len() >= 4 && part(0) == b"gamepad")
            .then(|| self.profile(&self.gamepad_profile))
            .flatten()
            .and_then(|i| self.profiles.get(i));
        let Some(p) = profile else {
            return (false, Vec::new(), Vec::new());
        };
        let idx = string::token_to_int(part(3));
        let list = match part(2) {
            b"button" => Some(&p.buttons),
            b"axis" => Some(&p.axes),
            _ => None,
        };
        let layout = usize::try_from(idx)
            .ok()
            .and_then(|i| list.and_then(|l| l.get(i)));
        match layout {
            Some(l) => (true, p.prefix.clone(), l.clone()),
            None => (true, Vec::new(), Vec::new()),
        }
    }
}

pub fn init(h: &dyn InHost, max_smooth: i32, prev_smooth_mul: f32, keys: usize, buttons: usize) {
    let fresh = Input {
        actions: Vec::new(),
        profiles: Vec::new(),
        gamepad_profile: Vec::new(),
        gamepad_preset: Vec::new(),
        down: [false; MAX_ACTIONS],
        prev_down: [false; MAX_ACTIONS],
        prev_keys: vec![false; keys],
        prev_buttons: vec![false; buttons],
        latest_key: Vec::new(),
        invert_mouse: false,
        smooth_mouse: true,
        mouse_sensitivity: 1.0,
        gamepad_sensitivity: 2.0,
        max_smooth,
        prev_smooth_mul,
        smooth: VecDeque::new(),
    };
    let _ = INPUT.try_with(|c| {
        if let Ok(mut b) = c.try_borrow_mut() {
            *b = Some(fresh);
        }
    });
    const MOUSE: [&[u8]; 5] = [
        b"LeftClick",
        b"MiddleClick",
        b"RightClick",
        b"ScrollUp",
        b"ScrollDown",
    ];
    for (i, name) in MOUSE.iter().enumerate() {
        if !h.action_exists_name(name) {
            h.action_create_mouse(name, 199_004 + i as i32, i as i32);
        }
    }
}

pub fn create_action(h: &dyn InHost, a: Action) {
    if h.action_exists_id(a.id) {
        h.action_destroy_id(a.id);
    }
    if h.action_exists_name(&a.name) {
        h.action_destroy_name(&a.name);
    }
    st(|s| {
        if let Some(i) = s.actions.iter().position(|x| x.id == a.id) {
            s.actions.remove(i);
        }
    });
    h.action_create(&a.name, a.id);
    st(|s| s.actions.push(a));
}

pub fn create_action_input(h: &dyn InHost, input: &[u8], id: i32) {
    if !h.action_exists_id(id) {
        let mut m = b"Could not add input ".to_vec();
        m.extend_from_slice(cstr(input));
        m.extend_from_slice(format!(" for action {id}. Action does not exist!\n").as_bytes());
        h.warn(&m);
        return;
    }
    let parts: Vec<&[u8]> = string::split(input, Some(b"."), true).collect();
    let (Some(&kind), Some(&arg)) = (parts.first(), parts.get(1)) else {
        return;
    };
    match kind {
        b"Keyboard" => h.add_key(id, arg),
        b"MouseButton" => h.add_button(id, arg),
        _ => {}
    }
}

fn cstr(s: &[u8]) -> &[u8] {
    s.split(|&c| c == 0).next().unwrap_or_default()
}

pub fn load_user_config(h: &dyn InHost) {
    let Some((invert, smooth, mouse, pad)) = h.user_config() else {
        return;
    };
    st(|s| {
        s.invert_mouse = invert;
        s.smooth_mouse = smooth;
        s.mouse_sensitivity = mouse;
        s.gamepad_sensitivity = pad;
    });
}

pub fn load_script(h: &dyn InHost) {
    st(|s| s.profiles.clear());
    h.call(b"void CreateGamepadProfiles()");
    h.call(b"void CreateInputLayoutMapping()");
    load_key_config(h);
}

pub fn load_key_config(h: &dyn InHost) {
    let ids: Vec<i32> = st(|s| s.actions.iter().map(|a| a.id).collect()).unwrap_or_default();
    for id in ids {
        if h.action_exists_id(id) {
            h.action_destroy_id(id);
        }
    }
    st(|s| s.actions.clear());
    h.call(b"void CreateActions()");
    h.call_key_config();
    h.log_actions(st(|s| s.actions.len()).unwrap_or(0));
}

pub fn latch(h: &dyn InHost) {
    for i in 0..MAX_ACTIONS {
        let down = h.action_exists_id(i as i32) && h.action_triggered(i as i32);
        st(|s| {
            if let (Some(p), Some(d)) = (s.prev_down.get_mut(i), s.down.get_mut(i)) {
                *p = *d;
                *d = down;
            }
        });
    }
    st(|s| s.latest_key.clear());
    let nkeys = st(|s| s.prev_keys.len()).unwrap_or(0);
    for i in 0..nkeys {
        let down = h.key_down(i as i32);
        let name = (down
            && st(|s| !s.prev_keys.get(i).copied().unwrap_or(true) && s.latest_key.is_empty())
                .unwrap_or(false))
        .then(|| h.key_name(i as i32));
        st(|s| {
            if let Some(n) = name {
                s.latest_key = [b"Keyboard.".as_slice(), &n].concat();
            }
            if let Some(p) = s.prev_keys.get_mut(i) {
                *p = down;
            }
        });
    }
    let nbuttons = st(|s| s.prev_buttons.len()).unwrap_or(0);
    for i in 0..nbuttons {
        let down = h.button_down(i as i32);
        let name = (down
            && st(|s| !s.prev_buttons.get(i).copied().unwrap_or(true) && s.latest_key.is_empty())
                .unwrap_or(false))
        .then(|| h.button_name(i as i32));
        st(|s| {
            if let Some(n) = name {
                s.latest_key = [b"MouseButton.".as_slice(), &n].concat();
            }
            if let Some(p) = s.prev_buttons.get_mut(i) {
                *p = down;
            }
        });
    }
}

pub fn clear_gui_input() {
    st(|s| {
        let menu: Vec<usize> = s
            .actions
            .iter()
            .filter(|a| a.name.starts_with(b"Menu"))
            .filter_map(|a| usize::try_from(a.id).ok().filter(|&i| i < MAX_ACTIONS))
            .collect();
        for i in menu {
            if let (Some(&d), Some(p)) = (s.down.get(i), s.prev_down.get_mut(i)) {
                *p = d;
            }
        }
    });
}

pub fn update(h: &dyn InHost, game_input: bool) {
    if !game_input {
        return;
    }
    let mut i = 0;
    while let Some(Some(a)) = st(|s| s.actions.get(i).cloned()) {
        i += 1;
        if a.gamepad {
            continue;
        }
        match st(|s| s.edge(a.id)) {
            Some(Edge::Down) => h.broadcast_action(a.id, true),
            Some(Edge::Up) => h.broadcast_action(a.id, false),
            _ => {}
        }
        let triggered = a.analog_id >= 0 && (0..3).contains(&a.axis) && h.action_triggered(a.id);
        if let Some((axis, mul)) = game::analog_fire(a.analog_id, a.axis, a.mul, triggered) {
            let mut v = [0.0f32; 3];
            if let Some(c) = v.get_mut(axis) {
                *c = mul;
            }
            h.broadcast_analog(a.analog_id, v);
        }
    }
    let (rx, ry) = h.mouse_rel();
    let height = h.screen_height();
    let Some((smooth, sens, max, prev_mul, invert)) = st(|s| {
        (
            s.smooth_mouse,
            s.mouse_sensitivity,
            s.max_smooth,
            s.prev_smooth_mul,
            s.invert_mouse,
        )
    }) else {
        return;
    };
    let mut v = player::mouse_vector(rx, ry, sens, height);
    if smooth {
        v = st(|s| {
            s.smooth.push_front(v);
            if player::pops_oldest_sample(s.smooth.len(), max) {
                s.smooth.pop_back();
            }
            let samples: Vec<(f32, f32)> = s.smooth.iter().copied().collect();
            player::smooth_mouse(&samples, prev_mul)
        })
        .unwrap_or(v);
    }
    if invert {
        v.1 = -v.1;
    }
    if v.0 != 0.0 || v.1 != 0.0 {
        h.broadcast_analog(0, [v.0, v.1, 0.0]);
    }
}

fn list<T>(items: &[T], one: impl Fn(&T) -> Vec<u8>) -> Vec<u8> {
    let mut out = b"[".to_vec();
    for i in items {
        out.extend_from_slice(&one(i));
        out.push(b',');
    }
    out.push(b']');
    out
}

fn join(parts: &[&[u8]]) -> Vec<u8> {
    parts.join(&b' ')
}

fn num(v: impl std::fmt::Display) -> Vec<u8> {
    v.to_string().into_bytes()
}

/// The `OHPL_S5_DUMP` lines of the handler state after the key config is loaded.
pub fn dump_state(h: &dyn InHost) {
    let Some((cfg, actions, profiles)) = st(|s| {
        (
            (
                s.invert_mouse,
                s.smooth_mouse,
                s.mouse_sensitivity,
                s.gamepad_sensitivity,
                s.max_smooth,
                s.prev_smooth_mul,
            ),
            s.actions.clone(),
            s.profiles.clone(),
        )
    }) else {
        return;
    };
    h.dump(&join(&[
        b"in cfg",
        &num(u8::from(cfg.0)),
        &num(u8::from(cfg.1)),
        format!("{:08x}", cfg.2.to_bits()).as_bytes(),
        format!("{:08x}", cfg.3.to_bits()).as_bytes(),
        &num(cfg.4),
        format!("{:08x}", cfg.5.to_bits()).as_bytes(),
    ]));
    for a in &actions {
        h.dump(&join(&[
            b"in action",
            &num(a.id),
            &a.name,
            &num(a.axis),
            format!("{:08x}", a.mul.to_bits()).as_bytes(),
            &num(a.analog_id),
            &num(u8::from(a.gamepad)),
        ]));
    }
    for i in 0..MAX_ACTIONS as i32 {
        let Some(name) = h.engine_action_name(i) else {
            continue;
        };
        let (plain, pad) =
            st(|s| (s.action_name(i, false), s.action_name(i, true))).unwrap_or_default();
        let wrap = |b: &[u8]| [b"[".as_slice(), b, b"]"].concat();
        h.dump(&join(&[
            b"in engine",
            &num(i),
            &name,
            &wrap(&h.engine_action_binds(i)),
            &wrap(&plain),
            &wrap(&pad),
        ]));
    }
    let commas = |v: Vec<Vec<u8>>| list(&v, Clone::clone);
    for p in &profiles {
        h.dump(&join(&[
            b"in profile",
            &p.name,
            &p.prefix,
            &commas(p.buttons.clone()),
            &commas(p.axes.clone()),
            &list(&p.dpad, |u| num(*u as i32)),
        ]));
        for r in &p.presets {
            h.dump(&join(&[
                b"in preset",
                &p.name,
                &r.name,
                &list(&r.actions, |a| num(*a)),
                &commas(r.bindings.clone()),
                &list(
                    &r.analog,
                    |b| if *b { b"1".to_vec() } else { b"0".to_vec() },
                ),
            ]));
            for control in &r.bindings {
                let mut out = Vec::new();
                st(|s| s.actions_for_control(&p.name, &r.name, control, &mut out));
                h.dump(&join(&[
                    b"in control",
                    &p.name,
                    &r.name,
                    control,
                    b"->",
                    &out,
                ]));
            }
        }
    }
}

pub fn became_down(id: i32) -> bool {
    st(|s| s.edge(id) == Edge::Down).unwrap_or(false)
}

pub fn was_triggered(id: i32) -> bool {
    st(|s| s.edge(id) == Edge::Up).unwrap_or(false)
}

pub fn create_profile(
    name: &[u8],
    prefix: &[u8],
    buttons: Vec<Vec<u8>>,
    axes: Vec<Vec<u8>>,
    dpad: Vec<u32>,
) {
    st(|s| {
        let i = s.profile(name).unwrap_or_else(|| {
            s.profiles.push(Profile::default());
            s.profiles.len() - 1
        });
        if let Some(p) = s.profiles.get_mut(i) {
            *p = Profile {
                name: name.to_vec(),
                prefix: prefix.to_vec(),
                buttons,
                axes,
                dpad,
                presets: std::mem::take(&mut p.presets),
            };
        }
    });
}

pub fn add_preset(profile: &[u8], preset: Preset) {
    st(|s| {
        if let Some(p) = s.profile(profile).and_then(|i| s.profiles.get_mut(i)) {
            p.presets.push(preset);
        }
    });
}

pub fn action_name(id: i32, gamepad: bool) -> Vec<u8> {
    st(|s| s.action_name(id, gamepad)).unwrap_or_default()
}

pub fn set_mapping(profile: &[u8], preset: &[u8]) {
    st(|s| {
        s.gamepad_profile = profile.to_vec();
        s.gamepad_preset = preset.to_vec();
    });
}

pub fn mapping_action_num() -> i32 {
    st(|s| s.preset().map_or(0, |p| p.actions.len() as i32)).unwrap_or(0)
}

#[derive(Default, Debug, PartialEq)]
pub struct Mapped {
    pub found: bool,
    /// Written to the `alAction` out parameter (`-1` without a preset).
    pub action: Option<i32>,
    /// Written to `asPrimary` / `abAnalog`.
    pub primary: Option<(Vec<u8>, bool)>,
}

pub fn mapping_action(id: i32) -> Mapped {
    st(|s| {
        let Some(p) = s.preset() else {
            return Mapped {
                action: Some(-1),
                ..Mapped::default()
            };
        };
        let i = usize::try_from(id)
            .ok()
            .filter(|&i| i < p.actions.len() && i < p.bindings.len() && i < p.analog.len());
        match i.and_then(|i| Some((p.actions.get(i)?, p.bindings.get(i)?, p.analog.get(i)?))) {
            Some((&a, b, &an)) => Mapped {
                found: true,
                action: Some(a),
                primary: Some((b.clone(), an)),
            },
            None => Mapped::default(),
        }
    })
    .unwrap_or_default()
}

pub fn actions_for_control(profile: &[u8], preset: &[u8], control: &[u8], out: &mut Vec<u8>) {
    st(|s| s.actions_for_control(profile, preset, control, out));
}

pub fn layout(input: &[u8]) -> (Vec<u8>, Vec<u8>) {
    st(|s| s.layout(input))
        .map(|(_, p, l)| (p, l))
        .unwrap_or_default()
}

pub fn latest_key() -> Vec<u8> {
    st(|s| s.latest_key.clone()).unwrap_or_default()
}

pub struct Settings {
    pub mouse: f32,
    pub gamepad: f32,
    pub smooth: bool,
    pub invert: bool,
}

pub fn settings() -> Settings {
    st(|s| Settings {
        mouse: s.mouse_sensitivity,
        gamepad: s.gamepad_sensitivity,
        smooth: s.smooth_mouse,
        invert: s.invert_mouse,
    })
    .unwrap_or(Settings {
        mouse: 1.0,
        gamepad: 2.0,
        smooth: true,
        invert: false,
    })
}

pub fn tune(f: impl FnOnce(&mut Input)) {
    st(f);
}

pub fn reset_smooth() {
    st(|s| s.smooth.clear());
}
