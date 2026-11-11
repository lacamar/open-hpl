//! Script variable and string/number helpers. Values are decimal text, like the C++ var maps.

use crate::string;

pub(crate) fn from_int(x: i32) -> Vec<u8> {
    string::int_to_string(x, 0)
}

pub(crate) fn from_float(x: f32) -> Vec<u8> {
    string::float_to_string(x, 20, false)
}

pub(crate) fn add_int(cur: &[u8], d: i32) -> Vec<u8> {
    from_int(string::token_to_int(cur).wrapping_add(d))
}

pub(crate) fn add_float(cur: &[u8], d: f32) -> Vec<u8> {
    from_float(string::token_to_float(cur) + d)
}

pub(crate) fn get_int(cur: &[u8]) -> i32 {
    if cur.is_empty() {
        0
    } else {
        string::token_to_int(cur)
    }
}

pub(crate) fn get_float(cur: &[u8]) -> f32 {
    if cur.is_empty() {
        0.0
    } else {
        string::token_to_float(cur)
    }
}

pub(crate) fn contains(s: &[u8], sub: &[u8]) -> bool {
    string::first_pos(s, sub) >= 0
}
