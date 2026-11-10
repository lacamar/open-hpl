use std::cell::Cell;
use std::ffi::c_void;
use std::ptr;

use super::draw::{
    self, ALIGN_CENTER, ALIGN_LEFT, ALIGN_RIGHT, Frame, Gfx, GfxInfo, GlyphTable, Host,
    MATERIAL_NONE, Text,
};
use super::layout;
use super::list::{
    BEGIN_MATERIAL, END_CLIP, END_MATERIAL, GfxVertex, OutVertex, RenderList, SET_CLIP,
};
use crate::math::{Rect2f, Vector2f, Vector3f};
use crate::types::{Color, Vector2l};

const WHITE: Color = Color {
    r: 1.0,
    g: 1.0,
    b: 1.0,
    a: 1.0,
};

struct FakeGfx {
    material: *const c_void,
    texture: *const c_void,
    vtx: [GfxVertex; 4],
    offset: Vector3f,
    image_size: Vector2f,
    flushes: Cell<u32>,
}

fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

fn v2(x: f32, y: f32) -> Vector2f {
    Vector2f { x, y }
}

fn unit_quad() -> [GfxVertex; 4] {
    let corner = |x: f32, y: f32| GfxVertex {
        pos: v3(x, y, 0.0),
        tex: v3(x, y, 0.0),
        tan: v3(0.0, 0.0, 0.0),
        norm: v3(0.0, 0.0, 0.0),
        col: WHITE,
    };
    [
        corner(0.0, 0.0),
        corner(1.0, 0.0),
        corner(1.0, 1.0),
        corner(0.0, 1.0),
    ]
}

fn fake(material: usize, texture: usize) -> Box<FakeGfx> {
    Box::new(FakeGfx {
        material: material as *const c_void,
        texture: texture as *const c_void,
        vtx: unit_quad(),
        offset: v3(0.0, 0.0, 0.0),
        image_size: v2(16.0, 8.0),
        flushes: Cell::new(0),
    })
}

unsafe extern "C" fn gfx_info(_: *mut c_void, gfx: *mut c_void, out: *mut GfxInfo) {
    // SAFETY: tests pass a live `FakeGfx` and a writable `GfxInfo`.
    unsafe {
        let g = &*gfx.cast::<FakeGfx>();
        *out = GfxInfo {
            offset: g.offset,
            image_size: g.image_size,
            material: ptr::addr_of!(g.material),
            texture0: ptr::addr_of!(g.texture),
            vtx: g.vtx.as_ptr(),
        };
    }
}

unsafe extern "C" fn gfx_flush(_: *mut c_void, gfx: *mut c_void) {
    // SAFETY: tests pass a live `FakeGfx`.
    let g = unsafe { &*gfx.cast::<FakeGfx>() };
    g.flushes.set(g.flushes.get() + 1);
}

unsafe extern "C" fn material(_: *mut c_void, n: i32) -> *mut c_void {
    (0x1000 + usize::try_from(n).unwrap_or(0)) as *mut c_void
}

fn host() -> Host {
    Host {
        ctx: ptr::null_mut(),
        gfx_info,
        gfx_flush,
        material,
    }
}

fn frame(clip: usize, rect: Rect2f) -> Frame {
    Frame {
        clip: clip as *const c_void,
        clip_rect: rect,
        draw_offset: v3(0.0, 0.0, 0.0),
    }
}

const OPEN: Rect2f = Rect2f {
    x: 0.0,
    y: 0.0,
    w: -1.0,
    h: -1.0,
};

fn draw_at(list: &mut RenderList, f: &Frame, h: &Host, g: &FakeGfx, pos: Vector3f, size: Vector2f) {
    let d = Gfx {
        pos,
        size,
        color: WHITE,
        material: MATERIAL_NONE,
        angle: 0.0,
        pivot: None,
        gfx: ptr::from_ref(g).cast_mut().cast(),
        host: h,
    };
    draw::draw_gfx(list, f, &d);
}

fn xs(list: &RenderList) -> Vec<f32> {
    list.vertices
        .iter()
        .step_by(4)
        .map(|v: &OutVertex| v.pos.x)
        .collect()
}

#[test]
fn z_ascending_then_pointers_descending_then_insertion_order() {
    let h = host();
    let f = frame(0x100, OPEN);
    let (a, b) = (fake(0x20, 0x10), fake(0x30, 0x10));
    let mut list = RenderList::default();
    draw_at(&mut list, &f, &h, &a, v3(1.0, 0.0, 5.0), v2(1.0, 1.0));
    draw_at(&mut list, &f, &h, &b, v3(2.0, 0.0, 1.0), v2(1.0, 1.0));
    draw_at(&mut list, &f, &h, &a, v3(3.0, 0.0, 1.0), v2(1.0, 1.0));
    draw_at(&mut list, &f, &h, &b, v3(4.0, 0.0, 1.0), v2(1.0, 1.0));
    draw_at(&mut list, &f, &h, &a, v3(5.0, 0.0, 1.0), v2(1.0, 1.0));
    list.build();
    assert_eq!(xs(&list), [2.0, 4.0, 3.0, 5.0, 1.0]);
}

#[test]
fn clip_regions_sort_descending_and_texture_breaks_last() {
    let h = host();
    let (lo, hi) = (frame(0x100, OPEN), frame(0x200, OPEN));
    let (a, b) = (fake(0x20, 0x10), fake(0x20, 0x18));
    let mut list = RenderList::default();
    draw_at(&mut list, &lo, &h, &a, v3(1.0, 0.0, 0.0), v2(1.0, 1.0));
    draw_at(&mut list, &hi, &h, &a, v3(2.0, 0.0, 0.0), v2(1.0, 1.0));
    draw_at(&mut list, &lo, &h, &b, v3(3.0, 0.0, 0.0), v2(1.0, 1.0));
    draw_at(&mut list, &hi, &h, &b, v3(4.0, 0.0, 0.0), v2(1.0, 1.0));
    list.build();
    assert_eq!(xs(&list), [4.0, 2.0, 3.0, 1.0]);
}

#[test]
fn negative_zero_ties_with_zero() {
    let h = host();
    let f = frame(0x100, OPEN);
    let a = fake(0x20, 0x10);
    let mut list = RenderList::default();
    draw_at(&mut list, &f, &h, &a, v3(1.0, 0.0, 0.0), v2(1.0, 1.0));
    draw_at(&mut list, &f, &h, &a, v3(2.0, 0.0, -0.0), v2(1.0, 1.0));
    draw_at(&mut list, &f, &h, &a, v3(3.0, 0.0, 0.0), v2(1.0, 1.0));
    list.build();
    assert_eq!(xs(&list), [1.0, 2.0, 3.0]);
}

#[test]
fn nan_z_does_not_panic() {
    let h = host();
    let f = frame(0x100, OPEN);
    let a = fake(0x20, 0x10);
    let mut list = RenderList::default();
    for i in 0..40 {
        let z = if i % 3 == 0 { f32::NAN } else { (i % 5) as f32 };
        draw_at(&mut list, &f, &h, &a, v3(i as f32, 0.0, z), v2(1.0, 1.0));
    }
    list.build();
    assert_eq!(list.vertices.len(), 160);
}

#[test]
fn batches_split_on_material_clip_and_five_thousand_quads() {
    let h = host();
    let f = frame(0x100, OPEN);
    let a = fake(0x20, 0x10);
    let mut list = RenderList::default();
    for i in 0..12000 {
        draw_at(&mut list, &f, &h, &a, v3(i as f32, 0.0, 0.0), v2(1.0, 1.0));
    }
    list.build();
    let counts: Vec<_> = list
        .batches
        .iter()
        .map(|b| (b.count, b.first, b.flags))
        .collect();
    assert_eq!(
        counts,
        [
            (20000, 0, BEGIN_MATERIAL | SET_CLIP),
            (20000, 20000, 0),
            (8000, 40000, END_CLIP | END_MATERIAL),
        ]
    );
}

#[test]
fn batch_flags_follow_material_and_clip_changes() {
    let h = host();
    let (c1, c2) = (frame(0x100, OPEN), frame(0x200, OPEN));
    let (a, b) = (fake(0x20, 0x10), fake(0x30, 0x10));
    let mut list = RenderList::default();
    draw_at(&mut list, &c2, &h, &a, v3(1.0, 0.0, 0.0), v2(1.0, 1.0));
    draw_at(&mut list, &c1, &h, &a, v3(2.0, 0.0, 0.0), v2(1.0, 1.0));
    draw_at(&mut list, &c1, &h, &b, v3(3.0, 0.0, 0.0), v2(1.0, 1.0));
    list.build();
    let flags: Vec<_> = list.batches.iter().map(|b| b.flags).collect();
    assert_eq!(
        flags,
        [
            BEGIN_MATERIAL | SET_CLIP | END_CLIP | END_MATERIAL,
            BEGIN_MATERIAL | SET_CLIP | END_MATERIAL,
            BEGIN_MATERIAL | END_CLIP | END_MATERIAL
        ]
    );
}

#[test]
fn empty_and_zero_size_clip_draw_nothing() {
    let h = host();
    let a = fake(0x20, 0x10);
    let mut list = RenderList::default();
    let null_clip = Frame {
        clip: ptr::null(),
        ..frame(0, OPEN)
    };
    draw_at(
        &mut list,
        &null_clip,
        &h,
        &a,
        v3(0.0, 0.0, 0.0),
        v2(1.0, 1.0),
    );
    let zero = frame(
        0x100,
        Rect2f {
            x: 0.0,
            y: 0.0,
            w: 0.0,
            h: 10.0,
        },
    );
    draw_at(&mut list, &zero, &h, &a, v3(0.0, 0.0, 0.0), v2(1.0, 1.0));
    assert!(list.is_empty());
    list.build();
    assert!(list.batches.is_empty());
    assert_eq!(a.flushes.get(), 0);
}

#[test]
fn clipped_out_gfx_is_not_flushed() {
    let h = host();
    let f = frame(
        0x100,
        Rect2f {
            x: 0.0,
            y: 0.0,
            w: 10.0,
            h: 10.0,
        },
    );
    let a = fake(0x20, 0x10);
    let mut list = RenderList::default();
    draw_at(&mut list, &f, &h, &a, v3(50.0, 0.0, 0.0), v2(4.0, 4.0));
    draw_at(&mut list, &f, &h, &a, v3(5.0, 5.0, 0.0), v2(4.0, 4.0));
    assert_eq!((list.len(), a.flushes.get()), (1, 1));
}

#[test]
fn negative_width_uses_image_size_and_offset() {
    let h = host();
    let f = frame(0x100, OPEN);
    let mut a = fake(0x20, 0x10);
    a.offset = v3(1.0, 2.0, 0.0);
    let mut list = RenderList::default();
    draw_at(&mut list, &f, &h, &a, v3(10.0, 10.0, 3.0), v2(-1.0, -1.0));
    list.build();
    let p: Vec<_> = list
        .vertices
        .iter()
        .map(|v| (v.pos.x, v.pos.y, v.pos.z))
        .collect();
    assert_eq!(
        p,
        [
            (11.0, 12.0, 3.0),
            (27.0, 12.0, 3.0),
            (27.0, 20.0, 3.0),
            (11.0, 20.0, 3.0)
        ]
    );
}

#[test]
fn rotation_about_default_pivot_keeps_centre() {
    let h = host();
    let f = frame(0x100, OPEN);
    let a = fake(0x20, 0x10);
    let mut list = RenderList::default();
    let d = Gfx {
        pos: v3(0.0, 0.0, 0.0),
        size: v2(2.0, 2.0),
        color: WHITE,
        material: MATERIAL_NONE,
        angle: std::f32::consts::FRAC_PI_2,
        pivot: None,
        gfx: ptr::from_ref(&*a).cast_mut().cast(),
        host: &h,
    };
    draw::draw_gfx(&mut list, &f, &d);
    list.build();
    let c = list
        .vertices
        .iter()
        .fold((0.0, 0.0), |s, v| (s.0 + v.pos.x, s.1 + v.pos.y));
    assert!((c.0 / 4.0 - 1.0).abs() < 1e-5 && (c.1 / 4.0 - 1.0).abs() < 1e-5);
}

#[test]
fn custom_material_overrides_gfx_material() {
    let h = host();
    let f = frame(0x100, OPEN);
    let a = fake(0x20, 0x10);
    let mut list = RenderList::default();
    let d = Gfx {
        pos: v3(0.0, 0.0, 0.0),
        size: v2(1.0, 1.0),
        color: WHITE,
        material: 2,
        angle: 0.0,
        pivot: None,
        gfx: ptr::from_ref(&*a).cast_mut().cast(),
        host: &h,
    };
    draw::draw_gfx(&mut list, &f, &d);
    list.build();
    assert_eq!(
        list.batches.first().map(|b| b.material as usize),
        Some(0x1002)
    );
}

fn glyph_bytes(
    gfx: *const c_void,
    offset: Vector2f,
    size: Vector2f,
    advance: f32,
) -> Box<[u8; 32]> {
    let mut b = Box::new([0u8; 32]);
    let put = |b: &mut [u8; 32], at: usize, bytes: &[u8]| {
        b[at..at + bytes.len()].copy_from_slice(bytes);
    };
    put(&mut b, 0, &(gfx as usize).to_ne_bytes());
    put(&mut b, 8, &offset.x.to_ne_bytes());
    put(&mut b, 12, &offset.y.to_ne_bytes());
    put(&mut b, 16, &size.x.to_ne_bytes());
    put(&mut b, 20, &size.y.to_ne_bytes());
    put(&mut b, 24, &advance.to_ne_bytes());
    b
}

fn text_xs(align: i32, text: &[u32]) -> Vec<f32> {
    let h = host();
    let f = frame(0x100, OPEN);
    let g = fake(0x20, 0x10);
    let glyph_a = glyph_bytes(ptr::from_ref(&*g).cast(), v2(0.0, 0.0), v2(1.0, 1.0), 2.0);
    let table_ptrs: [*const u8; 3] = [glyph_a.as_ptr(), ptr::null(), glyph_a.as_ptr()];
    let table = GlyphTable {
        glyphs: table_ptrs.as_ptr(),
        count: 3,
        first: 65,
        last: 70,
        off_gfx: 0,
        off_offset: 8,
        off_size: 16,
        off_advance: 24,
    };
    let mut list = RenderList::default();
    let t = Text {
        text,
        size: v2(10.0, 10.0),
        pos: v3(100.0, 0.0, 0.0),
        color: WHITE,
        material: MATERIAL_NONE,
        align,
        host: &h,
    };
    draw::draw_text(&mut list, &f, &table, &t);
    list.build();
    let mut x = xs(&list);
    x.sort_by(f32::total_cmp);
    x
}

#[test]
fn text_skips_missing_and_out_of_range_glyphs() {
    // 'A' draws, 'B' has no glyph, 'C' draws after 'A' advanced 20, 'G' is past `last`, NUL ends the string.
    let t = [65, 66, 67, 71, 0, 65];
    assert_eq!(text_xs(ALIGN_LEFT, &t), [100.0, 120.0]);
}

#[test]
fn text_alignment_uses_the_narrowed_length() {
    // The length counts three 'A' (60): 0x10041 narrows to 0x41 there but is skipped when drawing.
    let t = [65, 0x10041, 65];
    assert_eq!(text_xs(ALIGN_RIGHT, &t), [40.0, 60.0]);
    assert_eq!(text_xs(ALIGN_CENTER, &t), [70.0, 90.0]);
}

#[test]
fn child_clip_rect_intersects_and_clamps() {
    let parent = Rect2f {
        x: 10.0,
        y: 10.0,
        w: 100.0,
        h: 100.0,
    };
    let r = layout::child_clip_rect(&parent, v3(0.0, 0.0, 0.0), v2(50.0, 50.0));
    assert_eq!((r.x, r.y, r.w, r.h), (10.0, 10.0, 40.0, 40.0));
    let r = layout::child_clip_rect(&parent, v3(500.0, 500.0, 0.0), v2(5.0, 5.0));
    assert_eq!((r.w, r.h), (0.0, 0.0));
    let r = layout::child_clip_rect(&OPEN, v3(1.0, 2.0, 0.0), v2(-3.0, 4.0));
    assert_eq!((r.x, r.y, r.w, r.h), (1.0, 2.0, -3.0, 4.0));
}

#[test]
fn scissor_scales_and_flips() {
    let rect = Rect2f {
        x: 100.0,
        y: 50.0,
        w: 200.0,
        h: 100.0,
    };
    let t = Vector2l { x: 1600, y: 1200 };
    let (p, s) = layout::scissor(&rect, t, v2(800.0, 600.0), v2(0.0, 0.0), false);
    assert_eq!((p.x, p.y, s.x, s.y), (200, 100, 400, 200));
    let (p, _) = layout::scissor(&rect, t, v2(800.0, 600.0), v2(0.0, 0.0), true);
    assert_eq!(p.y, 1200 - 100 - 200);
    let huge = Rect2f {
        x: 1e30,
        y: f32::NAN,
        w: 1.0,
        h: 1.0,
    };
    let (p, _) = layout::scissor(&huge, t, v2(800.0, 600.0), v2(0.0, 0.0), false);
    assert_eq!((p.x, p.y), (i32::MAX, 0));
}

#[test]
fn tooltip_is_clamped_into_the_set() {
    let t = layout::tooltip(&layout::TooltipInput {
        mouse: v2(790.0, 590.0),
        mouse_z: 100.0,
        pointer_size: v2(10.0, 10.0),
        virtual_size: v2(800.0, 600.0),
        text_length: 500.0,
        font_height: 12.0,
        rows: 2,
    });
    assert_eq!((t.size.x, t.size.y), (320.0, 2.0 * 14.0 - 2.0 + 6.0));
    assert_eq!((t.pos.x, t.pos.y, t.pos.z), (480.0, 600.0 - t.size.y, 98.0));
    assert_eq!(
        (t.background_size.x, t.background_size.y),
        (318.0, t.size.y - 2.0)
    );
}

#[test]
fn bounds_popup_z_and_virtual_size() {
    let p = layout::position_inside_bounds(v3(-5.0, 590.0, 7.0), v2(100.0, 20.0), v2(800.0, 600.0));
    assert_eq!((p.x, p.y, p.z), (0.0, 580.0, 7.0));
    assert_eq!(layout::next_popup_z(20.0), 25.0);
    assert_eq!(layout::next_popup_z(495.0), 20.0);
    assert_eq!(
        layout::virtual_size(v2(800.0, 600.0), 2.0, false),
        v2(400.0, 300.0)
    );
    assert_eq!(
        layout::virtual_size(v2(800.0, 600.0), 2.0, true),
        v2(800.0, 600.0)
    );
}

#[test]
fn radix_order_matches_the_multiset_comparator() {
    use super::sort::{Key, Sorter};
    let mut seed = 0x2545_F491_4F6C_DD1Du64;
    let mut next = move |n: u64| {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        seed % n
    };
    let zs = [
        0.0f32,
        -0.0,
        1.0,
        -1.0,
        2.5,
        1e-30,
        -1e30,
        f32::INFINITY,
        f32::NEG_INFINITY,
    ];
    let mut sorter = Sorter::default();
    for round in 0..300 {
        let n = (next(200) + usize::from(round % 7 == 0) as u64 * 3000) as usize;
        let keys: Vec<Key> = (0..n)
            .map(|_| Key {
                z: zs[next(zs.len() as u64) as usize],
                clip: 0x100 * next(4) as usize,
                material: 0x20 * next(3) as usize,
                texture: 0x10 * next(5) as usize,
            })
            .collect();
        let mut expected: Vec<usize> = (0..n).collect();
        expected.sort_by(|&a, &b| {
            let (a, b) = (&keys[a], &keys[b]);
            a.z.partial_cmp(&b.z)
                .unwrap_or(std::cmp::Ordering::Equal)
                .then(b.clip.cmp(&a.clip))
                .then(b.material.cmp(&a.material))
                .then(b.texture.cmp(&a.texture))
        });
        let got: Vec<usize> = sorter.order(&keys).collect();
        assert_eq!(got, expected, "round {round}");
    }
}

#[test]
#[ignore = "timing"]
fn bench_build() {
    let h = host();
    let f = frame(0x100, OPEN);
    let gfx: Vec<_> = (0..40)
        .map(|i| fake(0x20 + 8 * (i % 5), 0x10 + 8 * (i % 7)))
        .collect();
    let mut list = RenderList::default();
    let mut total = std::time::Duration::ZERO;
    let n = 200;
    for _ in 0..n {
        list.clear();
        for i in 0..32000usize {
            let g = &gfx[(i * 7) % 40];
            draw_at(
                &mut list,
                &f,
                &h,
                g,
                v3(i as f32, 0.0, ((i * 13) % 8) as f32),
                v2(8.0, 8.0),
            );
        }
        let t = std::time::Instant::now();
        list.build();
        total += t.elapsed();
    }
    eprintln!("build 32000 quads: {:?}", total / n);
}
