use std::ffi::c_void;

pub type NameFn = unsafe extern "C" fn(*const c_void, *mut usize) -> *const u8;
pub type IdFn = unsafe extern "C" fn(*const c_void) -> i32;
pub type DropFn = unsafe extern "C" fn(*mut c_void);

/// Thunks for the virtuals of a C++ object Rust holds only by address (the "shell" form: the
/// C++ class keeps its vtable, Rust owns the container and the lifecycle order). One per kind,
/// registered once; a missing thunk means the kind has no such virtual.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Foreign {
    pub name: Option<NameFn>,
    pub id: Option<IdFn>,
    pub drop: Option<DropFn>,
}

impl Foreign {
    /// # Safety
    /// `p` is a live object of the kind this vtable was registered for.
    pub unsafe fn name_of<'a>(&self, p: usize) -> &'a [u8] {
        let Some(f) = self.name else { return &[] };
        let mut len = 0usize;
        // SAFETY: caller contract; the name string outlives the lookup and is not mutated by it.
        unsafe {
            let ptr = f(p as *const c_void, &mut len);
            if ptr.is_null() || len == 0 {
                &[]
            } else {
                std::slice::from_raw_parts(ptr, len)
            }
        }
    }

    /// # Safety
    /// As `name_of`.
    pub unsafe fn id_of(&self, p: usize) -> i32 {
        match self.id {
            // SAFETY: caller contract.
            Some(f) => unsafe { f(p as *const c_void) },
            None => -1,
        }
    }

    /// Runs the C++ destructor. May re-enter the registry: callers hold no borrow of it.
    ///
    /// # Safety
    /// `p` is a live object of the registered kind, not used afterwards.
    pub unsafe fn destroy(&self, p: usize) {
        if let Some(f) = self.drop {
            // SAFETY: caller contract.
            unsafe { f(p as *mut c_void) }
        }
    }
}
