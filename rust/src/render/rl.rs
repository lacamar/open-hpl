//! `cRenderList`: the object lists, the per-frame update gates and the compile step (sort keys
//! gathered from the materials, sorted by `list`). C++ keeps the renderable objects; their
//! virtuals and plain fields are reached through `Host` until the `iRenderable` classes move.

use std::ffi::c_void;

use super::h8::{self, GateIn, PostIn};
use super::list::{
    self, CompileIn, CompileOut, DecalKey, IllumKey, SolidKey, TEXTURE_UNITS, TransKey,
};
use crate::material::h7::MatState;
use crate::math::bv::{Bv, Frustum};
use crate::math::{IDENTITY, Matrixf};
use crate::scene::boxtree_dyn::PtrVec;
use crate::scene::rnd::Rnd;
use crate::types::Vector3f;

type O = *mut c_void;

const RT_SUB_MESH: i32 = 1;
const RT_LENS_FLARE: i32 = 8;
const LENS_MULTI_IRIS: i32 = 2;
const MODE_Z: usize = 0;
const MODE_DIFFUSE: usize = 2;
const TEX_DIFFUSE: usize = 0;
const TEX_ILLUMINATION: usize = 5;

pub const L_OCCLUSION: usize = 0;
pub const L_SOLID: usize = 1;
pub const L_TRANS: usize = 2;
pub const L_DECAL: usize = 3;
pub const L_ILLUM: usize = 4;
pub const L_LIGHT: usize = 5;
pub const L_FOG: usize = 6;
pub const L_FLARE: usize = 7;
pub const L_MULTI_IRIS: usize = 8;
pub const L_SORTED: usize = 9;
const LISTS: usize = L_SORTED + 5;

#[repr(C)]
pub struct Plane {
    pub normal: Vector3f,
    pub point: Vector3f,
    pub world: Matrixf,
}

/// `iRenderable` access: every call takes the object (`iRenderable*`).
#[repr(C)]
pub struct Host {
    pub renderer_frame: unsafe extern "C" fn() -> i32,
    pub render_type: unsafe extern "C" fn(O) -> i32,
    pub material: unsafe extern "C" fn(O) -> *mut MatState,
    /// bit 0 translucent, bit 1 decal of an `iMaterialType`.
    pub type_flags: unsafe extern "C" fn(O) -> u32,
    pub rnd: unsafe extern "C" fn(O) -> *mut Rnd,
    pub update_frame: unsafe extern "C" fn(O, f32),
    pub update_viewport: unsafe extern "C" fn(O, O, f32) -> bool,
    pub model_matrix: unsafe extern "C" fn(O, O) -> *mut Matrixf,
    pub bounding_volume: unsafe extern "C" fn(O) -> *mut Bv,
    pub uses_occlusion_query: unsafe extern "C" fn(O) -> bool,
    pub vertex_buffer: unsafe extern "C" fn(O) -> usize,
    pub world_position: unsafe extern "C" fn(O, *mut Vector3f),
    pub plane: unsafe extern "C" fn(O, *mut Plane) -> bool,
    pub collides_frustum: unsafe extern "C" fn(O, O) -> bool,
    pub flare_active: unsafe extern "C" fn(O, i32) -> bool,
    pub flare_type: unsafe extern "C" fn(O, i32) -> O,
}

pub struct RenderList {
    frame_time: f32,
    cfrustum: O,
    frustum: *const Frustum,
    lists: [PtrVec; LISTS],
    solid: Vec<SolidKey>,
    trans: Vec<TransKey>,
    decal: Vec<DecalKey>,
    illum: Vec<IllumKey>,
}

impl RenderList {
    pub fn new() -> Self {
        Self {
            frame_time: 0.0,
            cfrustum: std::ptr::null_mut(),
            frustum: std::ptr::null(),
            lists: std::array::from_fn(|_| PtrVec::default()),
            solid: Vec::new(),
            trans: Vec::new(),
            decal: Vec::new(),
            illum: Vec::new(),
        }
    }

    pub fn setup(&mut self, frame_time: f32, cfrustum: O, frustum: *const Frustum) {
        self.frame_time = frame_time;
        self.cfrustum = cfrustum;
        self.frustum = frustum;
    }

    #[cfg(test)]
    pub fn sorted(&self, list: usize) -> &[usize] {
        self.lists.get(list).map_or(&[], |l| &l.0)
    }

    fn push(&mut self, list: usize, o: O) {
        if let Some(l) = self.lists.get_mut(list) {
            l.0.push(o as usize);
        }
    }

    pub fn clear(&mut self) {
        for l in &mut self.lists {
            l.0.clear();
        }
    }

    /// # Safety
    /// `h` and every object are live; `frustum` is the `Rs()` view of `cfrustum`.
    pub unsafe fn add_object(&mut self, h: &Host, o: O) {
        // SAFETY: by the caller's contract.
        unsafe {
            if (h.render_type)(o) == RT_LENS_FLARE {
                if !(h.collides_frustum)(o, self.cfrustum) {
                    return;
                }
                self.push(L_OCCLUSION, o);
                self.push(L_FLARE, o);
                for i in 0..LENS_MULTI_IRIS {
                    if (h.flare_active)(o, i) {
                        self.add_object(h, (h.flare_type)(o, i));
                    }
                }
                if (h.flare_active)(o, LENS_MULTI_IRIS) {
                    self.push(L_MULTI_IRIS, o);
                }
                return;
            }
            self.add_plain(h, o);
        }
    }

    unsafe fn add_plain(&mut self, h: &Host, o: O) {
        // SAFETY: by `add_object`.
        unsafe {
            let frame = (h.renderer_frame)();
            let mat = (h.material)(o);
            let type_flags = match mat.as_ref() {
                Some(m) if !m.type_.is_null() => Some((h.type_flags)(m.type_)),
                _ => None,
            };
            let translucent = type_flags.is_some_and(|f| f & 1 != 0);
            let decal = type_flags.is_some_and(|f| f & 2 != 0);
            let gate = GateIn {
                has_material: !mat.is_null(),
                has_type: type_flags.is_some(),
                translucent,
                decal,
                frame,
                material_frame: mat.as_ref().map_or(0, |m| m.render_frame_count),
                object_frame: field(h, o, |r| r.render_frame_count),
            };
            let gates = h8::gates(&gate);
            if gates & h8::GATE_MATERIAL_UPDATE != 0
                && let Some(m) = mat.as_mut()
            {
                m.render_frame_count = frame;
                if m.has_uv {
                    m.update(self.frame_time);
                }
            }
            if gates & h8::GATE_OBJECT_UPDATE != 0 {
                set(h, o, |r| r.render_frame_count = frame);
                (h.update_frame)(o, self.frame_time);
            }
            if gates & h8::GATE_VIEWPORT_UPDATE != 0 {
                if !(h.update_viewport)(o, self.cfrustum, self.frame_time) {
                    return;
                }
                let m = (h.model_matrix)(o, self.cfrustum);
                set(h, o, |r| r.model = m);
            } else {
                let m = (h.model_matrix)(o, std::ptr::null_mut());
                set(h, o, |r| r.model = m);
            }

            let Some(bv) = (h.bounding_volume)(o).as_mut() else {
                return;
            };
            let (bv_min, bv_max) = bv.world_min_max();
            let bv_center = bv.world_center();
            let Some(fr) = self.frustum.as_ref() else {
                return;
            };
            let out = h8::post(&PostIn {
                render_type: (h.render_type)(o),
                has_material: gate.has_material,
                has_type: gate.has_type,
                translucent,
                decal,
                has_illum_texture: mat.as_ref().is_some_and(|m| tex(m, TEX_ILLUMINATION) != 0),
                uses_occlusion_query: (h.uses_occlusion_query)(o),
                illumination: field(h, o, |r| r.illumination),
                bv_min,
                bv_max,
                bv_center,
                origin: fr.origin,
                view: fr.view,
            });
            set(h, o, |r| r.view_space_z = out.view_z);
            for (bit, list) in [
                (h8::LIST_OCCLUSION, L_OCCLUSION),
                (h8::LIST_LIGHT, L_LIGHT),
                (h8::LIST_FOG, L_FOG),
                (h8::LIST_DECAL, L_DECAL),
                (h8::LIST_TRANS, L_TRANS),
                (h8::LIST_SOLID, L_SOLID),
                (h8::LIST_ILLUM, L_ILLUM),
            ] {
                if out.lists & bit != 0 {
                    self.push(list, o);
                }
            }
        }
    }

    fn objects(&self, list: usize) -> Vec<O> {
        self.lists
            .get(list)
            .map_or_else(Vec::new, |l| l.0.iter().map(|&p| p as O).collect())
    }

    /// # Safety
    /// As `add_object`; the listed objects have materials where the lists need them.
    pub unsafe fn compile(&mut self, h: &Host, flags: u32) {
        let z = flags & list::COMPILE_Z != 0;
        let diffuse = flags & list::COMPILE_DIFFUSE != 0;
        let decal = flags & list::COMPILE_DECAL != 0;
        let illum = flags & list::COMPILE_ILLUMINATION != 0;
        let trans = flags & list::COMPILE_TRANSLUCENT != 0;
        if !(z || diffuse || decal || illum || trans) {
            return;
        }
        let solid_objs = if z || diffuse {
            self.objects(L_SOLID)
        } else {
            Vec::new()
        };
        let trans_objs = if trans {
            self.objects(L_TRANS)
        } else {
            Vec::new()
        };
        let decal_objs = if decal {
            self.objects(L_DECAL)
        } else {
            Vec::new()
        };
        let illum_objs = if illum {
            self.objects(L_ILLUM)
        } else {
            Vec::new()
        };

        // SAFETY: by the caller's contract; materials of listed objects are live.
        unsafe {
            self.solid = solid_objs
                .iter()
                .map(|&o| {
                    let m = (h.material)(o).as_ref();
                    let mut k = SolidKey {
                        alpha_mode: m.map_or(0, |m| m.alpha),
                        view_z: field(h, o, |r| r.view_space_z),
                        prog_z: 0,
                        tex_diffuse: 0,
                        prog_diffuse: 0,
                        vtx: 0,
                        mtx: 0,
                        obj: 0,
                        tex_units: [0; TEXTURE_UNITS],
                    };
                    if let Some(m) = m {
                        if z {
                            k.prog_z = prog(m, MODE_Z);
                            k.tex_diffuse = tex(m, TEX_DIFFUSE);
                        }
                        if diffuse {
                            k.prog_diffuse = prog(m, MODE_DIFFUSE);
                            for (u, t) in k.tex_units.iter_mut().enumerate() {
                                *t = unit(m, u);
                            }
                            k.vtx = (h.vertex_buffer)(o);
                            k.mtx = field(h, o, |r| r.model as usize);
                            k.obj = o as usize;
                        }
                    }
                    k
                })
                .collect();

            self.trans = trans_objs
                .iter()
                .map(|&o| {
                    let center = (h.bounding_volume)(o)
                        .as_mut()
                        .map_or(Vector3f::default(), Bv::world_center);
                    let mut k = TransKey {
                        view_z: field(h, o, |r| r.view_space_z),
                        placement: 0,
                        plane_candidate: 0,
                        pad: 0,
                        center,
                        normal: Vector3f::default(),
                        point: Vector3f::default(),
                        world: IDENTITY,
                    };
                    if (h.render_type)(o) == RT_SUB_MESH
                        && (h.material)(o)
                            .as_ref()
                            .is_some_and(|m| m.large_transperant_surface)
                    {
                        let mut p = Plane {
                            normal: Vector3f::default(),
                            point: Vector3f::default(),
                            world: IDENTITY,
                        };
                        if (h.plane)(o, &raw mut p) {
                            k.plane_candidate = 1;
                            k.normal = p.normal;
                            k.point = p.point;
                            k.world = p.world;
                        }
                    }
                    k
                })
                .collect();

            self.decal = decal_objs
                .iter()
                .map(|&o| {
                    let m = (h.material)(o).as_ref();
                    let mut pos = Vector3f::default();
                    (h.world_position)(o, &raw mut pos);
                    DecalKey {
                        sort_order: m.map_or(0, |m| m.decal_sort_order),
                        pad: 0,
                        tex_illum: m.map_or(0, |m| tex(m, TEX_ILLUMINATION)),
                        vtx: (h.vertex_buffer)(o),
                        mtx: field(h, o, |r| r.model as usize),
                        pos,
                        pad2: 0.0,
                    }
                })
                .collect();

            self.illum = illum_objs
                .iter()
                .map(|&o| IllumKey {
                    illum_amount: field(h, o, |r| r.illumination),
                    pad: 0,
                    tex_illum: (h.material)(o)
                        .as_ref()
                        .map_or(0, |m| tex(m, TEX_ILLUMINATION)),
                    vtx: (h.vertex_buffer)(o),
                    mtx: field(h, o, |r| r.model as usize),
                })
                .collect();
        }

        let mut order: [Vec<u32>; 5] = Default::default();
        let sizes = [
            (z, solid_objs.len()),
            (diffuse, solid_objs.len()),
            (trans, trans_objs.len()),
            (decal, decal_objs.len()),
            (illum, illum_objs.len()),
        ];
        // eRenderListType: Z, Diffuse, Translucent, Decal, Illumination.
        for (o, (on, n)) in order.iter_mut().zip(sizes) {
            if on {
                o.resize(n, 0);
            }
        }
        let origin = if trans {
            // SAFETY: set by `Setup`.
            unsafe { self.frustum.as_ref() }.map_or(Vector3f::default(), |f| f.origin)
        } else {
            Vector3f::default()
        };
        let [oz, od, ot, odec, oi] = &mut order;
        list::compile(
            CompileIn {
                flags,
                origin,
                solid: &self.solid,
                trans: &mut self.trans,
                decal: &self.decal,
                illum: &self.illum,
            },
            CompileOut {
                z: oz,
                diffuse: od,
                trans: ot,
                decal: odec,
                illum: oi,
            },
        );

        if trans {
            for (k, &o) in self.trans.iter().zip(&trans_objs) {
                // SAFETY: by the caller's contract.
                unsafe { set(h, o, |r| r.placement = k.placement) };
            }
        }
        let sources = [
            &solid_objs,
            &solid_objs,
            &trans_objs,
            &decal_objs,
            &illum_objs,
        ];
        for (i, (on, _)) in sizes.into_iter().enumerate() {
            if on && let Some(PtrVec(sorted)) = self.lists.get_mut(L_SORTED + i) {
                sorted.clear();
                sorted.extend(order.get(i).into_iter().flatten().map(|&j| {
                    sources
                        .get(i)
                        .and_then(|s| s.get(j as usize))
                        .map_or(0, |&o| o as usize)
                }));
            }
        }
    }
}

fn prog(m: &MatState, mode: usize) -> usize {
    m.programs
        .first()
        .and_then(|p| p.get(mode))
        .map_or(0, |&p| p as usize)
}

fn tex(m: &MatState, i: usize) -> usize {
    m.textures.get(i).map_or(0, |&t| t as usize)
}

fn unit(m: &MatState, u: usize) -> usize {
    m.in_unit
        .get(MODE_DIFFUSE)
        .and_then(|r| r.get(u))
        .map_or(0, |&t| t as usize)
}

/// # Safety
/// `o` is a live renderable.
unsafe fn field<T: Default>(h: &Host, o: O, f: impl FnOnce(&Rnd) -> T) -> T {
    // SAFETY: by the caller's contract; the reference ends with the call.
    unsafe { (h.rnd)(o).as_ref() }.map_or_else(T::default, f)
}

/// # Safety
/// `o` is a live renderable.
unsafe fn set(h: &Host, o: O, f: impl FnOnce(&mut Rnd)) {
    // SAFETY: by the caller's contract; the reference ends with the call.
    if let Some(r) = unsafe { (h.rnd)(o).as_mut() } {
        f(r);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rl_new() -> *mut RenderList {
    Box::into_raw(Box::new(RenderList::new()))
}

/// # Safety
/// `rl` came from `ohpl_rl_new` and is freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rl_free(rl: *mut RenderList) {
    if !rl.is_null() {
        // SAFETY: by the caller's contract.
        drop(unsafe { Box::from_raw(rl) });
    }
}

/// # Safety
/// `rl` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rl_setup(rl: *mut RenderList, ft: f32, cfr: O) {
    // SAFETY: by the caller's contract.
    if let Some(rl) = unsafe { rl.as_mut() } {
        rl.setup(ft, cfr, cfr.cast());
    }
}

/// # Safety
/// `rl` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rl_clear(rl: *mut RenderList) {
    // SAFETY: by the caller's contract.
    if let Some(rl) = unsafe { rl.as_mut() } {
        rl.clear();
    }
}

/// # Safety
/// `rl` and `h` are live; the host callbacks hold no reference into `rl`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rl_add(rl: *mut RenderList, h: *const Host, o: O) {
    // SAFETY: by the caller's contract.
    unsafe {
        if let (Some(rl), Some(h)) = (rl.as_mut(), h.as_ref()) {
            rl.add_object(h, o);
        }
    }
}

/// # Safety
/// As `ohpl_rl_add`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rl_compile(rl: *mut RenderList, h: *const Host, flags: u32) {
    // SAFETY: by the caller's contract.
    unsafe {
        if let (Some(rl), Some(h)) = (rl.as_mut(), h.as_ref()) {
            rl.compile(h, flags);
        }
    }
}

/// The `PtrVec` of list `i` (`L_*`), stable for the life of `rl`.
///
/// # Safety
/// `rl` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rl_list(rl: *mut RenderList, i: usize) -> *mut PtrVec {
    // SAFETY: by the caller's contract.
    unsafe { rl.as_mut() }
        .and_then(|rl| rl.lists.get_mut(i))
        .map_or(std::ptr::null_mut(), std::ptr::from_mut)
}
