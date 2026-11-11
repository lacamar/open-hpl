//! Entity lookup by name and wildcard, entity type filter, body-name suffix match.

use crate::string;

pub(crate) const ENTITY_TYPE_AREA: i32 = 1;
pub(crate) const ENTITY_TYPE_ANY: i32 = 5;

pub(crate) const TYPE_NO: i32 = 0;
pub(crate) const TYPE_YES: i32 = 1;
pub(crate) const TYPE_NEED_AREA: i32 = 2;
pub(crate) const TYPE_NEED_PROP: i32 = 3;

pub(crate) fn has_wildcard(name: &[u8]) -> bool {
    string::count_chars(name, b"*") != 0
}

pub struct Wild {
    parts: Vec<Vec<u8>>,
}

impl Wild {
    pub(crate) fn new(name: &[u8]) -> Self {
        Self {
            parts: string::split(name, Some(b"*"), true)
                .map(<[u8]>::to_vec)
                .collect(),
        }
    }

    /// The C++ never advances its last-position cursor, so the order check only rejects a miss.
    pub(crate) fn matches(&self, entity_name: &[u8]) -> bool {
        self.parts
            .iter()
            .all(|p| string::first_pos(entity_name, p) > -1)
    }
}

/// `LuxIsCorrectType` up to the point where C++ must read the entity's own sub type.
pub(crate) fn type_prep(entity_type: i32, want_type: i32, want_sub: i32) -> i32 {
    if want_type == ENTITY_TYPE_ANY {
        TYPE_YES
    } else if entity_type != want_type {
        TYPE_NO
    } else if want_sub < 0 {
        TYPE_YES
    } else if entity_type == ENTITY_TYPE_AREA {
        TYPE_NEED_AREA
    } else {
        TYPE_NEED_PROP
    }
}

pub(crate) fn body_matches(body_name: &[u8], entity_name_len: i32, want: &[u8]) -> bool {
    string::sub(body_name, entity_name_len.wrapping_add(1), -1, false) == Some(want)
}

/// GetBodyInEntity searches by name only for a non-empty name on a multi-body entity.
pub(crate) fn searches_bodies(want: &[u8], body_num: i32) -> bool {
    !want.is_empty() && body_num > 1
}
