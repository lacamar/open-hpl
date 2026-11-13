//! Order contracts of the world registry, body/joint shells and surface data, against a recording host op.

use super::shell::{self, BodyState};
use super::surf::SurfaceState;
use super::wreg::{self, Wa};
use std::cell::RefCell;

thread_local! {
    static LOG: RefCell<Vec<(i32, usize, usize)>> = const { RefCell::new(Vec::new()) };
    static JOINT_BREAKS: RefCell<Vec<usize>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn op(code: i32, a: *mut Wa) -> usize {
    // SAFETY: tests pass a live block.
    let a = unsafe { &mut *a };
    LOG.with(|l| l.borrow_mut().push((code, a.p[0], a.p[1])));
    match code {
        wreg::WO_JOINT_UPDATE | wreg::WO_BODY_BEFORE | shell::BO_CB_AABB => 1,
        wreg::WO_JOINT_BREAKAGE => JOINT_BREAKS.with(|b| usize::from(b.borrow().contains(&a.p[0]))),
        shell::BO_JOINT_ORPHAN => usize::from(a.p[0] == 41),
        _ => 0,
    }
}

fn take() -> Vec<(i32, usize, usize)> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

#[test]
fn world_update_erases_broken_joints_and_keeps_order() {
    let w = wreg::new(op, 7);
    for j in [10, 11, 12] {
        wreg::add(w, wreg::JOINT, j);
    }
    JOINT_BREAKS.with(|b| *b.borrow_mut() = vec![11]);
    take();
    wreg::update(w, 0.5);
    let joints: Vec<_> = take()
        .iter()
        .filter(|e| e.0 == wreg::WO_JOINT_UPDATE || e.0 == wreg::WO_DELETE)
        .map(|e| (e.0, if e.0 == wreg::WO_DELETE { e.2 } else { e.1 }))
        .collect();
    assert_eq!(
        joints,
        [
            (wreg::WO_JOINT_UPDATE, 10),
            (wreg::WO_JOINT_UPDATE, 11),
            (wreg::WO_DELETE, 11),
            (wreg::WO_JOINT_UPDATE, 12)
        ]
    );
    assert_eq!(wreg::len(w, wreg::JOINT), 2);
    assert_eq!(wreg::at(w, wreg::JOINT, 1), 12);
    super::book::world_free(w);
}

#[test]
fn materials_keep_first_insert_in_name_order() {
    let w = wreg::new(op, 7);
    wreg::add_material(w, b"Zeta", 1);
    wreg::add_material(w, b"Alpha", 2);
    wreg::add_material(w, b"Zeta", 3);
    assert_eq!(wreg::material_len(w), 2);
    let (_, n, h) = wreg::material_at(w, 0).unwrap_or((std::ptr::null(), 0, 0));
    assert_eq!((n, h), (5, 2));
    super::book::world_free(w);
}

#[test]
fn destroy_all_runs_in_the_c_plus_plus_order() {
    let w = wreg::new(op, 7);
    wreg::add(w, wreg::SHAPE, 1);
    wreg::add(w, wreg::BODY, 2);
    wreg::add(w, wreg::CHAR, 3);
    wreg::add(w, wreg::JOINT, 4);
    wreg::add(w, wreg::CTRL, 5);
    wreg::add(w, wreg::ROPE, 6);
    wreg::add_material(w, b"M", 8);
    take();
    wreg::destroy_all(w);
    let order: Vec<_> = take()
        .into_iter()
        .map(|e| {
            (
                e.0,
                if e.0 == wreg::WO_DELETE {
                    (e.1, e.2)
                } else {
                    (0, e.1)
                },
            )
        })
        .collect();
    assert_eq!(
        order,
        [
            (wreg::WO_DELETE, (wreg::CHAR, 3)),
            (wreg::WO_BODY_DESTROY, (0, 2)),
            (wreg::WO_DELETE, (wreg::ROPE, 6)),
            (wreg::WO_DELETE, (wreg::SHAPE, 1)),
            (wreg::WO_DELETE, (wreg::JOINT, 4)),
            (wreg::WO_DELETE, (wreg::CTRL, 5)),
            (wreg::WO_DELETE, (6, 8)),
        ]
    );
    assert_eq!(wreg::len(w, wreg::BODY), 0);
    super::book::world_free(w);
}

#[test]
fn body_shell_lists_and_destroy_sequence() {
    let mut st = std::mem::MaybeUninit::<BodyState>::uninit();
    // SAFETY: `st` is writable and outlives the shell.
    let b = unsafe { shell::body_new(op, 100, st.as_mut_ptr(), 200, 300) };
    take();
    shell::body_add_callback(b, 1);
    shell::body_add_callback(b, 2);
    shell::body_add_callback(b, 1);
    shell::body_remove_callback(b, 1);
    assert!(shell::body_on_aabb(b, 100, 9));
    let cbs: Vec<_> = take()
        .into_iter()
        .filter(|e| e.0 == shell::BO_CB_AABB)
        .map(|e| e.1)
        .collect();
    assert_eq!(cbs, [2, 1]);
    shell::body_add_verlet(b, 5);
    shell::body_add_verlet(b, 6);
    shell::body_add_verlet(b, 5);
    shell::body_remove_verlet(b, 5);
    shell::body_add_joint(b, 40);
    shell::body_add_joint(b, 41);
    shell::body_add_char(b, 50);
    take();
    shell::body_destroy(b, 100);
    let codes: Vec<_> = take().into_iter().map(|e| e.0).collect();
    assert_eq!(
        codes,
        [
            shell::BO_SHAPE_DESTROY,
            shell::BO_CHAR_DISCONNECT,
            shell::BO_VERLET_REMOVE,
            shell::BO_JOINT_REMOVE_BODY,
            shell::BO_JOINT_ORPHAN,
            shell::BO_JOINT_REMOVE_BODY,
            shell::BO_JOINT_ORPHAN,
            shell::BO_WORLD_DESTROY_JOINT,
            shell::BO_SOUND_DESTROY,
            shell::BO_SOUND_DESTROY,
            shell::BO_DELETE_LOW_LEVEL
        ]
    );
    shell::body_free(b);
}

#[test]
fn surface_defaults_and_accessors() {
    let mut s = SurfaceState::with_defaults(0);
    assert_eq!(
        (s.elasticity, s.min_scrape_speed, s.max_scrape_freq_speed),
        (0.5, 0.6, 3.0)
    );
    s.set_f32(3, 0.25);
    s.set_i32(4, 6);
    assert_eq!(
        (s.get_f32(3), s.get_i32(4), s.min_roll_contacts),
        (0.25, 6, 0)
    );
}
