use super::funcs::{Gpu, Log, State};
use super::list::{self, DecalKey, IllumKey, SolidKey, TransKey};
use crate::math::{Matrixf, Vector3f};
use crate::types::Vector2l;

#[derive(Default)]
struct Rec {
    calls: Vec<String>,
}

impl Gpu for Rec {
    fn depth_test(&mut self, on: bool) {
        self.calls.push(format!("depth_test({on})"));
    }
    fn depth_write(&mut self, on: bool) {
        self.calls.push(format!("depth_write({on})"));
    }
    fn depth_func(&mut self, f: u32) {
        self.calls.push(format!("depth_func({f})"));
    }
    fn cull_active(&mut self, on: bool) {
        self.calls.push(format!("cull_active({on})"));
    }
    fn cull_mode(&mut self, m: u32) {
        self.calls.push(format!("cull_mode({m})"));
    }
    fn stencil_active(&mut self, on: bool) {
        self.calls.push(format!("stencil({on})"));
    }
    fn scissor_active(&mut self, on: bool) {
        self.calls.push(format!("scissor_active({on})"));
    }
    fn scissor_rect(&mut self, p: Vector2l, s: Vector2l) {
        self.calls
            .push(format!("scissor_rect({},{},{},{})", p.x, p.y, s.x, s.y));
    }
    fn color_write(&mut self, r: bool, g: bool, b: bool, a: bool) {
        self.calls.push(format!("color_write({r},{g},{b},{a})"));
    }
    fn alpha_test_active(&mut self, on: bool) {
        self.calls.push(format!("alpha_test({on})"));
    }
    fn alpha_test_greater_or_equal(&mut self, l: f32) {
        self.calls.push(format!("alpha_ge({l})"));
    }
    fn blend_active(&mut self, on: bool) {
        self.calls.push(format!("blend({on})"));
    }
    fn blend_func(&mut self, s: u32, d: u32) {
        self.calls.push(format!("blend_func({s},{d})"));
    }
    fn program_bind(&mut self, p: usize) {
        self.calls.push(format!("bind({p})"));
    }
    fn program_unbind(&mut self, p: usize) {
        self.calls.push(format!("unbind({p})"));
    }
    fn texture(&mut self, u: i32, t: usize) {
        self.calls.push(format!("tex({u},{t})"));
    }
    fn vertex_buffer_unbind(&mut self, v: usize) {
        self.calls.push(format!("vb_unbind({v})"));
    }
    fn vertex_buffer_bind(&mut self, v: usize) {
        self.calls.push(format!("vb_bind({v})"));
    }
    fn set_projection(&mut self, m: usize) {
        self.calls.push(format!("proj({m})"));
    }
    fn set_model_matrix(&mut self, m: usize) {
        self.calls.push(format!("model({m})"));
    }
    fn set_frame_buffer(&mut self, fb: usize, up: bool, p: Vector2l, s: Vector2l) {
        self.calls
            .push(format!("fb({fb},{up},{},{},{},{})", p.x, p.y, s.x, s.y));
    }
    fn frame_buffer_size(&mut self, _: usize) -> Vector2l {
        Vector2l { x: 640, y: 480 }
    }
    fn clear_frame_buffer(&mut self, f: u32) {
        self.calls.push(format!("clear({f})"));
    }
    fn log(&mut self, _: Log, _: i64, _: i64, _: i64, _: i64, _: f32) {
        self.calls.push("log".into());
    }
}

fn fresh() -> State {
    let mut s = State::default();
    let v = |x, y| Vector2l { x, y };
    s.init(
        1,
        false,
        false,
        v(0, 0),
        v(0, 0),
        v(1280, 720),
        crate::types::Vector2f {
            x: 1280.0,
            y: 720.0,
        },
        v(1280, 720),
        v(-1, -1),
    );
    s
}

#[test]
fn setters_skip_redundant_calls() {
    let (mut s, mut g) = (fresh(), Rec::default());
    assert!(!s.set_depth_test(&mut g, true));
    assert!(s.set_depth_test(&mut g, false));
    assert!(!s.set_depth_test(&mut g, false));
    s.set_texture(&mut g, 3, 9);
    s.set_texture(&mut g, 3, 9);
    s.set_texture_range(&mut g, 9, 2, 4);
    assert_eq!(
        g.calls,
        ["depth_test(false)", "tex(3,9)", "tex(2,9)", "tex(4,9)"]
    );
}

#[test]
fn blend_and_alpha_follow_the_modes() {
    let (mut s, mut g) = (fresh(), Rec::default());
    assert!(s.set_blend_mode(&mut g, 4));
    assert!(s.set_blend_mode(&mut g, 0));
    assert!(s.set_alpha_mode(&mut g, 1));
    assert!(s.set_alpha_limit(&mut g, 0.5));
    assert!(!s.set_alpha_limit(&mut g, 0.5));
    assert!(s.set_alpha_limit(&mut g, f32::NAN));
    assert!(s.set_alpha_limit(&mut g, f32::NAN));
    assert_eq!(
        g.calls[..5],
        [
            "blend(true)",
            "blend_func(6,7)",
            "blend(false)",
            "alpha_test(true)",
            "alpha_ge(0.5)"
        ]
    );
}

#[test]
fn program_switch_resets_material_and_unbinds() {
    let (mut s, mut g) = (fresh(), Rec::default());
    s.material = 5;
    assert!(s.set_program(&mut g, 7));
    assert_eq!(s.material, 0);
    assert!(s.set_program(&mut g, 0));
    assert!(!s.set_program(&mut g, 0));
    assert_eq!(g.calls, ["bind(7)", "unbind(7)"]);
}

#[test]
fn inverted_cull_flips_requested_mode() {
    let (mut s, mut g) = (fresh(), Rec::default());
    s.set_invert_cull(&mut g, true);
    assert_eq!(g.calls, ["cull_mode(0)"]);
    assert!(!s.set_cull_mode(&mut g, 1, true));
    assert!(s.set_cull_mode(&mut g, 1, false));
}

#[test]
fn scissor_rect_covering_target_turns_scissor_off() {
    let (mut s, mut g) = (fresh(), Rec::default());
    let (zero, size) = (Vector2l { x: 0, y: 0 }, Vector2l { x: 100, y: 50 });
    assert!(s.set_scissor_rect(&mut g, zero, size, true, zero));
    assert!(!s.set_scissor_rect(&mut g, zero, size, true, zero));
    let full = Vector2l { x: 1280, y: 720 };
    assert!(!s.set_scissor_rect(&mut g, zero, full, true, zero));
    assert_eq!(
        g.calls,
        [
            "scissor_active(true)",
            "scissor_rect(0,0,100,50)",
            "scissor_active(false)"
        ]
    );
}

#[test]
fn frame_buffer_updates_size() {
    let (mut s, mut g) = (fresh(), Rec::default());
    s.set_frame_buffer(&mut g, 4, false, true, Vector2l { x: 0, y: 0 });
    assert_eq!(s.current_frame_buffer_size, Vector2l { x: 640, y: 480 });
    s.set_frame_buffer(&mut g, 0, false, true, Vector2l { x: 0, y: 0 });
    assert_eq!(s.current_frame_buffer_size, Vector2l { x: 1280, y: 720 });
}

fn solid(alpha: i32, z: f32, obj: usize) -> SolidKey {
    SolidKey {
        alpha_mode: alpha,
        view_z: z,
        prog_z: 0,
        tex_diffuse: 0,
        prog_diffuse: 0,
        vtx: 0,
        mtx: 0,
        obj,
        tex_units: [0; list::TEXTURE_UNITS],
    }
}

#[test]
fn z_sort_puts_solid_first_then_near_to_far() {
    let keys = [solid(1, 5.0, 0), solid(0, 1.0, 1), solid(0, 9.0, 2)];
    let mut out = [0u32; 3];
    list::sort_z(&keys, &mut out);
    assert_eq!(out, [2, 1, 0]);
}

#[test]
fn diffuse_sort_breaks_ties_by_object_address() {
    let keys = [solid(0, 0.0, 30), solid(0, 0.0, 10), solid(0, 0.0, 20)];
    let mut out = [0u32; 3];
    list::sort_diffuse(&keys, &mut out);
    assert_eq!(out, [1, 2, 0]);
}

#[test]
fn decal_and_illum_sorts() {
    let d = |o, x| DecalKey {
        sort_order: o,
        pad: 0,
        tex_illum: 0,
        vtx: 0,
        mtx: 0,
        pos: Vector3f { x, y: 0.0, z: 0.0 },
        pad2: 0.0,
    };
    let mut out = [0u32; 3];
    list::sort_decal(&[d(2, 0.0), d(1, 5.0), d(1, 3.0)], &mut out);
    assert_eq!(out, [2, 1, 0]);
    let i = |a| IllumKey {
        illum_amount: a,
        pad: 0,
        tex_illum: 0,
        vtx: 0,
        mtx: 0,
    };
    list::sort_illumination(&[i(0.9), i(0.1), i(0.5)], &mut out);
    assert_eq!(out, [1, 2, 0]);
}

fn trans(z: f32, candidate: bool, center_y: f32) -> TransKey {
    let mut world = Matrixf::default();
    for k in 0..4 {
        world.m[k][k] = 1.0;
    }
    TransKey {
        view_z: z,
        placement: 99,
        plane_candidate: candidate.into(),
        pad: 0,
        center: Vector3f {
            x: 0.0,
            y: center_y,
            z: 0.0,
        },
        normal: Vector3f {
            x: 0.0,
            y: 1.0,
            z: 0.0,
        },
        point: Vector3f {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        },
        world,
    }
}

#[test]
fn large_plane_places_objects_above_and_below() {
    let mut keys = [
        trans(1.0, false, 2.0),
        trans(2.0, true, 0.0),
        trans(3.0, false, -2.0),
    ];
    let origin = Vector3f {
        x: 0.0,
        y: 5.0,
        z: 0.0,
    };
    list::find_large_surface_planes(&mut keys, origin);
    assert_eq!(keys.map(|k| k.placement), [1, 0, -1]);
    let mut out = [0u32; 3];
    list::sort_translucent(&keys, &mut out);
    assert_eq!(out, [2, 1, 0]);
}

#[test]
fn no_candidate_places_everything_at_zero() {
    let mut keys = [trans(1.0, false, 2.0), trans(2.0, false, -2.0)];
    list::find_large_surface_planes(&mut keys, Vector3f::default());
    assert_eq!(keys.map(|k| k.placement), [0, 0]);
}
