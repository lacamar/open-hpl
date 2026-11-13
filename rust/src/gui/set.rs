//! `cGuiSet` focus, attention, tab order, window order and input dispatch. Widgets stay C++ and
//! are reached through [`Host`]; the widget list is iterated through host cursors so list
//! mutation from callbacks behaves as it does for `std::list` iterators.

pub type W = usize;

pub const MSG_MOUSE_MOVE: i32 = 0;
pub const MSG_MOUSE_DOWN: i32 = 1;
pub const MSG_MOUSE_UP: i32 = 2;
pub const MSG_MOUSE_DOUBLE_CLICK: i32 = 3;
pub const MSG_MOUSE_ENTER: i32 = 4;
pub const MSG_MOUSE_LEAVE: i32 = 5;
pub const MSG_GOT_FOCUS: i32 = 6;
pub const MSG_LOST_FOCUS: i32 = 8;
pub const MSG_KEY_PRESS: i32 = 14;
pub const MSG_KEY_RELEASE: i32 = 15;
pub const MSG_GAMEPAD_INPUT: i32 = 16;
pub const MSG_UI_ARROW_PRESS: i32 = 17;
pub const MSG_UI_ARROW_RELEASE: i32 = 18;
pub const MSG_UI_BUTTON_PRESS: i32 = 19;
pub const MSG_UI_BUTTON_RELEASE: i32 = 20;
pub const MSG_UI_BUTTON_DOUBLE_PRESS: i32 = 21;
pub const MSG_GET_UI_NAV_FOCUS: i32 = 22;
pub const MSG_LOSE_UI_NAV_FOCUS: i32 = 23;

const KEY_TAB: i32 = 1;
const KEY_UP: i32 = 88;
const KEY_DOWN: i32 = 89;
const KEY_RIGHT: i32 = 90;
const KEY_LEFT: i32 = 91;
const ARROW_UP: i32 = 1;
const ARROW_RIGHT: i32 = 2;
const ARROW_DOWN: i32 = 4;
const ARROW_LEFT: i32 = 8;
const ARROW_NONE: i32 = 0;
const MOD_NONE: i32 = 0;
const MOD_SHIFT: i32 = 2;
const MOUSE_BITS: [i32; 5] = [1, 2, 4, 8, 16];

/// Bytes of `cGuiMessageData`; Rust reads and writes only position, `mlVal` and the key press.
pub const MSG_SIZE: usize = 72;

#[repr(C, align(8))]
#[derive(Clone, Copy)]
pub struct Msg(pub [u8; MSG_SIZE]);

/// `cMath::Log2ToInt` as an index, `None` where the C++ would index with a negative value.
fn mouse_index(v: i32) -> Option<usize> {
    let x = u64::try_from(v).ok().filter(|x| *x > 0)?;
    let l = 63 - x.leading_zeros();
    let round_up = x.checked_mul(x).is_some_and(|sq| sq >= 1u64 << (2 * l + 1));
    usize::try_from(l + u32::from(round_up)).ok()
}

impl Msg {
    pub const fn zero() -> Msg {
        Msg([0; MSG_SIZE])
    }

    fn put(&mut self, at: usize, b: [u8; 4]) {
        if let Some(d) = self.0.get_mut(at..at + 4) {
            d.copy_from_slice(&b);
        }
    }

    fn word(&self, at: usize) -> [u8; 4] {
        match self.0.get(at..at + 4) {
            Some(&[a, b, c, d]) => [a, b, c, d],
            _ => [0; 4],
        }
    }

    pub fn at(pos: [f32; 2]) -> Msg {
        let mut m = Msg::zero();
        m.set_pos(pos);
        m
    }

    pub fn with_val(val: i32) -> Msg {
        let mut m = Msg::zero();
        m.set_val(val);
        m
    }

    pub fn pos(&self) -> [f32; 2] {
        [
            f32::from_ne_bytes(self.word(0)),
            f32::from_ne_bytes(self.word(4)),
        ]
    }

    pub fn set_pos(&mut self, p: [f32; 2]) {
        self.put(0, p[0].to_ne_bytes());
        self.put(4, p[1].to_ne_bytes());
    }

    pub fn set_key(&mut self, key: i32, modifier: i32) {
        self.put(20, key.to_ne_bytes());
        self.put(28, modifier.to_ne_bytes());
    }

    pub fn val(&self) -> i32 {
        i32::from_ne_bytes(self.word(16))
    }

    pub fn set_val(&mut self, v: i32) {
        self.put(16, v.to_ne_bytes());
    }

    pub fn key(&self) -> i32 {
        i32::from_ne_bytes(self.word(20))
    }

    pub fn modifier(&self) -> i32 {
        i32::from_ne_bytes(self.word(28))
    }
}

pub trait Host {
    fn is_visible(&mut self, w: W) -> bool;
    fn is_enabled(&mut self, w: W) -> bool;
    fn is_connected_to(&mut self, w: W, other: W) -> bool;
    fn point_is_inside(&mut self, w: W, p: [f32; 2]) -> bool;
    fn mouse_is_over(&mut self, w: W) -> bool;
    fn set_mouse_is_over(&mut self, w: W, v: bool);
    fn process_message(&mut self, w: W, msg: i32, d: &Msg, skip_vis: bool, skip_en: bool) -> bool;
    fn get_focus(&mut self, w: W, d: &Msg) -> bool;
    fn tooltip_enabled(&mut self, w: W) -> bool;
    fn has_focus_nav(&mut self, w: W) -> bool;
    fn parent(&mut self, w: W) -> W;
    fn widget_set(&mut self, w: W) -> usize;
    fn pointer_gfx(&mut self, w: W) -> usize;
    fn focus_nav(&mut self, w: W, dir: i32) -> W;
    fn global_key_listener(&mut self, w: W) -> bool;
    fn global_ui_listener(&mut self, w: W) -> bool;
    fn got_tab_focus(&mut self, w: W, d: &Msg) -> bool;
    fn is_valid(&mut self, w: W) -> bool;
    fn list_begin(&mut self) -> usize;
    fn list_end(&mut self, it: usize) -> bool;
    fn list_get(&mut self, it: usize) -> W;
    fn list_next(&mut self, it: usize) -> usize;
    fn sort_widgets(&mut self);
    fn current_pointer(&mut self) -> usize;
    fn set_current_pointer(&mut self, g: usize);
    fn skin_normal_pointer(&mut self) -> usize;
    fn exec_shortcut(&mut self, d: &Msg) -> bool;
    fn global_pos(&mut self, w: W) -> [f32; 3];
    fn set_global_pos(&mut self, w: W, p: [f32; 3]);
}

/// Stacks and lists the C++ `cGuiSet` kept as `std::list`s.
#[derive(Default)]
pub struct Lists {
    pub focus_stack: Vec<W>,
    pub attention_stack: Vec<W>,
    pub default_stack: Vec<W>,
    pub tab_order: Vec<W>,
    pub windows: Vec<W>,
}

/// Plain fields shared with the C++ `cGuiSet`, which reads them directly.
#[repr(C)]
pub struct SetState {
    pub focused: W,
    pub attention: W,
    pub default_nav: W,
    pub tab_widget: W,
    pub tooltip_widget: W,
    pub tooltip_timer: f32,
    pub mouse_pos: [f32; 2],
    pub window_z: f32,
    pub mouse_down: [bool; 9],
    pub mouse_movement: bool,
    pub lists: *mut Lists,
}

pub fn lists_new() -> *mut Lists {
    Box::into_raw(Box::default())
}

/// # Safety
/// `l` came from [`lists_new`] and is not used afterwards.
pub unsafe fn lists_free(l: *mut Lists) {
    if !l.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(l) });
    }
}

/// Handle on a `SetState`; every access is a short raw-pointer read or write so host callbacks
/// that re-enter the set never overlap a live reference.
#[derive(Clone, Copy)]
pub struct Core(*mut SetState);

macro_rules! field {
    ($get:ident, $set:ident, $f:ident, $t:ty) => {
        #[allow(dead_code)]
        fn $get(&self) -> $t {
            // SAFETY: `Core::new` contract: the state outlives the handle.
            unsafe { (*self.0).$f }
        }
        fn $set(&self, v: $t) {
            // SAFETY: as above.
            unsafe { (*self.0).$f = v }
        }
    };
}

impl Core {
    /// # Safety
    /// `s` is valid for reads and writes while the handle is used, and its `lists` is null or valid.
    pub unsafe fn new(s: *mut SetState) -> Core {
        Core(s)
    }

    field!(focused, set_focused, focused, W);
    field!(attention, set_attention, attention, W);
    field!(default_nav, set_default_nav, default_nav, W);
    field!(tab_widget, set_tab_widget, tab_widget, W);
    field!(tooltip, set_tooltip, tooltip_widget, W);
    field!(tooltip_timer, set_tooltip_timer, tooltip_timer, f32);
    field!(mouse_pos, set_mouse_pos, mouse_pos, [f32; 2]);
    field!(mouse_movement, _set_mouse_movement, mouse_movement, bool);

    fn lists<R: Default>(&self, f: impl FnOnce(&mut Lists) -> R) -> R {
        // SAFETY: `Core::new` contract; the borrow ends inside `f`, which never calls the host.
        match unsafe { (*self.0).lists.as_mut() } {
            Some(l) => f(l),
            None => R::default(),
        }
    }

    fn mouse_down(&self, i: usize) -> bool {
        // SAFETY: as above.
        unsafe { (*self.0).mouse_down.get(i).copied().unwrap_or(false) }
    }

    fn set_mouse_down(&self, i: usize, v: bool) {
        // SAFETY: as above.
        if let Some(d) = unsafe { (*self.0).mouse_down.get_mut(i) } {
            *d = v;
        }
    }

    pub fn set_window_z(&self, z: f32) {
        // SAFETY: as above.
        unsafe { (*self.0).window_z = z }
    }

    fn msg_at_mouse(&self) -> Msg {
        Msg::at(self.mouse_pos())
    }

    pub fn set_focused_widget(&self, h: &mut dyn Host, mut w: W, check: bool) {
        if self.focused() == w {
            return;
        }
        if check && w != 0 && !h.is_valid(w) {
            w = 0;
        }
        let old = self.focused();
        let data = self.msg_at_mouse();
        self.set_focused(w);
        let now = self.focused();
        if now != 0 {
            h.get_focus(now, &data);
        }
        if old != 0 {
            h.process_message(old, MSG_LOST_FOCUS, &data, false, false);
        }
    }

    pub fn push_focused(&self) {
        let f = self.focused();
        self.lists(|l| l.focus_stack.push(f));
    }

    pub fn pop_focused(&self, h: &mut dyn Host) {
        let Some(top) = self.lists(|l| l.focus_stack.last().copied()) else {
            return;
        };
        self.set_focused_widget(h, top, true);
        self.lists(|l| l.focus_stack.pop());
    }

    pub fn set_attention_widget(&self, h: &mut dyn Host, mut w: W, clear_focus: bool, check: bool) {
        if self.attention() == w {
            return;
        }
        if check && w != 0 && !h.is_valid(w) {
            w = 0;
        }
        self.set_attention(w);
        let data = self.msg_at_mouse();
        let old = self.focused();
        if old != 0
            && !h.is_connected_to(old, self.attention())
            && (self.attention() != 0 || clear_focus)
        {
            self.set_focused(0);
        }
        if self.attention() != 0 && self.focused() == 0 {
            self.set_focused(w);
            let f = self.focused();
            if f != 0 {
                h.process_message(f, MSG_GOT_FOCUS, &data, false, false);
            }
        }
        if old != 0 && old != self.focused() {
            h.process_message(old, MSG_LOST_FOCUS, &data, false, false);
        }
    }

    pub fn push_attention(&self) {
        let a = self.attention();
        self.lists(|l| l.attention_stack.push(a));
    }

    pub fn pop_attention(&self, h: &mut dyn Host, clear_focus: bool) {
        let Some(top) = self.lists(|l| l.attention_stack.last().copied()) else {
            return;
        };
        self.set_attention_widget(h, top, clear_focus, true);
        self.lists(|l| l.attention_stack.pop());
    }

    pub fn set_default_focus_nav(&self, h: &mut dyn Host, mut w: W, check: bool) {
        if self.default_nav() == w {
            return;
        }
        if check && w != 0 && !h.is_valid(w) {
            w = 0;
        }
        self.set_default_nav(w);
    }

    pub fn push_default_focus_nav(&self) {
        let d = self.default_nav();
        self.lists(|l| l.default_stack.push(d));
    }

    pub fn pop_default_focus_nav(&self, h: &mut dyn Host) {
        let Some(top) = self.lists(|l| l.default_stack.last().copied()) else {
            return;
        };
        self.set_default_focus_nav(h, top, true);
        self.lists(|l| l.default_stack.pop());
    }

    /// The C++ reads `front()` of the list even when empty (undefined); this yields null.
    pub fn add_to_tab_order(&self, w: W) {
        let front = self.lists(|l| {
            if w != 0 && !l.tab_order.contains(&w) {
                l.tab_order.push(w);
            }
            l.tab_order.first().copied()
        });
        self.set_tab_widget(front.unwrap_or(0));
    }

    pub fn clear_tab_order(&self) {
        self.lists(|l| l.tab_order.clear());
        self.set_tab_widget(0);
    }

    pub fn add_window(&self, w: W) {
        self.lists(|l| l.windows.push(w));
    }

    pub fn remove_window(&self, w: W) {
        self.lists(|l| l.windows.retain(|x| *x != w));
    }

    pub fn set_window_on_top(&self, h: &mut dyn Host, w: W) {
        if !self.lists(|l| l.windows.contains(&w)) {
            return;
        }
        self.remove_window(w);
        self.add_window(w);
        let mut height = 20.0f32;
        let mut i = 0;
        while let Some(win) = self.lists(|l| l.windows.get(i).copied()) {
            let mut p = h.global_pos(win);
            p[2] = height;
            h.set_global_pos(win, p);
            height += 10.0;
            i += 1;
        }
    }

    /// Focus part of `cGuiSet::DestroyWidget`.
    pub fn destroy_widget_prelude(&self, w: W) {
        if self.focused() == w {
            self.set_focused(0);
        }
        self.lists(|l| l.tab_order.retain(|x| *x != w));
        self.set_tab_widget(0);
        if self.tooltip() == w {
            self.set_tooltip(0);
        }
        if self.default_nav() == w {
            self.set_default_nav(0);
        }
    }

    fn each(&self, h: &mut dyn Host, mut f: impl FnMut(&mut dyn Host, W) -> bool) {
        let mut it = h.list_begin();
        while !h.list_end(it) {
            let w = h.list_get(it);
            if f(h, w) {
                break;
            }
            it = h.list_next(it);
        }
    }

    fn outside_attention(&self, h: &mut dyn Host, w: W) -> bool {
        let a = self.attention();
        a != 0 && !h.is_connected_to(w, a)
    }

    pub fn send_message(&self, h: &mut dyn Host, msg: i32, d: &Msg) -> bool {
        h.sort_widgets();
        match msg {
            MSG_MOUSE_MOVE => self.on_mouse_move(h, d),
            MSG_MOUSE_DOWN => self.on_mouse_down(h, d),
            MSG_MOUSE_UP => self.on_mouse_up(h, d),
            MSG_MOUSE_DOUBLE_CLICK => self.on_mouse_double_click(h, d),
            MSG_KEY_PRESS => self.on_key_press(h, d),
            MSG_KEY_RELEASE => self.on_key_release(h, d),
            MSG_GAMEPAD_INPUT => self.on_gamepad_input(h, d),
            MSG_UI_ARROW_PRESS => self.on_ui_arrow_press(h, d),
            MSG_UI_ARROW_RELEASE => self.on_focused_only(h, MSG_UI_ARROW_RELEASE, d),
            MSG_UI_BUTTON_PRESS => self.on_ui_button(h, MSG_UI_BUTTON_PRESS, d),
            MSG_UI_BUTTON_RELEASE => self.on_ui_button(h, MSG_UI_BUTTON_RELEASE, d),
            MSG_UI_BUTTON_DOUBLE_PRESS => self.on_focused_only(h, MSG_UI_BUTTON_DOUBLE_PRESS, d),
            _ => false,
        }
    }

    fn on_focused_only(&self, h: &mut dyn Host, msg: i32, d: &Msg) -> bool {
        let f = self.focused();
        f != 0 && h.process_message(f, msg, d, false, false)
    }

    fn on_mouse_move(&self, h: &mut dyn Host, a: &Msg) -> bool {
        if !self.mouse_movement() {
            return false;
        }
        let mouse = a.pos();
        self.set_mouse_pos(mouse);
        let old_tip = self.tooltip();
        let mut new_tip: W = 0;
        let mut t = *a;
        let mut val = 0;
        for (i, bit) in MOUSE_BITS.iter().enumerate() {
            if self.mouse_down(i) {
                val |= bit;
            }
        }
        t.set_val(val);

        let mut ret = false;
        let mut pointer_set = false;
        let mut tip_set = false;
        let mut tip_left = false;
        let mut under: W = 0;
        self.each(h, |h, w| {
            if !h.is_visible(w) {
                return false;
            }
            if self.outside_attention(h, w) {
                return false;
            }
            if h.point_is_inside(w, mouse) {
                if under == 0 || h.is_connected_to(under, w) || h.is_connected_to(w, under) {
                    if under == 0 {
                        under = w;
                    }
                    if !h.mouse_is_over(w) {
                        h.set_mouse_is_over(w, true);
                        if h.process_message(w, MSG_MOUSE_ENTER, &t, false, false) {
                            ret = true;
                        }
                        if h.tooltip_enabled(w) && !tip_set {
                            tip_set = true;
                            new_tip = w;
                            if new_tip != old_tip {
                                self.set_tooltip_timer(0.0);
                            }
                        }
                        if h.has_focus_nav(w) && h.is_visible(w) && h.is_enabled(w) {
                            let old_focus = self.focused();
                            let same = old_focus == 0 || {
                                let (pa, pb) = (h.parent(old_focus), h.parent(w));
                                pa == pb && h.widget_set(old_focus) == h.widget_set(w)
                            };
                            if same {
                                self.set_focused_widget(h, w, false);
                            }
                        }
                    }
                    let g = h.pointer_gfx(w);
                    if !pointer_set && g != 0 && !self.outside_attention(h, w) {
                        if h.is_enabled(w) {
                            if h.current_pointer() != g {
                                h.set_current_pointer(g);
                            }
                        } else {
                            let n = h.skin_normal_pointer();
                            h.set_current_pointer(n);
                        }
                        pointer_set = true;
                    }
                }
            } else if h.mouse_is_over(w) {
                h.set_mouse_is_over(w, false);
                h.process_message(w, MSG_MOUSE_LEAVE, &t, false, false);
                if self.focused() == w && h.point_is_inside(w, mouse) {
                    h.set_mouse_is_over(w, true);
                    if h.process_message(w, MSG_MOUSE_ENTER, &t, false, false) {
                        ret = true;
                    }
                } else if old_tip == w {
                    tip_left = true;
                }
            }
            if (h.mouse_is_over(w) || self.focused() == w)
                && h.process_message(w, MSG_MOUSE_MOVE, &t, false, false)
            {
                ret = true;
            }
            false
        });
        if tip_set {
            self.set_tooltip(new_tip);
        } else if tip_left {
            self.set_tooltip(0);
        }
        ret
    }

    fn on_mouse_down(&self, h: &mut dyn Host, a: &Msg) -> bool {
        if let Some(i) = mouse_index(a.val()) {
            self.set_mouse_down(i, true);
        }
        let mut t = *a;
        t.set_pos(self.mouse_pos());
        let old_focus = self.focused();
        let mut ret = false;
        self.each(h, |h, w| {
            if !h.is_visible(w) || self.outside_attention(h, w) {
                return false;
            }
            if h.mouse_is_over(w) {
                if self.focused() != w && h.get_focus(w, &t) {
                    self.set_focused(w);
                }
                if h.process_message(w, MSG_MOUSE_DOWN, &t, false, false) {
                    ret = true;
                    return true;
                }
            }
            false
        });
        if !ret {
            self.set_focused(0);
        }
        if self.focused() != old_focus && old_focus != 0 && h.is_valid(old_focus) {
            h.process_message(old_focus, MSG_LOST_FOCUS, &t, true, true);
        }
        ret
    }

    fn on_mouse_up(&self, h: &mut dyn Host, a: &Msg) -> bool {
        if let Some(i) = mouse_index(a.val()) {
            self.set_mouse_down(i, false);
        }
        let mut t = *a;
        t.set_pos(self.mouse_pos());
        let f = self.focused();
        let mut ret = f != 0 && h.process_message(f, MSG_MOUSE_UP, &t, false, false);
        if !ret {
            self.each(h, |h, w| {
                if self.outside_attention(h, w) {
                    return false;
                }
                if w != self.focused()
                    && h.mouse_is_over(w)
                    && h.process_message(w, MSG_MOUSE_UP, &t, false, false)
                {
                    ret = true;
                    return true;
                }
                false
            });
        }
        ret
    }

    fn on_mouse_double_click(&self, h: &mut dyn Host, a: &Msg) -> bool {
        let mut t = *a;
        t.set_pos(self.mouse_pos());
        let mut ret = false;
        self.each(h, |h, w| {
            if self.outside_attention(h, w) {
                return false;
            }
            if h.mouse_is_over(w) && h.process_message(w, MSG_MOUSE_DOUBLE_CLICK, &t, false, false)
            {
                ret = true;
                return true;
            }
            false
        });
        ret
    }

    fn key_to_arrow(key: i32) -> i32 {
        match key {
            KEY_UP => ARROW_UP,
            KEY_RIGHT => ARROW_RIGHT,
            KEY_DOWN => ARROW_DOWN,
            KEY_LEFT => ARROW_LEFT,
            _ => ARROW_NONE,
        }
    }

    fn tab_focus(&self, h: &mut dyn Host, t: &Msg) -> bool {
        let tab: Vec<W> = self.lists(|l| l.tab_order.clone());
        let old = self.tab_widget();
        let mut found = None;
        if let Some(i) = tab
            .iter()
            .position(|x| *x == self.focused())
            .or_else(|| tab.iter().position(|x| *x == old))
        {
            let mut i = i;
            let mut cand;
            loop {
                if t.modifier() == MOD_SHIFT {
                    if i == 0 {
                        i = tab.len();
                    }
                    i -= 1;
                } else if t.modifier() == MOD_NONE {
                    i += 1;
                    if i == tab.len() {
                        i = 0;
                    }
                }
                cand = tab.get(i).copied().unwrap_or(0);
                if h.is_visible(cand) || h.is_enabled(cand) {
                    break;
                }
            }
            found = Some(cand);
        }
        match found {
            Some(n) if n != 0 && h.is_visible(n) && h.is_enabled(n) => {
                self.set_tab_widget(n);
                self.set_focused_widget(h, n, false);
                let tw = self.tab_widget();
                if tw != 0 {
                    h.got_tab_focus(tw, t);
                }
                true
            }
            _ => false,
        }
    }

    fn on_key_press(&self, h: &mut dyn Host, a: &Msg) -> bool {
        let mut t = *a;
        t.set_pos(self.mouse_pos());
        let mut ret = false;
        if t.key() == KEY_TAB && self.lists(|l| !l.tab_order.is_empty()) {
            ret = self.tab_focus(h, &t);
        }
        if !ret {
            let f = self.focused();
            if f != 0 {
                ret = h.process_message(f, MSG_KEY_PRESS, &t, false, false);
            }
            if !ret {
                let dir = Self::key_to_arrow(t.key());
                if dir != ARROW_NONE {
                    ret = self.send_message(h, MSG_UI_ARROW_PRESS, &Msg::with_val(dir));
                }
            }
        }
        if !ret {
            let saved = self.focused();
            self.set_focused_widget(h, 0, false);
            ret = h.exec_shortcut(&t);
            if self.focused() == 0 {
                self.set_focused_widget(h, saved, false);
            }
        }
        if !ret {
            self.each(h, |h, w| {
                if self.outside_attention(h, w) {
                    return false;
                }
                if (h.mouse_is_over(w) || h.global_key_listener(w) && self.focused() != w)
                    && h.process_message(w, MSG_KEY_PRESS, &t, false, false)
                {
                    ret = true;
                    return true;
                }
                false
            });
        }
        ret
    }

    fn on_key_release(&self, h: &mut dyn Host, a: &Msg) -> bool {
        let mut t = *a;
        t.set_pos(self.mouse_pos());
        let mut ret = false;
        let f = self.focused();
        if f != 0 {
            ret = h.process_message(f, MSG_KEY_RELEASE, &t, false, false);
            if !ret {
                let dir = Self::key_to_arrow(t.key());
                if dir != ARROW_NONE {
                    ret = self.send_message(h, MSG_UI_ARROW_RELEASE, &Msg::with_val(dir));
                }
            }
        }
        if !ret {
            self.each(h, |h, w| {
                self.hover_message(h, w, MSG_KEY_RELEASE, &t, &mut ret)
            });
        }
        ret
    }

    fn on_gamepad_input(&self, h: &mut dyn Host, a: &Msg) -> bool {
        let mut ret = false;
        let f = self.focused();
        if f != 0 {
            ret = h.process_message(f, MSG_GAMEPAD_INPUT, a, false, false);
        }
        if !ret {
            self.each(h, |h, w| {
                self.hover_message(h, w, MSG_GAMEPAD_INPUT, a, &mut ret)
            });
        }
        ret
    }

    fn hover_message(&self, h: &mut dyn Host, w: W, msg: i32, d: &Msg, ret: &mut bool) -> bool {
        if self.outside_attention(h, w) {
            return false;
        }
        if h.mouse_is_over(w) && self.focused() != w && h.process_message(w, msg, d, false, false) {
            *ret = true;
            return true;
        }
        false
    }

    /// Offers `msg` to global UI input listeners other than the focused widget.
    fn offer_to_global_listeners(&self, h: &mut dyn Host, msg: i32, d: &Msg) -> bool {
        let mut ret = false;
        self.each(h, |h, w| {
            if !h.global_ui_listener(w) || w == self.focused() || !h.is_visible(w) {
                return false;
            }
            if self.outside_attention(h, w) {
                return false;
            }
            ret = h.process_message(w, msg, d, false, false);
            ret
        });
        ret
    }

    fn on_ui_arrow_press(&self, h: &mut dyn Host, d: &Msg) -> bool {
        let f = self.focused();
        if f == 0 || !h.has_focus_nav(f) && f != self.default_nav() {
            let def = self.default_nav();
            if def == 0 {
                return false;
            }
            let a = self.attention();
            if a == 0 || h.is_connected_to(def, a) {
                self.set_focused_widget(h, def, false);
                let now = self.focused();
                if now != 0 {
                    h.process_message(now, MSG_GET_UI_NAV_FOCUS, &Msg::zero(), false, false);
                }
                return true;
            }
            return false;
        }
        let mut ret = h.process_message(f, MSG_UI_ARROW_PRESS, d, false, false);
        let cur = self.focused();
        if !ret && cur != 0 {
            let next = h.focus_nav(cur, d.val());
            let a = self.attention();
            if next != 0
                && h.is_enabled(next)
                && h.is_visible(next)
                && (a == 0 || h.is_connected_to(next, a))
            {
                let old = self.focused();
                self.set_focused_widget(h, next, false);
                let now = self.focused();
                if now != 0 {
                    h.process_message(now, MSG_GET_UI_NAV_FOCUS, &Msg::zero(), false, false);
                }
                h.process_message(old, MSG_LOSE_UI_NAV_FOCUS, &Msg::zero(), false, false);
                ret = true;
            }
        }
        if !ret {
            ret = self.offer_to_global_listeners(h, MSG_UI_ARROW_PRESS, d);
        }
        ret
    }

    fn on_ui_button(&self, h: &mut dyn Host, msg: i32, d: &Msg) -> bool {
        let f = self.focused();
        let ret = f != 0 && h.process_message(f, msg, d, false, false);
        ret || self.offer_to_global_listeners(h, msg, d)
    }
}
