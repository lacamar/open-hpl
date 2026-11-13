//! S3: script object-id registry (`SomaObjectID`/`SomaObjectFromID`) and the per-body liquid plan.

use super::geom::liquid_body_under;
use std::collections::BTreeMap;
use std::ffi::c_void;

const FOREIGN: u8 = 0xfe;
const JOINT: &[u8] = b"iPhysicsJoint";

#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq, Debug)]
pub struct OhplS3Id {
    pub a: u8,
    pub b: i32,
    pub c: i32,
}

#[derive(Clone, Default)]
struct Entry {
    obj: usize,
    ty: Vec<u8>,
    creation: i32,
}

/// Object queries only the C++ side can answer.
pub trait IdHost {
    fn alive(&mut self, obj: usize, ty: &[u8], creation: i32) -> bool;
    fn creation_id(&mut self, obj: usize) -> i32;
    /// `CastEntity3D(obj, ty)`, as a pointer (0 when not that class).
    fn cast_entity(&mut self, obj: usize, ty: &[u8]) -> usize;
    fn cast_joint(&mut self, obj: usize, ty: &[u8]) -> usize;
}

fn is_entity3d(ty: &[u8]) -> bool {
    !ty.starts_with(JOINT) && ty != b"iCharacterBody"
}

pub struct Ids {
    by_obj: BTreeMap<usize, OhplS3Id>,
    by_id: BTreeMap<i32, Entry>,
    next: i32,
}

impl Ids {
    pub fn new(seed: i32) -> Self {
        Self {
            by_obj: BTreeMap::new(),
            by_id: BTreeMap::new(),
            next: seed,
        }
    }

    pub fn clear(&mut self) {
        self.by_obj.clear();
        self.by_id.clear();
    }

    pub fn register_bodies(&mut self, bodies: &[usize]) {
        let mut l = 0i32;
        for &b in bodies {
            l = l.wrapping_sub(1);
            let id = OhplS3Id {
                a: FOREIGN,
                b: l,
                c: 0,
            };
            self.by_obj.insert(b, id);
            self.by_id.insert(
                id.b,
                Entry {
                    obj: b,
                    ty: b"iPhysicsBody".to_vec(),
                    creation: 0,
                },
            );
        }
    }

    pub fn object_id(&mut self, h: &mut dyn IdHost, obj: usize, ty: &[u8]) -> OhplS3Id {
        if obj == 0 {
            return OhplS3Id::default();
        }
        if let Some(&id) = self.by_obj.get(&obj) {
            // operator[] on a missing key inserts a default entry
            let e = self.by_id.entry(id.b).or_default().clone();
            let e3d = is_entity3d(ty);
            if h.alive(e.obj, &e.ty, e.creation)
                && is_entity3d(&e.ty) == e3d
                && (e.ty == ty || !e3d || h.cast_entity(obj, &e.ty) != 0)
            {
                return id;
            }
            self.by_id.remove(&id.b);
        }
        let id = OhplS3Id {
            a: FOREIGN,
            b: self.next,
            c: 0,
        };
        self.next = self.next.wrapping_add(1);
        let creation = if ty == b"cSoundEntity" {
            h.creation_id(obj)
        } else {
            0
        };
        self.by_obj.insert(obj, id);
        self.by_id.insert(
            id.b,
            Entry {
                obj,
                ty: ty.to_vec(),
                creation,
            },
        );
        id
    }

    pub fn object_from_id(&mut self, h: &mut dyn IdHost, id: OhplS3Id, ty: &[u8]) -> usize {
        if id.a != FOREIGN {
            return 0;
        }
        let Some(e) = self.by_id.get(&id.b).cloned() else {
            return 0;
        };
        if !h.alive(e.obj, &e.ty, e.creation) {
            return 0;
        }
        if e.ty == ty {
            return e.obj;
        }
        if is_entity3d(&e.ty) && is_entity3d(ty) {
            if e.ty != b"iEntity3D" && e.ty != b"iLight" {
                let ok = ty == b"iEntity3D" || (ty == b"iLight" && e.ty.starts_with(b"cLight"));
                return if ok { e.obj } else { 0 };
            }
            return h.cast_entity(e.obj, ty);
        }
        if ty == JOINT && e.ty.starts_with(JOINT) {
            return e.obj;
        }
        if ty.starts_with(JOINT) && e.ty == JOINT {
            return h.cast_joint(e.obj, ty);
        }
        0
    }
}

// ---- liquid body plan (`LiquidBuoyancy` per-body decisions) ----

pub const NONE: i8 = -1;

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct OhplS3LiquidBodyIn {
    pub collide: bool,
    pub active: bool,
    pub has_mass: bool,
    pub is_character: bool,
    pub inside: bool,
    pub no_gravity_underwater: bool,
    pub buoyancy_active: bool,
    pub owner_interacted: bool,
    pub waves_near: bool,
    pub surface_moved: bool,
    pub body_max_y: f32,
    pub center_y: f32,
    pub ext_y: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct OhplS3LiquidBodyPlan {
    pub skip: bool,
    pub exit_callback: bool,
    pub buoyancy_off: bool,
    pub init_buoyancy: bool,
    pub wave_surface: bool,
    pub reset_surface: bool,
    /// -1 leave, 0/1 set.
    pub underwater: i8,
    pub gravity: i8,
    /// 0 leaves the speed caps.
    pub max_speed: f32,
}

pub fn liquid_body_plan(i: &OhplS3LiquidBodyIn) -> OhplS3LiquidBodyPlan {
    let mut p = OhplS3LiquidBodyPlan {
        skip: false,
        exit_callback: false,
        buoyancy_off: false,
        init_buoyancy: false,
        wave_surface: false,
        reset_surface: false,
        underwater: NONE,
        gravity: NONE,
        max_speed: 0.0,
    };
    if !i.collide || !i.active || !i.has_mass || i.is_character {
        p.skip = true;
        return p;
    }
    if !i.inside {
        p.exit_callback = i.buoyancy_active;
        p.buoyancy_off = true;
        if i.no_gravity_underwater {
            p.underwater = 0;
            if !i.owner_interacted {
                p.gravity = 1;
            }
            p.max_speed = 20.0;
        }
        return p;
    }
    if i.no_gravity_underwater {
        let under = liquid_body_under(i.body_max_y, i.center_y, i.ext_y);
        if !i.owner_interacted {
            p.gravity = i8::from(!under);
        }
        p.underwater = i8::from(under);
        p.max_speed = if under { 1.0 } else { 20.0 };
    }
    p.init_buoyancy = !i.buoyancy_active;
    if i.waves_near {
        p.wave_surface = true;
    } else {
        p.reset_surface = i.surface_moved;
    }
    p
}

// ---- C ABI (mirrored in soma/src/game/RustSomaEntS3.h) ----

#[repr(C)]
pub struct OhplS3IdHost {
    ctx: *mut c_void,
    alive: unsafe extern "C" fn(*mut c_void, usize, *const u8, usize, i32) -> bool,
    creation_id: unsafe extern "C" fn(*mut c_void, usize) -> i32,
    cast_entity: unsafe extern "C" fn(*mut c_void, usize, *const u8, usize) -> usize,
    cast_joint: unsafe extern "C" fn(*mut c_void, usize, *const u8, usize) -> usize,
}

impl IdHost for OhplS3IdHost {
    fn alive(&mut self, obj: usize, ty: &[u8], creation: i32) -> bool {
        // SAFETY: host table supplied by the C++ caller.
        unsafe { (self.alive)(self.ctx, obj, ty.as_ptr(), ty.len(), creation) }
    }
    fn creation_id(&mut self, obj: usize) -> i32 {
        // SAFETY: host table supplied by the C++ caller.
        unsafe { (self.creation_id)(self.ctx, obj) }
    }
    fn cast_entity(&mut self, obj: usize, ty: &[u8]) -> usize {
        // SAFETY: host table supplied by the C++ caller.
        unsafe { (self.cast_entity)(self.ctx, obj, ty.as_ptr(), ty.len()) }
    }
    fn cast_joint(&mut self, obj: usize, ty: &[u8]) -> usize {
        // SAFETY: host table supplied by the C++ caller.
        unsafe { (self.cast_joint)(self.ctx, obj, ty.as_ptr(), ty.len()) }
    }
}

unsafe fn ty<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() {
        return &[];
    }
    // SAFETY: caller contract.
    unsafe { std::slice::from_raw_parts(p, n) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s3_ids_new(seed: i32) -> *mut Ids {
    Box::into_raw(Box::new(Ids::new(seed)))
}

/// # Safety
/// `ids` from `ohpl_s3_ids_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s3_ids_free(ids: *mut Ids) {
    if !ids.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(ids) });
    }
}

/// # Safety
/// `ids` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s3_ids_clear(ids: *mut Ids) {
    // SAFETY: caller contract.
    if let Some(i) = unsafe { ids.as_mut() } {
        i.clear();
    }
}

/// # Safety
/// `ids` valid; `bodies` valid for `n`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s3_ids_register_bodies(
    ids: *mut Ids,
    bodies: *const usize,
    n: usize,
) {
    // SAFETY: caller contract.
    if let Some(i) = unsafe { ids.as_mut() } {
        let b = if bodies.is_null() {
            &[][..]
        } else {
            // SAFETY: caller contract.
            unsafe { std::slice::from_raw_parts(bodies, n) }
        };
        i.register_bodies(b);
    }
}

/// # Safety
/// All pointers valid; `host` callbacks must not re-enter the registry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s3_object_id(
    ids: *mut Ids,
    host: *mut OhplS3IdHost,
    obj: usize,
    ty_p: *const u8,
    ty_n: usize,
    out: *mut OhplS3Id,
) {
    // SAFETY: caller contract.
    let (Some(i), Some(h), Some(o)) = (unsafe { ids.as_mut() }, unsafe { host.as_mut() }, unsafe {
        out.as_mut()
    }) else {
        return;
    };
    // SAFETY: caller contract.
    *o = i.object_id(h, obj, unsafe { ty(ty_p, ty_n) });
}

/// # Safety
/// All pointers valid; `host` callbacks must not re-enter the registry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s3_object_from_id(
    ids: *mut Ids,
    host: *mut OhplS3IdHost,
    id: *const OhplS3Id,
    ty_p: *const u8,
    ty_n: usize,
) -> usize {
    // SAFETY: caller contract.
    let (Some(i), Some(h), Some(id)) =
        (unsafe { ids.as_mut() }, unsafe { host.as_mut() }, unsafe {
            id.as_ref()
        })
    else {
        return 0;
    };
    // SAFETY: caller contract.
    i.object_from_id(h, *id, unsafe { ty(ty_p, ty_n) })
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s3_liquid_body_plan(
    i: *const OhplS3LiquidBodyIn,
    out: *mut OhplS3LiquidBodyPlan,
) {
    // SAFETY: caller contract.
    if let (Some(i), Some(o)) = (unsafe { i.as_ref() }, unsafe { out.as_mut() }) {
        *o = liquid_body_plan(i);
    }
}
