//! Amnesia main-menu options, key config and profile windows (GA7a, A10a): option tables and
//! value conversions, apply/revert decisions, key rebinding and conflict detection, profile list
//! rules. The options and key config windows are built and driven from Rust over the host ops.

pub mod keyconfig;
pub mod options;
pub mod profile;
#[cfg(test)]
mod tests;

#[cfg(test)]
mod fake;
mod ffi_win;
pub mod host;
#[cfg(not(test))]
mod host_direct;
pub mod keyconfig_win;
pub mod options_win;
pub mod store;
#[cfg(test)]
mod tests_win;
pub mod win;

pub mod mainmenu;
