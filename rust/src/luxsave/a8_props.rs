//! A8: `SaveToSaveData`/`LoadFromSaveData` of the Rust-owned A5 prop states. The save-data object is
//! addressed through the registered field table (name to offset), the state is the A5 POD; C++ no
//! longer copies member by member.

use crate::luxprop::a5_small::{A5Button, A5Door, A5Oil, A5Photocell};
use crate::system::h1::ohpl_h1_ser_flush;
use crate::system::serialize::lookup;

type Obj = *mut u8;

fn find(obj: Obj, class: &[u8], name: &[u8]) -> Option<(Obj, usize)> {
    let mut cur = Some(class.to_vec());
    while let Some(c) = cur {
        let cls = lookup(&c)?;
        if let Some(f) = cls.fields.iter().find(|f| f.name == name) {
            return Some((obj.wrapping_add(f.offset), f.size));
        }
        cur = (!cls.parent.is_empty()).then(|| cls.parent.clone());
    }
    None
}

fn slot(obj: Obj, class: &[u8], name: &[u8]) -> Option<(Obj, usize)> {
    // The registry fills at the first save, which is after the entities are gathered.
    find(obj, class, name).or_else(|| {
        ohpl_h1_ser_flush();
        find(obj, class, name)
    })
}

/// # Safety
/// `obj` is a live save-data object of `class` (or a subclass) laid out as registered.
unsafe fn put<T: Copy>(obj: Obj, class: &[u8], name: &[u8], v: T) {
    if let Some((p, size)) = slot(obj, class, name)
        && size == size_of::<T>()
    {
        // SAFETY: caller contract; `size` is the registered member size.
        unsafe { p.cast::<T>().write_unaligned(v) }
    }
}

/// # Safety
/// As [`put`].
unsafe fn get<T: Copy + Default>(obj: Obj, class: &[u8], name: &[u8]) -> T {
    match slot(obj, class, name) {
        // SAFETY: caller contract; `size` is the registered member size.
        Some((p, size)) if size == size_of::<T>() => unsafe { p.cast::<T>().read_unaligned() },
        _ => T::default(),
    }
}

const BUTTON: &[u8] = b"cLuxProp_Button_SaveData";
const PHOTO: &[u8] = b"cLuxProp_Photocell_SaveData";
const DOOR: &[u8] = b"cLuxProp_LevelDoor_SaveData";
const OIL: &[u8] = b"cLuxProp_OilBarrel_SaveData";

/// # Safety
/// `s` is a live state and `d` a live save-data object of the prop's class.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a8_button_save(s: &A5Button, d: Obj) {
    // SAFETY: caller contract.
    unsafe {
        put(d, BUTTON, b"mbSwitchedOn", s.on);
        put(d, BUTTON, b"mbCanBeSwitchedOn", s.can_on);
        put(d, BUTTON, b"mbCanBeSwitchedOff", s.can_off);
    }
}

/// # Safety
/// As [`ohpl_a8_button_save`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a8_button_load(s: &mut A5Button, d: Obj) {
    // SAFETY: caller contract.
    unsafe {
        s.on = get(d, BUTTON, b"mbSwitchedOn");
        s.can_on = get(d, BUTTON, b"mbCanBeSwitchedOn");
        s.can_off = get(d, BUTTON, b"mbCanBeSwitchedOff");
    }
}

/// # Safety
/// As [`ohpl_a8_button_save`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a8_photocell_save(s: &A5Photocell, d: Obj) {
    // SAFETY: caller contract.
    unsafe {
        put(d, PHOTO, b"mbLit", s.lit);
        put(d, PHOTO, b"mfLightLevel", s.light_level);
        put(d, PHOTO, b"mfLightLevelCheckCount", s.check_count);
        put(d, PHOTO, b"mfLightLevelOnLimit", s.on_limit);
        put(d, PHOTO, b"mfLightLevelOffLimit", s.off_limit);
    }
}

/// # Safety
/// As [`ohpl_a8_button_save`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a8_photocell_load(s: &mut A5Photocell, d: Obj) {
    // SAFETY: caller contract.
    unsafe {
        s.lit = get(d, PHOTO, b"mbLit");
        s.light_level = get(d, PHOTO, b"mfLightLevel");
        s.check_count = get(d, PHOTO, b"mfLightLevelCheckCount");
        s.on_limit = get(d, PHOTO, b"mfLightLevelOnLimit");
        s.off_limit = get(d, PHOTO, b"mfLightLevelOffLimit");
    }
}

/// # Safety
/// As [`ohpl_a8_button_save`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a8_door_save(s: &A5Door, d: Obj) {
    // SAFETY: caller contract.
    unsafe {
        put(d, DOOR, b"mbLocked", s.locked);
        put(d, DOOR, b"mbShowStats", s.show_stats);
    }
}

/// # Safety
/// As [`ohpl_a8_button_save`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a8_door_load(s: &mut A5Door, d: Obj) {
    // SAFETY: caller contract.
    unsafe {
        s.locked = get(d, DOOR, b"mbLocked");
        s.show_stats = get(d, DOOR, b"mbShowStats");
    }
}

/// # Safety
/// As [`ohpl_a8_button_save`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a8_oil_save(s: &A5Oil, d: Obj) {
    // SAFETY: caller contract.
    unsafe {
        put(d, OIL, b"mbAmountCalculated", s.calculated);
        put(d, OIL, b"mfOilAmount", s.amount);
    }
}

/// # Safety
/// As [`ohpl_a8_button_save`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a8_oil_load(s: &mut A5Oil, d: Obj) {
    // SAFETY: caller contract.
    unsafe {
        s.calculated = get(d, OIL, b"mbAmountCalculated");
        s.amount = get(d, OIL, b"mfOilAmount");
    }
}
