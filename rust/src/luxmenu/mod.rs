//! Amnesia main-menu options, key config and profile windows (GA7a): option tables and value
//! conversions, apply/revert decisions, key rebinding and conflict detection, profile list rules.
//! Widgets, engine and config writes stay in C++; re-entrant calls come back through callbacks.
//! `win` owns the start game, load game, profile and custom story windows (A9a).

mod ffi;
pub mod keyconfig;
pub mod options;
pub mod profile;
#[cfg(test)]
mod tests;
pub mod win;
