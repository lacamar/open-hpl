//! Start game, load game, profile and custom story windows of the Amnesia main menu (A9a). Rust owns
//! the window objects and the main menu (`mainmenu`) drives them; every widget call goes through
//! `host`.

#[cfg(test)]
pub(crate) mod fake;
pub mod host;
mod load;
mod profwin;
mod start;
mod story;
#[cfg(test)]
mod tests;

use crate::obj::{Arena, Handle};
use host::W;
use std::cell::{Cell, RefCell};
use std::rc::Rc;

pub const WIN_PROFILE: i32 = 0;
pub const WIN_LOAD: i32 = 3;
pub const WIN_STORY_LIST: i32 = 4;
pub const WIN_STORY: i32 = 5;
pub const WIN_NONE: usize = 7;
pub const EXIT_START_GAME: usize = 2;

pub struct Base {
    bits: Cell<u64>,
    set: W,
    window: Cell<W>,
}

impl Base {
    fn new(set: W) -> Base {
        Base {
            bits: Cell::new(0),
            set,
            window: Cell::new(W::NULL),
        }
    }

    /// Callback token: window handle in the high bits, callback id in the low byte.
    fn tok(&self, cb: u8) -> usize {
        ((self.bits.get() as usize) << 8) | usize::from(cb)
    }

    fn set_window(&self, w: W) {
        self.window.set(w);
    }

    fn popup_active(&self) -> bool {
        self.set.popup_active()
    }
}

pub(crate) fn go(window: usize) {
    host::lux(host::lux_op::MENU_SET_WINDOW, window);
}

enum Win {
    Profile(profwin::Profile),
    Load(load::Load),
    Start(start::Start),
    Story(story::Story),
    List(story::List),
}

impl Win {
    fn base(&self) -> &Base {
        match self {
            Win::Profile(w) => &w.base,
            Win::Load(w) => &w.base,
            Win::Start(w) => &w.base,
            Win::Story(w) => &w.base,
            Win::List(w) => &w.base,
        }
    }

    fn create_gui(&self) {
        match self {
            Win::Profile(w) => w.create_gui(),
            Win::Load(w) => w.create_gui(),
            Win::Start(w) => w.create_gui(),
            Win::Story(w) => w.create_gui(),
            Win::List(w) => w.create_gui(),
        }
    }

    fn exit_pressed(&self) {
        match self {
            Win::Profile(_) => {}
            Win::Load(w) => w.exit_pressed(),
            Win::Start(w) => w.exit_pressed(),
            Win::Story(_) => {}
            Win::List(w) => w.exit_pressed(),
        }
    }

    fn set_active(&self, on: bool) {
        match self {
            Win::Profile(w) => w.set_active(on),
            Win::Load(w) => w.set_active(on),
            Win::Start(w) => w.set_active(on),
            Win::Story(_) => {}
            Win::List(w) => w.set_active(on),
        }
    }

    fn callback(&self, id: u8, widget: W, val: i32) -> bool {
        match self {
            Win::Profile(w) => w.callback(id, widget, val),
            Win::Load(w) => w.callback(id, widget, val),
            Win::Start(w) => w.callback(id, widget, val),
            Win::Story(w) => w.callback(id, widget, val),
            Win::List(w) => w.callback(id, widget, val),
        }
    }
}

thread_local! {
    static REG: RefCell<Arena<Rc<Win>>> = RefCell::new(Arena::default());
}

fn lookup(bits: u64) -> Option<Rc<Win>> {
    let h = Handle::<Rc<Win>>::from_bits(bits)?;
    REG.with(|r| r.borrow().get(h).cloned())
}

pub const KIND_PROFILE: i32 = 0;
pub const KIND_LOAD: i32 = 1;
pub const KIND_START: i32 = 2;
pub const KIND_STORY: i32 = 3;
pub const KIND_STORY_LIST: i32 = 4;

pub fn new(kind: i32, set: W, story_win: u64) -> u64 {
    let base = Base::new(set);
    let win = match kind {
        KIND_PROFILE => Win::Profile(profwin::Profile::new(base)),
        KIND_LOAD => Win::Load(load::Load::new(base)),
        KIND_START => Win::Start(start::Start::new(base)),
        KIND_STORY => Win::Story(story::Story::new(base)),
        KIND_STORY_LIST => Win::List(story::List::new(base, story_win)),
        _ => return 0,
    };
    let win = Rc::new(win);
    let Some(h) = REG.with(|r| r.borrow_mut().insert(Rc::clone(&win))) else {
        return 0;
    };
    let bits = h.to_bits();
    // The token keeps 8 bits for the callback id.
    if bits >> 55 != 0 {
        REG.with(|r| r.borrow_mut().remove(h));
        return 0;
    }
    win.base().bits.set(bits);
    bits
}

pub fn free(bits: u64) {
    let Some(h) = Handle::<Rc<Win>>::from_bits(bits) else {
        return;
    };
    let win = REG.with(|r| r.borrow_mut().remove(h));
    drop(win);
}

pub fn create_gui(bits: u64) {
    if let Some(w) = lookup(bits) {
        w.create_gui();
    }
}

pub fn exit_pressed(bits: u64) {
    if let Some(w) = lookup(bits) {
        w.exit_pressed();
    }
}

pub fn set_active(bits: u64, on: bool) {
    if let Some(w) = lookup(bits) {
        w.set_active(on);
    }
}

/// The window widget once `create_gui` has run, else 0.
pub fn widget(bits: u64) -> usize {
    lookup(bits).map_or(0, |w| w.base().window.get().addr())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9_cb(tok: usize, widget: W, val: i32) -> bool {
    let (Some(w), id) = (lookup((tok >> 8) as u64), (tok & 0xFF) as u8) else {
        return false;
    };
    w.callback(id, widget, val)
}

/// `eUIButton` values carried by `eGuiMessage_UIButtonPress`.
pub(crate) fn ui_switch(val: i32) -> i32 {
    crate::luxmenu2::pages::ui_switch(val)
}

pub(crate) fn popup_ok(val: i32) -> bool {
    crate::luxmenu2::menu::popup_ok(val)
}

pub(crate) const UI_PRIMARY: i32 = 1;
pub(crate) const UI_SECONDARY: i32 = 2;
