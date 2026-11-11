//! SOMA/Rebirth/Bunker sound game layer: FMOD Designer event emulation (`SomaSound.cpp`), voice
//! scenes (`SomaLuxVoice.cpp`), soundscapes (`SomaSoundscape.cpp`) and the FSB glue
//! (`SomaFsb.cpp`). OpenAL, the sound handler and the script registrations stay C++ and are
//! reached through the host tables in `ffi`.

mod dump;
pub mod events;
mod ffi;
mod ffi_scape;
mod ffi_voice;
pub mod fsb;
pub mod instance;
mod presets;
pub mod scape;
#[cfg(test)]
mod tests;
pub mod voice;

unsafe extern "C" {
    #[link_name = "powf"]
    fn c_powf(x: f32, y: f32) -> f32;
    #[link_name = "fmodf"]
    fn c_fmodf(x: f32, y: f32) -> f32;
}

/// libm `powf` as gcc calls it (`std::pow(float, float)`), never folded to `exp2`/`exp10`.
pub(crate) fn powf(x: f32, y: f32) -> f32 {
    // SAFETY: pure libm function on plain floats.
    unsafe { c_powf(x, y) }
}

pub(crate) fn fmodf(x: f32, y: f32) -> f32 {
    // SAFETY: pure libm function on plain floats.
    unsafe { c_fmodf(x, y) }
}

/// `std::min(a, b)`: `b < a ? b : a`.
pub(crate) fn std_min(a: f32, b: f32) -> f32 {
    if b < a { b } else { a }
}

/// `std::max(a, b)`: `a < b ? b : a`.
pub(crate) fn std_max(a: f32, b: f32) -> f32 {
    if a < b { b } else { a }
}
