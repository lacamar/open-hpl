//! Amnesia in-game UI logic (GA8): inventory, journal, item types, hints, messages, commentary
//! icons and the serial key field. Widgets, draws, fonts, sound and script calls stay in C++;
//! every call that can re-enter the game comes back as an ordered [`event::Ev`].

pub mod commentary;
pub mod event;
mod ffi;
pub mod hint;
pub mod inventory;
pub mod itemtype;
pub mod journal;
pub mod keyentry;
pub mod message;
#[cfg(test)]
mod tests;
