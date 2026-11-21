//! Defines C++ member functions in Rust under their Itanium symbols (aarch64: `this` in x0, then the arguments).
//! `$get` maps `this` to the Rust state the method works on; member offsets are asserted by the C++
//! translation unit that owns the constructor.

/// libstdc++ `std::string` (cxx11 ABI): data pointer, then length.
#[repr(C)]
pub struct CxxStr {
    p: *const u8,
    n: usize,
}

impl CxxStr {
    /// # Safety
    /// `this` points to a live `std::string`.
    pub unsafe fn bytes<'a>(this: *const CxxStr) -> &'a [u8] {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts((*this).p, (*this).n) }
    }
}

/// # Safety
/// The member at `off` is a pointer to a live `T`.
pub unsafe fn ptr_member<'a, T>(this: *const u8, off: usize) -> &'a mut T {
    // SAFETY: caller contract.
    unsafe { &mut **this.add(off).cast::<*mut T>() }
}

/// # Safety
/// The member at `off` is a `T`.
pub unsafe fn val_member<T: Copy>(this: *const u8, off: usize) -> T {
    // SAFETY: caller contract.
    unsafe { *this.add(off).cast::<T>() }
}

macro_rules! cxx {
    ($get:expr; $($sym:literal $name:ident[$this:ident, $s:ident]($($a:ident: $t:ty),*) $(-> $r:ty)? => $e:expr;)*) => {$(
        #[unsafe(export_name = $sym)]
        unsafe extern "C" fn $name($this: *const u8 $(, $a: $t)*) $(-> $r)? {
            // SAFETY: C++ calls with a live object whose state members are set by its constructor.
            #[allow(unused_variables, unused_mut)]
            let $s = unsafe { $get($this) };
            #[allow(unused_unsafe)]
            unsafe { $e }
        }
    )*};
}

pub(crate) use cxx;
