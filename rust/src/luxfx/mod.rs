//! Amnesia TDD/AMFP effects, insanity, music and log handlers (`LuxEffectHandler.cpp`,
//! `LuxEffectRenderer.cpp`, `LuxPostEffects.cpp`, `LuxInsanityHandler.cpp`, `LuxMusicHandler.cpp`,
//! `LuxProgressLogHandler.cpp`, `LuxCompletionCountHandler.cpp`, `LuxAchievementHandler.cpp`).
//! Each step edits a POD copy of the C++ members and returns ordered ops or a draw plan; C++ keeps
//! object creation, gui/sound/gl calls, scripts and callbacks and replays them.

mod effects;
mod ffi;
mod hw;
mod insanity;
pub(crate) mod misc;
mod music;
mod ops;
mod postfx;
mod renderer;
#[cfg(test)]
mod tests;
mod voice;

pub(crate) mod ffi12;
pub(crate) mod fxffi;
mod fxobj;
pub mod host;
pub(crate) mod host12;
mod host12_direct;
mod insanity_h;
mod music_h;
mod rend;
#[cfg(test)]
mod tests12;
#[cfg(test)]
mod tests_a12a;
