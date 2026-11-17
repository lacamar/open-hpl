//! The AngelScript add-ons in Rust: `array<T>`, the reference-counted `string`, and `ExecuteString`.
//! The logic sits behind `host::Host` so it runs against a mock in tests; `shim` is the real engine.

pub mod array;
pub mod helper;
pub mod host;
pub mod shim;
pub mod sort;
pub mod string;
pub mod sys;

#[cfg(test)]
mod tests;
