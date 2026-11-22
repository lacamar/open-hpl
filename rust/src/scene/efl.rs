//! `cEngineFileLoading`: world entities (fog area, particle system, sound, billboard, lens flare,
//! lights) made from map and entity-file elements. The C++ objects behind the pods stay behind
//! one small host table (`RustEflHost.cpp`); the decal mesh helper is still C++.

use super::h10::{self, EntBox};
use super::ls_host;
use super::ls_light;
use super::sl::{BbPod, BoxPod, FlarePod, FogPod, KIND_POINT, KIND_SPOT, LightPod, SpotPod};
use super::wd::{
    World, ohpl_wd_create_billboard, ohpl_wd_create_lens_flare, ohpl_wd_create_light,
    ohpl_wd_create_particle_system, ohpl_wd_create_simple, ohpl_wd_create_sound,
};
use super::{ls_bb, ls_flare, rnd::Rnd, sl, sp_sys};
use crate::math::libm::c_rand;
use crate::math::{Vector2l, matrix_mul, matrix_rotate, matrix_scale};
use crate::resmgr::{mat, world as rmw};
use crate::types::{Color, Vector2f, Vector3f};
use crate::world_hpm::el::El;
use crate::world_hpm::h4_plan::set_translation;
use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

type P = *mut c_void;

const FOG_AREA: u32 = 11;
const KIND_FOG: u32 = 0;
const KIND_LIGHT: u32 = 1;
const KIND_BILLBOARD: u32 = 2;
const KIND_FLARE: u32 = 3;
const KIND_PARTICLE: u32 = 4;
const KIND_SOUND: u32 = 5;

const TEX_1D: i32 = 0;
const TEX_2D: i32 = 1;
const TEX_CUBE: i32 = 2;
const TEX_ANIM_2D: i32 = 3;
const TEX_ANIM_CUBE: i32 = 4;

const PI_F: f32 = std::f32::consts::PI;
const OP_CULL_DISTANCE: i32 = 1;
const OP_CULL_FOG: i32 = 2;

type AttrSink = unsafe extern "C" fn(ctx: P, name: *const u8, nl: usize, val: *const u8, vl: usize);
type ChildSink = unsafe extern "C" fn(ctx: P, child: P);

/// Engine objects behind the pods and the XML elements the C++ callers hold.
#[repr(C)]
pub struct Host {
    /// Fills `out[0..3]` with the kind pod, `OhplEntPod` and `OhplRnd` (null when the class has none).
    pub pods: unsafe extern "C" fn(kind: u32, obj: P, out: *mut P),
    pub tex_create:
        unsafe extern "C" fn(res: P, kind: i32, name: *const u8, len: usize, srgb: bool) -> P,
    pub tex_frame_time: unsafe extern "C" fn(tex: P, time: f32),
    pub el_name: unsafe extern "C" fn(el: P, len: *mut usize) -> *const u8,
    pub el_attrs: unsafe extern "C" fn(el: P, sink: AttrSink, ctx: P),
    pub el_children: unsafe extern "C" fn(el: P, sink: ChildSink, ctx: P),
    pub h10: *const h10::Host,
}

static HOST: AtomicPtr<Host> = AtomicPtr::new(std::ptr::null_mut());

/// # Safety
/// `h` outlives the process and matches [`Host`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_efl_set_host(h: *const Host) {
    HOST.store(h.cast_mut(), Ordering::Release);
}

fn host() -> Option<&'static Host> {
    // SAFETY: set once by `ohpl_efl_set_host`; the table is static.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }
}

unsafe extern "C" fn push_attr(ctx: P, n: *const u8, nl: usize, v: *const u8, vl: usize) {
    // SAFETY: `ctx` is the element being filled; the pointers hold `nl` and `vl` bytes.
    unsafe {
        let el = &mut *ctx.cast::<El>();
        let n = crate::string_ffi::units(n, nl);
        let v = crate::string_ffi::units(v, vl);
        el.attrs
            .insert(crate::xml::c_str(n).to_vec(), crate::xml::c_str(v).to_vec());
    }
}

unsafe extern "C" fn push_child(ctx: P, child: P) {
    // SAFETY: `ctx` is a `Vec` of child pointers.
    unsafe { (*ctx.cast::<Vec<P>>()).push(child) }
}

/// Copies a C++ `cXmlElement` subtree.
///
/// # Safety
/// `el` is a live element and the host table is registered.
unsafe fn read_el(h: &Host, el: P) -> El {
    let mut out = El::default();
    // SAFETY: caller contract; the sinks match the table.
    unsafe {
        let mut n = 0usize;
        let p = (h.el_name)(el, &mut n);
        out.name = crate::xml::c_str(crate::string_ffi::units(p, n)).to_vec();
        (h.el_attrs)(el, push_attr, (&raw mut out).cast());
        let mut kids: Vec<P> = Vec::new();
        (h.el_children)(el, push_child, (&raw mut kids).cast());
        out.children = kids.into_iter().map(|k| read_el(h, k)).collect();
    }
    out
}

struct Obj {
    pod: P,
    ent: *mut EntBox,
    rnd: *mut Rnd,
}

fn pods(h: &Host, kind: u32, obj: P) -> Obj {
    let mut out = [std::ptr::null_mut::<c_void>(); 3];
    // SAFETY: table contract.
    unsafe { (h.pods)(kind, obj, out.as_mut_ptr()) };
    Obj {
        pod: out[0],
        ent: out[1].cast(),
        rnd: out[2].cast(),
    }
}

fn zero3() -> Vector3f {
    Vector3f {
        x: 0.0,
        y: 0.0,
        z: 0.0,
    }
}

fn splat3(v: f32) -> Vector3f {
    Vector3f { x: v, y: v, z: v }
}

fn splat2(v: f32) -> Vector2f {
    Vector2f { x: v, y: v }
}

fn gray(v: f32, a: f32) -> Color {
    Color {
        r: v,
        g: v,
        b: v,
        a,
    }
}

fn join(prefix: &[u8], name: Vec<u8>) -> Vec<u8> {
    let mut s = prefix.to_vec();
    s.extend(name);
    s
}

/// `SetupWorldEntity`.
///
/// # Safety
/// `o.ent` live.
unsafe fn setup(h: &Host, o: &Obj, el: &El) {
    let id = el.int(b"ID", 0);
    let pos = el.vec3(b"WorldPos", zero3());
    let scale = el.vec3(b"Scale", splat3(1.0));
    let rot = el.vec3(b"Rotation", zero3());
    let mut m = matrix_mul(&matrix_rotate(rot, 0), &matrix_scale(scale));
    set_translation(&mut m, pos);
    // SAFETY: caller contract; `h.h10` is the C++ host table.
    unsafe {
        h10::ent_set_matrix(&*h.h10, o.ent, &m);
        (*o.ent).s.unique_id = id;
    }
}

/// `iEntity3D::SetTransformUpdated()`.
///
/// # Safety
/// `ent` live.
unsafe fn transform_updated(h: &Host, ent: *mut EntBox) {
    // SAFETY: caller contract.
    unsafe { h10::ent_set_transform_updated(&*h.h10, ent, true) }
}

/// `iRenderable::SetCulledByDistance`, `SetCulledByFog`.
///
/// # Safety
/// `r` live.
unsafe fn cull(r: *mut Rnd, by_distance: bool, by_fog: bool) {
    // SAFETY: caller contract.
    unsafe {
        (*r).culled_by_distance = by_distance;
        (*r).distance_cull_fade = 1.0;
        (*r).culled_by_fog = by_fog;
    }
}

fn tex(h: &Host, res: P, kind: i32, name: &[u8], srgb: bool) -> usize {
    // SAFETY: table contract.
    unsafe { (h.tex_create)(res, kind, name.as_ptr(), name.len(), srgb) as usize }
}

/// # Safety
/// `w` live.
pub(crate) unsafe fn load_fog_area(h: &Host, el: &El, prefix: &[u8], w: *mut World, st: bool) -> P {
    let name = join(prefix, el.string(b"Name"));
    // SAFETY: caller contract; each pod is live while the world holds the entity.
    unsafe {
        let p = ohpl_wd_create_simple(
            w,
            FOG_AREA,
            name.as_ptr(),
            name.len(),
            st,
            false,
            std::ptr::null_mut(),
            0,
        );
        if p.is_null() {
            return p;
        }
        let o = pods(h, KIND_FOG, p);
        let f = &mut *o.pod.cast::<FogPod>();
        f.color = el.color(b"Color", gray(1.0, 1.0));
        (*o.rnd).culled_by_fog = el.boolean(b"CulledByFog", true);
        f.brightness = el.float(b"Brightness", 1.0);
        f.start = el.float(b"Start", 0.0);
        f.end = el.float(b"End", 0.0);
        f.falloff_exp = el.float(b"FalloffExp", 0.0);
        f.backside_inside = el.boolean(b"ShownBacksideWhenInside", true);
        f.backside_outside = el.boolean(b"ShownBacksideWhenOutside", true);
        f.underwater = el.boolean(b"Underwater", false);
        f.skybox = el.boolean(b"Skybox", false);
        f.lighten = el.boolean(b"Lighten", false);
        f.height_based = el.boolean(b"HeightBased", false);
        f.exponential = el.boolean(b"Exponential", false);
        f.density = el.float(b"Density", 0.25);
        f.height_density = el.float(b"HeightDensity", 0.25);
        f.height_horizon = el.float(b"HeightHorizon", 1.0);
        f.noise_strength = el.float(b"NoiseStrength", 0.0);
        f.noise_size = el.float(b"NoiseSize", 1.0);
        f.noise_turbulence = el.vec3(b"NoiseTurbulence", zero3());
        setup(h, &o, el);
        p
    }
}

/// # Safety
/// `w` live.
pub(crate) unsafe fn load_particle_system(h: &Host, el: &El, prefix: &[u8], w: *mut World) -> P {
    let name = join(prefix, el.string(b"Name"));
    let file = el.string(b"File");
    let size = splat3(1.0);
    // SAFETY: caller contract.
    unsafe {
        let p = ohpl_wd_create_particle_system(
            w,
            0,
            name.as_ptr(),
            name.len(),
            file.as_ptr(),
            file.len(),
            std::ptr::null_mut(),
            &size,
            true,
        );
        if p.is_null() {
            return p;
        }
        let o = pods(h, KIND_PARTICLE, p);
        let s = &mut *o.pod.cast::<sp_sys::SpPub>();
        s.color = el.color(b"Color", gray(1.0, 1.0));
        s.brightness = el.float(b"Brightness", 1.0);
        s.fade_at_distance = el.boolean(b"FadeAtDistance", false);
        s.min_fade_start = el.float(b"MinFadeDistanceStart", 0.0);
        s.min_fade_end = el.float(b"MinFadeDistanceEnd", 0.0);
        s.max_fade_start = el.float(b"MaxFadeDistanceStart", 0.0);
        s.max_fade_end = el.float(b"MaxFadeDistanceEnd", 0.0);
        let sys = o.pod.cast::<sp_sys::Sys>();
        sp_sys::ohpl_sp_apply_all(sys, OP_CULL_DISTANCE, el.boolean(b"CulledByDistance", true));
        sp_sys::ohpl_sp_apply_all(sys, OP_CULL_FOG, el.boolean(b"CulledByFog", true));
        setup(h, &o, el);
        p
    }
}

/// # Safety
/// `w` live.
pub(crate) unsafe fn load_sound(h: &Host, el: &El, prefix: &[u8], w: *mut World) -> P {
    let name = join(prefix, el.string(b"Name"));
    let file = el.string(b"SoundEntityFile");
    let custom = el
        .attr(b"UseCustomProperties")
        .map(|_| el.boolean(b"UseCustomProperties", false));
    let use_default = rmw::sound_use_default(el.boolean(b"UseDefault", false), custom);
    if file.is_empty() {
        return std::ptr::null_mut();
    }
    // SAFETY: caller contract.
    unsafe {
        let p = ohpl_wd_create_sound(
            w,
            name.as_ptr(),
            name.len(),
            file.as_ptr(),
            file.len(),
            false,
        );
        if p.is_null() {
            return p;
        }
        let o = pods(h, KIND_SOUND, p);
        if !use_default {
            let e = o.pod.cast();
            crate::sound::own_ffi::ohpl_sndse_set(e, 1, el.float(b"MinDistance", 0.0));
            crate::sound::own_ffi::ohpl_sndse_set(e, 2, el.float(b"MaxDistance", 0.0));
            crate::sound::own_ffi::ohpl_sndse_set(e, 0, el.float(b"Volume", 0.0));
        }
        setup(h, &o, el);
        p
    }
}

/// Returns the entity and, when the billboard names a light, `(billboard id, prefixed light name)`.
///
/// # Safety
/// `w` live.
pub(crate) unsafe fn load_billboard(
    h: &Host,
    el: &El,
    prefix: &[u8],
    w: *mut World,
    st: bool,
    want_connection: bool,
) -> (P, Option<(i32, Vec<u8>)>) {
    let name = join(prefix, el.string(b"Name"));
    let size = el.vec2(b"BillboardSize", splat2(0.0));
    let mat_file = el.string(b"MaterialFile");
    let ty = rmw::billboard_type(&el.string(b"BillboardType"));
    // SAFETY: caller contract.
    unsafe {
        let p = ohpl_wd_create_billboard(
            w,
            name.as_ptr(),
            name.len(),
            &size,
            ty,
            mat_file.as_ptr(),
            mat_file.len(),
            st,
        );
        if p.is_null() {
            return (p, None);
        }
        let o = pods(h, KIND_BILLBOARD, p);
        let b = o.pod.cast::<BbPod>();
        (*b).forward_offset = el.float(b"BillboardOffset", 0.0);
        ls_bb::ohpl_ls_bb_set_color(b, &el.color(b"BillboardColor", gray(1.0, 1.0)));
        ls_bb::ohpl_ls_bb_set_brightness(b, el.float(b"Brightness", 1.0));
        ls_bb::ohpl_ls_bb_set_is_halo(b, el.boolean(b"IsHalo", false));
        (*b).halo_source_size = el.vec3(b"HaloSourceSize", splat3(1.0));
        (*b).halo_size_updated = true;
        cull(
            o.rnd,
            el.boolean(b"CulledByDistance", true),
            el.boolean(b"CulledByFog", true),
        );
        let light = el.string(b"ConnectLight");
        let conn =
            (want_connection && !light.is_empty()).then(|| (el.int(b"ID", 0), join(prefix, light)));
        setup(h, &o, el);
        (p, conn)
    }
}

/// # Safety
/// `w` live.
pub(crate) unsafe fn load_lens_flare(
    h: &Host,
    el: &El,
    prefix: &[u8],
    w: *mut World,
    st: bool,
) -> P {
    let name = join(prefix, el.string(b"Name"));
    let size = splat2(1.0);
    let is_static = st || el.boolean(b"Static", false);
    // SAFETY: caller contract.
    unsafe {
        let p = ohpl_wd_create_lens_flare(
            w,
            name.as_ptr(),
            name.len(),
            &size,
            std::ptr::null(),
            0,
            is_static,
        );
        if p.is_null() {
            return p;
        }
        let o = pods(h, KIND_FLARE, p);
        let f = o.pod.cast::<FlarePod>();
        (*o.rnd).culled_by_fog = el.boolean(b"CulledByFog", true);
        for (i, prefix) in ["Anamorphic", "Flare", "MultiIris"].iter().enumerate() {
            let k = i as i32;
            let n = |s: &str| format!("{s}{prefix}").into_bytes();
            ls_flare::ohpl_ls_flare_set_active(f, k, el.boolean(&n("Active"), false));
            ls_flare::ohpl_ls_flare_set_size(f, k, &el.vec2(&n("Size"), splat2(0.125)));
            if let Some(c) = (*f).color.get_mut(i) {
                *c = el.color(&n("Color"), gray(1.0, 1.0));
            }
            let mat_name = el.string(&n("Material"));
            let m = if mat_name.is_empty() {
                0
            } else {
                ls_host::mat_create((*w).pod.resources as usize, &mat_name)
            };
            ls_flare::ohpl_ls_flare_set_material(f, k, m);
        }
        (*f).multi_iris_seed = el.int(b"MultiIrisSeed", c_rand());
        (*f).multi_iris_count = el.int(b"MultiIrisCount", 16);
        let g = el.vec2(b"MultiIrisTextureAtlasGrid", splat2(1.0));
        (*f).atlas_grid = Vector2l {
            x: g.x as i32,
            y: g.y as i32,
        };
        let by_dist = el.float(b"SizeChangeBasedOnDistance", 0.0);
        sl::ohpl_sl_flare_set_size_by_dist(f, by_dist.clamp(0.0, 1.0));
        (*f).use_parent_mesh = el.boolean(b"UseParentMeshForOcclusion", true);
        (*f).shrink_when_occluded = el.boolean(b"ShrinkWhenOccluded", true);
        (*f).mul_multi_iris = el.boolean(b"MulGlareWithMultiIris", false);
        sl::ohpl_sl_flare_set_outer_fov(f, el.float(b"OuterFOV", PI_F));
        sl::ohpl_sl_flare_set_inner_fov(f, el.float(b"InnerFOV", PI_F));
        (*f).glare_brightness = el.float(b"GlareBrightness", 0.5);
        sl::ohpl_sl_flare_set_glare_fov(f, el.float(b"GlareFOV", 0.0));
        (*f).glare_stare_at = el.float(b"GlareStareAt", 0.5);
        (*f).glare_range = el.vec2(b"GlareRange", splat2(0.0));
        (*f).range_max = el.vec2(b"RangeMax", splat2(-1.0));
        (*f).range_min = el.vec2(b"RangeMin", splat2(-1.0));
        ls_flare::ohpl_ls_flare_set_source_size(f, &el.vec3(b"SourceSize", splat3(0.5)));
        (*f).brightness = el.float(b"Brightness", 1.0);
        setup(h, &o, el);
        p
    }
}

/// # Safety
/// `w` live.
pub(crate) unsafe fn load_light(h: &Host, el: &El, prefix: &[u8], w: *mut World, st: bool) -> P {
    let name = join(prefix, el.string(b"Name"));
    let res = unsafe { (*w).pod.resources };
    let Some(kind) = rmw::light_kind(&el.name) else {
        ls_host::log(
            true,
            &format!(
                "Unknown light type '{}'\n",
                String::from_utf8_lossy(&el.name)
            ),
        );
        return std::ptr::null_mut();
    };
    // SAFETY: caller contract; pods live while the world holds the entity.
    unsafe {
        let which = match kind {
            rmw::Light::Point => 0,
            rmw::Light::Spot => 1,
            rmw::Light::Box => 2,
        };
        let p = ohpl_wd_create_light(w, which, name.as_ptr(), name.len(), std::ptr::null(), 0, st);
        let o = pods(h, KIND_LIGHT, p);
        let l = o.pod.cast::<LightPod>();
        match kind {
            rmw::Light::Box => {
                let b = &mut *o.pod.cast::<BoxPod>();
                b.size = el.vec3(b"Size", splat3(1.0));
                (*o.ent).s.update_bv = true;
                transform_updated(h, o.ent);
                b.blend = el.int(b"BlendFunc", 1);
                b.ambient_sky = el.color(b"SkyColor", gray(1.0, 1.0));
                b.ambient_ground = el.color(b"GroundColor", gray(1.0, 1.0));
                b.weight = el.float(b"Weight", 1.0);
                b.bevel = el.float(b"Bevel", 0.0);
                b.falloff_pow = el.float(b"FalloffPow", 0.0);
                b.use_sh = el.boolean(b"UseSphericalHarmonics", false);
                b.probe_offset = el.vec3(b"ProbeOffset", zero3());
                if let Some(irr) = el.first(b"Irradiance") {
                    for set in &irr.children {
                        let vals: Vec<f32> =
                            crate::string::split(set.attr(b"_Text").unwrap_or(b""), None, true)
                                .map(crate::string::token_to_float)
                                .collect();
                        let bands: Vec<Vector3f> = vals
                            .chunks_exact(3)
                            .filter_map(|c| match *c {
                                [x, y, z] => Some(Vector3f { x, y, z }),
                                _ => None,
                            })
                            .take(rmw::band_count(vals.len()))
                            .collect();
                        let set_name = set.string(b"SetName");
                        sl::ohpl_sl_box_add_set(
                            b,
                            set_name.as_ptr(),
                            set_name.len(),
                            bands.as_ptr(),
                            bands.len(),
                        );
                    }
                }
            }
            rmw::Light::Spot => {
                let s = &mut *o.pod.cast::<SpotPod>();
                ls_light::ohpl_ls_spot_set_fov(l, el.float(b"FOV", 1.0));
                s.aspect = el.float(b"Aspect", 1.0);
                s.ctl.projection_updated = true;
                let sc = el.vec3(b"Scale", splat3(1.0));
                let near = el.float(b"NearClipPlane", 0.1) * (sc.x + sc.y + sc.z) / 3.0;
                s.near_clip = if near > 0.001 { near } else { 0.001 };
                s.ctl.projection_updated = true;
                let falloff = el.string(b"SpotFalloffMap");
                if !falloff.is_empty() {
                    let t = tex(h, res, TEX_1D, &falloff, false);
                    if t != 0 {
                        ls_light::ohpl_ls_spot_set_falloff_map(l, t);
                    }
                }
            }
            rmw::Light::Point => {}
        }
        let light_type = (*l).light_type;
        if light_type == KIND_POINT || light_type == KIND_SPOT {
            let falloff = el.string(b"FalloffMap");
            if !falloff.is_empty() {
                let t = tex(h, res, TEX_1D, &falloff, false);
                if t != 0 {
                    ls_light::ohpl_ls_light_set_falloff_map(l, t);
                }
            }
            let gobo = el.string(b"Gobo");
            (*l).gobo_specular = el.string(b"GoboType") == b"Specular";
            if !gobo.is_empty() {
                let anim = mat::anim_mode(&el.string(b"GoboAnimMode"));
                let frame_time = el.float(b"GoboAnimFrameTime", 1.0);
                let k = match rmw::gobo_kind(light_type == KIND_SPOT, anim) {
                    rmw::Gobo::Tex2D => TEX_2D,
                    rmw::Gobo::Anim2D => TEX_ANIM_2D,
                    rmw::Gobo::Cube => TEX_CUBE,
                    rmw::Gobo::AnimCube => TEX_ANIM_CUBE,
                };
                let t = tex(h, res, k, &gobo, true);
                if t != 0 {
                    ls_light::ohpl_ls_light_set_gobo(l, t);
                    (h.tex_frame_time)(t as P, frame_time);
                }
            }
        }
        (*l).cast_shadows = el.boolean(b"CastShadows", false);
        cull(
            o.rnd,
            el.boolean(b"CulledByDistance", true),
            el.boolean(b"CulledByFog", true),
        );
        ls_light::ohpl_ls_light_set_diffuse(l, &el.color(b"DiffuseColor", gray(1.0, 1.0)));
        (*l).default_diffuse = (*l).diffuse;
        (*l).cast_diffuse = el.boolean(b"CastDiffuseLight", true);
        (*l).cast_specular = el.boolean(b"CastSpecularLight", true);
        ls_light::ohpl_ls_light_set_radius(l, el.float(b"Radius", 1.0));
        (*l).falloff_pow = el.float(b"FalloffPow", 1.0);
        (*l).spot_falloff_pow = el.float(b"SpotFalloffPow", 1.0);
        ls_light::ohpl_ls_light_set_brightness(l, el.float(b"Brightness", 1.0));
        (*l).default_brightness = (*l).brightness;
        let mask = el.string_or(b"ConnectedLightMaskID", b"4294967295");
        (*l).mask_id = rmw::light_mask_id(&mask);
        (*l).shadow_res = rmw::shadow_resolution(&el.string_or(b"ShadowResolution", b"High"));
        let dynamic = el.boolean(b"ShadowsAffectDynamic", true);
        let stat = el.boolean(b"ShadowsAffectStatic", true);
        (*l).shadow_casters_affected = rmw::shadow_casters(dynamic, stat);
        let fade_on = el.float(b"FlickerOnFadeLength", 0.0);
        let fade_off = el.float(b"FlickerOffFadeLength", 0.0);
        (*l).anim.flickering = el.boolean(b"FlickerActive", false);
        let strings = [
            el.string(b"FlickerOnSound"),
            el.string(b"FlickerOnPS"),
            el.string(b"FlickerOffSound"),
            el.string(b"FlickerOffPS"),
        ];
        let ptrs = [
            strings[0].as_ptr(),
            strings[1].as_ptr(),
            strings[2].as_ptr(),
            strings[3].as_ptr(),
        ];
        let lens = [
            strings[0].len(),
            strings[1].len(),
            strings[2].len(),
            strings[3].len(),
        ];
        ls_light::ohpl_ls_light_set_flicker(
            l,
            &el.color(b"FlickerOffColor", gray(0.0, 0.0)),
            el.float(b"FlickerOffRadius", 0.0),
            el.float(b"FlickerOnMinLength", 0.0),
            el.float(b"FlickerOnMaxLength", 0.0),
            // the C++ call passes OffMax where the parameter is OffMin
            el.float(b"FlickerOffMaxLength", 0.0),
            el.float(b"FlickerOffMinLength", 0.0),
            el.boolean(b"FlickerFade", false),
            el.float(b"FlickerOnFadeMinLength", fade_on),
            el.float(b"FlickerOnFadeMaxLength", fade_on),
            el.float(b"FlickerOffFadeMinLength", fade_off),
            el.float(b"FlickerOffFadeMaxLength", fade_off),
            &ptrs,
            &lens,
        );
        setup(h, &o, el);
        p
    }
}

fn name_bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    // SAFETY: callers pass a live string of `n` bytes.
    unsafe { crate::string_ffi::units(p, n) }
}

macro_rules! entry {
    ($name:ident, $load:ident $(, $extra:ident: $ty:ty)?) => {
        /// # Safety
        /// `el` a live `cXmlElement`; `prefix` valid for `pl` bytes; `w` a live world.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(
            el: P,
            prefix: *const u8,
            pl: usize,
            w: *mut World,
            $($extra: $ty)?
        ) -> P {
            let Some(h) = host() else { return std::ptr::null_mut() };
            // SAFETY: caller contract.
            unsafe { $load(h, &read_el(h, el), name_bytes(prefix, pl), w $(, $extra)?) }
        }
    };
}

entry!(ohpl_efl_load_fog_area, load_fog_area, st: bool);
entry!(ohpl_efl_load_particle_system, load_particle_system);
entry!(ohpl_efl_load_sound, load_sound);
entry!(ohpl_efl_load_lens_flare, load_lens_flare, st: bool);
entry!(ohpl_efl_load_light, load_light, st: bool);

/// `sink` receives `(billboard id, light name)` when the billboard names a light and `sink` is set.
///
/// # Safety
/// As the other `ohpl_efl_load_*`; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_efl_load_billboard(
    el: P,
    prefix: *const u8,
    pl: usize,
    w: *mut World,
    st: bool,
    sink: Option<unsafe extern "C" fn(ctx: P, id: i32, light: *const u8, len: usize)>,
    ctx: P,
) -> P {
    let Some(h) = host() else {
        return std::ptr::null_mut();
    };
    // SAFETY: caller contract.
    unsafe {
        let e = read_el(h, el);
        let (p, conn) = load_billboard(h, &e, name_bytes(prefix, pl), w, st, sink.is_some());
        if let (Some(sink), Some((id, light))) = (sink, conn) {
            sink(ctx, id, light.as_ptr(), light.len());
        }
        p
    }
}
