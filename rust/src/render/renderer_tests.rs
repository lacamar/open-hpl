use super::renderer::*;
use crate::math::{IDENTITY, Planef, v3};
use crate::scene::boxtree_dyn::Handle;
use crate::scene::boxtree_dyn_tests::Mock;
use crate::types::Vector3f;

fn checks(planes: &[Planef]) -> Checks<'_> {
    Checks {
        occlusion_planes_active: true,
        planes,
        fog_cull_plane_active: false,
        fog_cull_plane: Planef::default(),
        distance_cull_active: false,
        dc_origin: v3(0.0, 0.0, 0.0),
        dc_min_range_sqr: 0.0,
        dc_screen_size_sqr: 0.0,
        dc_random_size: 0.0,
        dc_fade_time: 1.0,
        dc_fade_time_inv: 1.0,
        dc_max_range_sqr: 1.0e9,
        dc_time: 5.0,
    }
}

fn plane(a: f32, b: f32, c: f32, d: f32) -> Planef {
    Planef { a, b, c, d }
}

#[test]
fn object_visibility_flags_and_planes() {
    let mut m = Mock::new();
    m.add_obj(0x100000, v3(0.0, 0.0, 0.0), v3(1.0, 1.0, 1.0));
    m.add_obj(0x100010, v3(-5.0, 0.0, 0.0), v3(-4.0, 1.0, 1.0));
    m.objs.get_mut(&0x100000).unwrap().flags = 0b11;
    let keep_x_positive = [plane(1.0, 0.0, 0.0, 0.0)];
    let ck = checks(&keep_x_positive);
    assert!(object_visible(&mut m, &ck, 0x100000, 0b11));
    assert!(
        !object_visible(&mut m, &ck, 0x100000, 0b111),
        "missing flag"
    );
    assert!(
        !object_visible(&mut m, &ck, 0x100010, 0),
        "outside the plane"
    );
    let mut off = ck;
    off.occlusion_planes_active = false;
    assert!(object_visible(&mut m, &off, 0x100010, 0));
    m.objs.get_mut(&0x100000).unwrap().visible = false;
    assert!(!object_visible(&mut m, &ck, 0x100000, 0));
}

#[test]
fn distance_cull_fades_then_culls() {
    let mut m = Mock::new();
    m.add_obj(0x100000, v3(99.5, 0.0, 0.0), v3(100.5, 1.0, 1.0));
    m.objs.get_mut(&0x100000).unwrap().culled_by_distance = true;
    let mut ck = checks(&[]);
    ck.distance_cull_active = true;
    ck.dc_screen_size_sqr = 0.01;

    assert!(
        object_visible(&mut m, &ck, 0x100000, 0),
        "fading out, still drawn"
    );
    assert_eq!(m.objs[&0x100000].fade, -5.0);
    ck.dc_time = 7.0;
    assert!(
        !object_visible(&mut m, &ck, 0x100000, 0),
        "culled once the fade is over"
    );

    assert_eq!(distance_fade_amount(&ck, -5.0), 0.0);
    assert_eq!(distance_fade_amount(&ck, 6.5), 0.5);
    ck.distance_cull_active = false;
    assert_eq!(distance_fade_amount(&ck, -5.0), 1.0);
}

fn scene() -> (Mock, Handle, [Handle; 2]) {
    let mut m = Mock::new();
    let root = m.add_node(true);
    let leaves = [m.add_node(true), m.add_node(true)];
    for (i, &l) in leaves.iter().enumerate() {
        let x = if i == 0 { 5.0 } else { 20.0 };
        let id = 0x100000 + i as Handle * 0x10;
        m.add_obj(id, v3(x, 0.0, -10.0), v3(x + 1.0, 1.0, -9.0));
        m.objs.get_mut(&id).unwrap().node = l;
        let n = m.nodes.get_mut(&l).unwrap();
        n.objects.push(id);
        n.base.parent = root;
        n.base.min = v3(x, 0.0, -10.0);
        n.base.max = v3(x + 1.0, 1.0, -9.0);
        n.base.center = v3(x + 0.5, 0.5, -9.5);
        n.base.visible_objects = true;
        m.nodes.get_mut(&root).unwrap().children.push(l);
    }
    m.collision = 0;
    (m, root, leaves)
}

fn chc(min_objects: i32) -> Chc {
    Chc {
        frustum: 1,
        view: IDENTITY,
        origin: v3(0.0, 0.0, 0.0),
        needed_flags: 0,
        min_objects_before_occlusion: min_objects,
        sample_limit: 3,
        only_render_prev_visible: false,
        only_prev_visible_frame_count: 0,
    }
}

#[test]
fn chc_skips_occluded_leaf_and_rerenders_visible_one() {
    let (mut m, root, leaves) = scene();
    let ck = checks(&[]);
    let mut t = Tracker::default();
    m.samples.insert(leaves[1], 0);

    t.switch_and_clear();
    chc_traverse(&mut m, &ck, &mut t, &chc(0), &[root]);
    assert_eq!(
        m.rendered,
        vec![0x100000],
        "only the unoccluded leaf's object"
    );
    assert!(m.log.iter().any(|l| l == "flush"));

    m.rendered.clear();
    m.log.clear();
    t.switch_and_clear();
    chc_traverse(&mut m, &ck, &mut t, &chc(0), &[root]);
    assert!(t.was_visible(root) || !m.rendered.is_empty());
    // the leaf seen last frame is drawn straight away, before its query result
    assert_eq!(m.rendered.first(), Some(&0x100000));
}

#[test]
fn chc_renders_blockers_before_testing() {
    let (mut m, root, _) = scene();
    let ck = checks(&[]);
    let mut t = Tracker::default();
    t.switch_and_clear();
    chc_traverse(&mut m, &ck, &mut t, &chc(100), &[root]);
    assert_eq!(
        m.rendered.len(),
        2,
        "objects drawn without queries while blockers are needed"
    );
    assert!(m.log.iter().all(|l| !l.starts_with("bbox")));
}

#[test]
fn near_nodes_come_off_the_stack_first() {
    let (mut m, root, leaves) = scene();
    let ck = checks(&[]);
    let mut t = Tracker::default();
    // view space z: x=5 leaf is at z -9.5 -> far; both are in front, nearer (larger z) first
    m.nodes.get_mut(&leaves[1]).unwrap().base.center = v3(20.5, 0.5, -3.0);
    m.nodes.get_mut(&leaves[1]).unwrap().base.min = v3(20.0, 0.0, -3.5);
    m.nodes.get_mut(&leaves[1]).unwrap().base.max = v3(21.0, 1.0, -2.5);
    t.switch_and_clear();
    chc_traverse(&mut m, &ck, &mut t, &chc(0), &[root]);
    let first = m
        .log
        .iter()
        .find(|l| l.starts_with("bbox"))
        .cloned()
        .unwrap();
    assert!(first.contains(&format!("{:x}", leaves[1])), "{first}");
}

#[test]
fn brute_force_list_building() {
    let (mut m, root, _) = scene();
    let ck = checks(&[]);
    let mut t = Tracker::default();
    add_visible_to_list(&mut m, &ck, &mut t, 1, false, 0, root);
    assert_eq!(m.listed, vec![0x100000, 0x100010]);
    m.listed.clear();
    m.collision = 1;
    add_visible_to_list(&mut m, &ck, &mut t, 1, false, 0, root);
    assert!(
        m.listed.is_empty(),
        "children outside the frustum are skipped"
    );
}

#[test]
fn delayed_occlusion_marks_and_forgets() {
    let (mut m, _, leaves) = scene();
    let mut t = Tracker::default();
    let leaf = leaves[0];
    let mut asked = false;
    for _ in 0..8 {
        fetch_delayed_results(&mut m, &mut t, 3);
        assert!(!delayed_culled(&mut m, &mut t, 1, leaf) || asked);
        if has_delayed_queries(&t) {
            m.samples.insert(leaf, 0);
            render_delayed_queries(&mut m, &mut t);
            asked = true;
        }
    }
    assert!(asked, "a visible leaf is re-tested every few frames");
    fetch_delayed_results(&mut m, &mut t, 3);
    assert!(
        delayed_culled(&mut m, &mut t, 1, leaf),
        "occluded result is honoured"
    );
    for _ in 0..6 {
        fetch_delayed_results(&mut m, &mut t, 3);
    }
    m.samples.insert(leaf, 100);
    assert!(
        !delayed_culled(&mut m, &mut t, 1, leaf),
        "stale result expires"
    );
}

#[test]
fn tracker_visibility_ping_pong() {
    let mut t = Tracker::default();
    t.switch_and_clear();
    t.set_visible(0x1000);
    assert!(!t.was_visible(0x1000));
    t.switch_and_clear();
    assert!(t.was_visible(0x1000));
    t.switch_and_clear();
    assert!(!t.was_visible(0x1000));
    t.set_visible(0x2000);
    t.reset();
    t.switch_and_clear();
    assert!(!t.was_visible(0x2000));
}

#[test]
fn shadow_casters_filtered_and_sorted() {
    let mut m = Mock::new();
    let root = m.add_node(true);
    let specs = [
        (0x100000, 30.0, 0, 0usize, false, true),
        (0x100010, 10.0, 0, 0, false, true),
        (0x100020, 5.0, 1, 7, false, true),
        (0x100030, 4.0, 1, 3, false, true),
        (0x100040, 1.0, 0, 0, true, true),
        (0x100050, 2.0, 0, 0, false, false),
    ];
    for &(id, x, alpha, prog, translucent, present) in &specs {
        m.add_obj(id, v3(x, 0.0, 0.0), v3(x + 1.0, 1.0, 1.0));
        let o = m.objs.get_mut(&id).unwrap();
        o.flags = 1;
        o.node = root;
        o.material.alpha_mode = alpha;
        o.material.z_program = prog;
        o.material.translucent = translucent;
        o.material.present = present;
        m.nodes.get_mut(&root).unwrap().objects.push(id);
    }
    m.collision = 0;
    let ck = checks(&[]);
    let q = ShadowQuery {
        ck: &ck,
        light: 1,
        light_origin: v3(0.0, 0.0, 0.0),
        min_radius: 0.0,
        caster_flag: 1,
        view_check: None,
    };
    let mut out = Vec::new();
    shadow_casters(&mut m, &q, root, &mut out);
    assert_eq!(out, vec![0x100000, 0x100010, 0x100020, 0x100030]);
    sort_shadow_casters(&mut m, &mut out);
    // solid by distance first, then alpha-tested by program
    assert_eq!(out, vec![0x100010, 0x100000, 0x100030, 0x100020]);

    let mut big = q;
    big.min_radius = 2.0;
    let mut none = Vec::new();
    shadow_casters(&mut m, &big, root, &mut none);
    assert!(none.is_empty());
}

#[test]
fn view_check_drops_casters_beyond_a_facing_plane() {
    let mut view = FrustumData::default();
    // six planes facing +x .. only plane 0 faces along the light's forward vector
    view.planes[0] = plane(1.0, 0.0, 0.0, 0.0);
    for p in view.planes.iter_mut().skip(1) {
        *p = plane(-1.0, 0.0, 0.0, 1000.0);
    }
    let light = FrustumData {
        forward: v3(1.0, 0.0, 0.0),
        ..FrustumData::default()
    };
    let vc = ViewCheck::new(&view, &light);

    let mut m = Mock::new();
    let root = m.add_node(true);
    m.add_obj(0x100000, v3(-10.0, 0.0, 0.0), v3(-9.0, 1.0, 1.0));
    m.add_obj(0x100010, v3(9.0, 0.0, 0.0), v3(10.0, 1.0, 1.0));
    for id in [0x100000, 0x100010] {
        let o = m.objs.get_mut(&id).unwrap();
        o.flags = 1;
        o.node = root;
        o.bv.radius = 0.5;
        m.nodes.get_mut(&root).unwrap().objects.push(id);
    }
    m.collision = 0;
    let ck = checks(&[]);
    let q = ShadowQuery {
        ck: &ck,
        light: 1,
        light_origin: v3(0.0, 0.0, 0.0),
        min_radius: 0.0,
        caster_flag: 1,
        view_check: Some(vc),
    };
    let mut out = Vec::new();
    shadow_casters(&mut m, &q, root, &mut out);
    assert_eq!(out, vec![0x100010]);
}

#[test]
fn fog_ray_and_visibility() {
    let start = v3(0.0, 0.0, -5.0);
    let (entry, exit) = fog_ray_intersection(&IDENTITY, start, v3(0.0, 0.0, 1.0)).unwrap();
    assert_eq!((entry, exit), (4.5, 5.5));
    assert!(fog_ray_intersection(&IDENTITY, start, v3(0.0, 1.0, 0.0)).is_none());

    let f = FogParams {
        start: 0.0,
        end: 10.0,
        color_a: 1.0,
        falloff_exp: 1.0,
        ..FogParams::default()
    };
    let origin = v3(0.0, 0.0, -5.0);
    let object: Vector3f = v3(0.0, 0.0, 5.0);
    let vis = fog_visibility(&f, &IDENTITY, origin, origin, object);
    assert!((vis - 0.45).abs() < 1e-6, "{vis}");
    assert_eq!(
        fog_visibility(&f, &IDENTITY, origin, origin, v3(0.0, 0.0, -8.0)),
        1.0
    );

    let near = [
        v3(0.1, 0.1, 0.0),
        v3(-0.1, 0.1, 0.0),
        v3(-0.1, -0.1, 0.0),
        v3(0.1, -0.1, 0.0),
    ];
    assert!(fog_inside_near_plane(&IDENTITY, &near));
    let far = near.map(|v| v3(v.x + 10.0, v.y, v.z));
    assert!(!fog_inside_near_plane(&IDENTITY, &far));
}

#[test]
fn fog_sort_order_matches_less_than() {
    let mut out = [0u32; 4];
    fog_sort_order(&[3.0, 1.0, 2.0, 0.5], &mut out);
    assert_eq!(out, [3, 1, 2, 0]);
}

#[test]
fn shadow_map_pick_prefers_own_light_then_oldest() {
    let slots = [
        ShadowSlot {
            light: 1,
            frame_count: 10,
        },
        ShadowSlot {
            light: 2,
            frame_count: 3,
        },
        ShadowSlot {
            light: 3,
            frame_count: 9,
        },
    ];
    assert_eq!(pick_shadow_map(&slots, 3, 10), (Some(2), true));
    assert_eq!(
        pick_shadow_map(&slots, 9, 10),
        (Some(1), true),
        "furthest frame wins"
    );
    assert_eq!(
        pick_shadow_map(&slots[..1], 9, 10),
        (Some(0), false),
        "a lone slot is not touched"
    );
    assert_eq!(pick_shadow_map(&[], 9, 10), (None, false));
}

#[test]
fn shadow_cache_compares_spot_values_only_for_spots() {
    let c = ShadowCache {
        light: 5,
        transform_count: 2,
        radius: 4.0,
        fov: 1.0,
        aspect: 1.5,
        near_clip: 0.1,
    };
    let mut l = LightKey {
        light: 5,
        transform_count: 2,
        radius: 4.0,
        is_spot: false,
        fov: 9.0,
        aspect: 9.0,
        near_clip: 9.0,
    };
    assert!(shadow_cache_valid(&c, &l));
    l.is_spot = true;
    assert!(!shadow_cache_valid(&c, &l));
    l.fov = 1.0;
    l.aspect = 1.5;
    l.near_clip = 0.1;
    assert!(shadow_cache_valid(&c, &l));
    l.radius = f32::NAN;
    assert!(!shadow_cache_valid(&c, &l));
}

#[test]
fn shadow_map_resolution_clamps_to_the_maximum() {
    assert_eq!(shadow_map_resolution(2, 2), 2);
    assert_eq!(shadow_map_resolution(2, 1), 1);
    assert_eq!(shadow_map_resolution(0, 1), 0);
    assert_eq!(shadow_map_resolution(2, 0), 0);
}
