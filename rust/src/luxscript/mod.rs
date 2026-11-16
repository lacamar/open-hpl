//! GA1: decision logic of the Amnesia TDD/AMFP script API (LuxScriptHandler, LuxHelpFuncs,
//! LuxGlobalDataHandler, LuxTypes). AngelScript registrations and entity virtual calls stay in C++.
//! Narrow strings are bytes, wide strings `u32` units (Linux `wchar_t`).

mod a1;
mod a1_native;
mod a1_table;
mod ffi;
mod fns;
mod help;
mod lookup;
mod tables;
mod trace;
pub(crate) mod types;
mod vars;

#[cfg(test)]
mod a1_tests;
#[cfg(test)]
mod tests;
