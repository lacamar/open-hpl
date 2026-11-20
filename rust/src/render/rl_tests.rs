use std::cell::RefCell;
use std::ffi::c_void;
use std::mem::MaybeUninit;

use super::rl::*;
use crate::material::h7::MatState;
use crate::math::bv::{Bv, Frustum};
use crate::scene::rnd::Rnd;
use crate::types::{Matrixf, Vector3f};

struct Obj {
    rnd: Rnd,
    mat: MatState,
    bv: Bv,
    updated: u32,
}

thread_local! {
    static OBJS: RefCell<Vec<Obj>> = const { RefCell::new(Vec::new()) };
}

fn obj<'a>(o: *mut c_void) -> &'a mut Obj {
    // SAFETY: the test passes pointers to boxed `Obj`s that outlive the call.
    unsafe { &mut *o.cast::<Obj>() }
}

unsafe extern "C" fn renderer_frame() -> i32 {
    7
}
unsafe extern "C" fn render_type(_: *mut c_void) -> i32 {
    1
}
unsafe extern "C" fn material(o: *mut c_void) -> *mut MatState {
    &raw mut obj(o).mat
}
unsafe extern "C" fn type_flags(_: *mut c_void) -> u32 {
    0
}
unsafe extern "C" fn rnd(o: *mut c_void) -> *mut Rnd {
    &raw mut obj(o).rnd
}
unsafe extern "C" fn update_frame(o: *mut c_void, _: f32) {
    obj(o).updated += 1;
}
unsafe extern "C" fn update_viewport(_: *mut c_void, _: *mut c_void, _: f32) -> bool {
    true
}
unsafe extern "C" fn model_matrix(_: *mut c_void, _: *mut c_void) -> *mut Matrixf {
    std::ptr::null_mut()
}
unsafe extern "C" fn bounding_volume(o: *mut c_void) -> *mut Bv {
    &raw mut obj(o).bv
}
unsafe extern "C" fn no(_: *mut c_void) -> bool {
    false
}
unsafe extern "C" fn vertex_buffer(_: *mut c_void) -> usize {
    0
}
unsafe extern "C" fn world_position(_: *mut c_void, _: *mut Vector3f) {}
unsafe extern "C" fn plane(_: *mut c_void, _: *mut Plane) -> bool {
    false
}
unsafe extern "C" fn collides(_: *mut c_void, _: *mut c_void) -> bool {
    true
}
unsafe extern "C" fn flare_active(_: *mut c_void, _: i32) -> bool {
    false
}
unsafe extern "C" fn flare_type(_: *mut c_void, _: i32) -> *mut c_void {
    std::ptr::null_mut()
}

const HOST: Host = Host {
    renderer_frame,
    render_type,
    material,
    type_flags,
    rnd,
    update_frame,
    update_viewport,
    model_matrix,
    bounding_volume,
    uses_occlusion_query: no,
    vertex_buffer,
    world_position,
    plane,
    collides_frustum: collides,
    flare_active,
    flare_type,
};

fn new_obj(z: f32) -> Obj {
    let mut o = Obj {
        rnd: Rnd::new(),
        mat: MatState::new(false),
        bv: Bv::new(),
        updated: 0,
    };
    o.bv.set_local_min_max(
        Vector3f {
            x: -1.0,
            y: -1.0,
            z: -1.0,
        },
        Vector3f {
            x: 1.0,
            y: 1.0,
            z: 1.0,
        },
    );
    o.bv.set_position(Vector3f { x: 0.0, y: 0.0, z });
    o
}

#[test]
fn solid_objects_gate_and_sort_by_view_z() {
    let mut fr = MaybeUninit::<Frustum>::zeroed();
    // SAFETY: every field of `Frustum` is plain data; zero is a valid value.
    let fr = unsafe { fr.assume_init_mut() };
    fr.view = Matrixf {
        m: [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    };
    let mut rl = RenderList::new();
    rl.setup(0.5, std::ptr::null_mut(), fr);
    OBJS.with(|v| {
        let mut v = v.borrow_mut();
        v.clear();
        v.extend([new_obj(3.0), new_obj(1.0)]);
        for o in v.iter_mut() {
            // SAFETY: the objects outlive the calls.
            unsafe { rl.add_object(&HOST, (&raw mut *o).cast()) };
        }
        for o in v.iter() {
            assert_eq!(o.updated, 1);
            assert_eq!(o.rnd.render_frame_count, 7);
        }
        // SAFETY: as above.
        unsafe { rl.compile(&HOST, 1) };
        let z = rl.sorted(L_SORTED);
        assert_eq!(z.len(), 2);
        let zs: Vec<f32> = z
            .iter()
            .map(|&p| obj(p as *mut c_void).rnd.view_space_z)
            .collect();
        assert!((zs[0] - zs[1]).abs() > 1.0);
        assert_eq!(rl.sorted(L_SOLID).len(), 2);
        rl.clear();
        assert_eq!(rl.sorted(L_SORTED).len(), 0);
    });
}
