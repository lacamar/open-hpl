use super::fbo::{self, DS_DEPTH, DS_ERR_NO_PACKED, DS_PACKED, DrawPlan};
use super::glsl::{self, AsId, Params};
use super::state::{self, GlState};
use super::tables;
use super::texture::{self, Clock, CreateIn};
use super::vbuf::{self, Elem, Op};
use super::{c, texture::TYPE_2D};
use crate::types::{Matrixf, Vector2l, Vector3f, Vector3l};

fn v2(x: i32, y: i32) -> Vector2l {
    Vector2l { x, y }
}

fn v3(x: i32, y: i32, z: i32) -> Vector3l {
    Vector3l { x, y, z }
}

#[test]
fn tables_match_gl_values() {
    assert_eq!(tables::blend(6), 0x0302);
    assert_eq!(tables::blend(99), 0);
    assert_eq!(tables::compare(2), 0x0203);
    assert_eq!(tables::texture_target(2), 0x84F5);
    assert_eq!(tables::wrap(99), c::REPEAT);
    assert_eq!(tables::vertex_array(10), 0);
    assert_eq!(tables::draw_mode(3), c::QUADS);
    assert_eq!(tables::pixel_format(8), 0);
    assert_eq!(tables::pixel_internal_format(14), c::DEPTH_COMPONENT16);
    assert_eq!(tables::compression_format(27), c::COMPRESSED_RED_RGTC1);
    assert_eq!(tables::srgb(c::RGBA), c::SRGB8_ALPHA8);
    assert_eq!(tables::srgb(c::ALPHA), c::ALPHA);
    assert_eq!(tables::usage(2), c::STREAM_DRAW_ARB);
    assert_eq!(tables::shader_type(0), c::VERTEX_SHADER);
    assert_eq!(tables::pixel_format(-1), 0);
}

#[test]
fn state_filters_skip_repeats() {
    let mut s = GlState::new();
    s.setup();
    assert!(!s.set_color_write(true, true, true, true));
    assert!(s.set_color_write(true, false, true, true));
    assert!(!s.set_depth_write(true));
    assert!(s.set_depth_test(false));
    assert!(!s.set_depth_test(false));
    assert!(!s.set_depth_func(2));
    assert!(s.set_alpha_func(4, 0.5));
    assert!(!s.set_alpha_func(4, 0.5));
    assert!(s.set_active_unit(3));
    assert!(!s.set_active_unit(3));
}

#[test]
fn set_texture_sequences() {
    let mut s = GlState::new();
    let p = s.set_texture(1, c::TEXTURE_2D, true, true);
    assert_eq!(
        (p.active_unit, p.disable, p.bind, p.enable),
        (1, 0, true, c::TEXTURE_2D)
    );
    let p = s.set_texture(1, c::TEXTURE_3D, true, true);
    assert_eq!(p.active_unit, -1);
    assert_eq!((p.disable, p.enable), (c::TEXTURE_2D, c::TEXTURE_3D));
    let p = s.set_texture(1, 0, false, true);
    assert_eq!((p.disable, p.bind, p.enable), (c::TEXTURE_3D, false, 0));
    let p = s.set_texture(9, c::TEXTURE_2D, true, true);
    assert_eq!((p.disable, p.enable, p.bind), (0, 0, true));
    let p = s.set_texture(40, c::TEXTURE_2D, true, false);
    assert_eq!((p.active_unit, p.enable), (-1, 0));
}

#[test]
fn rect_arithmetic() {
    assert_eq!(
        state::scissor_rect(100, v2(10, 20), v2(30, 40)),
        [10, 40, 30, 40]
    );
    let vp = state::viewport(v2(640, 480), v2(5, 6), v2(-1, 10));
    assert_eq!((vp.size, vp.gl), (v2(640, 480), [5, -6, 640, 480]));
    assert_eq!(
        state::copy_to_texture(
            v2(1, 2),
            v2(100, 80),
            v2(100, 80),
            v2(3, 4),
            v2(0, 0),
            v2(7, 8),
            64
        ),
        [7, 64 - 80 - 8, 4, 80 - 80 - 6, 100, 80]
    );
    let mut out = [0u32; 3];
    assert_eq!(state::draw_targets(&[0, 2, 4, 1], &mut out), 3);
    assert_eq!(
        out,
        [
            c::COLOR_ATTACHMENT0_EXT,
            c::COLOR_ATTACHMENT0_EXT + 2,
            c::COLOR_ATTACHMENT0_EXT + 4
        ]
    );
}

fn elems() -> [Elem; 3] {
    [
        Elem {
            kind: 1,
            format: 0,
            element_num: 3,
            handle: 7,
        },
        Elem {
            kind: 0,
            format: 0,
            element_num: 3,
            handle: 8,
        },
        Elem {
            kind: 6,
            format: 0,
            element_num: 4,
            handle: 9,
        },
    ]
}

#[test]
fn vbo_bind_orders_position_last() {
    let mut ops = [Op::default(); vbuf::MAX_OPS];
    let n = vbuf::bind_vbo(&elems(), &mut ops);
    let kinds: Vec<(u32, u32)> = ops[..n].iter().map(|o| (o.kind, o.array)).collect();
    let tex = c::TEXTURE0_ARB + 2;
    assert_eq!(kinds[0], (vbuf::OP_ENABLE, c::NORMAL_ARRAY));
    assert_eq!(kinds[3], (vbuf::OP_CLIENT_ACTIVE, tex - 1));
    assert_eq!(kinds[n - 5], (vbuf::OP_CLIENT_ACTIVE, c::TEXTURE0_ARB));
    assert_eq!(kinds[n - 4], (vbuf::OP_ENABLE, c::VERTEX_ARRAY));
    assert_eq!(kinds[n - 1], (vbuf::OP_BIND_BUFFER, 0));
    assert_eq!(ops[n - 2].src, vbuf::SRC_OFFSET);
}

#[test]
fn array_bind_and_unbind() {
    let mut ops = [Op::default(); vbuf::MAX_OPS];
    let n = vbuf::bind_array(&elems(), &mut ops);
    let ptrs: Vec<u32> = ops[..n]
        .iter()
        .filter(|o| o.kind == vbuf::OP_POINTER)
        .map(|o| o.index)
        .collect();
    assert_eq!(ptrs, [0, 1, 2]);
    assert_eq!(ops[n - 1].array, c::TEXTURE0_ARB);
    let n = vbuf::unbind(&elems(), &mut ops);
    assert_eq!(n, 1 + 1 + 3 + 1);
    assert_eq!(ops[0].kind, vbuf::OP_BIND_BUFFER);
}

#[test]
fn batch_plans() {
    let mut ops = [Op::default(); vbuf::MAX_OPS];
    let n = vbuf::batch_states(vbuf::BATCH_POSITION | vbuf::BATCH_TEXTURE0 << 1, &mut ops);
    assert_eq!(n, 9);
    assert_eq!(ops[0].kind, vbuf::OP_ENABLE);
    assert_eq!(ops[1].kind, vbuf::OP_DISABLE);
    assert_eq!(ops[4].kind, vbuf::OP_DISABLE);
    assert_eq!(ops[6].kind, vbuf::OP_ENABLE);
    assert_eq!(vbuf::batch_arrays(52, &mut ops), 9);
    assert_eq!(vbuf::batch_texture_unit(2, true, 52, &mut ops), 2);
    assert_eq!(ops[1].src, vbuf::SRC_BATCH_TEXCOORD);
}

#[test]
fn vertex_helpers() {
    let mut out = [0.0; 4];
    let v = Vector3f {
        x: 1.0,
        y: 2.0,
        z: 3.0,
    };
    assert_eq!(vbuf::vec3_components(2, v, &mut out), 2);
    assert_eq!(vbuf::vec3_components(4, v, &mut out), 4);
    assert_eq!(out, [1.0, 2.0, 3.0, 1.0]);
    assert_eq!(vbuf::vertex_num(12, 0, false), 0);
    assert_eq!(vbuf::vertex_num(24, 4, true), 3);
    assert_eq!(vbuf::copy_flags(vbuf::FLAG_ALL, 5), 5);
    assert_eq!(vbuf::bounding_check(0, 0), 1);
    assert_eq!(vbuf::bounding_check(2, 1), 2);
    assert_eq!(vbuf::draw_mode(0, 8), c::TRIANGLES);
    assert_eq!(vbuf::draw_count(-1, 9), 9);
}

#[test]
fn shadow_double_copies_positions() {
    let size = 8;
    let extra = vbuf::shadow_double_extra(size);
    let mut data = vec![0u32; size + extra];
    for (i, d) in data.iter_mut().take(size).enumerate() {
        *d = 10 + i as u32;
    }
    vbuf::shadow_double_fill(0, &mut data, size);
    assert_eq!(&data[8..], &[10, 11, 12, 0, 14, 15, 16, 0]);
}

#[test]
fn transform_moves_positions_and_shadow() {
    let mut m = Matrixf::default();
    for i in 0..4 {
        m.m[i][i] = 1.0;
    }
    m.m[0][3] = 5.0;
    let mut pos = vec![
        1.0, 2.0, 3.0, 1.0, 4.0, 5.0, 6.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
    ];
    let mut normal = vec![0.0, 0.0, 2.0, 0.0, 3.0, 0.0];
    let flags = vbuf::transform(&m, &mut pos, Some(&mut normal), None, 2, 4, true);
    assert_eq!(flags, vbuf::FLAG_POSITION | vbuf::FLAG_NORMAL);
    assert_eq!(&pos[..3], &[6.0, 2.0, 3.0]);
    assert_eq!(&pos[8..11], &[6.0, 2.0, 3.0]);
    assert_eq!(&normal[..3], &[0.0, 0.0, 1.0]);
}

#[test]
fn create_plan_downscales() {
    let i = CreateIn {
        tex_type: TYPE_2D,
        format: 5,
        size: v3(256, 128, 1),
        min_size: v3(16, 16, 16),
        num_mips: 1,
        down_level: 2,
        generate_mips: true,
        check_resize: true,
    };
    let p = texture::plan_create(&i);
    assert!(p.resize && p.build_mips && !p.warn_pow2);
    assert_eq!((p.size.x, p.size.y, p.resize_bytes), (64, 32, 64 * 32 * 4));
    let p = texture::plan_create(&CreateIn { num_mips: 9, ..i });
    assert!(!p.resize && p.start_level == 2 && p.end_level == 9 && p.max_level == 6);
    assert_eq!((p.size.x, p.size.y), (64, 32));
    assert_eq!(texture::level_size(v3(4, 1, 1), 5), v3(1, 1, 1));
    assert!(
        texture::plan_create(&CreateIn {
            size: v3(100, 64, 1),
            ..i
        })
        .warn_pow2
    );
}

#[test]
fn upload_and_mip_plans() {
    let u = texture::plan_upload(3, false, 8, 2, 64, v3(4, 4, 1), 5);
    assert_eq!(
        (u.kind, u.target),
        (2, c::TEXTURE_CUBE_MAP_POSITIVE_X_ARB + 5)
    );
    let u = texture::plan_upload(1, true, 13, 0, 64, v3(4, 4, 1), 0);
    assert!(u.swizzle && u.internal == c::COMPRESSED_RG_RGTC2);
    let u = texture::plan_upload(1, true, 5, 0, 64, v3(4, 4, 1), 0);
    assert_eq!(
        (u.kind, u.internal, u.gl_type),
        (5, c::SRGB8_ALPHA8, c::UNSIGNED_BYTE)
    );
    assert_eq!(
        texture::plan_upload(2, false, 8, 0, 8, v3(4, 4, 1), 0).kind,
        0
    );
    let m = texture::plan_mips(1, 0, true, false, 5, v3(4, 4, 1), 0);
    assert_eq!((m.action, m.memory), (texture::MIP_BUILD_2D, 2 * 2 * 4 + 4));
    assert!(texture::plan_mips(1, 0, true, false, 8, v3(4, 4, 1), 0).disable_mips);
    assert_eq!(
        texture::plan_mips(1, 1, true, false, 5, v3(4, 4, 1), 0).action,
        texture::MIP_GENERATE
    );
}

#[test]
fn property_and_filter_plans() {
    let p = texture::plan_props(2, true, 1, [0, 0, 0]);
    assert!(p.rect && p.min_filter == c::LINEAR && p.wrap == [c::CLAMP_TO_EDGE; 3]);
    let p = texture::plan_props(1, true, 1, [0, 1, 2]);
    assert_eq!(p.min_filter, c::LINEAR_MIPMAP_NEAREST);
    assert_eq!(p.wrap, [c::REPEAT, c::CLAMP, c::CLAMP_TO_EDGE]);
    assert_eq!(
        texture::filter_params(2, true, 1),
        [c::LINEAR_MIPMAP_LINEAR, c::LINEAR]
    );
    assert_eq!(texture::filter_params(9, true, 1), [0, 0]);
    assert!(texture::aniso_apply(true, true, 16, 8.0, 1.0));
    assert!(!texture::aniso_apply(true, true, 16, 8.0, 8.0));
    assert!(!texture::aniso_apply(true, true, 4, 8.0, 1.0));
    assert!(texture::aniso_fits(16, 16.0));
}

#[test]
fn animation_clock() {
    let mut k = Clock {
        count: 0.0,
        dir: 1.0,
    };
    k.update(1, 0.5, 1.0, 1);
    assert_eq!(k.count, 0.0);
    k.update(4, 3.0, 1.0, 1);
    assert_eq!(k.count, 3.0);
    k.next_frame(4, 1);
    assert_eq!(k.count, 0.0);
    k.count = 3.0;
    k.next_frame(4, 2);
    assert_eq!((k.count, k.dir), (3.0, -1.0));
    k.next_frame(4, 2);
    k.prev_frame(4, 2);
    assert_eq!(k.dir, -1.0);
    assert_eq!(texture::handle_slot(true, 4, 2.7), Some(2));
    assert_eq!(texture::handle_slot(true, 4, 9.0), None);
    assert_eq!(texture::handle_slot(false, 4, 1.0), None);
    assert_eq!(texture::handle_slot(true, 1, 7.0), Some(0));
    let (s, bytes) = texture::raw_size(v3(0, 4, 2), 5);
    assert_eq!((s, bytes), (v3(1, 4, 2), 0));
}

#[test]
fn param_cache() {
    let mut p = Params::default();
    assert_eq!(p.add(b"a", 3), 0);
    assert_eq!(p.find(b"a"), Some(0));
    assert_eq!(p.location(0), Some(3));
    assert_eq!(p.location(1), None);
    assert_eq!(p.as_id_begin(b"a", 0), AsId::Done(true));
    assert_eq!(p.as_id_begin(b"a", 2), AsId::Done(false));
    assert_eq!(p.as_id_begin(b"b", -1), AsId::Done(false));
    assert_eq!(p.as_id_begin(b"b", 4), AsId::Lookup);
    assert!(p.as_id_finish(b"b", 4, 9));
    assert_eq!(
        (p.len(), p.location(2), p.location(4)),
        (5, Some(-1), Some(9))
    );
    assert!(p.as_id_finish(b"c", 4, 9));
    assert!(!p.as_id_finish(b"c", 4, 8));
    assert!(!p.as_id_finish(b"c", 4, -1));
    assert!(!p.as_id_finish(b"d", glsl::MAX_ID + 1, 1));
}

#[test]
fn program_filter_and_shader_text() {
    let mut cur = 0;
    assert!(glsl::bind(&mut cur, 5));
    assert!(!glsl::bind(&mut cur, 5));
    glsl::forget(&mut cur, 5);
    assert!(!glsl::unbind(&mut cur));
    let mut lines = Vec::new();
    glsl::code_lines(b"a\r\nb\n\nlast", |r, l| lines.push((r, l.to_vec())));
    assert_eq!(lines, [(1, b"a".to_vec()), (2, b"b".to_vec()), (3, vec![])]);
    assert_eq!(glsl::dump_name(3, b"x/y\\z", true), b"3_x_y_z.vert");
}

#[test]
fn framebuffer_plans() {
    assert_eq!(fbo::plan_depth_stencil(24, 8, true).kind, DS_PACKED);
    assert_eq!(fbo::plan_depth_stencil(24, 8, false).kind, DS_ERR_NO_PACKED);
    assert_eq!(fbo::plan_depth_stencil(16, 0, false).kind, DS_DEPTH);
    assert_eq!(
        fbo::plan_depth_stencil(0, 3, false).kind,
        fbo::DS_ERR_STENCIL
    );
    assert_eq!(fbo::plan_depth_stencil(0, 0, false).kind, fbo::DS_NONE);
    assert!(fbo::entry_accepts(0, 2) && !fbo::entry_accepts(1, 1) && fbo::entry_accepts(2, 3));
    let a = fbo::plan_attach(c::COLOR_ATTACHMENT0_EXT, 2, 3, 1, 4, v2(64, 1));
    assert_eq!(a.target, c::TEXTURE_CUBE_MAP_POSITIVE_X_ARB + 4);
    assert_eq!(
        (a.attachment, a.color_index, a.first_size, a.explicit_mip),
        (c::COLOR_ATTACHMENT0_EXT + 2, 2, v2(32, 1), true)
    );
    assert_eq!(
        fbo::plan_attach(c::COLOR_ATTACHMENT0_EXT, 9, 1, 0, 0, v2(8, 8)).color_index,
        -1
    );
    let d: DrawPlan = fbo::plan_draw_buffers(0b10101);
    assert_eq!((d.mode, d.count), (fbo::DRAW_MULTI, 3));
    assert_eq!(
        d.buffers[..3],
        [
            c::COLOR_ATTACHMENT0_EXT,
            c::COLOR_ATTACHMENT0_EXT + 2,
            c::COLOR_ATTACHMENT0_EXT + 4
        ]
    );
    assert_eq!(fbo::plan_draw_buffers(0).mode, fbo::DRAW_NONE);
    assert_eq!(
        fbo::status_class(c::FRAMEBUFFER_COMPLETE_EXT),
        fbo::STATUS_COMPLETE
    );
    assert_eq!(fbo::status_class(1), fbo::STATUS_UNKNOWN);
}
