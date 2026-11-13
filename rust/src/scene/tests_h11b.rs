//! H11 pass 2: `h11b_mesh` over a mock entity with bone bodies (the C++ harness has no physics
//! world, so the body paths are checked here against the C++ statement order).

use super::h11::MeshCtl;
use super::h11b_mesh::*;
use crate::math::{IDENTITY, matrix_inverse, matrix_mul};
use crate::types::Matrixf;
use std::ffi::c_void;

#[derive(Default)]
struct Node {
    children: Vec<usize>,
    world: Matrixf,
    body: usize,
    collider: usize,
    inv_body: Matrixf,
    body_matrix: Matrixf,
}

#[derive(Default)]
struct Mock {
    nodes: Vec<Node>,
    bodies: Vec<Matrixf>,
    log: Vec<String>,
    root: usize,
}

fn tr(x: f32, y: f32, z: f32) -> Matrixf {
    let mut m = IDENTITY;
    m.m[0][3] = x;
    m.m[1][3] = y;
    m.m[2][3] = z;
    m
}

unsafe extern "C" fn call(ctx: *mut c_void, op: i32, a: usize, b: usize, i: i32, _f: f32) -> usize {
    // SAFETY: the tests pass a live `Mock`.
    let m = unsafe { &mut *ctx.cast::<Mock>() };
    let node = |a: usize| a.wrapping_sub(1);
    let ptr = |x: &Matrixf| x as *const Matrixf as usize;
    match op {
        x if x == Op::NodeChildCount as i32 => m.nodes[node(a)].children.len(),
        x if x == Op::NodeChild as i32 => m.nodes[node(a)].children[i as usize] + 1,
        x if x == Op::NodeWorld as i32 => ptr(&m.nodes[node(a)].world),
        x if x == Op::NodeUpdateWorld as i32 => {
            m.log.push(format!("uw {a}"));
            0
        }
        x if x == Op::NodeSetMatrix as i32 => {
            // SAFETY: `b` is the matrix the module passes.
            let v = unsafe { *(b as *const Matrixf) };
            m.log.push(format!("set {a} {:?}", v.m[0]));
            m.nodes[node(a)].world = v;
            0
        }
        x if x == Op::BoneBody as i32 => m.nodes[node(a)].body,
        x if x == Op::BoneCollider as i32 => m.nodes[node(a)].collider,
        x if x == Op::BoneInvBody as i32 => ptr(&m.nodes[node(a)].inv_body),
        x if x == Op::BoneBodyMatrix as i32 => ptr(&m.nodes[node(a)].body_matrix),
        x if x == Op::BodyWorld as i32 => ptr(&m.bodies[a - 1]),
        x if x == Op::BodySetActive as i32 => {
            m.log.push(format!("active {a} {i}"));
            0
        }
        x if x == Op::BodyEnable as i32 => {
            m.log.push(format!("enable {a}"));
            0
        }
        x if x == Op::BodyZeroVel as i32 => {
            m.log.push(format!("zero {a}"));
            0
        }
        x if x == Op::BoneState as i32 => i as usize + 1,
        x if x == Op::BoneCount as i32 => m.nodes.len(),
        x if x == Op::Root as i32 => m.root + 1,
        x if x == Op::ResetGfx as i32 => {
            m.log.push("reset".into());
            0
        }
        _ => 0,
    }
}

fn mock() -> Mock {
    // root(0) -> bone 1 (body 1) -> bone 2 (no body) ; bone 3 (body 2, collider 3) under root
    let mut m = Mock::default();
    for _ in 0..4 {
        m.nodes.push(Node {
            world: IDENTITY,
            inv_body: tr(0.0, -1.0, 0.0),
            body_matrix: IDENTITY,
            ..Node::default()
        });
    }
    m.root = 0;
    m.nodes[0].children = vec![1, 3];
    m.nodes[1].children = vec![2];
    m.nodes[1].body = 1;
    m.nodes[3].body = 2;
    m.nodes[3].collider = 3;
    m.bodies = vec![tr(1.0, 2.0, 3.0), tr(4.0, 5.0, 6.0)];
    m
}

#[test]
fn bone_from_body_sets_local_from_body_and_recurses() {
    let mut m = mock();
    let vt = HostCall {
        ctx: (&raw mut m).cast(),
        call,
    };
    let h = Host(&vt);
    physics_from_bodies(&h);
    let parent = IDENTITY;
    let bone_world = matrix_mul(&tr(1.0, 2.0, 3.0), &tr(0.0, -1.0, 0.0));
    let want = matrix_mul(&matrix_inverse(&parent), &bone_world);
    assert_eq!(m.nodes[1].world, want);
    assert!(m.log.contains(&"uw 3".to_string()));
    assert_eq!(m.log.iter().filter(|l| l.starts_with("set")).count(), 2);
}

#[test]
fn physics_active_and_fade_drive_bodies() {
    let mut m = mock();
    let vt = HostCall {
        ctx: (&raw mut m).cast(),
        call,
    };
    let h = Host(&vt);
    let mut ctl = MeshCtl::new();
    ctl.colliders = true;
    physics_active(&h, &raw mut ctl, true);
    assert!(ctl.physics && !ctl.updated_bones);
    assert_eq!(m.log[0], "reset");
    assert!(m.log.contains(&"active 3 0".to_string()) && m.log.contains(&"active 2 1".to_string()));
    assert!(!m.log.iter().any(|l| l.starts_with("zero")));
    m.log.clear();
    physics_fade(&h, &raw mut ctl, 0.5);
    assert!(ctl.fading && ctl.physics_fade_speed == 2.0);
    assert_eq!(m.log, ["active 1 0", "active 2 0"]);
    m.log.clear();
    physics_active(&h, &raw mut ctl, false);
    assert_eq!(m.log.iter().filter(|l| l.starts_with("zero")).count(), 2);
}
