use super::types::{blend, light_kind, manager, mode, texsel, ukind};
use super::*;
use crate::math::IDENTITY;
use crate::types::{Color, Matrixf, Vector2f, Vector2l, Vector3f};
use std::collections::HashMap;

const fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

/// `eMaterialRenderMode_Light` and `eMaterialBlendMode_Alpha`
const LIGHT_MODE: i32 = 4;
const ALPHA_BLEND: i32 = 4;

const WHITE: Color = Color {
    r: 1.0,
    g: 1.0,
    b: 1.0,
    a: 1.0,
};

fn mask(slots: &[u32]) -> u64 {
    slots.iter().fold(0, |m, s| m | (1u64 << s))
}

fn view(slots: &[u32]) -> MatView {
    MatView {
        tex_mask: mask(slots),
        uv_matrix: IDENTITY,
        liquid_size: Vector2l { x: 0, y: 0 },
        blend_mode: blend::ADD,
        has_uv_anim: false,
        diffuse_srgb: false,
    }
}

fn globals() -> Globals {
    Globals {
        refraction: true,
        reflection_size_div: 2,
        ..Globals::default()
    }
}

fn render_view() -> RenderView {
    RenderView {
        view: IDENTITY,
        origin: v3(1.0, 2.0, 3.0),
        time: 10.0,
        near: 0.5,
        far: 100.0,
        temp_alpha: 0.75,
        rt_size: Vector2l { x: 1280, y: 720 },
        fog_area_color: Color {
            r: 0.0,
            g: 0.0,
            b: 0.0,
            a: 0.0,
        },
    }
}

fn obj_view() -> ObjView {
    ObjView {
        model: IDENTITY,
        color_mul: WHITE,
        illum_color: WHITE,
        illum_amount: 1.0,
        liquid_amount: 0.0,
        has_model: false,
        is_static: false,
    }
}

#[derive(Default)]
struct FakeHost {
    coverage: f32,
    fields: Vec<ForceFieldInfo>,
    world: Option<WorldInfo>,
    lights: Vec<LightInfo>,
    bv: Bv,
}

impl Host for FakeHost {
    fn coverage(&mut self) -> f32 {
        self.coverage
    }

    fn force_fields(&mut self, out: &mut [ForceFieldInfo; 4]) -> usize {
        for (d, s) in out.iter_mut().zip(&self.fields) {
            *d = *s;
        }
        self.fields.len().min(4)
    }

    fn fields_in_box(&mut self, _: Vector3f, _: Vector3f) -> usize {
        self.fields.len().min(4)
    }

    fn world(&mut self) -> Option<WorldInfo> {
        self.world
    }

    fn object_bv(&mut self) -> Bv {
        self.bv
    }

    fn light_count(&mut self) -> usize {
        self.lights.len()
    }

    fn light(&mut self, i: usize, _: bool) -> Option<LightInfo> {
        self.lights.get(i).copied()
    }

    fn light_intersects(&mut self, _: usize) -> bool {
        true
    }

    fn spot_clip(&mut self, _: usize) -> Option<(Vector3f, Vector3f)> {
        Some((v3(-0.5, -0.5, 0.0), v3(0.5, 0.5, 0.0)))
    }
}

#[derive(Default)]
struct FakeVars(HashMap<&'static str, &'static str>);

impl FakeVars {
    fn raw(&self, name: &str) -> Option<&'static str> {
        self.0.get(name).copied()
    }
}

impl VarSource for FakeVars {
    fn boolean(&mut self, name: &str, def: bool) -> bool {
        self.raw(name).map_or(def, |s| s == "true")
    }

    fn float(&mut self, name: &str, def: f32) -> f32 {
        self.raw(name).and_then(|s| s.parse().ok()).unwrap_or(def)
    }

    fn vec2(&mut self, _: &str, def: Vector2f) -> Vector2f {
        def
    }

    fn vec3(&mut self, _: &str, def: Vector3f) -> Vector3f {
        def
    }

    fn color(&mut self, _: &str, def: Color) -> Color {
        def
    }

    fn string(&mut self, name: &str, def: &str) -> Vec<u8> {
        self.raw(name).unwrap_or(def).as_bytes().to_vec()
    }
}

fn run<F: FnOnce(&mut Uniforms)>(f: F) -> Vec<Uniform> {
    let mut buf = [Uniform::ZERO; 64];
    let mut u = Uniforms::new(&mut buf);
    f(&mut u);
    assert_eq!(u.dropped(), 0, "uniform buffer overflow");
    u.as_slice().to_vec()
}

fn find(u: &[Uniform], id: i32) -> Option<&Uniform> {
    u.iter().find(|x| x.id == id)
}

#[test]
fn uv_matrix_translate_sin_rotate() {
    let identity = uv_matrix(&[], 3.0);
    assert_eq!(identity, IDENTITY);

    let anims = [UvAnim {
        kind: 0,
        speed: 2.0,
        amp: 0.0,
        axis: 1,
    }];
    let m = uv_matrix(&anims, 1.5);
    assert_eq!(m.m[1][3], 3.0);
    assert_eq!(m.m[0][3], 0.0);

    let sin = [UvAnim {
        kind: 1,
        speed: 1.0,
        amp: 4.0,
        axis: 0,
    }];
    let m = uv_matrix(&sin, 0.0);
    assert_eq!(m.m[0][3], 0.0);
    let m = uv_matrix(&sin, std::f32::consts::FRAC_PI_2);
    assert!((m.m[0][3] - 4.0).abs() < 1e-5);

    let rot = [UvAnim {
        kind: 2,
        speed: 1.0,
        amp: 0.0,
        axis: 2,
    }];
    let m = uv_matrix(&rot, std::f32::consts::FRAC_PI_2);
    assert!(m.m[0][0].abs() < 1e-6 && (m.m[1][0] - 1.0).abs() < 1e-6);
}

#[test]
fn type_tables() {
    assert_eq!(type_flags(Kind::SolidDiffuse), 1 << (8 + mode::DIFFUSE));
    let tr = type_flags(Kind::Translucent);
    assert_eq!(tr & 3, 1);
    assert_eq!(tr >> 8, 0b110_1100);
    assert_eq!(type_flags(Kind::Decal) & 3, 3);
    assert_eq!(type_flags(Kind::Undergrowth) >> 8, 0b101);

    assert_eq!(used_textures(Kind::SolidDiffuse).len(), 13);
    assert_eq!(
        used_textures(Kind::ProjectedUv),
        (12..=20).collect::<Vec<_>>()
    );
    assert_eq!(used_textures(Kind::VertexBlend).len(), 12);
    assert!(used_textures(Kind::TerrainBlend).is_empty());
    assert!(used_textures(Kind::Undergrowth).is_empty());

    let names = |k| user_vars(k).iter().map(|v| v.name).collect::<Vec<_>>();
    assert_eq!(names(Kind::SolidDiffuse)[0], "HeightMapScale");
    assert_eq!(
        user_vars(Kind::SolidDiffuse)[0].default,
        VarDefault::Float(0.05)
    );
    let t = names(Kind::Translucent);
    assert_eq!(t.len(), 10);
    assert_eq!(t.iter().filter(|n| **n == "RefractionNormals").count(), 2);
    assert_eq!(names(Kind::Water).len(), 13);
    assert!(user_vars(Kind::Decal).is_empty());
}

#[test]
fn solid_variable_defaults_and_derived_values() {
    let mut v = default_solid();
    assert_eq!(v.height_map_scale, 0.05);
    load_solid(&mut v, &mut FakeVars::default());
    // the loader's own default, not the user variable table's
    assert_eq!(v.height_map_scale, 0.1);
    assert_eq!(v.detail_properties, [5.0, 5.0, 4.0, 4.0]);
    assert_eq!(v.liquid_blend_mode, 1);
    assert_eq!(v.sway_octave_muls, v3(0.125, 0.25, 1.0));

    let mut src = FakeVars::default();
    src.0.insert("DetailFadeStart", "2");
    src.0.insert("DetailFadeEnd", "10");
    src.0.insert("LiquidTrickleBlendMode", "Mul");
    load_solid(&mut v, &mut src);
    assert_eq!(v.detail_properties[..2], [2.0, 8.0]);
    assert_eq!(v.liquid_blend_mode, 2);
    src.0.insert("LiquidTrickleBlendMode", "mul");
    load_solid(&mut v, &mut src);
    assert_eq!(v.liquid_blend_mode, 0);
}

#[test]
fn loaders_clamp_and_invert() {
    let mut p = default_projected_uv();
    let mut src = FakeVars::default();
    src.0.insert("BlendSmoothness", "2");
    src.0.insert("NormalMapBlendImpact", "7");
    src.0.insert("TextureUVScaleTop", "4");
    load_projected_uv(&mut p, &mut src);
    assert_eq!(p.blend_hardness, 0.0);
    assert_eq!(p.normal_map_blend_impact, 1.0);
    assert_eq!(p.texture_scale, v3(1.0, 0.25, 1.0));

    let mut b = default_vertex_blend();
    let mut src = FakeVars::default();
    src.0.insert("BlendSmoothness", "0.5");
    src.0.insert("ColorBlendSmoothness", "-1");
    src.0.insert("HeightMapScaleG", "0.7");
    src.0.insert("AutoUvB", "false");
    load_vertex_blend(&mut b, &mut src);
    assert_eq!(b.normal_blend_hardness, 0.5);
    assert_eq!(b.color_blend_hardness, 0.99);
    assert_eq!(b.height_map_scale, v3(0.1, 0.7, 0.1));
    assert_eq!(b.auto_uv, [true, true, false]);
    assert!(b.alpha_mask);
}

#[test]
fn translucent_and_water_loaders_report_material_settings() {
    let mut v = default_translucent();
    let mut out = LoadOut::NONE;
    load_translucent(&mut v, &mut FakeVars::default(), &mut out);
    // RefractionNormals loads as true though the table lists false
    assert!(v.refraction_normals && v.refraction_edge_check && !v.refraction);
    assert_eq!(v.refraction_scale, 1.0);
    assert_eq!(out.affected_by_dof, 1);

    let mut w = default_water();
    let mut out = LoadOut::NONE;
    let mut src = FakeVars::default();
    src.0.insert("ReflectionFadeEnd", "30");
    src.0.insert("LargeSurface", "true");
    load_water(&mut w, &mut src, &mut out);
    assert_eq!(out.max_reflection_distance, 30.0);
    assert!(out.has_max_reflection_distance);
    assert_eq!(out.large_transparent_surface, 1);
    assert_eq!(out.world_reflection_occlusion_test, 1);
}

#[derive(Default)]
struct Recorder(Vec<String>);

impl SetupSink for Recorder {
    fn clear_defines(&mut self) {
        self.0.push("clear".into());
    }

    fn define(&mut self, name: &str) {
        self.0.push(format!("define {name}"));
    }

    fn define_value(&mut self, name: &str, value: f32) {
        self.0.push(format!("define {name}={value}"));
    }

    fn setup_program(
        &mut self,
        m: i32,
        mode: i32,
        name: &str,
        vtx: &str,
        frag: &str,
        f: &[Feature],
    ) {
        self.0.push(format!(
            "program m{m} mode{mode} {name} {vtx} {frag} {}",
            f.len()
        ));
    }

    fn var_id(&mut self, m: i32, name: &str, id: i32, mode: i32) {
        self.0.push(format!("var m{m} {name} {id} {mode}"));
    }
}

#[test]
fn program_setup_scripts() {
    let mut r = Recorder::default();
    setup_programs(Kind::SolidDiffuse, true, &globals(), &mut r);
    assert_eq!(r.0[..2], ["clear", "define UseUv"]);
    assert!(r.0.contains(
        &"program m0 mode0 Z deferred_base_vtx.glsl deferred_base_frag.glsl 9".to_string()
    ));

    let mut r = Recorder::default();
    let g = Globals {
        hdr: true,
        gbuffer_32bit: true,
        parallax_low: false,
        shader_model_3: true,
        virtual_position_add_scale: 0.03,
        ..Globals::default()
    };
    setup_programs(Kind::SolidDiffuse, false, &g, &mut r);
    for d in [
        "define Deferred_32bit",
        "define RenderTargets_3",
        "define ParallaxMethod_Relief",
        "define UseColorMul",
        "define VirtualPositionAddScale=0.03",
    ] {
        assert!(r.0.iter().any(|l| l == d), "{d}");
    }
    // a_mtxInvView shares the rotation matrix id: a bug kept
    assert!(r.0.contains(&"var m0 a_mtxInvView 6 2".to_string()));

    let mut r = Recorder::default();
    setup_programs(Kind::Translucent, false, &globals(), &mut r);
    let blends: Vec<_> =
        r.0.iter()
            .filter(|l| l.starts_with("define BlendMode"))
            .collect();
    assert_eq!(blends.len(), 5);
    assert_eq!(r.0.iter().filter(|l| l.starts_with("program m")).count(), 5);
    assert!(
        r.0.contains(
            &"program m6 mode2 Diffuse deferred_base_vtx.glsl deferred_transparent_frag.glsl 17"
                .to_string()
        )
    );

    let mut r = Recorder::default();
    setup_programs(
        Kind::Water,
        false,
        &Globals {
            has_water_vtx: true,
            ..globals()
        },
        &mut r,
    );
    assert!(r.0.iter().any(|l| l.contains("water_surface_vtx.glsl")));
    assert!(r.0.iter().any(|l| l == "define UseRefraction"));
    assert_eq!(r.0.iter().filter(|l| *l == "define UseNormals").count(), 2);
}

#[test]
fn solid_program_flags() {
    let mut vars = default_solid();
    let g = Globals {
        parallax_enabled: true,
        hpsl: true,
        ..globals()
    };
    let v = view(&[0, 1, 4]);
    let p = program(
        Kind::SolidDiffuse,
        &v,
        Vars::Solid(&vars),
        &g,
        mode::DIFFUSE,
        false,
    );
    assert_eq!(p, ProgSel::own(mode::DIFFUSE, 0b101));

    vars.sway_active = true;
    vars.sway_single_dir = true;
    let p = program(
        Kind::SolidDiffuse,
        &v,
        Vars::Solid(&vars),
        &g,
        mode::Z,
        false,
    );
    // sway, single dir and sway map; no parallax while swaying
    assert_eq!(p, ProgSel::global(mode::Z, (1 << 5) | (1 << 6) | (1 << 7)));
    let p = program(
        Kind::SolidDiffuse,
        &v,
        Vars::Solid(&vars),
        &g,
        mode::DIFFUSE,
        false,
    );
    assert_eq!(p.flags & (1 << 2), 0);

    let none = program(
        Kind::SolidDiffuse,
        &v,
        Vars::Solid(&vars),
        &g,
        LIGHT_MODE,
        false,
    );
    assert_eq!(none, ProgSel::NONE);
    let d = destroy_target(Kind::SolidDiffuse, mode::Z_DISSOLVE, 0);
    assert_eq!((d.manager, d.mode), (manager::GLOBAL, mode::Z));
}

#[test]
fn translucent_blend_manager_and_dof_slot() {
    let vars = default_translucent();
    let g = globals();
    let mut v = view(&[0]);
    for (b, want) in [
        (1, 2),
        (2, 3),
        (5, 6),
        (0, manager::INVALID),
        (6, manager::INVALID),
    ] {
        v.blend_mode = b;
        let p = program(
            Kind::Translucent,
            &v,
            Vars::Translucent(&vars),
            &g,
            mode::DIFFUSE,
            false,
        );
        assert_eq!(p.manager, want, "blend {b}");
    }
    v.blend_mode = ALPHA_BLEND;
    let p = program(
        Kind::Translucent,
        &v,
        Vars::Translucent(&vars),
        &g,
        mode::DIFFUSE,
        true,
    );
    assert_eq!(p, ProgSel::NONE);
    let dof = Globals {
        depth_of_field: true,
        ..g
    };
    let p = program(
        Kind::Translucent,
        &v,
        Vars::Translucent(&vars),
        &dof,
        mode::DIFFUSE_FOG,
        true,
    );
    assert_eq!(p.flags & (1 << 15), 1 << 15);
    assert_eq!(
        render_program_slot(Kind::Translucent, mode::DIFFUSE, &dof),
        1
    );
    assert_eq!(
        render_program_slot(Kind::Translucent, mode::ILLUMINATION, &dof),
        0
    );
    assert_eq!(render_program_slot(Kind::Translucent, mode::DIFFUSE, &g), 0);

    // illumination programs always use the add manager
    let d = destroy_target(Kind::Translucent, mode::ILLUMINATION_FOG, 4);
    assert_eq!(d.manager, manager::BLEND0);
    assert_eq!(world_fog_flags(true, true), (1 << 10) | (1 << 11));
}

#[test]
fn projected_and_vertex_blend_textures() {
    let v = view(&[tex_slot(13), tex_slot(20)]);
    // unit 0 is DiffuseSide: missing, falls back to Top (13)
    assert_eq!(
        texture_unit(
            Kind::ProjectedUv,
            &v,
            Vars::None,
            &globals(),
            mode::DIFFUSE,
            0
        ),
        TexSel::slot(13)
    );
    // unit 8 SpecularBottom is present
    assert_eq!(
        texture_unit(
            Kind::ProjectedUv,
            &v,
            Vars::None,
            &globals(),
            mode::DIFFUSE,
            8
        ),
        TexSel::slot(20)
    );
    assert_eq!(
        texture_unit(
            Kind::ProjectedUv,
            &v,
            Vars::None,
            &globals(),
            mode::DIFFUSE,
            9
        ),
        TexSel::NONE
    );
    assert_eq!(
        texture_unit(Kind::ProjectedUv, &v, Vars::None, &globals(), mode::Z, 1).kind,
        texsel::DISSOLVE
    );

    let v = view(&[21 + 5]);
    assert_eq!(
        texture_unit(
            Kind::VertexBlend,
            &v,
            Vars::None,
            &globals(),
            mode::DIFFUSE,
            5
        ),
        TexSel::slot(26)
    );
    assert_eq!(
        texture_unit(
            Kind::VertexBlend,
            &v,
            Vars::None,
            &globals(),
            mode::DIFFUSE,
            12
        ),
        TexSel::NONE
    );
}

fn tex_slot(s: u32) -> u32 {
    s
}

#[test]
fn vertex_blend_flags() {
    let mut vars = default_vertex_blend();
    vars.auto_uv = [true, true, false];
    vars.height_map_scale = v3(1.0, 0.0, 1.0);
    vars.alpha_mask = true;
    // diffuse R (21), normal map G (21 + 4 + 2 = 27), height G (28, scale 0), diffuse B (29)
    let v = view(&[21, 27, 28, 29]);
    let p = program(
        Kind::VertexBlend,
        &v,
        Vars::VertexBlend(&vars),
        &globals(),
        mode::DIFFUSE,
        false,
    );
    let diffuse_r = 1 << 0;
    let nmap_g = 1 << (2 * 3 + 1);
    let diffuse_b = 1 << 2;
    let normal_uv_b = (1 << 12) << 2;
    let alpha = 1 << 15;
    let tangents = 1 << 17;
    assert_eq!(
        p.flags,
        diffuse_r | nmap_g | diffuse_b | normal_uv_b | alpha | tangents
    );
}

#[test]
fn compile_settings() {
    let g = globals();
    let v = view(&[tex_slot(3), tex_slot(1), tex_slot(4)]);
    let vars = default_solid();
    let c = compile(Kind::SolidDiffuse, &v, Vars::Solid(&vars), &g);
    assert_eq!(c.alpha_mode, 1);
    assert_ne!(c.spec_val & (1 << mode::DIFFUSE), 0);
    assert_ne!(c.obj_val & (1 << mode::Z_DISSOLVE), 0);

    let w = default_water();
    let c = compile(Kind::Water, &view(&[]), Vars::Water(&w), &g);
    assert_eq!(c.blend_mode, blend::NONE);
    assert_eq!(
        (c.has_refraction, c.refraction_edge, c.world_reflection),
        (1, 1, 1)
    );
    let c = compile(
        Kind::Water,
        &view(&[6]),
        Vars::Water(&w),
        &Globals::default(),
    );
    assert_eq!(
        (c.blend_mode, c.has_refraction, c.world_reflection),
        (blend::MUL, -1, -1)
    );

    let t = default_translucent();
    let c = compile(Kind::Translucent, &view(&[6]), Vars::Translucent(&t), &g);
    assert_eq!(c.translucent_illum, 1);
}

#[test]
fn solid_object_uniforms() {
    let vars = SolidVars {
        liquid_fade_speed: Vector2f { x: 2.0, y: 4.0 },
        liquid_edge_size: 0.49,
        liquid_dryness: 0.25,
        ..default_solid()
    };
    let mut v = view(&[tex_slot(33)]);
    v.liquid_size = Vector2l { x: 512, y: 1024 };
    let mut o = obj_view();
    o.liquid_amount = 3.0;
    let mut host = FakeHost::default();
    let u = run(|u| {
        setup_object(
            Kind::SolidDiffuse,
            mode::DIFFUSE,
            &v,
            Vars::Solid(&vars),
            &globals(),
            &render_view(),
            &o,
            &mut host,
            u,
        )
    });
    let fade = find(&u, 55).expect("liquid fade");
    assert_eq!(fade.kind, ukind::VEC4);
    assert_eq!(fade.v[..4], [0.5, 0.5, 2.0, 0.5]);
    assert!(find(&u, 7).is_some());

    let u = run(|u| {
        setup_material(
            Kind::SolidDiffuse,
            mode::DIFFUSE,
            &v,
            Vars::Solid(&vars),
            &globals(),
            &render_view(),
            0,
            &mut host,
            u,
        )
    });
    assert_eq!(find(&u, 54).map(|x| x.v[0]), Some(1.0 / 1024.0));

    // a negative amount counts from the renderer time
    o.liquid_amount = -2.0;
    let u = run(|u| {
        setup_object(
            Kind::SolidDiffuse,
            mode::DIFFUSE,
            &v,
            Vars::Solid(&vars),
            &globals(),
            &render_view(),
            &o,
            &mut host,
            u,
        )
    });
    assert_eq!(find(&u, 55).map(|x| x.v[0]), Some(9.0 / 2.0 - 1.0));
}

#[test]
fn dissolve_amount_is_checked_and_sway_uses_force_fields() {
    let mut vars = default_solid();
    vars.sway_active = true;
    vars.sway_force_field_affected = true;
    vars.sway_force_field_mul = 2.0;
    vars.sway_force_field_max = 9.0;
    let mut host = FakeHost {
        coverage: 0.25,
        fields: vec![ForceFieldInfo {
            pos: v3(1.0, 2.0, 3.0),
            falloff_start: 1.0,
            radius: 5.0,
            force: 3.0,
            t: 7.0,
        }],
        ..FakeHost::default()
    };
    let u = run(|u| {
        setup_object(
            Kind::SolidDiffuse,
            mode::Z_DISSOLVE,
            &view(&[]),
            Vars::Solid(&vars),
            &globals(),
            &render_view(),
            &obj_view(),
            &mut host,
            u,
        )
    });
    let d = find(&u, 4).expect("dissolve");
    assert_eq!((d.kind, d.check, d.v[0]), (ukind::FLOAT, 1, 0.25));
    // model matrix, 4 positions, 4 vec4, max force
    assert_eq!(u.len(), 1 + 1 + 4 + 4 + 1 + 1 - 1);
    assert_eq!(
        find(&u, 24).map(|x| x.v[..3].to_vec()),
        Some(vec![1.0, 2.0, 3.0])
    );
    assert_eq!(
        find(&u, 25).map(|x| x.v[..3].to_vec()),
        Some(vec![0.0, 0.0, 0.0])
    );
    assert_eq!(
        find(&u, 29).map(|x| x.v[..4].to_vec()),
        Some(vec![4.0, 1.0, 1.0, 1.0])
    );
    assert_eq!(find(&u, 30).map(|x| x.v[0]), Some(6.0));
    assert_eq!(find(&u, 32).map(|x| x.v[0]), Some(9.0));
}

#[test]
fn water_fog_uniforms() {
    let vars = default_water();
    let world = WorldInfo {
        fog_start: 2.0,
        fog_end: 12.0,
        fog_falloff_exp: 1.5,
        fog_brightness: 2.0,
        fog_color: Color {
            r: 0.5,
            g: 0.5,
            b: 0.5,
            a: 1.0,
        },
        fog_active: true,
        fog_exponential: true,
        fog_density: 0.5,
        fog_height_density: 0.2,
        fog_height_horizon: 1.0,
        ..WorldInfo::default()
    };
    let mut host = FakeHost {
        world: Some(world),
        ..FakeHost::default()
    };
    let g = Globals {
        hdr: true,
        ..globals()
    };
    let u = run(|u| {
        setup_material(
            Kind::Water,
            mode::DIFFUSE_FOG,
            &view(&[]),
            Vars::Water(&vars),
            &g,
            &render_view(),
            0,
            &mut host,
            u,
        )
    });
    assert_eq!(
        find(&u, 8).map(|x| x.v[..2].to_vec()),
        Some(vec![2.0, 10.0])
    );
    // hdr squares the colour and applies the brightness
    assert_eq!(
        find(&u, 9).map(|x| x.v[..4].to_vec()),
        Some(vec![0.5, 0.5, 0.5, 1.0])
    );
    let settings = find(&u, 23).expect("settings");
    assert_eq!(settings.v[2], 0.5 * 0.03125);
    assert_eq!(settings.v[3], 0.2);
    assert_eq!(
        find(&u, 22).map(|x| x.v[..3].to_vec()),
        Some(vec![1.0, 2.0, 2.0])
    );
    assert_eq!(find(&u, 24).map(|x| x.int_value), Some(1));

    // a fog area replaces world fog
    let mut area = render_view();
    area.fog_area_color.a = 0.5;
    let u = run(|u| {
        setup_material(
            Kind::Water,
            mode::DIFFUSE_FOG,
            &view(&[]),
            Vars::Water(&vars),
            &g,
            &area,
            0,
            &mut host,
            u,
        )
    });
    assert_eq!(find(&u, 24).map(|x| x.int_value), Some(0));
    assert!(find(&u, 22).is_none());

    // no world: nothing past the wave parameters
    host.world = None;
    let u = run(|u| {
        setup_material(
            Kind::Water,
            mode::DIFFUSE,
            &view(&[]),
            Vars::Water(&vars),
            &g,
            &render_view(),
            0,
            &mut host,
            u,
        )
    });
    assert!(find(&u, 24).is_none() && find(&u, 1).is_some());
}

fn point_light(pos: Vector3f, radius: f32) -> LightInfo {
    LightInfo {
        kind: 0,
        min: v3(pos.x - radius, pos.y - radius, pos.z - radius),
        max: v3(pos.x + radius, pos.y + radius, pos.z + radius),
        pos,
        radius,
        falloff_pow: 0.5,
        brightness: 1.0,
        diffuse: WHITE,
        ..LightInfo::default()
    }
}

fn probe(host: &mut FakeHost, vars: &TranslucentVars, g: &Globals) -> Vec<Uniform> {
    run(|u| {
        setup_object(
            Kind::Translucent,
            mode::DIFFUSE,
            &view(&[]),
            Vars::Translucent(vars),
            g,
            &render_view(),
            &obj_view(),
            host,
            u,
        )
    })
}

#[test]
fn translucent_light_level_and_probe() {
    let mut vars = default_translucent();
    vars.affected_by_light_level = true;
    let bv = Bv {
        min: v3(-1.0, -1.0, -1.0),
        max: v3(1.0, 1.0, 1.0),
        world_center: v3(0.0, 0.0, 0.0),
    };
    let mut host = FakeHost {
        bv,
        lights: vec![point_light(v3(0.0, 0.0, 0.0), 4.0)],
        ..FakeHost::default()
    };
    let g = globals();
    let u = probe(&mut host, &vars, &g);
    assert_eq!(find(&u, 0).map(|x| x.v[0]), Some(0.75));
    assert_eq!(find(&u, 9).map(|x| x.v[0]), Some(1.0));

    // two lights saturate the level at one
    host.lights.push(point_light(v3(0.0, 0.0, 0.0), 4.0));
    host.lights[0].diffuse.g = 0.5;
    let u = probe(&mut host, &vars, &g);
    assert_eq!(find(&u, 9).map(|x| x.v[0]), Some(1.0));

    let lit = Globals {
        light_probes: true,
        ..g
    };
    host.lights.truncate(1);
    let u = probe(&mut host, &vars, &lit);
    let m = find(&u, 12).expect("probe");
    assert_eq!(m.kind, ukind::MAT);
    // rows are r, g, b; the fourth is zero; a light straight above lights the +y term only
    assert!(m.v[3] > 0.0 && m.v[7] > 0.0 && m.v[11] > 0.0);
    assert_eq!(m.v[12..], [0.0; 4]);

    // no lights, or none touching: nothing but the ambient floor of zero
    host.lights.clear();
    let u = probe(&mut host, &vars, &lit);
    assert_eq!(find(&u, 12).map(|x| x.v.to_vec()), Some(vec![0.0; 16]));
    host.lights.push(point_light(v3(100.0, 0.0, 0.0), 1.0));
    let u = probe(&mut host, &vars, &lit);
    assert_eq!(find(&u, 12).map(|x| x.v.to_vec()), Some(vec![0.0; 16]));
}

#[test]
fn box_light_blend_modes_order_and_nan() {
    let mut vars = default_translucent();
    vars.affected_by_light_level = true;
    let bv = Bv {
        min: v3(-1.0, -1.0, -1.0),
        max: v3(1.0, 1.0, 1.0),
        world_center: v3(0.0, 0.0, 0.0),
    };
    let make = |blend_func, weight| LightInfo {
        kind: light_kind::BOX,
        min: v3(-2.0, -2.0, -2.0),
        max: v3(2.0, 2.0, 2.0),
        pos: v3(0.0, 0.0, 0.0),
        radius: 5.0,
        falloff_pow: 1.0,
        brightness: 1.0,
        diffuse: WHITE,
        blend_func,
        weight,
        bevel: 0.5,
        ..LightInfo::default()
    };
    let lit = Globals {
        light_probes: true,
        ..globals()
    };
    let value = |lights: Vec<LightInfo>| {
        let mut host = FakeHost {
            bv,
            lights,
            ..FakeHost::default()
        };
        find(&probe(&mut host, &vars, &lit), 12).map(|x| x.v[3])
    };
    // boxes apply in blend function order whatever the list order
    let a = value(vec![make(2, 1.0), make(0, 1.0)]);
    let b = value(vec![make(0, 1.0), make(2, 1.0)]);
    assert_eq!(a, b);
    assert!(a.is_some_and(|x| x > 0.0));
    // a NaN weight skips the box
    assert_eq!(value(vec![make(2, f32::NAN)]), Some(0.0));
}

#[test]
fn undergrowth_fields_and_programs() {
    let mut vars = default_undergrowth();
    vars.wind = true;
    vars.force_field_mul = 2.0;
    vars.dissolve = Vector2f { x: 5.0, y: 5.0 };
    let mut host = FakeHost {
        fields: vec![ForceFieldInfo {
            pos: v3(1.0, 2.0, 3.0),
            falloff_start: 1.0,
            radius: 5.0,
            force: 3.0,
            t: 7.0,
        }],
        ..FakeHost::default()
    };
    let (p, n) = undergrowth_render_program(&vars, mode::Z, &render_view(), &mut host);
    assert_eq!((p, n), (ProgSel::own(mode::Z, 0b011), 1));
    let (p, _) = undergrowth_render_program(&vars, LIGHT_MODE, &render_view(), &mut host);
    assert_eq!(p, ProgSel::NONE);
    host.fields.extend_from_within(..);
    let (p, n) = undergrowth_render_program(&vars, mode::DIFFUSE, &render_view(), &mut host);
    assert_eq!((p.flags, n), (0b101, 2));
    vars.force_field_mul = 0.0;
    assert_eq!(
        undergrowth_render_program(&vars, mode::DIFFUSE, &render_view(), &mut host).1,
        0
    );

    vars.force_field_mul = 2.0;
    let mut u = vec![];
    for field_num in [1, 2] {
        u.push(run(|u| {
            setup_material(
                Kind::Undergrowth,
                mode::Z,
                &view(&[]),
                Vars::Undergrowth(&vars),
                &globals(),
                &render_view(),
                field_num,
                &mut host,
                u,
            )
        }));
    }
    assert_eq!(
        find(&u[0], 14).map(|x| x.v[..4].to_vec()),
        Some(vec![1.0, 2.0, 3.0, 7.0])
    );
    assert_eq!(
        find(&u[0], 15).map(|x| x.v[..4].to_vec()),
        Some(vec![1.0, 4.0, 6.0, 2.0])
    );
    assert!(find(&u[1], 14).is_none() && find(&u[1], 13).is_some());
}

#[test]
fn terrain_blend_and_decal() {
    let v = view(&[tex_slot(5)]);
    assert_eq!(
        texture_unit(
            Kind::TerrainBlend,
            &v,
            Vars::None,
            &globals(),
            mode::DIFFUSE,
            5
        ),
        TexSel::slot(5)
    );
    assert_eq!(
        texture_unit(
            Kind::TerrainBlend,
            &v,
            Vars::None,
            &globals(),
            mode::DIFFUSE,
            4
        )
        .kind,
        texsel::WHITE
    );
    assert_eq!(
        texture_unit(
            Kind::TerrainBlend,
            &v,
            Vars::None,
            &globals(),
            mode::DIFFUSE,
            13
        ),
        TexSel::NONE
    );
    let p = program(
        Kind::TerrainBlend,
        &v,
        Vars::None,
        &globals(),
        mode::DIFFUSE,
        false,
    );
    assert_eq!(p.flags, 1);

    let mut d = view(&[0]);
    d.has_uv_anim = true;
    d.uv_matrix = Matrixf { m: [[2.0; 4]; 4] };
    let u = run(|u| {
        setup_material(
            Kind::Decal,
            mode::DIFFUSE,
            &d,
            Vars::None,
            &globals(),
            &render_view(),
            0,
            &mut FakeHost::default(),
            u,
        )
    });
    assert_eq!(u.len(), 1);
    assert_eq!(u[0].v, [2.0; 16]);
    assert_eq!(
        compile(Kind::Decal, &d, Vars::None, &globals()).spec_val,
        1 << mode::DIFFUSE
    );
}

#[test]
fn uniform_buffer_drops_instead_of_panicking() {
    let mut buf = [Uniform::ZERO; 2];
    let mut u = Uniforms::new(&mut buf);
    u.float(0, 1.0);
    u.int(1, 2);
    u.vec2(2, 0.0, 0.0);
    assert_eq!((u.len(), u.dropped()), (2, 1));
    let mut u = Uniforms::new(&mut []);
    u.float(0, 1.0);
    assert!(u.is_empty());
}
