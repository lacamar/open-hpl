//! Expected values were recorded from the original C++ (see the port's differential harness).

use std::sync::{Mutex, MutexGuard};

use super::bitmap::{self, Surface};
use super::font::{self, Glyphs};
use super::formats::{self, BGRA, LUMINANCE, RGB};
use super::packer::{RectPacker, reorganize_order};
use super::{scatter, stdsort, uv};
use crate::math::Rect2l;
use crate::types::{Vector2f, Vector2l, Vector3l};

/// The bitmap scratch pixels and libc `rand()` are process-wide.
static GLOBAL: Mutex<()> = Mutex::new(());

pub(super) fn global() -> MutexGuard<'static, ()> {
    GLOBAL.lock().unwrap_or_else(|e| e.into_inner())
}

fn v3(x: i32, y: i32, z: i32) -> Vector3l {
    Vector3l { x, y, z }
}

fn surface(x: i32, y: i32, format: i32) -> (Surface, Vec<u8>) {
    let bpp = formats::channels(format);
    let s = Surface {
        size: v3(x, y, 1),
        format,
        bytes_per_pixel: bpp,
    };
    (s, vec![0; (x * y * bpp) as usize])
}

#[test]
fn format_tables() {
    let cases = [
        (0, 0, 0),
        (5, 4, 4),
        (8, 4, 1),
        (14, 1, 2),
        (21, 4, 8),
        (26, 4, 16),
        (27, 1, 1),
        (28, 3, 1),
        (29, 4, 1),
        (30, 0, 0),
        (-1, 0, 0),
    ];
    for (f, ch, bytes) in cases {
        assert_eq!(formats::channels(f), ch, "channels {f}");
        assert_eq!(formats::bytes_per_pixel(f), bytes, "bytes {f}");
    }
    assert!(
        formats::is_compressed(13) && formats::is_compressed(29) && !formats::is_compressed(14)
    );
    assert!(formats::is_depth(15) && !formats::is_depth(17));
    assert!(
        formats::is_floating_point(17)
            && formats::is_floating_point(26)
            && !formats::is_floating_point(27)
    );
    assert_eq!(formats::vertex_element_flag(4), 0x20);
    assert_eq!(formats::vertex_element_flag(14), 0);
    assert_eq!(formats::vertex_element_texture_unit(7), 2);
    assert_eq!(formats::vertex_element_texture_unit(0), -1);
    assert_eq!(formats::vertex_format_byte_size(2), 1);
}

#[test]
fn clear_and_blit_follow_the_cpp_including_stale_scratch_pixels() {
    let _g = global();
    let src_data: Vec<u8> = (10..28).collect();
    let src = Surface {
        size: v3(3, 2, 1),
        format: RGB,
        bytes_per_pixel: 3,
    };
    let (dst, mut d) = surface(4, 3, BGRA);

    bitmap::clear(&mut d, 12, 4, BGRA, [255, 128, 0, 64]);
    let cleared = [0, 128, 255, 64].repeat(12);
    assert_eq!(d, cleared);

    bitmap::blit(
        &dst,
        &mut d,
        &src,
        &src_data,
        v3(1, 1, 0),
        v3(3, 2, 1),
        v3(0, 0, 0),
    );
    #[rustfmt::skip]
    let expected = [
        0, 128, 255, 64, 0, 128, 255, 64, 0, 128, 255, 64, 0, 128, 255, 64,
        0, 128, 255, 64, 12, 11, 10, 255, 15, 14, 13, 255, 18, 17, 16, 255,
        0, 128, 255, 64, 21, 20, 19, 255, 24, 23, 22, 255, 27, 26, 25, 255,
    ];
    assert_eq!(d, expected);

    bitmap::blit(
        &dst,
        &mut d,
        &src,
        &src_data,
        v3(-1, 0, 0),
        v3(3, 1, 1),
        v3(1, 1, 0),
    );
    assert_eq!(d.get(..4), Some(&[27, 26, 25, 255][..]));
    assert_eq!(d.get(4..), expected.get(4..));

    let (lum, mut l) = surface(2, 1, LUMINANCE);
    bitmap::clear(&mut l, 2, 1, LUMINANCE, [51, 102, 153, 204]);
    assert_eq!(l, [51, 51]);

    // DXT1 has no conversion case: the destination receives the last converted pixel.
    let (dxt, mut x) = surface(2, 1, 8);
    bitmap::blit(
        &dxt,
        &mut x,
        &src,
        &src_data,
        v3(0, 0, 0),
        v3(2, 1, 1),
        v3(0, 0, 0),
    );
    assert_eq!(x, [51, 26, 25, 255, 51, 26, 25, 255]);
    let _ = lum;
}

#[test]
fn blit_stops_instead_of_overrunning_the_buffer() {
    let _g = global();
    let (src, sd) = surface(2, 2, 5);
    let (dst, mut dd) = surface(2, 2, 5);
    dd.truncate(8);
    bitmap::blit(
        &dst,
        &mut dd,
        &src,
        &sd,
        v3(0, 0, 0),
        v3(2, 2, 1),
        v3(0, 0, 0),
    );
    assert_eq!(dd.len(), 8);
}

#[test]
fn packer_places_like_the_cpp_tree() {
    let mut p = RectPacker::new(64, 64);
    let r = |x, y, w, h| Some(Rect2l { x, y, w, h });
    let seq: [((i32, i32), Option<Rect2l>); 12] = [
        ((10, 10), r(0, 0, 12, 12)),
        ((20, 5), r(12, 0, 22, 7)),
        ((30, 30), r(0, 12, 32, 32)),
        ((1, 1), r(34, 0, 3, 3)),
        ((25, 25), r(32, 12, 27, 27)),
        ((60, 60), None),
        ((28, 5), r(0, 44, 30, 7)),
        ((3, 3), r(12, 7, 5, 5)),
        ((5, 40), None),
        ((12, 12), None),
        ((12, 12), None),
        ((2, 2), r(34, 3, 4, 4)),
    ];
    for ((w, h), want) in seq {
        assert_eq!(p.insert(w, h).rect, want, "{w}x{h}");
    }
}

#[test]
fn packer_minimum_fit_takes_the_whole_leaf() {
    let mut p = RectPacker::new(32, 32);
    let first = p.insert(27, 27);
    assert_eq!(
        first.rect,
        Some(Rect2l {
            x: 0,
            y: 0,
            w: 29,
            h: 29
        })
    );
    assert_eq!(p.nodes_in_order().len(), 1);
    let second = p.insert(1, 1);
    assert_eq!((second.rect, second.found_empty), (None, false));
    assert_eq!(p.insert(28, 28).rect, None);
}

#[test]
fn packer_tree_shape_after_a_split() {
    let mut p = RectPacker::new(64, 64);
    p.insert(10, 10);
    let handles: Vec<i32> = p.nodes_in_order().iter().map(|n| n.handle).collect();
    assert_eq!(handles, [2, -2, -4, -1, -3]);
    let leaves: Vec<i32> = p.leaf_nodes().iter().map(|n| n.handle).collect();
    assert_eq!(leaves, [2, -4, -3]);
    p.clear(8, 8);
    assert_eq!(p.nodes_in_order().len(), 1);
}

#[test]
fn reorganize_order_is_largest_side_then_area_stable() {
    let sizes = [(5, 5), (10, 2), (3, 10), (10, 2), (7, 7)];
    assert_eq!(reorganize_order(&sizes), [2, 1, 3, 4, 0]);
    assert!(reorganize_order(&[]).is_empty());
}

#[test]
fn stdsort_orders_ties_like_libstdcxx() {
    #[derive(Clone, Copy, Default)]
    struct P {
        key: i32,
        id: i32,
    }
    let ids = |n: i32, m: i32, k: i32| {
        let mut v: Vec<P> = (0..n)
            .map(|i| P {
                key: (i * m) % k,
                id: i,
            })
            .collect();
        stdsort::sort(&mut v, |a, b| a.key > b.key);
        v.iter().map(|p| p.id).collect::<Vec<_>>()
    };
    assert_eq!(
        ids(60, 7, 5),
        [
            2, 57, 37, 22, 7, 52, 27, 42, 12, 47, 17, 32, 34, 24, 39, 19, 44, 14, 49, 29, 9, 54, 4,
            59, 1, 31, 36, 41, 46, 51, 56, 26, 11, 6, 21, 16, 28, 8, 53, 48, 13, 43, 18, 3, 38, 23,
            58, 33, 5, 55, 30, 0, 10, 50, 15, 45, 20, 40, 25, 35
        ]
    );
    assert_eq!(ids(12, 3, 4), [1, 5, 9, 2, 6, 10, 3, 7, 11, 0, 4, 8]);
    let mut empty: Vec<P> = Vec::new();
    stdsort::sort(&mut empty, |a, b| a.key > b.key);
}

#[test]
fn stdsort_sorts_large_adversarial_inputs() {
    let mut v: Vec<u32> = (0..5000).map(|i| (i * 7919) % 101).collect();
    stdsort::sort(&mut v, |a, b| a < b);
    assert!(v.windows(2).all(|w| w[0] <= w[1]));
    let mut desc: Vec<u32> = (0..3000).rev().collect();
    stdsort::sort(&mut desc, |a, b| a < b);
    assert!(desc.windows(2).all(|w| w[0] <= w[1]));
}

#[test]
fn scatter_disk_matches_the_recorded_stream() {
    let _g = global();
    let unsorted = [
        118, 172, 72, 90, 182, 144, 108, 158, 164, 189, 68, 154, 114, 169, 121, 135, 159, 121, 202,
        190, 112, 63, 209, 155, 162, 73, 195, 202, 135, 110, 218, 176, 45, 168, 170, 44, 72, 181,
        208, 127, 50, 165, 221, 87, 60, 52, 130, 36, 211, 208, 54, 25, 186, 236, 19, 101, 252, 145,
        21, 121, 213, 214, 7, 124,
    ];
    let sorted = [
        54, 25, 211, 208, 186, 236, 19, 101, 252, 145, 21, 121, 213, 214, 7, 124, 202, 190, 170,
        44, 209, 155, 208, 127, 221, 87, 195, 202, 218, 176, 60, 52, 45, 168, 72, 90, 72, 181, 112,
        63, 50, 165, 164, 189, 130, 36, 114, 169, 118, 172, 159, 121, 182, 144, 108, 158, 68, 154,
        162, 73, 135, 110, 121, 135,
    ];
    for (sort, want) in [(false, unsorted), (true, sorted)] {
        crate::math::c_srand(1);
        let mut out = vec![0u8; scatter::data_len(2, 8).unwrap_or(0)];
        assert!(scatter::generate(2, 8, sort, &mut out));
        assert_eq!(out, want, "sort={sort}");
        assert_eq!(crate::math::c_rand(), 939_819_582);
    }
}

#[test]
fn scatter_disk_rejects_unusable_arguments() {
    let mut out = vec![0u8; 4];
    assert!(!scatter::generate(0, 8, false, &mut out));
    assert!(!scatter::generate(2, 0, false, &mut out));
    assert!(!scatter::generate(2, 8, false, &mut out));
    assert_eq!(scatter::data_len(1, 1), Some(0));
    assert_eq!(scatter::data_len(-1, 8), None);
}

#[repr(C)]
struct FakeGlyph {
    pad: u32,
    advance: f32,
}

fn with_glyphs<R>(first: u16, adv: &[f32], f: impl FnOnce(&Glyphs) -> R) -> R {
    let boxed: Vec<FakeGlyph> = adv
        .iter()
        .map(|&advance| FakeGlyph { pad: 0, advance })
        .collect();
    let ptrs: Vec<*const u8> = boxed
        .iter()
        .map(|g| {
            if g.advance.is_nan() {
                std::ptr::null()
            } else {
                std::ptr::from_ref(g).cast()
            }
        })
        .collect();
    let g = Glyphs {
        glyphs: ptrs.as_ptr(),
        count: ptrs.len(),
        first,
        last: first + u16::try_from(adv.len()).unwrap_or(0).saturating_sub(1),
        advance_offset: std::mem::offset_of!(FakeGlyph, advance),
    };
    f(&g)
}

fn wide(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn rows(g: &Glyphs, max: f32, text: &[u32]) -> Option<Vec<String>> {
    let r = font::word_wrap_rows(g, max, 8.0, text)?;
    Some(
        r.into_iter()
            .map(|(a, l)| {
                text.get(a..a + l)
                    .unwrap_or(&[])
                    .iter()
                    .filter_map(|&c| char::from_u32(c))
                    .collect()
            })
            .collect(),
    )
}

#[test]
fn word_wrap_and_length_match_the_cpp() {
    let adv: Vec<f32> = (0..95).map(|i| 0.5 + (i % 4) as f32 * 0.25).collect();
    with_glyphs(32, &adv, |g| {
        let cases: [(&str, f32, &[&str], f32); 5] = [
            (
                "hello world this is a test",
                60.0,
                &["hello", "world", "this is a", "test"],
                154.0,
            ),
            ("a\nb c", 30.0, &["a", "b c"], 28.0),
            ("", 10.0, &[""], 0.0),
            ("  x  ", 3.5, &["", "", "x", "", ""], 20.0),
            (
                "one two three four five six seven",
                45.0,
                &["one", "two", "three", "four", "five", "six", "seven"],
                218.0,
            ),
        ];
        for (text, max, want, len) in cases {
            let t = wide(text);
            assert_eq!(
                rows(g, max, &t).as_deref(),
                Some(&want.iter().map(|s| s.to_string()).collect::<Vec<_>>()[..]),
                "{text:?}"
            );
            assert_eq!(g.length(8.0, &t), len, "{text:?}");
        }
        // empty text with a negative width: the C++ substr throws
        assert_eq!(font::word_wrap_rows(g, -1.0, 8.0, &[]), None);

        let cn: Vec<u32> = vec![97, 65292, 98, 99, 12290, 100];
        let r = font::word_wrap_rows(g, 8.0, 8.0, &cn).unwrap_or_default();
        let got: Vec<Vec<u32>> = r
            .iter()
            .map(|&(a, l)| cn.get(a..a + l).unwrap_or(&[]).to_vec())
            .collect();
        assert_eq!(got, [vec![97, 65292], vec![98, 99], vec![100]]);
    });
}

#[test]
fn length_narrows_chars_to_16_bits_and_stops_at_nul() {
    with_glyphs(65, &[1.0, 2.0, f32::NAN, 4.0], |g| {
        assert_eq!(g.length(2.0, &[65, 66, 0, 65]), 6.0);
        assert_eq!(g.length(2.0, &[65 + 65536, 67, 68, 69, 64]), 10.0);
        assert_eq!(g.length(1.0, &[]), 0.0);
    });
}

#[test]
fn subimage_uvs_contract_the_rect() {
    let u = uv::subimage_uvs(
        &Rect2l {
            x: 1,
            y: 1,
            w: 30,
            h: 20,
        },
        &Vector2l { x: 64, y: 64 },
    );
    let f = |a: f32, b: f32| Vector2f { x: a, y: b };
    assert_eq!(
        u,
        [
            f(0.016_625, 0.016_625),
            f(0.483_375, 0.016_625),
            f(0.483_375, 0.327_125),
            f(0.016_625, 0.327_125)
        ]
    );
}

#[test]
fn glyph_metrics_scale_by_font_size_and_ratio() {
    let m = uv::glyph_metrics(
        &Vector2l { x: 2, y: 4 },
        &Vector2l { x: 8, y: 16 },
        &Vector2l { x: 16, y: 32 },
        12,
        &Vector2f { x: 1.0, y: 2.0 },
    );
    assert_eq!(
        (m.offset.x, m.offset.y, m.size.x, m.size.y, m.advance),
        (0.125, 0.25, 0.5, 1.0, 0.75)
    );
}
