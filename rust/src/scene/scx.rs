//! Defines C++ member functions in Rust under their Itanium symbols (aarch64: `this` in x0).
//! `$get` maps `this` to the Rust view of the object.

macro_rules! scx {
    ($get:path; $($sym:literal $name:ident[$this:ident, $s:ident]($($a:ident: $t:ty),*) $(-> $r:ty)? => $e:expr;)*) => {$(
        #[unsafe(export_name = $sym)]
        unsafe extern "C" fn $name($this: *const u8 $(, $a: $t)*) $(-> $r)? {
            // SAFETY: C++ calls with a live object whose constructor has run.
            #[allow(unused_variables, unused_mut)]
            let mut $s = unsafe { $get($this) };
            #[allow(unused_unsafe)]
            unsafe { $e }
        }
    )*};
}

pub(crate) use scx;
