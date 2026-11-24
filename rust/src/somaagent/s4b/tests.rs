//! Ownership layer against a mock host-op channel: state machine call order, message dispatch,
//! path save round trip and sound broadcast.

use super::super::misc::SoundListener;
use super::api;
use super::core::{self, add_comp};
use super::io::{self, Io, op};
use super::model::{
    COMP_CHAR_MOVER, COMP_PATHFINDER, COMP_SOUND_LISTENER, COMP_STATE_MACHINE, Cm, Comp, K, Pf, Sm,
};
use super::pod;
use crate::math::v3;
use std::cell::RefCell;

thread_local! {
    static CALLS: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    static KNOWN: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn mock(code: u32, io: *mut Io) -> i32 {
    // SAFETY: the caller passes a live `Io`.
    let io = unsafe { &mut *io };
    match code {
        op::E_FLAGS => {
            io.i = [1, 0, 1, 0, 1, 0, 0, 0];
        }
        op::E_NAME => {
            io.s[0] = b"ag".as_ptr();
            io.n[0] = 2;
        }
        op::E_CALL => {
            // SAFETY: the decl string outlives the call.
            let d = unsafe { std::slice::from_raw_parts(io.s[0], io.n[0]) };
            let d = String::from_utf8_lossy(d).into_owned();
            let known = KNOWN.with(|k| k.borrow().contains(&d));
            CALLS.with(|c| c.borrow_mut().push(d));
            io.i[0] = i32::from(known);
        }
        _ => {}
    }
    0
}

fn setup(known: &[&str]) {
    io::install(Some(mock));
    CALLS.with(|c| c.borrow_mut().clear());
    KNOWN.with(|k| *k.borrow_mut() = known.iter().map(|s| (*s).to_string()).collect());
}

fn calls() -> Vec<String> {
    CALLS.with(|c| c.borrow().clone())
}

fn sm_agent(ent: usize) {
    let mut sm = Sm::new();
    sm.states.insert(0, b"Idle".to_vec());
    sm.states.insert(1, b"Alert".to_vec());
    sm.pod.next = 0;
    add_comp(ent, Comp::new(ent, COMP_STATE_MACHINE, K::Sm(sm)));
}

#[test]
fn state_machine_enters_then_updates_and_falls_back_to_default() {
    setup(&[
        "void State_Idle_Enter()",
        "void State_Default_Update(float)",
    ]);
    let ent = 0x1000;
    pod::test_pod(ent);
    sm_agent(ent);
    core::update_components(ent, 0.5);
    let c = calls();
    assert_eq!(
        c.first().map(String::as_str),
        Some("void State_Idle_Enter()"),
        "{c:?}"
    );
    assert!(c.iter().any(|d| d == "void State_Idle_Update(float)"));
    assert!(c.iter().any(|d| d == "void State_Default_Update(float)"));
    core::destroy_agent(ent);
    assert!(core::find(ent).is_null());
}

#[test]
fn message_runs_entity_handler_then_components_in_order() {
    setup(&["bool State_Idle_Message(int)"]);
    let ent = 0x2000;
    pod::test_pod(ent);
    sm_agent(ent);
    core::update_components(ent, 0.0);
    CALLS.with(|c| c.borrow_mut().clear());
    core::send_message(ent, 4, v3(1.0, 2.0, 3.0), 7);
    let c = calls();
    assert_eq!(
        c.first().map(String::as_str),
        Some("void OnRecieveMessage(int, cLuxEntityMessageData@)")
    );
    assert!(
        c.iter().any(|d| d == "bool State_Idle_Message(int)"),
        "{c:?}"
    );
    core::destroy_agent(ent);
}

#[test]
fn change_state_applies_on_next_update() {
    setup(&[]);
    let ent = 0x3000;
    pod::test_pod(ent);
    sm_agent(ent);
    core::update_components(ent, 0.0);
    super::natives_c::change_state(ent, 1);
    CALLS.with(|c| c.borrow_mut().clear());
    core::update_components(ent, 0.0);
    let c = calls();
    let leave = c.iter().position(|d| d == "void State_Idle_Leave()");
    let enter = c.iter().position(|d| d == "void State_Alert_Enter()");
    assert!(
        matches!((leave, enter), (Some(l), Some(e)) if l < e),
        "{c:?}"
    );
    assert_eq!(api::get_state(ent), 1);
    core::destroy_agent(ent);
}

#[test]
fn path_blob_round_trips_through_the_pathfinder() {
    setup(&[]);
    let ent = 0x4000;
    pod::test_pod(ent);
    add_comp(ent, Comp::new(ent, COMP_CHAR_MOVER, K::Cm(Cm::new(0))));
    let mut pf = Pf::new();
    pf.st.track_active = true;
    pf.st.track_idx = 2;
    pf.st.track_wait = 1.5;
    pf.end_cb = b"EndOfPath".to_vec();
    add_comp(ent, Comp::new(ent, COMP_PATHFINDER, K::Pf(pf)));
    let blob = api::save_path(ent);
    assert!(!blob.is_empty());
    core::destroy_agent(ent);
    let ent2 = 0x4001;
    pod::test_pod(ent2);
    add_comp(ent2, Comp::new(ent2, COMP_CHAR_MOVER, K::Cm(Cm::new(0))));
    add_comp(ent2, Comp::new(ent2, COMP_PATHFINDER, K::Pf(Pf::new())));
    api::load_path(ent2, &blob);
    assert_eq!(api::save_path(ent2), blob);
    core::destroy_agent(ent2);
}

#[test]
fn sound_broadcast_reaches_each_listener() {
    setup(&[]);
    let ents = [0x5000, 0x5001];
    for ent in ents {
        pod::test_pod(ent);
        add_comp(
            ent,
            Comp::new(ent, COMP_SOUND_LISTENER, K::Sl(SoundListener::default())),
        );
    }
    api::broadcast_sound(v3(0.0, 0.0, 0.0), 10.0, 0);
    let c = calls();
    assert_eq!(c.len(), 2, "{c:?}");
    for ent in ents {
        core::destroy_agent(ent);
    }
}

#[test]
fn cpp_recorded_path_blobs_replay_byte_for_byte() {
    setup(&[]);
    let blobs = include_str!("../../../tests/golden/S4b/saveagent_paths.txt");
    for (i, line) in blobs.lines().enumerate() {
        let mut blob: Vec<u8> = (0..line.len() / 2)
            .map(|k| u8::from_str_radix(&line[2 * k..2 * k + 2], 16).unwrap())
            .collect();
        if blob[0] == 0 {
            // a stopped agent's goal is not restored by the loader
            blob[1..13].fill(0);
        }
        let ent = 0x6000 + i;
        pod::test_pod(ent);
        add_comp(ent, Comp::new(ent, COMP_CHAR_MOVER, K::Cm(Cm::new(0))));
        add_comp(ent, Comp::new(ent, COMP_PATHFINDER, K::Pf(Pf::new())));
        api::load_path(ent, &blob);
        assert_eq!(api::save_path(ent), blob, "blob {i}");
        core::destroy_agent(ent);
    }
}

#[test]
fn pod_door_flags_and_script_ticks() {
    let ent = 0x7000;
    pod::test_pod(ent);
    assert!(!pod::door_flag(ent, 0, None));
    assert!(pod::door_flag(ent, 1, Some(true)));
    assert!(!pod::door_flag(ent, 0, None));
    assert_eq!(pod::name(ent), b"ag");
    assert!(pod::active(ent));
    assert_eq!(super::dump::atoi(" 12 wake x"), 12);
    assert_eq!(super::dump::atoi("-3"), -3);
    assert_eq!(super::dump::atoi("x"), 0);
}
