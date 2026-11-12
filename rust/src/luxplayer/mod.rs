//! Amnesia TDD/AMFP player core (`LuxPlayer.cpp`, `LuxPlayerHelpers.cpp`, `LuxPlayerHands.cpp`):
//! health/sanity/oil rules, camera fades, head offsets, terror and focus-text timers, the helper
//! state machines (insanity collapse, camera sway, hurt, look-at, flashback, sanity, lantern, light
//! level, darkness, death) and hands animation state. Each step edits a POD copy of the C++ members
//! and returns ordered ops; C++ replays them (sounds, effects, player and body calls, scripts).
//!
//! The player and move states (`LuxPlayerState*.cpp`, `LuxMoveState*.cpp`) take pure kernels instead
//! (`states_*`, `move_normal`): POD in, POD out, with the body, joint, sound and script calls and the
//! state changes left in C++.

mod camera;
mod ffi;
mod hands;
mod helpers;
mod hw;
mod move_normal;
mod ops;
mod sanity;
mod states_base;
mod states_ffi;
mod states_grab;
mod states_ladder;
mod states_rotate;
#[cfg(test)]
mod states_tests;
mod states_vec;
#[cfg(test)]
mod tests;
mod vitals;
