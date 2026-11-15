//! `cLuxMainMenu_KeyConfig` and `cLuxInputMenuEntry`: the key binding window, built and driven
//! from Rust over the host ops. Entries live in an arena; widget callbacks carry their handle.

use super::host::{self as h, Args, DOWN, LEFT, RIGHT, Set, UP, Wd, call, msg, op, tr};
use super::keyconfig::{self as kc, Bind, Fade, Sub};
use super::store::{Cb, CbData, Kind, lock, token};
use crate::obj::{Arena, Handle};
use std::sync::{LazyLock, Mutex};

const OPTIONS_WINDOW: i32 = 1;
const CATEGORIES: usize = 4;
const POS_SECONDARY: i32 = 1;

pub const WINDOW_UPDATE: u16 = 1;
pub const CATEGORY: u16 = 2;
pub const CATEGORY_BUTTON: u16 = 3;
pub const SET_DEFAULT: u16 = 4;
pub const PRESS_OK: u16 = 5;
pub const PRESS_CANCEL: u16 = 6;
pub const UI_CANCEL: u16 = 7;
pub const LOAD_DEFAULTS: u16 = 8;
pub const EXIT: u16 = 9;

pub const E_OVER: u16 = 1;
pub const E_ENTER: u16 = 2;
pub const E_LEAVE: u16 = 3;
pub const E_CHANGE: u16 = 4;
pub const E_CLICK: u16 = 5;
pub const E_KEY: u16 = 6;
pub const E_UI_BUTTON: u16 = 7;
pub const E_UI_ARROW: u16 = 8;
pub const E_PAD: u16 = 9;

const WHITE: [f32; 4] = [1.0, 1.0, 1.0, 1.0];
const RED: [f32; 4] = [1.0, 0.0, 0.0, 1.0];
const YELLOW: [f32; 4] = [1.0, 1.0, 0.0, 1.0];

struct Entry {
    action: i32,
    label: Wd,
    inputs: [Wd; 2],
    colors: [[f32; 4]; 2],
    subs: [Sub; 2],
    changed: bool,
}

struct Window {
    set: Set,
    window: Wd,
    tip: Wd,
    category: Wd,
    default_keys: Wd,
    groups: Vec<Wd>,
    inputs: Vec<Vec<Handle<Entry>>>,
    waiting: Wd,
    was_waiting: bool,
    invalid: bool,
    cancelled: bool,
    setting_initial: bool,
    header: [f32; 2],
    entry_size: (f32, f32),
    tip_entry: Option<Handle<Entry>>,
    fade: Fade,
}

#[derive(Default)]
struct Store {
    windows: Arena<Window>,
    entries: Arena<Entry>,
    pad_pressed: bool,
}

static STORE: LazyLock<Mutex<Store>> = LazyLock::new(|| Mutex::new(Store::default()));

fn win<R>(me: u64, f: impl FnOnce(&mut Window) -> R) -> Option<R> {
    let h = Handle::from_bits(me)?;
    lock(&STORE).windows.get_mut(h).map(f)
}

fn ent<R>(e: Handle<Entry>, f: impl FnOnce(&mut Entry) -> R) -> Option<R> {
    lock(&STORE).entries.get_mut(e).map(f)
}

fn tok(me: u64, id: u16) -> usize {
    token(Cb {
        kind: Kind::KeyConfig,
        owner: me,
        aux: 0,
        id,
    })
}

fn etok(me: u64, e: Handle<Entry>, id: u16) -> usize {
    token(Cb {
        kind: Kind::Entry,
        owner: me,
        aux: e.to_bits(),
        id,
    })
}

fn p3(x: f32, y: f32, z: f32) -> [f32; 3] {
    [x, y, z]
}

fn add3(a: [f32; 3], b: [f32; 3]) -> [f32; 3] {
    [a[0] + b[0], a[1] + b[1], a[2] + b[2]]
}

const NEG: (f32, f32) = (-1.0, -1.0);

pub fn create(set: usize) -> u64 {
    let w = Window {
        set: Set(set),
        window: Wd(0),
        tip: Wd(0),
        category: Wd(0),
        default_keys: Wd(0),
        groups: Vec::new(),
        inputs: Vec::new(),
        waiting: Wd(0),
        was_waiting: false,
        invalid: false,
        cancelled: false,
        setting_initial: false,
        header: [0.0; 2],
        entry_size: (0.0, 0.0),
        tip_entry: None,
        fade: Fade {
            out: false,
            restore: false,
        },
    };
    lock(&STORE).windows.insert(w).map_or(0, Handle::to_bits)
}

pub fn free(me: u64) {
    let Some(h) = Handle::from_bits(me) else {
        return;
    };
    let mut st = lock(&STORE);
    if let Some(w) = st.windows.remove(h) {
        for e in w.inputs.into_iter().flatten() {
            st.entries.remove(e);
        }
    }
}

fn fade_label(l: Wd, mul: f32) {
    let s = l.font_size();
    l.set_font_size(s.0 * mul, s.1 * mul);
}

struct Action {
    id: i32,
    configurable: bool,
    name: String,
}

fn actions(cat: i32) -> Vec<Action> {
    let mut a = Args::new();
    a.i[0] = cat;
    let n = call(op::ACTIONS, &mut a) as i32;
    (0..n)
        .map(|i| {
            let mut a = Args::new();
            a.i[0] = i;
            call(op::ACTION, &mut a);
            Action {
                id: a.i[0],
                configurable: a.i[1] != 0,
                name: h::narrow(&h::host_list_str(2, i)),
            }
        })
        .collect()
}

fn create_entry(me: u64, set: Set, parent: Wd, pos: [f32; 3], a: &Action) -> Option<Handle<Entry>> {
    let (header, entry_size) = win(me, |w| (w.header, w.entry_size))?;
    let e = lock(&STORE).entries.insert(Entry {
        action: a.id,
        label: Wd(0),
        inputs: [Wd(0); 2],
        colors: [WHITE; 2],
        subs: [Sub::dev(kc::DEV_NONE, -1); 2],
        changed: false,
    })?;
    let bits = e.to_bits() as usize;
    let binds_pad = kc::binds_pad(a.name.as_bytes());

    let label = set.label(pos, NEG, &tr("Actions", &a.name), parent);
    label.on(msg::MOUSE_ENTER, etok(me, e, E_ENTER));
    label.on(msg::MOUSE_LEAVE, etok(me, e, E_LEAVE));
    label.on(msg::GET_NAV_FOCUS, etok(me, e, E_ENTER));
    label.on(msg::LOSE_NAV_FOCUS, etok(me, e, E_LEAVE));
    label.on(msg::TEXT_CHANGE, etok(me, e, E_CHANGE));
    if binds_pad {
        label.on(msg::UI_BUTTON, etok(me, e, E_UI_BUTTON));
    }
    label.on(msg::UI_ARROW, etok(me, e, E_UI_ARROW));
    label.set_user_data(bits);

    let mut inputs = [Wd(0); 2];
    for (i, slot) in inputs.iter_mut().enumerate() {
        let l = set.label(
            p3(header.get(i).copied().unwrap_or(0.0), 0.0, 0.0),
            entry_size,
            &[],
            label,
        );
        l.set_font_color(WHITE);
        l.set_center_align();
        if i == 0 {
            l.on(msg::MOUSE_ENTER, etok(me, e, E_OVER));
            l.on(msg::MOUSE_LEAVE, etok(me, e, E_OVER));
            l.on(msg::MOUSE_ENTER, etok(me, e, E_ENTER));
            l.on(msg::MOUSE_LEAVE, etok(me, e, E_LEAVE));
            l.on(msg::MOUSE_DOWN, etok(me, e, E_CLICK));
        }
        l.on(msg::TEXT_CHANGE, etok(me, e, E_CHANGE));
        l.on(msg::KEY_PRESS, etok(me, e, E_KEY));
        if binds_pad {
            l.on(msg::GAMEPAD, etok(me, e, E_PAD));
        }
        l.set_user_data(bits);
        l.set_user_value(i as i32);
        *slot = l;
    }
    ent(e, |en| {
        en.label = label;
        en.inputs = inputs;
    });
    Some(e)
}

pub fn create_gui(me: u64) -> usize {
    let Some(set) = win(me, |w| {
        w.setting_initial = false;
        w.set
    }) else {
        return 0;
    };
    let window = set.window(
        p3(0.0, 0.0, 5.0),
        (620.0, 460.0),
        &tr("KeyConfig", "KeyConfigTitle"),
    );
    window.on(msg::ON_UPDATE, tok(me, WINDOW_UPDATE));

    let (header_size, border, font_mul) = (70.0f32, 15.0f32, 1.25f32);
    let entry_size = (140.0f32, 16.0f32);
    let entry_sep = 10.0f32;
    let header = [240.0f32, 240.0 + entry_size.0 + entry_sep];
    win(me, |w| {
        w.window = window;
        w.entry_size = entry_size;
        w.header = header;
    });

    let mut pos = p3(border * 2.0, header_size + border, 0.1);
    let group = set.dummy(pos, window);
    let label = set.label(p3(0.0, 0.0, 0.0), NEG, &tr("KeyConfig", "Category"), group);
    fade_label(label, font_mul);
    label.set_autosize(true);

    let category = set.combo(p3(label.size().0 + 10.0, 0.0, 0.0), (175.0, 25.0), group);
    category.combo_add(&tr("KeyConfig", "Movement"));
    category.combo_add(&tr("KeyConfig", "Actions"));
    category.combo_add(&tr("KeyConfig", "Misc"));
    category.on(msg::SELECTION, tok(me, CATEGORY));
    category.on(msg::UI_BUTTON, tok(me, CATEGORY_BUTTON));
    category.set_global_ui(true);

    pos = add3(group.local_pos(), p3(border, group.size().1 + 5.0, 0.0));

    let l = set.label(pos, NEG, &tr("KeyConfig", "HeaderAction"), window);
    fade_label(l, font_mul);
    let l = set.label(
        add3(pos, p3(header[0], 0.0, 0.0)),
        entry_size,
        &tr("KeyConfig", "HeaderPrimary"),
        window,
    );
    fade_label(l, font_mul);
    l.set_center_align();
    let l = set.label(
        add3(pos, p3(header[1], 0.0, 0.0)),
        entry_size,
        &tr("KeyConfig", "HeaderSecondary"),
        window,
    );
    fade_label(l, font_mul);
    l.set_center_align();
    pos[1] += l.size().1 + 10.0;

    let old: Vec<Handle<Entry>> = win(me, |w| std::mem::take(&mut w.inputs))
        .unwrap_or_default()
        .into_iter()
        .flatten()
        .collect();
    {
        let mut st = lock(&STORE);
        for e in old {
            st.entries.remove(e);
        }
    }
    let mut groups = vec![Wd(0); CATEGORIES];
    let mut inputs: Vec<Vec<Handle<Entry>>> = Vec::new();
    for cat in 1..CATEGORIES {
        let group = set.dummy(pos, window);
        let mut in_group = p3(0.0, 0.0, 0.1);
        let mut list: Vec<Handle<Entry>> = Vec::new();
        for a in actions(cat as i32).iter().filter(|a| a.configurable) {
            let Some(e) = create_entry(me, set, group, in_group, a) else {
                continue;
            };
            let first = ent(e, |en| en.inputs[0]).unwrap_or_default();
            list.push(e);
            in_group[1] += first.size().1 + 10.0;
        }
        let labels: Vec<Wd> = list
            .iter()
            .map(|&e| ent(e, |en| en.label).unwrap_or_default())
            .collect();
        for (i, l) in labels.iter().enumerate() {
            if let Some(prev) = i.checked_sub(1).and_then(|p| labels.get(p)) {
                l.set_nav(UP, *prev);
            }
            if let Some(next) = labels.get(i + 1) {
                l.set_nav(DOWN, *next);
            }
        }
        if let Some(g) = groups.get_mut(cat) {
            *g = group;
        }
        inputs.push(list);
    }

    let (bw, sep) = (80.0f32, 3.0f32);
    pos[0] = 30.0;
    pos[1] = window.size().1 - 25.0 - 10.0;

    let tip = set.label(add3(pos, p3(10.0, 0.0, 0.0)), (400.0, 30.0), &[], window);
    tip.set_font_color(WHITE);
    tip.set_word_wrap(true);
    tip.set_clip(true);
    tip.set_scroll_speed(4.0);
    tip.set_draw_bg(true);
    tip.set_bg_color([0.0, 0.0, 0.0, 0.5]);

    let default_keys = set.button(
        add3(pos, p3(10.0, -35.0, 0.0)),
        (bw, 25.0),
        &tr("KeyConfig", "SetDefault"),
        window,
    );
    default_keys.on(msg::BUTTON_PRESSED, tok(me, SET_DEFAULT));
    let width = default_keys.text_length();
    default_keys.set_size(width + 20.0, default_keys.size().1);

    pos[0] = window.size().0 - bw * 2.0 - sep - 5.0;
    let ok = set.button(pos, (bw, 30.0), &tr("Global", "OK"), window);
    ok.on(msg::BUTTON_PRESSED, tok(me, PRESS_OK));
    pos[0] += bw + sep;
    let cancel = set.button(pos, (bw, 30.0), &tr("Global", "Cancel"), window);
    cancel.on(msg::BUTTON_PRESSED, tok(me, PRESS_CANCEL));
    cancel.on(msg::UI_BUTTON, tok(me, UI_CANCEL));
    cancel.set_global_ui(true);

    let label_of =
        |e: Option<&Handle<Entry>>| e.and_then(|&e| ent(e, |en| en.label)).unwrap_or_default();
    category.set_nav(DOWN, label_of(inputs.first().and_then(|l| l.first())));
    for list in &inputs {
        label_of(list.last()).set_nav(DOWN, default_keys);
        label_of(list.first()).set_nav(UP, category);
    }
    default_keys.set_nav(UP, category);
    default_keys.set_nav(DOWN, ok);
    ok.set_nav(UP, default_keys);
    ok.set_nav(RIGHT, cancel);
    cancel.set_nav(UP, default_keys);
    cancel.set_nav(LEFT, ok);

    win(me, |w| {
        w.tip = tip;
        w.category = category;
        w.default_keys = default_keys;
        w.groups = groups;
        w.inputs = inputs;
    });
    window.0
}

pub fn exit_pressed(me: u64) {
    let Some((set, cancelled)) = win(me, |w| (w.set, w.cancelled)) else {
        return;
    };
    match kc::exit_pressed(set.popup_active(), cancelled) {
        kc::Exit::Nothing => {}
        kc::Exit::ClearCancelled => {
            win(me, |w| w.cancelled = false);
        }
        kc::Exit::ToOptions => {
            h::simple(op::SET_WINDOW_ACTIVE, OPTIONS_WINDOW);
        }
    }
}

fn all_entries(me: u64) -> Vec<Handle<Entry>> {
    win(me, |w| w.inputs.iter().flatten().copied().collect()).unwrap_or_default()
}

pub fn on_set_active(me: u64, active: bool) {
    let Some((set, window, category)) = win(me, |w| (w.set, w.window, w.category)) else {
        return;
    };
    if active {
        set.push_attention();
        set.push_focused();
        set.push_default_nav();
        set_initial_values(me);
        category.combo_select(-1, false, true);
        category.combo_select(0, false, true);
    } else if call(op::GAMEPAD_PRESENT, &mut Args::new()) == 0 {
        set.set_draw_mouse(true);
    }
    set.set_attention(if active { window } else { Wd(0) });
}

fn retrieve_default(e: Handle<Entry>) {
    let Some(action) = ent(e, |en| en.action) else {
        return;
    };
    let mut a = Args::new();
    a.i[0] = action;
    let n = call(op::DEFAULT_INPUTS, &mut a) as i32;
    let names: Vec<Vec<u8>> = (0..n)
        .map(|i| h::narrow(&h::host_list_str(3, i)).into_bytes())
        .collect();
    let values: Vec<i32> = (0..n)
        .map(|i| {
            let mut a = Args::new();
            a.i[0] = i;
            call(op::DEFAULT_INPUT, &mut a) as i32
        })
        .collect();
    let inputs: Vec<(&[u8], i32)> = names.iter().map(Vec::as_slice).zip(values).collect();
    ent(e, |en| {
        kc::fill_default(&inputs, &mut en.subs, None);
        en.changed = true;
    });
}

fn retrieve_initial(e: Handle<Entry>) {
    let Some(action) = ent(e, |en| en.action) else {
        return;
    };
    let mut a = Args::new();
    a.i[0] = action;
    let n = call(op::INITIAL_INPUTS, &mut a) as i32;
    let names: Vec<Vec<u8>> = (0..n)
        .map(|i| h::narrow(&h::host_list_str(4, i)).into_bytes())
        .collect();
    ent(e, |en| kc::fill_initial(&names, &mut en.subs));
}

fn update_action(e: Handle<Entry>) {
    let Some((action, ops)) = ent(e, |en| (en.action, kc::update_ops(&en.subs, en.changed))) else {
        return;
    };
    let Some(ops) = ops else { return };
    let mut a = Args::new();
    a.i[0] = action;
    call(op::ACTION_CLEAR, &mut a);
    for o in ops {
        let (kind, x, y) = match o {
            kc::Op::Key(k) => (1, k, 0),
            kc::Op::Mouse(b) => (2, b, 0),
            kc::Op::PadButton(b) => (3, b, 0),
            kc::Op::PadHat(hat, state) => (4, hat, state),
            kc::Op::PadAxis(axis, range) => (5, axis, range),
        };
        let mut a = Args::new();
        a.i = [action, kind, x, y];
        call(op::ACTION_ADD, &mut a);
    }
    ent(e, |en| en.changed = false);
}

fn update_entry(me: u64, e: Handle<Entry>) {
    check_inputs(me);
    let Some((inputs, subs)) = ent(e, |en| (en.inputs, en.subs)) else {
        return;
    };
    for (input, sub) in inputs.iter().zip(subs.iter()) {
        let key = kc::label(sub);
        input.set_text(&tr("ButtonNames", &String::from_utf8_lossy(&key)));
    }
}

fn set_color(e: Handle<Entry>, pos: i32, col: [f32; 4]) {
    let Ok(p) = usize::try_from(pos) else { return };
    let Some(input) = ent(e, |en| {
        if let Some(c) = en.colors.get_mut(p) {
            *c = col;
        }
        en.inputs.get(p).copied()
    })
    .flatten() else {
        return;
    };
    input.set_font_color(col);
}

fn check_inputs(me: u64) {
    let Some((setting, lists)) = win(me, |w| (w.setting_initial, w.inputs.clone())) else {
        return;
    };
    if setting {
        return;
    }
    let counts: Vec<usize> = lists.iter().map(Vec::len).collect();
    let flat: Vec<Handle<Entry>> = lists.iter().flatten().copied().collect();
    let subs: Vec<Sub> = flat
        .iter()
        .flat_map(|&e| ent(e, |en| en.subs).unwrap_or([Sub::EMPTY; 2]))
        .collect();
    let mut sets = Vec::new();
    let invalid = kc::check_inputs(&subs, &counts, |k, pos, red| sets.push((k, pos, red)));
    win(me, |w| w.invalid = invalid);
    for (k, pos, red) in sets {
        if let Some(&e) = flat.get(k) {
            set_color(e, pos, if red { RED } else { WHITE });
        }
    }
}

fn set_initial_values(me: u64) {
    win(me, |w| w.setting_initial = true);
    for e in all_entries(me) {
        retrieve_initial(e);
        update_entry(me, e);
    }
    win(me, |w| w.setting_initial = false);
    check_inputs(me);
}

fn set_default_values(me: u64) {
    win(me, |w| w.setting_initial = true);
    for e in all_entries(me) {
        retrieve_default(e);
        update_entry(me, e);
    }
    win(me, |w| w.setting_initial = false);
    check_inputs(me);
}

fn set_waiting_input(me: u64, input: Wd) {
    let Some((set, current)) = win(me, |w| (w.set, w.waiting)) else {
        return;
    };
    match kc::set_waiting(!input.is_null(), !current.is_null()) {
        kc::Waiting::Ignore => return,
        kc::Waiting::Push => {
            win(me, |w| {
                w.waiting = input;
                w.was_waiting = true;
            });
            set.push_attention();
            set.push_focused();
            set.set_attention(input);
        }
        kc::Waiting::Pop => {
            win(me, |w| {
                w.waiting = input;
                w.was_waiting = true;
            });
            set.pop_attention();
            set.pop_focused();
        }
    }
    set.set_mouse_move(input.is_null());
}

fn set_waiting(me: u64, e: Handle<Entry>, on: bool, pos: i32) {
    let Ok(p) = usize::try_from(pos) else { return };
    let Some(input) = ent(e, |en| en.inputs.get(p).copied()).flatten() else {
        return;
    };
    let waiting = if on { input } else { Wd(0) };
    set_waiting_input(me, waiting);
    if on {
        waiting.set_text(&h::w("..."));
    }
}

fn is_waiting(me: u64) -> bool {
    win(me, |w| !w.waiting.is_null()).unwrap_or(false)
}

fn save_sub(me: u64, e: Handle<Entry>, pos: i32, bind: Option<Bind>) {
    if pos >= kc::POS_LAST {
        return;
    }
    if let Some(b) = bind {
        ent(e, |en| kc::set(&mut en.subs, pos, b));
    }
    update_entry(me, e);
    set_waiting(me, e, false, pos);
}

fn set_tip_entry(me: u64, e: Option<Handle<Entry>>) {
    let Some((current, tip)) = win(me, |w| (w.tip_entry, w.tip)) else {
        return;
    };
    if current == e {
        return;
    }
    win(me, |w| w.tip_entry = e);
    let mut text = h::S::new();
    if let Some(e) = e {
        let id = ent(e, |en| en.action).unwrap_or(-1);
        let mut name = h::narrow(&h::string_op(op::ACTION_NAME, id));
        name.push_str("Tooltip");
        text = tr("Actions", &name);
        tip.set_text(&text);
    }
    let (out, restore) = kc::tip_flags(text.is_empty());
    win(me, |w| w.fade = Fade { out, restore });
}

fn entry_callback(me: u64, e: Handle<Entry>, id: u16, w: Wd, d: &CbData) -> bool {
    let Some(set) = win(me, |x| x.set) else {
        return true;
    };
    let pos = if w.is_null() {
        kc::POS_LAST
    } else {
        w.user_value()
    };
    match id {
        E_OVER => {
            if set.popup_active() {
                return true;
            }
            let normal = usize::try_from(pos)
                .ok()
                .and_then(|p| ent(e, |en| en.colors.get(p).copied()).flatten())
                .unwrap_or(WHITE);
            w.set_font_color(if w.mouse_over() { YELLOW } else { normal });
        }
        E_ENTER => set_tip_entry(me, Some(e)),
        E_LEAVE => {
            if win(me, |x| x.tip_entry) == Some(Some(e)) {
                set_tip_entry(me, None);
            }
        }
        E_CHANGE => {
            ent(e, |en| en.changed = true);
        }
        E_CLICK => match kc::click(!is_waiting(me), d.lval == h::MOUSE_LEFT) {
            kc::Click::Nothing => {}
            kc::Click::StartWaiting => set_waiting(me, e, true, pos),
            kc::Click::SaveMouse => {
                let mut a = Args::new();
                a.i[0] = d.lval;
                let button = call(op::GUI_MOUSE, &mut a) as i32;
                save_sub(me, e, pos, Some(Bind::Mouse(button)));
            }
        },
        E_KEY => match kc::key_press(is_waiting(me), d.key, h::KEY_ESCAPE) {
            kc::KeyAct::None => {}
            kc::KeyAct::Save => save_sub(me, e, pos, Some(Bind::Key(d.key))),
            kc::KeyAct::Cancel => {
                update_entry(me, e);
                set_waiting(me, e, false, pos);
                win(me, |x| x.cancelled = true);
            }
        },
        E_UI_ARROW => {
            let flag = lock(&STORE).pad_pressed;
            let p = kc::ui_arrow(flag);
            if p.clear_flag {
                lock(&STORE).pad_pressed = false;
            }
            return p.handled;
        }
        E_UI_BUTTON => {
            let flag = lock(&STORE).pad_pressed;
            let p = kc::ui_button(d.lval == h::UI_PRIMARY, flag, !is_waiting(me));
            if p.clear_flag {
                lock(&STORE).pad_pressed = false;
            }
            if p.start_waiting {
                set_waiting(me, e, true, POS_SECONDARY);
            }
            return p.handled;
        }
        E_PAD => {
            let act = kc::pad_press(
                is_waiting(me),
                d.pad_value,
                d.pad_type == kc::GP_BUTTON,
                d.pad_id,
                h::PAD_B,
            );
            match act {
                kc::PadAct::Eat => {}
                kc::PadAct::Pass => return false,
                kc::PadAct::Cancel => {
                    update_entry(me, e);
                    set_waiting(me, e, false, pos);
                    win(me, |x| x.cancelled = true);
                }
                kc::PadAct::Save => {
                    lock(&STORE).pad_pressed = true;
                    let bind = match d.pad_type {
                        kc::GP_BUTTON => Some(Bind::PadButton(d.pad_id)),
                        kc::GP_HAT => Some(Bind::PadHat(d.pad_id, d.pad_value as i32)),
                        kc::GP_AXIS => {
                            let mut a = Args::new();
                            a.f[0] = d.pad_value;
                            Some(Bind::PadAxis(d.pad_id, call(op::AXIS_RANGE, &mut a) as i32))
                        }
                        _ => None,
                    };
                    save_sub(me, e, pos, bind);
                }
            }
        }
        _ => {}
    }
    true
}

fn exit_callback(me: u64, lval: i32) {
    if lval == 0 {
        for e in all_entries(me) {
            update_action(e);
        }
        h::simple(op::SET_WINDOW_ACTIVE, OPTIONS_WINDOW);
    }
}

fn window_update(me: u64, dt: f32) {
    let Some((set, tip, waiting, fade)) = win(me, |w| (w.set, w.tip, w.waiting, w.fade)) else {
        return;
    };
    if waiting.is_null() {
        win(me, |w| w.was_waiting = false);
    }
    if set.popup_active() {
        return;
    }
    let mut col = tip.font_color();
    let (fade, alphas) = kc::tip_fade(col[3], fade, dt);
    win(me, |w| w.fade = fade);
    for a in alphas {
        col[3] = a;
        tip.set_font_color(col);
    }
}

fn category_change(me: u64, selected: i32) {
    let Some((set, groups, inputs, category, default_keys)) = win(me, |w| {
        (
            w.set,
            w.groups.clone(),
            w.inputs.clone(),
            w.category,
            w.default_keys,
        )
    }) else {
        return;
    };
    for i in 1..CATEGORIES {
        let active = kc::category_active(selected, i as i32);
        let Some(g) = groups.get(i) else { continue };
        g.set_visible(active);
        g.set_enabled(active);
        if active {
            let list = inputs.get(i - 1);
            let label = |e: Option<&Handle<Entry>>| {
                e.and_then(|&e| ent(e, |en| en.label)).unwrap_or_default()
            };
            let focus = label(list.and_then(|l| l.first()));
            let tail = label(list.and_then(|l| l.last()));
            set.set_default_nav(focus);
            set.set_focused(focus);
            category.set_nav(DOWN, focus);
            default_keys.set_nav(UP, tail);
        }
    }
}

pub fn callback(cb: &Cb, w: Wd, d: &CbData) -> bool {
    let me = cb.owner;
    if cb.kind == Kind::Entry {
        let Some(e) = Handle::from_bits(cb.aux) else {
            return true;
        };
        return entry_callback(me, e, cb.id, w, d);
    }
    let Some((set, category, was_waiting, invalid)) =
        win(me, |x| (x.set, x.category, x.was_waiting, x.invalid))
    else {
        return true;
    };
    let msgbox = |text: &str, id: u16| {
        set.msgbox(
            [
                &tr("Global", "Warning"),
                &tr("KeyConfig", text),
                &tr("Global", "OK"),
                &tr("Global", "Cancel"),
            ],
            tok(me, id),
        );
    };
    match cb.id {
        WINDOW_UPDATE => window_update(me, d.fval),
        CATEGORY => category_change(me, d.lval),
        CATEGORY_BUTTON => {
            let pick = kc::category_button(
                was_waiting,
                d.lval,
                h::UI_PREV_PAGE,
                h::UI_NEXT_PAGE,
                category.combo_selected(),
                category.combo_num(),
            );
            let Some(c) = pick else { return false };
            category.combo_select(c, false, true);
        }
        SET_DEFAULT => msgbox("LoadDefaultsMessage", LOAD_DEFAULTS),
        LOAD_DEFAULTS => {
            if d.lval == 0 {
                set_default_values(me);
            }
        }
        PRESS_OK => {
            if invalid {
                msgbox("InvalidInputMessage", EXIT);
            } else {
                exit_callback(me, 0);
            }
        }
        EXIT => exit_callback(me, d.lval),
        PRESS_CANCEL => {
            h::simple(op::SET_WINDOW_ACTIVE, OPTIONS_WINDOW);
        }
        UI_CANCEL => {
            if !kc::ui_cancel(was_waiting, d.lval == h::UI_SECONDARY) {
                return false;
            }
            h::simple(op::SET_WINDOW_ACTIVE, OPTIONS_WINDOW);
        }
        _ => {}
    }
    true
}
