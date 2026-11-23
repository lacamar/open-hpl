//! Shell glue against a fake shell: constructor defaults, loader variables, save data round trip.

use super::a7_area::{
    A7Area, Pod, T_EXAMINE, T_FLASHBACK, T_INSANITY, T_LADDER, T_LIQUID, T_SCRIPT, T_SIGN, T_SLIME,
    T_STICKY, st,
};
use super::a7_shell::{Sv, V, Vt};
use crate::luxsave::layout_ar as lay;
use std::ffi::c_void;

unsafe extern "C" fn var_f(_: *mut c_void, _: *const u8, _: usize, d: f32) -> f32 {
    d
}
unsafe extern "C" fn var_i(_: *mut c_void, _: *const u8, _: usize, d: i32) -> i32 {
    d
}
unsafe extern "C" fn var_b(_: *mut c_void, _: *const u8, _: usize, d: bool) -> bool {
    d
}
unsafe extern "C" fn var_s(_: *mut c_void, _: *const u8, _: usize, d: *const u8, n: usize) -> Sv {
    Sv { p: d, n }
}
unsafe extern "C" fn doc_open(_: *mut c_void, _: *const u8, _: usize) -> bool {
    false
}
unsafe extern "C" fn doc_f(_: *mut c_void, _: *const u8, _: usize) -> f32 {
    0.0
}
unsafe extern "C" fn doc_s(_: *mut c_void, _: *const u8, _: usize) -> Sv {
    Sv {
        p: std::ptr::null(),
        n: 0,
    }
}
unsafe extern "C" fn doc_close(_: *mut c_void) {}
unsafe extern "C" fn max_focus(_: *mut c_void, _: f32) {}
unsafe extern "C" fn callback(_: *mut c_void, _: i32, _: *const u8, _: usize, _: bool) {}
unsafe extern "C" fn cfg_f(
    _: *mut c_void,
    _: *const u8,
    _: usize,
    _: *const u8,
    _: usize,
    d: f32,
) -> f32 {
    d
}
unsafe extern "C" fn cfg_s(_: *mut c_void, _: *const u8, _: usize, _: *const u8, _: usize) -> Sv {
    Sv {
        p: std::ptr::null(),
        n: 0,
    }
}
unsafe extern "C" fn preload(_: *mut c_void, _: *const u8, _: usize) {}
unsafe extern "C" fn mat_find(_: *mut c_void, _: *const u8, _: usize) -> usize {
    0x77
}
unsafe extern "C" fn mat_name(_: *mut c_void, _: usize) -> Sv {
    Sv {
        p: b"stone".as_ptr(),
        n: 5,
    }
}
unsafe extern "C" fn body_ids(_: *mut c_void, _: usize, out: *mut i32) {
    // SAFETY: the caller passes room for two ids.
    unsafe {
        *out = 12;
        *out.add(1) = 34;
    }
}
unsafe extern "C" fn find_body(_: *mut c_void, e: i32, b: i32) -> usize {
    usize::try_from(e * 100 + b).unwrap_or(0)
}

fn vt() -> Vt {
    Vt {
        ctx: std::ptr::null_mut(),
        var_f,
        var_i,
        var_b,
        var_s,
        cfg_f,
        cfg_s,
        preload_sound: preload,
        preload_ps: preload,
        mat_find,
        mat_name,
        body_ids,
        find_body,
        max_focus,
        callback,
        doc_open,
        doc_f,
        doc_s,
        doc_close,
    }
}

fn area(kind: i32) -> A7Area {
    A7Area::new(kind).unwrap()
}

#[test]
fn sticky_defaults_and_full_game_save() {
    let t = vt();
    // SAFETY: the fake thunks ignore their context.
    let v = unsafe { V::new(&t) };
    let mut a = area(T_STICKY);
    assert!(a.init(b"sticky_1", &v));
    assert_eq!(a.name, b"sticky_1");
    let Pod::Sticky(s) = a.pod else { panic!() };
    assert!(s.move_body && s.rotate_body && s.check_center_in_area && s.can_detach);
    assert_eq!((s.pose_time, s.set_mtx_time), (0.2, 1.0));
    assert!(!area(T_SLIME).init(b"s", &v));
    assert!(area(T_LIQUID).init(b"l", &v));
}

#[test]
fn loader_variables_take_the_defaults() {
    let t = vt();
    // SAFETY: as above.
    let v = unsafe { V::new(&t) };
    let mut l = area(T_LIQUID);
    l.load_vars(&v);
    let Pod::Liquid(s) = l.pod else { panic!() };
    assert_eq!(s.player_speed_mul, 0.8);
    assert_eq!(s.max_wave_distance_sqr, 625.0);
    assert_eq!(l.material, 0x77);
    let mut k = area(T_STICKY);
    k.load_vars(&v);
    let Pod::Sticky(s) = k.pod else { panic!() };
    assert_eq!(s.pose_time, 1.0);
    let mut sl = area(T_SLIME);
    sl.load_vars(&v);
    let Pod::Slime(s) = sl.pod else { panic!() };
    assert_eq!(s.check_collision_count, 0.0);
}

fn zeroed<T>() -> Box<T> {
    // SAFETY: the mirrors hold integers, floats and word arrays only.
    Box::new(unsafe { std::mem::zeroed() })
}

#[test]
fn sticky_save_round_trip() {
    let t = vt();
    // SAFETY: as above.
    let v = unsafe { V::new(&t) };
    let mut a = area(T_STICKY);
    a.init(b"s", &v);
    a.strs[st::ATTACH_FUNCTION] = b"OnAttach".to_vec();
    if let Pod::Sticky(s) = &mut a.pod {
        s.attached_body = 0x1000;
        s.pose_time = 3.0;
        s.attached_body_gravity = true;
    }
    let mut d = zeroed::<lay::cLuxArea_Sticky_SaveData>();
    // SAFETY: `d` is a live save data mirror.
    unsafe { a.save(std::ptr::from_mut(&mut *d).cast(), &v) };
    assert_eq!((d.mlAttachedEntityID, d.mlAttachedBodyID), (12, 34));
    assert_eq!(d.mfPoseTime, 3.0);
    let mut b = area(T_STICKY);
    // SAFETY: as above.
    unsafe { b.load(std::ptr::from_ref(&*d).cast(), &v) };
    let Pod::Sticky(s) = b.pod else { panic!() };
    assert_eq!(s.attached_body, 12 * 100 + 34);
    assert_eq!(s.last_attached, s.attached_body);
    assert!(s.attached_body_gravity);
    assert_eq!(b.strs[st::ATTACH_FUNCTION], b"OnAttach");
}

#[test]
fn sticky_without_attachment_saves_minus_one() {
    let t = vt();
    // SAFETY: as above.
    let v = unsafe { V::new(&t) };
    let mut a = area(T_STICKY);
    let mut d = zeroed::<lay::cLuxArea_Sticky_SaveData>();
    // SAFETY: as above.
    unsafe { a.save(std::ptr::from_mut(&mut *d).cast(), &v) };
    assert_eq!((d.mlAttachedEntityID, d.mlAttachedBodyID), (-1, -1));
}

#[test]
fn liquid_material_round_trip() {
    let t = vt();
    // SAFETY: as above.
    let v = unsafe { V::new(&t) };
    let mut a = area(T_LIQUID);
    a.material = 1;
    let mut d = zeroed::<lay::cLuxArea_Liquid_SaveData>();
    // SAFETY: as above.
    unsafe { a.save(std::ptr::from_mut(&mut *d).cast(), &v) };
    let mut b = area(T_LIQUID);
    // SAFETY: as above.
    unsafe { b.load(std::ptr::from_ref(&*d).cast(), &v) };
    assert_eq!(b.material, 0x77);
}

#[test]
fn slime_save_overwrites_the_callback_from_the_save_data() {
    let t = vt();
    // SAFETY: as above.
    let v = unsafe { V::new(&t) };
    let mut a = area(T_SLIME);
    a.strs[st::SLIME_CALLBACK] = b"Cb".to_vec();
    a.strs[st::SLIME_SOUND] = b"snd".to_vec();
    let mut d = zeroed::<lay::cLuxArea_SlimeDamage_SaveData>();
    d.msCallback = [0, 0, 0, 0];
    // SAFETY: as above; the zeroed string reads as empty through a null/0 pair.
    unsafe {
        d.msCallback[0] = b"".as_ptr() as u64;
        a.save(std::ptr::from_mut(&mut *d).cast(), &v);
    }
    assert!(a.strs[st::SLIME_CALLBACK].is_empty());
    assert_eq!(a.strs[st::SLIME_SOUND], b"snd");
}

#[test]
fn insanity_and_flashback_round_trip() {
    let t = vt();
    // SAFETY: as above.
    let v = unsafe { V::new(&t) };
    let mut a = area(T_INSANITY);
    if let Pod::Insanity(s) = &mut a.pod {
        s.auto_disable = true;
    }
    let mut d = zeroed::<lay::cLuxArea_Insanity_SaveData>();
    // SAFETY: as above.
    unsafe { a.save(std::ptr::from_mut(&mut *d).cast(), &v) };
    let mut b = area(T_INSANITY);
    // SAFETY: as above.
    unsafe { b.load(std::ptr::from_ref(&*d).cast(), &v) };
    assert!(matches!(b.pod, Pod::Insanity(s) if s.auto_disable));

    let mut f = area(T_FLASHBACK);
    f.strs[st::FLASHBACK_FILE] = b"fb.xml".to_vec();
    let mut d = zeroed::<lay::cLuxArea_Flashback_SaveData>();
    // SAFETY: as above.
    unsafe { f.save(std::ptr::from_mut(&mut *d).cast(), &v) };
    let mut g = area(T_FLASHBACK);
    // SAFETY: as above.
    unsafe { g.load(std::ptr::from_ref(&*d).cast(), &v) };
    assert_eq!(g.strs[st::FLASHBACK_FILE], b"fb.xml");
}

#[test]
fn script_sign_examine_ladder_round_trip() {
    let t = vt();
    // SAFETY: as above.
    let v = unsafe { V::new(&t) };

    let mut a = area(T_SCRIPT);
    a.load_vars(&v);
    if let Pod::Script(s) = &mut a.pod {
        s.item_interaction = true;
    }
    let mut d = zeroed::<lay::cLuxArea_Script_SaveData>();
    // SAFETY: as above.
    unsafe { a.save(std::ptr::from_mut(&mut *d).cast(), &v) };
    let mut b = area(T_SCRIPT);
    // SAFETY: as above.
    unsafe { b.load(std::ptr::from_ref(&*d).cast(), &v) };
    assert!(matches!(b.pod, Pod::Script(s) if s.item_interaction));

    let mut a = area(T_SIGN);
    a.strs[st::TEXT_CAT] = b"cat".to_vec();
    a.strs[st::TEXT_ENTRY] = b"entry".to_vec();
    let mut d = zeroed::<lay::cLuxArea_Sign_SaveData>();
    // SAFETY: as above.
    unsafe { a.save(std::ptr::from_mut(&mut *d).cast(), &v) };
    let mut b = area(T_SIGN);
    // SAFETY: as above.
    unsafe { b.load(std::ptr::from_ref(&*d).cast(), &v) };
    assert_eq!(
        (&b.strs[0][..], &b.strs[1][..]),
        (&b"cat"[..], &b"entry"[..])
    );

    let mut a = area(T_EXAMINE);
    a.strs[st::INSANE_SOUND] = b"snd".to_vec();
    a.strs[st::DESC_ENTRY] = b"e".to_vec();
    let mut d = zeroed::<lay::cLuxArea_Examine_SaveData>();
    // SAFETY: as above.
    unsafe { a.save(std::ptr::from_mut(&mut *d).cast(), &v) };
    let mut b = area(T_EXAMINE);
    // SAFETY: as above.
    unsafe { b.load(std::ptr::from_ref(&*d).cast(), &v) };
    assert_eq!(b.strs[st::INSANE_SOUND], b"snd");
    assert_eq!(b.strs[st::DESC_ENTRY], b"e");

    let mut a = area(T_LADDER);
    a.load_vars(&v);
    assert_eq!(a.strs[st::MATERIAL], b"metal");
    let mut d = zeroed::<lay::cLuxArea_Ladder_SaveData>();
    // SAFETY: as above.
    unsafe { a.save(std::ptr::from_mut(&mut *d).cast(), &v) };
    let mut b = area(T_LADDER);
    // SAFETY: as above.
    unsafe { b.load(std::ptr::from_ref(&*d).cast(), &v) };
    assert_eq!(b.strs[st::MATERIAL], b"metal");
}

#[test]
fn rope_loads_start_and_round_trips() {
    let t = vt();
    // SAFETY: as above.
    let v = unsafe { V::new(&t) };
    let mut a = area(super::a7_area::T_ROPE);
    a.load_rope(
        crate::types::Vector3f {
            x: 1.0,
            y: 2.0,
            z: 3.0,
        },
        &v,
    );
    a.strs[st::ROPE_SOUND] = b"rope.snt".to_vec();
    if let Pod::Rope(s) = &mut a.pod {
        s.rope_created = true;
        s.radius = 0.25;
    }
    let mut d = zeroed::<lay::cLuxRope_SaveData>();
    // SAFETY: as above.
    unsafe { a.save(std::ptr::from_mut(&mut *d).cast(), &v) };
    assert_eq!(d.mvStartPos, [1.0, 2.0, 3.0]);
    let mut b = area(super::a7_area::T_ROPE);
    // SAFETY: as above.
    unsafe { b.load(std::ptr::from_ref(&*d).cast(), &v) };
    assert_eq!(b.strs[st::ROPE_SOUND], b"rope.snt");
    assert!(
        matches!(b.pod, Pod::Rope(s) if s.rope_created && s.radius == 0.25 && s.start_pos.z == 3.0)
    );
}
