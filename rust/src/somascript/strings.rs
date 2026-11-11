//! Script string natives (`SomaScriptStringFuncs.cpp`, the `ToStr` family of `SomaScriptStrings.cpp`).

use super::cfmt;

/// `std::string::find(needle, from)`.
pub(crate) fn find<T: PartialEq>(h: &[T], n: &[T], from: usize) -> Option<usize> {
    if from > h.len() {
        return None;
    }
    if n.is_empty() {
        return Some(from);
    }
    h.get(from..)?
        .windows(n.len())
        .position(|w| w == n)
        .map(|p| p + from)
}

/// `std::string::rfind(needle)`.
pub(crate) fn rfind<T: PartialEq>(h: &[T], n: &[T]) -> Option<usize> {
    if n.len() > h.len() {
        return None;
    }
    (0..=h.len() - n.len())
        .rev()
        .find(|&i| h.get(i..i + n.len()) == Some(n))
}

pub(crate) fn float_to_string(f: f32, decimals: i32, remove_zeros: bool) -> Vec<u8> {
    let mut s = if decimals < 0 {
        cfmt::fmt_g(f, 64)
    } else {
        cfmt::fmt_prec_f(decimals, f, 64)
    };
    if remove_zeros && s.contains(&b'.') {
        while s.last() == Some(&b'0') {
            s.pop();
        }
        if s.last() == Some(&b'.') {
            s.pop();
        }
    }
    s
}

pub(crate) fn int_padded(x: i32, padding: i32) -> Vec<u8> {
    cfmt::fmt_padded_int(padding, x, 32)
}

pub(crate) fn find_char<T: PartialEq + Copy>(s: &[T], c: T, last: bool) -> i32 {
    let p = if last {
        s.iter().rposition(|&x| x == c)
    } else {
        s.iter().position(|&x| x == c)
    };
    p.map_or(-1, |p| p as i32)
}

pub(crate) fn find_str<T: PartialEq>(s: &[T], sub: &[T], last: bool) -> i32 {
    let p = if last { rfind(s, sub) } else { find(s, sub, 0) };
    p.map_or(-1, |p| p as i32)
}

pub(crate) fn count_chars<T: PartialEq>(s: &[T], c: &[T]) -> i32 {
    let mut n = 0;
    let mut p = find(s, c, 0);
    while !c.is_empty() {
        let Some(at) = p else { break };
        n += 1;
        p = find(s, c, at + c.len());
    }
    n
}

pub(crate) fn hash(s: &[u8]) -> u32 {
    s.iter().fold(5381u32, |h, &c| {
        h.wrapping_mul(33).wrapping_add(u32::from(c))
    })
}

pub(crate) fn numeric_suffix<T: Copy + Into<u32>>(s: &[T]) -> &[T] {
    let is_digit = |c: T| (u32::from(b'0')..=u32::from(b'9')).contains(&c.into());
    match s.iter().rposition(|&c| !is_digit(c)) {
        Some(p) => s.get(p + 1..).unwrap_or_default(),
        None => s,
    }
}

pub(crate) fn numeric_suffix_int(s: &[u8], default: i32) -> i32 {
    let n = numeric_suffix(s);
    if n.is_empty() {
        default
    } else {
        cfmt::c_atoi(n)
    }
}

/// `(int)wcstol(digits)`: strtol saturates at LONG_MAX, then the cast truncates.
pub(crate) fn numeric_suffix_int_wide(s: &[u32], default: i32) -> i32 {
    let n = numeric_suffix(s);
    if n.is_empty() {
        return default;
    }
    let mut acc: i64 = 0;
    for &c in n {
        let d = i64::from(c.wrapping_sub(u32::from(b'0')));
        match acc.checked_mul(10).and_then(|a| a.checked_add(d)) {
            Some(a) => acc = a,
            None => {
                acc = i64::MAX;
                break;
            }
        }
    }
    acc as i32
}

/// `cString::GetStringVec` semantics: an empty separator set falls back to whitespace and comma.
pub(crate) fn split<'a, T: Copy + PartialEq + From<u8>>(data: &'a [T], sep: &[T]) -> Vec<&'a [T]> {
    let default: [T; 5] = (*b" \n\r\t,").map(T::from);
    let sep: &[T] = if sep.is_empty() { &default } else { sep };
    let mut out = Vec::new();
    let mut start = 0;
    for (i, c) in data.iter().enumerate() {
        if sep.contains(c) {
            if i > start {
                out.push(data.get(start..i).unwrap_or_default());
            }
            start = i + 1;
        }
    }
    if data.len() > start {
        out.push(data.get(start..).unwrap_or_default());
    }
    out
}

pub(crate) fn seconds_to_clock(
    mut s: f32,
    hours: bool,
    minutes: bool,
    seconds: bool,
    hundredths: bool,
) -> Vec<u8> {
    let mut v = Vec::new();
    let h = (s / 3600.0f32) as i32;
    s -= h.wrapping_mul(3600) as f32;
    if hours {
        v.push(h);
    }
    let m = (s / 60.0f32) as i32;
    s -= m.wrapping_mul(60) as f32;
    if minutes {
        v.push(m);
    }
    let sec = s as i32;
    if seconds {
        v.push(sec);
    }
    if hundredths {
        v.push(((s - sec as f32) * 100.0f32) as i32);
    }
    let mut r = Vec::new();
    for (i, x) in v.iter().enumerate() {
        if i > 0 {
            r.push(b':');
        }
        if *x <= 9 {
            r.push(b'0');
        }
        r.extend_from_slice(x.to_string().as_bytes());
    }
    r
}

/// `cString_ReplaceStringToW`: the next search starts after the inserted text.
pub(crate) fn replace_all_wide(s: &[u32], old: &[u32], new: &[u32]) -> Vec<u32> {
    let mut r = s.to_vec();
    if old.is_empty() {
        return r;
    }
    let mut i = find(&r, old, 0);
    while let Some(at) = i {
        let end = (at + old.len()).min(r.len());
        r.splice(at..end, new.iter().copied());
        i = find(&r, old, at + new.len());
    }
    r
}

pub(crate) fn last_char(s: &[u8]) -> &[u8] {
    s.get(s.len().saturating_sub(1)..).unwrap_or_default()
}

/// `std::wstring(s.begin(), s.end())`: plain `char` is unsigned on aarch64.
pub(crate) fn widen(s: &[u8]) -> Vec<u32> {
    s.iter().map(|&c| u32::from(c)).collect()
}

fn join(parts: &[Vec<u8>]) -> Vec<u8> {
    let mut s = b"(".to_vec();
    for p in parts {
        if s.len() > 1 {
            s.extend_from_slice(b" : ");
        }
        s.extend_from_slice(p);
    }
    s.push(b')');
    s
}

fn floats(f: &[f32]) -> Vec<u8> {
    join(&f.iter().map(|&v| cfmt::fmt_f(v, 64)).collect::<Vec<_>>())
}

fn ints(i: &[i32]) -> Vec<u8> {
    join(
        &i.iter()
            .map(|v| v.to_string().into_bytes())
            .collect::<Vec<_>>(),
    )
}

pub(crate) const TOSTR_FLOAT: u32 = 0;
pub(crate) const TOSTR_FLOATS: u32 = 1;
pub(crate) const TOSTR_INTS: u32 = 2;
pub(crate) const TOSTR_RECT_F: u32 = 3;
pub(crate) const TOSTR_RECT_L: u32 = 4;
pub(crate) const TOSTR_PLANE: u32 = 5;
pub(crate) const TOSTR_MATRIX: u32 = 6;
pub(crate) const TOSTR_ID: u32 = 7;
pub(crate) const TOSTR_INT: u32 = 8;
pub(crate) const TOSTR_UINT: u32 = 9;
pub(crate) const TOSTR_BOOL: u32 = 10;

/// Text of a script value as `tString opAssign/opAdd` concatenation renders it.
pub(crate) fn tostr(kind: u32, f: &[f32], i: &[i32]) -> Vec<u8> {
    let part = |s: &[f32], a: usize, b: usize| floats(s.get(a..b).unwrap_or_default());
    match kind {
        TOSTR_FLOAT => cfmt::fmt_f(f.first().copied().unwrap_or(0.0), 64),
        TOSTR_FLOATS => floats(f),
        TOSTR_INTS => ints(i),
        TOSTR_RECT_F => [b"(".as_slice(), &part(f, 0, 2), &part(f, 2, 4), b")"].concat(),
        TOSTR_RECT_L => [
            b"(".as_slice(),
            &ints(i.get(..2).unwrap_or_default()),
            &ints(i.get(2..4).unwrap_or_default()),
            b")",
        ]
        .concat(),
        TOSTR_PLANE => [b"(".as_slice(), &floats(f), b")"].concat(),
        TOSTR_MATRIX => {
            let mut s = b"(".to_vec();
            for r in 0..4 {
                let row = part(f, r * 4, r * 4 + 4);
                s.extend_from_slice(if r > 0 { b" [" } else { b"[" });
                s.extend_from_slice(row.get(1..row.len().saturating_sub(1)).unwrap_or_default());
                s.push(b']');
            }
            s.push(b')');
            s
        }
        TOSTR_ID => {
            let a = i.first().copied().unwrap_or(0);
            let b = i.get(1).copied().unwrap_or(0);
            let c = i.get(2).copied().unwrap_or(0);
            format!("({a} {b} {c})").into_bytes()
        }
        TOSTR_INT => i.first().copied().unwrap_or(0).to_string().into_bytes(),
        TOSTR_UINT => (i.first().copied().unwrap_or(0) as u32)
            .to_string()
            .into_bytes(),
        TOSTR_BOOL => {
            if i.first().copied().unwrap_or(0) != 0 {
                b"true".to_vec()
            } else {
                b"false".to_vec()
            }
        }
        _ => Vec::new(),
    }
}
