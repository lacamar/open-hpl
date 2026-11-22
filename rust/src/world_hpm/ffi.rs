//! C ABI for the `.hpm` parser (`HPL2/core/include/resources/RustHpm.h`): the C++ engine passes a
//! table of builder callbacks and gets the map driven through them.

use super::abi::{self, Cstr};
use super::el::El;
use super::parse::{Builder, FileRead, parse_map};
use crate::scene::efl_hpm::HpmEnv;
use crate::scene::wd::World;
use std::ffi::c_void;

type ReadSink = unsafe extern "C" fn(*mut c_void, *const u8, usize);

#[repr(C)]
pub struct TreeSink {
    ctx: *mut c_void,
    begin: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    attr: unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize),
    end: unsafe extern "C" fn(*mut c_void),
}

/// Callbacks into the C++ builder; `ctx` is passed back as the first argument of each.
#[repr(C)]
pub struct Vt {
    ctx: *mut c_void,
    world: *mut World,
    now_ms: unsafe extern "C" fn(*mut c_void) -> u64,
    object_done: unsafe extern "C" fn(*mut c_void),
    warn: unsafe extern "C" fn(*mut c_void, Cstr),
    log: unsafe extern "C" fn(*mut c_void, Cstr),
    read_file: unsafe extern "C" fn(*mut c_void, *const u32, usize, ReadSink, *mut c_void) -> i32,
    dir_light: unsafe extern "C" fn(*mut c_void, *const abi::DirLight),
    env_particles: unsafe extern "C" fn(*mut c_void, *const abi::EnvParticles),
    post_effects: unsafe extern "C" fn(*mut c_void, *const abi::PostEffects),
    sky_box: unsafe extern "C" fn(*mut c_void, *const abi::SkyBox),
    static_object: unsafe extern "C" fn(*mut c_void, *const abi::StaticObject) -> bool,
    plane: unsafe extern "C" fn(*mut c_void, *const abi::Plane) -> bool,
    entity: unsafe extern "C" fn(*mut c_void, *const abi::Entity, *const El) -> bool,
    area: unsafe extern "C" fn(*mut c_void, *const abi::Area, *const El) -> bool,
    decal: unsafe extern "C" fn(*mut c_void, *const abi::Decal, *const El) -> bool,
    detail_meshes: unsafe extern "C" fn(*mut c_void, *const abi::DetailMeshes) -> i32,
    exposure_area: unsafe extern "C" fn(*mut c_void, *const abi::ExposureArea),
    terrain: unsafe extern "C" fn(*mut c_void, *const abi::Terrain),
    track_report:
        unsafe extern "C" fn(*mut c_void, Cstr, i32, i32, i32, bool, *const abi::Skip, usize),
}

struct VtBuilder<'a> {
    vt: &'a Vt,
    env: HpmEnv,
}

unsafe extern "C" fn collect(ctx: *mut c_void, p: *const u8, n: usize) {
    // SAFETY: `ctx` is the `Vec` handed to `read_file` and `p` holds `n` bytes.
    unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(crate::string_ffi::units(p, n)) }
}

fn msg(m: &[u8]) -> std::ffi::CString {
    std::ffi::CString::new(crate::xml::c_str(m)).unwrap_or_default()
}

// SAFETY (all calls below): the table was built by the C++ bridge for exactly these signatures
// and every pointer passed lives across the call.
impl Builder for VtBuilder<'_> {
    fn now_ms(&mut self) -> u64 {
        unsafe { (self.vt.now_ms)(self.vt.ctx) }
    }

    fn object_done(&mut self) {
        unsafe { (self.vt.object_done)(self.vt.ctx) }
    }

    fn warn(&mut self, m: &[u8]) {
        unsafe { (self.vt.warn)(self.vt.ctx, msg(m).as_ptr()) }
    }

    fn log(&mut self, m: &[u8]) {
        unsafe { (self.vt.log)(self.vt.ctx, msg(m).as_ptr()) }
    }

    fn read_file(&mut self, path: &[u32]) -> FileRead {
        let mut data = Vec::new();
        let status = unsafe {
            (self.vt.read_file)(
                self.vt.ctx,
                path.as_ptr(),
                path.len(),
                collect,
                (&raw mut data).cast(),
            )
        };
        match status {
            0 => FileRead::NotOpened,
            1 => FileRead::Read(data),
            _ => FileRead::Short(data),
        }
    }

    fn global_fog(&mut self, v: &abi::Fog) {
        self.env.global_fog(v);
    }

    fn distance_cull(&mut self, v: &abi::DistanceCull) {
        self.env.distance_cull(v);
    }

    fn light(&mut self, el: &El) -> bool {
        self.env.light(el)
    }

    fn sound(&mut self, el: &El, active: bool) -> bool {
        self.env.sound(el, active)
    }

    fn billboard(&mut self, el: &El) -> bool {
        self.env.billboard(el)
    }

    fn particle_system(&mut self, el: &El, connect_light: Cstr) -> bool {
        // SAFETY: the parser passes a NUL-terminated string.
        let light = unsafe { std::ffi::CStr::from_ptr(connect_light) };
        self.env.particle_system(el, light.to_bytes())
    }

    fn fog_area(&mut self, el: &El) -> bool {
        self.env.fog_area(el)
    }

    fn lens_flare(&mut self, el: &El) -> bool {
        self.env.lens_flare(el)
    }

    fn light_mask_box(&mut self, v: &abi::LightMask) {
        self.env.light_mask_box(v);
    }

    fn connect_lights(&mut self) -> (i32, i32) {
        self.env.connect_lights()
    }

    fn connect_light_masks(&mut self) {
        self.env.connect_light_masks();
    }

    fn dir_light(&mut self, v: &abi::DirLight) {
        unsafe { (self.vt.dir_light)(self.vt.ctx, v) }
    }

    fn env_particles(&mut self, v: &abi::EnvParticles) {
        unsafe { (self.vt.env_particles)(self.vt.ctx, v) }
    }

    fn post_effects(&mut self, v: &abi::PostEffects) {
        unsafe { (self.vt.post_effects)(self.vt.ctx, v) }
    }

    fn sky_box(&mut self, v: &abi::SkyBox) {
        unsafe { (self.vt.sky_box)(self.vt.ctx, v) }
    }

    fn static_object(&mut self, v: &abi::StaticObject) -> bool {
        unsafe { (self.vt.static_object)(self.vt.ctx, v) }
    }

    fn plane(&mut self, v: &abi::Plane) -> bool {
        unsafe { (self.vt.plane)(self.vt.ctx, v) }
    }

    fn entity(&mut self, v: &abi::Entity, el: &El) -> bool {
        unsafe { (self.vt.entity)(self.vt.ctx, v, el) }
    }

    fn area(&mut self, v: &abi::Area, el: &El) -> bool {
        unsafe { (self.vt.area)(self.vt.ctx, v, el) }
    }

    fn decal(&mut self, v: &abi::Decal, mesh: Option<&El>) -> bool {
        let mesh = mesh.map_or(std::ptr::null(), std::ptr::from_ref);
        unsafe { (self.vt.decal)(self.vt.ctx, v, mesh) }
    }

    fn detail_meshes(&mut self, v: &abi::DetailMeshes) -> i32 {
        unsafe { (self.vt.detail_meshes)(self.vt.ctx, v) }
    }

    fn exposure_area(&mut self, v: &abi::ExposureArea) {
        unsafe { (self.vt.exposure_area)(self.vt.ctx, v) }
    }

    fn terrain(&mut self, v: &abi::Terrain) {
        unsafe { (self.vt.terrain)(self.vt.ctx, v) }
    }

    fn track_report(
        &mut self,
        track: Cstr,
        in_xml: i32,
        created: i32,
        time_ms: i32,
        file_missing: bool,
        skipped: &[abi::Skip],
    ) {
        unsafe {
            (self.vt.track_report)(
                self.vt.ctx,
                track,
                in_xml,
                created,
                time_ms,
                file_missing,
                skipped.as_ptr(),
                skipped.len(),
            )
        }
    }
}

/// Parses the map at `base` (a wide path) and its sidecars, driving the builder behind `vt`.
///
/// # Safety
/// `vt` holds callbacks matching [`Vt`]; `base` points to `n` readable wide characters.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_hpm_parse(vt: *const Vt, base: *const u32, n: usize) {
    // SAFETY: caller contract.
    let (vt, base) = unsafe { (&*vt, crate::string_ffi::units(base, n)) };
    // SAFETY: the table carries a live world.
    let env = unsafe { HpmEnv::new(vt.world) };
    parse_map(&mut VtBuilder { vt, env }, base);
}

fn walk(el: &El, s: &TreeSink) {
    // SAFETY: the sink contract of `ohpl_hpm_el_walk`.
    unsafe { (s.begin)(s.ctx, el.name.as_ptr(), el.name.len()) };
    for (k, v) in &el.attrs {
        // SAFETY: as above.
        unsafe { (s.attr)(s.ctx, k.as_ptr(), k.len(), v.as_ptr(), v.len()) };
    }
    for c in &el.children {
        walk(c, s);
    }
    // SAFETY: as above.
    unsafe { (s.end)(s.ctx) };
}

/// Replays an element handed to a builder callback: begin, attributes, children, end.
///
/// # Safety
/// `el` is an element passed to the current builder callback; `sink` holds callbacks accepting
/// its `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_hpm_el_walk(el: *const El, sink: *const TreeSink) {
    // SAFETY: caller contract.
    let (el, sink) = unsafe { (&*el, &*sink) };
    walk(el, sink);
}
