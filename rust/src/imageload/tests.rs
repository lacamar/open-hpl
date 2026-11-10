use super::dds::{self, BlockPlan};
use super::theora::{self, Planes};
use super::{devil, fontdata};

fn fnv(bytes: impl IntoIterator<Item = u8>) -> u64 {
    bytes.into_iter().fold(1_469_598_103_934_665_603, |h, b| {
        (h ^ u64::from(b)).wrapping_mul(0x0000_0100_0000_01b3)
    })
}

#[test]
fn yuv_tables_match_original_cpp() {
    let t = theora::build_tables();
    assert_eq!(fnv(t.to_r.iter().copied()), 0x755f_e380_8995_5fe3);
    assert_eq!(fnv(t.to_b.iter().copied()), 0x8fb1_d85f_a1ad_fd45);
    assert_eq!(
        fnv(t.g_uv.iter().flat_map(|v| v.to_le_bytes())),
        0x1e16_9611_61f4_0772
    );
    assert_eq!(fnv(t.g_y_uv.iter().copied()), 0x7c56_01ea_a3ab_06cd);
}

fn header(fourcc: &[u8; 4], w: u32, h: u32, mips: u32, caps2: u32) -> Vec<u8> {
    let mut v = vec![0u8; 148];
    v[..4].copy_from_slice(b"DDS ");
    v[12..16].copy_from_slice(&h.to_le_bytes());
    v[16..20].copy_from_slice(&w.to_le_bytes());
    v[28..32].copy_from_slice(&mips.to_le_bytes());
    v[84..88].copy_from_slice(fourcc);
    v[112..116].copy_from_slice(&caps2.to_le_bytes());
    v
}

#[test]
fn dxt1_chain_sizes() {
    let h = header(b"DXT1", 8, 8, 4, 0);
    // 8x8: 4 blocks * 8, 4x4: 8, 2x2: 8, 1x1: 8
    let need = 128 + 32 + 8 + 8 + 8;
    let p = dds::plan_block(&h[..128], need, false).unwrap();
    assert_eq!(
        (p.format, p.block_size, p.mips, p.images),
        (dds::FMT_DXT1, 8, 4, 1)
    );
    assert_eq!(p.mip_size(0), Some(32));
    assert_eq!(p.mip_size(3), Some(8));
    assert!(dds::plan_block(&h[..128], need - 1, false).is_none());
    assert!(dds::plan_block(&h[..128], need, true).is_none());
}

#[test]
fn cube_needs_all_faces() {
    let ok = header(b"DXT5", 4, 4, 0, 0x200 | 0xFE00);
    assert_eq!(dds::plan_block(&ok, 128 + 6 * 16, false).unwrap().images, 6);
    assert!(dds::plan_block(&header(b"DXT5", 4, 4, 0, 0x200), 4096, false).is_none());
}

#[test]
fn dx10_needs_extended_header() {
    let mut h = header(b"DX10", 4, 4, 1, 0);
    h[128..132].copy_from_slice(&98u32.to_le_bytes());
    let p = dds::plan_block(&h, 148 + 16, false).unwrap();
    assert_eq!((p.format, p.data_offset), (dds::FMT_BC7, 148));
    assert!(dds::plan_block(&h[..128], 4096, false).is_none());
}

#[test]
fn hostile_sizes_rejected_without_overflow() {
    for (w, h, m) in [(0x2000_0000, 4, 1), (4, 4, u32::MAX), (4, 4, 0x8000_0000)] {
        assert!(
            dds::plan_block(&header(b"DXT5", w, h, m, 0), 1 << 20, false).is_none(),
            "{w} {h} {m}"
        );
    }
    let raw = BlockPlan {
        format: dds::FMT_RGBA,
        block_size: 0,
        bytes_per_pixel: 4,
        cube: false,
        width: u32::MAX,
        height: u32::MAX,
        mips: 1,
        images: 1,
        data_offset: 128,
    };
    assert!(raw.mip_size(0).is_some_and(|s| s >= 0));
}

#[test]
fn zero_dimension_allowed_like_original() {
    let p = dds::plan_block(&header(b"DXT1", 0, 0, 1, 0), 128, false).unwrap();
    assert_eq!(p.mip_size(0), Some(0));
}

fn alpha_header(w: u32, h: u32, pitch: u32, flags: u32) -> Vec<u8> {
    let mut v = vec![0u8; 128];
    v[..4].copy_from_slice(b"DDS ");
    v[8..12].copy_from_slice(&flags.to_le_bytes());
    v[12..16].copy_from_slice(&h.to_le_bytes());
    v[16..20].copy_from_slice(&w.to_le_bytes());
    v[20..24].copy_from_slice(&pitch.to_le_bytes());
    v[80..84].copy_from_slice(&1u32.to_le_bytes());
    v[88..92].copy_from_slice(&8u32.to_le_bytes());
    v
}

#[test]
fn alpha_plan_rows() {
    let p = dds::plan_alpha(&alpha_header(4, 3, 8, 8), 128 + 2 * 8 + 4).unwrap();
    assert_eq!((p.width, p.height, p.row_bytes), (4, 3, 8));
    assert!(dds::plan_alpha(&alpha_header(4, 3, 8, 8), 128 + 2 * 8 + 3).is_none());
    assert_eq!(
        dds::plan_alpha(&alpha_header(4, 3, 8, 0), 128 + 12)
            .unwrap()
            .row_bytes,
        4
    );
    assert!(dds::plan_alpha(&alpha_header(0, 3, 0, 0), 4096).is_none());
    assert!(dds::plan_alpha(&alpha_header(65536, 65536, 0, 0), u64::MAX).is_none());
    assert!(dds::plan_alpha(&alpha_header(2, u32::MAX, u32::MAX, 8), u64::MAX).is_none());
}

#[test]
fn devil_maps() {
    assert_eq!(devil::pixel_format_to_hpl(devil::IL_BGRA), 7);
    assert_eq!(devil::pixel_format_to_hpl(devil::IL_LUMINANCE), 1);
    assert_eq!(devil::pixel_format_to_hpl(1), 0);
    assert_eq!(devil::pixel_format_from_hpl(2), devil::IL_LUMINANCE);
    assert_eq!(devil::pixel_format_from_hpl(1), 0);
    assert_eq!(devil::dxt_to_hpl(devil::IL_DXT5), dds::FMT_DXT5);
    assert_eq!(devil::dxt_to_hpl(devil::IL_DXT_NO_COMP), 0);
    let ext = |s: &str| s.chars().map(u32::from).collect::<Vec<_>>();
    assert_eq!(devil::type_from_extension(&ext("jpeg")), devil::IL_JPG);
    assert_eq!(devil::type_from_extension(&ext("tga")), devil::IL_TGA);
    assert_eq!(devil::type_from_extension(&ext("")), 0);
    assert_eq!(devil::type_from_extension(&ext("pngg")), 0);
    assert_eq!(devil::type_from_extension(&ext("PNG")), 0);
    let l = devil::dds_layout(0, 0, 0);
    assert_eq!((l.images, l.mips, l.setup_data), (1, 1, false));
    assert!(devil::dds_layout(0, 4, 0).setup_data);
    assert_eq!(devil::dds_layout(0, 0, 0xfe00).images, 6);
    assert_eq!(devil::dds_layout(i32::MAX, 0, 0).images, i32::MIN);
    assert_eq!(
        (devil::misc_image_count(0), devil::misc_image_count(3)),
        (1, 3)
    );
}

fn gray_planes(w: usize, h: usize) -> (Vec<u8>, Vec<u8>, Vec<u8>) {
    (
        (0..w * h).map(|i| (i * 7 % 256) as u8).collect(),
        (0..w * h / 4 + w).map(|i| (i * 3 % 256) as u8).collect(),
        (0..w * h / 4 + w).map(|i| (i * 5 % 256) as u8).collect(),
    )
}

#[test]
fn draw_frame_even_and_odd_sizes() {
    let t = theora::build_tables();
    for (w, h) in [(4usize, 4usize), (6, 2), (5, 4), (4, 5), (7, 7), (2, 2)] {
        let (y, u, v) = gray_planes(w + 2, h + 2);
        let planes = Planes {
            y: &y,
            u: &u,
            v: &v,
            y_stride: w + 2,
            uv_stride: (w + 2) / 2,
            offset_x: 0,
            offset_y: 0,
        };
        let mut out = vec![0u8; w * h * 3];
        assert!(
            theora::draw_frame(&mut out, &planes, &t.as_ref(), w as i32, h as i32),
            "{w}x{h}"
        );
    }
}

#[test]
fn draw_frame_rejects_short_planes_and_degenerate_sizes() {
    let t = theora::build_tables();
    let (y, u, v) = gray_planes(4, 4);
    let planes = Planes {
        y: &y[..8],
        u: &u,
        v: &v,
        y_stride: 4,
        uv_stride: 2,
        offset_x: 0,
        offset_y: 0,
    };
    let mut out = vec![0u8; 48];
    assert!(!theora::draw_frame(&mut out, &planes, &t.as_ref(), 4, 4));
    let planes = Planes {
        y: &y,
        u: &u,
        v: &v,
        y_stride: 4,
        uv_stride: 2,
        offset_x: 0,
        offset_y: 0,
    };
    assert!(!theora::draw_frame(
        &mut out[..10],
        &planes,
        &t.as_ref(),
        4,
        4
    ));
    assert!(theora::draw_frame(&mut out, &planes, &t.as_ref(), 0, 4));
    assert!(theora::draw_frame(&mut out, &planes, &t.as_ref(), 1, 4));
    assert!(!theora::draw_frame(&mut out, &planes, &t.as_ref(), -1, 4));
    assert_eq!(theora::frame_buffer_len(640, 360), 691_200);
    assert_eq!(theora::frame_buffer_len(i32::MAX, 3), 0);
    assert_eq!(theora::frame_buffer_len(-1, 3), 0);
}

#[test]
fn font_header_math() {
    let h = fontdata::header(1, 30, 24);
    assert_eq!((h.line_height, h.base, h.height), (32, 26, 32.0));
    assert_eq!(h.size_ratio_x, 26.0 / 32.0);
    assert!(fontdata::header(0, 0, 5).size_ratio_x.is_infinite());
    assert_eq!(fontdata::header(i32::MAX, 0, 0).line_height, -2);
    assert_eq!(fontdata::glyph_count(-1), Some(0));
    assert_eq!(fontdata::glyph_count(65), Some(66));
    assert_eq!(fontdata::glyph_count(i32::MAX), None);
    assert!(
        fontdata::glyph_id_valid(65, 66)
            && !fontdata::glyph_id_valid(66, 66)
            && !fontdata::glyph_id_valid(-1, 66)
    );
    assert!(
        fontdata::page_valid(0, 1) && !fontdata::page_valid(1, 1) && !fontdata::page_valid(-1, 1)
    );
    assert_eq!(fontdata::glyph_advance(10, 2), 12);
}
