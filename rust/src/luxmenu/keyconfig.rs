//! Key config window: the two bindings of an action, matching, conflict marking and the rebinding
//! decisions. Sub-actions are `cSubActionWrapper` (same layout); engine calls stay in C++.

use crate::input;
use crate::string;

pub const DEV_KEYBOARD: i32 = 0;
pub const DEV_MOUSE: i32 = 1;
pub const DEV_GAMEPAD: i32 = 2;
pub const DEV_NONE: i32 = 3;
pub const GP_BUTTON: i32 = 0;
pub const GP_HAT: i32 = 1;
pub const GP_AXIS: i32 = 2;
pub const GP_NONE: i32 = 4;
pub const HAT_LAST: i32 = 10;
pub const AXIS_LAST: i32 = 10;
pub const POS_NUM: usize = 2;
pub const POS_LAST: i32 = 2;

#[repr(C)]
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct Sub {
    pub kind: i32,
    pub gtype: i32,
    pub id: i32,
    pub value: f32,
}

impl Sub {
    pub const EMPTY: Sub = Sub::dev(DEV_NONE, -1);

    pub const fn dev(kind: i32, id: i32) -> Sub {
        Sub {
            kind,
            gtype: GP_NONE,
            id,
            value: 1.0,
        }
    }

    pub const fn pad(gtype: i32, id: i32, value: f32) -> Sub {
        Sub {
            kind: DEV_GAMEPAD,
            gtype,
            id,
            value,
        }
    }
}

/// Hat states and axis ranges are unsigned C++ enums: a negative int widens through `unsigned`.
fn enum_f32(x: i32) -> f32 {
    (x as u32) as f32
}

fn pos_of(subs: &[Sub], f: impl Fn(&Sub) -> bool) -> i32 {
    subs.iter().position(f).map_or(POS_LAST, |i| i as i32)
}

pub fn key_pos(subs: &[Sub], key: i32) -> i32 {
    pos_of(subs, |s| s.kind == DEV_KEYBOARD && s.id == key)
}

pub fn mouse_pos(subs: &[Sub], button: i32) -> i32 {
    pos_of(subs, |s| s.kind == DEV_MOUSE && s.id == button)
}

pub fn pad_button_pos(subs: &[Sub], button: i32) -> i32 {
    pos_of(subs, |s| s.gtype == GP_BUTTON && s.id == button)
}

pub fn pad_hat_pos(subs: &[Sub], hat: i32, state: i32) -> i32 {
    pos_of(subs, |s| {
        s.gtype == GP_HAT && s.id == hat && s.value == enum_f32(state)
    })
}

pub fn pad_axis_pos(subs: &[Sub], axis: i32, range: i32) -> i32 {
    pos_of(subs, |s| {
        s.gtype == GP_AXIS && s.id == axis && s.value == enum_f32(range)
    })
}

pub fn has_sub_actions(subs: &[Sub]) -> bool {
    subs.iter().take(POS_NUM).any(|s| s.kind != DEV_NONE)
}

/// Positions of `a` whose binding `b` also holds.
pub fn sharing(a: &[Sub], b: &[Sub]) -> Vec<i32> {
    let mut out = Vec::new();
    for (i, s) in a.iter().enumerate() {
        let shared = match s.kind {
            DEV_KEYBOARD => key_pos(b, s.id) != POS_LAST,
            DEV_MOUSE => mouse_pos(b, s.id) != POS_LAST,
            DEV_GAMEPAD => match s.gtype {
                GP_BUTTON => pad_button_pos(b, s.id) != POS_LAST,
                GP_HAT => pad_hat_pos(b, s.id, s.value as i32) != POS_LAST,
                GP_AXIS => pad_axis_pos(b, s.id, s.value as i32) != POS_LAST,
                _ => false,
            },
            _ => false,
        };
        if shared {
            out.push(i as i32);
        }
    }
    out
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Bind {
    Key(i32),
    Mouse(i32),
    PadButton(i32),
    PadHat(i32, i32),
    PadAxis(i32, i32),
}

/// `SetInput*`: binds at `pos` unless the action already holds it. A `pos` that is not a slot
/// writes nothing (the C++ wrote out of bounds).
pub fn set(subs: &mut [Sub], pos: i32, bind: Bind) {
    let (held, new) = match bind {
        Bind::Key(k) => (key_pos(subs, k), Sub::dev(DEV_KEYBOARD, k)),
        Bind::Mouse(b) => (mouse_pos(subs, b), Sub::dev(DEV_MOUSE, b)),
        Bind::PadButton(b) => (pad_button_pos(subs, b), Sub::pad(GP_BUTTON, b, 1.0)),
        Bind::PadHat(h, st) => (pad_hat_pos(subs, h, st), Sub::pad(GP_HAT, h, enum_f32(st))),
        Bind::PadAxis(ax, r) => (
            pad_axis_pos(subs, ax, r),
            Sub::pad(GP_AXIS, ax, enum_f32(r)),
        ),
    };
    if held != POS_LAST {
        return;
    }
    if let Some(s) = usize::try_from(pos).ok().and_then(|p| subs.get_mut(p)) {
        *s = new;
    }
}

fn part<'a>(parts: &[&'a [u8]], i: usize) -> &'a [u8] {
    parts.get(i).copied().unwrap_or(&[])
}

fn or_name(value: i32, name: impl FnOnce() -> i32) -> i32 {
    if value != -1 { value } else { name() }
}

fn from_pad(parts: &[&[u8]], value: i32) -> Sub {
    if part(parts, 0) == b"GamepadButton" {
        let b = or_name(value, || input::pad_button_from_name(part(parts, 1)));
        return Sub::pad(GP_BUTTON, b, 1.0);
    }
    let hat = input::pad_hat_from_name(part(parts, 1));
    let axis = input::pad_axis_from_name(part(parts, 1));
    if hat != HAT_LAST {
        let state = or_name(value, || input::pad_hat_state_from_name(part(parts, 2)));
        Sub::pad(GP_HAT, hat, enum_f32(state))
    } else if axis != AXIS_LAST {
        let range = or_name(value, || input::pad_axis_range_from_name(part(parts, 2)));
        Sub::pad(GP_AXIS, axis, enum_f32(range))
    } else {
        Sub::dev(DEV_GAMEPAD, -1)
    }
}

/// `GetSubActionWrapperFromString`. Missing dotted parts read as empty (the C++ read past the
/// vector).
pub fn from_string(s: &[u8], value: i32) -> Sub {
    let parts: Vec<&[u8]> = string::split(s, Some(&b"."[..]), true).collect();
    let dev = string::to_lower(part(&parts, 0));
    if dev == b"keyboard" {
        let k = or_name(value, || input::key_from_name(part(&parts, 1)));
        Sub::dev(DEV_KEYBOARD, k)
    } else if dev == b"mousebutton" || dev == b"mouse" {
        let b = or_name(value, || input::mouse_button_from_name(part(&parts, 1)));
        Sub::dev(DEV_MOUSE, b)
    } else if string::first_pos(&dev, b"gamepad") != -1 {
        from_pad(&parts, value)
    } else {
        Sub::EMPTY
    }
}

/// Button label before translation.
pub fn label(s: &Sub) -> Vec<u8> {
    match s.kind {
        DEV_KEYBOARD => input::key_name(s.id).as_bytes().to_vec(),
        DEV_MOUSE => input::mouse_button_name(s.id).as_bytes().to_vec(),
        DEV_GAMEPAD => match s.gtype {
            GP_BUTTON => input::pad_button_name(s.id).as_bytes().to_vec(),
            GP_HAT => [
                input::pad_hat_name(s.id),
                ".",
                input::pad_hat_state_name(s.value as i32),
            ]
            .concat()
            .into_bytes(),
            GP_AXIS => [
                input::pad_axis_name(s.id),
                ".",
                input::pad_axis_range_name(s.value as i32),
            ]
            .concat()
            .into_bytes(),
            _ => b"None".to_vec(),
        },
        _ => b"None".to_vec(),
    }
}

fn clear_slot(s: &mut Sub) {
    s.kind = DEV_NONE;
    s.id = -1;
}

/// `RetrieveDefaultValue`: `mac_attack_key` is the macOS Attack kludge.
pub fn fill_default(inputs: &[(&[u8], i32)], subs: &mut [Sub], mac_attack_key: Option<i32>) {
    let mut i = 0;
    for (name, value) in inputs {
        let Some(slot) = subs.get_mut(i) else { break };
        *slot = from_string(name, *value);
        i += 1;
    }
    if let (Some(key), Some(slot)) = (mac_attack_key, subs.get_mut(i).filter(|_| i < POS_NUM)) {
        slot.kind = DEV_KEYBOARD;
        slot.id = key;
        i += 1;
    }
    subs.iter_mut().skip(i).for_each(clear_slot);
}

/// `RetrieveInitialValue`: `names` are the engine's `type.name` strings.
pub fn fill_initial(names: &[Vec<u8>], subs: &mut [Sub]) {
    let mut i = 0;
    for name in names {
        let Some(slot) = subs.get_mut(i) else { break };
        *slot = from_string(name, -1);
        i += 1;
    }
    subs.iter_mut().skip(i).for_each(clear_slot);
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub enum Op {
    Key(i32),
    Mouse(i32),
    PadButton(i32),
    PadHat(i32, i32),
    PadAxis(i32, i32),
}

/// `UpdateAction`: the engine calls for the bindings, `None` when nothing changed.
pub fn update_ops(subs: &[Sub], changed: bool) -> Option<Vec<Op>> {
    if !changed {
        return None;
    }
    let ops = subs
        .iter()
        .filter_map(|s| match (s.kind, s.gtype) {
            (DEV_KEYBOARD, _) => Some(Op::Key(s.id)),
            (DEV_MOUSE, _) => Some(Op::Mouse(s.id)),
            (DEV_GAMEPAD, GP_BUTTON) => Some(Op::PadButton(s.id)),
            (DEV_GAMEPAD, GP_HAT) => Some(Op::PadHat(s.id, s.value as i32)),
            (DEV_GAMEPAD, GP_AXIS) => Some(Op::PadAxis(s.id, s.value as i32)),
            _ => None,
        })
        .collect();
    Some(ops)
}

/// `CheckEmptyAndDuplicateInputs`. `subs` holds two bindings per entry, `counts` the entries per
/// category; `set(entry, pos, red)` runs at the C++ call's original point. Returns "invalid".
pub fn check_inputs(subs: &[Sub], counts: &[usize], mut set: impl FnMut(usize, i32, bool)) -> bool {
    let entry = |k: usize| subs.get(k * POS_NUM..(k + 1) * POS_NUM);
    let total: usize = counts.iter().sum();
    for k in 0..total {
        set(k, 0, false);
        set(k, 1, false);
    }
    let mut invalid = false;
    let mut base = 0;
    for &n in counts {
        for i in 0..n {
            for j in i + 1..n {
                let (Some(e1), Some(e2)) = (entry(base + i), entry(base + j)) else {
                    continue;
                };
                if !has_sub_actions(e2) {
                    invalid = true;
                    set(base + j, 0, true);
                    set(base + j, 1, true);
                    continue;
                }
                let shared = sharing(e1, e2);
                if !shared.is_empty() {
                    invalid = true;
                    shared.iter().for_each(|&p| set(base + i, p, true));
                    sharing(e2, e1).iter().for_each(|&p| set(base + j, p, true));
                }
            }
        }
        base += n;
    }
    invalid
}

/// Lean actions have no gamepad or UI-button binding.
pub fn binds_pad(action: &[u8]) -> bool {
    action != b"LeanLeft" && action != b"LeanRight"
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum KeyAct {
    None,
    Save,
    Cancel,
}

pub fn key_press(waiting: bool, key: i32, escape: i32) -> KeyAct {
    match (waiting, key == escape) {
        (false, _) => KeyAct::None,
        (true, false) => KeyAct::Save,
        (true, true) => KeyAct::Cancel,
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum PadAct {
    /// Not waiting: the event is consumed.
    Eat,
    /// Zero value: let it pass.
    Pass,
    Cancel,
    /// Save and raise the gamepad-press flag.
    Save,
}

pub fn pad_press(
    waiting: bool,
    value: f32,
    is_button: bool,
    id: i32,
    cancel_button: i32,
) -> PadAct {
    if !waiting {
        return PadAct::Eat;
    }
    if value == 0.0 {
        return PadAct::Pass;
    }
    if is_button && id == cancel_button {
        PadAct::Cancel
    } else {
        PadAct::Save
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Press {
    pub handled: bool,
    pub clear_flag: bool,
    pub start_waiting: bool,
}

/// `InputEntryUIButtonPress`.
pub fn ui_button(is_primary: bool, pad_flag: bool, nobody_waiting: bool) -> Press {
    if !is_primary {
        return Press {
            handled: false,
            clear_flag: false,
            start_waiting: false,
        };
    }
    if pad_flag {
        return Press {
            handled: true,
            clear_flag: true,
            start_waiting: false,
        };
    }
    Press {
        handled: true,
        clear_flag: false,
        start_waiting: nobody_waiting,
    }
}

/// `InputEntryUIArrowPress`.
pub fn ui_arrow(pad_flag: bool) -> Press {
    Press {
        handled: pad_flag,
        clear_flag: pad_flag,
        start_waiting: false,
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Click {
    Nothing,
    StartWaiting,
    SaveMouse,
}

pub fn click(nobody_waiting: bool, is_left: bool) -> Click {
    match (nobody_waiting, is_left) {
        (true, true) => Click::StartWaiting,
        (true, false) => Click::Nothing,
        (false, _) => Click::SaveMouse,
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Waiting {
    Ignore,
    Push,
    Pop,
}

/// `SetWaitingInput`: a new widget is ignored while another one is waiting.
pub fn set_waiting(has_new: bool, has_current: bool) -> Waiting {
    match (has_new, has_current) {
        (true, true) => Waiting::Ignore,
        (true, false) => Waiting::Push,
        (false, _) => Waiting::Pop,
    }
}

/// Tip fade flags after the tip text is known: `(fade_out, fade_restore)`.
pub fn tip_flags(tip_empty: bool) -> (bool, bool) {
    (tip_empty, !tip_empty)
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub struct Fade {
    pub out: bool,
    pub restore: bool,
}

/// `WindowOnUpdate` tip fade: the alpha after each colour write, in order.
pub fn tip_fade(mut alpha: f32, mut f: Fade, dt: f32) -> (Fade, Vec<f32>) {
    let mut sets = Vec::new();
    if f.out {
        alpha = (-dt).mul_add(0.8, alpha);
        if alpha <= 0.0 {
            alpha = 0.0;
            f.out = false;
        }
        sets.push(alpha);
    }
    if f.restore {
        alpha = dt.mul_add(3.0, alpha);
        if alpha >= 1.0 {
            alpha = 1.0;
            f.restore = false;
        }
        sets.push(alpha);
    }
    (f, sets)
}

pub fn category_active(selected: i32, category: i32) -> bool {
    selected == category.wrapping_sub(1)
}

/// `CategorySelector_OnUIButtonPress`: category to select, `None` when not handled.
pub fn category_button(
    was_waiting: bool,
    button: i32,
    prev: i32,
    next: i32,
    category: i32,
    items: i32,
) -> Option<i32> {
    if was_waiting {
        return None;
    }
    if button == prev {
        let p = category.wrapping_sub(1);
        Some(if p >= 0 { p } else { category })
    } else if button == next {
        let n = category.wrapping_add(1);
        Some(if n < items { n } else { category })
    } else {
        None
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Exit {
    Nothing,
    ClearCancelled,
    ToOptions,
}

pub fn exit_pressed(popup_active: bool, cancelled: bool) -> Exit {
    if popup_active {
        Exit::Nothing
    } else if cancelled {
        Exit::ClearCancelled
    } else {
        Exit::ToOptions
    }
}

/// `UIPressCancel`: leave to the options unless the window was waiting for input this frame.
pub fn ui_cancel(was_waiting: bool, is_secondary: bool) -> bool {
    !was_waiting && is_secondary
}
