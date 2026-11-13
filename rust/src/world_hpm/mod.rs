//! `.hpm` map parser (SOMA, Rebirth, Bunker): the parse half of `cWorldLoaderHpm`.

pub mod abi;
pub mod el;
mod ffi;
mod h4_ffi;
pub mod h4_plan;
pub mod parse;

#[cfg(test)]
mod h4_golden;
#[cfg(test)]
mod tests;
