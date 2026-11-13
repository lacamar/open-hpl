//! H21: the five GUI popups (`iGuiPopUp`, message box, file picker, colour picker, UI keyboard)
//! as Rust state machines. Widgets, `cGuiSet`, the file browser and the filesystem stay C++ and
//! are reached through one command channel ([`Host::exec`]); decisions come from [`super::popup`].
//! Callbacks arrive as `(tag, widget, message)` events. No `&mut` is held across a host call:
//! a command can re-enter [`Popup::event`] (combo selection fires callbacks), so mutable state
//! lives in `Cell`s.

use super::popup as pl;
use super::set::{Msg, W};
use super::widget::v2;
use crate::math::v3;
use crate::string;
use crate::types::{Color, Vector2f, Vector3f};
use std::cell::Cell;

macro_rules! ops {
    ($($n:ident),* $(,)?) => {
        #[derive(Clone, Copy, Debug, PartialEq, Eq)]
        #[repr(i32)]
        pub enum Op { $($n),* }
        pub const OP_NAMES: &[&str] = &[$(stringify!($n)),*];
    };
}

ops!(
    VirtualSize,
    PopupZ,
    IncPopupZ,
    PopupCount,
    GetAttention,
    GetFocused,
    GetDefaultFocus,
    SetAttention,
    SetFocused,
    SetDefaultFocus,
    PushAttention,
    PushFocused,
    PushDefaultFocus,
    PopAttention,
    PopFocused,
    PopDefaultFocus,
    DestroyWidget,
    CreateWindow,
    CreateButton,
    CreateLabel,
    CreateSlider,
    CreateTextbox,
    CreateFrame,
    CreateCombo,
    CreateList,
    MessageBox,
    DestroyPopup,
    AddResourceDir,
    RunCallback,
    CallDestroyCb,
    AddCallback,
    SetText,
    GetText,
    SetPosition,
    SetSize,
    SetGlobalPosition,
    GetGlobalPosition,
    GetSize,
    SetEnabled,
    SetVisible,
    SetFocusNav,
    SetGlobalUiListener,
    WinCloseDisables,
    WinEscCloses,
    WinGetEsc,
    WinStatic,
    WinDrawLabel,
    WinFontSize,
    InsideBounds,
    BtnUpImage,
    SliderGet,
    SliderSet,
    TbGetNum,
    TbSetNum,
    TbLower,
    TbUpper,
    TbDecimals,
    TbForceEnter,
    TbLostFocus,
    TbSelectAll,
    TbKbOpen,
    TbKbClose,
    TbUserData,
    TbKeySkip,
    TbKey,
    FrameColor,
    FrameDrawBg,
    FrameBgZ,
    CbCanEdit,
    CbItemNum,
    CbSelected,
    CbItemText,
    CbAddItem,
    CbClear,
    CbSetSelected,
    MlAddColumn,
    MlColWidth,
    MlSelected,
    MlProp1,
    MlClear,
    MlAddRow,
    FbNavUp,
    FbNavTo,
    FbFullPath,
    FbPathCount,
    FbPathPop,
    FbPathClear,
    FbPathEntry,
    FbDirs,
    FbCatCount,
    FbCatName,
    FbCatFilterCount,
    FbCatFilter,
    FbInit,
    FbRootCount,
    FbRoot,
    FbList,
    FbRow,
    FpLoadGfx,
    FileExists,
    FolderExists,
    SaveDest,
    LoadDest,
    TextLen,
    FontSize,
    ColorRead,
    ColorWrite,
);

pub const MSG_BUTTON_PRESSED: i32 = 11;
pub const MSG_UI_BUTTON_PRESS: i32 = 19;
pub const MSG_UI_BUTTON_RELEASE: i32 = 20;
pub const MSG_SLIDER_MOVE: i32 = 24;
pub const MSG_SELECTION_CHANGE: i32 = 26;
pub const MSG_SELECTION_DOUBLE_CLICK: i32 = 27;
pub const MSG_TEXTBOX_ENTER: i32 = 28;
pub const MSG_WINDOW_CLOSE: i32 = 31;

pub const ARROW_UP: i32 = 1;
pub const ARROW_RIGHT: i32 = 2;
pub const ARROW_DOWN: i32 = 4;
pub const ARROW_LEFT: i32 = 8;

pub const KEY_BACKSPACE: i32 = 0;
pub const KEY_RETURN: i32 = 3;
pub const KEY_SPACE: i32 = 6;
pub const KEY_0: i32 = 21;
pub const KEY_A: i32 = 44;
pub const KEY_NONE: i32 = 133;

pub const TAG_WINDOW_CLOSE: i32 = 1;
pub const TAG_BUTTON: i32 = 10;
pub const TAG_PAD: i32 = 11;
pub const TAG_SLIDER: i32 = 12;
pub const TAG_INPUT: i32 = 13;
pub const TAG_MSGBOX_RETURN: i32 = 14;
pub const TAG_DIR: i32 = 15;
pub const TAG_DBL: i32 = 16;
pub const TAG_SELECT: i32 = 17;
pub const TAG_FILTER: i32 = 18;
pub const TAG_KEY: i32 = 19;
pub const TAG_ENTER: i32 = 20;
pub const TAG_CANCEL: i32 = 21;
pub const TAG_UI_PRESS: i32 = 22;
pub const TAG_UI_RELEASE: i32 = 23;

pub const NO_TAG: i32 = -1;

/// One host command; unused fields are zero. Strings are UTF-32 and only valid for the call.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Cmd {
    pub op: i32,
    pub i: [i32; 3],
    pub w: [W; 2],
    pub f: [f32; 6],
    pub p: [usize; 2],
    pub s: [*const u32; 4],
    pub n: [usize; 4],
}

/// Host answer; string pointers are valid until the next command and are copied at once.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Ret {
    pub w: W,
    pub i: i32,
    pub f: [f32; 4],
    pub s: [*const u32; 3],
    pub n: [usize; 3],
}

impl Default for Ret {
    fn default() -> Self {
        Ret {
            w: 0,
            i: 0,
            f: [0.0; 4],
            s: [std::ptr::null(); 3],
            n: [0; 3],
        }
    }
}

impl Cmd {
    pub fn new(op: Op) -> Cmd {
        Cmd {
            op: op as i32,
            i: [0; 3],
            w: [0; 2],
            f: [0.0; 6],
            p: [0; 2],
            s: [std::ptr::null(); 4],
            n: [0; 4],
        }
    }

    fn w(mut self, a: W) -> Cmd {
        self.w[0] = a;
        self
    }

    fn w2(mut self, a: W) -> Cmd {
        self.w[1] = a;
        self
    }

    fn i(mut self, k: usize, v: i32) -> Cmd {
        if let Some(d) = self.i.get_mut(k) {
            *d = v;
        }
        self
    }

    fn f(mut self, k: usize, v: f32) -> Cmd {
        if let Some(d) = self.f.get_mut(k) {
            *d = v;
        }
        self
    }

    fn v3(mut self, v: Vector3f) -> Cmd {
        self.f[0] = v.x;
        self.f[1] = v.y;
        self.f[2] = v.z;
        self
    }

    fn v2at(mut self, k: usize, v: Vector2f) -> Cmd {
        if let Some([a, b]) = self.f.get_mut(k..k + 2) {
            *a = v.x;
            *b = v.y;
        }
        self
    }

    fn p(mut self, k: usize, v: usize) -> Cmd {
        if let Some(d) = self.p.get_mut(k) {
            *d = v;
        }
        self
    }

    fn s(mut self, k: usize, v: &[u32]) -> Cmd {
        if let (Some(p), Some(n)) = (self.s.get_mut(k), self.n.get_mut(k)) {
            *p = v.as_ptr();
            *n = v.len();
        }
        self
    }
}

pub trait Host {
    fn exec(&self, c: &Cmd) -> Ret;
}

pub struct Hx<'a>(pub &'a dyn Host);

fn copy_str(r: &Ret, k: usize) -> Vec<u32> {
    match (r.s.get(k), r.n.get(k)) {
        (Some(&p), Some(&n)) if !p.is_null() => {
            // SAFETY: the host returns `n` valid code units at `p` until its next command.
            unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
        }
        _ => Vec::new(),
    }
}

fn create(o: Op, pos: Vector3f, size: Vector2f, parent: W) -> Cmd {
    Cmd::new(o).v3(pos).v2at(3, size).w(parent)
}

fn at<T: Copy + Default>(a: &[T], i: usize) -> T {
    a.get(i).copied().unwrap_or_default()
}

fn ws(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn cat(a: &[u32], b: &[u32]) -> Vec<u32> {
    let mut v = a.to_vec();
    v.extend_from_slice(b);
    v
}

impl Hx<'_> {
    fn run(&self, c: Cmd) -> Ret {
        self.0.exec(&c)
    }

    fn op(&self, o: Op) -> Ret {
        self.run(Cmd::new(o))
    }

    fn text(&self, w: W) -> Vec<u32> {
        copy_str(&self.run(Cmd::new(Op::GetText).w(w)), 0)
    }

    fn set_text(&self, w: W, t: &[u32]) {
        self.run(Cmd::new(Op::SetText).w(w).s(0, t));
    }

    fn pos3(&self, w: W, o: Op, v: Vector3f) {
        self.run(Cmd::new(o).w(w).v3(v));
    }

    fn size2(&self, w: W, v: Vector2f) {
        self.run(Cmd::new(Op::SetSize).w(w).v2at(0, v));
    }

    fn callback(&self, w: W, msg: i32, tag: i32) {
        self.run(Cmd::new(Op::AddCallback).w(w).i(0, msg).i(1, tag));
    }

    fn button(&self, pos: Vector3f, size: Vector2f, text: &[u32], parent: W) -> W {
        self.run(create(Op::CreateButton, pos, size, parent).s(0, text))
            .w
    }

    fn label(&self, pos: Vector3f, size: Vector2f, text: &[u32], parent: W) -> W {
        self.run(create(Op::CreateLabel, pos, size, parent).s(0, text))
            .w
    }

    fn destroy(&self, w: W, immediate: bool) {
        self.run(Cmd::new(Op::DestroyWidget).w(w).i(0, i32::from(immediate)));
    }

    fn nav(&self, w: W, dir: i32, to: W) {
        self.run(Cmd::new(Op::SetFocusNav).w(w).i(0, dir).w2(to));
    }

    fn set_default_focus(&self, w: W) {
        self.run(Cmd::new(Op::SetDefaultFocus).w(w));
        self.run(Cmd::new(Op::SetFocused).w(w));
    }

    fn message_box(&self, label: &str, text: &str, b1: &str, b2: &str, tag: i32) {
        let (l, t, a, b) = (ws(label), ws(text), ws(b1), ws(b2));
        self.run(
            Cmd::new(Op::MessageBox)
                .s(0, &l)
                .s(1, &t)
                .s(2, &a)
                .s(3, &b)
                .i(0, tag),
        );
    }
}

/// Base popup state (`iGuiPopUp`).
pub struct Base {
    st: Cell<pl::PopUpState>,
    win: W,
    dcb: Cell<[usize; 2]>,
}

struct MsgBox {
    btn: [W; 2],
    label: W,
    cb: [usize; 2],
}

struct ColorPicker {
    btn: [W; 3],
    slider: [W; 4],
    input: [W; 4],
    cur: W,
    prev: W,
    color: Cell<[f32; 4]>,
    old: [f32; 4],
    dest: usize,
    cb: [usize; 2],
}

#[derive(Default)]
struct Fp {
    kind: i32,
    start_filename: Vec<u32>,
    btn: [Cell<W>; 3],
    dir: Cell<W>,
    name: Cell<W>,
    filter: Cell<W>,
    list: Cell<W>,
    cb: [usize; 2],
}

struct KeyEntry {
    w: W,
    key: Option<(i32, [i32; 2])>,
}

struct Keyboard {
    target: W,
    backup: Vec<u32>,
    shift: Cell<bool>,
    keys: Vec<KeyEntry>,
}

enum Kind {
    Msg(MsgBox),
    Color(ColorPicker),
    Fp(Fp),
    Kb(Keyboard),
}

pub struct Popup {
    base: Base,
    kind: Kind,
}

fn color_of(c: [f32; 4]) -> Color {
    Color {
        r: c[0],
        g: c[1],
        b: c[2],
        a: c[3],
    }
}

impl Base {
    fn new(h: &Hx, close: bool, size: Vector2f) -> Base {
        h.run(Cmd::new(Op::PopupCount).i(0, 1));
        let vs = h.op(Op::VirtualSize).f;
        let z = h.op(Op::PopupZ).f[0];
        let pos = pl::popup_pos(v2(vs[0], vs[1]), size, z);
        h.op(Op::IncPopupZ);
        let win = h
            .run(
                Cmd::new(Op::CreateWindow)
                    .i(0, i32::from(close))
                    .v3(pos)
                    .v2at(3, size),
            )
            .w;
        h.run(Cmd::new(Op::WinCloseDisables).w(win));
        h.callback(win, MSG_WINDOW_CLOSE, TAG_WINDOW_CLOSE);
        let att = h.op(Op::GetAttention).w;
        let foc = h.op(Op::GetFocused).w;
        let def = h.op(Op::GetDefaultFocus).w;
        h.op(Op::PushAttention);
        h.op(Op::PushFocused);
        h.op(Op::PushDefaultFocus);
        h.run(Cmd::new(Op::SetAttention).w(win));
        h.run(Cmd::new(Op::WinEscCloses).w(win).i(0, 1));
        Base {
            st: Cell::new(pl::PopUpState::new(att, foc, def)),
            win,
            dcb: Cell::new([0, 0]),
        }
    }

    fn self_destruct(&self, h: &Hx) {
        h.op(Op::DestroyPopup);
    }

    fn run_callback(
        &self,
        h: &Hx,
        cb: [usize; 2],
        w: W,
        mode: i32,
        val: i32,
        checks: bool,
    ) -> bool {
        if cb[0] == 0 || cb[1] == 0 {
            return false;
        }
        let snap = || {
            [
                h.op(Op::GetAttention).w,
                h.op(Op::GetFocused).w,
                h.op(Op::GetDefaultFocus).w,
            ]
        };
        let pre = if checks { snap() } else { [0; 3] };
        let ret =
            h.run(
                Cmd::new(Op::RunCallback)
                    .p(0, cb[0])
                    .p(1, cb[1])
                    .w(w)
                    .i(0, mode)
                    .i(1, val),
            )
            .i != 0;
        if checks {
            let post = snap();
            let mut st = self.st.get();
            st.after_callback(pre, post);
            self.st.set(st);
        }
        ret
    }

    fn teardown(&self, h: &Hx) {
        h.run(Cmd::new(Op::PopupCount).i(0, -1));
        h.op(Op::PopAttention);
        h.op(Op::PopFocused);
        h.op(Op::PopDefaultFocus);
        let st = self.st.get();
        if st.att_changed {
            h.run(Cmd::new(Op::SetAttention).w(st.new_attention));
        }
        if st.focus_changed {
            h.run(Cmd::new(Op::SetFocused).w(st.new_focus));
        }
        if st.default_focus_changed {
            h.run(Cmd::new(Op::SetDefaultFocus).w(st.new_default_focus));
        }
        let [obj, f] = self.dcb.get();
        if obj != 0 && f != 0 {
            h.run(Cmd::new(Op::CallDestroyCb).p(0, obj).p(1, f));
        }
        if self.win != 0 {
            h.destroy(self.win, true);
        }
    }
}

impl Popup {
    pub fn window(&self) -> W {
        self.base.win
    }

    pub fn set_destroy_callback(&self, obj: usize, f: usize) {
        self.base.dcb.set([obj, f]);
    }

    pub fn set_kill_on_escape(&self, h: &Hx, x: bool) {
        h.run(
            Cmd::new(Op::WinEscCloses)
                .w(self.base.win)
                .i(0, i32::from(x)),
        );
    }

    pub fn kill_on_escape(&self, h: &Hx) -> bool {
        h.run(Cmd::new(Op::WinGetEsc).w(self.base.win)).i != 0
    }

    pub fn set_next_focus(&self, w: W) {
        let mut st = self.base.st.get();
        st.set_next_focus(w);
        self.base.st.set(st);
    }

    pub fn event(&self, h: &Hx, tag: i32, w: W, d: &Msg) -> bool {
        if tag == TAG_WINDOW_CLOSE {
            self.base.self_destruct(h);
            return true;
        }
        match &self.kind {
            Kind::Msg(m) => m.event(&self.base, h, tag, w, d),
            Kind::Color(c) => c.event(&self.base, h, tag, w),
            Kind::Fp(f) => f.event(&self.base, h, tag, w, d),
            Kind::Kb(k) => k.event(&self.base, h, tag, w, d),
        }
    }

    pub fn destroy(&self, h: &Hx) {
        match &self.kind {
            Kind::Msg(m) => {
                for b in m.btn {
                    if b != 0 {
                        h.destroy(b, false);
                    }
                }
                if m.label != 0 {
                    h.destroy(m.label, false);
                }
            }
            Kind::Color(c) => {
                for i in 0..4 {
                    let ids = [c.btn.get(i), c.slider.get(i), c.input.get(i)];
                    for w in ids.into_iter().flatten().filter(|w| **w != 0) {
                        h.destroy(*w, false);
                    }
                }
                for w in [c.cur, c.prev] {
                    if w != 0 {
                        h.destroy(w, false);
                    }
                }
            }
            Kind::Fp(f) => {
                let ids = f
                    .btn
                    .iter()
                    .chain([&f.dir, &f.name, &f.list, &f.filter])
                    .map(Cell::get);
                for w in ids {
                    if w != 0 {
                        h.destroy(w, false);
                    }
                }
            }
            Kind::Kb(k) => {
                h.run(Cmd::new(Op::TbKbClose).w(k.target));
            }
        }
        self.base.teardown(h);
    }
}

// ---------------------------------------------------------------- message box

pub fn msgbox_new(
    h: &Hx,
    label: &[u32],
    text: &[u32],
    b1: &[u32],
    b2: &[u32],
    cb: [usize; 2],
) -> Popup {
    let base = Base::new(h, false, v2(0.0, 0.0));
    let font = h.op(Op::FontSize).f;
    let label_len = h.run(Cmd::new(Op::TextLen).s(0, label)).f[0];
    let text_len = h.run(Cmd::new(Op::TextLen).s(0, text)).f[0];
    let vs = h.op(Op::VirtualSize).f;
    let two = !b2.is_empty();
    let l = pl::msgbox_layout(label_len, text_len, font[1], v2(vs[0], vs[1]), two);
    h.set_text(base.win, label);
    h.pos3(base.win, Op::SetPosition, l.window_pos);
    h.size2(base.win, l.window_size);
    let mut btn = [0; 2];
    let first = h.button(l.button0_pos, v2(80.0, 30.0), b1, base.win);
    btn[0] = first;
    let wire = |b: W| {
        h.callback(b, MSG_BUTTON_PRESSED, TAG_BUTTON);
        h.callback(b, MSG_UI_BUTTON_PRESS, TAG_PAD);
        h.run(Cmd::new(Op::SetGlobalUiListener).w(b).i(0, 1));
    };
    wire(first);
    if two {
        let second = h.button(l.button1_pos, v2(80.0, 30.0), b2, base.win);
        btn[1] = second;
        wire(second);
        h.nav(first, ARROW_RIGHT, second);
        h.nav(second, ARROW_LEFT, first);
    }
    h.set_default_focus(first);
    let label_w = h.label(l.label_pos, l.label_size, text, base.win);
    h.set_default_focus(first);
    Popup {
        base,
        kind: Kind::Msg(MsgBox {
            btn,
            label: label_w,
            cb,
        }),
    }
}

impl MsgBox {
    fn event(&self, b: &Base, h: &Hx, tag: i32, w: W, d: &Msg) -> bool {
        match tag {
            TAG_BUTTON => self.press(b, h, w),
            TAG_PAD => match pl::msgbox_gamepad(d.val(), self.btn[1] != 0) {
                None => false,
                Some(second) => self.press(b, h, if second { self.btn[1] } else { w }),
            },
            _ => false,
        }
    }

    fn press(&self, b: &Base, h: &Hx, w: W) -> bool {
        let which = i32::from(w != self.btn[0]);
        b.run_callback(h, self.cb, w, 1, which, true);
        b.self_destruct(h);
        true
    }
}

// ---------------------------------------------------------------- colour picker

pub fn color_new(h: &Hx, dest: usize, cb: [usize; 2]) -> Popup {
    let base = Base::new(h, true, v2(350.0, 250.0));
    let color = if dest == 0 {
        [1.0; 4]
    } else {
        h.run(Cmd::new(Op::ColorRead).p(0, dest)).f
    };
    h.set_text(base.win, &ws("Color Picker"));
    h.run(Cmd::new(Op::WinStatic).w(base.win));
    let mut btn = [0; 3];
    for b in &mut btn {
        *b = h.button(v3(0.0, 0.0, 0.0), v2(30.0, 30.0), &[], base.win);
        h.callback(*b, MSG_BUTTON_PRESSED, TAG_BUTTON);
    }
    let l = pl::color_layout();
    let [b0, b1, b2] = btn;
    h.run(Cmd::new(Op::SetEnabled).w(b0));
    h.run(Cmd::new(Op::SetVisible).w(b0));
    h.pos3(b0, Op::SetPosition, l.button_pos[0]);
    h.size2(b0, l.button_size[0]);
    for (k, (w, text)) in [(b1, "Ok"), (b2, "Cancel")].into_iter().enumerate() {
        h.set_text(w, &ws(text));
        h.pos3(w, Op::SetPosition, at(&l.button_pos, k + 1));
        h.size2(w, at(&l.button_size, k + 1));
    }
    let mut slider = [0; 4];
    let mut input = [0; 4];
    for (i, name) in ["R", "G", "B", "A"].into_iter().enumerate() {
        let c = color.get(i).copied().unwrap_or(0.0);
        h.label(at(&l.label_pos, i), v2(0.0, 0.0), &ws(name), base.win);
        let s = h
            .run(
                create(
                    Op::CreateSlider,
                    at(&l.slider_pos, i),
                    v2(100.0, 25.0),
                    base.win,
                )
                .i(1, 1000),
            )
            .w;
        h.run(
            Cmd::new(Op::SliderSet)
                .w(s)
                .i(0, pl::color_slider_value(c))
                .i(1, 1),
        );
        h.callback(s, MSG_SLIDER_MOVE, TAG_SLIDER);
        let t = h
            .run(
                create(
                    Op::CreateTextbox,
                    at(&l.input_pos, i),
                    v2(80.0, 25.0),
                    base.win,
                )
                .s(0, &[])
                .i(0, 1)
                .f(5, 0.1),
            )
            .w;
        h.run(Cmd::new(Op::TbLower).w(t).i(0, 1));
        h.run(Cmd::new(Op::TbUpper).w(t).i(0, 1).f(0, 1.0));
        h.run(Cmd::new(Op::TbDecimals).w(t).i(0, 3));
        h.run(Cmd::new(Op::TbSetNum).w(t).f(0, c));
        h.callback(t, MSG_TEXTBOX_ENTER, TAG_INPUT);
        if let (Some(ds), Some(di)) = (slider.get_mut(i), input.get_mut(i)) {
            *ds = s;
            *di = t;
        }
    }
    let frame = |pos: Vector3f| {
        let f = h
            .run(create(Op::CreateFrame, pos, v2(50.0, 50.0), base.win).i(0, 1))
            .w;
        h.run(frame_color(f, color));
        h.run(Cmd::new(Op::FrameDrawBg).w(f).i(0, 1));
        h.run(Cmd::new(Op::FrameBgZ).w(f));
        f
    };
    let cur = frame(l.current_frame_pos);
    let prev = frame(l.previous_frame_pos);
    Popup {
        base,
        kind: Kind::Color(ColorPicker {
            btn,
            slider,
            input,
            cur,
            prev,
            color: Cell::new(color),
            old: color,
            dest,
            cb,
        }),
    }
}

fn frame_color(f: W, c: [f32; 4]) -> Cmd {
    Cmd::new(Op::FrameColor)
        .w(f)
        .f(0, c[0])
        .f(1, c[1])
        .f(2, c[2])
        .f(3, 1.0)
}

impl ColorPicker {
    fn event(&self, b: &Base, h: &Hx, tag: i32, w: W) -> bool {
        match tag {
            TAG_BUTTON => {
                if w == self.btn[1] {
                    self.close(b, h);
                }
                if w == self.btn[2] {
                    b.self_destruct(h);
                }
            }
            TAG_SLIDER => {
                let Some(i) = Self::component(&self.slider, w) else {
                    return true;
                };
                let v = h.run(Cmd::new(Op::SliderGet).w(at(&self.slider, i))).i;
                self.set(i, pl::color_from_slider(v));
                self.update_inputs(h);
                h.run(frame_color(self.cur, self.color.get()));
            }
            TAG_INPUT => {
                let Some(i) = Self::component(&self.input, w) else {
                    return true;
                };
                let v = h.run(Cmd::new(Op::TbGetNum).w(at(&self.input, i))).f[0];
                self.set(i, v);
                self.update_sliders(h);
                h.run(frame_color(self.cur, self.color.get()));
            }
            _ => return false,
        }
        true
    }

    fn component(ids: &[W; 4], w: W) -> Option<usize> {
        pl::color_component(&ids.map(|x| x == w))
    }

    fn set(&self, i: usize, v: f32) {
        let mut c = self.color.get();
        if let Some(d) = c.get_mut(i) {
            *d = v;
        }
        self.color.set(c);
    }

    fn update_sliders(&self, h: &Hx) {
        for (i, &s) in self.slider.iter().enumerate() {
            let c = self.color.get().get(i).copied().unwrap_or(0.0);
            h.run(Cmd::new(Op::SliderSet).w(s).i(0, pl::color_slider_value(c)));
        }
    }

    fn update_inputs(&self, h: &Hx) {
        for (i, &t) in self.input.iter().enumerate() {
            let c = self.color.get().get(i).copied().unwrap_or(0.0);
            h.run(Cmd::new(Op::TbSetNum).w(t).f(0, c));
        }
    }

    fn close(&self, b: &Base, h: &Hx) {
        let c = self.color.get();
        if pl::color_changed(color_of(c), color_of(self.old)) {
            if self.dest != 0 {
                h.run(
                    Cmd::new(Op::ColorWrite)
                        .p(0, self.dest)
                        .f(0, c[0])
                        .f(1, c[1])
                        .f(2, c[2])
                        .f(3, c[3]),
                );
            }
            b.run_callback(h, self.cb, 0, 0, 0, true);
        }
        b.self_destruct(h);
    }
}

// ---------------------------------------------------------------- file picker

pub fn fp_new(
    h: &Hx,
    kind: i32,
    start_path: &[u32],
    start_filename: &[u32],
    cb: [usize; 2],
) -> Popup {
    Popup {
        base: Base::new(h, false, v2(600.0, 500.0)),
        kind: Kind::Fp(Fp {
            kind,
            start_filename: pl::fp_start_filename(start_filename, start_path),
            cb,
            ..Fp::default()
        }),
    }
}

fn full_path(h: &Hx) -> Vec<u32> {
    copy_str(&h.op(Op::FbFullPath), 0)
}

fn count(h: &Hx, o: Op) -> usize {
    usize::try_from(h.op(o).i).unwrap_or(0)
}

fn idx(i: usize) -> i32 {
    i32::try_from(i).unwrap_or(0)
}

impl Popup {
    pub fn fp_init(&self, h: &Hx) {
        if let Kind::Fp(f) = &self.kind {
            f.init(&self.base, h);
        }
    }

    pub fn fp_set_save_dest(&self, h: &Hx, name: &[u32]) {
        if let Kind::Fp(f) = &self.kind {
            h.set_text(f.name.get(), string::file_name(name));
        }
    }

    pub fn fp_on_add_filter(&self, h: &Hx) {
        if let Kind::Fp(f) = &self.kind {
            f.on_add_filter(h);
        }
    }

    pub fn fp_on_navigate(&self, h: &Hx) {
        if let Kind::Fp(f) = &self.kind {
            f.on_navigate(h);
        }
    }
}

impl Fp {
    fn init(&self, b: &Base, h: &Hx) {
        let caption = pl::fp_caption(self.kind);
        let font = h.run(Cmd::new(Op::WinFontSize).w(b.win)).f;
        let l = pl::fp_layout(font[1]);
        h.op(Op::FpLoadGfx);
        h.set_text(b.win, &caption);
        let dir = h
            .run(create(Op::CreateCombo, l.dir_pos, l.dir_size, b.win).s(0, &[]))
            .w;
        self.dir.set(dir);
        h.run(Cmd::new(Op::CbCanEdit).w(dir));
        h.callback(dir, MSG_SELECTION_CHANGE, TAG_DIR);
        for slot in &self.btn {
            let w = h.button(v3(0.0, 0.0, 0.0), v2(0.0, 0.0), &[], b.win);
            slot.set(w);
            h.callback(w, MSG_BUTTON_PRESSED, TAG_BUTTON);
        }
        let [b0, b1, b2] = [self.btn[0].get(), self.btn[1].get(), self.btn[2].get()];
        h.run(Cmd::new(Op::BtnUpImage).w(b0));
        h.pos3(b0, Op::SetPosition, l.button_pos[0]);
        h.size2(b0, l.button_size[0]);
        let cancel = ws("Cancel");
        for (k, (w, text)) in [(b1, &caption), (b2, &cancel)].into_iter().enumerate() {
            h.set_text(w, text);
            h.pos3(w, Op::SetPosition, at(&l.button_pos, k + 1));
            h.size2(w, at(&l.button_size, k + 1));
        }
        let name = h
            .run(
                create(Op::CreateTextbox, l.name_pos, l.name_size, b.win)
                    .s(0, &self.start_filename)
                    .f(5, 1.0),
            )
            .w;
        self.name.set(name);
        h.run(Cmd::new(Op::TbForceEnter).w(name).i(0, 1));
        h.run(Cmd::new(Op::TbLostFocus).w(name));
        h.callback(name, MSG_TEXTBOX_ENTER, TAG_BUTTON);
        h.run(Cmd::new(Op::SetFocused).w(name));
        h.run(Cmd::new(Op::TbSelectAll).w(name));
        let filter = h
            .run(create(Op::CreateCombo, l.filter_pos, l.filter_size, b.win).s(0, &[]))
            .w;
        self.filter.set(filter);
        h.callback(filter, MSG_SELECTION_CHANGE, TAG_FILTER);
        let list = h
            .run(
                Cmd::new(Op::CreateList)
                    .v3(l.list_pos)
                    .v2at(3, l.list_size)
                    .w(b.win),
            )
            .w;
        self.list.set(list);
        for (i, (name, align)) in [("", 0), ("Name", 0), ("Size", 1), ("Date modified", 0)]
            .into_iter()
            .enumerate()
        {
            let n = ws(name);
            h.run(
                Cmd::new(Op::MlAddColumn)
                    .w(list)
                    .s(0, &n)
                    .i(0, idx(i))
                    .i(1, align),
            );
            let width = l.column_width.get(i).copied().unwrap_or(0);
            h.run(
                Cmd::new(Op::MlColWidth)
                    .w(list)
                    .i(0, idx(i))
                    .f(0, width as f32),
            );
        }
        h.callback(list, MSG_SELECTION_CHANGE, TAG_SELECT);
        h.callback(list, MSG_SELECTION_DOUBLE_CLICK, TAG_DBL);
        h.op(Op::FbInit);
    }

    fn event(&self, b: &Base, h: &Hx, tag: i32, w: W, d: &Msg) -> bool {
        match tag {
            TAG_BUTTON => self.button(b, h, w),
            TAG_MSGBOX_RETURN => {
                if pl::fp_overwrite_confirmed(self.kind, d.val()) {
                    self.build(h, Op::SaveDest);
                    self.close(b, h, true);
                }
                true
            }
            TAG_DIR => self.dir_changed(h, w),
            TAG_DBL => self.double_click(b, h),
            TAG_SELECT => self.selected(h),
            TAG_FILTER => {
                self.populate(h);
                true
            }
            _ => false,
        }
    }

    fn button(&self, b: &Base, h: &Hx, w: W) -> bool {
        let name = self.name.get();
        if w == self.btn[0].get() {
            h.op(Op::FbNavUp);
        } else if w == self.btn[1].get() || w == name {
            let empty = h.text(name).is_empty();
            if pl::fp_accept(self.kind, empty, false) == pl::FP_ERROR_EMPTY {
                h.message_box("Error", "File name must not be empty", "OK", "", NO_TAG);
            } else {
                self.accept(b, h);
            }
        } else if w == self.btn[2].get() {
            self.close(b, h, false);
        }
        true
    }

    fn accept(&self, b: &Base, h: &Hx) {
        let name = self.name.get();
        let mut full = cat(&full_path(h), &h.text(name));
        let mut exists = h.run(Cmd::new(Op::FileExists).s(0, &full)).i != 0;
        if self.kind == pl::PICKER_SAVE {
            let has_filter =
                count(h, Op::FbCatCount) > 0 && h.run(Cmd::new(Op::FbCatFilterCount)).i > 0;
            let filter0 = has_filter.then(|| copy_str(&h.run(Cmd::new(Op::FbCatFilter)), 0));
            if let Some(ext) = pl::fp_save_extension(&full, filter0.as_deref()) {
                h.set_text(name, &cat(&h.text(name), &ext));
                full.extend_from_slice(&ext);
                exists = exists || h.run(Cmd::new(Op::FileExists).s(0, &full)).i != 0;
            }
        }
        match pl::fp_accept(self.kind, false, exists) {
            pl::FP_SAVE_CONFIRM => h.message_box(
                "Warning",
                "File already exists. Overwrite?",
                "Yes",
                "No",
                TAG_MSGBOX_RETURN,
            ),
            pl::FP_SAVE_DONE => {
                self.build(h, Op::SaveDest);
                self.close(b, h, true);
            }
            pl::FP_LOAD_MISSING => h.message_box(
                "Error",
                "File does not exist. Please choose a valid file.",
                "OK",
                "",
                NO_TAG,
            ),
            pl::FP_LOAD_DONE => {
                self.build(h, Op::LoadDest);
                self.close(b, h, true);
            }
            _ => {}
        }
    }

    fn build(&self, h: &Hx, o: Op) {
        let p = cat(&full_path(h), &h.text(self.name.get()));
        h.run(Cmd::new(o).s(0, &p));
    }

    fn close(&self, b: &Base, h: &Hx, picked: bool) {
        if picked {
            let p = full_path(h);
            h.run(Cmd::new(Op::AddResourceDir).s(0, &p));
        }
        b.run_callback(h, self.cb, 0, 1, i32::from(picked), true);
        b.self_destruct(h);
    }

    fn dir_changed(&self, h: &Hx, w: W) -> bool {
        let items = h.run(Cmd::new(Op::CbItemNum).w(w)).i;
        let sel = h.run(Cmd::new(Op::CbSelected).w(w)).i;
        let item = || copy_str(&h.run(Cmd::new(Op::CbItemText).w(w).i(0, sel)), 0);
        let s = pl::fp_dir_select(sel, items, count(h, Op::FbPathCount));
        match s.action {
            1 => {
                h.op(Op::FbPathClear);
                h.run(Cmd::new(Op::FbNavTo).s(0, &item()));
            }
            2 => {
                let dir = item();
                h.run(Cmd::new(Op::FbPathPop).i(0, idx(s.pops)));
                h.run(Cmd::new(Op::FbNavTo).s(0, &dir));
            }
            _ => {}
        }
        true
    }

    fn selected_name(&self, h: &Hx, sel: i32) -> Option<Vec<u32>> {
        let r = h.run(Cmd::new(Op::MlProp1).w(self.list.get()).i(0, sel));
        (r.i == 2).then(|| copy_str(&r, 0))
    }

    fn double_click(&self, b: &Base, h: &Hx) -> bool {
        let sel = h.run(Cmd::new(Op::MlSelected).w(self.list.get())).i;
        let Some(path) = self.selected_name(h, sel) else {
            return true;
        };
        if h.run(Cmd::new(Op::FbNavTo).s(0, &path)).i == 0 {
            self.button(b, h, self.btn[1].get());
        }
        true
    }

    fn selected(&self, h: &Hx) -> bool {
        if h.run(Cmd::new(Op::CbSelected).w(self.dir.get())).i == 0 {
            return true;
        }
        let sel = h.run(Cmd::new(Op::MlSelected).w(self.list.get())).i;
        let path = if sel == -1 {
            Vec::new()
        } else {
            self.selected_name(h, sel).unwrap_or_default()
        };
        if !path.is_empty() {
            let full = cat(&full_path(h), &path);
            if h.run(Cmd::new(Op::FolderExists).s(0, &full)).i == 0 {
                h.set_text(self.name.get(), &path);
            }
        }
        true
    }

    fn on_add_filter(&self, h: &Hx) {
        let filter = self.filter.get();
        h.run(Cmd::new(Op::CbClear).w(filter));
        for i in 0..count(h, Op::FbCatCount) {
            let name = copy_str(&h.run(Cmd::new(Op::FbCatName).i(0, idx(i))), 0);
            let n =
                usize::try_from(h.run(Cmd::new(Op::FbCatFilterCount).i(0, idx(i))).i).unwrap_or(0);
            let filters: Vec<Vec<u32>> = (0..n)
                .map(|j| {
                    copy_str(
                        &h.run(Cmd::new(Op::FbCatFilter).i(0, idx(i)).i(1, idx(j))),
                        0,
                    )
                })
                .collect();
            let refs: Vec<&[u32]> = filters.iter().map(Vec::as_slice).collect();
            let text = pl::fp_filter_text(&name, &refs);
            h.run(Cmd::new(Op::CbAddItem).w(filter).s(0, &text));
        }
        h.run(Cmd::new(Op::CbSetSelected).w(filter));
    }

    fn on_navigate(&self, h: &Hx) {
        let dir = self.dir.get();
        h.run(Cmd::new(Op::CbClear).w(dir));
        let bits = h.op(Op::FbDirs).i;
        let entries = pl::fp_dir_entries(bits & 1 != 0, bits & 2 != 0, count(h, Op::FbPathCount));
        for e in entries {
            let text = match e {
                pl::DIR_ROOT => ws("<System Root>"),
                pl::DIR_PERSONAL => ws("<Personal Folder>"),
                pl::DIR_GAME => ws("<Game Folder>"),
                i => copy_str(&h.run(Cmd::new(Op::FbPathEntry).i(0, i)), 0),
            };
            h.run(Cmd::new(Op::CbAddItem).w(dir).s(0, &text));
        }
        h.set_text(dir, &full_path(h));
        self.populate(h);
    }

    fn populate(&self, h: &Hx) {
        let sel = h.run(Cmd::new(Op::CbSelected).w(self.filter.get())).i;
        if sel < 0 {
            return;
        }
        let list = self.list.get();
        h.run(Cmd::new(Op::MlClear).w(list));
        if full_path(h).is_empty() {
            for i in 0..count(h, Op::FbRootCount) {
                let name = copy_str(&h.run(Cmd::new(Op::FbRoot).i(0, idx(i))), 0);
                h.run(Cmd::new(Op::MlAddRow).w(list).s(0, &name));
            }
            return;
        }
        let n = usize::try_from(h.run(Cmd::new(Op::FbList).i(0, sel)).i).unwrap_or(0);
        for i in 0..n {
            let r = h.run(Cmd::new(Op::FbRow).i(0, idx(i)));
            let (name, size, date) = (copy_str(&r, 0), copy_str(&r, 1), copy_str(&r, 2));
            h.run(
                Cmd::new(Op::MlAddRow)
                    .w(list)
                    .i(0, 1)
                    .i(1, r.i)
                    .s(0, &name)
                    .s(1, &size)
                    .s(2, &date),
            );
        }
    }
}

// ---------------------------------------------------------------- UI keyboard

pub fn kb_new(h: &Hx, target: W) -> Popup {
    let base = Base::new(h, true, v2(0.0, 0.0));
    let backup = h.text(target);
    h.run(Cmd::new(Op::TbKbOpen).w(target));
    let user = h.run(Cmd::new(Op::TbUserData).w(target)).w;
    let mut st = base.st.get();
    st.set_next_focus(user);
    base.st.set(st);

    h.set_text(base.win, &[]);
    h.run(Cmd::new(Op::WinStatic).w(base.win).i(0, 1));
    h.run(Cmd::new(Op::WinDrawLabel).w(base.win));
    h.run(Cmd::new(Op::SetGlobalUiListener).w(base.win).i(0, 1));
    h.callback(base.win, MSG_UI_BUTTON_PRESS, TAG_UI_PRESS);
    h.callback(base.win, MSG_UI_BUTTON_RELEASE, TAG_UI_RELEASE);

    let mut keys = Vec::new();
    for k in pl::kb_keys() {
        let (key, text) = match k.kind {
            pl::KEY_DIGIT => (KEY_0 + k.offset, vec![k.lower as u32]),
            pl::KEY_LETTER => (KEY_A + k.offset, vec![k.lower as u32]),
            pl::KEY_ENTER => (KEY_RETURN, ws("Enter")),
            pl::KEY_CANCEL => (KEY_NONE, ws("Cancel")),
            _ => (KEY_SPACE, ws("Space")),
        };
        let w = h.button(v3(0.0, 0.0, 0.0), k.size, &text, base.win);
        let (tag, data) = match key {
            KEY_RETURN => (TAG_ENTER, None),
            KEY_NONE => (TAG_CANCEL, None),
            _ => (TAG_KEY, Some((key, [k.lower, k.upper]))),
        };
        h.callback(w, MSG_BUTTON_PRESSED, tag);
        keys.push(KeyEntry { w, key: data });
    }

    let sizes: Vec<Vector2f> = keys
        .iter()
        .map(|e| {
            let s = h.run(Cmd::new(Op::GetSize).w(e.w)).f;
            v2(s[0], s[1])
        })
        .collect();
    let mut pos = vec![Vector3f::default(); keys.len()];
    let mut row = vec![0i32; keys.len()];
    let lay = pl::kb_layout(&sizes, &mut pos, &mut row);
    for (e, p) in keys.iter().zip(&pos) {
        h.pos3(e.w, Op::SetPosition, *p);
    }
    h.size2(base.win, lay.window_size);
    let g = h.run(Cmd::new(Op::GetGlobalPosition).w(target)).f;
    let ts = h.run(Cmd::new(Op::GetSize).w(target)).f;
    let wp = pl::kb_window_pos(v3(g[0], g[1], g[2]), v2(ts[0], ts[1]), lay.window_size.x);
    h.pos3(base.win, Op::SetGlobalPosition, wp);
    h.run(Cmd::new(Op::InsideBounds).w(base.win));

    let mut row_len = vec![0usize; lay.rows];
    for r in &row {
        if let Some(n) = usize::try_from(*r).ok().and_then(|r| row_len.get_mut(r)) {
            *n += 1;
        }
    }
    let mut nav = vec![
        pl::KbNav {
            up: 0,
            right: 0,
            down: 0,
            left: 0
        };
        keys.len()
    ];
    pl::kb_nav(&row_len, &mut nav);
    let at = |i: i32| {
        usize::try_from(i)
            .ok()
            .and_then(|i| keys.get(i))
            .map_or(0, |e| e.w)
    };
    for (e, n) in keys.iter().zip(&nav) {
        h.nav(e.w, ARROW_UP, at(n.up));
        h.nav(e.w, ARROW_RIGHT, at(n.right));
        h.nav(e.w, ARROW_DOWN, at(n.down));
        h.nav(e.w, ARROW_LEFT, at(n.left));
    }
    if let Some(first) = keys.first() {
        h.set_default_focus(first.w);
    }
    Popup {
        base,
        kind: Kind::Kb(Keyboard {
            target,
            backup,
            shift: Cell::new(false),
            keys,
        }),
    }
}

impl Keyboard {
    fn event(&self, b: &Base, h: &Hx, tag: i32, w: W, d: &Msg) -> bool {
        match tag {
            TAG_KEY => {
                let Some((key, uni)) = self.keys.iter().find(|e| e.w == w).and_then(|e| e.key)
                else {
                    return false;
                };
                let c = uni.get(usize::from(self.shift.get())).copied().unwrap_or(0);
                let cmd = Cmd::new(Op::TbKeySkip).w(self.target).i(0, key).i(1, c);
                h.run(cmd).i != 0
            }
            TAG_ENTER => {
                b.self_destruct(h);
                true
            }
            TAG_CANCEL => self.cancel(b, h),
            TAG_UI_PRESS => match pl::kb_ui_press(d.val()) {
                pl::KB_CANCEL => self.cancel(b, h),
                pl::KB_DELETE => {
                    let cmd = Cmd::new(Op::TbKey).w(self.target).i(0, KEY_BACKSPACE);
                    h.run(cmd).i != 0
                }
                pl::KB_CLEAR => {
                    h.set_text(self.target, &[]);
                    true
                }
                pl::KB_SHIFT_ON => {
                    self.set_shift(h, true);
                    true
                }
                _ => false,
            },
            TAG_UI_RELEASE if pl::kb_ui_release(d.val()) => {
                self.set_shift(h, false);
                true
            }
            _ => false,
        }
    }

    fn cancel(&self, b: &Base, h: &Hx) -> bool {
        h.set_text(self.target, &self.backup);
        b.self_destruct(h);
        true
    }

    fn set_shift(&self, h: &Hx, x: bool) {
        if self.shift.get() == x {
            return;
        }
        self.shift.set(x);
        for e in &self.keys {
            if let Some((_, uni)) = e.key {
                let c = uni.get(usize::from(x)).copied().unwrap_or(0);
                h.set_text(e.w, &[c as u32]);
            }
        }
    }
}
