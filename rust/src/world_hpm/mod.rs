//! `.hpm` map parser (SOMA, Rebirth, Bunker): the parse half of `cWorldLoaderHpm`.

pub mod abi;
pub mod el;
mod ffi;
pub mod parse;

#[cfg(test)]
mod tests;
