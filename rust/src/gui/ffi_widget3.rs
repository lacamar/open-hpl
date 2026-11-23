//! C ABI for the menu and popup logic, declared in `gui/RustWidget3.h`.
//!
//! Pointers are valid, aligned and non-null for the call unless noted; a null input makes the call
//! a no-op that returns the zero value. Text is passed as `u32` units with a length; text results
//! are written into a caller buffer of `cap` units and the call returns the full length.

use super::popup as p;
use super::widgets_menu as m;

// ---------------------------------------------------------------- menu item

// ---------------------------------------------------------------- context menu

// ---------------------------------------------------------------- main menu

// ---------------------------------------------------------------- popup base

// ---------------------------------------------------------------- message box

// ---------------------------------------------------------------- colour picker

// ---------------------------------------------------------------- file picker

// ---------------------------------------------------------------- UI keyboard

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<m::MenuOp>() == 40);
    assert!(size_of::<m::ItemUpdate>() == 8);
    assert!(size_of::<m::ItemUp>() == 3 && size_of::<m::ItemLeave>() == 3);
    assert!(size_of::<m::CtxMetrics>() == 32 && size_of::<m::CtxWidth>() == 52);
    assert!(size_of::<m::CtxHide>() == 2 && size_of::<m::CtxUpdate>() == 8);
    assert!(size_of::<m::MainItemSize>() == 20 && size_of::<m::MainInit>() == 32);
    assert!(size_of::<p::PopUpState>() == 32 && size_of::<p::MsgBoxLayout>() == 64);
    assert!(size_of::<p::ColorLayout>() == 228 && size_of::<p::FpLayout>() == 156);
    assert!(size_of::<p::DirSelect>() == 16 && size_of::<p::KbKey>() == 28);
    assert!(size_of::<p::KbLayout>() == 16 && size_of::<p::KbNav>() == 16);
};
