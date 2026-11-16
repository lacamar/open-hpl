//! `cSomaImGui`: the script-facing immediate-mode UI (state, fades, timers, modifiers, groups,
//! layouts, focus, draw recording and the widgets). The `cGuiSet`, textures and fonts stay C++
//! behind [`Host`]; script struct blocks are read at the offsets SOMA's binary uses.
#![allow(clippy::too_many_arguments)]

use super::imgui::{
    Actions, Fade, Focus, Layout, MouseIn, NavEntry, Prev, State, WidgetIn, advance,
    align as align_pos, default_or_current_float, default_or_current_int, fade_advance, fade_apply,
    fade_blocked, fade_make, layout as place, mouse_map, mouse_over, update_ui_movement,
    widget_base,
};
use super::imgui_widgets::{
    self as widgets, ButtonFlags, COLOR_FOCUS, COLOR_TRIG, COLOR_TRIG_FOCUS, MultiIn, SliderIn,
    color_mul, gauge_fill, text_origin,
};
use super::imgui_window::{WindowIn, window};
use super::s8d_sd::{Sd, off};
use super::s8d_text::SubAction;
use crate::somalux::scriptable::{easing, hash64};
use std::cell::Cell;
use std::collections::{BTreeMap, HashMap};

/// Engine object pointer (`cGuiGfxElement`, `iFontData`, `cGuiSet`, clip region); 0 is none.
pub type Ptr = usize;

pub const CLIP_BEGIN: i32 = 1;
pub const CLIP_END: i32 = 2;
const ALIGN_LEFT: i32 = 0;
const ALIGN_CENTER: i32 = 2;
const MATERIAL_FONT_NORMAL: i32 = 2;
const WHITE: [f32; 4] = [1.0; 4];

/// What the C++ host needs to find or create a gui image.
pub struct GfxDesc<'a> {
    pub file: &'a [u8],
    pub ty: i32,
    pub material: i32,
    pub uv_min: [f32; 2],
    pub uv_max: [f32; 2],
    pub alpha_limit: [f32; 2],
}

pub trait Host {
    fn string(&self, p: Ptr) -> Vec<u8>;
    fn intern(&self, s: &[u8]) -> Ptr;
    fn gfx_element(&self, d: &GfxDesc<'_>) -> Ptr;
    fn gfx_image_size(&self, gfx: Ptr) -> [f32; 2];
    fn gfx_name(&self, gfx: Ptr) -> Vec<u8>;
    fn white(&self) -> Ptr;
    fn font(&self, file: &[u8]) -> Ptr;
    fn font_name(&self, font: Ptr) -> Vec<u8>;
    fn font_length(&self, font: Ptr, size: [f32; 2], text: &[u32]) -> f32;
    fn font_wrap(
        &self,
        font: Ptr,
        width: f32,
        row_h: f32,
        size: [f32; 2],
        text: &[u32],
    ) -> Vec<Vec<u32>>;
    fn draw_gfx(&self, set: Ptr, gfx: Ptr, o: &Op);
    fn draw_font(&self, set: Ptr, font: Ptr, o: &Op);
    fn clip_current(&self, set: Ptr) -> Ptr;
    fn clip_child(&self, parent: Ptr, pos: [f32; 3], size: [f32; 2]) -> Ptr;
    fn clip_set(&self, set: Ptr, clip: Ptr);
    /// Virtual size, virtual offset, 3D flag.
    fn set_info(&self, set: Ptr) -> ([f32; 2], [f32; 2], bool);
    fn screen_size(&self) -> ([f32; 2], [i32; 2]);
    fn create_set(&self, name: &[u8]) -> Ptr;
    fn set_hud_virtual(&self, set: Ptr, size: [f32; 2], offset: [f32; 2]);
    fn set_render(&self, set: Ptr);
    /// A `std::wstring` object's units.
    fn wstr(&self, p: Ptr) -> Vec<u32>;
    fn array_size(&self, a: Ptr) -> usize;
    fn array_at(&self, a: Ptr, i: usize) -> Ptr;
    fn array_resize(&self, a: Ptr, n: usize);
    fn array_push_wstring(&self, a: Ptr, s: &[u32]);
    fn array_push_bool(&self, a: Ptr, b: bool);
    fn array_push_i32(&self, a: Ptr, v: i32);
    /// The bindings of an input action, `None` when the action does not exist.
    fn action_subs(&self, name: &[u8]) -> Option<Vec<SubAction>>;
    /// The "ButtonNames" translation of an input name; empty when there is none.
    fn translate_button(&self, name: &[u8]) -> Vec<u32>;
    fn key_name(&self, key: i32) -> Vec<u8>;
    fn string_duration(&self, text: &[u32]) -> f32;
    fn camtex_create(&self, name: &[u8], size: [i32; 2], fps: u32, fov: f32, near: f32, far: f32);
    fn camtex_destroy(&self, name: &[u8]);
    fn camtex_settings(&self, name: &[u8], fov: f32, near: f32, far: f32);
    fn camtex_matrix(&self, name: &[u8], m: Ptr);
    fn camtex_attach(&self, name: &[u8], ent: Ptr);
}

/// One recorded draw: a gui image, a text, or (both pointers 0) a clip begin/end in `align`.
#[derive(Clone, Default)]
pub struct Op {
    pub gfx: Ptr,
    pub font: Ptr,
    pub text: Vec<u32>,
    pub pos: [f32; 3],
    pub size: [f32; 2],
    pub color: [f32; 4],
    pub material: i32,
    pub align: i32,
    pub angle: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Timer {
    pub id: u64,
    pub time: f32,
    pub repeat: bool,
    pub touched: bool,
}

#[derive(Clone, Copy)]
pub struct Mods {
    pub color_mul: [f32; 4],
    pub text_color_mul: [f32; 4],
    pub use_input: bool,
    pub use_ui_pos: bool,
    pub exp_hori: [f32; 2],
    pub exp_vert: [f32; 2],
    pub rotate_angle: f32,
    pub rotate_custom_pivot: bool,
    pub rotate_pivot: [f32; 2],
}

impl Default for Mods {
    fn default() -> Self {
        Self {
            color_mul: WHITE,
            text_color_mul: WHITE,
            use_input: true,
            use_ui_pos: true,
            exp_hori: [0.0; 2],
            exp_vert: [0.0; 2],
            rotate_angle: 0.0,
            rotate_custom_pivot: false,
            rotate_pivot: [0.0; 2],
        }
    }
}

#[derive(Clone, Copy)]
pub struct Group {
    pub pos: [f32; 3],
    pub size: [f32; 2],
    pub clip: bool,
}

pub fn new_state() -> State {
    State {
        col: WHITE,
        ..State::default()
    }
}

pub fn preload_gfx(h: &dyn Host, file: &[u8], ty: i32) {
    if !file.is_empty() && ty != 4 {
        h.gfx_element(&GfxDesc {
            file,
            ty,
            material: 1,
            uv_min: [0.0; 2],
            uv_max: [1.0; 2],
            alpha_limit: [0.0; 2],
        });
    }
}

pub fn id_of(name: &[u8]) -> u64 {
    hash64(name)
}

pub struct ImGui {
    pub name: Vec<u8>,
    pub name_ptr: Ptr,
    pub set: Ptr,
    pub first_run: bool,
    pub time_step: f32,
    pub time_count: f32,
    pub down: [bool; 10],
    pub triggered: [bool; 10],
    pub used: [bool; 10],
    pub mouse_pos: [f32; 2],
    pub mouse_rel: [f32; 2],
    pub show_mouse: bool,
    pub screen_clear: [f32; 4],
    pub screen_offline_clear: [f32; 4],
    pub cursor_3d: [f32; 2],
    pub show_mouse_auto: bool,
    pub states: BTreeMap<u64, Box<State>>,
    pub fades: BTreeMap<u64, Fade>,
    pub timers: Vec<Timer>,
    pub mods: Mods,
    pub mod_stack: Vec<Mods>,
    pub line_strip: Vec<[f32; 2]>,
    pub align: i32,
    pub groups: Vec<Group>,
    pub layouts: Vec<Layout>,
    pub prev: Prev,
    pub nav: Vec<NavEntry>,
    pub focus: Focus,
    pub items: Vec<Vec<u32>>,
    defaults: HashMap<Vec<u8>, usize>,
    building: Vec<Op>,
    drawn: Vec<Op>,
    pub tmp_v3: [f32; 3],
    pub tmp_v2: [f32; 2],
    pub tmp_font: [f32; 2],
    dump_last: Vec<u8>,
}

thread_local! {
    static CURRENT: Cell<usize> = const { Cell::new(0) };
    static INPUT_FOCUS: Cell<usize> = const { Cell::new(0) };
    static PREV_INPUT_FOCUS: Cell<usize> = const { Cell::new(0) };
    static HUD_FOCUS: Cell<bool> = const { Cell::new(false) };
}

pub fn current() -> *mut ImGui {
    CURRENT.with(Cell::get) as *mut ImGui
}
pub fn set_current(p: *mut ImGui) {
    CURRENT.with(|c| c.set(p as usize));
}
pub fn script_input_focus() -> *mut ImGui {
    INPUT_FOCUS.with(Cell::get) as *mut ImGui
}
pub fn prev_input_focus() -> *mut ImGui {
    PREV_INPUT_FOCUS.with(Cell::get) as *mut ImGui
}
pub fn update_focus_history() {
    PREV_INPUT_FOCUS.with(|c| c.set(INPUT_FOCUS.with(Cell::get)));
}
pub fn game_hud_focus() -> bool {
    HUD_FOCUS.with(Cell::get)
}
pub fn set_game_hud_focus(b: bool) {
    HUD_FOCUS.with(|c| c.set(b));
}

/// `GetInputFocus`: the HUD while it holds the game's focus, else the script's pick.
pub fn input_focus(h: &dyn Host) -> *mut ImGui {
    if game_hud_focus() {
        super::s8d_hud::hud(h)
    } else {
        script_input_focus()
    }
}

/// # Safety
/// `p` is null or a UI made by [`ImGui::new_boxed`].
pub unsafe fn set_input_focus(h: &dyn Host, p: *mut ImGui, show_mouse: bool) {
    if !p.is_null() && p as usize != INPUT_FOCUS.with(Cell::get) {
        // SAFETY: handed out by `new_boxed` and never freed.
        let im = unsafe { &mut *p };
        let (size, _, _) = h.set_info(im.set);
        im.cursor_3d = [size[0] * 0.5, size[1] * 0.5];
    }
    INPUT_FOCUS.with(|c| c.set(p as usize));
    if !p.is_null() {
        // SAFETY: as above.
        unsafe { (*p).show_mouse = show_mouse };
    }
}

macro_rules! act {
    ($s:expr) => {
        Actions {
            down: &mut $s.down,
            triggered: &mut $s.triggered,
            used: &mut $s.used,
        }
    };
}

fn add3(a: [f32; 3], b: [f32; 3]) -> [f32; 3] {
    [a[0] + b[0], a[1] + b[1], a[2] + b[2]]
}

fn dump_path() -> Option<&'static std::ffi::OsString> {
    static PATH: std::sync::OnceLock<Option<std::ffi::OsString>> = std::sync::OnceLock::new();
    PATH.get_or_init(|| std::env::var_os("OHPL_S8D_DUMP").filter(|p| !p.is_empty()))
        .as_ref()
}

fn hex(f: f32) -> String {
    format!("{:08x}", f.to_bits())
}

impl ImGui {
    pub fn new_boxed(h: &dyn Host, name: &[u8], set: Ptr) -> *mut ImGui {
        Box::into_raw(Box::new(ImGui {
            name: name.to_vec(),
            name_ptr: h.intern(name),
            set,
            first_run: true,
            time_step: 0.0,
            time_count: 0.0,
            down: [false; 10],
            triggered: [false; 10],
            used: [false; 10],
            mouse_pos: [0.0; 2],
            mouse_rel: [0.0; 2],
            show_mouse: false,
            screen_clear: [0.0; 4],
            screen_offline_clear: [0.0; 4],
            cursor_3d: [0.0; 2],
            show_mouse_auto: true,
            states: BTreeMap::new(),
            fades: BTreeMap::new(),
            timers: Vec::new(),
            mods: Mods::default(),
            mod_stack: Vec::new(),
            line_strip: Vec::new(),
            align: 0,
            groups: Vec::new(),
            layouts: Vec::new(),
            prev: Prev::default(),
            nav: Vec::new(),
            focus: Focus {
                wrap_mode: 3,
                group_flags: 1,
                ..Focus::default()
            },
            items: Vec::new(),
            defaults: HashMap::new(),
            building: Vec::new(),
            drawn: Vec::new(),
            tmp_v3: [0.0; 3],
            tmp_v2: [0.0; 2],
            tmp_font: [0.0; 2],
            dump_last: Vec::new(),
        }))
    }

    pub fn state(&mut self, id: u64) -> &mut State {
        self.states
            .entry(id)
            .or_insert_with(|| Box::new(new_state()))
    }

    fn state_ptr(&mut self, id: u64) -> *mut State {
        std::ptr::from_mut::<State>(
            &mut **self
                .states
                .entry(id)
                .or_insert_with(|| Box::new(new_state())),
        )
    }

    /// `GetOrSetState`: the default sticks only while no field of the state was ever set.
    pub fn get_or_set(&mut self, id: u64, d: Val) -> Val {
        let s = self.state(id);
        if !(s.set_int || s.set_float || s.set_vec || s.set_col) {
            d.put(s);
        }
        if d.is_set(s) { d.read(s) } else { d }
    }

    pub fn set_val(&mut self, id: u64, v: Val) {
        v.put(self.state(id));
    }

    pub fn inc_val(&mut self, id: u64, v: Val) {
        let s = self.state(id);
        let cur = v.read(s);
        let sum = match (cur, v) {
            (Val::Int(a), Val::Int(b)) => Val::Int(a.wrapping_add(b)),
            (Val::Float(a), Val::Float(b)) => Val::Float(a + b),
            (Val::Vec(a), Val::Vec(b)) => Val::Vec([a[0] + b[0], a[1] + b[1], a[2] + b[2]]),
            (Val::Col(a), Val::Col(b)) => {
                Val::Col([a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]])
            }
            _ => v,
        };
        sum.put(s);
    }

    pub fn get_default(&mut self, ty: &[u8]) -> Sd {
        if let Some(p) = self.defaults.get(ty) {
            return Sd(*p as *const u8);
        }
        let p = crate::somascript::s2_api::new_script_struct(ty) as usize;
        self.defaults.insert(ty.to_vec(), p);
        Sd(p as *const u8)
    }

    pub fn set_default(&mut self, ty: &[u8], data: Sd) {
        let d = self.get_default(ty);
        // SAFETY: both are 4096-byte script struct blocks.
        unsafe { std::ptr::copy_nonoverlapping(data.0, d.0.cast_mut(), 4096) };
    }

    pub fn clear_states(&mut self) {
        self.states.clear();
        self.fades.clear();
        self.timers.clear();
        self.first_run = true;
    }

    pub fn begin(&mut self, dt: f32) {
        self.time_step = dt;
        self.time_count += dt;
        self.building.clear();
        self.drawn.clear();
        self.mods = Mods::default();
        self.mod_stack.clear();
        self.groups.clear();
        self.layouts.clear();
        self.align = 0;
        self.items.clear();
        self.nav.clear();
        self.focus.found_focus = false;
        self.focus.wrap_mode = 3;
        self.focus.group_flags = 1;
        if self.focus.mouse_lock > 0 {
            self.focus.mouse_lock -= 1;
        }
        let ids: Vec<u64> = self.fades.keys().copied().collect();
        for id in ids {
            let Some(f) = self.fades.get_mut(&id) else {
                continue;
            };
            let x = fade_advance(f, dt);
            let t = x.map_or(1.0, |x| easing(f.easing, x));
            let f = *f;
            fade_apply(&f, t, self.state(id));
        }
        for t in &mut self.timers {
            t.time -= dt;
            t.touched = false;
        }
    }

    pub fn end(&mut self, h: &dyn Host) {
        self.timers
            .retain(|t| !(t.time <= 0.0 || (t.repeat && !t.touched)));
        self.update_ui_movement();
        self.first_run = false;
        self.triggered = [false; 10];
        self.used = [false; 10];
        for d in self.down.iter_mut().skip(4) {
            *d = false;
        }
        self.mouse_rel = [0.0; 2];
        if self.show_mouse && std::ptr::eq(self, input_focus(h)) {
            let m = self.get_default(b"__mouse");
            if !m.string(h, off::GFX_FILE).is_empty() {
                let pos = [self.mouse_pos[0], self.mouse_pos[1], 100.0];
                self.draw_gfx(h, m, pos, [-1.0, -1.0], WHITE);
            }
        }
        std::mem::swap(&mut self.drawn, &mut self.building);
        self.building.clear();
    }

    pub fn drawn_count(&self) -> usize {
        self.drawn.len()
    }

    /// The `DebugOps` text of the headless control.
    pub fn debug_ops(&self, h: &dyn Host, max: usize) -> Vec<u8> {
        let mut s = format!("{}: ", self.drawn.len()).into_bytes();
        for op in self.drawn.iter().take(max) {
            let what: Vec<u8> = if op.gfx != 0 {
                b"gfx ".to_vec()
            } else if op.font != 0 {
                let mut w = b"text '".to_vec();
                w.extend(
                    op.text
                        .iter()
                        .take(64)
                        .map(|&c| u8::try_from(c).unwrap_or(b'?')),
                );
                w.extend_from_slice(b"' ");
                w
            } else {
                b"clip ".to_vec()
            };
            s.extend(what);
            let line = format!(
                "{:.1},{:.1},{:.1} {:.1}x{:.1} c={:.2},{:.2},{:.2},{:.2} m={}; ",
                op.pos[0],
                op.pos[1],
                op.pos[2],
                op.size[0],
                op.size[1],
                op.color[0],
                op.color[1],
                op.color[2],
                op.color[3],
                op.material
            );
            s.extend(line.into_bytes());
        }
        let _ = h;
        s
    }

    pub fn draw_all(&mut self, h: &dyn Host) {
        self.dump(h);
        let mut clips = vec![h.clip_current(self.set)];
        if self.screen_clear[3] > 0.0 {
            let white = h.white();
            let (size, _, _) = h.set_info(self.set);
            let o = Op {
                gfx: white,
                pos: [0.0, 0.0, -1.0],
                size,
                color: self.screen_clear,
                material: 1,
                ..Op::default()
            };
            h.draw_gfx(self.set, white, &o);
        }
        for op in &self.drawn {
            if op.gfx == 0 && op.font == 0 {
                if op.align == CLIP_BEGIN {
                    let parent = clips.last().copied().unwrap_or(0);
                    clips.push(h.clip_child(parent, op.pos, op.size));
                } else if clips.len() > 1 {
                    clips.pop();
                }
                h.clip_set(self.set, clips.last().copied().unwrap_or(0));
            } else if op.gfx != 0 {
                h.draw_gfx(self.set, op.gfx, op);
            } else {
                h.draw_font(self.set, op.font, op);
            }
        }
        h.clip_set(self.set, clips.first().copied().unwrap_or(0));
    }

    /// The draw list of the frame as `OHPL_S8D_DUMP` writes it: float bits, text as hex units.
    pub fn dump_text(&self, h: &dyn Host) -> String {
        let mut s = format!(
            "im {} clear={}{}{}{}\n",
            String::from_utf8_lossy(&self.name),
            hex(self.screen_clear[0]),
            hex(self.screen_clear[1]),
            hex(self.screen_clear[2]),
            hex(self.screen_clear[3])
        );
        for o in &self.drawn {
            s += &if o.gfx != 0 {
                format!("g {}", String::from_utf8_lossy(&h.gfx_name(o.gfx)))
            } else if o.font != 0 {
                let t: String = o.text.iter().map(|c| format!("{c:x},")).collect();
                format!("t {t} |{}", String::from_utf8_lossy(&h.font_name(o.font)))
            } else if o.align == CLIP_BEGIN {
                "c+".into()
            } else {
                "c-".into()
            };
            s += &format!(
                " {}{}{} {}{} {}{}{}{} m{} a{} r{}\n",
                hex(o.pos[0]),
                hex(o.pos[1]),
                hex(o.pos[2]),
                hex(o.size[0]),
                hex(o.size[1]),
                hex(o.color[0]),
                hex(o.color[1]),
                hex(o.color[2]),
                hex(o.color[3]),
                o.material,
                o.align,
                hex(o.angle)
            );
        }
        s
    }

    /// `OHPL_S8D_DUMP`: every frame's draw list, repeats of the last frame skipped.
    fn dump(&mut self, h: &dyn Host) {
        let Some(path) = dump_path() else {
            return;
        };
        let s = self.dump_text(h);
        if s.as_bytes() == self.dump_last.as_slice() {
            return;
        }
        self.dump_last = s.clone().into_bytes();
        use std::io::Write;
        if let Ok(mut f) = std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open(path)
        {
            let _ = f.write_all(s.as_bytes());
        }
    }

    #[cfg(test)]
    pub fn set_drawn(&mut self, ops: Vec<Op>) {
        self.drawn = ops;
    }

    pub fn send_action(&mut self, a: i32, down: bool, triggered: bool) {
        let Some(i) = usize::try_from(a).ok().filter(|i| *i < 10) else {
            return;
        };
        if let (Some(d), Some(t)) = (self.down.get_mut(i), self.triggered.get_mut(i)) {
            *d = down;
            *t = *t || triggered;
        }
    }

    pub fn action_triggered(&mut self, a: i32, check_used: bool) -> bool {
        act!(self).triggered(a, check_used)
    }

    pub fn action_is_down(&mut self, a: i32, check_used: bool) -> bool {
        act!(self).is_down(a, check_used)
    }

    pub fn send_mouse_virtual(&mut self, pos: [f32; 2], rel: [f32; 2]) {
        self.mouse_pos = pos;
        self.mouse_rel = [self.mouse_rel[0] + rel[0], self.mouse_rel[1] + rel[1]];
    }

    pub fn send_mouse(&mut self, h: &dyn Host, pos: [i32; 2], rel: [i32; 2]) {
        let (screen, _) = h.screen_size();
        let (virt, offset, is_3d) = h.set_info(self.set);
        let i = MouseIn {
            screen,
            virtual_size: virt,
            virtual_offset: if is_3d { [0.0; 2] } else { offset },
            is_3d,
            cursor_3d: self.cursor_3d,
        };
        let (p, r, c) = mouse_map(&i, pos, rel);
        if is_3d {
            self.cursor_3d = c;
        }
        self.send_mouse_virtual(p, r);
    }

    pub fn fade(
        &mut self,
        id: u64,
        ty: i32,
        goal: [f32; 4],
        time: f32,
        easing: i32,
        replace: bool,
    ) {
        if fade_blocked(self.fades.get(&id), replace) {
            return;
        }
        let f = fade_make(self.state(id), ty, goal, time, easing);
        self.fades.insert(id, f);
    }

    pub fn timer(&self, id: u64) -> Option<usize> {
        self.timers.iter().position(|t| t.id == id)
    }

    pub fn group_pos(&self) -> [f32; 3] {
        self.groups.last().map_or([0.0; 3], |g| g.pos)
    }

    pub fn group_size(&self, h: &dyn Host) -> [f32; 2] {
        self.groups
            .last()
            .map_or_else(|| h.set_info(self.set).0, |g| g.size)
    }

    fn record(&mut self, o: Op) {
        self.building.push(o);
    }

    pub fn clip_begin(&mut self, pos: [f32; 3], size: [f32; 2]) {
        self.record(Op {
            pos,
            size,
            color: WHITE,
            align: CLIP_BEGIN,
            ..Op::default()
        });
    }

    pub fn clip_end(&mut self) {
        self.record(Op {
            color: WHITE,
            align: CLIP_END,
            ..Op::default()
        });
    }

    pub fn push_group(&mut self, g: Group) {
        self.groups.push(g);
        if g.clip {
            self.clip_begin(g.pos, g.size);
        }
    }

    pub fn pop_group(&mut self) {
        let Some(g) = self.groups.last() else {
            return;
        };
        if g.clip {
            self.clip_end();
        }
        self.groups.pop();
    }

    // ---- gfx and fonts

    fn gfx_ptr(&self, h: &dyn Host, g: Sd) -> Ptr {
        let file = g.string(h, off::GFX_FILE);
        h.gfx_element(&GfxDesc {
            file: &file,
            ty: g.i32(off::GFX_TYPE),
            material: g.i32(off::GFX_MATERIAL),
            uv_min: g.v2(off::GFX_UV_MIN),
            uv_max: g.v2(off::GFX_UV_MAX),
            alpha_limit: g.v2(off::GFX_ALPHA_LIMIT),
        })
    }

    /// Full texture size; the official engine ignores mvUVMin/mvUVMax here.
    pub fn gfx_size(&self, h: &dyn Host, g: Sd) -> [f32; 2] {
        let gfx = self.gfx_ptr(h, g);
        if gfx == 0 {
            return [0.0; 2];
        }
        let (a, b) = (g.v2(off::GFX_UV_MIN), g.v2(off::GFX_UV_MAX));
        let uv = [b[0] - a[0], b[1] - a[1]];
        let s = h.gfx_image_size(gfx);
        [
            if uv[0] > 0.0 { s[0] / uv[0] } else { s[0] },
            if uv[1] > 0.0 { s[1] / uv[1] } else { s[1] },
        ]
    }

    pub fn draw_gfx(
        &mut self,
        h: &dyn Host,
        g: Sd,
        pos: [f32; 3],
        size: [f32; 2],
        color: [f32; 4],
    ) {
        let gfx = self.gfx_ptr(h, g);
        if gfx == 0 {
            return;
        }
        let size = if size[0] < 0.0 || size[1] < 0.0 {
            self.gfx_size(h, g)
        } else {
            size
        };
        let color = color_mul(color_mul(color, g.col(off::GFX_COLOR)), self.mods.color_mul);
        self.record(Op {
            gfx,
            pos: add3(pos, g.v3(28)),
            size,
            color,
            material: g.i32(off::GFX_MATERIAL),
            angle: self.mods.rotate_angle,
            ..Op::default()
        });
    }

    /// `cImGuiFrameGfx`: background, then corners at native size and borders stretched between.
    pub fn draw_frame(
        &mut self,
        h: &dyn Host,
        f: Sd,
        pos: [f32; 3],
        size: [f32; 2],
        color: [f32; 4],
    ) {
        const BG: usize = 16;
        const TR: usize = 184;
        const BR: usize = 352;
        const BL: usize = 520;
        const TL: usize = 688;
        const TOP: usize = 856;
        const RIGHT: usize = 1024;
        const BOTTOM: usize = 1192;
        const LEFT: usize = 1360;
        let sz = |s: &Self, o: usize| {
            let g = f.at(o);
            if g.string(h, off::GFX_FILE).is_empty() {
                [0.0; 2]
            } else {
                s.gfx_size(h, g)
            }
        };
        let (tl, tr, bl, br) = (sz(self, TL), sz(self, TR), sz(self, BL), sz(self, BR));
        let (top, bottom, left, right) = (
            sz(self, TOP)[1],
            sz(self, BOTTOM)[1],
            sz(self, LEFT)[0],
            sz(self, RIGHT)[0],
        );
        let (x0, y0, x1, y1, z) = (pos[0], pos[1], pos[0] + size[0], pos[1] + size[1], pos[2]);
        self.draw_gfx(
            h,
            f.at(BG),
            [x0 + left, y0 + top, z],
            [size[0] - left - right, size[1] - top - bottom],
            color,
        );
        let part = |s: &mut Self, o: usize, x: f32, y: f32, w: f32, hh: f32| {
            let g = f.at(o);
            if w > 0.0 && hh > 0.0 && !g.string(h, off::GFX_FILE).is_empty() {
                s.draw_gfx(h, g, [x, y, z], [w, hh], color);
            }
        };
        part(self, TOP, x0 + tl[0], y0, size[0] - tl[0] - tr[0], top);
        part(
            self,
            BOTTOM,
            x0 + bl[0],
            y1 - bottom,
            size[0] - bl[0] - br[0],
            bottom,
        );
        part(self, LEFT, x0, y0 + tl[1], left, size[1] - tl[1] - bl[1]);
        part(
            self,
            RIGHT,
            x1 - right,
            y0 + tr[1],
            right,
            size[1] - tr[1] - br[1],
        );
        part(self, TL, x0, y0, tl[0], tl[1]);
        part(self, TR, x1 - tr[0], y0, tr[0], tr[1]);
        part(self, BL, x0, y1 - bl[1], bl[0], bl[1]);
        part(self, BR, x1 - br[0], y1 - br[1], br[0], br[1]);
    }

    pub fn font(&mut self, h: &dyn Host, f: Sd) -> Ptr {
        let mut file = f.string(h, off::FONT_FILE);
        if file.is_empty() {
            let d = self.get_default(b"__font");
            file = d.string(h, off::FONT_FILE);
            if file.is_empty() {
                file = b"sansation_medium_bold.fnt".to_vec();
            }
        }
        h.font(&file)
    }

    pub fn font_size(&mut self, f: Sd, mul: f32) -> [f32; 2] {
        let mut s = f.v2(off::FONT_SIZE);
        if s[0] <= 0.0 || s[1] <= 0.0 {
            s = self.get_default(b"__font").v2(off::FONT_SIZE);
        }
        [s[0] * mul, s[1] * mul]
    }

    pub fn font_length(&mut self, h: &dyn Host, f: Sd, mul: f32, text: &[u32]) -> f32 {
        let font = self.font(h, f);
        if font == 0 {
            return 0.0;
        }
        let size = self.font_size(f, mul);
        h.font_length(font, size, text)
    }

    pub fn draw_font(
        &mut self,
        h: &dyn Host,
        text: &[u32],
        f: Sd,
        pos: [f32; 3],
        align: i32,
        mul: [f32; 2],
        color: [f32; 4],
    ) {
        let font = self.font(h, f);
        if font == 0 || text.is_empty() {
            return;
        }
        let s = self.font_size(f, 1.0);
        let c = color_mul(
            color_mul(color, f.col(off::FONT_COLOR)),
            self.mods.text_color_mul,
        );
        self.record(Op {
            font,
            text: text.to_vec(),
            pos,
            size: [s[0] * mul[0], s[1] * mul[1]],
            color: color_mul(c, self.mods.color_mul),
            material: MATERIAL_FONT_NORMAL,
            align,
            ..Op::default()
        });
    }

    pub fn draw_text(
        &mut self,
        h: &dyn Host,
        text: &[u32],
        f: Sd,
        color: [f32; 4],
        align: i32,
        pos: [f32; 3],
        size: [f32; 2],
        mul: f32,
    ) {
        let fs = self.font_size(f, mul);
        let o = text_origin(pos, size, fs, align);
        self.draw_font(h, text, f, o, align, [mul, mul], color);
    }

    // ---- layout and widgets

    pub fn layout(
        &self,
        pos: &mut [f32; 3],
        size: &mut [f32; 2],
        default: [f32; 2],
        do_align: bool,
    ) {
        let origin = self
            .layouts
            .last()
            .map_or_else(|| self.group_pos(), |l| l.cursor);
        place(pos, size, default, origin, do_align, self.align);
    }

    pub fn align_pos(&self, pos: [f32; 3], size: [f32; 2], mode: i32) -> [f32; 3] {
        align_pos(pos, size, mode)
    }

    fn advance(&mut self, pos: [f32; 3], size: [f32; 2], updated: bool) {
        self.prev.pos = pos;
        self.prev.size = size;
        self.prev.updated = updated;
        if let Some(l) = self.layouts.last_mut() {
            advance(l, pos, size);
        }
    }

    pub fn mouse_over(&self, h: &dyn Host, pos: [f32; 3], size: [f32; 2]) -> bool {
        mouse_over(
            self.mods.use_input,
            std::ptr::eq(self, input_focus(h)),
            self.mouse_pos,
            pos,
            size,
        )
    }

    fn widget_base(
        &mut self,
        h: &dyn Host,
        id: u64,
        pos: [f32; 3],
        size: [f32; 2],
        st: *mut State,
    ) -> bool {
        let i = WidgetIn {
            use_input: self.mods.use_input,
            show_mouse: self.show_mouse,
            has_focus: std::ptr::eq(self, input_focus(h)),
            first_run: self.first_run,
            use_ui_pos: self.mods.use_ui_pos,
            mouse_rel: self.mouse_rel,
            exp_hori: self.mods.exp_hori,
            exp_vert: self.mods.exp_vert,
            pos,
            size,
            mouse: self.mouse_pos,
        };
        // SAFETY: `st` is a boxed state of this UI that nothing frees during the call.
        let st = unsafe { &mut *st };
        let o = widget_base(id, &i, &mut self.focus, &mut act!(self), st, &mut self.prev);
        if o.has_nav {
            self.nav.push(o.nav);
        }
        o.in_focus
    }

    pub fn set_focus(&mut self, name: &[u8]) {
        let id = if name.is_empty() { 0 } else { id_of(name) };
        self.focus.focus = id;
        self.focus.prev_focus = id;
        self.focus.last_dir = [0, 0];
        self.focus.last_count = 0;
    }

    fn update_ui_movement(&mut self) {
        update_ui_movement(&self.nav, &mut self.focus, &mut act!(self));
    }

    fn widget_base_draw(
        &mut self,
        h: &dyn Host,
        d: Sd,
        pos: [f32; 3],
        size: [f32; 2],
        in_focus: bool,
        triggered: bool,
        focus_gfx: i32,
        trig_gfx: i32,
    ) {
        let col = d.col(off::W_COLOR_BASE);
        if d.bool(off::W_USE_BG) {
            self.draw_gfx(h, d.at(off::W_GFX_BG), pos, size, col);
        }
        if in_focus && focus_gfx >= 0 && d.bool(focus_gfx as usize - 8) {
            self.draw_gfx(
                h,
                d.at(focus_gfx as usize),
                [pos[0], pos[1], pos[2] + 0.05],
                size,
                WHITE,
            );
        }
        if triggered && trig_gfx >= 0 && d.bool(trig_gfx as usize - 4) {
            self.draw_gfx(
                h,
                d.at(trig_gfx as usize),
                [pos[0], pos[1], pos[2] + 0.06],
                size,
                WHITE,
            );
        }
    }

    /// `mode`: 0 button, 1 toggle, 2 repeat, 3/4 multi-toggle item on/off.
    pub fn do_button(
        &mut self,
        h: &dyn Host,
        name: &[u8],
        text: &[u32],
        d: Sd,
        mut pos: [f32; 3],
        mut size: [f32; 2],
        mode: i32,
    ) -> bool {
        self.layout(&mut pos, &mut size, d.v2(off::W_DEFAULT_SIZE), true);
        let id = if name.is_empty() {
            id_of(vec3_string(pos).as_bytes())
        } else {
            id_of(name)
        };
        let st = self.state_ptr(id);
        let over = self.widget_base(h, id, pos, size, st);
        let mouse_over = self.prev.mouse_over;
        let use_trig_gfx = d.bool(off::B_USE_TRIG_GFX);
        let use_focus_gfx = d.bool(off::B_USE_IN_FOCUS_GFX);
        let flags = ButtonFlags {
            use_trig_gfx,
            use_focus_gfx,
            use_trig_color: d.bool(off::B_USE_TRIG_COLOR),
            use_trig_focus_color: d.bool(off::B_USE_TRIG_FOCUS_COLOR),
            use_focus_color: d.bool(off::B_USE_IN_FOCUS_COLOR),
        };
        // SAFETY: as in `widget_base`.
        let r = widgets::button(
            unsafe { &mut *st },
            &mut act!(self),
            over,
            mouse_over,
            mode,
            self.time_step,
            flags,
        );
        let mut col = d.col(off::W_COLOR_BASE);
        col = match r.color {
            COLOR_TRIG_FOCUS => d.col(off::B_COLOR_TRIG_FOCUS),
            COLOR_TRIG => d.col(off::B_COLOR_TRIG),
            COLOR_FOCUS => d.col(off::B_COLOR_IN_FOCUS),
            _ => col,
        };
        let mut text_col = d.col(off::W_COLOR_TEXT);
        if !self.mods.use_input && d.bool(off::W_USE_DISABLED_COLOR) {
            let dis = d.col(off::W_COLOR_DISABLED);
            col = color_mul(col, dis);
            text_col = color_mul(text_col, dis);
        }
        if d.bool(off::B_USE_FRAME) {
            self.draw_frame(h, d.at(off::B_FRAME), pos, size, col);
        } else if d.bool(off::W_USE_BG) {
            self.draw_gfx(h, d.at(off::W_GFX_BG), pos, size, col);
        }
        self.draw_text(
            h,
            text,
            d.at(off::W_FONT),
            text_col,
            d.i32(off::W_FONT_ALIGN),
            pos,
            size,
            1.0,
        );
        if r.trig && use_trig_gfx {
            let mut c = WHITE;
            if r.focus && d.bool(off::B_USE_TRIG_FOCUS_COLOR) {
                c = d.col(off::B_COLOR_TRIG_FOCUS);
            } else if d.bool(off::B_USE_TRIG_COLOR) {
                c = d.col(off::B_COLOR_TRIG);
            }
            self.draw_gfx(
                h,
                d.at(off::B_GFX_TRIG),
                [pos[0], pos[1], pos[2] + 0.15],
                size,
                c,
            );
        }
        if r.focus && use_focus_gfx {
            let c = if d.bool(off::B_USE_IN_FOCUS_COLOR) {
                d.col(off::B_COLOR_IN_FOCUS)
            } else {
                WHITE
            };
            self.draw_gfx(
                h,
                d.at(off::B_GFX_IN_FOCUS),
                [pos[0], pos[1], pos[2] + 0.1],
                size,
                c,
            );
        }
        self.prev.mouse_over = mouse_over;
        self.prev.pressed = r.down;
        self.prev.became_pressed = r.clicked;
        self.advance(pos, size, r.result);
        r.result
    }

    pub fn do_label(
        &mut self,
        h: &dyn Host,
        text: &[u32],
        d: Sd,
        mut pos: [f32; 3],
        mut size: [f32; 2],
        mul: f32,
    ) {
        let font = d.at(off::W_FONT);
        let mut def = d.v2(off::W_DEFAULT_SIZE);
        if def[0] < 0.0 {
            def[0] = self.font_length(h, font, mul, text);
        }
        if def[1] < 0.0 {
            def[1] = self.font_size(font, mul)[1];
        }
        self.layout(&mut pos, &mut size, def, true);
        self.widget_base_draw(h, d, pos, size, false, false, -1, -1);
        self.draw_text(
            h,
            text,
            font,
            d.col(off::W_COLOR_BASE),
            d.i32(off::W_FONT_ALIGN),
            pos,
            size,
            mul,
        );
        self.prev.mouse_over = self.mouse_over(h, pos, size);
        self.advance(pos, size, false);
    }

    pub fn do_image(&mut self, h: &dyn Host, g: Sd, mut pos: [f32; 3], mut size: [f32; 2]) {
        let def = self.gfx_size(h, g);
        self.layout(&mut pos, &mut size, def, true);
        self.draw_gfx(h, g, pos, size, WHITE);
        let over = self.mouse_over(h, pos, size);
        self.prev.mouse_over = over;
        self.prev.in_focus = over;
        self.prev.became_in_focus = false;
        self.prev.became_pressed = over && self.triggered[1];
        self.prev.pressed = over && self.down[1];
        self.advance(pos, size, false);
    }

    #[allow(clippy::too_many_arguments)]
    pub fn do_text_frame(
        &mut self,
        h: &dyn Host,
        text: &[u32],
        edge: [f32; 2],
        row_space: f32,
        start_row: f32,
        d: Sd,
        mut pos: [f32; 3],
        mut size: [f32; 2],
    ) -> f32 {
        // official DoTextFrame skips SetupWidgetRect, so ignores SetAlignment
        self.layout(&mut pos, &mut size, d.v2(off::W_DEFAULT_SIZE), false);
        self.widget_base_draw(h, d, pos, size, false, false, -1, -1);
        let font = d.at(off::W_FONT);
        let fd = self.font(h, font);
        let vf = self.font_size(font, 1.0);
        let rows = if fd != 0 {
            h.font_wrap(fd, size[0] - edge[0] * 2.0, vf[1] + row_space, vf, text)
        } else {
            Vec::new()
        };
        let row_h = vf[1] + row_space;
        let mut y = (-start_row).mul_add(row_h, pos[1] + edge[1]);
        for r in &rows {
            if y >= pos[1] - 0.5 && y + vf[1] <= pos[1] + size[1] + 0.5 {
                self.draw_text(
                    h,
                    r,
                    font,
                    d.col(off::W_COLOR_TEXT),
                    d.i32(off::W_FONT_ALIGN),
                    [pos[0] + edge[0], y, pos[2]],
                    [size[0] - edge[0] * 2.0, vf[1]],
                    1.0,
                );
            }
            y += row_h;
        }
        self.advance(pos, size, false);
        if row_h <= 0.0 {
            return 0.0;
        }
        let n = rows.len() as f32;
        let v = (row_space.mul_add(n, vf[1] * n) - (size[1] - edge[1])) / row_h;
        if v > 0.0 { v } else { 0.0 }
    }

    pub fn do_frame(&mut self, h: &dyn Host, d: Sd, mut pos: [f32; 3], mut size: [f32; 2]) {
        self.layout(&mut pos, &mut size, d.v2(off::W_DEFAULT_SIZE), true);
        self.widget_base_draw(h, d, pos, size, false, false, -1, -1);
        self.draw_frame(h, d.at(320), pos, size, d.col(off::W_COLOR_BASE));
        self.advance(pos, size, false);
    }

    #[allow(clippy::too_many_arguments)]
    pub fn do_slider(
        &mut self,
        h: &dyn Host,
        name: &[u8],
        default: f32,
        min: f32,
        max: f32,
        step: f32,
        d: Sd,
        mut pos: [f32; 3],
        mut size: [f32; 2],
        vertical: bool,
    ) -> f32 {
        self.layout(&mut pos, &mut size, d.v2(off::W_DEFAULT_SIZE), true);
        let id = id_of(name);
        let st = self.state_ptr(id);
        // SAFETY: boxed state of this UI, not freed below.
        default_or_current_float(unsafe { &mut *st }, default);
        let in_focus = self.widget_base(h, id, pos, size, st);
        let over = self.prev.mouse_over;
        // SAFETY: as above.
        let old = unsafe { (*st).float };
        let i = SliderIn {
            in_focus,
            over,
            min,
            max,
            step,
            vertical,
            pos,
            size,
            mouse: self.mouse_pos,
        };
        // SAFETY: as above.
        if widgets::slider(unsafe { &mut *st }, &mut act!(self), &i) {
            self.focus.mouse_lock = 2;
        }
        self.widget_base_draw(h, d, pos, size, in_focus, false, -1, -1);
        let button = d.v2(off::SLIDER_BUTTON_SIZE);
        // SAFETY: as above.
        let value = unsafe { (*st).float };
        let bp = widgets::slider_button_pos(value, [min, max], pos, size, button, vertical);
        if d.bool(off::SLIDER_USE_BUTTON) {
            self.draw_gfx(h, d.at(off::SLIDER_GFX_BUTTON), bp, button, WHITE);
        }
        self.advance(pos, size, value != old);
        value
    }

    pub fn do_checkbox(
        &mut self,
        h: &dyn Host,
        name: &[u8],
        text: &[u32],
        default: bool,
        d: Sd,
        mut pos: [f32; 3],
        mut size: [f32; 2],
    ) -> bool {
        self.layout(&mut pos, &mut size, d.v2(off::W_DEFAULT_SIZE), true);
        let id = id_of(name);
        let st = self.state_ptr(id);
        // SAFETY: boxed state of this UI, not freed below.
        default_or_current_int(unsafe { &mut *st }, i32::from(default));
        let over = self.widget_base(h, id, pos, size, st);
        // SAFETY: as above.
        let toggled = widgets::checkbox(unsafe { &mut *st }, &mut act!(self), over);
        self.widget_base_draw(h, d, pos, size, over, false, -1, -1);
        let bx = d.v2(off::CHECK_BOX_SIZE);
        self.draw_gfx(
            h,
            d.at(off::CHECK_GFX_BOX),
            [pos[0], pos[1], pos[2] + 0.1],
            bx,
            WHITE,
        );
        // SAFETY: as above.
        let on = unsafe { (*st).int };
        if on != 0 {
            self.draw_gfx(
                h,
                d.at(off::CHECK_GFX_OVERLAY),
                [pos[0], pos[1], pos[2] + 0.2],
                d.v2(off::CHECK_OVERLAY_SIZE),
                WHITE,
            );
        }
        self.draw_text(
            h,
            text,
            d.at(off::W_FONT),
            d.col(off::W_COLOR_TEXT),
            ALIGN_LEFT,
            [pos[0] + (bx[0] + 4.0), pos[1], pos[2]],
            [size[0] - (bx[0] + 4.0), size[1]],
            1.0,
        );
        self.advance(pos, size, toggled);
        on != 0
    }

    pub fn do_multi_select(
        &mut self,
        h: &dyn Host,
        name: &[u8],
        default: i32,
        d: Sd,
        mut pos: [f32; 3],
        mut size: [f32; 2],
    ) -> i32 {
        self.layout(&mut pos, &mut size, d.v2(off::W_DEFAULT_SIZE), true);
        let id = id_of(name);
        let st = self.state_ptr(id);
        // SAFETY: boxed state of this UI, not freed below.
        default_or_current_int(unsafe { &mut *st }, default);
        let num = self.items.len() as i32;
        // SAFETY: as above.
        let old = unsafe { (*st).int };
        let arrow = d.v2(off::MULTI_ARROW_SIZE);
        let over = self.widget_base(h, id, pos, size, st);
        let i = MultiIn {
            over,
            mouse_over: self.prev.mouse_over,
            num,
            mouse_x: self.mouse_pos[0],
            pos_x: pos[0],
            size_x: size[0],
        };
        // SAFETY: as above.
        widgets::multi_select(unsafe { &mut *st }, &mut act!(self), &i);
        self.widget_base_draw(h, d, pos, size, over, false, -1, -1);
        let ay = (size[1] - arrow[1]) * 0.5;
        self.draw_gfx(
            h,
            d.at(off::MULTI_GFX_ARROW_LEFT),
            [pos[0], pos[1] + ay, pos[2] + 0.1],
            arrow,
            WHITE,
        );
        self.draw_gfx(
            h,
            d.at(off::MULTI_GFX_ARROW_RIGHT),
            [pos[0] + (size[0] - arrow[0]), pos[1] + ay, pos[2] + 0.1],
            arrow,
            WHITE,
        );
        // SAFETY: as above.
        let sel = unsafe { (*st).int };
        if let Some(t) = usize::try_from(sel)
            .ok()
            .and_then(|i| self.items.get(i))
            .cloned()
        {
            self.draw_text(
                h,
                &t,
                d.at(off::W_FONT),
                d.col(off::W_COLOR_TEXT),
                ALIGN_CENTER,
                pos,
                size,
                1.0,
            );
        }
        self.items.clear();
        self.advance(pos, size, sel != old);
        sel
    }

    #[allow(clippy::too_many_arguments)]
    pub fn do_multi_toggle(
        &mut self,
        h: &dyn Host,
        name: &[u8],
        default: i32,
        cols: u32,
        spacing: [f32; 2],
        d: Sd,
        pos: [f32; 3],
        size: [f32; 2],
    ) -> i32 {
        let id = id_of(name);
        let st = self.state_ptr(id);
        // SAFETY: boxed state of this UI; the buttons below only touch other boxes.
        default_or_current_int(unsafe { &mut *st }, default);
        let items = std::mem::take(&mut self.items);
        let mut updated = false;
        if !items.is_empty() {
            let n = items.len();
            let cols = if cols != 0 { (cols as usize).min(n) } else { n };
            let rows = n.div_ceil(cols);
            let item = [
                (-((cols - 1) as f32)).mul_add(spacing[0], size[0]) / cols as f32,
                (-((rows - 1) as f32)).mul_add(spacing[1], size[1]) / rows as f32,
            ];
            let gp = self.group_pos();
            self.groups.push(Group {
                pos: add3(gp, pos),
                size,
                clip: false,
            });
            for (i, text) in items.iter().enumerate() {
                let p = [
                    (item[0] + spacing[0]) * i.checked_rem(cols).unwrap_or(0) as f32,
                    (item[1] + spacing[1]) * i.checked_div(cols).unwrap_or(0) as f32,
                    0.0,
                ];
                let mut sub = name.to_vec();
                sub.extend(format!("_{i}").bytes());
                // SAFETY: as above.
                let cur = unsafe { (*st).int };
                let mode = if cur == i as i32 { 3 } else { 4 };
                if self.do_button(h, &sub, text, d, p, item, mode) {
                    // SAFETY: as above.
                    unsafe {
                        updated = (*st).int != i as i32;
                        (*st).int = i as i32;
                    }
                }
            }
            self.groups.pop();
        }
        self.prev.updated = updated;
        // SAFETY: as above.
        unsafe { (*st).int }
    }

    pub fn do_gauge(
        &mut self,
        h: &dyn Host,
        d: Sd,
        fill: f32,
        mut pos: [f32; 3],
        mut size: [f32; 2],
    ) {
        self.layout(&mut pos, &mut size, d.v2(off::W_DEFAULT_SIZE), true);
        if d.bool(off::GAUGE_USE_FRAME) {
            self.draw_frame(
                h,
                d.at(off::GAUGE_FRAME),
                pos,
                size,
                d.col(off::W_COLOR_BASE),
            );
        }
        let pad = d.v2(off::GAUGE_PADDING);
        let vertical = d.i32(off::GAUGE_ORIENT) == 1;
        let (fp, fs) = gauge_fill(pos, size, pad, fill, vertical);
        self.draw_gfx(h, d.at(off::GAUGE_FILL), fp, fs, d.col(off::W_COLOR_BASE));
        self.advance(pos, size, false);
    }

    pub fn do_window_start(
        &mut self,
        h: &dyn Host,
        caption: &[u32],
        d: Sd,
        mut pos: [f32; 3],
        mut size: [f32; 2],
        clip: bool,
    ) {
        self.layout(&mut pos, &mut size, d.v2(off::W_DEFAULT_SIZE), true);
        let base = d.col(off::W_COLOR_BASE);
        self.draw_frame(h, d.at(off::WINDOW_FRAME), pos, size, base);
        let font = d.at(off::W_FONT);
        let mul = d.f32(off::W_CAPTION_SIZE_MUL);
        let use_header = d.bool(off::W_USE_HEADER);
        let header_type = d.i32(off::W_HEADER_TYPE);
        let mut font_size = [0.0; 2];
        let mut caption_length = 0.0;
        if use_header {
            font_size = self.font_size(font, mul);
            if header_type == 1 {
                caption_length = self.font_length(h, font, mul, caption);
            }
        }
        let font_align = d.i32(off::W_FONT_ALIGN);
        let wi = WindowIn {
            pos,
            size,
            use_header,
            header_type,
            label_offset: d.v3(off::W_LABEL_OFFSET),
            label_pad: [
                d.f32(off::WINDOW_LABEL_PAD_TOP),
                d.f32(off::WINDOW_LABEL_PAD_TOP + 4),
                d.f32(off::WINDOW_LABEL_PAD_TOP + 8),
                d.f32(off::WINDOW_LABEL_PAD_TOP + 12),
            ],
            label_min_width: d.f32(off::WINDOW_LABEL_MIN_WIDTH),
            font_size,
            caption_length,
            font_align,
            pad: [
                d.f32(off::WINDOW_PAD_TOP),
                d.f32(off::WINDOW_PAD_RIGHT),
                d.f32(off::WINDOW_PAD_BOTTOM),
                d.f32(off::WINDOW_PAD_LEFT),
            ],
        };
        let wo = window(&wi);
        if wo.has_header {
            self.draw_frame(
                h,
                d.at(off::W_LABEL_FRAME),
                wo.header_pos,
                wo.header_size,
                base,
            );
            self.draw_font(
                h,
                caption,
                font,
                wo.text_pos,
                font_align,
                [mul, mul],
                d.col(off::W_COLOR_TEXT),
            );
        }
        self.push_group(Group {
            pos: wo.group_pos,
            size: wo.group_size,
            clip,
        });
        self.prev.pos = pos;
        self.prev.size = size;
    }

    pub fn do_mouse(&mut self, h: &dyn Host, g: Sd, offset: [f32; 3], size: [f32; 2]) {
        let p = add3([self.mouse_pos[0], self.mouse_pos[1], 100.0], offset);
        self.draw_gfx(h, g, p, size, WHITE);
    }

    pub fn draw_line(
        &mut self,
        h: &dyn Host,
        a: [f32; 2],
        b: [f32; 2],
        z: f32,
        t: f32,
        c: [f32; 4],
        g: Sd,
    ) {
        let d = [b[0] - a[0], b[1] - a[1]];
        let len = d[0].mul_add(d[0], d[1] * d[1]).sqrt();
        let angle = self.mods.rotate_angle;
        self.mods.rotate_angle += d[1].atan2(d[0]);
        self.draw_gfx(
            h,
            g,
            [(a[0] + b[0] - len) * 0.5, (a[1] + b[1] - t) * 0.5, z],
            [len, t],
            c,
        );
        self.mods.rotate_angle = angle;
    }

    pub fn do_label_plain(
        &mut self,
        h: &dyn Host,
        text: &[u32],
        d: Sd,
        pos: [f32; 3],
        size: [f32; 2],
    ) {
        self.do_label(h, text, d, pos, size, 1.0);
    }
}

fn vec3_string(p: [f32; 3]) -> String {
    format!("{:.6} : {:.6} : {:.6}", p[0], p[1], p[2])
}

#[derive(Clone, Copy)]
pub enum Val {
    Int(i32),
    Float(f32),
    Vec([f32; 3]),
    Col([f32; 4]),
}

impl Val {
    fn put(self, s: &mut State) {
        match self {
            Self::Int(v) => {
                s.int = v;
                s.set_int = true;
            }
            Self::Float(v) => {
                s.float = v;
                s.set_float = true;
            }
            Self::Vec(v) => {
                s.vec = v;
                s.set_vec = true;
            }
            Self::Col(v) => {
                s.col = v;
                s.set_col = true;
            }
        }
    }

    fn is_set(self, s: &State) -> bool {
        match self {
            Self::Int(_) => s.set_int,
            Self::Float(_) => s.set_float,
            Self::Vec(_) => s.set_vec,
            Self::Col(_) => s.set_col,
        }
    }

    fn read(self, s: &State) -> Val {
        match self {
            Self::Int(_) => Self::Int(s.int),
            Self::Float(_) => Self::Float(s.float),
            Self::Vec(_) => Self::Vec(s.vec),
            Self::Col(_) => Self::Col(s.col),
        }
    }
}
