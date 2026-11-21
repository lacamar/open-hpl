//! `cMaterial` lifecycle over the material types (stage MM, types from stage MT): construction,
//! `SetType`, `Compile` and destruction.

use core::sync::atomic::{AtomicBool, Ordering};

use super::h7::{MODES, MatState, UNITS};
use super::mt::Mt;
use crate::graphics::mm::Host;

type P = *mut core::ffi::c_void;

/// `cMaterial::mbDestroyTypeSpecifics`
pub static DESTROY_TYPE_SPECIFICS: AtomicBool = AtomicBool::new(true);

fn idx(i: usize) -> i32 {
    i32::try_from(i).unwrap_or(0)
}

/// # Safety
/// `t` is null or a live `Mt`.
unsafe fn ty<'a>(t: P) -> Option<&'a Mt> {
    // SAFETY: caller contract.
    unsafe { t.cast::<Mt>().as_ref() }
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
        // SAFETY: `t` is null or a live type.
        let ty = unsafe { ty(t) };
        if let Some(ty) = ty {
            self.set_translucent(ty.is_translucent());
        }
        if !self.vars.is_null() {
            // SAFETY: the material owns its vars.
            unsafe { (h.vars_delete)(self.vars) };
            self.vars = core::ptr::null_mut();
        }
        if let Some(ty) = ty {
            self.vars = ty.create_vars();
        }
    }

    /// `cMaterial::CompileTextureLookup`
    pub fn compile_texture_lookup(&mut self) {
        // SAFETY: the type is live while the material is.
        let Some(ty) = (unsafe { ty(self.type_) }) else {
            return;
        };
        for i in 0..MODES {
            for j in 0..UNITS {
                let t = ty.texture_for_unit(self, idx(i), idx(j));
                if let Some(slot) = self.in_unit.get_mut(i).and_then(|r| r.get_mut(j)) {
                    *slot = t;
                }
            }
        }
    }

    /// `cMaterial::Compile`
    pub fn compile(&mut self) {
        // SAFETY: the type is live while the material is.
        let Some(ty) = (unsafe { ty(self.type_) }) else {
            return;
        };
        self.has_specific = [false; MODES];
        self.has_object_specific = [false; MODES];
        self.compiled_world_fog = super::mt::world_fog();
        for i in 0..MODES {
            for j in 0..2 {
                let prev = self.programs.get(j).and_then(|r| r.get(i)).copied();
                let prog = ty.gpu_program(self, idx(i), idx(j));
                if let Some(slot) = self.programs.get_mut(j).and_then(|r| r.get_mut(i)) {
                    *slot = prog;
                }
                if let Some(p) = prev.filter(|p| !p.is_null()) {
                    ty.destroy_program(self.blend, idx(i), p);
                }
            }
        }
        self.compile_texture_lookup();
        ty.compile_specifics(self);
    }

    pub fn vars_object(&self) -> P {
        // SAFETY: the type is live while the material is.
        unsafe { ty(self.type_) }.map_or(core::ptr::null_mut(), |t| t.var_values(self))
    }

    pub(crate) fn load_vars(&mut self, obj: P) {
        // SAFETY: the type is live while the material is.
        if let Some(t) = unsafe { ty(self.type_) } {
            t.load_variables(self, obj);
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
        // SAFETY: types outlive materials unless destroy-type-specifics is off.
        if let Some(ty) = unsafe { ty(self.type_) }
            && DESTROY_TYPE_SPECIFICS.load(Ordering::Relaxed)
        {
            for i in 0..MODES {
                for j in 0..2 {
                    let p = self
                        .programs
                        .get(j)
                        .and_then(|r| r.get(i))
                        .copied()
                        .unwrap_or(core::ptr::null_mut());
                    if !p.is_null() {
                        ty.destroy_program(self.blend, idx(i), p);
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
