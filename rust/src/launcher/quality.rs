//! `cQualityChooser`: card database from `launcher_card_database.cfg` and the series matcher.

use super::stdsort;
use crate::string;
use crate::xml::{DOC, Doc, Kind, c_str};

pub const UNSUPPORTED: i32 = -1;
pub const UNKNOWN: i32 = 3;
const ALPHA: u8 = b'*';
const NUM: u8 = b'?';

struct Series {
    code: Vec<u8>,
    rating: i32,
}

struct Model {
    name: Vec<u8>,
    series: Vec<Series>,
}

pub struct Chooser {
    models: Vec<Model>,
}

#[derive(Debug, PartialEq, Eq)]
pub struct LoadError;

fn words(s: &[u8]) -> usize {
    string::split(s, None, true).count()
}

fn query_int(value: &[u8]) -> Option<i32> {
    unsafe extern "C" {
        fn sscanf(s: *const std::ffi::c_char, fmt: *const std::ffi::c_char, ...) -> i32;
    }
    let mut buf = c_str(value).to_vec();
    buf.push(0);
    let mut out: i32 = 0;
    // SAFETY: NUL-terminated buffer and format; `out` is a live int for the single %d.
    let n = unsafe { sscanf(buf.as_ptr().cast(), c"%d".as_ptr(), &raw mut out) };
    (n == 1).then_some(out)
}

fn to_int(s: &[u8]) -> i32 {
    string::token_to_int(c_str(s))
}

impl Chooser {
    /// `BuildDatabase`; an unreadable or unparsable file is `Err`, and so are the malformed
    /// shapes the C++ crashed on (no root or `Models`, non-element child, missing attribute).
    pub fn from_bytes(data: Option<&[u8]>) -> Result<Self, LoadError> {
        let mut doc = Doc::new();
        if !data.is_some_and(|d| doc.load(d, true)) {
            return Err(LoadError);
        }
        let root = doc.first_child_element(DOC, None).ok_or(LoadError)?;
        let parent = doc
            .first_child_element(root, Some(b"Models"))
            .ok_or(LoadError)?;
        let mut models = Vec::new();
        for m in doc.children(parent) {
            if doc.node(m).kind != Kind::Element {
                return Err(LoadError);
            }
            let name = string::to_lower(doc.attribute(m, b"Name").ok_or(LoadError)?);
            let mut series = Vec::new();
            for s in doc.children(m) {
                if doc.node(s).kind != Kind::Element {
                    return Err(LoadError);
                }
                let code = string::to_lower(doc.attribute(s, b"Code").ok_or(LoadError)?);
                let rating = doc
                    .attribute(s, b"Rating")
                    .and_then(query_int)
                    .unwrap_or(UNKNOWN);
                series.push(Series { code, rating });
            }
            models.push(Model { name, series });
        }
        let counts: Vec<usize> = models.iter().map(|m| words(&m.name)).collect();
        let mut order: Vec<usize> = (0..models.len()).collect();
        let count = |i: usize| counts.get(i).copied().unwrap_or(0);
        stdsort::sort(&mut order, &|a, b| count(b) < count(a));
        let mut slots: Vec<Option<Model>> = models.into_iter().map(Some).collect();
        let models = order
            .into_iter()
            .filter_map(|i| slots.get_mut(i).and_then(Option::take))
            .collect();
        Ok(Self { models })
    }

    pub fn load(path: &[u8]) -> Result<Self, LoadError> {
        use std::os::unix::ffi::OsStrExt;
        let data = std::fs::read(std::ffi::OsStr::from_bytes(path)).ok();
        Self::from_bytes(data.as_deref())
    }

    /// `GetQualityRatingByCardString`: rating and the closest-series text (empty when exact).
    pub fn rate(&self, card: &[u8]) -> (i32, Vec<u8>) {
        let mut card: Vec<Vec<u8>> = string::split(&string::to_lower(card), None, true)
            .map(<[u8]>::to_vec)
            .collect();
        let (cur, closest) = self.parse_series(&mut card);
        if let Some((m, s)) = cur {
            return (self.series(m, s).map_or(UNKNOWN, |s| s.rating), Vec::new());
        }
        if let Some((m, s)) = closest
            && let (Some(model), Some(series)) = (self.models.get(m), self.series(m, s))
        {
            return (
                series.rating,
                [model.name.as_slice(), b" ", &series.code].concat(),
            );
        }
        (UNKNOWN, Vec::new())
    }

    fn series(&self, m: usize, s: usize) -> Option<&Series> {
        self.models.get(m)?.series.get(s)
    }

    fn parse_model(&self, card: &mut Vec<Vec<u8>>) -> Option<usize> {
        for (i, model) in self.models.iter().enumerate() {
            let pieces: Vec<&[u8]> = string::split(&model.name, Some(b" "), true).collect();
            let found = pieces
                .iter()
                .filter(|p| card.iter().any(|c| c == *p))
                .count();
            if found == pieces.len() {
                for p in pieces {
                    remove_erase_one(card, p);
                }
                return Some(i);
            }
        }
        None
    }

    fn parse_series(&self, card: &mut Vec<Vec<u8>>) -> (Found, Found) {
        let Some(m) = self.parse_model(card) else {
            return (None, None);
        };
        let Some(model) = self.models.get(m) else {
            return (None, None);
        };
        if card.is_empty() && !model.series.is_empty() {
            return (Some((m, 0)), None);
        }
        let mut closest = None;
        for token in card.iter() {
            let (mut prefix, mut numeric, mut suffix) = (0usize, 0usize, 0usize);
            for &c in token {
                if c.is_ascii_digit() || c == NUM {
                    if suffix == 0 {
                        numeric += 1;
                    } else {
                        break;
                    }
                } else if c.is_ascii_alphabetic() || c == ALPHA {
                    if numeric == 0 {
                        prefix += 1;
                    } else {
                        suffix += 1;
                    }
                } else {
                    break;
                }
            }
            let sub = |s: &[u8], start: usize, count: usize| -> Vec<u8> {
                string::sub(s, start as i32, count as i32, false)
                    .unwrap_or(&[])
                    .to_vec()
            };
            let lower = |s: Vec<u8>| string::to_lower(&s);
            let card_parts = [
                lower(sub(token, 0, prefix)),
                lower(sub(token, prefix + numeric, suffix)),
            ];
            let range = (2.0f64 * 10.0f64.powf(numeric as f64 - 1.0)) as i32;
            for (j, series) in model.series.iter().enumerate() {
                let code = &series.code;
                let code_parts = [
                    lower(sub(code, 0, prefix)),
                    lower(sub(code, prefix + numeric, suffix)),
                ];
                if !prefix_suffix_match(&card_parts, &code_parts) {
                    continue;
                }
                let card_num = sub(token, prefix, numeric);
                let code_num = sub(code, prefix, numeric);
                if card_num.len() != code_num.len() {
                    continue;
                }
                if code_num
                    .iter()
                    .zip(&card_num)
                    .all(|(&k, &c)| k == NUM || c == k)
                {
                    return (Some((m, j)), closest);
                }
                if numeric_difference(code, &card_num) < range {
                    closest = Some((m, j));
                }
            }
        }
        (None, closest)
    }
}

type Found = Option<(usize, usize)>;

fn prefix_suffix_match(card: &[Vec<u8>; 2], code: &[Vec<u8>; 2]) -> bool {
    for (cs, ks) in card.iter().zip(code) {
        if !ks.contains(&ALPHA) && cs.len() != ks.len() {
            return false;
        }
        for (i, &k) in ks.iter().enumerate() {
            if k != ALPHA && cs.get(i).copied().unwrap_or(0) != k {
                return false;
            }
        }
    }
    true
}

/// `cVideoCardSeries::GetNumericDifference`. The C++ passed the wildcard `char` to
/// `ToString(int)`, so the character replaced is the '6' of "63", not '?'.
fn numeric_difference(code: &[u8], card_number: &[u8]) -> i32 {
    let own = to_int(&string::replace_char_to(code, b"63", b"0"));
    to_int(card_number).wrapping_sub(own)
}

/// `v.erase(std::remove(v.begin(), v.end(), piece))`: erases the element at the new logical end,
/// which is a moved-from empty string or a leftover match. When `piece` is absent that is
/// `erase(end())`: UB in C++ (SIGSEGV with the shipped libstdc++), nothing here.
fn remove_erase_one(v: &mut Vec<Vec<u8>>, piece: &[u8]) {
    let Some(first) = v.iter().position(|c| c == piece) else {
        return;
    };
    let mut result = first;
    for i in first + 1..v.len() {
        if v.get(i).is_some_and(|c| c != piece) {
            let moved = v.get_mut(i).map(std::mem::take).unwrap_or_default();
            if let Some(slot) = v.get_mut(result) {
                *slot = moved;
            }
            result += 1;
        }
    }
    if result < v.len() {
        v.remove(result);
    }
}
