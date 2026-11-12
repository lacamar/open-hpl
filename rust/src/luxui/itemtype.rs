//! Inventory item types (`iLuxItemType` and its subclasses): the type table and what picking up
//! or using an item of each type does, as ordered events for C++ to run.

use super::event::{Ev, Out};
use crate::string;

pub const PUZZLE: i32 = 0;
pub const COINS: i32 = 1;
pub const NOTE: i32 = 2;
pub const DIARY: i32 = 3;
pub const LANTERN: i32 = 4;
pub const HEALTH: i32 = 5;
pub const SANITY: i32 = 6;
pub const LAMP_OIL: i32 = 7;
pub const TINDERBOX: i32 = 8;
pub const HAND_OBJECT: i32 = 9;
pub const TYPE_COUNT: i32 = 10;

pub const EV_PROG_LOG: u32 = 1;
pub const EV_COMPLETION: u32 = 2;
pub const EV_SET_COINS: u32 = 3;
pub const EV_HINT: u32 = 4;
pub const EV_SET_CONTAINER_JOURNAL: u32 = 5;
pub const EV_FORCE_INSTANT_EXIT: u32 = 6;
pub const EV_OPEN_NOTE: u32 = 7;
pub const EV_RUN_MAP_SCRIPT: u32 = 8;
pub const EV_OPEN_DIARY: u32 = 9;
pub const EV_DIARY_LAST_READ: u32 = 10;
pub const EV_LANTERN_TOGGLE: u32 = 11;
pub const EV_SET_HEALTH: u32 = 12;
pub const EV_ADD_SANITY: u32 = 13;
pub const EV_SET_LAMP_OIL: u32 = 14;
pub const EV_REMOVE_ITEM: u32 = 15;
pub const EV_SOUND: u32 = 16;
pub const EV_MESSAGE_KEY: u32 = 17;
pub const EV_USE_ITEM_STATE_RESET: u32 = 18;
pub const EV_USE_ITEM_ENTER: u32 = 19;
pub const EV_EXIT_PRESSED: u32 = 20;
pub const EV_EQUIP: u32 = 21;
pub const EV_ADD_TINDERBOX: u32 = 22;

pub const COMPLETION_ITEM: i32 = 0;
pub const COMPLETION_NOTE: i32 = 1;
pub const COMPLETION_DIARY: i32 = 2;

pub struct Info {
    pub name: &'static [u8],
    pub has_count: bool,
    pub max_count: i32,
    pub show_pickup: bool,
}

const fn info(name: &'static [u8], has_count: bool, show_pickup: bool) -> Info {
    Info {
        name,
        has_count,
        max_count: if has_count { 99 } else { 0 },
        show_pickup,
    }
}

const TABLE: [Info; 10] = [
    info(b"Puzzle", false, true),
    info(b"Coins", false, true),
    info(b"Note", false, false),
    info(b"Diary", false, false),
    info(b"Lantern", false, true),
    info(b"Health", true, true),
    info(b"Sanity", true, true),
    info(b"LampOil", true, true),
    info(b"Tinderbox", true, true),
    info(b"HandObject", false, true),
];

pub fn type_info(t: i32) -> Option<&'static Info> {
    usize::try_from(t).ok().and_then(|i| TABLE.get(i))
}

/// Case-insensitive name lookup in table order; `None` is the caller's "does not exist" error.
pub fn from_string(name: &[u8]) -> Option<i32> {
    let low = string::to_lower(name);
    (0..TYPE_COUNT).find(|&t| type_info(t).is_some_and(|i| string::to_lower(i.name) == low))
}

pub fn has_max_amount(_t: i32) -> bool {
    false
}

pub struct Item<'a> {
    pub name: &'a [u8],
    pub sval: &'a [u8],
    pub extra: &'a [u8],
    pub amount: f32,
}

pub struct Player {
    pub coins: i32,
    pub health: f32,
    pub sanity: f32,
    pub lamp_oil: f32,
    pub has_lantern: bool,
    pub lantern_active: bool,
    pub use_item_state: bool,
}

fn hint(o: &mut Out, name: &'static [u8]) {
    o.push(Ev::strs(EV_HINT, name, b""));
}

/// `BeforeAddItem` for types without a journal step. Returns whether the item is consumed.
pub fn before_add(t: i32, it: &Item, p: &Player, o: &mut Out) -> bool {
    match t {
        PUZZLE => {
            let mut s = b"Picked up puzzle item ".to_vec();
            s.extend_from_slice(it.name);
            o.text(EV_PROG_LOG, s);
            o.push(Ev::ints(EV_COMPLETION, COMPLETION_ITEM, 0));
            false
        }
        COINS => {
            o.push(Ev::ints(
                EV_SET_COINS,
                p.coins.wrapping_add(it.amount as i32),
                0,
            ));
            hint(o, b"PickCoin");
            true
        }
        LANTERN => {
            o.push(Ev::strs(EV_PROG_LOG, b"Picked up latern", b""));
            hint(o, b"PickLantern");
            false
        }
        HEALTH => {
            hint(o, b"PickHealthPotion");
            false
        }
        SANITY => {
            hint(o, b"PickSanityPotion");
            false
        }
        LAMP_OIL => {
            hint(o, b"PickOil");
            false
        }
        TINDERBOX => {
            hint(o, b"PickTinderbox");
            o.push(Ev::ints(EV_ADD_TINDERBOX, 1, 0));
            true
        }
        _ => false,
    }
}

/// Note pickup after the journal created the note: `null` is the failed-create result.
pub fn note_before_add(null: bool, it: &Item, o: &mut Out) -> bool {
    if null {
        return true;
    }
    let mut s = b"Picked up note ".to_vec();
    s.extend_from_slice(it.sval);
    o.text(EV_PROG_LOG, s);
    if it.amount > 0.0 {
        o.push(Ev::new(EV_SET_CONTAINER_JOURNAL));
        o.push(Ev::ints(EV_FORCE_INSTANT_EXIT, 1, 0));
        o.push(Ev::ints(EV_OPEN_NOTE, i32::from(it.amount >= 2.0), 0));
    }
    o.push(Ev::ints(EV_COMPLETION, COMPLETION_NOTE, 0));
    true
}

/// Diary pickup up to the optional script callback. Returns false when the journal refused it.
pub fn diary_before_add(null: bool, idx: i32, it: &Item, o: &mut Out) -> bool {
    if null {
        return false;
    }
    let mut s = b"Picked up diary ".to_vec();
    s.extend_from_slice(it.sval);
    o.text(EV_PROG_LOG, s);
    if !it.extra.is_empty() {
        let mut script = it.extra.to_vec();
        script.extend_from_slice(b"(\"");
        script.extend_from_slice(it.name);
        script.extend_from_slice(b"\",");
        script.extend_from_slice(&string::int_to_string(idx, -1));
        script.extend_from_slice(b")");
        o.text(EV_RUN_MAP_SCRIPT, script);
    }
    true
}

pub fn diary_after_script(show_journal: bool, o: &mut Out) {
    if show_journal {
        o.push(Ev::new(EV_SET_CONTAINER_JOURNAL));
        o.push(Ev::ints(EV_FORCE_INSTANT_EXIT, 1, 0));
        o.push(Ev::ints(EV_OPEN_DIARY, 1, 0));
    } else {
        o.push(Ev::new(EV_DIARY_LAST_READ));
    }
    o.push(Ev::ints(EV_COMPLETION, COMPLETION_DIARY, 0));
}

fn use_item(o: &mut Out) {
    o.push(Ev::new(EV_USE_ITEM_ENTER));
    o.push(Ev::new(EV_EXIT_PRESSED));
}

pub fn on_use(t: i32, it: &Item, p: &Player, o: &mut Out) {
    match t {
        PUZZLE => {
            if p.use_item_state {
                o.push(Ev::new(EV_USE_ITEM_STATE_RESET));
            }
            use_item(o);
        }
        TINDERBOX => use_item(o),
        LANTERN => o.push(Ev::ints(EV_LANTERN_TOGGLE, i32::from(!p.lantern_active), 0)),
        HEALTH => {
            if p.health >= 100.0 {
                return;
            }
            let mut h = p.health + it.amount;
            if h > 100.0 {
                h = 100.0;
            }
            o.push(Ev::floats(EV_SET_HEALTH, &[h]));
            o.push(Ev::new(EV_REMOVE_ITEM));
            o.push(Ev::strs(EV_SOUND, b"ui_use_health", b""));
        }
        SANITY => {
            if p.sanity >= 100.0 {
                return;
            }
            o.push(Ev::floats(EV_ADD_SANITY, &[it.amount]));
            o.push(Ev::new(EV_REMOVE_ITEM));
            o.push(Ev::strs(EV_SOUND, b"ui_use_sanity", b""));
        }
        LAMP_OIL => {
            if !p.has_lantern {
                o.push(Ev::strs(EV_MESSAGE_KEY, b"OilNeedsLantern", b""));
                return;
            }
            if p.lamp_oil >= 100.0 {
                return;
            }
            let mut l = p.lamp_oil + it.amount;
            if l > 100.0 {
                l = 100.0;
            }
            o.push(Ev::floats(EV_SET_LAMP_OIL, &[l]));
            o.push(Ev::new(EV_REMOVE_ITEM));
            o.push(Ev::strs(EV_SOUND, b"ui_use_oil", b""));
        }
        HAND_OBJECT => o.push(Ev::new(EV_EQUIP)),
        _ => {}
    }
}

pub fn lantern_name_key(active: bool) -> &'static [u8] {
    if active { b"LanternOn" } else { b"LanternOff" }
}
