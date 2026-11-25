//! C ABI of the tone mapping object, mirrored in `soma/src/game/RustSomaS8b.h`: one `call` entry of
//! the shell answers every host op.

use super::s8b_tone::{self as tone, Host, Ptr};
use super::tonemap::ExposureArea;
use crate::somascript::s2_as::Engine;
use std::ffi::c_void;
use std::sync::OnceLock;

#[repr(C)]
pub struct Io {
    pub(super) p: [u64; 2],
    pub(super) i: [i32; 4],
    pub(super) f: [f32; 24],
    pub(super) s: *const u8,
    pub(super) n: usize,
    pub(super) r: u64,
}

impl Io {
    pub(super) fn new() -> Self {
        Self {
            p: [0; 2],
            i: [0; 4],
            f: [0.0; 24],
            s: std::ptr::null(),
            n: 0,
            r: 0,
        }
    }

    pub(super) fn text(&self) -> Vec<u8> {
        if self.s.is_null() {
            return Vec::new();
        }
        // SAFETY: the host points `s` at `n` bytes valid until its next call.
        unsafe { std::slice::from_raw_parts(self.s, self.n) }.to_vec()
    }
}

#[repr(C)]
pub struct HostTable {
    ctx: *mut c_void,
    call: unsafe extern "C" fn(*mut c_void, u32, *mut Io),
}

// SAFETY: installed once, lives for the process, used on the main thread only.
unsafe impl Sync for HostTable {}
// SAFETY: as above.
unsafe impl Send for HostTable {}

static HOST: OnceLock<&'static HostTable> = OnceLock::new();

pub(super) fn host_call(op: u32, io: &mut Io) -> bool {
    let Some(h) = HOST.get() else {
        return false;
    };
    // SAFETY: the table was installed by the host and `io` outlives the call.
    unsafe { (h.call)(h.ctx, op, io) };
    true
}

mod op {
    pub const GRADING_TEX: u32 = 1;
    pub const NOISE_TEX: u32 = 2;
    pub const TEX_DESTROY: u32 = 3;
    pub const TEX_NAME: u32 = 4;
    pub const WORLD_GRADING_NAME: u32 = 5;
    pub const WORLD_TONE: u32 = 6;
    pub const AREA_COUNT: u32 = 7;
    pub const AREA_GET: u32 = 8;
    pub const CURRENT_WORLD: u32 = 9;
    pub const CURRENT_VIEWPORT: u32 = 10;
    pub const TONE_OBJECT: u32 = 11;
    pub const CAMERA_POS: u32 = 12;
    pub const SET_TONE: u32 = 13;
    pub const SET_LAYERS: u32 = 14;
    pub const SET_GRADING: u32 = 15;
    pub const USER_BLOOM: u32 = 16;
    pub const SET_BLOOM: u32 = 17;
    pub const SET_GRAIN: u32 = 18;
    pub const SET_SRGB: u32 = 19;
}

struct CHost(&'static HostTable);

impl CHost {
    fn go(&self, op: u32, mut io: Io) -> Io {
        // SAFETY: the table was installed by the host and `io` outlives the call.
        unsafe { (self.0.call)(self.0.ctx, op, &mut io) };
        io
    }

    fn ptr(&self, op: u32, p: Ptr) -> Io {
        let mut io = Io::new();
        io.p[0] = p as u64;
        self.go(op, io)
    }

    fn named(&self, op: u32, name: &[u8]) -> Io {
        let mut io = Io::new();
        io.s = name.as_ptr();
        io.n = name.len();
        self.go(op, io)
    }
}

impl Host for CHost {
    fn grading_tex(&self, name: &[u8]) -> Ptr {
        self.named(op::GRADING_TEX, name).r as Ptr
    }
    fn noise_tex(&self) -> Ptr {
        self.go(op::NOISE_TEX, Io::new()).r as Ptr
    }
    fn tex_destroy(&self, t: Ptr) {
        self.ptr(op::TEX_DESTROY, t);
    }
    fn tex_name(&self, t: Ptr) -> Option<Vec<u8>> {
        let io = self.ptr(op::TEX_NAME, t);
        (io.r != 0).then(|| io.text())
    }
    fn world_grading_name(&self, w: Ptr) -> Vec<u8> {
        self.ptr(op::WORLD_GRADING_NAME, w).text()
    }
    fn world_tone(&self, w: Ptr) -> [f32; 3] {
        let io = self.ptr(op::WORLD_TONE, w);
        [io.f[0], io.f[1], io.f[2]]
    }
    fn world_areas(&self, w: Ptr) -> Vec<ExposureArea> {
        let n = self.ptr(op::AREA_COUNT, w).r as usize;
        (0..n)
            .map(|k| {
                let mut io = Io::new();
                io.p[0] = w as u64;
                io.i[0] = k as i32;
                let f = self.go(op::AREA_GET, io).f;
                let mut m = [[0.0f32; 4]; 4];
                for (row, c) in m.iter_mut().zip(f.as_chunks::<4>().0.iter()) {
                    row.copy_from_slice(c);
                }
                ExposureArea {
                    inv_transform: crate::types::Matrixf { m },
                    half_size: [f[16], f[17], f[18]],
                    exposure: f[19],
                    white_cut: f[20],
                    transition_time: f[21],
                }
            })
            .collect()
    }
    fn current_world(&self) -> Ptr {
        self.go(op::CURRENT_WORLD, Io::new()).r as Ptr
    }
    fn current_viewport(&self) -> Ptr {
        self.go(op::CURRENT_VIEWPORT, Io::new()).r as Ptr
    }
    fn tone_object(&self) -> Ptr {
        self.go(op::TONE_OBJECT, Io::new()).r as Ptr
    }
    fn camera_pos(&self) -> Option<[f32; 3]> {
        let io = self.go(op::CAMERA_POS, Io::new());
        (io.r != 0).then(|| [io.f[0], io.f[1], io.f[2]])
    }
    fn set_tone(&self, key: f32, scale: f32, white_cut: f32, gamma: f32) {
        let mut io = Io::new();
        io.f[..4].copy_from_slice(&[key, scale, white_cut, gamma]);
        self.go(op::SET_TONE, io);
    }
    fn set_layers(&self, layers: &[(Ptr, f32)]) {
        let mut buf = Vec::with_capacity(layers.len() * 16);
        for (t, a) in layers {
            buf.extend_from_slice(&(*t as u64).to_ne_bytes());
            buf.extend_from_slice(&a.to_ne_bytes());
            buf.extend_from_slice(&[0; 4]);
        }
        let mut io = Io::new();
        io.s = buf.as_ptr();
        io.n = layers.len();
        self.go(op::SET_LAYERS, io);
    }
    fn set_grading(&self, tex: Ptr, target: Ptr, blend: f32) {
        let mut io = Io::new();
        io.p = [tex as u64, target as u64];
        io.f[0] = blend;
        self.go(op::SET_GRADING, io);
    }
    fn user_bloom(&self) -> bool {
        self.go(op::USER_BLOOM, Io::new()).r != 0
    }
    fn set_bloom(&self, active: bool, bright_pass: f32, width: f32, tint: [f32; 4]) {
        let mut io = Io::new();
        io.i[0] = i32::from(active);
        io.f[0] = bright_pass;
        io.f[1] = width;
        io.f[2..6].copy_from_slice(&tint);
        self.go(op::SET_BLOOM, io);
    }
    fn set_grain(&self, tex: Ptr, intensity: f32) {
        let mut io = Io::new();
        io.p[0] = tex as u64;
        io.f[0] = intensity;
        self.go(op::SET_GRAIN, io);
    }
    fn set_srgb(&self, on: bool) {
        let mut io = Io::new();
        io.i[0] = i32::from(on);
        self.go(op::SET_SRGB, io);
    }
}

fn with_host(f: impl FnOnce(&dyn Host)) {
    if let Some(h) = HOST.get() {
        f(&CHost(h));
    }
}

/// # Safety
/// `host` points at a table that lives for the whole process.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8b_tone_install(host: *const HostTable) {
    // SAFETY: caller contract.
    if let Some(h) = unsafe { host.as_ref() } {
        let _ = HOST.set(h);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8b_tone_new() {
    tone::create();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8b_tone_map_loaded(world: usize) {
    with_host(|h| tone::map_loaded(h, world));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8b_tone_update(dt: f32) {
    with_host(|h| tone::update(h, dt));
}

/// # Safety
/// `exposure` and `white_cut` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8b_tone_world(exposure: *mut f32, white_cut: *mut f32) -> bool {
    let Some((e, w)) = tone::world_exposure() else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        exposure.write(e);
        white_cut.write(w);
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8b_tone_fade_exposure(exposure: f32, white_cut: f32, time: f32) {
    tone::fade_exposure(exposure, white_cut, time);
}

/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8b_tone_register(engine: *mut c_void) {
    if let Some(h) = HOST.get() {
        let h: &'static CHost = Box::leak(Box::new(CHost(h)));
        tone::register(Engine(engine), h);
    }
}
