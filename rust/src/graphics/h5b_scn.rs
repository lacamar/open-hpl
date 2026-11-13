//! Seeded call scenarios over `dyn Gfx`: every operation of the trait, in random but valid order.
//! The same scenario drives the C++ recorder (through the facade) and `TraceGfx`; the returned log
//! holds every value a caller could branch on.

use std::fmt::Write as _;

use super::h5b_gfx::*;
use super::h5b_trace::fnv;
use crate::graphics::formats;

struct Rng(u64);

impl Rng {
    fn next(&mut self) -> u64 {
        self.0 ^= self.0 >> 12;
        self.0 ^= self.0 << 25;
        self.0 ^= self.0 >> 27;
        self.0.wrapping_mul(0x2545_f491_4f6c_dd1d)
    }

    fn below(&mut self, n: u32) -> u32 {
        ((self.next() >> 33) as u32)
            .checked_rem(n.max(1))
            .unwrap_or(0)
    }

    fn int(&mut self, n: u32) -> i32 {
        self.below(n) as i32
    }

    fn bool(&mut self) -> bool {
        self.below(2) == 1
    }

    fn f(&mut self) -> f32 {
        (self.below(4000) as f32 - 2000.0) / 16.0
    }

    fn v2(&mut self) -> V2f {
        [self.f(), self.f()]
    }

    fn v3(&mut self) -> V3f {
        [self.f(), self.f(), self.f()]
    }

    fn col(&mut self) -> Col {
        [self.unit(), self.unit(), self.unit(), self.unit()]
    }

    fn unit(&mut self) -> f32 {
        self.below(256) as f32 / 255.0
    }

    fn v2l(&mut self) -> V2l {
        [self.int(512) - 16, self.int(512) - 16]
    }

    fn bytes(&mut self, n: usize) -> Vec<u8> {
        (0..n).map(|_| self.next() as u8).collect()
    }

    fn vtx(&mut self) -> Vtx {
        Vtx {
            pos: self.v3(),
            tex: self.v3(),
            tan: self.v3(),
            norm: self.v3(),
            col: self.col(),
        }
    }

    fn vtxs(&mut self, n: usize) -> Vec<Vtx> {
        (0..n).map(|_| self.vtx()).collect()
    }

    fn mat(&mut self) -> Mat {
        let mut m = [0.0; 16];
        m.iter_mut()
            .for_each(|x| *x = (self.below(200) as f32 - 100.0) / 8.0);
        m
    }

    fn word(&mut self) -> String {
        let n = 1 + self.below(8);
        (0..n)
            .map(|_| char::from(b'a' + self.below(26) as u8))
            .collect()
    }

    fn pick<T: Copy>(&mut self, v: &[T]) -> Option<T> {
        let n = u32::try_from(v.len()).ok()?;
        v.get(self.below(n) as usize).copied()
    }

    fn opt<T: Copy>(&mut self, v: &[T]) -> Option<T> {
        if self.below(5) == 0 {
            None
        } else {
            self.pick(v)
        }
    }

    fn bmp(&mut self) -> Bmp {
        let size = [1 + self.int(6), 1 + self.int(6), 1 + self.int(2)];
        let format = self.pick(&[1, 2, 3, 4, 5, 6, 7]).unwrap_or(5);
        let per = size.iter().map(|&x| x as usize).product::<usize>()
            * formats::channels(format).max(0) as usize;
        let (images, mips) = (1 + self.below(2), 1 + self.below(3));
        let images = (0..images)
            .map(|_| (0..mips).map(|_| self.bytes(per)).collect())
            .collect();
        Bmp {
            size,
            format,
            images,
        }
    }
}

#[derive(Default)]
struct Live {
    tex: Vec<TexId>,
    vb: Vec<VbId>,
    prog: Vec<ProgId>,
    shader: Vec<ShaderId>,
    fb: Vec<FbId>,
    dsb: Vec<DsbId>,
    oq: Vec<OqId>,
    font: Vec<FontId>,
}

/// Number of distinct actions; each round visits all of them once in a seeded order.
pub const ACTIONS: u32 = 172;

fn log(out: &mut String, k: &str, v: impl std::fmt::Debug) {
    let _ = writeln!(out, "{k}={v:?}");
}

/// Runs `rounds` rounds of every action on `g`; returns the query log.
pub fn run(g: &mut dyn Gfx, seed: u64, rounds: u32) -> String {
    let mut r = Rng(seed.wrapping_mul(0x9e37_79b9_7f4a_7c15) | 1);
    let mut l = Live::default();
    let mut out = String::new();
    let mut ran = 0;
    for _ in 0..rounds {
        let mut order: Vec<u32> = (0..ACTIONS).collect();
        for k in (1..order.len()).rev() {
            let j = r.below(k as u32 + 1) as usize;
            order.swap(k, j);
        }
        for a in order {
            step(g, &mut r, &mut l, &mut out, a);
            ran += 1;
        }
    }
    finish(g, &mut r, &mut l);
    log(&mut out, "actions", ran);
    out
}

fn finish(g: &mut dyn Gfx, r: &mut Rng, l: &mut Live) {
    g.set_current_frame_buffer(None, [0, 0], [-1, -1]);
    for p in std::mem::take(&mut l.prog) {
        g.destroy_program(p);
    }
    for s in std::mem::take(&mut l.shader) {
        g.destroy_shader(s);
    }
    for f in std::mem::take(&mut l.fb) {
        g.destroy_frame_buffer(f);
    }
    for d in std::mem::take(&mut l.dsb) {
        g.destroy_depth_stencil(d);
    }
    for q in std::mem::take(&mut l.oq) {
        g.destroy_occlusion_query(q);
    }
    for f in std::mem::take(&mut l.font) {
        g.destroy_font(f);
    }
    for v in std::mem::take(&mut l.vb) {
        g.destroy_vertex_buffer(v);
    }
    let mut tex = std::mem::take(&mut l.tex);
    if r.bool() {
        tex.reverse();
    }
    for t in tex {
        g.destroy_texture(t);
    }
}

#[allow(clippy::too_many_lines)]
fn step(g: &mut dyn Gfx, r: &mut Rng, l: &mut Live, out: &mut String, a: u32) {
    macro_rules! with {
        ($list:ident, $make:expr, $f:expr) => {{
            if l.$list.is_empty() {
                l.$list.push($make);
            }
            if let Some(id) = r.pick(&l.$list) {
                #[allow(clippy::redundant_closure_call)]
                ($f)(id)
            }
        }};
    }
    macro_rules! tex {
        ($f:expr) => {
            with!(tex, g.create_texture(&r.word(), r.int(6), r.int(3)), $f)
        };
    }
    macro_rules! vb {
        ($f:expr) => {
            with!(
                vb,
                g.create_vertex_buffer(r.int(3), r.int(8), r.int(3), r.int(100), r.int(100)),
                $f
            )
        };
    }
    macro_rules! prog {
        ($f:expr) => {
            with!(prog, g.create_program(&r.word()), $f)
        };
    }
    macro_rules! fb {
        ($f:expr) => {
            with!(fb, g.create_frame_buffer(&r.word()), $f)
        };
    }
    match a {
        0 => {
            let a = InitArgs {
                size: [r.int(2000) + 1, r.int(2000) + 1],
                display: r.int(3),
                bpp: 32,
                fullscreen: r.int(2),
                multisampling: r.int(9),
                program_format: r.int(3),
                caption: r.word(),
                window_pos: [r.int(100) - 50, r.int(100) - 50],
            };
            log(out, "init", g.init(&a));
        }
        1 => log(out, "gpu_program_format", g.gpu_program_format()),
        2 => log(out, "caps", g.caps(r.int(24))),
        3 => log(out, "renderer", g.renderer_description()),
        4 => log(out, "window_handle", g.window_handle()),
        5 => {
            let m = r.pick(&["sdr", "hdr", "scrgb"]).unwrap_or("sdr");
            log(out, "set_color_output", g.set_color_output(m, r.f(), r.f()));
        }
        6 => log(out, "color_output", g.color_output()),
        7 => log(out, "hdr_headroom", g.hdr_headroom().to_bits()),
        8 => log(
            out,
            "float_stats",
            g.screen_float_stats().map(|s| s.map(f32::to_bits)),
        ),
        9 => g.show_cursor(r.bool()),
        10 => g.set_window_grab(r.bool()),
        11 => g.set_relative_mouse(r.bool()),
        12 => g.set_window_caption(&r.word()),
        13 => log(out, "mouse_focus", g.window_mouse_focus()),
        14 => log(out, "input_focus", g.window_input_focus()),
        15 => log(out, "visible", g.window_is_visible()),
        16 => log(out, "multisampling", g.multisampling()),
        17 => log(out, "size_float", g.screen_size_float().map(f32::to_bits)),
        18 => log(out, "size_int", g.screen_size_int()),
        19 => log(out, "check_screen", g.check_update_screen_size()),
        20 => log(
            out,
            "force_size",
            g.force_window_size(r.int(1000) + 1, r.int(1000) + 1),
        ),
        21 => log(out, "fullscreen", g.fullscreen_active()),
        22 => g.set_vsync(r.bool(), r.bool()),
        23 => g.set_multisampling_active(r.bool()),
        24 => g.set_gamma(r.unit() + 0.5),
        25 => log(out, "gamma", g.gamma().to_bits()),
        26 => l.font.push(g.create_font(&r.word())),
        27 => l.prog.push(g.create_program(&r.word())),
        28 => l.shader.push(g.create_shader(&r.word(), r.int(2))),
        29 => l.tex.push(g.create_texture(&r.word(), r.int(6), r.int(3))),
        30 => {
            l.vb.push(g.create_vertex_buffer(r.int(3), r.int(8), r.int(3), r.int(100), r.int(100)))
        }
        31 => l.fb.push(g.create_frame_buffer(&r.word())),
        32 => l.dsb.push(g.create_depth_stencil(r.v2l(), 24, 8)),
        33 => l.oq.push(g.create_occlusion_query()),
        34 => {
            let (f, bg) = (r.int(32) as u32, r.col());
            g.clear_frame_buffer(f);
            g.set_clear_color(bg);
        }
        35 => g.set_clear_depth(r.unit()),
        36 => g.set_clear_stencil(r.int(256)),
        37 => g.flush_rendering(),
        38 => g.wait_and_finish_rendering(),
        39 => g.swap_buffers(),
        40 => tex!(|t| g.copy_frame_buffer_to_texture(t, r.v2l(), r.v2l(), r.v2l())),
        41 => {
            let neg = r.bool();
            let b = g.copy_frame_buffer_to_bitmap(
                r.v2l().map(|x| x.max(0)),
                if neg {
                    [-1, -1]
                } else {
                    [r.int(8) + 1, r.int(8) + 1]
                },
            );
            log(
                out,
                "bitmap",
                (
                    b.size,
                    b.format,
                    b.images
                        .iter()
                        .flatten()
                        .map(|d| fnv(d))
                        .collect::<Vec<_>>(),
                ),
            );
        }
        42 => {
            let f = r.opt(&l.fb.clone());
            g.set_current_frame_buffer(f, r.v2l(), r.v2l());
        }
        43 => log(out, "current_fb", g.current_frame_buffer()),
        44 => {
            let n = r.int(6);
            let t: Vec<i32> = (0..n).map(|_| r.int(5)).collect();
            g.set_frame_buffer_draw_targets(&t, n);
        }
        45 => g.set_color_write_active(r.bool(), r.bool(), r.bool(), r.bool()),
        46 => g.set_depth_write_active(r.bool()),
        47 => g.set_cull_active(r.bool()),
        48 => g.set_cull_mode(r.int(2)),
        49 => g.set_depth_test_active(r.bool()),
        50 => g.set_depth_test_func(r.int(8)),
        51 => g.set_alpha_test_active(r.bool()),
        52 => g.set_alpha_test_func(r.int(8), r.unit()),
        53 => g.set_stencil_active(r.bool()),
        54 => g.set_stencil_write_mask(r.next() as u32),
        55 => g.set_stencil(
            r.int(8),
            r.int(256),
            r.next() as u32,
            r.int(8),
            r.int(8),
            r.int(8),
        ),
        56 => {
            let ops = [r.int(8), r.int(8), r.int(8), r.int(8), r.int(8), r.int(8)];
            g.set_stencil_two_side(r.int(8), r.int(8), r.int(256), r.next() as u32, ops);
        }
        57 => g.set_scissor_active(r.bool()),
        58 => g.set_scissor_rect(r.v2l(), r.v2l()),
        59 => {
            let i = r.int(10) - 1;
            g.set_clip_plane(i, r.col());
        }
        60 => {
            let i = r.int(10) - 1;
            log(out, "clip_plane", g.clip_plane(i).map(f32::to_bits));
        }
        61 => g.set_clip_plane_active(r.int(8), r.bool()),
        62 => g.set_color(r.col()),
        63 => g.set_blend_active(r.bool()),
        64 => g.set_blend_func(r.int(10), r.int(10)),
        65 => g.set_blend_func_separate(r.int(10), r.int(10), r.int(10), r.int(10)),
        66 => g.set_polygon_offset_active(r.bool()),
        67 => g.set_depth_clamp_active(r.bool()),
        68 => g.set_polygon_offset(r.f(), r.f()),
        69 => g.push_matrix(r.int(3)),
        70 => g.pop_matrix(r.int(3)),
        71 => g.set_identity_matrix(r.int(3)),
        72 => g.set_matrix(r.int(3), &r.mat()),
        73 => g.set_ortho_size(r.v2(), r.f(), r.f()),
        74 => g.set_ortho_box(r.v3(), r.v3()),
        75 => {
            let t = r.opt(&l.tex.clone());
            g.set_texture(r.int(16) as u32, t);
        }
        76 => g.set_active_texture_unit(r.int(16) as u32),
        77 => g.set_texture_env(r.int(20), r.int(10)),
        78 => g.set_texture_constant_color(r.col()),
        79 => {
            let b = r.bmp();
            tex!(|t| log(out, "from_bitmap", g.tex_create_from_bitmap(t, &b)));
        }
        80 => {
            let b: Vec<Bmp> = (0..1 + r.below(3)).map(|_| r.bmp()).collect();
            tex!(|t| log(out, "anim", g.tex_create_anim(t, &b)));
        }
        81 => {
            let b: Vec<Bmp> = (0..6).map(|_| r.bmp()).collect();
            tex!(|t| log(out, "cube", g.tex_create_cube(t, &b)));
        }
        82 => {
            let size = [1 + r.int(6), 1 + r.int(6), 1 + r.int(2)];
            let fmt = r.pick(&[1, 2, 3, 4, 5, 6, 7]).unwrap_or(5);
            let n = size.iter().map(|&x| x as usize).product::<usize>()
                * formats::bytes_per_pixel(fmt).max(0) as usize;
            let d = r.bytes(n);
            let none = r.below(6) == 0;
            tex!(|t| log(
                out,
                "raw",
                g.tex_create_raw(t, size, fmt, if none { None } else { Some(&d) })
            ));
        }
        83 => {
            let (off, size) = (
                [r.int(4), r.int(4), r.int(2)],
                [1 + r.int(5), 1 + r.int(5), 1 + r.int(2)],
            );
            let fmt = r.pick(&[1, 2, 3, 4, 5, 6, 7]).unwrap_or(5);
            let n = size.iter().map(|&x| x as usize).product::<usize>()
                * formats::bytes_per_pixel(fmt).max(0) as usize;
            let d = r.bytes(n);
            let none = r.below(6) == 0;
            let lv = r.int(3);
            tex!(|t| g.tex_set_raw(t, lv, off, size, fmt, if none { None } else { Some(&d) }));
        }
        84 => tex!(|t| g.tex_update(t, r.unit())),
        85 => tex!(|t| g.tex_set_filter(t, r.int(3))),
        86 => tex!(|t| g.tex_set_anisotropy(t, r.f())),
        87 => tex!(|t| g.tex_set_wrap_s(t, r.int(4))),
        88 => tex!(|t| g.tex_set_wrap_t(t, r.int(4))),
        89 => tex!(|t| g.tex_set_wrap_r(t, r.int(4))),
        90 => tex!(|t| g.tex_set_wrap_str(t, r.int(4))),
        91 => tex!(|t| g.tex_set_compare_mode(t, r.int(2))),
        92 => tex!(|t| g.tex_set_compare_func(t, r.int(8))),
        93 => tex!(|t| g.tex_set_red_from_alpha(t, r.bool())),
        94 => tex!(|t| g.tex_auto_mipmaps(t)),
        95 => tex!(|t| log(out, "has_anim", g.tex_has_animation(t))),
        96 => tex!(|t| g.tex_next_frame(t)),
        97 => tex!(|t| g.tex_prev_frame(t)),
        98 => tex!(|t| log(out, "time_count", g.tex_time_count(t).to_bits())),
        99 => tex!(|t| g.tex_set_time_count(t, r.unit())),
        100 => tex!(|t| log(out, "handle", g.tex_handle(t))),
        101 => tex!(|t| log(out, "tex_size", g.tex_size(t))),
        102 => tex!(|t| log(out, "pixels", g.tex_pixels_rgba_float(t).map(|p| p.len()))),
        103 => vb!(|v| {
            let e = r.int(14);
            g.vb_create_element_array(v, e, r.int(3), 1 + r.int(4), r.int(4));
        }),
        104 => vb!(|v| g.vb_add_vec3(v, r.int(14), r.v3())),
        105 => vb!(|v| g.vb_add_vec4(v, r.int(14), r.v3(), r.f())),
        106 => vb!(|v| g.vb_add_color(v, r.int(14), r.col())),
        107 => vb!(|v| g.vb_add_index(v, r.int(1000) as u32)),
        108 => vb!(|v| log(out, "compile", g.vb_compile(v, r.int(8) as u32))),
        109 => vb!(|v| g.vb_update(v, r.int(1 << 14) as u32, r.bool())),
        110 => vb!(|v| g.vb_create_shadow_double(v, r.bool())),
        111 => vb!(|v| g.vb_transform(v, &r.mat())),
        112 => vb!(|v| g.vb_draw(v, r.int(9))),
        113 => vb!(|v| {
            let n = r.int(12);
            let idx: Vec<u32> = (0..n).map(|_| r.int(500) as u32).collect();
            g.vb_draw_indices(v, &idx, n, r.int(9));
        }),
        114 => vb!(|v| g.vb_bind(v)),
        115 => vb!(|v| g.vb_unbind(v)),
        116 => vb!(|v| {
            let c = g.vb_create_copy(v, r.int(3), r.int(3), r.int(1 << 14) as u32);
            l.vb.push(c);
        }),
        117 => vb!(|v| log(out, "bounds", g.vb_bounding_volume(v).0.map(f32::to_bits))),
        118 => vb!(|v| log(
            out,
            "element",
            (
                g.vb_element_num(v, 1),
                g.vb_element_format(v, 1),
                g.vb_element_var(v, 1)
            )
        )),
        119 => vb!(|v| log(out, "counts", (g.vb_vertex_num(v), g.vb_index_num(v)))),
        120 => vb!(|v| g.vb_resize_array(v, r.int(14), r.int(40))),
        121 => vb!(|v| g.vb_resize_indices(v, r.int(40))),
        122 => with!(shader, g.create_shader(&r.word(), 0), |s| {
            let (f, e) = (r.word(), r.word());
            log(out, "shader_file", g.shader_from_file(s, &f, &e, r.bool()));
        }),
        123 => with!(shader, g.create_shader(&r.word(), 1), |s| {
            let (src, e) = (format!("void main() {{ {} }}", r.word()), r.word());
            log(
                out,
                "shader_string",
                g.shader_from_string(s, &src, &e, r.bool()),
            );
        }),
        124 => prog!(|p| {
            let s = r.opt(&l.shader.clone());
            g.prog_set_shader(p, r.int(2), s);
        }),
        125 => prog!(|p| log(out, "link", g.prog_link(p))),
        126 => prog!(|p| log(out, "dump", g.prog_dump_uniforms(p))),
        127 => prog!(|p| g.prog_bind(p)),
        128 => prog!(|p| g.prog_unbind(p)),
        129 => prog!(|p| {
            let n = r.word();
            log(out, "sampler", g.prog_set_sampler_to_unit(p, &n, r.int(16)));
        }),
        130 => prog!(|p| {
            let n = format!("v{}", r.below(6));
            log(out, "var_id", g.prog_variable_id(p, &n));
        }),
        131 => prog!(|p| {
            let n = format!("v{}", r.below(6));
            log(out, "var_as_id", g.prog_variable_as_id(p, &n, r.int(40)));
        }),
        132 => prog!(|p| log(out, "set_int", g.prog_set_int(p, r.int(8), r.int(100)))),
        133 => prog!(|p| log(out, "set_float", g.prog_set_float(p, r.int(8), r.f()))),
        134 => prog!(|p| log(out, "set_vec2", g.prog_set_vec2(p, r.int(8), r.v2()))),
        135 => prog!(|p| log(out, "set_vec3", g.prog_set_vec3(p, r.int(8), r.v3()))),
        136 => prog!(|p| log(out, "set_vec4", g.prog_set_vec4(p, r.int(8), r.col()))),
        137 => prog!(|p| {
            let x: Vec<f32> = (0..r.below(6)).map(|_| r.f()).collect();
            log(out, "set_array", g.prog_set_float_array(p, r.int(8), &x));
        }),
        138 => prog!(|p| {
            let m = r.mat();
            log(out, "set_matrix", g.prog_set_matrix(p, r.int(8), &m));
            log(
                out,
                "set_matrix_api",
                g.prog_set_matrix_api(p, r.int(8), r.int(3), r.int(4)),
            );
        }),
        139 => fb!(|f| {
            let (t, i, m) = (r.opt(&l.tex.clone()), r.int(5), r.int(3));
            g.fb_set_texture_2d(f, i, t, m);
        }),
        140 => fb!(|f| {
            let (t, i, z, m) = (r.opt(&l.tex.clone()), r.int(5), r.int(4), r.int(3));
            g.fb_set_texture_3d(f, i, t, z, m);
        }),
        141 => fb!(|f| {
            let (t, i, z, m) = (r.opt(&l.tex.clone()), r.int(5), r.int(6), r.int(3));
            g.fb_set_texture_cube(f, i, t, z, m);
            let (t, m) = (r.opt(&l.tex.clone()), r.int(3));
            g.fb_set_depth_texture_2d(f, t, m);
            let (t, z, m) = (r.opt(&l.tex.clone()), r.int(6), r.int(3));
            g.fb_set_depth_texture_cube(f, t, z, m);
        }),
        142 => fb!(|f| {
            let d = r.opt(&l.dsb.clone());
            g.fb_set_depth_stencil(f, d);
            log(out, "fb_compile", g.fb_compile_and_validate(f));
            g.fb_post_bind_update(f);
        }),
        143 => with!(oq, g.create_occlusion_query(), |q| {
            g.oq_begin(q);
            g.oq_end(q);
            log(out, "fetch", g.oq_fetch_results(q));
            log(out, "samples", g.oq_sample_count(q));
        }),
        144 => with!(font, g.create_font(&r.word()), |f| {
            let w = format!("{}\u{e9}\u{1f600}", r.word());
            log(
                out,
                "font_file",
                g.font_from_font_file(f, &w, 8 + r.int(20), 32, 255),
            );
            log(out, "font_bitmap", g.font_from_bitmap_file(f, &r.word()));
        }),
        _ => draws(g, r, a - 145),
    }
}

fn draws(g: &mut dyn Gfx, r: &mut Rng, which: u32) {
    let (p, s, c) = (r.v3(), r.v2(), r.col());
    match which {
        0 => g.draw_triangle(&r.vtxs(3)),
        1 => g.draw_quad_pos(p, s, c),
        2 => g.draw_quad_uv(p, s, r.v2(), r.v2(), c),
        3 => g.draw_quad_uv2(p, s, [r.v2(), r.v2(), r.v2(), r.v2()], c),
        4 => g.draw_quad_vtx(&r.vtxs(4)),
        5 => g.draw_quad_vtx_col(&r.vtxs(4), c),
        6 => g.draw_quad_vtx_z(&r.vtxs(4), r.f()),
        7 => g.draw_quad_vtx_z_col(&r.vtxs(4), r.f(), c),
        8 => {
            let n = 4 * r.below(3);
            let extra: Vec<V3f> = (0..n).map(|_| r.v3()).collect();
            g.draw_quad_multitex(&r.vtxs(4), &extra);
        }
        9 => g.draw_line(p, r.v3(), c),
        10 => g.draw_line_cols(p, c, r.v3(), r.col()),
        11 => g.draw_box_min_max(p, r.v3(), c),
        12 => g.draw_sphere(p, r.f().abs(), c),
        13 => g.draw_sphere_axes(p, r.f().abs(), [c, r.col(), r.col()]),
        14 => g.draw_line_quad_rect(r.col(), r.f(), c),
        15 => g.draw_line_quad_pos(p, s, c),
        16 => g.add_vertex_to_batch(&r.vtx()),
        17 => g.add_vertex_to_batch_translated(&r.vtx(), p),
        18 => g.add_vertex_to_batch_matrix(&r.vtx(), &r.mat()),
        19 => {
            let col = if r.below(5) == 0 { None } else { Some(c) };
            g.add_vertex_to_batch_size2d(&r.vtx(), p, col, r.f(), r.f());
        }
        20 => g.add_vertex_to_batch_raw(p, c, r.v3()),
        21 => g.add_index_to_batch(r.int(100)),
        22 => g.add_texcoord_to_batch(r.int(16) as u32, p),
        23 => g.set_batch_texture_unit_active(r.int(16) as u32, r.bool()),
        24 => g.flush_tri_batch(r.int(64) as u32, r.bool()),
        25 => g.flush_quad_batch(r.int(64) as u32, r.bool()),
        _ => g.clear_batch(),
    }
}
