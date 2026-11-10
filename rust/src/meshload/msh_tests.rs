use super::msh::{MAGIC, MshError, parse_animation, parse_mesh};

#[derive(Default)]
struct W(Vec<u8>);

impl W {
    fn i32(&mut self, v: i32) -> &mut Self {
        self.0.extend(v.to_le_bytes());
        self
    }
    fn i16(&mut self, v: i16) -> &mut Self {
        self.0.extend(v.to_le_bytes());
        self
    }
    fn u8(&mut self, v: u8) -> &mut Self {
        self.0.push(v);
        self
    }
    fn f32(&mut self, v: f32) -> &mut Self {
        self.0.extend(v.to_le_bytes());
        self
    }
    fn f32s(&mut self, v: &[f32]) -> &mut Self {
        v.iter().for_each(|&x| {
            self.f32(x);
        });
        self
    }
    fn str(&mut self, s: &str) -> &mut Self {
        self.0.extend(s.as_bytes());
        self.0.push(0);
        self
    }
    fn mat(&mut self) -> &mut Self {
        let id = [
            1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.,
        ];
        self.f32s(&id)
    }
    fn array(&mut self, kind: i16, format: i16, elem: i32, data: &[f32]) -> &mut Self {
        self.i16(kind).i16(format).i32(0).i32(elem).f32s(data)
    }
}

fn submesh(w: &mut W, hpl3: bool, arrays: impl FnOnce(&mut W), array_num: i32) {
    w.str("sub").str("mat.mat").mat().f32s(&[1., 2., 3.]).u8(1);
    w.i32(1).i16(2).mat().f32s(&[1., 1., 1.]).u8(0);
    w.i32(1).i32(0).i32(1).f32(0.5);
    w.i32(2).i32(array_num);
    arrays(w);
    if hpl3 {
        w.i32(1);
    }
    w.i32(3);
    if hpl3 {
        w.f32(0.0);
    }
    w.i32(0).i32(1).i32(2);
}

fn standard(hpl3: bool, skeleton: bool) -> Vec<u8> {
    let mut w = W::default();
    w.i32(MAGIC).i32(if hpl3 { 8 } else { 7 });
    if hpl3 {
        w.i32(0);
    }
    w.i32(1).i32(1).u8(u8::from(skeleton));
    if skeleton {
        w.i32(1).str("root").str("sid").mat();
        if !hpl3 {
            w.f32(0.25);
        }
        w.i32(1);
        w.str("child").str("").mat();
        if !hpl3 {
            w.f32(0.5);
        }
        w.i32(0);
    }
    w.i32(1).str("node").mat().i32(7).i32(1);
    w.str("leaf").mat().i32(9).i32(0);
    submesh(
        &mut w,
        hpl3,
        |w| {
            w.array(1, 0, 3, &[0., 0., 0., 1., 0., 0.]);
            w.array(0, 0, 3, &[0., 0., 1., 0., 0., 1.]);
        },
        2,
    );
    w.i32(1);
    w.str("anim").f32(2.0).i32(1);
    if hpl3 {
        w.i32(1);
    }
    w.str("track").i16(3).i32(2);
    w.f32(1.0).f32s(&[1., 2., 3., 0., 0., 0., 1.]);
    w.f32(0.5).f32s(&[4., 5., 6., 0., 0., 0., 1.]);
    if hpl3 {
        w.f32(1.0).f32s(&[0.; 6]);
    }
    w.0
}

fn floats(data: &[u8]) -> Vec<f32> {
    data.chunks_exact(4)
        .map(|c| f32::from_le_bytes([c[0], c[1], c[2], c[3]]))
        .collect()
}

#[test]
fn reads_hpl2_mesh() {
    let m = parse_mesh(&standard(false, true), true, false).unwrap();
    assert!(!m.hpl3);
    let bones = m.bones.unwrap();
    assert_eq!(bones.len(), 2);
    assert_eq!((bones[0].parent, bones[1].parent), (-1, 0));
    assert_eq!((bones[0].unit_scale, bones[1].unit_scale), (0.25, 0.5));
    assert_eq!(m.nodes.len(), 2);
    assert_eq!((m.nodes[0].flags, m.nodes[1].parent), (7, 0));
    let s = &m.submeshes[0];
    assert_eq!(s.name, b"sub");
    assert_eq!(s.scale, [1., 2., 3.]);
    assert_eq!(s.colliders.len(), 1);
    assert_eq!(s.bone_pairs[0].weight, 0.5);
    assert_eq!(s.arrays.len(), 2);
    assert_eq!(s.arrays[0].count, 6);
    assert_eq!(s.indices, [0, 1, 2]);
    let a = &m.animations[0];
    assert_eq!(a.tracks[0].keys.len(), 2);
    assert_eq!(a.tracks[0].keys[1].trans, [4., 5., 6.]);
}

#[test]
fn unit_scale_off_reads_one() {
    let m = parse_mesh(&standard(false, true), false, false);
    assert!(m.is_err() || m.unwrap().bones.unwrap()[0].unit_scale == 1.0);
}

#[test]
fn hpl3_extras_and_animations() {
    let f = standard(true, true);
    let m = parse_mesh(&f, true, false).unwrap();
    assert!(m.hpl3);
    assert!(m.animations.is_empty());
    assert_eq!(m.bones.unwrap()[0].unit_scale, 1.0);
    let m = parse_mesh(&f, false, true).unwrap();
    assert_eq!(m.animations.len(), 1);
    assert_eq!(m.animations[0].tracks[0].keys.len(), 2);
}

#[test]
fn hpl3_uv_set_and_skipped_arrays() {
    let mut w = W::default();
    w.i32(MAGIC).i32(8).i32(0).i32(1).i32(0).u8(0).i32(0);
    submesh(
        &mut w,
        true,
        |w| {
            w.array(1, 0, 3, &[0., 0., 0., 1., 0., 0.]);
            w.array(7, 0, 2, &[0.5, 0.5, 0.25, 0.25]);
            w.array(6, 0, 2, &[9., 9., 9., 9.]);
        },
        3,
    );
    w.i32(0);
    let m = parse_mesh(&w.0, false, false).unwrap();
    let kinds: Vec<i32> = m.submeshes[0].arrays.iter().map(|a| a.kind).collect();
    assert_eq!(kinds, [1, 5]);
    assert_eq!(m.submeshes[0].indices, [0, 1, 2]);
}

#[test]
fn degenerate_tangents_are_patched() {
    let mut w = W::default();
    w.i32(MAGIC).i32(7).i32(1).i32(0).u8(0).i32(0);
    submesh(
        &mut w,
        false,
        |w| {
            w.array(1, 0, 3, &[0.; 6]);
            w.array(0, 0, 3, &[0., 0., 1., 1., 0., 0.]);
            w.array(4, 0, 4, &[0., 0., 0., 0., 1., 0., 0., 0.5]);
        },
        3,
    );
    w.i32(0);
    let m = parse_mesh(&w.0, false, false).unwrap();
    let tan = floats(&m.submeshes[0].arrays[2].data);
    assert_eq!(tan, [0., 1., 0., 1., 0., 0., 1., 0.5]);
}

#[test]
fn bad_headers() {
    assert_eq!(parse_mesh(&[], false, false), Err(MshError::BadMagic));
    let mut w = W::default();
    w.i32(MAGIC).i32(6);
    assert_eq!(parse_mesh(&w.0, false, false), Err(MshError::BadVersion));
    assert_eq!(parse_animation(&w.0), Err(MshError::BadVersion));
}

#[test]
fn truncation_reads_zeros() {
    let f = standard(false, false);
    for cut in 8..f.len() {
        let r = parse_mesh(&f[..cut], false, false);
        assert!(r.is_ok(), "cut {cut}: {r:?}");
    }
}

#[test]
fn rejects_what_the_original_cannot_survive() {
    let mut w = W::default();
    w.i32(MAGIC).i32(7).i32(1).i32(0).u8(0).i32(0);
    submesh(
        &mut w,
        false,
        |w| {
            w.i16(1).i16(2).i32(0).i32(3);
            w.0.extend([1, 2, 3, 4, 5, 6]);
            w.array(1, 0, 3, &[0.; 6]);
        },
        2,
    );
    assert!(matches!(
        parse_mesh(&w.0, false, false),
        Err(MshError::Unsupported(_))
    ));
    let mut w = W::default();
    w.i32(MAGIC).i32(7).i32(-1);
    assert_eq!(parse_mesh(&w.0, false, false).unwrap().submeshes.len(), 0);
    let mut w = W::default();
    w.i32(MAGIC).i32(7).i32(i32::MAX);
    assert!(parse_mesh(&w.0, false, false).is_err());
}

#[test]
fn reads_standalone_animation() {
    let mut w = W::default();
    w.i32(MAGIC).i32(7).str("a").f32(1.5).i32(1);
    w.str("t").i16(1).i32(1).f32(0.0);
    w.f32s(&[0., 0., 0., 0., 0., 0., 1.]);
    let a = parse_animation(&w.0).unwrap();
    assert_eq!(a.length, 1.5);
    assert_eq!(a.tracks[0].keys[0].rot, [0., 0., 0., 1.]);
}
