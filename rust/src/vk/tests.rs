use super::glsl_rewrite::{atoi, glsl_to_vulkan};
use super::tables::*;

fn rewrite(v: &str, f: &str) -> super::glsl_rewrite::Program {
    glsl_to_vulkan(v.as_bytes(), f.as_bytes()).expect("rewrite")
}

const VTX: &str = "#version 120\nuniform mat4 a_mtxUV; // comment\nuniform vec3 avA, avB[2];\nvarying vec3 gvNormal;\nattribute vec4 vtx_vBoneWeight;\ninvariant gl_Position;\nvoid main(){ gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex; gvNormal = gl_NormalMatrix * gl_Normal + avB[1];\n  gl_TexCoord[0] = a_mtxUV * gl_MultiTexCoord0; gl_FrontColor = gl_Color * vtx_vBoneWeight; }\n";
const FRAG: &str = "#version 120\n#extension GL_ARB_texture_rectangle : enable\nuniform sampler2D diffuseMap;\nuniform sampler2DRect aDepth;\nuniform sampler2DShadow aShadow;\nuniform vec3 avA;\nuniform float afFalloff = 0.5f;\nvarying vec3 gvNormal;\nvoid main(){ vec4 c = texture2D(diffuseMap, gl_TexCoord[0].xy) * gl_Color;\n  c += texture2DRect(aDepth, gl_FragCoord.xy) * shadow2D(aShadow, gvNormal).x;\n  gl_FragData[0] = c * afFalloff; gl_FragData[1] = vec4(avA, 1.0); }\n";

#[test]
fn sample_pair_metadata() {
    let p = rewrite(VTX, FRAG);
    assert_eq!(p.samplers.len(), 3);
    assert_eq!(p.samplers[1].ty, b"sampler2D");
    assert_eq!(p.samplers[1].binding, 2);
    assert_eq!(p.frag_outputs, 2);
    assert_eq!(p.attrib_mask, 1 | 2 | 4 | 16);
    assert_eq!(p.inits.len(), 1);
    assert_eq!(p.inits[0].values, [0.5]);
}

#[test]
fn sample_pair_text() {
    let p = rewrite(VTX, FRAG);
    let v = String::from_utf8(p.vertex).expect("utf8");
    let f = String::from_utf8(p.fragment).expect("utf8");
    assert!(v.starts_with("#version 450\nlayout(location=0) in vec4 hpl_Vertex;\n"));
    assert!(v.contains("const vec4 vtx_vBoneWeight = vec4(vec4(0.0, 0.0, 0.0, 1.0));"));
    assert!(v.contains("void hpl_main()"));
    assert!(v.contains("gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;"));
    assert!(f.contains("layout(location=0) out vec4 hpl_FragData[2];"));
    assert!(f.contains("hpl_textureRect(aDepth, gl_FragCoord.xy)"));
    assert!(f.contains("vec4 hpl_shadow2D(sampler2DShadow s, vec3 c)"));
    assert!(f.contains("layout(set=0, binding=2) uniform sampler2D aDepth;"));
}

#[test]
fn errors() {
    let e = glsl_to_vulkan(b"void main(){ gl_Fog = 1; }", b"").expect_err("builtin");
    assert_eq!(e, b"unsupported built-in gl_Fog");
    let e = glsl_to_vulkan(b"uniform vec3 a;\n", b"uniform vec4 a;\n").expect_err("mismatch");
    assert_eq!(e, b"uniform type mismatch: a");
    let e = glsl_to_vulkan(b"uniform float a b;\n", b"").expect_err("parse");
    assert_eq!(e, b"can't parse declaration: uniform float a b");
}

#[test]
fn precision_word_as_type() {
    let p = rewrite("uniform highp x;\nvoid main(){}", "void main(){}");
    let v = String::from_utf8(p.vertex).expect("utf8");
    assert!(v.contains("\thighp x;\n"));
}

#[test]
fn vec_constructor_digits_are_init_values() {
    let p = rewrite("uniform vec3 a = vec3(1.0, 2.0, 3.0);\nvoid main(){}", "");
    assert_eq!(p.inits[0].values, [3.0, 1.0, 2.0, 3.0]);
}

#[test]
fn atoi_matches_glibc() {
    assert_eq!(atoi(b"  42]"), 42);
    assert_eq!(atoi(b"-3]"), -3);
    assert_eq!(atoi(b"x"), 0);
    assert_eq!(atoi(b"99999999999999999999"), -1);
    assert_eq!(atoi(b"4294967297"), 1);
}

#[test]
fn enum_tables() {
    assert_eq!(
        (0..8).map(stencil_op).collect::<Vec<_>>(),
        (0..8).collect::<Vec<_>>()
    );
    assert_eq!(stencil_op(8), 0);
    assert_eq!(blend_factor(10), 14);
    assert_eq!(blend_factor(11), 1);
    assert_eq!([wrap(0), wrap(1), wrap(2), wrap(3)], [0, 2, 2, 3]);
}

#[test]
fn formats() {
    let f = vk_format(4, true, 0);
    assert_eq!(
        (f.format, f.convert),
        (vkf::R8G8B8A8_SRGB, CONVERT_RGB_TO_RGBA)
    );
    assert_eq!(vk_format(13, false, 0).format, vkf::BC5_UNORM);
    assert_eq!(vk_format(15, false, 99).format, 99);
    assert_eq!(vk_format(0, false, 0).format, vkf::UNDEFINED);
    assert_eq!(unorm_of_srgb(vkf::B8G8R8A8_SRGB), vkf::B8G8R8A8_UNORM);
    assert_eq!(format_size(vkf::R16G16B16A16_SFLOAT), 8);
}

#[test]
fn half_floats() {
    assert_eq!(float_to_half(1.0), 0x3C00);
    assert_eq!(float_to_half(-2.0), 0xC000);
    assert_eq!(float_to_half(65504.0), 0x7BFF);
    assert_eq!(float_to_half(1e10), 0x7C00);
    assert_eq!(float_to_half(f32::NAN), 0x7E00);
    assert_eq!(float_to_half(1e-9), 0);
}

#[test]
fn pixel_conversion() {
    let f = vk_format(4, false, 0);
    let mut dst = vec![0u8; convert_dst_len(&f, 2).expect("len")];
    convert_pixels(&f, &[1, 2, 3, 4, 5, 6], 2, &mut dst);
    assert_eq!(dst, [1, 2, 3, 255, 4, 5, 6, 255]);
    let f = vk_format(20, false, 0);
    assert_eq!(convert_dst_len(&f, 1), Some(8));
    let mut dst = [0u8; 8];
    let src: Vec<u8> = [0.5f32, 1.0, 2.0]
        .iter()
        .flat_map(|v| v.to_ne_bytes())
        .collect();
    convert_pixels(&f, &src, 1, &mut dst);
    let h: Vec<u16> = dst
        .chunks(2)
        .map(|c| u16::from_ne_bytes([c[0], c[1]]))
        .collect();
    assert_eq!(h, [0x3800, 0x3C00, 0x4000, 0x3C00]);
    assert_eq!(convert_dst_len(&f, usize::MAX), None);
}

#[test]
fn mips() {
    assert_eq!(full_mip_count(256, 64, 1), 9);
    assert_eq!(full_mip_count(1, 1, 1), 1);
    let plan = mip_plan(&MipIn {
        size: [256, 256, 1],
        num_mips: 1,
        downscale_level: 2,
        min_size: [16, 16, 16],
        use_mipmaps: true,
        tex2d: true,
        ..MipIn::default()
    });
    assert!(plan.cpu_resize);
    assert_eq!(
        (plan.halve_steps, plan.size, plan.levels),
        (2, [64, 64, 1], 7)
    );
    let plan = mip_plan(&MipIn {
        size: [256, 256, 1],
        num_mips: 9,
        downscale_level: 2,
        use_mipmaps: true,
        tex2d: true,
        ..MipIn::default()
    });
    assert_eq!(
        (plan.start, plan.size, plan.levels, plan.provided),
        (2, [64, 64, 1], 7, true)
    );
}

#[test]
fn halving_averages_2x2() {
    let src = [0u8, 4, 8, 12];
    let mut dst = [0u8; 1];
    halve_image(&src, (2, 2), 1, &mut dst);
    assert_eq!(dst, [6]);
}

#[test]
fn quad_indices() {
    assert_eq!(converted_len(DRAW_QUAD, 8), 12);
    let mut out = [0u32; 6];
    assert!(convert_indices(DRAW_QUAD, &[10, 11, 12, 13], &mut out));
    assert_eq!(out, [10, 11, 12, 10, 12, 13]);
    let mut out = [0u32; 6];
    assert!(convert_indices(DRAW_QUAD_STRIP, &[0, 1, 2, 3], &mut out));
    assert_eq!(out, [0, 1, 3, 0, 3, 2]);
    let mut out = [0u32; 4];
    assert!(convert_indices(DRAW_LINE_LOOP, &[7, 8, 9], &mut out));
    assert_eq!(out, [7, 8, 9, 7]);
    assert!(!convert_indices(DRAW_TRI, &[1, 2, 3], &mut []));
}

#[test]
fn ring() {
    let r = ring_alloc(10, 100, 20, 16);
    assert_eq!((r.offset, r.grow, r.used), (16, false, 36));
    let r = ring_alloc(90, 100, 20, 16);
    assert_eq!((r.offset, r.grow, r.new_size, r.used), (0, true, 200, 20));
}

#[test]
fn viewport_flips_y() {
    let v = viewport([100, 80], [10, 5], [50, 40], None);
    assert_eq!(v.viewport, [10.0, 35.0, 50.0, 40.0]);
    assert_eq!((v.scissor, v.scissor_size), ([0, 0], [100, 80]));
    let v = viewport([100, 80], [0, 0], [1, 1], Some(([-5, 2], [10, 3])));
    assert_eq!((v.scissor, v.scissor_size), ([0, 2], [5, 3]));
}

#[test]
fn uniforms() {
    let u = reflect_uniform(16, 0x8B52, 2, 0);
    assert_eq!((u.components, u.array_stride, u.count), (4, 16, 2));
    let mut data = vec![0u8; 64];
    assert!(write_uniform(&mut data, &u, 1, 4, 1, &[1.0, 2.0, 3.0, 4.0]));
    assert_eq!(&data[32..36], &1.0f32.to_ne_bytes());
    assert!(!write_uniform(&mut data, &u, 0, 3, 1, &[1.0; 3]));
    let i = reflect_uniform(0, 0x1404, 1, 0);
    assert!(write_int(&mut data, &i, 0, -7));
    assert_eq!(&data[0..4], &(-7i32).to_ne_bytes());
    assert_eq!(split_uniform_name(b"arr[3]"), (3, 3));
    assert_eq!(split_uniform_name(b"x"), (1, 0));
    let m = reflect_uniform(0, 0x8B5C, 1, 0);
    let mut mat = [0.0f32; 16];
    mat[1] = 5.0;
    assert!(write_mat4(&mut data, &m, 0, &mat));
    assert_eq!(&data[16..20], &5.0f32.to_ne_bytes());
    assert!(!write_mat4(&mut data[..32], &m, 0, &mat));
}

#[test]
fn sampler_key_packs_fields() {
    let k = SamplerKey {
        mag_filter: 1,
        min_filter: 1,
        mipmap_mode: 1,
        address_u: 2,
        address_v: 3,
        address_w: 0,
        compare_enable: 1,
        compare_op: 3,
        max_anisotropy: 4.0,
        anisotropy_enable: 1,
        max_lod: -1.0,
    };
    assert_eq!(
        sampler_key(&k),
        1 | 1 << 2 | 1 << 4 | 2 << 6 | 3 << 9 | 1 << 15 | 3 << 16 | 16 << 20 | 1 << 28
    );
}

#[test]
fn unpack_pixels() {
    assert_eq!(
        unpack_pixel(vkf::B8G8R8A8_UNORM, &[0, 255, 0, 255]),
        [0.0, 1.0, 0.0, 1.0]
    );
    assert_eq!(
        unpack_pixel(vkf::R16_SFLOAT, &[0x00, 0x3C]),
        [1.0, 1.0, 1.0, 1.0]
    );
    assert_eq!(unpack_pixel(vkf::R8_UNORM, &[]), [0.0, 0.0, 0.0, 1.0]);
}

#[test]
fn ortho_and_matrix_columns() {
    let m = gl_ortho(0.0, 2.0, 2.0, 0.0, -1.0, 1.0);
    assert_eq!(
        (m[0], m[3], m[5], m[7], m[10]),
        (1.0, -1.0, -1.0, 1.0, -1.0)
    );
    let mut d = [9.0f32; 16];
    write_matrix(&mut d, &m, 3);
    assert_eq!(d[3], 0.0);
    assert_eq!(d[0], 1.0);
}
