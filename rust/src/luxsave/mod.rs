//! Amnesia save layer (`LuxSaveHandler`, `LuxSavedGame`, `LuxSavedGameTypes`,
//! `LuxSavedEngineTypes`, `LuxSavedMap`): slot naming and rotation, and the gather/apply decision
//! flow of the saved types. Engine objects, entity creation and file I/O stay C++ and are
//! reached through [`env::Env`]; the byte format is the already ported `SerializeClass`.

pub mod engine;
pub mod env;
mod ffi;
pub mod game;
pub mod gamemap;
pub mod map;
pub mod names;
#[cfg(test)]
mod tests;
