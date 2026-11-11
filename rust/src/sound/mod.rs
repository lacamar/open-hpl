//! Sound layer: `cSoundEntry`, the `cSoundHandler::Play` set-up, `cMusicHandler` and the `.snt`
//! sound entity files. The channels (`iSoundChannel`, OpenAL), resource loading, physics and the
//! game's music entries stay C++ and are reached through the function tables in `channel` and `ffi`.

mod channel;
pub mod entity_data;
pub mod entry;
mod ffi;
pub mod music;
pub mod play;
#[cfg(test)]
mod tests;
