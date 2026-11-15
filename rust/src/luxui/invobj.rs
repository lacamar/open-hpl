//! The inventory object (A11a): items, combine callbacks, texts, picked and equipped item, session state and the
//! lifecycle the C++ shell forwards. The GUI half is `invgui.rs`. State is reached through short [`g`] borrows that
//! never span a host call (host ops can re-enter through the widget callbacks).

use super::event::{Ev, Out};
use super::inventory::{self as inv, InvState};
use super::invgui::{Gui, Layout};
use super::invhost::{self as host, game, op};
use super::itemtype::{self, Info};
use crate::luxscript::types::AlphaFader;
use crate::obj::{Arena, Handle};
use std::cell::UnsafeCell;

pub const SLOT_NONE: usize = 0;

pub struct Item {
    pub name: Vec<u8>,
    pub ty: i32,
    pub sub: Vec<u8>,
    pub val: Vec<u8>,
    pub extra: Vec<u8>,
    pub image: usize,
    pub image_name: Vec<u8>,
    pub count: i32,
    pub amount: f32,
    pub name_entry: Vec<u8>,
    pub desc_entry: Vec<u8>,
}

#[derive(Clone)]
pub struct Combine {
    pub name: Vec<u8>,
    pub a: Vec<u8>,
    pub b: Vec<u8>,
    pub func: Vec<u8>,
    pub auto_destroy: bool,
}

pub type ItemRef = Handle<Item>;

pub struct Inv {
    pub st: InvState,
    pub disabled: bool,
    pub items: Arena<Item>,
    pub order: Vec<ItemRef>,
    pub combine: Vec<Combine>,
    pub picked: Option<ItemRef>,
    pub picked_offset: [f32; 2],
    pub equipped: Option<ItemRef>,
    pub current_widget: usize,
    pub last_current_widget: usize,
    pub header: Vec<u32>,
    pub normal: Vec<u32>,
    pub old_header: Vec<u32>,
    pub old_normal: Vec<u32>,
    pub message: Vec<u32>,
    pub faders: [AlphaFader; 5],
    pub script: usize,
    pub show_journal_on_pickup: bool,
    pub gui: Gui,
    pub layout: Layout,
}

struct Global(UnsafeCell<Option<Box<Inv>>>);

// SAFETY: the game logic runs on one thread.
unsafe impl Sync for Global {}

static INV: Global = Global(UnsafeCell::new(None));

/// A short borrow of the inventory; never keep it across a host call.
pub fn g() -> Option<&'static mut Inv> {
    // SAFETY: single game thread; callers keep the borrow within one statement block without host calls.
    unsafe { (*INV.0.get()).as_deref_mut() }
}

pub fn install(i: Box<Inv>) {
    // SAFETY: single game thread, no live borrow.
    unsafe { *INV.0.get() = Some(i) };
}

pub fn take() -> Option<Box<Inv>> {
    // SAFETY: single game thread, no live borrow.
    unsafe { (*INV.0.get()).take() }
}

pub fn bits(h: Option<ItemRef>) -> usize {
    h.map_or(0, |h| h.to_bits() as usize)
}

pub fn from_bits(b: usize) -> Option<ItemRef> {
    ItemRef::from_bits(b as u64)
}

impl Inv {
    pub fn blank() -> Inv {
        Inv {
            st: InvState::default(),
            disabled: false,
            items: Arena::default(),
            order: Vec::new(),
            combine: Vec::new(),
            picked: None,
            picked_offset: [0.0; 2],
            equipped: None,
            current_widget: 0,
            last_current_widget: 0,
            header: Vec::new(),
            normal: Vec::new(),
            old_header: Vec::new(),
            old_normal: Vec::new(),
            message: Vec::new(),
            faders: std::array::from_fn(|_| AlphaFader {
                alpha: 0.0,
                active: false,
                fade_in: 1.0,
                fade_out: 1.0,
            }),
            script: 0,
            show_journal_on_pickup: true,
            gui: Gui::default(),
            layout: Layout::default(),
        }
    }

    pub fn item(&self, h: ItemRef) -> Option<&Item> {
        self.items.get(h)
    }

    pub fn index_of(&self, h: Option<ItemRef>) -> i32 {
        h.and_then(|h| self.order.iter().position(|&x| x == h))
            .map_or(-1, |i| i as i32)
    }

    pub fn find(&self, pred: impl Fn(&Item) -> bool) -> Option<ItemRef> {
        self.order
            .iter()
            .copied()
            .find(|&h| self.items.get(h).is_some_and(&pred))
    }
}

fn w(s: &str) -> Vec<u32> {
    s.chars().map(|c| c as u32).collect()
}

pub fn info_of(t: i32) -> Option<&'static Info> {
    itemtype::type_info(t)
}

pub fn has_count(t: i32) -> bool {
    info_of(t).is_some_and(|i| i.has_count)
}

// ---- items ----------------------------------------------------------------------------------

fn destroy_item(h: ItemRef) {
    let img = g().and_then(|i| i.items.remove(h)).map_or(0, |it| it.image);
    if img != 0 {
        host::gfx_destroy(img);
    }
}

fn delete_all_items() {
    let order = g()
        .map(|i| std::mem::take(&mut i.order))
        .unwrap_or_default();
    for h in order {
        destroy_item(h);
    }
}

pub fn get_item_by_name(name: &[u8]) -> Option<ItemRef> {
    g()?.find(|it| it.name == name)
}

pub fn get_item_from_subtype(sub: &[u8]) -> Option<ItemRef> {
    g()?.find(|it| it.sub == sub)
}

pub fn has_item_of_type(t: i32) -> bool {
    g().is_some_and(|i| i.find(|it| it.ty == t).is_some())
}

pub fn item_type_from_string(name: &[u8]) -> i32 {
    match itemtype::from_string(name) {
        Some(t) => t,
        None => {
            let mut m = b"Item type '".to_vec();
            m.extend_from_slice(name);
            m.extend_from_slice(b"' does not exist!\n");
            host::log(0, &m);
            itemtype::PUZZLE
        }
    }
}

pub fn player_info(t: i32) -> itemtype::Player {
    let mut p = itemtype::Player {
        coins: 0,
        health: 0.0,
        sanity: 0.0,
        lamp_oil: 0.0,
        has_lantern: false,
        lantern_active: false,
        use_item_state: false,
    };
    match t {
        itemtype::PUZZLE => p.use_item_state = host::game(game::STATE_USE_ITEM).r != 0,
        itemtype::COINS => p.coins = host::game(game::COINS).r as i32,
        itemtype::LANTERN => p.lantern_active = host::game(game::LANTERN_ACTIVE).r != 0,
        itemtype::HEALTH => p.health = host::game(game::HEALTH).f[0],
        itemtype::SANITY => p.sanity = host::game(game::SANITY).f[0],
        itemtype::LAMP_OIL => {
            p.has_lantern = has_item_of_type(itemtype::LANTERN);
            if p.has_lantern {
                p.lamp_oil = host::game(game::LAMP_OIL).f[0];
            }
        }
        _ => {}
    }
    p
}

struct Snap {
    name: Vec<u8>,
    sval: Vec<u8>,
    extra: Vec<u8>,
    amount: f32,
    image_name: Vec<u8>,
}

fn snap(h: ItemRef) -> Option<Snap> {
    let i = g()?;
    let it = i.items.get(h)?;
    Some(Snap {
        name: it.name.clone(),
        sval: it.val.clone(),
        extra: it.extra.clone(),
        amount: it.amount,
        image_name: it.image_name.clone(),
    })
}

fn item_view(s: &Snap) -> itemtype::Item<'_> {
    itemtype::Item {
        name: &s.name,
        sval: &s.sval,
        extra: &s.extra,
        amount: s.amount,
    }
}

pub fn run_events(o: &Out, item: Option<ItemRef>, note: usize, diary: usize) {
    for e in &o.evs {
        run_event(e, item, note, diary);
    }
}

fn s0(e: &Ev) -> &[u8] {
    if e.s0.p.is_null() || e.s0.n == 0 {
        return &[];
    }
    // SAFETY: the event strings point into the live `Out`.
    unsafe { std::slice::from_raw_parts(e.s0.p, e.s0.n) }
}

fn host_event(kind: i32, e: &Ev, item: Option<ItemRef>, note: usize, diary: usize) {
    let mut a = host::A::new();
    a.i[0] = kind;
    a.i[1] = e.a;
    a.f[0] = e.v[0];
    a.s[0] = e.s0;
    a.p[0] = bits(item);
    a.p[1] = note;
    a.p[2] = diary;
    host::call(op::ITEM_EVENT, &mut a);
}

fn run_event(e: &Ev, item: Option<ItemRef>, note: usize, diary: usize) {
    use host::ev;
    use itemtype as it;
    match e.kind {
        it::EV_PROG_LOG => host_event(ev::PROG_LOG, e, item, note, diary),
        it::EV_COMPLETION => host_event(ev::COMPLETION, e, item, note, diary),
        it::EV_SET_COINS => host_event(ev::SET_COINS, e, item, note, diary),
        it::EV_HINT => host_event(ev::HINT, e, item, note, diary),
        it::EV_SET_CONTAINER_JOURNAL => host_event(ev::SET_CONTAINER_JOURNAL, e, item, note, diary),
        it::EV_FORCE_INSTANT_EXIT => host_event(ev::FORCE_INSTANT_EXIT, e, item, note, diary),
        it::EV_OPEN_NOTE => host_event(ev::OPEN_NOTE, e, item, note, diary),
        it::EV_RUN_MAP_SCRIPT => host_event(ev::RUN_MAP_SCRIPT, e, item, note, diary),
        it::EV_OPEN_DIARY => host_event(ev::OPEN_DIARY, e, item, note, diary),
        it::EV_DIARY_LAST_READ => host_event(ev::DIARY_LAST_READ, e, item, note, diary),
        it::EV_LANTERN_TOGGLE => host_event(ev::LANTERN_TOGGLE, e, item, note, diary),
        it::EV_SET_HEALTH => host_event(ev::SET_HEALTH, e, item, note, diary),
        it::EV_ADD_SANITY => host_event(ev::ADD_SANITY, e, item, note, diary),
        it::EV_SET_LAMP_OIL => host_event(ev::SET_LAMP_OIL, e, item, note, diary),
        it::EV_SOUND => host_event(ev::SOUND, e, item, note, diary),
        it::EV_USE_ITEM_STATE_RESET => host_event(ev::USE_ITEM_STATE_RESET, e, item, note, diary),
        it::EV_USE_ITEM_ENTER => host_event(ev::USE_ITEM_ENTER, e, item, note, diary),
        it::EV_ADD_TINDERBOX => host_event(ev::ADD_TINDERBOX, e, item, note, diary),
        it::EV_REMOVE_ITEM => {
            if let Some(h) = item {
                remove_item(h);
            }
        }
        it::EV_MESSAGE_KEY => {
            let t = host::translate(b"Inventory", s0(e));
            set_message_text(&t, 0.0);
        }
        it::EV_EXIT_PRESSED => exit_pressed(),
        it::EV_EQUIP => {
            if let Some(i) = g() {
                i.equipped = item;
            }
        }
        _ => {}
    }
}

/// `BeforeAddItem` of the item's type; true when the item is consumed instead of added.
fn before_add(h: ItemRef, ty: i32) -> bool {
    let Some(s) = snap(h) else { return false };
    let mut o = Out::default();
    match ty {
        itemtype::NOTE => {
            let mut a = host::A::new();
            a.i[0] = 0;
            a.s[0] = super::event::Str::of(&s.sval);
            a.s[1] = super::event::Str::of(&s.image_name);
            host::call(op::JOURNAL, &mut a);
            let note = a.r;
            itemtype::note_before_add(note == 0, &item_view(&s), &mut o);
            run_events(&o, Some(h), note, 0);
            true
        }
        itemtype::DIARY => {
            let mut a = host::A::new();
            a.i[0] = 1;
            a.s[0] = super::event::Str::of(&s.sval);
            a.s[1] = super::event::Str::of(&s.image_name);
            host::call(op::JOURNAL, &mut a);
            let (diary, idx) = (a.r, a.i[1]);
            if !itemtype::diary_before_add(diary == 0, idx, &item_view(&s), &mut o) {
                return true;
            }
            if let Some(i) = g() {
                i.show_journal_on_pickup = true;
            }
            run_events(&o, Some(h), 0, diary);
            let mut o2 = Out::default();
            let show = g().is_some_and(|i| i.show_journal_on_pickup);
            itemtype::diary_after_script(show, &mut o2);
            run_events(&o2, Some(h), 0, diary);
            true
        }
        itemtype::HAND_OBJECT => {
            if g().is_some_and(|i| i.equipped.is_none()) {
                set_equipped(Some(h));
            }
            false
        }
        _ => {
            let p = player_info(ty);
            let consumed = itemtype::before_add(ty, &item_view(&s), &p, &mut o);
            run_events(&o, Some(h), 0, 0);
            consumed
        }
    }
}

pub struct NewItem<'a> {
    pub name: &'a [u8],
    pub ty: i32,
    pub sub: &'a [u8],
    pub image: &'a [u8],
    pub amount: f32,
    pub val: &'a [u8],
    pub extra: &'a [u8],
}

pub fn add_item(n: &NewItem, remove_prop: &mut i32) -> Option<ItemRef> {
    let NewItem {
        name,
        ty,
        sub,
        image,
        amount,
        val,
        extra,
    } = *n;
    *remove_prop = 0;
    g()?;

    if ty == itemtype::TINDERBOX {
        let cur = host::game_i(game::TINDER_GET, 0);
        if cur.i[1] == 0 {
            let mut m = b"Couldn't find Global var '".to_vec();
            m.extend_from_slice(name);
            m.extend_from_slice(b"'\n");
            host::log(0, &m);
        } else {
            let (new, unlock) = inv::tinderbox_count(&cur.ret_bytes());
            host::game_s(game::TINDER_SET, &new);
            if unlock {
                host::game_i(game::ACHIEVEMENT, 0);
            }
        }
    }

    let existing = if !itemtype::has_max_amount(ty) && has_count(ty) {
        get_item_from_subtype(sub)
    } else {
        None
    };
    let existing_count = existing
        .and_then(|h| g().and_then(|i| i.items.get(h).map(|it| it.count)))
        .unwrap_or(0);
    match inv::add_plan(&inv::AddIn {
        ty,
        subtype: sub,
        existing: existing.is_some(),
        existing_count,
    }) {
        inv::ADD_REJECT => {
            let t = host::translate(b"Inventory", b"TooManyItemsOfSort");
            host::game_w(game::SET_MESSAGE, &t);
            return None;
        }
        inv::ADD_COUNT => {
            let h = existing?;
            if let Some(it) = g().and_then(|i| i.items.get_mut(h)) {
                it.count += 1;
            }
            *remove_prop = 1;
            return existing;
        }
        _ => {}
    }

    let full = inv::full_image_name(image);
    let h = g()?.items.insert(Item {
        name: name.to_vec(),
        ty,
        sub: sub.to_vec(),
        val: val.to_vec(),
        extra: extra.to_vec(),
        image: 0,
        image_name: full.clone(),
        count: 1,
        amount,
        name_entry: inv::game_name_entry(sub),
        desc_entry: inv::game_desc_entry(sub),
    })?;

    if inv::is_still_alive_item(val) {
        host::game_i(game::ACHIEVEMENT, 1);
    }

    if before_add(h, ty) {
        *remove_prop = 1;
        destroy_item(h);
        return None;
    }

    let gfx = host::gfx_image(&full, 0);
    if gfx == 0 {
        let mut m = b"Could not create image '".to_vec();
        m.extend_from_slice(image);
        m.extend_from_slice(b"'! Not adding item '");
        m.extend_from_slice(name);
        m.extend_from_slice(b"'!\n");
        host::log(0, &m);
        destroy_item(h);
        return None;
    }
    if let Some(it) = g().and_then(|i| i.items.get_mut(h)) {
        it.image = gfx;
    }
    *remove_prop = 1;
    g()?.order.push(h);
    Some(h)
}

pub fn remove_item_by_name(name: &[u8]) {
    let found = g().and_then(|i| i.find(|it| it.name == name));
    let Some(h) = found else {
        let mut m = b"Item '".to_vec();
        m.extend_from_slice(name);
        m.extend_from_slice(b"' could not be found when trying to remove it!\n");
        host::log(1, &m);
        return;
    };
    if let Some(i) = g() {
        if i.picked == Some(h) {
            i.picked = None;
        }
        if i.equipped == Some(h) {
            i.equipped = None;
        }
        i.order.retain(|&x| x != h);
    }
    destroy_item(h);
}

pub fn remove_item(h: ItemRef) {
    let Some(i) = g() else { return };
    let Some(ty) = i.items.get(h).map(|it| it.ty) else {
        return;
    };
    if has_count(ty) {
        let after = i.items.get_mut(h).map_or(0, |it| {
            it.count -= 1;
            it.count
        });
        if !inv::remove_one(true, after) {
            return;
        }
    }
    if i.picked == Some(h) {
        i.picked = None;
    }
    if i.equipped == Some(h) {
        i.equipped = None;
    }
    if let Some(pos) = i.order.iter().position(|&x| x == h) {
        i.order.remove(pos);
        destroy_item(h);
    }
}

pub fn remove_item_of_type(t: i32) {
    let found = g().and_then(|i| i.find(|it| it.ty == t));
    if let Some(h) = found {
        remove_item(h);
    }
}

pub fn set_equipped(h: Option<ItemRef>) {
    if let Some(i) = g() {
        if i.equipped == h {
            return;
        }
        i.equipped = h;
    }
}

// ---- combine callbacks ----------------------------------------------------------------------

pub fn add_combine(name: &[u8], a: &[u8], b: &[u8], func: &[u8], auto_destroy: bool) {
    if let Some(i) = g() {
        i.combine.push(Combine {
            name: name.to_vec(),
            a: a.to_vec(),
            b: b.to_vec(),
            func: func.to_vec(),
            auto_destroy,
        });
    }
}

/// The original removal loop stops at `begin()` and never erases: only the warning runs.
pub fn remove_combine(name: &[u8]) {
    let mut m = b"Inventory combination callback '".to_vec();
    m.extend_from_slice(name);
    m.extend_from_slice(b"' could not be found!\n");
    host::log(1, &m);
}

pub fn find_combine(a: &[u8], b: &[u8]) -> Option<usize> {
    g()?.combine
        .iter()
        .position(|c| inv::combine_matches(&c.a, &c.b, a, b))
}

// ---- script ---------------------------------------------------------------------------------

fn script_op(sub: i32, p: usize, text: &[u8]) -> host::A {
    let mut a = host::A::new();
    a.i[0] = sub;
    a.p[0] = p;
    a.s[0] = super::event::Str::of(text);
    host::call(op::SCRIPT, &mut a);
    a
}

fn destroy_script() {
    let s = g().map_or(0, |i| std::mem::take(&mut i.script));
    if s != 0 {
        script_op(1, s, b"");
    }
}

pub fn load_script() {
    destroy_script();
    let folder = host::game(game::MAP_FOLDER).ret_bytes();
    let mut file = folder;
    file.extend_from_slice(b"inventory.hps");
    let a = script_op(0, 0, &file);
    if let Some(i) = g() {
        i.script = a.r;
    }
    if a.r == 0 {
        let mut m = b"Inventory script '".to_vec();
        m.extend_from_slice(&file);
        m.extend_from_slice(b"' not found!\n");
        host::log(0, &m);
    }
}

pub fn run_script(cmd: &[u8]) {
    let s = g().map_or(0, |i| i.script);
    if s != 0 {
        script_op(2, s, cmd);
    }
}

pub fn recompile_script() -> (bool, Vec<u8>) {
    destroy_script();
    let folder = host::game(game::MAP_FOLDER).ret_bytes();
    let mut file = folder;
    file.extend_from_slice(b"inventory.hps");
    let a = script_op(3, 0, &file);
    if let Some(i) = g() {
        i.script = a.r;
    }
    (a.r != 0, a.ret_bytes())
}

// ---- texts ----------------------------------------------------------------------------------

fn strip_gamepad_icons(text: &[u32]) -> Vec<u32> {
    let mut out = Vec::new();
    let mut cmd: Vec<u32> = Vec::new();
    let mut parse = false;
    for &c in text {
        if parse {
            if c != u32::from(b'$') {
                cmd.push(c);
            } else {
                out.extend(gamepad_text(&cmd));
                parse = false;
            }
        } else if c == u32::from(b'$') {
            parse = true;
            cmd.clear();
        } else {
            out.push(c);
        }
    }
    out
}

fn gamepad_text(cmd: &[u32]) -> Vec<u32> {
    let narrow: Vec<u8> = cmd.iter().map(|&c| c as u8).collect();
    let parts: Vec<&[u8]> = crate::string::split(&narrow, Some(b"."), false).collect();
    let part = |i: usize| parts.get(i).copied().unwrap_or(&[]);
    if part(0) == b"GamepadButton" {
        host::translate(b"ButtonNames", part(1))
    } else {
        let mut k = part(1).to_vec();
        k.push(b'.');
        k.extend_from_slice(part(2));
        host::translate(b"ButtonNames", &k)
    }
}

pub fn set_desc_text(header: &[u32], normal: &[u32]) {
    let save_old =
        g().is_some_and(|i| inv::set_desc_text(&mut i.st, header.is_empty(), normal.is_empty()));
    if save_old && let Some(i) = g() {
        i.old_header = std::mem::take(&mut i.header);
        i.old_normal = std::mem::take(&mut i.normal);
    }
    let h = strip_gamepad_icons(&host::parse_string(header));
    let n = strip_gamepad_icons(&host::parse_string(normal));
    if let Some(i) = g() {
        i.header = h;
        i.normal = n;
    }
}

pub fn set_desc_text_from_item(h: Option<ItemRef>) {
    let Some(h) = h else {
        set_desc_text(&[], &[]);
        return;
    };
    let Some((ty, ne, de)) = g().and_then(|i| {
        i.items
            .get(h)
            .map(|it| (it.ty, it.name_entry.clone(), it.desc_entry.clone()))
    }) else {
        return;
    };
    let normal = host::translate(b"Inventory", &de);
    let mut header = host::translate(b"Inventory", &ne);
    if ty == itemtype::LANTERN {
        let active = host::game(game::LANTERN_ACTIVE).r != 0;
        let key = itemtype::lantern_name_key(active);
        header.extend(w(" ("));
        header.extend(host::translate(b"Inventory", key));
        header.extend(w(")"));
    }
    set_desc_text(&header, &normal);
}

pub fn set_message_text(text: &[u32], life: f32) {
    let parsed = host::parse_string(text);
    let dur = if life <= 0.0 {
        host::string_duration(&parsed)
    } else {
        0.0
    };
    if let Some(i) = g() {
        i.message = parsed;
        inv::set_message(&mut i.st, life, dur);
    }
}

pub fn now() -> f64 {
    host::game(game::GAME_TIME).d
}

pub fn set_picked(h: Option<ItemRef>, offset: [f32; 2]) {
    let t = now();
    if let Some(i) = g() {
        inv::set_picked(&mut i.st, t);
        i.picked = h;
        i.picked_offset = offset;
    }
}

pub fn set_current_widget(wd: usize) {
    let t = now();
    if let Some(i) = g() {
        inv::note_current_widget(&mut i.st, t);
        i.current_widget = wd;
    }
    host::set_widget(host::set::FOCUSED_WIDGET, wd);
}

pub fn exit_pressed() {
    let r = g().map_or(inv::PRESS_NONE, |i| inv::exit_pressed(&mut i.st));
    if r == inv::PRESS_EXIT {
        leave_input();
    }
}

pub fn open_journal() {
    let r = g().map_or(inv::PRESS_NONE, |i| inv::open_journal(&mut i.st));
    if r == inv::PRESS_EXIT {
        leave_input();
    }
}

fn leave_input() {
    host::game_i(game::INPUT_STATE, 1);
    host::set_flag(host::set::DRAW_MOUSE, 0);
    host::set_flag(host::set::GUI_FOCUS, 0);
}

fn reset_session_vars() {
    if let Some(i) = g() {
        inv::reset_session(&mut i.st);
        i.header.clear();
        i.normal.clear();
        i.picked = None;
        i.current_widget = 0;
        i.message.clear();
    }
}

// ---- lifecycle ------------------------------------------------------------------------------

pub fn reset() {
    if let Some(i) = g() {
        inv::reset(&mut i.st);
        i.disabled = false;
        i.equipped = None;
    }
    reset_session_vars();
    destroy_script();
    delete_all_items();
    if let Some(i) = g() {
        i.combine.clear();
    }
}

pub fn on_game_start() {
    load_script();
    run_script(b"OnGameStart()");
}

pub fn update(dt: f32) {
    let left = host::game_i(game::TRIGGERED, 0).r != 0;
    let primary = host::game_i(game::TRIGGERED, 1).r != 0;
    if inv::should_drop_picked(left, primary)
        && let Some(i) = g()
    {
        i.picked = None;
    }

    let exit = g().map_or(inv::EXIT_NONE, |i| inv::update_alpha(&mut i.st, dt));
    match exit {
        inv::EXIT_TO_JOURNAL => {
            host::game_i(game::OPENED_FROM_INVENTORY, 1);
            host::game_s(game::SET_CONTAINER, b"Journal");
        }
        inv::EXIT_TO_DEFAULT => {
            host::game_s(game::SET_CONTAINER, b"Default");
            host::game_i(game::INPUT_STATE, 0);
        }
        _ => {}
    }

    if let Some(i) = g() {
        for f in &mut i.faders {
            f.update(dt);
        }
        inv::update_texts(&mut i.st, dt);
    }

    let (widgets, alpha) = g().map_or((Vec::new(), 0.0), |i| {
        (i.gui.image_widgets.clone(), i.st.alpha)
    });
    for wd in widgets {
        let mut c = host::widget_get(wd, host::wd::GET_COLOR_MUL).f;
        c[3] = alpha;
        host::widget_set(wd, host::wd::SET_COLOR_MUL, &c[..4], 0, 0);
    }

    host::game_f(game::HINT_UPDATE, dt);
    host::game_f(game::FLASH_UPDATE, dt);
}

pub fn on_enter_container() {
    if host::game(game::STATE_USE_ITEM).r != 0 {
        host::game(game::LEAVE_USE_ITEM);
    }
    super::invgui::create_background();
    if let Some(i) = g() {
        i.st.active = 1;
    }
    host::game_i(game::INPUT_STATE, 2);
    host::set_flag(host::set::VIEWPORT, 1);
    host::set_flag(host::set::ACTIVE, 1);

    let pad = host::set_query(host::set::GAMEPAD_PRESENT).r != 0;
    host::set_flag(host::set::DRAW_MOUSE, i32::from(!pad));
    host::set_flag(host::set::MOUSE_MOVEMENT, i32::from(!pad));
    host::set_flag(host::set::DRAW_FOCUS, i32::from(pad));

    host::set_flag(host::set::RESET_MOUSE_OVER, 0);
    host::set_flag(host::set::GUI_FOCUS, 1);
    host::game(game::RESUME_SOUNDS);

    if let Some(i) = g() {
        for f in &mut i.faders {
            f.reset();
        }
    }

    host::game(game::FADE_LOOPS);
    host::game(game::STOP_TERROR);

    if g().is_some_and(|i| inv::pauses_voices_on_enter(&i.st)) {
        host::game_i(game::VOICES, 1);
    }

    reset_session_vars();

    let (last, health) = g().map_or((0, 0), |i| (i.last_current_widget, i.gui.health_widget));
    let cur = if last != 0 { last } else { health };
    if let Some(i) = g() {
        i.last_current_widget = cur;
    }
    set_current_widget(cur);
    if cur != 0 && host::set_query(host::set::GAMEPAD_PRESENT).r != 0 {
        host::widget_set(cur, host::wd::NAV_FOCUS, &[], 0, 0);
    }
}

pub fn on_leave_container() {
    if g().is_some_and(|i| inv::leave(&mut i.st)) {
        host::game_i(game::VOICES, 0);
    }
    host::set_flag(host::set::VIEWPORT, 0);
    host::set_flag(host::set::ACTIVE, 0);
    if let Some(i) = g() {
        i.last_current_widget = i.current_widget;
    }
    super::invgui::destroy_background();
}

pub fn gamepad_changed() {
    let pad = host::set_query(host::set::GAMEPAD_PRESENT).r != 0;
    host::set_flag(host::set::DRAW_FOCUS, i32::from(pad));
}

pub fn drop_all() {
    delete_all_items();
    destroy_script();
    take();
}

pub fn blank_from(i: Inv) {
    install(Box::new(i));
}
