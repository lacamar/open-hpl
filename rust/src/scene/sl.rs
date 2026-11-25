//! Rust-owned state of `cFogArea`, `cBillboard`, `cBeam`/`cBeamEnd`, `iLight` and its subclasses.
//!
//! Each object is a box-pinned `repr(C)` pod whose address the C++ shell holds (`RustSL.h` mirrors
//! the layouts; sizes are asserted on both sides). Non-pod members live behind the pod in the same
//! allocation. Engine side effects (vertex buffers, textures, world) stay in the shells.

use super::beam;
use super::billboard::{self, ModelIn};
use super::h11::{self, ShadowCache, SpotCtl};
use super::h12b_ent;
use super::lensflare::{self, VisState};
use super::light::{self, FlickerCfg, LightAnim, LightLive};
use super::rope::RopeParams;
use crate::ffi::slice_mut;
use crate::types::{Color, Matrixf, Vector2f, Vector2l, Vector3f};
use std::collections::BTreeMap;
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(
        size_of::<FogPod>() == 148 && size_of::<BbPod>() == 240 && size_of::<BeamEndPod>() == 24
    );
    assert!(size_of::<BeamPod>() == 168 && size_of::<LightPod>() == 360);
    assert!(size_of::<SpotPod>() == 608 && size_of::<BoxPod>() == 664);
    assert!(size_of::<FlarePod>() == 608 && size_of::<FlareTypePod>() == 80);
    assert!(size_of::<LightAnim>() == 144 && offset_of!(LightAnim, flickering) == 140);
    assert!(size_of::<LightLive>() == 24 && size_of::<FlickerCfg>() == 56);
    assert!(size_of::<lensflare::VisState>() == 16 && size_of::<RopeParams>() == 28);
    assert!(
        size_of::<lensflare::FlareIn>() == 152 && offset_of!(lensflare::FlareIn, brightness) == 144
    );
    assert!(offset_of!(lensflare::FlareIn, color) == 4 && size_of::<lensflare::EntIn>() == 24);
    assert!(size_of::<lensflare::ViewIn>() == 144);
};

static HPL3_VISIBILITY: AtomicBool = AtomicBool::new(false);
static SPOT_SHADOW_NEAR: AtomicU32 = AtomicU32::new(0);

fn boxed<T>(v: T) -> *mut T {
    Box::into_raw(Box::new(v))
}

/// # Safety
/// `p` from the matching `*_new`, not used afterwards; null is ignored.
unsafe fn unbox<T>(p: *mut T) {
    if !p.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(p) });
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FogPod {
    pub color: Color,
    pub brightness: f32,
    pub size: Vector3f,
    pub start: f32,
    pub end: f32,
    pub falloff_exp: f32,
    pub backside_outside: bool,
    pub backside_inside: bool,
    pub underwater: bool,
    pub skybox: bool,
    pub noise_strength: f32,
    pub noise_size: f32,
    pub noise_turbulence: Vector3f,
    pub lighten: bool,
    pub height_based: bool,
    pub exponential: bool,
    pub density: f32,
    pub height_density: f32,
    pub height_horizon: f32,
    pub model: Matrixf,
}

impl FogPod {
    fn new() -> Self {
        Self {
            color: Color {
                r: 1.0,
                g: 1.0,
                b: 1.0,
                a: 1.0,
            },
            brightness: 1.0,
            size: Vector3f {
                x: 1.0,
                y: 1.0,
                z: 1.0,
            },
            start: 0.0,
            end: 10.0,
            falloff_exp: 1.0,
            backside_outside: true,
            backside_inside: true,
            noise_size: 1.0,
            density: 0.25,
            height_density: 0.25,
            height_horizon: 1.0,
            ..Self::default()
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_sl_fog_new() -> *mut FogPod {
    boxed(FogPod::new())
}

/// # Safety
/// See `unbox`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_fog_free(p: *mut FogPod) {
    // SAFETY: caller contract.
    unsafe { unbox(p) }
}

/// Stores the size and writes the bounding-volume size.
///
/// # Safety
/// `p`, `size`, `bv` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_fog_set_size(
    p: *mut FogPod,
    size: *const Vector3f,
    bv: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        (*p).size = *size;
        *bv = h11::fog_bounds_size(*size);
    }
}

/// # Safety
/// `p`, `world` valid; the result lives as long as `p`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_fog_model(
    p: *mut FogPod,
    world: *const Matrixf,
) -> *const Matrixf {
    // SAFETY: caller contract.
    unsafe {
        (*p).model = super::node::scaled_world(&*world, (*p).size);
        &(*p).model
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct BbPod {
    pub material: usize,
    pub vtx: usize,
    pub halo_bv: usize,
    pub mat_mgr: usize,
    pub halo_occlusion: Matrixf,
    pub temp: Matrixf,
    pub kind: i32,
    pub size: Vector2f,
    pub axis: Vector3f,
    pub last_render_count: i32,
    pub halo_source_size: Vector3f,
    pub halo_bv_matrix_count: i32,
    pub forward_offset: f32,
    pub color: Color,
    pub halo_alpha: f32,
    pub brightness: f32,
    pub is_halo: bool,
    pub halo_size_updated: bool,
}

pub(crate) unsafe fn ohpl_sl_bb_new(kind: i32, size: *const Vector2f) -> *mut BbPod {
    boxed(BbPod {
        kind,
        size: unsafe { *size },
        axis: Vector3f {
            x: 0.0,
            y: 1.0,
            z: 0.0,
        },
        color: Color {
            r: 1.0,
            g: 1.0,
            b: 1.0,
            a: 1.0,
        },
        brightness: 1.0,
        halo_alpha: 1.0,
        last_render_count: -1,
        halo_source_size: Vector3f {
            x: 1.0,
            y: 1.0,
            z: 1.0,
        },
        halo_bv_matrix_count: -1,
        halo_size_updated: true,
        ..BbPod::default()
    })
}

pub(crate) unsafe fn ohpl_sl_bb_free(p: *mut BbPod) {
    unsafe { unbox(p) }
}

pub(crate) unsafe fn ohpl_sl_bb_set_color(p: *mut BbPod, c: *const Color) -> bool {
    unsafe {
        if (*p).color == *c {
            return false;
        }
        (*p).color = *c;
    }
    true
}

pub(crate) unsafe fn ohpl_sl_bb_set_halo_alpha(p: *mut BbPod, a: f32) -> bool {
    unsafe {
        if (*p).halo_alpha == a {
            return false;
        }
        (*p).halo_alpha = a;
    }
    true
}

pub(crate) unsafe fn ohpl_sl_bb_set_brightness(p: *mut BbPod, b: f32) -> bool {
    unsafe {
        if (*p).brightness == b {
            return false;
        }
        (*p).brightness = b;
    }
    true
}

pub(crate) unsafe fn ohpl_sl_bb_set_axis(
    p: *mut BbPod,
    axis: *const Vector3f,
    out: *mut f32,
) -> bool {
    unsafe {
        (*p).axis = *axis;
        let b = &*p;
        h12b_ent::bb_axis_size(b.kind == 1, b.axis, b.size.x, b.size.y).is_some_and(|v| {
            *out = v;
            true
        })
    }
}

pub(crate) unsafe fn ohpl_sl_bb_halo_gates(p: *mut BbPod, transform_count: i32) -> u32 {
    unsafe {
        let b = &mut *p;
        h12b_ent::bb_halo_gates(
            &mut b.halo_size_updated,
            &mut b.halo_bv_matrix_count,
            transform_count,
        )
    }
}

pub(crate) unsafe fn ohpl_sl_bb_model(
    p: *mut BbPod,
    world: *const Matrixf,
    cam_origin: *const Vector3f,
    view_up: *const Vector3f,
    warn: *mut bool,
) -> *const Matrixf {
    unsafe {
        let b = &mut *p;
        let (m, w) = billboard::model_matrix(&ModelIn {
            kind: b.kind,
            forward_offset: b.forward_offset,
            world: *world,
            cam_origin: *cam_origin,
            view_up: *view_up,
            axis: b.axis,
        });
        b.temp = m;
        *warn = w;
        &b.temp
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct BeamEndPod {
    pub color: Color,
    pub beam: usize,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_sl_beam_end_new(beam: usize) -> *mut BeamEndPod {
    boxed(BeamEndPod {
        color: Color {
            r: 1.0,
            g: 1.0,
            b: 1.0,
            a: 1.0,
        },
        beam,
    })
}

/// # Safety
/// See `unbox`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_beam_end_free(p: *mut BeamEndPod) {
    // SAFETY: caller contract.
    unsafe { unbox(p) }
}

pub(crate) unsafe fn ohpl_sl_beam_end_set_color(p: *mut BeamEndPod, c: *const Color) -> bool {
    unsafe {
        if (*p).color == *c {
            return false;
        }
        (*p).color = *c;
    }
    true
}

#[repr(C)]
pub struct BeamPod {
    pub material: usize,
    pub vtx: usize,
    pub end: usize,
    pub mat_mgr: usize,
    pub file_searcher: usize,
    pub start_transform_count: i32,
    pub end_transform_count: i32,
    pub temp: Matrixf,
    pub last_render_count: i32,
    pub size: Vector2f,
    pub axis: Vector3f,
    pub mid: Vector3f,
    pub tile_height: bool,
    pub multiply_alpha: bool,
    pub color: Color,
}

/// Pod first so the C++ pointer is the allocation address.
#[repr(C)]
pub struct BeamObj {
    pub pod: BeamPod,
    pub(super) file: Vec<u8>,
}

pub(crate) fn ohpl_sl_beam_new() -> *mut BeamPod {
    boxed(BeamObj {
        pod: BeamPod {
            material: 0,
            vtx: 0,
            end: 0,
            mat_mgr: 0,
            file_searcher: 0,
            start_transform_count: -1,
            end_transform_count: -1,
            temp: Matrixf::default(),
            last_render_count: -1,
            size: Vector2f { x: 1.0, y: 1.0 },
            axis: Vector3f::default(),
            mid: Vector3f::default(),
            tile_height: true,
            multiply_alpha: false,
            color: Color {
                r: 1.0,
                g: 1.0,
                b: 1.0,
                a: 1.0,
            },
        },
        file: Vec::new(),
    })
    .cast()
}

pub(crate) unsafe fn ohpl_sl_beam_set_file(p: *mut BeamPod, s: *const u8, n: usize) {
    unsafe { (*p.cast::<BeamObj>()).file = std::slice::from_raw_parts(s, n).to_vec() }
}

/// # Safety
/// `p` from `ohpl_sl_beam_new`; the bytes live until the next `set_file`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_beam_file(p: *const BeamPod, n: *mut usize) -> *const u8 {
    // SAFETY: caller contract.
    unsafe {
        let f = &(*p.cast::<BeamObj>()).file;
        *n = f.len();
        f.as_ptr()
    }
}

pub(crate) unsafe fn ohpl_sl_beam_set_color(
    p: *mut BeamPod,
    c: *const Color,
    colors: *mut f32,
) -> bool {
    unsafe {
        if (*p).color == *c {
            return false;
        }
        (*p).color = *c;
        beam::set_edge_colors(
            (*p).color,
            (*p).multiply_alpha,
            std::slice::from_raw_parts_mut(colors, 8),
        );
    }
    true
}

pub(crate) unsafe fn ohpl_sl_beam_set_end_color(
    p: *mut BeamPod,
    e: *mut BeamEndPod,
    c: *const Color,
    colors: *mut f32,
) -> bool {
    unsafe {
        if !ohpl_sl_beam_end_set_color(e, c) {
            return false;
        }
        beam::set_edge_colors(
            (*e).color,
            (*p).multiply_alpha,
            std::slice::from_raw_parts_mut(colors.add(8), 8),
        );
    }
    true
}

pub(crate) unsafe fn ohpl_sl_beam_bounds(
    p: *const BeamPod,
    start: *const Vector3f,
    end: *const Vector3f,
    min: *mut Vector3f,
    max: *mut Vector3f,
) {
    unsafe {
        let (lo, hi) = beam::bounds(*start, *end, (*p).size.x);
        *min = lo;
        *max = hi;
    }
}

pub(crate) unsafe fn ohpl_sl_beam_geometry(
    p: *mut BeamPod,
    transform_count: i32,
    start: *const Vector3f,
    end: *const Vector3f,
    pos: *mut f32,
    tex: *mut f32,
) -> bool {
    unsafe {
        let b = &mut *p;
        if !h12b_ent::beam_needs_geometry(
            b.start_transform_count,
            b.end_transform_count,
            transform_count,
        ) {
            return false;
        }
        let g = beam::geometry(*start, *end, [b.size.x, b.size.y], b.tile_height);
        for (dst, src) in slice_mut(pos, 16)
            .as_chunks_mut::<4>()
            .0
            .iter_mut()
            .zip(g.pos)
        {
            for (d, s) in dst.iter_mut().zip(src) {
                *d = s;
            }
        }
        for (dst, src) in slice_mut(tex, 12)
            .as_chunks_mut::<3>()
            .0
            .iter_mut()
            .zip(g.uv)
        {
            for (d, s) in dst.iter_mut().zip(src) {
                *d = s;
            }
        }
        b.axis = g.axis;
        b.mid = g.mid;
    }
    true
}

pub(crate) unsafe fn ohpl_sl_beam_model(
    p: *mut BeamPod,
    world: *const Matrixf,
    world_pos: *const Vector3f,
    camera: *const Vector3f,
) -> *const Matrixf {
    unsafe {
        let b = &mut *p;
        b.temp = beam::model_matrix(&*world, *world_pos, *camera, b.axis, b.mid);
        &b.temp
    }
}

#[repr(C)]
pub struct LightPod {
    pub world: usize,
    pub res: usize,
    pub ctx: usize,
    pub ent: usize,
    pub bv: usize,
    pub rnd: usize,
    pub falloff_map: usize,
    pub gobo: usize,
    pub tracker: usize,
    pub diffuse: Color,
    pub default_diffuse: Color,
    pub specular: Color,
    pub anim: LightAnim,
    pub mask_center: Vector3f,
    pub mask_size: Vector3f,
    pub light_type: i32,
    pub shadow_res: i32,
    pub shadow_blur: f32,
    pub source_radius: f32,
    pub radius: f32,
    pub falloff_pow: f32,
    pub spot_falloff_pow: f32,
    pub translucency: f32,
    pub brightness: f32,
    pub default_brightness: f32,
    pub mask_id: u32,
    pub shadow_casters_affected: u32,
    pub shadow_bias_mul: f32,
    pub shadow_distance: f32,
    pub shadow_slope_bias_mul: f32,
    pub occlusion_cull: bool,
    pub gobo_specular: bool,
    pub cast_diffuse: bool,
    pub cast_specular: bool,
    pub mask_box: bool,
    pub cast_shadows: bool,
}

pub(super) struct LightRest {
    pub(super) billboards: Vec<(usize, Color)>,
    pub(super) particle_systems: Vec<usize>,
    shadow_cache: Option<Box<ShadowCache>>,
    pub(super) strings: [Vec<u8>; 4],
}

#[repr(C)]
pub struct LightObj {
    pub pod: LightPod,
    rest: LightRest,
}

#[repr(C)]
pub struct SpotPod {
    pub light: LightPod,
    pub mtx_projection: Matrixf,
    pub mtx_view_proj: Matrixf,
    pub mtx_view: Matrixf,
    pub frustum: usize,
    pub spot_falloff_map: usize,
    pub fov: f32,
    pub aspect: f32,
    pub near_clip: f32,
    pub tan_half_fov: f32,
    pub cos_half_fov: f32,
    pub fov_updated: bool,
    pub ctl: SpotCtl,
}

#[repr(C)]
pub struct BoxPod {
    pub light: LightPod,
    pub size: Vector3f,
    pub blend: i32,
    pub prio: i32,
    pub ambient_sky: Color,
    pub ambient_ground: Color,
    pub weight: f32,
    pub bevel: f32,
    pub falloff_pow: f32,
    pub use_sh: bool,
    pub probe_offset: Vector3f,
    pub bands: [Vector3f; 9],
    pub fade_from: [Vector3f; 9],
    pub fade_t: f32,
    pub fade_speed: f32,
}

struct BoxRest {
    sets: BTreeMap<Vec<u8>, Vec<Vector3f>>,
    fade_target: Option<Vec<u8>>,
}

/// Subclass pods start with the `LightPod`, so every light pointer is the allocation address.
#[repr(C)]
struct SpotObj {
    pod: SpotPod,
    rest: LightRest,
}

#[repr(C)]
struct BoxObj {
    pod: BoxPod,
    rest: LightRest,
    box_rest: BoxRest,
}

pub(super) const KIND_POINT: i32 = 0;
pub(super) const KIND_SPOT: i32 = 1;
pub(super) const KIND_BOX: i32 = 2;

fn black() -> Color {
    Color {
        r: 0.0,
        g: 0.0,
        b: 0.0,
        a: 1.0,
    }
}

fn white() -> Color {
    Color {
        r: 1.0,
        g: 1.0,
        b: 1.0,
        a: 1.0,
    }
}

impl LightPod {
    fn new() -> Self {
        Self {
            world: 0,
            res: 0,
            ctx: 0,
            ent: 0,
            bv: 0,
            rnd: 0,
            falloff_map: 0,
            gobo: 0,
            tracker: 0,
            diffuse: black(),
            default_diffuse: black(),
            specular: black(),
            anim: LightAnim::default(),
            mask_center: Vector3f::default(),
            mask_size: Vector3f::default(),
            light_type: KIND_POINT,
            shadow_res: 2,
            shadow_blur: 6.0,
            source_radius: 0.0,
            radius: 0.0,
            falloff_pow: 1.0,
            spot_falloff_pow: 1.0,
            translucency: 1.0,
            brightness: 1.0,
            default_brightness: 1.0,
            mask_id: 0xFFFF_FFFF,
            shadow_casters_affected: 3,
            shadow_bias_mul: 1.0,
            shadow_distance: -1.0,
            shadow_slope_bias_mul: 1.0,
            occlusion_cull: false,
            gobo_specular: false,
            cast_diffuse: true,
            cast_specular: true,
            mask_box: false,
            cast_shadows: false,
        }
    }

    pub(super) fn live(&self) -> LightLive {
        LightLive {
            diffuse: self.diffuse,
            radius: self.radius,
            brightness: self.brightness,
        }
    }

    pub(super) fn is_lit(&self) -> bool {
        let c = self.diffuse;
        h11::light_is_lit(
            c.r,
            c.g,
            c.b,
            c.a,
            self.brightness,
            HPL3_VISIBILITY.load(Ordering::Relaxed),
        )
    }
}

fn new_rest() -> LightRest {
    LightRest {
        billboards: Vec::new(),
        particle_systems: Vec::new(),
        shadow_cache: None,
        strings: Default::default(),
    }
}

pub(crate) fn ohpl_sl_light_new(kind: i32) -> *mut LightPod {
    let mut light = LightPod::new();
    light.light_type = kind;
    match kind {
        KIND_SPOT => {
            light.radius = 100.0;
            let (t, c) = light::spot_half_fov(60.0_f32.to_radians());
            let id = Matrixf {
                m: [
                    [1.0, 0.0, 0.0, 0.0],
                    [0.0, 1.0, 0.0, 0.0],
                    [0.0, 0.0, 1.0, 0.0],
                    [0.0, 0.0, 0.0, 1.0],
                ],
            };
            boxed(SpotObj {
                pod: SpotPod {
                    light,
                    mtx_projection: id,
                    mtx_view_proj: id,
                    mtx_view: id,
                    frustum: 0,
                    spot_falloff_map: 0,
                    fov: 60.0_f32.to_radians(),
                    aspect: 1.0,
                    near_clip: 0.1,
                    tan_half_fov: t,
                    cos_half_fov: c,
                    fov_updated: true,
                    ctl: SpotCtl::new(),
                },
                rest: new_rest(),
            })
            .cast()
        }
        KIND_BOX => boxed(BoxObj {
            pod: BoxPod {
                light,
                size: Vector3f {
                    x: 1.0,
                    y: 1.0,
                    z: 1.0,
                },
                blend: 0,
                prio: 0,
                ambient_sky: white(),
                ambient_ground: white(),
                weight: 1.0,
                bevel: 0.0,
                falloff_pow: 0.0,
                use_sh: false,
                probe_offset: Vector3f::default(),
                bands: [Vector3f::default(); 9],
                fade_from: [Vector3f::default(); 9],
                fade_t: 1.0,
                fade_speed: 0.0,
            },
            rest: new_rest(),
            box_rest: BoxRest {
                sets: BTreeMap::new(),
                fade_target: None,
            },
        })
        .cast(),
        _ => boxed(LightObj {
            pod: light,
            rest: new_rest(),
        })
        .cast(),
    }
}

pub(crate) unsafe fn ohpl_sl_light_free(p: *mut LightPod) {
    if p.is_null() {
        return;
    }
    unsafe {
        match (*p).light_type {
            KIND_SPOT => unbox(p.cast::<SpotObj>()),
            KIND_BOX => unbox(p.cast::<BoxObj>()),
            _ => unbox(p.cast::<LightObj>()),
        }
    }
}

/// # Safety
/// `p` from `ohpl_sl_light_new`.
pub(super) unsafe fn rest<'a>(p: *mut LightPod) -> &'a mut LightRest {
    // SAFETY: caller contract; `rest` follows the pod at the same offset class per kind.
    unsafe {
        match (*p).light_type {
            KIND_SPOT => &mut (*p.cast::<SpotObj>()).rest,
            KIND_BOX => &mut (*p.cast::<BoxObj>()).rest,
            _ => &mut (*p.cast::<LightObj>()).rest,
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_sl_set_hpl3_visibility(on: bool) {
    HPL3_VISIBILITY.store(on, Ordering::Relaxed);
}

pub(crate) fn ohpl_sl_hpl3_visibility() -> bool {
    HPL3_VISIBILITY.load(Ordering::Relaxed)
}

pub(crate) fn ohpl_sl_spot_shadow_near() -> f32 {
    f32::from_bits(SPOT_SHADOW_NEAR.load(Ordering::Relaxed))
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_light_is_lit(p: *const LightPod) -> bool {
    // SAFETY: caller contract.
    unsafe { (*p).is_lit() }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_light_is_visible(p: *const LightPod, visible: bool) -> bool {
    // SAFETY: caller contract.
    unsafe { h11::light_is_visible((*p).is_lit(), (*p).radius, visible) }
}

pub(crate) unsafe fn ohpl_sl_light_set_diffuse(p: *mut LightPod, c: *const Color) -> bool {
    unsafe {
        let was = (*p).is_lit();
        (*p).diffuse = *c;
        (*p).is_lit() != was
    }
}

pub(crate) unsafe fn ohpl_sl_light_set_brightness(p: *mut LightPod, b: f32) -> bool {
    unsafe {
        let was = (*p).is_lit();
        (*p).brightness = b;
        (*p).is_lit() != was
    }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_light_fade_brightness_to(p: *mut LightPod, dest: f32, time: f32) {
    // SAFETY: caller contract.
    unsafe {
        let live = (*p).live();
        light::fade_brightness_to(&mut (*p).anim, &live, dest, time);
    }
}

/// # Safety
/// `p`, `c` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_light_fade_to(
    p: *mut LightPod,
    c: *const Color,
    radius: f32,
    time: f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let live = (*p).live();
        light::fade_to(&mut (*p).anim, &live, *c, radius, time);
    }
}

pub(crate) unsafe fn ohpl_sl_light_set_flicker(
    p: *mut LightPod,
    cfg: *const FlickerCfg,
    strings: *const [*const u8; 4],
    lens: *const [usize; 4],
) {
    unsafe {
        let r = rest(p);
        for (dst, (ptr, len)) in r
            .strings
            .iter_mut()
            .zip((*strings).iter().zip((*lens).iter()))
        {
            *dst = std::slice::from_raw_parts(*ptr, *len).to_vec();
        }
        let live = (*p).live();
        light::set_flicker(&mut (*p).anim, &live, &*cfg);
    }
}

/// `which`: 0 on sound, 1 on PS, 2 off sound, 3 off PS.
///
/// # Safety
/// `p` valid; the bytes live until the next `set_flicker`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_light_string(
    p: *mut LightPod,
    which: i32,
    n: *mut usize,
) -> *const u8 {
    // SAFETY: caller contract.
    unsafe {
        let s: &[u8] = rest(p)
            .strings
            .get(usize::try_from(which).unwrap_or(0))
            .map_or(&[], Vec::as_slice);
        *n = s.len();
        s.as_ptr()
    }
}

pub(crate) unsafe fn ohpl_sl_light_attach_billboard(
    p: *mut LightPod,
    bb: usize,
    base: *const Color,
) {
    unsafe { rest(p).billboards.push((bb, *base)) }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_light_remove_billboard(p: *mut LightPod, bb: usize) {
    // SAFETY: caller contract.
    unsafe {
        let v = &mut rest(p).billboards;
        if let Some(i) = v.iter().position(|c| c.0 == bb) {
            v.remove(i);
        }
    }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_light_has_billboard(p: *mut LightPod, bb: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { rest(p).billboards.iter().any(|c| c.0 == bb) }
}

pub(crate) unsafe fn ohpl_sl_light_billboard_count(p: *mut LightPod) -> usize {
    unsafe { rest(p).billboards.len() }
}

pub(crate) unsafe fn ohpl_sl_light_billboard_at(
    p: *mut LightPod,
    i: usize,
    base: *mut Color,
) -> usize {
    unsafe {
        let c = rest(p)
            .billboards
            .get(i)
            .copied()
            .unwrap_or((0, Color::default()));
        *base = c.1;
        c.0
    }
}

pub(crate) unsafe fn ohpl_sl_light_attach_ps(p: *mut LightPod, ps: usize) {
    unsafe { rest(p).particle_systems.push(ps) }
}

pub(crate) unsafe fn ohpl_sl_light_remove_ps(p: *mut LightPod, ps: usize) -> bool {
    unsafe {
        let v = &mut rest(p).particle_systems;
        v.iter()
            .position(|c| *c == ps)
            .map(|i| v.remove(i))
            .is_some()
    }
}

pub(crate) unsafe fn ohpl_sl_light_ps_count(p: *mut LightPod) -> usize {
    unsafe { rest(p).particle_systems.len() }
}

pub(crate) unsafe fn ohpl_sl_light_ps_at(p: *mut LightPod, i: usize) -> usize {
    unsafe { rest(p).particle_systems.get(i).copied().unwrap_or(0) }
}

pub(crate) unsafe fn ohpl_sl_light_cache_add(p: *mut LightPod, object: usize, count: i32) {
    unsafe {
        rest(p)
            .shadow_cache
            .get_or_insert_with(Box::default)
            .add(object, count)
    }
}

pub(crate) unsafe fn ohpl_sl_light_cache_valid(
    p: *mut LightPod,
    object: usize,
    count: i32,
) -> bool {
    unsafe {
        rest(p)
            .shadow_cache
            .as_ref()
            .is_some_and(|c| c.is_valid(object, count))
    }
}

pub(crate) unsafe fn ohpl_sl_light_cache_unchanged(
    p: *mut LightPod,
    casters: *const h11::Caster,
    n: i32,
) -> bool {
    unsafe {
        let list = std::slice::from_raw_parts(casters, usize::try_from(n).unwrap_or(0));
        rest(p)
            .shadow_cache
            .get_or_insert_with(Box::default)
            .unchanged(list)
    }
}

pub(crate) unsafe fn ohpl_sl_light_cache_set(
    p: *mut LightPod,
    casters: *const h11::Caster,
    n: i32,
) {
    unsafe {
        let list = std::slice::from_raw_parts(casters, usize::try_from(n).unwrap_or(0));
        rest(p)
            .shadow_cache
            .get_or_insert_with(Box::default)
            .set_from(list);
    }
}

pub(crate) unsafe fn ohpl_sl_light_cache_clear(p: *mut LightPod) {
    unsafe {
        if let Some(c) = rest(p).shadow_cache.as_mut() {
            c.clear();
        }
    }
}

pub(crate) unsafe fn ohpl_sl_box_add_set(
    p: *mut BoxPod,
    name: *const u8,
    name_len: usize,
    bands: *const Vector3f,
    n: usize,
) {
    unsafe {
        let o = &mut *p.cast::<BoxObj>();
        let first = o.box_rest.sets.is_empty();
        let key = std::slice::from_raw_parts(name, name_len).to_ascii_lowercase();
        let mut v = std::slice::from_raw_parts(bands, n).to_vec();
        v.resize(9, Vector3f::default());
        if first {
            for (d, s) in o.pod.bands.iter_mut().zip(&v) {
                *d = *s;
            }
        }
        o.box_rest.sets.insert(key, v);
    }
}

pub(crate) unsafe fn ohpl_sl_box_fade_set(
    p: *mut BoxPod,
    name: *const u8,
    name_len: usize,
    time: f32,
) -> bool {
    unsafe {
        let o = &mut *p.cast::<BoxObj>();
        let key = std::slice::from_raw_parts(name, name_len).to_ascii_lowercase();
        if !o.box_rest.sets.contains_key(&key) {
            return false;
        }
        o.box_rest.fade_target = Some(key);
        o.pod.fade_from = o.pod.bands;
        o.pod.fade_t = 0.0;
        o.pod.fade_speed = light::box_fade_speed(time);
    }
    true
}

pub(crate) unsafe fn ohpl_sl_box_fade_step(p: *mut BoxPod, dt: f32) {
    unsafe {
        let o = &mut *p.cast::<BoxObj>();
        let Some(target) = o
            .box_rest
            .fade_target
            .as_ref()
            .and_then(|k| o.box_rest.sets.get(k))
        else {
            return;
        };
        let Ok(target) = <&[Vector3f; 9]>::try_from(target.as_slice()) else {
            return;
        };
        let (from, speed) = (o.pod.fade_from, o.pod.fade_speed);
        if light::box_fade_step(
            &mut o.pod.bands,
            &from,
            target,
            &mut o.pod.fade_t,
            speed,
            dt,
        ) {
            o.box_rest.fade_target = None;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn light_lifecycle() {
        for kind in [KIND_POINT, KIND_SPOT, KIND_BOX] {
            let p = ohpl_sl_light_new(kind);
            // SAFETY: fresh pod.
            unsafe {
                assert!(ohpl_sl_light_is_lit(p));
                let c = Color::default();
                assert!(ohpl_sl_light_set_diffuse(p, &c));
                assert!(!ohpl_sl_light_is_lit(p));
                ohpl_sl_light_attach_ps(p, 7);
                assert!(ohpl_sl_light_remove_ps(p, 7));
                assert!(!ohpl_sl_light_remove_ps(p, 7));
                ohpl_sl_light_free(p);
            }
        }
    }

    #[test]
    fn box_fade_reaches_target() {
        let p = ohpl_sl_light_new(KIND_BOX).cast::<BoxPod>();
        let b = [Vector3f {
            x: 1.0,
            y: 2.0,
            z: 3.0,
        }; 9];
        // SAFETY: fresh pod.
        unsafe {
            ohpl_sl_box_add_set(p, b"A".as_ptr(), 1, b.as_ptr(), 9);
            ohpl_sl_box_add_set(p, b"B".as_ptr(), 1, b.as_ptr(), 3);
            assert!(ohpl_sl_box_fade_set(p, b"b".as_ptr(), 1, 0.0));
            ohpl_sl_box_fade_step(p, 0.0);
            assert!((*p.cast::<BoxObj>()).box_rest.fade_target.is_none());
            assert_eq!((*p).bands[8], Vector3f::default());
            ohpl_sl_light_free(p.cast());
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FlarePod {
    pub mat_mgr: usize,
    pub low_level: usize,
    pub vtx: usize,
    pub multi_iris_vtx: usize,
    pub occlusion_bv: usize,
    pub types: [usize; 3],
    pub material: [usize; 3],
    pub size_rot: [Matrixf; 3],
    pub occluder: Matrixf,
    pub billboard: Matrixf,
    pub active: [bool; 3],
    pub color: [Color; 3],
    pub size: [Vector2f; 3],
    pub atlas_grid: Vector2l,
    pub multi_iris_seed: i32,
    pub multi_iris_count: i32,
    pub size_by_dist: f32,
    pub use_parent_mesh: bool,
    pub shrink_when_occluded: bool,
    pub mul_multi_iris: bool,
    pub range_max: Vector2f,
    pub range_min: Vector2f,
    pub glare_range: Vector2f,
    pub outer_fov: f32,
    pub inner_fov: f32,
    pub glare_fov: f32,
    pub outer_cos: f32,
    pub inner_cos: f32,
    pub glare_cos: f32,
    pub glare_brightness: f32,
    pub glare_stare_at: f32,
    pub fade: f32,
    pub fade_rate: f32,
    pub brightness: f32,
    pub source_size: Vector3f,
    pub vis: VisState,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FlareTypePod {
    pub parent: usize,
    pub kind: i32,
    pub model: Matrixf,
}

pub(crate) fn ohpl_sl_flare_new(seed: i32) -> *mut FlarePod {
    let v2 = |x: f32, y: f32| Vector2f { x, y };
    let mut size = [v2(0.125, 0.125); 3];
    size[0] = v2(0.25, 0.0625);
    boxed(FlarePod {
        color: [white(); 3],
        size,
        atlas_grid: Vector2l { x: 1, y: 1 },
        multi_iris_seed: seed,
        multi_iris_count: 16,
        size_by_dist: 0.5,
        use_parent_mesh: true,
        shrink_when_occluded: true,
        range_max: v2(-1.0, -1.0),
        range_min: v2(-1.0, -1.0),
        glare_range: v2(-1.0, -1.0),
        outer_fov: std::f32::consts::PI,
        inner_fov: std::f32::consts::PI,
        outer_cos: -1.0,
        inner_cos: -1.0,
        glare_cos: 1.0,
        glare_brightness: 0.5,
        glare_stare_at: 0.2,
        fade: 1.0,
        fade_rate: 1.0,
        brightness: 1.0,
        source_size: Vector3f {
            x: 0.5,
            y: 0.5,
            z: 0.5,
        },
        vis: VisState {
            target: 0.0,
            vis: 1.0,
            vel: 0.0,
            frame: -1,
        },
        ..FlarePod::default()
    })
}

pub(crate) unsafe fn ohpl_sl_flare_free(p: *mut FlarePod) {
    unsafe { unbox(p) }
}

impl FlarePod {
    pub(super) fn is_active(&self, i: usize) -> bool {
        self.active.get(i).copied().unwrap_or(false)
            && self.material.get(i).is_some_and(|m| *m != 0)
    }

    fn actives(&self) -> [bool; 3] {
        [self.is_active(0), self.is_active(1), self.is_active(2)]
    }

    fn make_in(&self) -> lensflare::FlareIn {
        lensflare::FlareIn {
            active: self.actives(),
            color: self.color,
            size: self.size,
            range_max: self.range_max,
            range_min: self.range_min,
            glare_range: self.glare_range,
            atlas_grid: self.atlas_grid,
            multi_iris_seed: self.multi_iris_seed,
            multi_iris_count: self.multi_iris_count,
            size_by_dist: self.size_by_dist,
            outer_cos: self.outer_cos,
            inner_cos: self.inner_cos,
            glare_cos: self.glare_cos,
            glare_brightness: self.glare_brightness,
            glare_stare_at: self.glare_stare_at,
            fade: self.fade,
            brightness: self.brightness,
            shrink_when_occluded: self.shrink_when_occluded,
            mul_iris_with_glare: self.mul_multi_iris,
        }
    }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_flare_fade(p: *mut FlarePod, fade_in: bool, time: f32) {
    // SAFETY: caller contract.
    unsafe {
        let f = &mut *p;
        f.fade_rate = if fade_in {
            lensflare::fade_in_rate(f.fade, time)
        } else {
            lensflare::fade_out_rate(f.fade, time)
        };
    }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_flare_update(p: *mut FlarePod, dt: f32) {
    // SAFETY: caller contract.
    unsafe { (*p).fade = lensflare::update_fade((*p).fade, (*p).fade_rate, dt) }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_flare_update_visibility(p: *mut FlarePod, dt: f32, frame: i32) {
    // SAFETY: caller contract.
    unsafe { lensflare::update_visibility(&mut (*p).vis, dt, frame) }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_flare_set_outer_fov(p: *mut FlarePod, a: f32) {
    // SAFETY: caller contract.
    unsafe {
        (*p).outer_fov = a;
        (*p).outer_cos = lensflare::outer_cos(a, (*p).inner_cos);
    }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_flare_set_inner_fov(p: *mut FlarePod, a: f32) {
    // SAFETY: caller contract.
    unsafe {
        (*p).inner_fov = a;
        let (i, o) = lensflare::set_inner_fov(a, (*p).outer_cos);
        (*p).inner_cos = i;
        (*p).outer_cos = o;
    }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_flare_set_glare_fov(p: *mut FlarePod, a: f32) {
    // SAFETY: caller contract.
    unsafe {
        (*p).glare_fov = a;
        (*p).glare_cos = (a * 0.5).cos();
    }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_flare_set_size_by_dist(p: *mut FlarePod, x: f32) {
    // SAFETY: caller contract.
    unsafe { (*p).size_by_dist = lensflare::clamp01(x) }
}

pub(crate) unsafe fn ohpl_sl_flare_bv_radius(p: *const FlarePod) -> f32 {
    unsafe { lensflare::bv_radius(&(*p).active, &(*p).size) }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_flare_first_active(p: *const FlarePod) -> i32 {
    // SAFETY: caller contract.
    unsafe { h12b_ent::first_active_flare(&(*p).actives()) }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sl_flare_any_active(p: *const FlarePod) -> bool {
    // SAFETY: caller contract.
    unsafe { h12b_ent::any_flare_active((*p).fade, &(*p).actives()) }
}

pub(crate) unsafe fn ohpl_sl_flare_color(
    p: *const FlarePod,
    e: *const lensflare::EntIn,
    v: *const lensflare::ViewIn,
    kind: i32,
    vis: f32,
    out: *mut Color,
) {
    let Some(kind) = lensflare::Kind::from_i32(kind) else {
        return;
    };
    unsafe { *out = lensflare::color_and_brightness(&(*p).make_in(), &*e, &*v, kind, vis) }
}

pub(crate) unsafe fn ohpl_sl_flare_glare(
    p: *const FlarePod,
    e: *const lensflare::EntIn,
    v: *const lensflare::ViewIn,
    scale: f32,
    out: *mut Color,
    screen: *mut Vector3f,
) {
    unsafe {
        let (c, s) = lensflare::glare(&(*p).make_in(), &*e, &*v, scale);
        *out = c;
        *screen = s;
    }
}

pub(crate) unsafe fn ohpl_sl_flare_size_rotation(
    p: *mut FlarePod,
    e: *const lensflare::EntIn,
    v: *const lensflare::ViewIn,
    kind: i32,
    vis: f32,
) -> *mut Matrixf {
    unsafe {
        let i = usize::try_from(kind).unwrap_or(0).min(2);
        let Some(m) = (*p).size_rot.get_mut(i).map(|m| m as *mut Matrixf) else {
            return std::ptr::null_mut();
        };
        if let Some(kind) = lensflare::Kind::from_i32(kind) {
            *m = lensflare::size_and_rotation(&(*p).make_in(), &*e, &*v, kind, vis);
        }
        m
    }
}

pub(crate) unsafe fn ohpl_sl_flare_billboard(
    p: *mut FlarePod,
    e: *const lensflare::EntIn,
    v: *const lensflare::ViewIn,
) -> *mut Matrixf {
    unsafe {
        let m: *mut Matrixf = &mut (*p).billboard;
        *m = lensflare::billboard_matrix(&(*p).make_in(), &*e, &*v);
        m
    }
}

pub(crate) unsafe fn ohpl_sl_flare_type_color(
    p: *const FlarePod,
    e: *const lensflare::EntIn,
    v: *const lensflare::ViewIn,
    kind: i32,
    vis: f32,
    out: *mut Color,
) -> bool {
    let Some(kind) = lensflare::Kind::from_i32(kind) else {
        return false;
    };
    match unsafe { lensflare::type_color(&(*p).make_in(), &*e, &*v, kind, vis) } {
        Some(c) => {
            unsafe { *out = c };
            true
        }
        None => false,
    }
}

#[allow(clippy::too_many_arguments)]
pub(crate) unsafe fn ohpl_sl_flare_collide_box(
    p: *const FlarePod,
    v: *const lensflare::ViewIn,
    wc: *const Vector3f,
    lmin: *const Vector3f,
    lmax: *const Vector3f,
    lc: *const Vector3f,
    out_min: *mut Vector3f,
    out_max: *mut Vector3f,
) -> bool {
    unsafe {
        match lensflare::collide_box(&(*p).make_in(), &*v, *wc, *lmin, *lmax, *lc) {
            Some((a, b)) => {
                *out_min = a;
                *out_max = b;
                true
            }
            None => false,
        }
    }
}

pub(crate) unsafe fn ohpl_sl_flare_set_target(
    p: *mut FlarePod,
    vis: f32,
    total: f32,
    vmin: *const Vector3f,
    vmax: *const Vector3f,
) {
    unsafe { (*p).vis.target = lensflare::occlusion_target(vis, total, *vmin, *vmax) }
}

#[allow(clippy::too_many_arguments)]
pub(crate) unsafe fn ohpl_sl_flare_multi_iris(
    p: *const FlarePod,
    e: *const lensflare::EntIn,
    v: *const lensflare::ViewIn,
    vis: f32,
    pos: *mut f32,
    col: *mut f32,
    uv: *mut f32,
    count: i32,
) -> i32 {
    let q = count.max(0);
    let n = unsafe {
        lensflare::multi_iris(
            &(*p).make_in(),
            &*e,
            &*v,
            vis,
            slice_mut(pos, usize::try_from(q.saturating_mul(16)).unwrap_or(0)),
            slice_mut(col, usize::try_from(q.saturating_mul(16)).unwrap_or(0)),
            slice_mut(uv, usize::try_from(q.saturating_mul(12)).unwrap_or(0)),
        )
    };
    i32::try_from(n).unwrap_or(i32::MAX)
}
