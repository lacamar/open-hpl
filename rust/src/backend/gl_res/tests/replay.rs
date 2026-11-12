//! Replays a logged sequence of `ohpl_gl*` shell calls (the log format of
//! `rust/tests/golden/L2/harness/gen_replay_l3.py`) against the recording fake GL.

use core::ffi::c_void;
use std::collections::HashMap;

use super::replay_gen::dispatch;
use crate::backend::gl_ctx::ctx::Ctx;
use crate::backend::gl_ctx::tests::{fake, fakegl};
use crate::backend::gl_res::tex::{Face, Io, Level};
use crate::backend::gl_res::vb::Data;
use crate::types::{Vector2l, Vector3l};

pub struct Replay {
    handles: HashMap<u32, u64>,
    ranges: HashMap<String, Vec<u8>>,
    dummy: Vec<u8>,
    ctx: Ctx,
}

pub fn fl(t: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(t.strip_prefix('f').unwrap_or(t), 16).unwrap_or(0))
}

fn ints<const N: usize>(t: &str) -> [i32; N] {
    let mut o = [0; N];
    for (d, s) in o.iter_mut().zip(t.split(',')) {
        *d = s.parse().unwrap_or(0);
    }
    o
}

pub fn v2(t: &str) -> Vector2l {
    let [x, y] = ints(t);
    Vector2l { x, y }
}

pub fn v3(t: &str) -> Vector3l {
    let [x, y, z] = ints(t);
    Vector3l { x, y, z }
}

pub fn cstr(t: &str) -> std::ffi::CString {
    let bytes: Vec<u8> = t
        .strip_prefix('x')
        .map(|h| {
            (0..h.len() / 2)
                .filter_map(|i| u8::from_str_radix(h.get(i * 2..i * 2 + 2)?, 16).ok())
                .collect()
        })
        .unwrap_or_default();
    std::ffi::CString::new(bytes).unwrap_or_default()
}

pub fn io(t: &str) -> Io {
    let f: Vec<&str> = t.split(',').collect();
    let n = |i: usize| f.get(i).and_then(|s| s.parse::<i32>().ok()).unwrap_or(0);
    let fv = |i: usize| fl(f.get(i).copied().unwrap_or("f0"));
    Io {
        tex_type: n(0),
        usage: n(1),
        filter: n(2),
        wrap: [n(3), n(4), n(5)],
        compare_mode: n(6),
        compare_func: n(7),
        anim_mode: n(8),
        pixel_format: n(9),
        down_level: n(10) as u32,
        memory: n(11),
        size: Vector3l {
            x: n(12),
            y: n(13),
            z: n(14),
        },
        min_size: Vector3l {
            x: n(15),
            y: n(16),
            z: n(17),
        },
        frame_time: fv(18),
        aniso: fv(19),
        use_mips: n(20) != 0,
        srgb: n(21) != 0,
        compressed: n(22) != 0,
    }
}

impl Replay {
    pub fn new() -> Self {
        let gl = Box::leak(Box::new(fakegl::table()));
        super::super::TEST_GL.set(Some(gl));
        let mut ctx = Ctx::with_table(gl);
        ctx.derive_caps(|n| {
            let name = std::ffi::CString::new(n).unwrap_or_default();
            !unsafe { fake::SDL_GL_GetProcAddress(name.as_ptr()) }.is_null()
        });
        fake::take_trace();
        Self {
            handles: HashMap::new(),
            ranges: HashMap::new(),
            dummy: vec![0; 64],
            ctx,
        }
    }

    pub fn ctx(&mut self) -> &mut Ctx {
        &mut self.ctx
    }

    pub fn handle(&self, t: &str) -> u64 {
        t.strip_prefix('H')
            .and_then(|n| n.parse().ok())
            .and_then(|n| self.handles.get(&n))
            .copied()
            .unwrap_or(0)
    }

    pub fn ptr(&self, t: &str) -> *const c_void {
        match t {
            "null" | "-" => core::ptr::null(),
            "p" => self.dummy.as_ptr().cast(),
            _ => {
                let (label, off) = t.split_once('+').unwrap_or((t, "0"));
                self.ranges
                    .get(label)
                    .map_or(self.dummy.as_ptr().cast(), |v| {
                        v.as_ptr().wrapping_add(off.parse().unwrap_or(0)).cast()
                    })
            }
        }
    }

    pub fn faces(&self, t: &str) -> (Vec<Vec<Level>>, Vec<Face>) {
        let body = t.strip_prefix('F').unwrap_or("");
        let mut levels = Vec::new();
        let mut faces = Vec::new();
        for f in body.split('^').filter(|s| !s.is_empty()) {
            let p: Vec<&str> = f.split(';').collect();
            let lv: Vec<Level> = p
                .get(3)
                .copied()
                .unwrap_or("")
                .split('|')
                .filter(|s| !s.is_empty())
                .map(|l| {
                    let (d, s) = l.rsplit_once('/').unwrap_or((l, "0"));
                    Level {
                        data: self.ptr(d).cast(),
                        size: s.parse().unwrap_or(0),
                    }
                })
                .collect();
            levels.push((
                lv,
                p.first().and_then(|s| s.parse().ok()).unwrap_or(0),
                v3(p.get(1).copied().unwrap_or("0,0,0")),
                p.get(2).and_then(|s| s.parse().ok()).unwrap_or(0),
            ));
        }
        let storage: Vec<Vec<Level>> = levels.iter().map(|l| l.0.clone()).collect();
        for (i, l) in levels.iter().enumerate() {
            faces.push(Face {
                levels: storage.get(i).map_or(core::ptr::null(), |v| v.as_ptr()),
                num_mips: l.1,
                size: l.2,
                format: l.3,
            });
        }
        (storage, faces)
    }

    pub fn vbdata(&self, t: &str) -> Vec<Data> {
        if t == "-" {
            return Vec::new();
        }
        t.split('|')
            .map(|e| {
                let (p, b) = e.rsplit_once(':').unwrap_or((e, "0"));
                Data {
                    ptr: self.ptr(p),
                    bytes: b.parse().unwrap_or(0),
                }
            })
            .collect()
    }

    pub fn ptrs(&self, t: &str) -> Vec<*const c_void> {
        if t == "-" {
            return Vec::new();
        }
        t.split('|').map(|p| self.ptr(p)).collect()
    }

    fn directive(&mut self, line: &str) {
        let mut it = line.split(' ');
        let cmd = it.next().unwrap_or("");
        let a = it.next().unwrap_or("");
        let b = it.next().unwrap_or("");
        let n = b.parse::<i32>().unwrap_or(0);
        match cmd {
            "@range" => {
                let v = vec![0u8; b.parse().unwrap_or(0)];
                fake::add_range(v.as_ptr() as usize, v.len(), a);
                self.ranges.insert(a.to_string(), v);
            }
            "@clear" => {
                self.ranges.clear();
                fake::RANGES.with(|r| r.borrow_mut().clear());
            }
            "@gen" => {
                for k in [&fake::GEN, &fake::SHADERS, &fake::PROGRAMS] {
                    k.with(|c| c.set(0));
                }
            }
            "@fail" => match a {
                "errors" => fake::ERRORS.with(|c| c.set(n)),
                "compile" => fake::FAIL_COMPILE.with(|c| c.set(n)),
                "link" => fake::FAIL_LINK.with(|c| c.set(n)),
                "fbstatus" => fake::FB_STATUS.with(|c| c.set(b.parse().unwrap_or(0))),
                _ => {}
            },
            _ => {}
        }
    }

    pub fn run(&mut self, log: &str) -> usize {
        let mut calls = 0;
        for line in log.lines().filter(|l| !l.is_empty()) {
            if line.starts_with('@') {
                self.directive(line);
                continue;
            }
            let mut toks: Vec<&str> = line.split(' ').collect();
            let name = toks.remove(0);
            let bind = toks
                .iter()
                .position(|t| *t == "=>")
                .map(|i| (toks[i + 1].to_string(), toks.split_off(i)));
            let out = dispatch(self, name, &toks);
            assert!(out.is_some(), "unknown call {name}");
            if let (Some((h, _)), Some(Some(v))) = (bind, out)
                && let Ok(n) = h.trim_start_matches('H').parse::<u32>()
            {
                self.handles.insert(n, v);
            }
            calls += 1;
        }
        calls
    }
}
