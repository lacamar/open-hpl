//! SOMA Lux game layer (`SomaLux*.cpp`): map change and transfer decisions, preload and pause
//! bookkeeping, player camera/movement/health math and the scriptable timers and faders. Object
//! creation, world loading, physics/character-body calls and script callbacks stay C++, which
//! replays the plans returned here in order.

mod ffi;
pub mod game;
pub mod pause;
pub mod player;
pub mod scriptable;
#[cfg(test)]
mod tests;
pub mod transfer;

pub mod s5_input;
mod s5_input_ffi;
pub mod s5_scriptable;
#[cfg(test)]
mod s5_tests;

pub mod s5c_game;
