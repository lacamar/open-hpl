//! `cMultipleSettingsHandler` arithmetic over `cGSEntry` records. The entries stay heap objects
//! owned by C++ (callers hold the pointers); this decides slots and blends values.

use std::mem::offset_of;

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Entry {
    pub val: f32,
    pub dest: f32,
    pub speed: f32,
    pub types: u32,
    pub destroy_at_dest: bool,
}

pub const ENTRY_OFFSETS: [usize; 5] = [
    offset_of!(Entry, val),
    offset_of!(Entry, dest),
    offset_of!(Entry, speed),
    offset_of!(Entry, types),
    offset_of!(Entry, destroy_at_dest),
];

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Calc {
    Mul,
    Min,
    Max,
    Avg,
}

impl Calc {
    pub fn from_i32(v: i32) -> Option<Self> {
        Some(match v {
            0 => Self::Mul,
            1 => Self::Min,
            2 => Self::Max,
            3 => Self::Avg,
            _ => return None,
        })
    }
}

pub fn correct_sign_of_speed(current: f32, dest: f32, speed: f32) -> f32 {
    if dest < current && speed > 0.0 {
        return -speed;
    }
    if dest > current && speed < 0.0 {
        return -speed;
    }
    speed
}

impl Entry {
    pub fn fade_to(&mut self, dest: f32, speed: f32, destroy: bool) {
        self.dest = dest;
        self.speed = correct_sign_of_speed(self.val, self.dest, speed);
        self.destroy_at_dest = destroy;
    }

    pub fn set_val_and_dest(&mut self, val: f32) {
        self.val = val;
        self.dest = val;
    }

    /// One `Update` step; true when the entry is to be destroyed.
    pub fn update(&mut self, dt: f32) -> bool {
        if self.val != self.dest {
            self.val = self.speed.mul_add(dt, self.val);
            if self.speed < 0.0 && self.val < self.dest || self.speed > 0.0 && self.val > self.dest
            {
                self.val = self.dest;
            }
        }
        self.destroy_at_dest && self.val == self.dest
    }
}

/// `CalcResults` over the slot vector; `None` slots are unset. An unknown calc type blends
/// nothing but still counts as used, like the C++ switch.
pub fn calc_results(slots: &[Option<Entry>], types: u32, calc: i32, default: f32) -> f32 {
    if slots.is_empty() {
        return default;
    }
    let calc = Calc::from_i32(calc);
    let mut fin = 0.0f32;
    let mut first = true;
    let mut used = false;
    for e in slots.iter().flatten() {
        if e.types & types == 0 {
            continue;
        }
        used = true;
        match calc {
            Some(Calc::Mul) => {
                if first {
                    fin = 1.0;
                }
                fin *= e.val;
            }
            Some(Calc::Min) => {
                if first || fin > e.val {
                    fin = e.val;
                }
            }
            Some(Calc::Max) => {
                if first || fin < e.val {
                    fin = e.val;
                }
            }
            Some(Calc::Avg) => {
                if first {
                    fin = 0.0;
                }
                fin += e.val;
            }
            None => {}
        }
        first = false;
    }
    if !used {
        return default;
    }
    if calc == Some(Calc::Avg) {
        fin /= slots.len() as f32;
    }
    fin
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum SlotAction {
    Null,
    Existing,
    Create,
    ResizeAndCreate,
}

/// `GetEntry`; a negative index is out of range (C++ indexed the vector out of bounds).
pub fn get_slot_action(idx: i32, len: usize, occupied: bool, force: bool) -> SlotAction {
    let Ok(i) = usize::try_from(idx) else {
        return SlotAction::Null;
    };
    if i >= len {
        if force {
            SlotAction::ResizeAndCreate
        } else {
            SlotAction::Null
        }
    } else if !occupied && force {
        SlotAction::Create
    } else if occupied {
        SlotAction::Existing
    } else {
        SlotAction::Null
    }
}

/// `CreateEntry`: first empty slot, else one past the end.
pub fn create_slot(occupied: &[bool]) -> usize {
    occupied.iter().position(|o| !o).unwrap_or(occupied.len())
}
