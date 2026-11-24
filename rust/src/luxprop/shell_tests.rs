use super::shell::*;
use crate::luxprop::ffi::Sink;
use crate::luxprop::ops::Op;
use crate::system::serialize::{Class, Field, M_VAR, T_BOOL, T_FLOAT32, T_INT32, register};
use std::ffi::c_void;
use std::sync::Arc;

unsafe extern "C" fn push(ctx: *mut c_void, op: *const Op) {
    // SAFETY: `ctx` is the `Vec<Op>` of the test and `op` is live for the call.
    unsafe { (*ctx.cast::<Vec<Op>>()).push(*op) }
}

fn reg(name: &str, vars: &[(&str, usize, usize, u32)]) {
    register(Class {
        name: name.as_bytes().to_vec(),
        parent: Vec::new(),
        fields: vars
            .iter()
            .map(|&(n, offset, size, ty)| {
                Arc::new(Field {
                    name: n.as_bytes().to_vec(),
                    class_name: Vec::new(),
                    offset,
                    size,
                    ty,
                    main: M_VAR,
                    array_size: 0,
                })
            })
            .collect(),
        size: 64,
        create: None,
    });
}

#[test]
fn button_switch_emits_connection_and_effects() {
    let p = ohpl_prs_new(0);
    let mut ops: Vec<Op> = Vec::new();
    let sink = Sink {
        ctx: std::ptr::from_mut(&mut ops).cast(),
        push,
    };
    // SAFETY: `p` is live and the sink outlives the calls.
    unsafe {
        assert!(!ohpl_prs_can_interact(p, 1.0));
        ohpl_prs_set_can(p, false, true);
        ohpl_prs_set_on(p, false, true, &sink);
        assert!(ohpl_prs_can_interact(p, 1.0));
        ohpl_prs_free(p);
    }
    let codes: Vec<(i32, i32, i32)> = ops.iter().map(|o| (o.code, o.i, o.j)).collect();
    assert_eq!(codes, [(1001, -1, 0), (1002, 0, 1)]);
}

#[test]
fn lever_save_round_trips_in_two_phases() {
    reg(
        "cLuxProp_Lever_SaveData",
        &[
            ("mlCurrentState", 0, 4, T_INT32),
            ("mlStuckState", 4, 4, T_INT32),
            ("mfMiddleAngleAmount", 8, 4, T_FLOAT32),
            ("mbAutoMoveToAngle", 12, 1, T_BOOL),
            ("mlAutoMoveGoal", 16, 4, T_INT32),
            ("mbInteractionDisablesStuck", 20, 1, T_BOOL),
        ],
    );
    let mut a = [0u8; 64];
    a[0..4].copy_from_slice(&1i32.to_ne_bytes());
    a[4..8].copy_from_slice(&(-1i32).to_ne_bytes());
    a[8..12].copy_from_slice(&0.5f32.to_ne_bytes());
    a[12] = 1;
    a[16..20].copy_from_slice(&(-1i32).to_ne_bytes());
    a[20] = 1;
    let mut b = [0u8; 64];
    let p = ohpl_prs_new(4);
    // SAFETY: `p` is live; both buffers match the registered class.
    unsafe {
        assert_eq!(ohpl_prs_load(p, a.as_mut_ptr(), 0), -1);
        assert_eq!(ohpl_prs_lever_state(p), 1);
        assert_eq!(ohpl_prs_load(p, a.as_mut_ptr(), 1), i32::MIN);
        ohpl_prs_save(p, b.as_mut_ptr());
        ohpl_prs_free(p);
    }
    assert_eq!(b[0..4], a[0..4]);
    assert_eq!(b[8..24], a[8..24]);
    assert_eq!(b[4..8], 0i32.to_ne_bytes());
}

#[test]
fn string_slots_start_empty_and_keep_what_c_stores() {
    let p = ohpl_prs_new(2);
    let mut n = 99usize;
    // SAFETY: `p` is live; `n` is writable.
    unsafe {
        ohpl_prs_str(p, 3, &mut n);
        assert_eq!(n, 0);
        ohpl_prs_set_str(p, 3, b"abc".as_ptr(), 3);
        let s = ohpl_prs_str(p, 3, &mut n);
        assert_eq!(std::slice::from_raw_parts(s, n), b"abc");
        ohpl_prs_free(p);
    }
}
