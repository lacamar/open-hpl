//! SOMA/Rebirth/Bunker sound game layer: FMOD Designer event emulation (`SomaSound.cpp`), voice
//! scenes (`SomaLuxVoice.cpp`), soundscapes (`SomaSoundscape.cpp`) and the FSB glue
//! (`SomaFsb.cpp`). OpenAL, the sound handler and the script registrations stay C++ and are
//! reached through the host tables in `ffi`.

mod dialog;
mod dump;
pub mod events;
mod extract;
mod ffi_fsb;
mod ffi_game;
pub mod fsb;
mod game;
mod glue;
mod host;
pub mod instance;
pub mod music;
mod natives;
mod presets;
pub mod scape;
mod soundscape;
#[cfg(test)]
mod tests;
pub mod voice;
mod voice_handler;
mod vorbis_setups;

pub(crate) use crate::math::{fmodf, powf, std_max, std_min};
