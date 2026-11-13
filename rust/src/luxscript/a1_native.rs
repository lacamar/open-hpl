//! A1 script functions with no engine object behind them.
#![allow(non_snake_case)]

use super::a1::raw_bytes;
use super::{tables, vars};
use crate::math;
use crate::string;
use std::ffi::c_void;

macro_rules! scalar {
    ($name:ident, $f:expr; ($($a:ident),*)) => {
        pub(super) unsafe extern "C" fn $name($($a: f32),*) -> f32 {
            $f($($a),*)
        }
    };
}

scalar!(f_ScriptSin, math::sin; (x));
scalar!(f_ScriptCos, math::cos; (x));
scalar!(f_ScriptTan, math::tan; (x));
scalar!(f_ScriptAsin, math::asin; (x));
scalar!(f_ScriptAcos, math::acos; (x));
scalar!(f_ScriptAtan, f32::atan; (x));
scalar!(f_ScriptAtan2, math::atan2; (x, y));
scalar!(f_ScriptSqrt, f32::sqrt; (x));
scalar!(f_ScriptPow, f32::powf; (b, e));
scalar!(f_ScriptMin, tables::min; (a, b));
scalar!(f_ScriptMax, tables::max; (a, b));
scalar!(f_ScriptClamp, math::clamp; (x, lo, hi));
scalar!(f_ScriptAbs, f32::abs; (x));

/// # Safety
/// `s` is an engine script string.
pub(super) unsafe extern "C" fn f_ScriptStringToInt(s: *const c_void) -> i32 {
    // SAFETY: caller contract.
    string::token_to_int(unsafe { raw_bytes(s) })
}

/// # Safety
/// `s` is an engine script string.
pub(super) unsafe extern "C" fn f_ScriptStringToFloat(s: *const c_void) -> f32 {
    // SAFETY: caller contract.
    string::token_to_float(unsafe { raw_bytes(s) })
}

/// # Safety
/// `s` is an engine script string.
pub(super) unsafe extern "C" fn f_ScriptStringToBool(s: *const c_void) -> bool {
    // SAFETY: caller contract.
    string::to_bool(unsafe { raw_bytes(s) })
}

/// # Safety
/// Arguments are engine script strings.
pub(super) unsafe extern "C" fn f_StringContains(s: *const c_void, sub: *const c_void) -> bool {
    // SAFETY: caller contract.
    vars::contains(unsafe { raw_bytes(s) }, unsafe { raw_bytes(sub) })
}
