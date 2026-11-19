//! `cSubMeshEntity` state and logic (WM2). C++ keeps the vtable and the objects it still owns
//! (vertex buffer, material, mesh entity) behind the `Host` table.

use std::ffi::c_void;
use std::ptr;

use super::h11;
use crate::types::Matrixf;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Sub>() == 48 && offset_of!(Sub, user_data) == 16);
    assert!(size_of::<GateIn>() == 8 && size_of::<SkinIn>() == 88);
};

#[repr(C)]
pub struct Sub {
    pub graphics_updated: bool,
    pub update_body: bool,
    pub skinned_version: i32,
    pub static_null_count: i32,
    pub user_data: *mut c_void,
    pub material: *mut c_void,
    pub local_node: *mut c_void,
    pub dyn_vb: *mut c_void,
}

#[repr(C)]
pub struct SkinIn {
    pub bind_pos: *const f32,
    pub bind_normal: *const f32,
    pub bind_tangent: *const f32,
    pub skin_pos: *mut f32,
    pub skin_normal: *mut f32,
    pub skin_tangent: *mut f32,
    pub weights: *const f32,
    pub bones: *const u8,
    pub matrices: *const Matrixf,
    pub vertex_count: i32,
    pub pos_stride: i32,
    pub matrix_count: i32,
}

#[repr(C)]
pub struct GateIn {
    pub mesh_version: i32,
    pub sleeping: bool,
}

/// C++ side of one sub mesh entity (`ctx` is the `cSubMeshEntity`).
#[repr(C)]
pub struct Host {
    pub ctx: *mut c_void,
    pub has_skeleton: unsafe extern "C" fn(*mut c_void) -> bool,
    pub copy_vb: unsafe extern "C" fn(*mut c_void) -> *mut c_void,
    pub delete_vb: unsafe extern "C" fn(*mut c_void),
    pub destroy_material: unsafe extern "C" fn(*mut c_void, *mut c_void),
    pub update_mesh: unsafe extern "C" fn(*mut c_void, f32),
    pub skin_gate: unsafe extern "C" fn(*mut c_void, *mut GateIn) -> bool,
    pub skin_in: unsafe extern "C" fn(*mut c_void, *mut SkinIn),
    pub upload_vb: unsafe extern "C" fn(*mut c_void),
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_wm2_sub_new() -> *mut Sub {
    Box::into_raw(Box::new(Sub {
        graphics_updated: false,
        update_body: false,
        skinned_version: -1,
        static_null_count: 0,
        user_data: ptr::null_mut(),
        material: ptr::null_mut(),
        local_node: ptr::null_mut(),
        dyn_vb: ptr::null_mut(),
    }))
}

/// # Safety
/// `s` from `ohpl_wm2_sub_new`, `h` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wm2_sub_init(s: *mut Sub, h: *const Host) {
    // SAFETY: caller contract.
    let (s, h) = unsafe { (&mut *s, &*h) };
    // SAFETY: host table valid for the call.
    unsafe {
        if (h.has_skeleton)(h.ctx) {
            s.dyn_vb = (h.copy_vb)(h.ctx);
        }
    }
}

/// # Safety
/// `s` from `ohpl_wm2_sub_new` (freed here), `h` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wm2_sub_free(s: *mut Sub, h: *const Host) {
    // SAFETY: caller contract.
    let (s, h) = unsafe { (Box::from_raw(s), &*h) };
    // SAFETY: host table valid for the call.
    unsafe {
        if !s.dyn_vb.is_null() {
            (h.delete_vb)(s.dyn_vb);
        }
        if !s.material.is_null() {
            (h.destroy_material)(h.ctx, s.material);
        }
    }
}

/// # Safety
/// `s`, `h` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wm2_sub_set_material(
    s: *mut Sub,
    h: *const Host,
    mat: *mut c_void,
    destroy_old: bool,
) {
    // SAFETY: caller contract.
    let (s, h) = unsafe { (&mut *s, &*h) };
    if destroy_old && !s.material.is_null() {
        // SAFETY: host table valid for the call.
        unsafe { (h.destroy_material)(h.ctx, s.material) };
    }
    s.material = mat;
}

/// # Safety
/// `s`, `h` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wm2_sub_update_gfx(s: *mut Sub, h: *const Host, frame_time: f32) {
    // SAFETY: caller contract.
    let (s, h) = unsafe { (&mut *s, &*h) };
    // SAFETY: host table valid for the call; the host never touches `s` during these calls.
    unsafe {
        (h.update_mesh)(h.ctx, frame_time);
        if s.dyn_vb.is_null() {
            return;
        }
        let mut g = GateIn {
            mesh_version: 0,
            sleeping: false,
        };
        if !(h.skin_gate)(h.ctx, &mut g) {
            return;
        }
        if !h11::sub_skin_gate(
            &mut s.graphics_updated,
            &mut s.skinned_version,
            g.mesh_version,
            g.sleeping,
        ) {
            return;
        }
        let mut i = SkinIn {
            bind_pos: ptr::null(),
            bind_normal: ptr::null(),
            bind_tangent: ptr::null(),
            skin_pos: ptr::null_mut(),
            skin_normal: ptr::null_mut(),
            skin_tangent: ptr::null_mut(),
            weights: ptr::null(),
            bones: ptr::null(),
            matrices: ptr::null(),
            vertex_count: 0,
            pos_stride: 0,
            matrix_count: 0,
        };
        (h.skin_in)(h.ctx, &mut i);
        super::ffi_meshentity::ohpl_submesh_skin(
            i.bind_pos,
            i.bind_normal,
            i.bind_tangent,
            i.skin_pos,
            i.skin_normal,
            i.skin_tangent,
            i.weights,
            i.bones,
            i.matrices,
            i.vertex_count,
            i.pos_stride,
            i.matrix_count,
        );
        (h.upload_vb)(h.ctx);
    }
}

/// `GetModelMatrix`: true when the caller gets no matrix.
///
/// # Safety
/// `s`, `world` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wm2_sub_model_null(
    s: *mut Sub,
    is_static: bool,
    world: *const Matrixf,
) -> bool {
    // SAFETY: caller contract.
    let (s, w) = unsafe { (&mut *s, &*world) };
    is_static && s.static_null_count >= 0 && h11::static_null_matrix(&mut s.static_null_count, w)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicI32, Ordering};

    static LOG: AtomicI32 = AtomicI32::new(0);

    unsafe extern "C" fn has_skel(_: *mut c_void) -> bool {
        true
    }
    unsafe extern "C" fn copy(_: *mut c_void) -> *mut c_void {
        8 as *mut c_void
    }
    unsafe extern "C" fn del(_: *mut c_void) {
        LOG.fetch_add(100, Ordering::SeqCst);
    }
    unsafe extern "C" fn destroy_mat(_: *mut c_void, _: *mut c_void) {
        LOG.fetch_add(10, Ordering::SeqCst);
    }
    unsafe extern "C" fn upd(_: *mut c_void, _: f32) {
        LOG.fetch_add(1, Ordering::SeqCst);
    }
    unsafe extern "C" fn gate(_: *mut c_void, g: *mut GateIn) -> bool {
        // SAFETY: test call passes a valid pointer.
        unsafe { (*g).mesh_version = 3 };
        false
    }
    unsafe extern "C" fn skin_in(_: *mut c_void, _: *mut SkinIn) {}
    unsafe extern "C" fn up(_: *mut c_void) {}

    #[test]
    fn lifecycle() {
        let h = Host {
            ctx: ptr::null_mut(),
            has_skeleton: has_skel,
            copy_vb: copy,
            delete_vb: del,
            destroy_material: destroy_mat,
            update_mesh: upd,
            skin_gate: gate,
            skin_in,
            upload_vb: up,
        };
        // SAFETY: pointers come from `ohpl_wm2_sub_new` and the local host table.
        unsafe {
            let s = ohpl_wm2_sub_new();
            ohpl_wm2_sub_init(s, &h);
            assert!(!(*s).dyn_vb.is_null());
            ohpl_wm2_sub_update_gfx(s, &h, 0.1);
            assert!(!(*s).graphics_updated);
            ohpl_wm2_sub_set_material(s, &h, 4 as *mut c_void, true);
            ohpl_wm2_sub_set_material(s, &h, 5 as *mut c_void, true);
            ohpl_wm2_sub_free(s, &h);
        }
        assert_eq!(LOG.load(Ordering::SeqCst), 1 + 10 + 10 + 100);
    }

    #[test]
    fn model_null() {
        let s = ohpl_wm2_sub_new();
        let id = crate::math::IDENTITY;
        // SAFETY: valid pointers.
        unsafe {
            assert!(!ohpl_wm2_sub_model_null(s, false, &id));
            assert!(ohpl_wm2_sub_model_null(s, true, &id));
            assert_eq!((*s).static_null_count, 1);
            ohpl_wm2_sub_free(
                s,
                &Host {
                    ctx: ptr::null_mut(),
                    has_skeleton: has_skel,
                    copy_vb: copy,
                    delete_vb: del,
                    destroy_material: destroy_mat,
                    update_mesh: upd,
                    skin_gate: gate,
                    skin_in,
                    upload_vb: up,
                },
            );
        }
    }
}
