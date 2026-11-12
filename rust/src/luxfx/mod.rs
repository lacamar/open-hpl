//! Amnesia TDD/AMFP effects, insanity, music and log handlers (`LuxEffectHandler.cpp`,
//! `LuxEffectRenderer.cpp`, `LuxPostEffects.cpp`, `LuxInsanityHandler.cpp`, `LuxMusicHandler.cpp`,
//! `LuxProgressLogHandler.cpp`, `LuxCompletionCountHandler.cpp`, `LuxAchievementHandler.cpp`).
//! Each step edits a POD copy of the C++ members and returns ordered ops or a draw plan; C++ keeps
//! object creation, gui/sound/gl calls, scripts and callbacks and replays them.

mod effects;
mod ffi;
mod hw;
mod insanity;
mod misc;
mod music;
mod ops;
mod postfx;
mod renderer;
#[cfg(test)]
mod tests;
mod voice;
