//! Replays a log of `ohpl_glctx_*` / `ohpl_glq_*` calls (recorded from the C++ shell) through the ffi.

use core::ffi::c_void;
use std::ffi::CString;

use super::fake::add_range;
use crate::backend::gl_ctx::ctx::{Ctx, WindowCfg};
use crate::backend::gl_ctx::oq::Query;
use crate::types::Vector2l;

const BATCH_STRIDE: usize = 13;
const BATCH_SIZE: usize = 20000;
const UNITS: usize = 16;

pub struct Replay {
    pub ctx: *mut Ctx,
    pub query: Query,
    vtx: Vec<f32>,
    idx: Vec<u32>,
    tc: Vec<Vec<f32>>,
    tcs: Vec<*mut f32>,
    scratch: Vec<u8>,
    caption: CString,
}

impl Replay {
    pub fn new() -> Self {
        let mut vtx = vec![0.0f32; BATCH_STRIDE * BATCH_SIZE];
        let mut idx = vec![0u32; BATCH_SIZE];
        let mut tc: Vec<Vec<f32>> = (0..UNITS).map(|_| vec![0.0f32; 3 * BATCH_SIZE]).collect();
        let mut tcs: Vec<*mut f32> = tc.iter_mut().map(|v| v.as_mut_ptr()).collect();
        add_range(tcs.as_mut_ptr() as usize, 8 * UNITS, "tcs");
        add_range(vtx.as_mut_ptr() as usize, 4 * vtx.len(), "vtx");
        add_range(idx.as_mut_ptr() as usize, 4 * idx.len(), "idx");
        for (i, v) in tc.iter_mut().enumerate() {
            add_range(v.as_mut_ptr() as usize, 4 * v.len(), &format!("tc{i}"));
        }
        Self {
            ctx: core::ptr::null_mut(),
            query: Query::default(),
            vtx,
            idx,
            tc,
            tcs,
            scratch: vec![0u8; 1 << 16],
            caption: CString::default(),
        }
    }

    fn base(&mut self, label: &str) -> usize {
        match label {
            "vtx" => self.vtx.as_mut_ptr() as usize,
            "idx" => self.idx.as_mut_ptr() as usize,
            "tcs" => self.tcs.as_mut_ptr() as usize,
            "p" => self.scratch.as_mut_ptr() as usize,
            "null" => 0,
            l => l
                .strip_prefix("tc")
                .and_then(|n| n.parse::<usize>().ok())
                .and_then(|n| self.tc.get_mut(n))
                .map_or(0, |v| v.as_mut_ptr() as usize),
        }
    }

    fn address(&mut self, tok: &str) -> usize {
        let (label, off) = tok.split_once('+').unwrap_or((tok, "0"));
        self.base(label) + off.parse::<usize>().unwrap_or(0)
    }

    pub fn p_f32(&mut self, tok: &str) -> *const f32 {
        self.address(tok) as *const f32
    }

    pub fn p_cvoid(&mut self, tok: &str) -> *const c_void {
        self.address(tok) as *const c_void
    }

    pub fn p_mvoid(&mut self, tok: &str) -> *mut c_void {
        self.address(tok) as *mut c_void
    }

    pub fn p_tcs(&mut self, tok: &str) -> *const *mut f32 {
        self.address(tok) as *const *mut f32
    }

    pub fn cfg(&mut self, tok: &str) -> (WindowCfg, ()) {
        let (nums, cap) = tok.split_once(",s:").unwrap_or((tok, ""));
        let n: Vec<i32> = nums.split(',').map(|v| v.parse().unwrap_or(0)).collect();
        let at = |i: usize| n.get(i).copied().unwrap_or(0);
        self.caption = CString::new(cap).unwrap_or_default();
        let cfg = WindowCfg {
            width: at(0),
            height: at(1),
            display: at(2),
            bpp: at(3),
            fullscreen: at(4) != 0,
            multisampling: at(5),
            headless: at(6) != 0,
            caption: self.caption.as_ptr(),
            out_w: 0,
            out_h: 0,
            out_fallback: false,
        };
        (cfg, ())
    }

    pub fn run(&mut self, log: &str) {
        for line in log.lines() {
            if let Some(n) = line.strip_prefix("@gen ") {
                super::fake::GEN.with(|g| g.set(n.parse().unwrap_or(0)));
                continue;
            }
            let mut it = line.split(' ');
            let name = it.next().unwrap_or("");
            let toks: Vec<&str> = it.collect();
            assert!(super::replay_gen::dispatch(self, name, &toks), "{name}");
        }
    }
}

pub fn fl(tok: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(tok.trim_start_matches('f'), 16).unwrap_or(0))
}

pub fn v2(tok: &str) -> Vector2l {
    let (x, y) = tok.split_once(',').unwrap_or(("0", "0"));
    Vector2l {
        x: x.parse().unwrap_or(0),
        y: y.parse().unwrap_or(0),
    }
}

fn items(tok: &str) -> impl Iterator<Item = &str> {
    tok.trim_matches(['[', ']'])
        .split(';')
        .filter(|s| !s.is_empty())
}

pub fn list<T: std::str::FromStr + Default>(tok: &str) -> Vec<T> {
    items(tok).map(|s| s.parse().unwrap_or_default()).collect()
}

pub fn arr<const N: usize>(tok: &str) -> [i32; N] {
    let mut out = [0; N];
    for (o, v) in out.iter_mut().zip(list::<i32>(tok)) {
        *o = v;
    }
    out
}

pub fn farr<const N: usize>(tok: &str) -> [f32; N] {
    let mut out = [0.0; N];
    for (o, v) in out.iter_mut().zip(items(tok)) {
        *o = fl(v);
    }
    out
}
