//! C ABI for the `.hpm` parser (`HPL2/core/include/resources/RustHpm.h`): the C++ engine passes a
//! table of builder callbacks and gets the map driven through them.

use super::abi::{self, Cstr};
use super::el::El;
use super::parse::{Builder, FileRead, parse_map};
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
    now_ms: unsafe extern "C" fn(*mut c_void) -> u64,
    object_done: unsafe extern "C" fn(*mut c_void),
    warn: unsafe extern "C" fn(*mut c_void, Cstr),
    log: unsafe extern "C" fn(*mut c_void, Cstr),
    read_file: unsafe extern "C" fn(*mut c_void, *const u32, usize, ReadSink, *mut c_void) -> i32,
    global_fog: unsafe extern "C" fn(*mut c_void, *const abi::Fog),
    distance_cull: unsafe extern "C" fn(*mut c_void, *const abi::DistanceCull),
    dir_light: unsafe extern "C" fn(*mut c_void, *const abi::DirLight),
    env_particles: unsafe extern "C" fn(*mut c_void, *const abi::EnvParticles),
    post_effects: unsafe extern "C" fn(*mut c_void, *const abi::PostEffects),
    sky_box: unsafe extern "C" fn(*mut c_void, *const abi::SkyBox),
    static_object: unsafe extern "C" fn(*mut c_void, *const abi::StaticObject) -> bool,
    plane: unsafe extern "C" fn(*mut c_void, *const abi::Plane) -> bool,
    entity: unsafe extern "C" fn(*mut c_void, *const abi::Entity, *const El) -> bool,
    light: unsafe extern "C" fn(*mut c_void, *const El) -> bool,
    area: unsafe extern "C" fn(*mut c_void, *const abi::Area, *const El) -> bool,
    sound: unsafe extern "C" fn(*mut c_void, *const El, bool) -> bool,
    decal: unsafe extern "C" fn(*mut c_void, *const abi::Decal, *const El) -> bool,
    billboard: unsafe extern "C" fn(*mut c_void, *const El) -> bool,
    particle_system: unsafe extern "C" fn(*mut c_void, *const El, Cstr) -> bool,
    fog_area: unsafe extern "C" fn(*mut c_void, *const El) -> bool,
    lens_flare: unsafe extern "C" fn(*mut c_void, *const El) -> bool,
    light_mask_box: unsafe extern "C" fn(*mut c_void, *const abi::LightMask),
    connect_lights: unsafe extern "C" fn(*mut c_void, *mut i32, *mut i32),
    connect_light_masks: unsafe extern "C" fn(*mut c_void),
    detail_meshes: unsafe extern "C" fn(*mut c_void, *const abi::DetailMeshes) -> i32,
    exposure_area: unsafe extern "C" fn(*mut c_void, *const abi::ExposureArea),
    terrain: unsafe extern "C" fn(*mut c_void, *const abi::Terrain),
    track_report:
        unsafe extern "C" fn(*mut c_void, Cstr, i32, i32, i32, bool, *const abi::Skip, usize),
}

struct VtBuilder<'a>(&'a Vt);

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
        unsafe { (self.0.now_ms)(self.0.ctx) }
    }

    fn object_done(&mut self) {
        unsafe { (self.0.object_done)(self.0.ctx) }
    }

    fn warn(&mut self, m: &[u8]) {
        unsafe { (self.0.warn)(self.0.ctx, msg(m).as_ptr()) }
    }

    fn log(&mut self, m: &[u8]) {
        unsafe { (self.0.log)(self.0.ctx, msg(m).as_ptr()) }
    }

    fn read_file(&mut self, path: &[u32]) -> FileRead {
        let mut data = Vec::new();
        let status = unsafe {
            (self.0.read_file)(
                self.0.ctx,
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
        unsafe { (self.0.global_fog)(self.0.ctx, v) }
    }

    fn distance_cull(&mut self, v: &abi::DistanceCull) {
        unsafe { (self.0.distance_cull)(self.0.ctx, v) }
    }

    fn dir_light(&mut self, v: &abi::DirLight) {
        unsafe { (self.0.dir_light)(self.0.ctx, v) }
    }

    fn env_particles(&mut self, v: &abi::EnvParticles) {
        unsafe { (self.0.env_particles)(self.0.ctx, v) }
    }

    fn post_effects(&mut self, v: &abi::PostEffects) {
        unsafe { (self.0.post_effects)(self.0.ctx, v) }
    }

    fn sky_box(&mut self, v: &abi::SkyBox) {
        unsafe { (self.0.sky_box)(self.0.ctx, v) }
    }

    fn static_object(&mut self, v: &abi::StaticObject) -> bool {
        unsafe { (self.0.static_object)(self.0.ctx, v) }
    }

    fn plane(&mut self, v: &abi::Plane) -> bool {
        unsafe { (self.0.plane)(self.0.ctx, v) }
    }

    fn entity(&mut self, v: &abi::Entity, el: &El) -> bool {
        unsafe { (self.0.entity)(self.0.ctx, v, el) }
    }

    fn light(&mut self, el: &El) -> bool {
        unsafe { (self.0.light)(self.0.ctx, el) }
    }

    fn area(&mut self, v: &abi::Area, el: &El) -> bool {
        unsafe { (self.0.area)(self.0.ctx, v, el) }
    }

    fn sound(&mut self, el: &El, active: bool) -> bool {
        unsafe { (self.0.sound)(self.0.ctx, el, active) }
    }

    fn decal(&mut self, v: &abi::Decal, mesh: Option<&El>) -> bool {
        let mesh = mesh.map_or(std::ptr::null(), std::ptr::from_ref);
        unsafe { (self.0.decal)(self.0.ctx, v, mesh) }
    }

    fn billboard(&mut self, el: &El) -> bool {
        unsafe { (self.0.billboard)(self.0.ctx, el) }
    }

    fn particle_system(&mut self, el: &El, connect_light: Cstr) -> bool {
        unsafe { (self.0.particle_system)(self.0.ctx, el, connect_light) }
    }

    fn fog_area(&mut self, el: &El) -> bool {
        unsafe { (self.0.fog_area)(self.0.ctx, el) }
    }

    fn lens_flare(&mut self, el: &El) -> bool {
        unsafe { (self.0.lens_flare)(self.0.ctx, el) }
    }

    fn light_mask_box(&mut self, v: &abi::LightMask) {
        unsafe { (self.0.light_mask_box)(self.0.ctx, v) }
    }

    fn connect_lights(&mut self) -> (i32, i32) {
        let (mut billboards, mut particles) = (0, 0);
        unsafe { (self.0.connect_lights)(self.0.ctx, &mut billboards, &mut particles) };
        (billboards, particles)
    }

    fn connect_light_masks(&mut self) {
        unsafe { (self.0.connect_light_masks)(self.0.ctx) }
    }

    fn detail_meshes(&mut self, v: &abi::DetailMeshes) -> i32 {
        unsafe { (self.0.detail_meshes)(self.0.ctx, v) }
    }

    fn exposure_area(&mut self, v: &abi::ExposureArea) {
        unsafe { (self.0.exposure_area)(self.0.ctx, v) }
    }

    fn terrain(&mut self, v: &abi::Terrain) {
        unsafe { (self.0.terrain)(self.0.ctx, v) }
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
            (self.0.track_report)(
                self.0.ctx,
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
    parse_map(&mut VtBuilder(vt), base);
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
