//! GA1: decision logic of the Amnesia TDD/AMFP script API (LuxScriptHandler, LuxHelpFuncs,
//! LuxGlobalDataHandler, LuxTypes). AngelScript registrations and entity virtual calls stay in C++.
//! Narrow strings are bytes, wide strings `u32` units (Linux `wchar_t`).

mod ffi;
mod help;
mod lookup;
mod tables;
mod types;
mod vars;

#[cfg(test)]
mod tests;
