//! Replays the C++ recordings under `tests/golden/H7` (no C++, no game data).

use super::anim::UvAnim;
use super::ffi::{ohpl_mat_compile, ohpl_mat_program, ohpl_mat_texture_unit};
use super::h7::{MatState, PeState, material_type, material_types, post_effect_type};
use super::types::{CompileOut, Globals, MatView, ProgSel, TexSel};
use super::vars::VarDefault;
use crate::math::IDENTITY;
use crate::types::{Color, Vector2l};
use std::fmt::Write as _;
use std::path::PathBuf;

fn golden(name: &str) -> String {
    let mut p = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    p.push("tests/golden/H7");
    p.push(name);
    std::fs::read_to_string(&p).unwrap_or_else(|e| panic!("{}: {e}", p.display()))
}

fn hex_u32(s: &str) -> u32 {
    u32::from_str_radix(s, 16).expect("hex")
}

fn bits(s: &str) -> f32 {
    f32::from_bits(hex_u32(s))
}

const MODES: usize = 7;

#[test]
fn registry_matches_cpp() {
    let text = golden("reg.txt");
    let mut lines = text.lines();
    assert_eq!(
        lines.next(),
        Some(format!("count {}", material_types().count()).as_str())
    );
    for line in lines {
        let (name, rest) = line.split_once(' ').expect("name");
        let t = material_type(name.as_bytes()).unwrap_or_else(|| panic!("type {name}"));
        let f: Vec<&str> = rest.split(' ').collect();
        assert_eq!(f[0], "tr");
        let tr: u32 = f[1].parse().expect("tr");
        assert_eq!(f[2], "decal");
        let decal: u32 = f[3].parse().expect("decal");
        assert_eq!(f[4], "spec");
        let mut flags = tr | decal << 1;
        for m in 0..MODES {
            let on: u32 = f[5 + m].parse().expect("spec");
            flags |= on << (8 + m);
        }
        assert_eq!(t.flags(), flags, "{name} flags");
        assert_eq!(f[5 + MODES], "tex");
        let vars_at = f.iter().position(|s| *s == "vars").expect("vars");
        let tex: Vec<u32> = f[6 + MODES..vars_at]
            .iter()
            .map(|s| s.parse().expect("tex"))
            .collect();
        assert_eq!(t.used_textures(), tex, "{name} textures");
        let vars: Vec<String> = t
            .user_vars()
            .iter()
            .map(|v| match v.default {
                VarDefault::Bool(b) => format!("{}={b}", v.name),
                VarDefault::Float(x) => format!(
                    "{}={}",
                    v.name,
                    String::from_utf8(crate::string::float_to_string(x, -1, true)).expect("utf8")
                ),
            })
            .collect();
        assert_eq!(vars, f[vars_at + 1..].to_vec(), "{name} vars");
    }
    assert!(post_effect_type(b"Bloom").is_some());
    assert!(post_effect_type(b"Other").is_none());
}

fn mat_line(m: &MatState) -> String {
    let mut s = format!(
        "blend {} alpha {} depth {} hasuv {} anims {} uv",
        m.blend,
        m.alpha,
        u8::from(m.depth_test),
        u8::from(m.has_uv),
        m.anim_count()
    );
    for v in m.uv.m.iter().flatten() {
        let _ = write!(s, " {:08x}", v.to_bits());
    }
    s
}

#[test]
fn material_state_matches_cpp() {
    let mut cases = 0;
    for n in 0..12 {
        let text = golden(&format!("mat_{n:03}.txt"));
        let mut flags = std::collections::HashMap::new();
        let mut m = MatState::new(false);
        let mut out = Vec::new();
        for line in text.lines() {
            let f: Vec<&str> = line.split(' ').collect();
            match f[0] {
                "type" => {
                    let t = material_type(f[1].as_bytes()).expect("type");
                    flags.insert(f[1], t.flags());
                }
                "new" => {
                    m = MatState::new(flags[f[1]] & 1 != 0);
                    out.push(format!("new {} {}", f[1], mat_line(&m)));
                }
                "-o" => {
                    match f[1] {
                        "blend" => m.set_blend(f[2].parse().expect("v")),
                        "alpha" => m.set_alpha(f[2].parse().expect("v")),
                        "depth" => m.set_depth_test(f[2] == "1"),
                        "anim" => m.add_anim(UvAnim {
                            kind: f[2].parse().expect("k"),
                            axis: f[3].parse().expect("a"),
                            speed: bits(f[4]),
                            amp: bits(f[5]),
                        }),
                        "clear" => m.clear_anims(),
                        "update" => m.update(bits(f[2])),
                        "settype" => m.set_translucent(flags[f[2]] & 1 != 0),
                        _ => panic!("op {line}"),
                    }
                    out.push(format!("-o {}", f[1..].join(" ")));
                    out.push(mat_line(&m));
                    cases += 1;
                }
                _ => {}
            }
        }
        let expect: Vec<&str> = text.lines().filter(|l| !l.starts_with("type ")).collect();
        assert_eq!(out, expect, "mat_{n:03}");
    }
    assert!(cases > 300);
}

#[test]
fn post_effect_state_matches_cpp() {
    for n in 0..12 {
        let text = golden(&format!("pe_{n:03}.txt"));
        let mut e = PeState::new(None);
        let mut p = 0i32;
        let mut out = Vec::new();
        let state = |e: &PeState, p: i32| {
            format!(
                "active {} disabled {} p {p}",
                u8::from(e.is_active()),
                u8::from(e.is_disabled())
            )
        };
        for line in text.lines() {
            let f: Vec<&str> = line.split(' ').collect();
            if f[0] == "new" {
                e = PeState::new((f[1] != "(none)").then_some(f[1].as_bytes()));
                p = 0;
                out.push(format!("new {} {}", f[1], state(&e, p)));
            } else if f[0] == "-o" {
                match f[1] {
                    "active" => {
                        let a = f[2] == "1";
                        out.push(format!("-o active {}", f[2]));
                        if e.set_active(a) {
                            out.push(format!("  onsetactive {}", u8::from(a)));
                        }
                    }
                    "disabled" => {
                        out.push(format!("-o disabled {}", f[2]));
                        e.set_disabled(f[2] == "1");
                    }
                    "set" => {
                        let x: i32 = f[3].parse().expect("x");
                        out.push(format!("-o set {} {x}", f[2]));
                        if e.accepts(f[2].as_bytes()) {
                            p = x;
                            out.push(format!("  onsetparams {p}"));
                        }
                    }
                    "get" => {
                        let got = if e.accepts(f[2].as_bytes()) { p } else { -1 };
                        out.push(format!("-o get {} -> {got}", f[2]));
                    }
                    _ => panic!("op {line}"),
                }
                out.push(state(&e, p));
            }
        }
        let expect: Vec<&str> = text.lines().collect();
        assert_eq!(out, expect, "pe_{n:03}");
    }
}

#[test]
fn final_copy_follows_last_and_final_buffer() {
    let mut e = PeState::new(Some(b"Bloom"));
    e.begin_render();
    assert!(e.needs_final_copy(true));
    assert!(!e.needs_final_copy(false));
    e.mark_final_used();
    assert!(!e.needs_final_copy(true));
    e.begin_render();
    assert!(e.needs_final_copy(true));
}

fn fnv(text: &str) -> u64 {
    text.bytes().fold(1469598103934665603u64, |h, b| {
        (h ^ u64::from(b)).wrapping_mul(1099511628211)
    })
}

fn globals(hex: &str) -> Globals {
    let b: Vec<u8> = (0..hex.len() / 2)
        .map(|i| u8::from_str_radix(&hex[2 * i..2 * i + 2], 16).expect("hex"))
        .collect();
    assert_eq!(b.len(), std::mem::size_of::<Globals>());
    let flag = |i: usize| b[i] != 0;
    let word = |i: usize| u32::from_le_bytes([b[i], b[i + 1], b[i + 2], b[i + 3]]);
    Globals {
        hdr: flag(0),
        parallax_enabled: flag(1),
        parallax_low: flag(2),
        shader_model_3: flag(3),
        gbuffer_32bit: flag(4),
        gbuffer_four_targets: flag(5),
        hpsl: flag(6),
        refraction: flag(7),
        light_probes: flag(8),
        depth_of_field: flag(9),
        tex_rect: flag(10),
        has_water_vtx: flag(11),
        world_fog: word(12),
        reflection_size_div: i32::from_le_bytes(word(16).to_le_bytes()),
        virtual_position_add_scale: f32::from_bits(word(20)),
        dof_params: Color {
            r: f32::from_bits(word(24)),
            g: f32::from_bits(word(28)),
            b: f32::from_bits(word(32)),
            a: f32::from_bits(word(36)),
        },
    }
}

/// Program selection, texture units and compile settings of every distinct material setup in the
/// SOMA, TDD, Rebirth, Bunker and AMFP `.mat` files, hashed as the C++ census does.
#[test]
fn census_setups_match_cpp() {
    let text = golden("census.txt");
    let mut g = None;
    let mut setups = 0;
    let mut mats = 0;
    for line in text.lines() {
        if let Some(h) = line.strip_prefix('G') {
            g = Some(globals(h));
        } else if line.starts_with("M ") {
            mats += 1;
        } else if let Some(rest) = line.strip_prefix("U ") {
            let f: Vec<&str> = rest.split(' ').collect();
            let kind: i32 = f[0].parse().expect("kind");
            let view = MatView {
                tex_mask: u64::from_str_radix(f[1], 16).expect("mask"),
                uv_matrix: IDENTITY,
                liquid_size: Vector2l { x: 0, y: 0 },
                blend_mode: f[2].parse().expect("blend"),
                has_uv_anim: f[3] == "1",
                diffuse_srgb: false,
            };
            let mut vars = vec![0u64; 32];
            let pod: *const std::ffi::c_void = if f[4] == "-" {
                std::ptr::null()
            } else {
                assert!(f[4].len() <= 256 * 2);
                for (i, c) in f[4].as_bytes().chunks(2).enumerate() {
                    let byte =
                        u8::from_str_radix(std::str::from_utf8(c).expect("utf8"), 16).expect("hex");
                    // SAFETY: `vars` holds 256 bytes and `i < 256`.
                    unsafe { vars.as_mut_ptr().cast::<u8>().add(i).write(byte) };
                }
                vars.as_ptr().cast()
            };
            let g = g.expect("G line first");
            let mut out = String::new();
            for sk in 0..2 {
                for m in 0..MODES as i32 {
                    let mut s = ProgSel::NONE;
                    // SAFETY: every pointer is valid for the call.
                    unsafe { ohpl_mat_program(kind, &view, pod, &g, m, sk, &mut s) };
                    let _ = writeln!(out, "prog {sk} {m}: {} {} {:x}", s.manager, s.mode, s.flags);
                }
            }
            for m in 0..MODES as i32 {
                for u in 0..16 {
                    let mut s = TexSel::NONE;
                    // SAFETY: every pointer is valid for the call.
                    unsafe { ohpl_mat_texture_unit(kind, &view, pod, &g, m, u, &mut s) };
                    let _ = writeln!(out, "tu {m} {u}: {} {}", s.kind, s.slot);
                }
            }
            let mut c = CompileOut::NONE;
            // SAFETY: every pointer is valid for the call.
            unsafe { ohpl_mat_compile(kind, &view, pod, &g, &mut c) };
            let _ = writeln!(
                out,
                "compile {:x} {:x} {:x} {:x} {} {} {} {} {} {} {}",
                c.spec_set,
                c.spec_val,
                c.obj_set,
                c.obj_val,
                c.alpha_mode,
                c.dissolve_filter,
                c.blend_mode,
                c.has_refraction,
                c.refraction_edge,
                c.world_reflection,
                c.translucent_illum
            );
            assert_eq!(
                format!("{:016x}", fnv(&out)),
                f[5],
                "setup kind {kind} mask {} vars {}",
                f[1],
                f[4]
            );
            setups += 1;
        }
    }
    assert_eq!(mats, 10306);
    assert_eq!(setups, 1246);
}
