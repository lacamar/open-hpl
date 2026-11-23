//! C ABI for the widget logic, declared in `gui/RustWidget.h`.
//!
//! Pointers are valid, aligned and non-null for the call unless noted; a null input makes the call
//! a no-op that returns the zero value. Op buffers are `cap` entries long and the call returns the
//! number written.

// ---------------------------------------------------------------- iWidget

// ---------------------------------------------------------------- image, dummy, group
