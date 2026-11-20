//! `cMaterial` lifecycle over the engine's `iMaterialType` (stage MM): construction, `SetType`,
//! `Compile`, destruction and the type hooks, through `Host` thunks until the material types
//! and GPU programs are Rust.

use core::sync::atomic::{AtomicBool, Ordering};

use super::h7::{MODES, MatState, UNITS};
use crate::graphics::mm::Host;

type P = *mut core::ffi::c_void;

/// `cMaterial::mbDestroyTypeSpecifics`
pub static DESTROY_TYPE_SPECIFICS: AtomicBool = AtomicBool::new(true);

fn idx(i: usize) -> i32 {
    i32::try_from(i).unwrap_or(0)
}

impl MatState {
    fn host(&self) -> Option<&'static Host> {
        self.host
    }

    /// `cMaterial::SetType`
    pub(crate) fn set_type(&mut self, t: P) {
        let Some(h) = self.host() else {
            return;
        };
        if self.type_ == t {
            return;
        }
        self.type_ = t;
        if !t.is_null() {
            // SAFETY: `t` is a live iMaterialType.
            self.set_translucent(unsafe { (h.type_translucent)(t) });
        }
        if !self.vars.is_null() {
            // SAFETY: the material owns its vars.
            unsafe { (h.vars_delete)(self.vars) };
            self.vars = core::ptr::null_mut();
        }
        if !t.is_null() {
            // SAFETY: `t` is a live iMaterialType.
            self.vars = unsafe { (h.type_create_vars)(t) };
        }
    }

    /// `cMaterial::CompileTextureLookup`
    pub fn compile_texture_lookup(&mut self) {
        let Some(h) = self.host() else {
            return;
        };
        for i in 0..MODES {
            for j in 0..UNITS {
                // SAFETY: type and owner are live engine objects.
                let t =
                    unsafe { (h.type_texture_for_unit)(self.type_, self.owner, idx(i), idx(j)) };
                if let Some(slot) = self.in_unit.get_mut(i).and_then(|r| r.get_mut(j)) {
                    *slot = t;
                }
            }
        }
    }

    /// `cMaterial::Compile`
    pub fn compile(&mut self) {
        let Some(h) = self.host() else {
            return;
        };
        self.has_specific = [false; MODES];
        self.has_object_specific = [false; MODES];
        // SAFETY: plain static read.
        self.compiled_world_fog = unsafe { (h.world_fog)() };
        for i in 0..MODES {
            for j in 0..2 {
                let prev = self.programs.get(j).and_then(|r| r.get(i)).copied();
                // SAFETY: type and owner are live engine objects.
                let prog = unsafe { (h.type_program)(self.type_, self.owner, idx(i), idx(j)) };
                if let Some(slot) = self.programs.get_mut(j).and_then(|r| r.get_mut(i)) {
                    *slot = prog;
                }
                if let Some(p) = prev.filter(|p| !p.is_null()) {
                    // SAFETY: as above; `p` came from `type_program`.
                    unsafe { (h.type_destroy_program)(self.type_, self.owner, idx(i), p, idx(j)) };
                }
            }
        }
        self.compile_texture_lookup();
        // SAFETY: as above.
        unsafe { (h.type_compile_specifics)(self.type_, self.owner) };
    }

    pub fn vars_object(&self) -> P {
        // SAFETY: as above.
        self.host().map_or(core::ptr::null_mut(), |h| unsafe {
            (h.type_vars_object)(self.type_, self.owner)
        })
    }

    pub(crate) fn load_vars(&self, obj: P) {
        if let Some(h) = self.host() {
            // SAFETY: as above.
            unsafe { (h.type_load_vars)(self.type_, self.owner, obj) };
        }
    }
}

impl Drop for MatState {
    fn drop(&mut self) {
        let Some(h) = self.host() else {
            return;
        };
        if !self.vars.is_null() {
            // SAFETY: the material owns its vars.
            unsafe { (h.vars_delete)(self.vars) };
        }
        if DESTROY_TYPE_SPECIFICS.load(Ordering::Relaxed) && !self.type_.is_null() {
            for i in 0..MODES {
                for j in 0..2 {
                    let p = self
                        .programs
                        .get(j)
                        .and_then(|r| r.get(i))
                        .copied()
                        .unwrap_or(core::ptr::null_mut());
                    if !p.is_null() {
                        // SAFETY: type and owner are live while the material is destroyed.
                        unsafe {
                            (h.type_destroy_program)(self.type_, self.owner, idx(i), p, idx(j));
                        }
                    }
                }
            }
        }
        if self.auto_destroy_textures {
            for t in self.textures.iter().copied().filter(|t| !t.is_null()) {
                // SAFETY: the texture manager destroys textures the material holds.
                unsafe { (h.texture_destroy)(self.resources, t) };
            }
        }
    }
}
