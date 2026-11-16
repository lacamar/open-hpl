//! A1 script functions with no engine object behind them.
#![allow(non_snake_case)]

use super::a1::{Arg, raw_bytes, str_arg};
use super::{tables, vars};
use crate::math;
use crate::string;
use std::ffi::c_void;

macro_rules! scalar {
    ($name:ident, $f:expr; ($($a:ident),*)) => {
        pub(super) unsafe extern "C" fn $name($($a: f32),*) -> f32 {
            let r = $f($($a),*);
            if super::trace::enabled() {
                let a = [$(Arg::f($a)),*];
                super::trace::line(stringify!($name).get(2..).unwrap_or_default(), &"f".repeat(a.len()), &a, 'f', &Arg::f(r));
            }
            r
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
    let r = string::token_to_int(unsafe { raw_bytes(s) });
    super::trace::line(
        "ScriptStringToInt",
        "s",
        &[unsafe { str_arg(s) }],
        'i',
        &Arg::i(r),
    );
    r
}

/// # Safety
/// `s` is an engine script string.
pub(super) unsafe extern "C" fn f_ScriptStringToFloat(s: *const c_void) -> f32 {
    // SAFETY: caller contract.
    let r = string::token_to_float(unsafe { raw_bytes(s) });
    super::trace::line(
        "ScriptStringToFloat",
        "s",
        &[unsafe { str_arg(s) }],
        'f',
        &Arg::f(r),
    );
    r
}

/// # Safety
/// `s` is an engine script string.
pub(super) unsafe extern "C" fn f_ScriptStringToBool(s: *const c_void) -> bool {
    // SAFETY: caller contract.
    let r = string::to_bool(unsafe { raw_bytes(s) });
    super::trace::line(
        "ScriptStringToBool",
        "s",
        &[unsafe { str_arg(s) }],
        'b',
        &Arg::b(r),
    );
    r
}

/// # Safety
/// Arguments are engine script strings.
pub(super) unsafe extern "C" fn f_StringContains(s: *const c_void, sub: *const c_void) -> bool {
    // SAFETY: caller contract.
    let r = vars::contains(unsafe { raw_bytes(s) }, unsafe { raw_bytes(sub) });
    super::trace::line(
        "StringContains",
        "ss",
        &[unsafe { str_arg(s) }, unsafe { str_arg(sub) }],
        'b',
        &Arg::b(r),
    );
    r
}
