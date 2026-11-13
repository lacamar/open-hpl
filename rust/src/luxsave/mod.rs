//! Amnesia save layer (`LuxSaveHandler`, `LuxSavedGame`, `LuxSavedGameTypes`,
//! `LuxSavedEngineTypes`, `LuxSavedMap`): slot naming and rotation, and the gather/apply decision
//! flow of the saved types. Engine objects, entity creation and file I/O stay C++ and are
//! reached through [`env::Env`]; the byte format is the already ported `SerializeClass`.

#[cfg(test)]
mod a8_prop_tests;
pub mod a8_props;
pub mod a8_table;
mod a8_tables;
#[cfg(test)]
mod a8_tests;
pub mod engine;
pub mod env;
mod ffi;
pub mod game;
pub mod gamemap;
pub mod map;
pub mod names;
#[cfg(test)]
mod tests;
