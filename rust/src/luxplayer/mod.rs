//! Amnesia TDD/AMFP player core (`LuxPlayer.cpp`, `LuxPlayerHelpers.cpp`, `LuxPlayerHands.cpp`):
//! health/sanity/oil rules, camera fades, head offsets, terror and focus-text timers, the helper
//! state machines (insanity collapse, camera sway, hurt, look-at, flashback, sanity, lantern, light
//! level, darkness, death) and hands animation state. Each step edits a POD copy of the C++ members
//! and returns ordered ops; C++ replays them (sounds, effects, player and body calls, scripts).

mod camera;
mod ffi;
mod hands;
mod helpers;
mod hw;
mod ops;
mod sanity;
#[cfg(test)]
mod tests;
mod vitals;
