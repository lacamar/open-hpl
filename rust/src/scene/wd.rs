//! WD: `cWorld` owned by Rust. The scalar state (fog, sky, tone mapping, subsystem pointers) is a
//! `#[repr(C)]` pod C++ reads in place; entity lists live in the `Reg` the pod points at; creation,
//! destruction and the per-frame update run here against the C++ objects behind `wd_host.rs`.

use super::ffi_h13 as reg;
use super::h13_world::Reg;
use super::ls_host::{self, Vb};
use super::wd_host::{CreateArgs, DumpItem, FieldOut, Host, LoadArgs, host};
use super::world::{self as logic, FieldIn};
use crate::types::{Color, Matrixf, Vector2f, Vector3f};
use std::ffi::c_void;
use std::io::Write;
use std::sync::atomic::{AtomicBool, Ordering};

const LIGHT: u32 = 0;
const DYN_MESH: u32 = 1;
const STATIC_MESH: u32 = 2;
const BILLBOARD: u32 = 3;
const LENS_FLARE: u32 = 4;
const BEAM: u32 = 5;
const PARTICLE_SYSTEM: u32 = 6;
const GUI_SET: u32 = 7;
const ROPE: u32 = 8;
const SOUND: u32 = 9;
const START_POS: u32 = 10;
const FOG_AREA: u32 = 11;
const FORCE_FIELD: u32 = 12;
const DUMMY: u32 = 13;
const AI_CONTAINER: u32 = 14;
const ASTAR: u32 = 15;
const ENV_PARTICLES: u32 = 16;

const STATIC: usize = 0;
const DYNAMIC: usize = 1;
const PS_EMITTER_CAP: usize = 64;
const VB_COLOR0: i32 = ls_host::E_COLOR0;
const VB_FLAG_COLOR0: i32 = ls_host::F_COLOR0;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Pod>() == 416 && offset_of!(Pod, reg) == 128);
    assert!(offset_of!(Pod, sound_id_count) == 388 && offset_of!(Pod, active) == 392);
};

static HPL3_SKYBOX: AtomicBool = AtomicBool::new(false);

type P = *mut c_void;

/// Read in place by C++ (`OhplWdPod` in `RustWd.h`, same order, size asserted on both sides).
#[repr(C)]
pub struct Pod {
    pub graphics: P,
    pub sound: P,
    pub resources: P,
    pub physics: P,
    pub scene: P,
    pub system: P,
    pub ai: P,
    pub physics_world: P,
    pub container: [P; 2],
    pub sky_vb: P,
    pub sky_tex: P,
    pub terrain: P,
    pub dir_light: P,
    pub fog_sky_tex: P,
    pub root: P,
    pub reg: *mut Reg,
    pub owner: P,
    pub world_size: Vector3f,
    pub sky_color: Color,
    pub sky_rotation: Vector3f,
    pub fog_color: Color,
    pub fog2_color: Color,
    pub night_color: Color,
    pub ambient: Color,
    pub fog_noise_turbulence: Vector3f,
    pub sky_brightness: f32,
    pub cull_min: f32,
    pub cull_screen: f32,
    pub cull_random: f32,
    pub cull_max: f32,
    pub cull_fade: f32,
    pub fog_start: f32,
    pub fog_end: f32,
    pub fog_falloff: f32,
    pub fog_brightness: f32,
    pub fog2_brightness: f32,
    pub fog_noise_strength: f32,
    pub fog_noise_size: f32,
    pub fog2_start: f32,
    pub fog2_end: f32,
    pub fog2_falloff: f32,
    pub fog_density: f32,
    pub fog_height_density: f32,
    pub fog_height_horizon: f32,
    pub fog2_density: f32,
    pub fog2_height_density: f32,
    pub fog2_height_horizon: f32,
    pub dof_start: f32,
    pub dof_end: f32,
    pub dof_falloff: f32,
    pub night_brightness: f32,
    pub night_radius_min: f32,
    pub night_radius_max: f32,
    pub night_falloff: f32,
    pub tone_key: f32,
    pub tone_exposure: f32,
    pub tone_white_cut: f32,
    pub sound_id_count: i32,
    pub active: bool,
    pub auto_delete_physics: bool,
    pub sound_emitter: bool,
    pub auto_destroy_sky: bool,
    pub sky_active: bool,
    pub dir_light_active: bool,
    pub env_particles_active: bool,
    pub fog_active: bool,
    pub fog_culling: bool,
    pub cull_active: bool,
    pub fog_underwater: bool,
    pub fog_use_sky: bool,
    pub fog_after_areas: bool,
    pub fog2_active: bool,
    pub fog_lighten: bool,
    pub fog_height_based: bool,
    pub fog_exponential: bool,
    pub dof_active: bool,
}

#[repr(C)]
pub struct World {
    pub pod: Pod,
    name: Vec<u8>,
    ent_files: Vec<usize>,
}

fn col(r: f32, a: f32) -> Color {
    Color { r, g: r, b: r, a }
}

fn h() -> Option<&'static Host> {
    host()
}

fn log(error: bool, msg: String) {
    ls_host::log(error, &msg);
}

fn lossy(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

/// # Safety
/// `p` valid for `n` reads when `n > 0`.
unsafe fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        return &[];
    }
    // SAFETY: caller contract.
    unsafe { std::slice::from_raw_parts(p, n) }
}

struct Cur {
    reg: *mut Reg,
    kind: u32,
    cur: u64,
}

impl Cur {
    /// # Safety
    /// `reg` is the world's live registry.
    unsafe fn new(reg: *mut Reg, kind: u32) -> Cur {
        // SAFETY: caller contract.
        let cur = unsafe { reg::ohpl_h13_cur_begin(reg, kind) };
        Cur { reg, kind, cur }
    }

    fn get(&self) -> usize {
        // SAFETY: `reg` outlives the cursor.
        unsafe { reg::ohpl_h13_cur_get(self.reg, self.kind, self.cur) as usize }
    }

    fn advance(&mut self) {
        // SAFETY: as `get`.
        self.cur = unsafe { reg::ohpl_h13_cur_next(self.reg, self.kind, self.cur) };
    }

    fn erase(&mut self) {
        // SAFETY: as `get`.
        self.cur = unsafe { reg::ohpl_h13_cur_erase(self.reg, self.kind, self.cur) };
    }
}

fn args(name: &[u8]) -> CreateArgs {
    CreateArgs {
        name: name.as_ptr(),
        name_len: name.len(),
        size2: Vector2f::default(),
        ty: 0,
        which: 0,
        flag: false,
        id: 0,
        a: std::ptr::null_mut(),
        b: std::ptr::null_mut(),
    }
}

impl World {
    fn reg(&self) -> *mut Reg {
        self.pod.reg
    }
}

/// # Safety
/// Pointers are the subsystems of the owning `cWorld`; `name` valid for `len` reads.
#[allow(clippy::too_many_arguments)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_new(
    owner: P,
    graphics: P,
    resources: P,
    sound: P,
    physics: P,
    scene: P,
    system: P,
    ai: P,
    name: *const u8,
    len: usize,
) -> *mut World {
    let Some(host) = h() else {
        return std::ptr::null_mut();
    };
    let reg = Box::into_raw(Box::<Reg>::default());
    // SAFETY: table contract; `reg` is a live registry.
    let (root, stat, dynamic, sky_vb) = unsafe {
        (host.reg_foreign)(reg.cast());
        (
            (host.root_new)(),
            (host.cont_new)(false),
            (host.cont_new)(true),
            (host.sky_vb_new)(graphics),
        )
    };
    let pod = Pod {
        graphics,
        sound,
        resources,
        physics,
        scene,
        system,
        ai,
        physics_world: std::ptr::null_mut(),
        container: [stat, dynamic],
        sky_vb,
        sky_tex: std::ptr::null_mut(),
        terrain: std::ptr::null_mut(),
        dir_light: std::ptr::null_mut(),
        fog_sky_tex: std::ptr::null_mut(),
        root,
        reg,
        owner,
        world_size: Vector3f::default(),
        sky_color: col(1.0, 1.0),
        sky_rotation: Vector3f::default(),
        fog_color: col(1.0, 1.0),
        fog2_color: col(0.0, 0.0),
        night_color: col(0.0, 0.0),
        ambient: col(0.0, 0.0),
        fog_noise_turbulence: Vector3f::default(),
        sky_brightness: 1.0,
        cull_min: 0.0,
        cull_screen: 0.05,
        cull_random: 0.25,
        cull_max: 999_999.0,
        cull_fade: 0.5,
        fog_start: 0.0,
        fog_end: 10.0,
        fog_falloff: 1.0,
        fog_brightness: 1.0,
        fog2_brightness: 1.0,
        fog_noise_strength: 0.0,
        fog_noise_size: 1.0,
        fog2_start: 0.0,
        fog2_end: 0.0,
        fog2_falloff: 1.0,
        fog_density: 0.25,
        fog_height_density: 0.25,
        fog_height_horizon: 1.0,
        fog2_density: 0.25,
        fog2_height_density: 0.25,
        fog2_height_horizon: 1.0,
        dof_start: 0.0,
        dof_end: 0.0,
        dof_falloff: 0.0,
        night_brightness: 0.0,
        night_radius_min: 0.0,
        night_radius_max: 0.0,
        night_falloff: 1.0,
        tone_key: 0.5,
        tone_exposure: 0.0,
        tone_white_cut: 3.5,
        sound_id_count: 0,
        active: true,
        auto_delete_physics: false,
        sound_emitter: false,
        auto_destroy_sky: false,
        sky_active: false,
        dir_light_active: false,
        env_particles_active: true,
        fog_active: false,
        fog_culling: true,
        cull_active: false,
        fog_underwater: false,
        fog_use_sky: false,
        fog_after_areas: true,
        fog2_active: false,
        fog_lighten: false,
        fog_height_based: false,
        fog_exponential: false,
        dof_active: false,
    };
    // SAFETY: caller contract.
    let name = unsafe { bytes(name, len) }.to_vec();
    Box::into_raw(Box::new(World {
        pod,
        name,
        ent_files: Vec::new(),
    }))
}

/// # Safety
/// `w` from `ohpl_wd_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_free(w: *mut World) {
    if w.is_null() {
        return;
    }
    let Some(host) = h() else { return };
    // SAFETY: caller contract; each step ends before a destructor can re-enter the world.
    unsafe {
        let p = &(*w).pod as *const Pod;
        if !(*p).dir_light.is_null() {
            (host.dirlight_delete)((*p).dir_light);
        }
        if !(*p).terrain.is_null() {
            (host.terrain_delete)((*p).terrain);
        }
        reg::ohpl_h13_reg_destroy_all((*p).reg, ENV_PARTICLES);
        Vb((*p).sky_vb).free();
        if !(*p).sky_tex.is_null() && (*p).auto_destroy_sky {
            ls_host::tex_destroy((*p).resources as usize, (*p).sky_tex as usize);
        }
        let files = std::mem::take(&mut (*w).ent_files);
        for ef in files {
            (host.entfile_destroy)((*p).resources, ef as P);
        }
        ohpl_wd_destroy_all(w, 0);
        for c in (*p).container {
            if !c.is_null() {
                (host.cont_delete)(c);
            }
        }
        (host.root_delete)((*p).root);
        reg::ohpl_h13_reg_free((*p).reg);
        drop(Box::from_raw(w));
    }
}

/// # Safety
/// `w` live; the length is written to `len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_name(w: *const World, len: *mut usize) -> *const u8 {
    // SAFETY: caller contract.
    unsafe {
        *len = (*w).name.len();
        (*w).name.as_ptr()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_wd_set_hpl3_skybox(on: bool) {
    HPL3_SKYBOX.store(on, Ordering::Relaxed);
}

/// # Safety
/// `w` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_destroy_all(w: *mut World, flags: u32) {
    let Some(host) = h() else { return };
    let plan = logic::destroy_plan(flags);
    // SAFETY: caller contract; destructors may re-enter the world, so only raw reads are held.
    unsafe {
        let r = (*w).reg();
        if plan & logic::DESTROY_STATIC != 0 {
            reg::ohpl_h13_reg_destroy_all(r, STATIC_MESH);
        }
        for kind in [
            DYN_MESH,
            LIGHT,
            BILLBOARD,
            LENS_FLARE,
            BEAM,
            PARTICLE_SYSTEM,
            GUI_SET,
            ROPE,
            FOG_AREA,
            FORCE_FIELD,
            START_POS,
        ] {
            reg::ohpl_h13_reg_destroy_all(r, kind);
        }
        reg::ohpl_h13_area_destroy_all(r);
        reg::ohpl_h13_reg_destroy_all(r, AI_CONTAINER);
        reg::ohpl_h13_reg_destroy_all(r, ASTAR);
        (host.temp_nodes_clear)((*w).pod.owner);
        if plan & logic::DESTROY_PHYSICS != 0 {
            let pw = (*w).pod.physics_world;
            if !pw.is_null() && (*w).pod.auto_delete_physics {
                (host.phys_destroy)((*w).pod.physics, pw);
            }
        }
        reg::ohpl_h13_reg_destroy_all(r, SOUND);
    }
}

unsafe fn add_rend(w: *mut World, rend: P) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract; `rend` is an `iRenderable`.
    unsafe {
        let which = if (host.is_static)(u32::MAX, rend) {
            STATIC
        } else {
            DYNAMIC
        };
        if let Some(&c) = (*w).pod.container.get(which) {
            (host.cont_add)(c, rend);
        }
    }
}

unsafe fn remove_rend(w: *mut World, rend: P) {
    let Some(host) = h() else { return };
    // SAFETY: as `add_rend`.
    unsafe {
        let which = if (host.is_static)(u32::MAX, rend) {
            STATIC
        } else {
            DYNAMIC
        };
        if let Some(&c) = (*w).pod.container.get(which) {
            (host.cont_remove)(c, rend);
        }
    }
}

unsafe fn rend_of(kind: u32, p: P) -> P {
    // SAFETY: table contract.
    h().map_or(std::ptr::null_mut(), |host| unsafe {
        (host.rend_of)(kind, p)
    })
}

unsafe fn emitters(ps: P) -> Vec<P> {
    let mut out = [std::ptr::null_mut(); PS_EMITTER_CAP];
    // SAFETY: table contract; `out` holds `PS_EMITTER_CAP` entries.
    let n = h().map_or(0, |host| unsafe {
        (host.ps_emitters)(ps, out.as_mut_ptr(), PS_EMITTER_CAP)
    });
    out.iter().copied().take(n).collect()
}

unsafe fn mesh_subs(mesh: P, skip_collide: bool) -> Vec<P> {
    let mut out = vec![std::ptr::null_mut(); 256];
    // SAFETY: table contract.
    let n = h().map_or(0, |host| unsafe {
        (host.mesh_subs)(mesh, out.as_mut_ptr(), out.len(), skip_collide)
    });
    out.truncate(n);
    out
}

/// # Safety
/// `w` live.
unsafe fn make(w: *mut World, kind: u32, a: &CreateArgs) -> P {
    // SAFETY: caller contract.
    h().map_or(std::ptr::null_mut(), |host| unsafe {
        (host.create)(kind, &raw mut (*w).pod, a)
    })
}

unsafe fn push(w: *mut World, kind: u32, p: P) {
    // SAFETY: caller contract.
    unsafe { reg::ohpl_h13_reg_add((*w).reg(), kind, p) }
}

unsafe fn finish_renderable(w: *mut World, kind: u32, p: P, is_static: bool) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract.
    unsafe {
        (host.set_static)(kind, p, is_static);
        add_rend(w, rend_of(kind, p));
    }
}

/// # Safety
/// `w` live; `name` and `gobo` valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_create_light(
    w: *mut World,
    which: i32,
    name: *const u8,
    name_len: usize,
    gobo: *const u8,
    gobo_len: usize,
    is_static: bool,
) -> P {
    let Some(host) = h() else {
        return std::ptr::null_mut();
    };
    // SAFETY: caller contract.
    unsafe {
        let mut a = args(bytes(name, name_len));
        a.which = which;
        let light = make(w, LIGHT, &a);
        push(w, LIGHT, light);
        if which < 2 && gobo_len != 0 {
            (host.light_gobo)((*w).pod.resources, which == 1, light, gobo, gobo_len);
        }
        finish_renderable(w, LIGHT, light, is_static);
        (host.set_world)(LIGHT, light, (*w).pod.owner);
        light
    }
}

/// # Safety
/// `w` live; `name` valid; `material` valid when `material_len > 0`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_create_billboard(
    w: *mut World,
    name: *const u8,
    name_len: usize,
    size: *const Vector2f,
    ty: i32,
    material: *const u8,
    material_len: usize,
    is_static: bool,
) -> P {
    // SAFETY: caller contract.
    unsafe {
        let mut a = args(bytes(name, name_len));
        a.size2 = *size;
        a.ty = ty;
        let bb = make(w, BILLBOARD, &a);
        push(w, BILLBOARD, bb);
        if material_len != 0 {
            let mat =
                ls_host::mat_create((*w).pod.resources as usize, bytes(material, material_len));
            if let Some(host) = h() {
                (host.bb_material)(bb, mat as P);
            }
        }
        finish_renderable(w, BILLBOARD, bb, is_static);
        bb
    }
}

/// # Safety
/// `w` live; `name` valid; `material` valid when `material_len > 0`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_create_lens_flare(
    w: *mut World,
    name: *const u8,
    name_len: usize,
    size: *const Vector2f,
    material: *const u8,
    material_len: usize,
    is_static: bool,
) -> P {
    // SAFETY: caller contract.
    unsafe {
        let flare = make(w, LENS_FLARE, &args(bytes(name, name_len)));
        push(w, LENS_FLARE, flare);
        if let Some(host) = h() {
            (host.flare_halo)(flare, size);
            if material_len != 0 {
                let mat =
                    ls_host::mat_create((*w).pod.resources as usize, bytes(material, material_len));
                (host.flare_material)(flare, mat as P);
            }
        }
        finish_renderable(w, LENS_FLARE, flare, is_static);
        flare
    }
}

/// # Safety
/// `w` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_create_simple(
    w: *mut World,
    kind: u32,
    name: *const u8,
    name_len: usize,
    is_static: bool,
    flag: bool,
    a: P,
    id: i32,
) -> P {
    if !matches!(kind, BEAM | GUI_SET | ROPE | FOG_AREA | FORCE_FIELD | DUMMY) {
        return std::ptr::null_mut();
    }
    // SAFETY: caller contract.
    unsafe {
        let mut ar = args(bytes(name, name_len));
        ar.flag = flag;
        ar.a = a;
        ar.id = id;
        let p = make(w, kind, &ar);
        if kind == FORCE_FIELD {
            if let Some(host) = h() {
                (host.set_static)(kind, p, is_static);
            }
            push(w, kind, p);
            return p;
        }
        push(w, kind, p);
        if kind == ROPE {
            add_rend(w, rend_of(kind, p));
        } else {
            finish_renderable(w, kind, p, is_static);
        }
        p
    }
}

/// # Safety
/// `w` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_create_env_particles(
    w: *mut World,
    name: *const u8,
    name_len: usize,
) -> P {
    // SAFETY: caller contract.
    unsafe {
        let p = make(w, ENV_PARTICLES, &args(bytes(name, name_len)));
        push(w, ENV_PARTICLES, p);
        p
    }
}

/// # Safety
/// `w` live; `name` valid; `mesh` a `cMesh` or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_create_mesh(
    w: *mut World,
    name: *const u8,
    name_len: usize,
    mesh: P,
    is_static: bool,
) -> P {
    let Some(host) = h() else {
        return std::ptr::null_mut();
    };
    if mesh.is_null() {
        return std::ptr::null_mut();
    }
    // SAFETY: caller contract.
    unsafe {
        let mut a = args(bytes(name, name_len));
        a.a = mesh;
        let me = make(w, DYN_MESH, &a);
        push(w, if is_static { STATIC_MESH } else { DYN_MESH }, me);
        (host.set_static)(DYN_MESH, me, is_static);
        for sub in mesh_subs(me, true) {
            add_rend(w, sub);
        }
        (host.set_world)(DYN_MESH, me, (*w).pod.owner);
        me
    }
}

/// # Safety
/// `w` live; `mesh` a registered `cMeshEntity` or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_destroy_mesh(w: *mut World, mesh: P) {
    let Some(host) = h() else { return };
    if mesh.is_null() {
        return;
    }
    // SAFETY: caller contract.
    unsafe {
        for sub in mesh_subs(mesh, false) {
            remove_rend(w, sub);
        }
        let kind = if (host.is_static)(DYN_MESH, mesh) {
            STATIC_MESH
        } else {
            DYN_MESH
        };
        reg::ohpl_h13_reg_destroy((*w).reg(), kind, mesh, false);
    }
}

/// # Safety
/// `w` live; `meshes` valid for `n` reads, ascending addresses.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_destroy_static_meshes(
    w: *mut World,
    meshes: *const usize,
    n: usize,
) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract.
    unsafe {
        let set = std::slice::from_raw_parts(meshes, n);
        let mut subs: Vec<usize> = set
            .iter()
            .flat_map(|&m| mesh_subs(m as P, false))
            .map(|s| s as usize)
            .collect();
        subs.sort_unstable();
        subs.dedup();
        (host.cont_remove_set)((*w).pod.container[STATIC], subs.as_ptr(), subs.len());
        reg::ohpl_h13_reg_remove_set((*w).reg(), STATIC_MESH, meshes, n);
        let f = (*(*w).reg()).foreign(STATIC_MESH as usize);
        for &m in set {
            f.destroy(m);
        }
    }
}

/// # Safety
/// `w` live; `mesh` a registered `cMeshEntity`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_make_mesh_dynamic(w: *mut World, mesh: P) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract.
    unsafe {
        if !(host.is_static)(DYN_MESH, mesh) {
            return;
        }
        let subs = mesh_subs(mesh, false);
        for &sub in &subs {
            remove_rend(w, sub);
        }
        reg::ohpl_h13_reg_remove((*w).reg(), STATIC_MESH, mesh);
        reg::ohpl_h13_reg_add((*w).reg(), DYN_MESH, mesh);
        (host.set_static)(DYN_MESH, mesh, false);
        for sub in mesh_subs(mesh, true) {
            add_rend(w, sub);
        }
    }
}

/// # Safety
/// `w` live; `rend` an `iRenderable`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_make_renderable_dynamic(w: *mut World, rend: P) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract.
    unsafe {
        if !(host.is_static)(u32::MAX, rend) {
            return;
        }
        remove_rend(w, rend);
        (host.set_static)(u32::MAX, rend, false);
        add_rend(w, rend);
    }
}

/// Destroys a registered entity; renderable kinds leave their container first.
///
/// # Safety
/// `w` live; `p` an entity of `kind` or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_destroy(w: *mut World, kind: u32, p: P) {
    // SAFETY: caller contract.
    unsafe {
        if matches!(
            kind,
            LIGHT | BILLBOARD | LENS_FLARE | BEAM | ROPE | FOG_AREA | DUMMY
        ) {
            remove_rend(w, rend_of(kind, p));
        }
        reg::ohpl_h13_reg_destroy((*w).reg(), kind, p, kind == SOUND);
    }
}

unsafe fn add_emitters(w: *mut World, ps: P) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract.
    unsafe {
        for e in emitters(ps) {
            add_rend(w, e);
            (host.set_world)(u32::MAX - 1, e, (*w).pod.owner);
        }
    }
}

/// `mode` 0 creates by type name, 1 from an XML element.
///
/// # Safety
/// `w` live; strings valid; `elem` an XML element for mode 1.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_create_particle_system(
    w: *mut World,
    mode: i32,
    name: *const u8,
    name_len: usize,
    ty: *const u8,
    ty_len: usize,
    elem: P,
    size: *const Vector3f,
    remove_when_dead: bool,
) -> P {
    let Some(host) = h() else {
        return std::ptr::null_mut();
    };
    // SAFETY: caller contract.
    unsafe {
        let ps = (host.ps_create)(
            (*w).pod.resources,
            mode,
            name,
            name_len,
            ty,
            ty_len,
            elem,
            size,
        );
        if ps.is_null() {
            log(
                true,
                format!(
                    "Couldn't create particle system '{}' of type '{}'\n",
                    lossy(bytes(name, name_len)),
                    lossy(bytes(ty, ty_len))
                ),
            );
            return ps;
        }
        if mode == 0 {
            (host.ps_remove_when_dead)(ps, remove_when_dead);
        }
        add_emitters(w, ps);
        push(w, PARTICLE_SYSTEM, ps);
        ps
    }
}

/// # Safety
/// `w` live; `ps` a registered particle system or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_destroy_particle_system(w: *mut World, ps: P) {
    if ps.is_null() {
        return;
    }
    // SAFETY: caller contract.
    unsafe {
        for e in emitters(ps) {
            remove_rend(w, e);
        }
        reg::ohpl_h13_reg_destroy((*w).reg(), PARTICLE_SYSTEM, ps, false);
    }
}

/// # Safety
/// `w` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_destroy_all_particle_systems(w: *mut World) {
    // SAFETY: caller contract.
    unsafe {
        let mut it = Cur::new((*w).reg(), PARTICLE_SYSTEM);
        let f = (*(*w).reg()).foreign(PARTICLE_SYSTEM as usize);
        loop {
            let ps = it.get();
            if ps == 0 {
                break;
            }
            for e in emitters(ps as P) {
                remove_rend(w, e);
            }
            f.destroy(ps);
            it.advance();
        }
        reg::ohpl_h13_reg_clear((*w).reg(), PARTICLE_SYSTEM);
    }
}

/// # Safety
/// `w` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_create_sound(
    w: *mut World,
    name: *const u8,
    name_len: usize,
    data_name: *const u8,
    data_len: usize,
    remove_when_over: bool,
) -> P {
    let Some(host) = h() else {
        return std::ptr::null_mut();
    };
    // SAFETY: caller contract.
    unsafe {
        let data = (host.sound_data)((*w).pod.resources, data_name, data_len);
        if data.is_null() {
            log(
                true,
                format!(
                    "Cannot find sound entity '{}'\n",
                    lossy(bytes(data_name, data_len))
                ),
            );
            return data;
        }
        let mut a = args(bytes(name, name_len));
        a.a = data;
        a.flag = remove_when_over;
        a.id = (*w).pod.sound_id_count;
        (*w).pod.sound_id_count += 1;
        let p = make(w, SOUND, &a);
        push(w, SOUND, p);
        p
    }
}

/// # Safety
/// `w` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_destroy_all_sounds(w: *mut World) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract.
    unsafe {
        let pw = (*w).pod.physics_world;
        if !pw.is_null() {
            (host.phys_clear_sounds)(pw);
        }
        reg::ohpl_h13_reg_destroy_all((*w).reg(), SOUND);
    }
}

/// # Safety
/// `w` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_sound_exists(w: *mut World, p: P, creation_id: i32) -> bool {
    let Some(host) = h() else { return false };
    // SAFETY: caller contract.
    unsafe {
        reg::ohpl_h13_reg_index((*w).reg(), SOUND, p) >= 0
            && (host.sound_creation_id)(p) == creation_id
    }
}

/// # Safety
/// `w` live; `min`, `max` valid; `out` holds 4 entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_force_fields(
    w: *mut World,
    min: *const Vector3f,
    max: *const Vector3f,
    out: *mut P,
) -> i32 {
    let Some(host) = h() else { return 0 };
    let mut fields = Vec::new();
    let mut cands = Vec::new();
    // SAFETY: caller contract.
    unsafe {
        let mut it = Cur::new((*w).reg(), FORCE_FIELD);
        loop {
            let p = it.get();
            if p == 0 {
                break;
            }
            let mut f = FieldOut::default();
            (host.field_info)(p as P, &mut f);
            if f.usable {
                fields.push(FieldIn {
                    min: f.min,
                    max: f.max,
                    radius: f.radius,
                });
                cands.push(p);
            }
            it.advance();
        }
        let mut order = [0u32; 4];
        let n = logic::force_fields(&fields, *min, *max, &mut order);
        for (i, &o) in order.iter().enumerate().take(n.max(0) as usize) {
            if let Some(&c) = cands.get(o as usize) {
                *out.add(i) = c as P;
            }
        }
        n
    }
}

/// # Safety
/// `w` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_dir_light(w: *mut World) -> P {
    let Some(host) = h() else {
        return std::ptr::null_mut();
    };
    // SAFETY: caller contract.
    unsafe {
        if (*w).pod.dir_light.is_null() {
            (*w).pod.dir_light = (host.dirlight_new)((*w).pod.resources);
        }
        (*w).pod.dir_light
    }
}

/// # Safety
/// `w` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_set_terrain(w: *mut World, terrain: P) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract.
    unsafe {
        if !(*w).pod.terrain.is_null() {
            (host.terrain_delete)((*w).pod.terrain);
        }
        (*w).pod.terrain = terrain;
    }
}

/// # Safety
/// `w` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_set_physics_world(w: *mut World, pw: P, auto_delete: bool) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract.
    unsafe {
        (*w).pod.physics_world = pw;
        (*w).pod.auto_delete_physics = auto_delete;
        if !pw.is_null() {
            (host.phys_attach)(pw, (*w).pod.owner);
        }
    }
}

/// # Safety
/// `w` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_compile(w: *mut World, calc_physics_size: bool) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract.
    unsafe {
        for c in (*w).pod.container {
            if !c.is_null() {
                (host.cont_compile)(c);
            }
        }
        let pw = (*w).pod.physics_world;
        if !pw.is_null() && calc_physics_size {
            let mut b = [Vector3f::default(); 2];
            (host.cont_bounds)((*w).pod.container[STATIC], b.as_mut_ptr());
            let (min, max) = logic::physics_bounds(b[0], b[1]);
            (host.phys_set_size)(pw, [min, max].as_ptr());
        }
    }
}

/// # Safety
/// `w` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_set_skybox(w: *mut World, tex: P, auto_destroy: bool) {
    // SAFETY: caller contract.
    unsafe {
        let p = &raw mut (*w).pod;
        if !(*p).sky_tex.is_null() && (*p).auto_destroy_sky {
            ls_host::tex_destroy((*p).resources as usize, (*p).sky_tex as usize);
        }
        (*p).auto_destroy_sky = auto_destroy;
        (*p).sky_tex = tex;
        if !tex.is_null() {
            ls_host::tex_clamp(tex as usize, false);
        }
    }
}

unsafe fn update_sky_colors(w: *mut World) {
    // SAFETY: caller contract; the vertex buffer holds `n` colours of 4 floats.
    unsafe {
        let p = &raw const (*w).pod;
        let c = logic::skybox_color(
            (*p).sky_color,
            HPL3_SKYBOX.load(Ordering::Relaxed),
            (*p).sky_brightness,
        );
        let vb = Vb((*p).sky_vb);
        let n = ls_host::host()
            .map_or(0, |lh| (lh.vb_vertex_num)(vb.0))
            .max(0) as usize;
        for px in vb.floats(VB_COLOR0, n * 4).chunks_exact_mut(4) {
            px.copy_from_slice(&[c.r, c.g, c.b, c.a]);
        }
        vb.update(VB_FLAG_COLOR0);
    }
}

/// # Safety
/// `w` live; `c` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_set_skybox_color(w: *mut World, c: *const Color) {
    // SAFETY: caller contract.
    unsafe {
        if (*w).pod.sky_color == *c {
            return;
        }
        (*w).pod.sky_color = *c;
        update_sky_colors(w);
    }
}

/// # Safety
/// `w` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_set_skybox_brightness(w: *mut World, b: f32) {
    // SAFETY: caller contract.
    unsafe {
        if (*w).pod.sky_brightness == b {
            return;
        }
        (*w).pod.sky_brightness = b;
        update_sky_colors(w);
    }
}

/// # Safety
/// `w` live; strings valid; `vars` an instance-vars object or null; `mtx`, `scale` valid.
#[allow(clippy::too_many_arguments)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_create_entity(
    w: *mut World,
    name: *const u8,
    name_len: usize,
    mtx: *const Matrixf,
    file: *const u8,
    file_len: usize,
    id: i32,
    active: bool,
    scale: *const Vector3f,
    vars: P,
    skip_non_static: bool,
) -> P {
    let Some(host) = h() else {
        return std::ptr::null_mut();
    };
    // SAFETY: caller contract.
    unsafe {
        let res = (*w).pod.resources;
        let ef = (host.entfile_create)(res, file, file_len);
        if ef.is_null() {
            return ef;
        }
        (*w).ent_files.push(ef as usize);
        let (mut ty, mut ty_len, mut nm, mut nm_len, mut has) =
            (std::ptr::null(), 0, std::ptr::null(), 0, false);
        (host.entfile_info)(ef, &mut ty, &mut ty_len, &mut nm, &mut nm_len, &mut has);
        let file_name = lossy(bytes(file, file_len));
        let ent_type = if has {
            bytes(ty, ty_len).to_vec()
        } else {
            log(
                false,
                format!(
                    "Can not find a UserDefinedVariables element in '{file_name}'. Using default entity type\n"
                ),
            );
            Vec::new()
        };
        let loader = (host.loader_get)(res, ent_type.as_ptr(), ent_type.len());
        if loader.is_null() {
            log(
                true,
                format!(
                    "Couldn't find loader for type '{}' in file '{}'\n",
                    lossy(&ent_type),
                    lossy(bytes(nm, nm_len))
                ),
            );
            return std::ptr::null_mut();
        }
        if skip_non_static && !(host.loader_static)(loader) {
            return std::ptr::null_mut();
        }
        let la = LoadArgs {
            name,
            name_len,
            id,
            active,
            mtx,
            scale,
            vars,
        };
        let ent = (host.loader_load)(loader, ef, (*w).pod.owner, &la);
        if !ent.is_null() {
            (host.entity_source)(ent, ef);
        }
        ent
    }
}

unsafe fn update_particles(w: *mut World, dt: f32) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract; the registry is not borrowed across the callbacks.
    unsafe {
        let mut it = Cur::new((*w).reg(), PARTICLE_SYSTEM);
        let f = (*(*w).reg()).foreign(PARTICLE_SYSTEM as usize);
        loop {
            let ps = it.get();
            if ps == 0 {
                break;
            }
            if (host.update)(PARTICLE_SYSTEM, ps as P, dt) != 0 {
                it.erase();
                for e in emitters(ps as P) {
                    remove_rend(w, e);
                }
                f.destroy(ps);
            } else {
                it.advance();
            }
        }
    }
}

/// # Safety
/// `w` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_update(w: *mut World, dt: f32) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract; entity callbacks may re-enter the world.
    unsafe {
        let r = (*w).reg();
        let pw = (*w).pod.physics_world;
        if !pw.is_null() {
            (host.phys_update)(pw, dt);
        }
        let mut it = Cur::new(r, DYN_MESH);
        while it.get() != 0 {
            (host.update)(DYN_MESH, it.get() as P, dt);
            it.advance();
        }
        update_particles(w, dt);
        let mut it = Cur::new(r, LENS_FLARE);
        while it.get() != 0 {
            (host.update)(LENS_FLARE, it.get() as P, dt);
            it.advance();
        }
        let mut it = Cur::new(r, FORCE_FIELD);
        let f = (*r).foreign(FORCE_FIELD as usize);
        while it.get() != 0 {
            let p = it.get();
            if (host.update)(FORCE_FIELD, p as P, dt) != 0 {
                f.destroy(p);
                it.erase();
            } else {
                it.advance();
            }
        }
        let mut it = Cur::new(r, LIGHT);
        while it.get() != 0 {
            (host.update)(LIGHT, it.get() as P, dt);
            it.advance();
        }
        let mut it = Cur::new(r, SOUND);
        let f = (*r).foreign(SOUND as usize);
        while it.get() != 0 {
            let p = it.get();
            if (host.update)(SOUND, p as P, dt) != 0 {
                it.erase();
                f.destroy(p);
            } else {
                it.advance();
            }
        }
        let mut it = Cur::new(r, ENV_PARTICLES);
        while it.get() != 0 {
            (host.update)(ENV_PARTICLES, it.get() as P, dt);
            it.advance();
        }
    }
}

/// # Safety
/// `w` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_pre_update(w: *mut World, total: f32, dt: f32) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract.
    unsafe {
        (host.sound_silent)((*w).pod.sound, true);
        for _ in 0..logic::pre_update_steps(total, dt) {
            let pw = (*w).pod.physics_world;
            if !pw.is_null() {
                (host.phys_update)(pw, dt);
            }
            update_particles(w, dt);
        }
        (host.sound_silent)((*w).pod.sound, false);
    }
}

fn hex(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn mtx_hex(m: &[f32; 16]) -> String {
    m.iter().map(|v| format!(" {}", hex(*v))).collect()
}

/// # Safety
/// `w` live; `path`, `map` NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_dump(
    w: *mut World,
    path: *const std::ffi::c_char,
    map: *const std::ffi::c_char,
) {
    let Some(host) = h() else { return };
    // SAFETY: caller contract.
    let (path, map) = unsafe {
        (
            std::ffi::CStr::from_ptr(path)
                .to_string_lossy()
                .into_owned(),
            std::ffi::CStr::from_ptr(map).to_string_lossy().into_owned(),
        )
    };
    let Ok(mut f) = std::fs::OpenOptions::new()
        .append(true)
        .create(true)
        .open(path)
    else {
        return;
    };
    let mut out = String::new();
    // SAFETY: caller contract.
    unsafe {
        let r = (*w).reg();
        let pod = &raw const (*w).pod;
        out += &format!("map {map}\nworld {}\n", lossy(&(*w).name));
        out += &format!(
            "fog {} {} {}\nsky {}\n",
            (*pod).fog_active as i32,
            (*pod).fog_culling as i32,
            hex((*pod).fog_start),
            (*pod).sky_active as i32
        );
        let lists = [
            ("light", LIGHT),
            ("meshdyn", DYN_MESH),
            ("meshstatic", STATIC_MESH),
            ("billboard", BILLBOARD),
            ("lensflare", LENS_FLARE),
            ("beam", BEAM),
            ("ps", PARTICLE_SYSTEM),
            ("guiset", GUI_SET),
            ("rope", ROPE),
            ("sound", SOUND),
            ("fogarea", FOG_AREA),
            ("forcefield", FORCE_FIELD),
            ("dummy", DUMMY),
        ];
        for (label, kind) in lists {
            out += &format!("list {label} {}\n", reg::ohpl_h13_reg_len(r, kind));
            let mut it = Cur::new(r, kind);
            let mut idx = 0;
            while it.get() != 0 {
                let mut d = DumpItem::default();
                (host.dump_item)(kind, it.get() as P, &mut d);
                out += &format!(
                    "{label} {idx} {} {}{}\n",
                    lossy(bytes(d.name, d.name_len)),
                    d.uid,
                    mtx_hex(&d.mtx)
                );
                idx += 1;
                it.advance();
            }
        }
        out += &format!("list startpos {}\n", reg::ohpl_h13_reg_len(r, START_POS));
        let mut it = Cur::new(r, START_POS);
        while it.get() != 0 {
            let mut d = DumpItem::default();
            (host.dump_item)(START_POS, it.get() as P, &mut d);
            out += &format!(
                "startpos {} {}{}\n",
                lossy(bytes(d.name, d.name_len)),
                d.crouch as i32,
                mtx_hex(&d.mtx)
            );
            it.advance();
        }
        let areas = (*r).areas();
        out += &format!("list area {}\n", areas.len());
        for (key, p) in areas {
            let mut d = DumpItem::default();
            (host.dump_item)(u32::MAX, p as P, &mut d);
            out += &format!(
                "area {} {}{} {} {} {}\n",
                lossy(&key),
                lossy(bytes(d.kind_name, d.kind_len)),
                mtx_hex(&d.mtx),
                hex(d.size[0]),
                hex(d.size[1]),
                hex(d.size[2])
            );
        }
        out += &format!(
            "list ainode {}\nlist astar {}\nlist envparticles {}\n",
            reg::ohpl_h13_reg_len(r, AI_CONTAINER),
            reg::ohpl_h13_reg_len(r, ASTAR),
            reg::ohpl_h13_reg_len(r, ENV_PARTICLES)
        );
    }
    let _ = f.write_all(out.as_bytes());
}
