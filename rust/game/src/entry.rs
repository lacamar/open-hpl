use core::ffi::{c_char, c_int};

unsafe extern "C" {
    fn OhplGameMain(cmd: *const u8, len: usize) -> c_int;
}

/// # Safety
/// Called by the C runtime with a valid `argv`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn main(argc: c_int, argv: *const *const c_char) -> c_int {
    // SAFETY: C runtime contract; `OhplGameMain` is `hplMain` of the game library.
    unsafe { ohpl::backend::sdl::game_main(argc, argv, OhplGameMain) }
}
