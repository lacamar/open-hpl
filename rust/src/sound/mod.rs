//! Sound layer: the owned handler, entries, entity data, sound entities and the music handler.
//! The channels (`iSoundChannel`, OpenAL), resource loading, physics and the game's event
//! objects stay C++ and are reached through the host tables in `channel` and `own`.

mod channel;
pub mod entity_data;
pub mod entry;
pub mod music;
#[allow(unused_unsafe)] // `f!` reads entity fields inside and outside unsafe blocks
pub mod own;
pub mod own_ffi;
#[cfg(test)]
mod own_tests;
pub mod play;
#[cfg(test)]
mod tests;
