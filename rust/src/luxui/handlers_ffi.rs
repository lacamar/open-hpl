//! C ABI of the hint and message handlers (`amnesia/src/game/RustLuxUiA11b.h`). The handlers live
//! in thread-local arenas; C++ shells hold the handle bits.

use super::hint_handler::{Hint, HintHandler};
use super::host::{Slot, Ui, host};
use super::msg_handler::{MessageHandler, Msg};
use crate::obj::{Arena, Handle};
use std::cell::RefCell;
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink = unsafe extern "C" fn(Ctx, *const u8, usize);

thread_local! {
    static HINTS: RefCell<Arena<HintHandler>> = RefCell::new(Arena::default());
    static MSGS: RefCell<Arena<MessageHandler>> = RefCell::new(Arena::default());
}

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        return &[];
    }
    // SAFETY: the caller passes `n` readable bytes.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn units<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if p.is_null() || n == 0 {
        return &[];
    }
    // SAFETY: the caller passes `n` readable units.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn hint<R>(bits: u64, none: R, f: impl FnOnce(&Hint) -> R) -> R {
    let (Some(h), Some(ui)) = (Handle::from_bits(bits), host()) else {
        return none;
    };
    HINTS.with(|a| {
        f(&Hint {
            ui: &ui,
            c: Slot { arena: a, h },
        })
    })
}

fn msg<R>(bits: u64, none: R, f: impl FnOnce(&Msg) -> R) -> R {
    let (Some(h), Some(ui)) = (Handle::from_bits(bits), host()) else {
        return none;
    };
    MSGS.with(|a| {
        f(&Msg {
            ui: &ui,
            c: Slot { arena: a, h },
        })
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_new() -> u64 {
    let Some(ui) = host() else { return 0 };
    let h = HintHandler::new(&ui);
    let bits = HINTS.with(|a| a.borrow_mut().insert(h).map_or(0, Handle::to_bits));
    super::dump::hint_created(bits);
    bits
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_free(h: u64) {
    if let Some(h) = Handle::from_bits(h) {
        let old = HINTS.with(|a| a.borrow_mut().remove(h));
        drop(old);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_font(h: u64, font: usize) {
    hint(h, (), |x| {
        x.c.with(|s| s.font = font);
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_reset(h: u64) {
    hint(h, (), |x| {
        x.c.with(HintHandler::reset);
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_load_user_config(h: u64) {
    hint(h, (), |x| {
        let on = x.ui.user_bool(b"Game", b"ShowHints", true);
        x.c.with(|s| s.s.active = u8::from(on));
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_save_user_config(h: u64) {
    hint(h, (), |x| {
        if let Some(on) = x.c.with(|s| s.s.active != 0) {
            x.ui.set_user_bool(b"Game", b"ShowHints", on);
        }
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_update(h: u64, dt: f32) {
    hint(h, (), |x| x.update(dt));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_draw(h: u64, set: usize) {
    hint(h, (), |x| x.draw(set));
}

/// `cLuxHintHandler::Update`: the dump driver steps the handler from its draw instead.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_on_update(h: u64, dt: f32) {
    if !super::dump::enabled() {
        hint(h, (), |x| x.update(dt));
    }
}

/// `cLuxHintHandler::OnDraw`.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_on_draw(h: u64, set: usize) {
    if super::dump::enabled() {
        hint(h, (), |x| x.update(super::dump::DT));
    }
    hint(h, (), |x| x.draw(set));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_set_active(h: u64, on: i32) {
    hint(h, (), |x| {
        x.c.with(|s| s.set_active(on != 0));
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_is_active(h: u64) -> i32 {
    hint(h, 0, |x| {
        i32::from(x.c.with(|s| s.s.active != 0).unwrap_or(false))
    })
}

/// # Safety
/// `name` is `nn` bytes and `text` is `tn` units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a11b_hint_add(
    h: u64,
    name: *const u8,
    nn: usize,
    text: *const u32,
    tn: usize,
    time: f32,
) -> i32 {
    hint(h, 0, |x| {
        i32::from(x.add(bytes(name, nn), units(text, tn), time))
    })
}

/// # Safety
/// `name` is `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a11b_hint_remove(h: u64, name: *const u8, n: usize) -> i32 {
    hint(h, 0, |x| {
        i32::from(x.c.with(|s| s.remove(bytes(name, n))).unwrap_or(false))
    })
}

/// # Safety
/// `name` is `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a11b_hint_block(h: u64, name: *const u8, n: usize, on: i32) {
    hint(h, (), |x| {
        x.c.with(|s| {
            if on != 0 {
                s.block(bytes(name, n));
            } else {
                s.unblock(bytes(name, n));
            }
        });
    });
}

/// Calls `sink` once per name of the given or blocked set, in set order.
///
/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a11b_hint_names(h: u64, blocked: i32, sink: Sink, ctx: Ctx) {
    let names = hint(h, Vec::new(), |x| {
        x.c.with(|s| s.names(blocked != 0)).unwrap_or_default()
    });
    for n in &names {
        // SAFETY: `sink` accepts `ctx`.
        unsafe { sink(ctx, n.as_ptr(), n.len()) };
    }
}

/// Replaces the given or blocked set; `ohpl_a11b_hint_names_add` then fills it.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_hint_names_clear(h: u64, blocked: i32) {
    hint(h, (), |x| {
        x.c.with(|s| s.clear_names(blocked != 0));
    });
}

/// # Safety
/// `name` is `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a11b_hint_names_add(h: u64, blocked: i32, name: *const u8, n: usize) {
    hint(h, (), |x| {
        x.c.with(|s| s.add_name(blocked != 0, bytes(name, n)));
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_new() -> u64 {
    let Some(ui) = host() else { return 0 };
    let h = MessageHandler::new(&ui);
    let bits = MSGS.with(|a| a.borrow_mut().insert(h).map_or(0, Handle::to_bits));
    super::dump::msg_created(bits);
    bits
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_free(h: u64) {
    if let Some(h) = Handle::from_bits(h) {
        let old = MSGS.with(|a| a.borrow_mut().remove(h));
        drop(old);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_font(h: u64, font: usize) {
    msg(h, (), |x| {
        x.c.with(|s| s.font = font);
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_reset(h: u64) {
    msg(h, (), |x| {
        x.c.with(MessageHandler::reset);
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_load_user_config(h: u64) {
    msg(h, (), |x| {
        let ui: &dyn Ui = x.ui;
        x.c.with(|s| s.load_user_config(ui));
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_save_user_config(h: u64) {
    msg(h, (), |x| {
        let ui: &dyn Ui = x.ui;
        x.c.with(|s| s.save_user_config(ui));
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_update(h: u64, dt: f32) {
    msg(h, (), |x| x.update(dt));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_draw(h: u64) {
    msg(h, (), |x| x.draw());
}

/// `cLuxMessageHandler::Update`.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_on_update(h: u64, dt: f32) {
    if !super::dump::enabled() {
        msg(h, (), |x| x.update(dt));
    }
}

/// `cLuxMessageHandler::OnDraw`: the dump drivers step first.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_on_draw(h: u64) {
    super::dump::tick();
    if super::dump::enabled() {
        msg(h, (), |x| x.update(super::dump::DT));
    }
    msg(h, (), |x| x.draw());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_start_quest(h: u64) {
    msg(h, (), |x| {
        x.c.with(MessageHandler::start_quest_added);
    });
}

/// # Safety
/// `text` is `n` units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a11b_msg_start_pause(
    h: u64,
    text: *const u32,
    n: usize,
    yes_no: i32,
    cb: usize,
) {
    msg(h, (), |x| x.start_pause(units(text, n), yes_no != 0, cb));
}

/// # Safety
/// `text` is `n` units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a11b_msg_set_message(h: u64, text: *const u32, n: usize, time: f32) {
    msg(h, (), |x| x.set_message(units(text, n), time));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_do_action(h: u64, action: i32, pressed: i32) {
    msg(h, (), |x| x.do_action(action, pressed != 0));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_set_pause(h: u64, on: i32) {
    msg(h, (), |x| x.set_pause_active(on != 0));
}

/// Flag `which`: 0 message active, 1 pause active, 2 subtitles, 3 effect subtitles.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_flag(h: u64, which: i32) -> i32 {
    msg(h, 0, |x| {
        i32::from(
            x.c.with(|s| match which {
                0 => s.is_message_active(),
                1 => s.s.pause_active != 0,
                2 => s.s.show_subtitles != 0,
                _ => s.s.show_effect_subtitles != 0,
            })
            .unwrap_or(false),
        )
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a11b_msg_set_flag(h: u64, which: i32, on: i32) {
    msg(h, (), |x| {
        x.c.with(|s| match which {
            2 => s.s.show_subtitles = u8::from(on != 0),
            _ => s.s.show_effect_subtitles = u8::from(on != 0),
        });
    });
}
