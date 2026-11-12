//! Pure parts of `hpl::cString`. Narrow strings are `u8` units, wide strings `u32` (Linux `wchar_t`).

use std::ffi::{c_char, c_int, c_long, c_ulong};

unsafe extern "C" {
    fn tolower(c: c_int) -> c_int;
    fn toupper(c: c_int) -> c_int;
    fn atoi(s: *const c_char) -> c_int;
    fn atof(s: *const c_char) -> f64;
    fn snprintf(buf: *mut c_char, n: usize, fmt: *const c_char, ...) -> c_int;
}

pub trait Unit: Copy + Eq {
    fn ascii(c: u8) -> Self;
    fn code(self) -> i32;
    fn from_code(c: i32) -> Self;
}

impl Unit for u8 {
    fn ascii(c: u8) -> Self {
        c
    }
    fn code(self) -> i32 {
        i32::from(self)
    }
    fn from_code(c: i32) -> Self {
        c as u8
    }
}

impl Unit for u32 {
    fn ascii(c: u8) -> Self {
        u32::from(c)
    }
    fn code(self) -> i32 {
        self as i32
    }
    fn from_code(c: i32) -> Self {
        c as u32
    }
}

fn a<T: Unit>(c: u8) -> T {
    T::ascii(c)
}

fn is_slash<T: Unit>(c: T) -> bool {
    c == a(b'/') || c == a(b'\\')
}

fn window_is<T: Unit>(s: &[T], needle: &[T], i: usize) -> bool {
    s.get(i..(i + needle.len()).min(s.len()))
        .is_some_and(|w| w == needle)
}

pub fn first_pos<T: Unit>(s: &[T], needle: &[T]) -> i32 {
    (0..s.len())
        .find(|&i| window_is(s, needle, i))
        .map_or(-1, |i| i as i32)
}

pub fn last_pos<T: Unit>(s: &[T], needle: &[T]) -> i32 {
    (0..s.len())
        .rev()
        .find(|&i| window_is(s, needle, i))
        .map_or(-1, |i| i as i32)
}

fn last_pos_ch<T: Unit>(s: &[T], c: u8) -> i32 {
    last_pos(s, &[a(c)])
}

/// `None` where the C++ `substr` throws `std::out_of_range`.
pub fn sub<T: Unit>(s: &[T], start: i32, count: i32, wide: bool) -> Option<&[T]> {
    let size = s.len() as i32;
    if (wide && size == 0) || start >= size {
        return Some(&[]);
    }
    let count = if start.wrapping_add(count) > size {
        size.wrapping_sub(start)
    } else {
        count
    };
    if start < 0 {
        return None;
    }
    let start = start as usize;
    let rest = s.get(start..).unwrap_or_default();
    Some(if count < 0 {
        rest
    } else {
        rest.get(..count as usize).unwrap_or(rest)
    })
}

fn tail<T: Unit>(s: &[T], from: usize) -> &[T] {
    s.get(from..).unwrap_or_default()
}

fn name_start<T: Unit>(s: &[T]) -> usize {
    let pos = last_pos_ch(s, b'\\').max(last_pos_ch(s, b'/'));
    (pos + 1) as usize
}

pub fn file_name<T: Unit>(s: &[T]) -> &[T] {
    s.get(name_start(s)..).unwrap_or_default()
}

pub fn file_ext<T: Unit>(s: &[T]) -> &[T] {
    let name = file_name(s);
    match last_pos_ch(name, b'.') {
        -1 => &[],
        p => name.get(p as usize + 1..).unwrap_or_default(),
    }
}

pub fn file_path<T: Unit>(s: &[T]) -> &[T] {
    if last_pos_ch(s, b'.') < 0 {
        return s;
    }
    s.get(..name_start(s)).unwrap_or_default()
}

pub fn set_file_ext<T: Unit>(s: &[T], ext: &[T]) -> Vec<T> {
    let ext = ext.strip_prefix(&[a(b'.')][..]).unwrap_or(ext);
    let mut out = if file_ext(s).is_empty() {
        s.to_vec()
    } else {
        s.get(..last_pos_ch(s, b'.') as usize)
            .unwrap_or_default()
            .to_vec()
    };
    if !ext.is_empty() {
        out.push(a(b'.'));
        out.extend_from_slice(ext);
    }
    out
}

pub fn set_file_path<T: Unit>(s: &[T], path: &[T]) -> Vec<T> {
    let mut out = path.to_vec();
    if !path.last().is_some_and(|&c| is_slash(c)) {
        out.push(a(b'/'));
    }
    out.extend_from_slice(file_name(s));
    out
}

pub fn add_slash_at_end<T: Unit>(s: &[T], slash: T) -> Vec<T> {
    let mut out = s.to_vec();
    if s.last().is_some_and(|&c| !is_slash(c)) {
        out.push(slash);
    }
    out
}

pub fn remove_slash_at_end<T: Unit>(s: &[T]) -> &[T] {
    match s.split_last() {
        Some((&c, rest)) if is_slash(c) => rest,
        _ => s,
    }
}

pub fn last_char<T: Unit>(s: &[T]) -> &[T] {
    tail(s, s.len().saturating_sub(1))
}

fn map_case<T: Unit>(s: &[T], f: unsafe extern "C" fn(c_int) -> c_int) -> Vec<T> {
    // SAFETY: tolower/toupper accept any int.
    s.iter()
        .map(|c| T::from_code(unsafe { f(c.code()) }))
        .collect()
}

pub fn to_lower<T: Unit>(s: &[T]) -> Vec<T> {
    map_case(s, tolower)
}

pub fn to_upper<T: Unit>(s: &[T]) -> Vec<T> {
    map_case(s, toupper)
}

pub fn replace_char_to<T: Unit>(s: &[T], old: &[T], new: &[T]) -> Vec<T> {
    let old = old.first().copied().unwrap_or(a(0));
    match new.first() {
        Some(&n) => s.iter().map(|&c| if c == old { n } else { c }).collect(),
        None => s.iter().copied().filter(|&c| c != old).collect(),
    }
}

/// Empty `old` hangs the C++ (index wraps); here it returns the input.
pub fn replace_string_to(s: &[u8], old: &[u8], new: &[u8]) -> Vec<u8> {
    if old.is_empty() {
        return s.to_vec();
    }
    let mut out = Vec::with_capacity(s.len());
    let mut i = 0;
    while let Some(&c) = s.get(i) {
        if s.get(i..).is_some_and(|rest| rest.starts_with(old)) {
            out.extend_from_slice(new);
            i += old.len();
        } else {
            out.push(c);
            i += 1;
        }
    }
    out
}

pub fn count_chars<T: Unit>(s: &[T], c: &[T]) -> i32 {
    let c = c.first().copied().unwrap_or(a(0));
    s.iter().filter(|&&x| x == c).count() as i32
}

pub fn split<'a, T: Unit>(
    data: &'a [T],
    seps: Option<&[T]>,
    cr_is_default_sep: bool,
) -> impl Iterator<Item = &'a [T]> {
    let seps: Vec<T> = seps.map_or_else(
        || {
            let mut d = vec![a(b' '), a(b'\n'), a(b'\t'), a(b',')];
            if cr_is_default_sep {
                d.push(a(b'\r'));
            }
            d
        },
        <[T]>::to_vec,
    );
    data.split(move |c| seps.contains(c))
        .filter(|t| !t.is_empty())
}

pub fn relative_path<T: Unit>(path: &[T], relative_to: &[T]) -> Vec<T> {
    let seps = [a(b'/'), a(b'\\')];
    let p: Vec<&[T]> = split(path, Some(&seps), true).collect();
    let r: Vec<&[T]> = split(relative_to, Some(&seps), true).collect();
    let common = p.iter().zip(&r).take_while(|(x, y)| x == y).count();
    if common == 0 {
        return path.to_vec();
    }
    let mut out = Vec::new();
    for _ in common..r.len() {
        out.extend_from_slice(&[a(b'.'), a(b'.'), a(b'/')]);
    }
    for seg in p.get(common..).unwrap_or_default() {
        out.extend_from_slice(seg);
        out.push(a(b'/'));
    }
    out
}

pub fn numeric_suffix<T: Unit>(s: &[T]) -> (&[T], i32) {
    let digit = |c: T| c.code() >= i32::from(b'0') && c.code() <= i32::from(b'9');
    match s.iter().rposition(|&c| !digit(c)) {
        Some(i) if i + 1 < s.len() => (s.get(i + 1..).unwrap_or_default(), (i + 1) as i32),
        _ => (&[], -1),
    }
}

pub fn matches_wildcard(pattern: &[u8], name: &[u8]) -> bool {
    if !pattern.contains(&b'*') {
        return pattern == name;
    }
    let mut prev: i64 = -1;
    for seg in pattern.split(|&c| c == b'*').filter(|s| !s.is_empty()) {
        let Some(pos) = name.windows(seg.len()).position(|w| w == seg) else {
            return false;
        };
        if pos as i64 <= prev || (prev < 0 && pattern.first() != Some(&b'*') && pos != 0) {
            return false;
        }
        prev = pos as i64;
    }
    true
}

fn terminated(token: &[u8]) -> Vec<u8> {
    let mut t = token.to_vec();
    t.push(0);
    t
}

/// # Safety
/// `s` is NUL-terminated.
pub unsafe fn to_int(s: *const c_char) -> i32 {
    // SAFETY: caller contract.
    unsafe { atoi(s) }
}

/// # Safety
/// `s` is NUL-terminated.
pub unsafe fn to_float(s: *const c_char) -> f32 {
    // SAFETY: caller contract.
    unsafe { atof(s) as f32 }
}

pub fn token_to_int(token: &[u8]) -> i32 {
    // SAFETY: `terminated` appends the NUL.
    unsafe { to_int(terminated(token).as_ptr().cast()) }
}

pub fn token_to_float(token: &[u8]) -> f32 {
    // SAFETY: `terminated` appends the NUL.
    unsafe { to_float(terminated(token).as_ptr().cast()) }
}

pub fn to_bool(s: &[u8]) -> bool {
    to_lower(s) == b"true"
}

/// `None` unless the string holds exactly `out.len()` numbers.
pub fn floats_exact(s: &[u8], out: &mut [f32]) -> bool {
    let v: Vec<f32> = split(s, None, true).map(token_to_float).collect();
    v.len() == out.len() && {
        out.copy_from_slice(&v);
        true
    }
}

pub fn ints_exact(s: &[u8], out: &mut [i32]) -> bool {
    let v: Vec<i32> = split(s, None, true).map(token_to_int).collect();
    v.len() == out.len() && {
        out.copy_from_slice(&v);
        true
    }
}

fn format_with(f: impl FnOnce(*mut c_char, usize) -> c_int) -> Vec<u8> {
    let mut buf = [0u8; 256];
    let n = f(buf.as_mut_ptr().cast(), buf.len());
    let len = usize::try_from(n).unwrap_or(0).min(buf.len() - 1);
    buf.get(..len).unwrap_or_default().to_vec()
}

pub fn int_to_string(x: i32, pad: i32) -> Vec<u8> {
    // SAFETY: format matches the (int, int) varargs; buffer bound passed.
    format_with(|b, n| unsafe { snprintf(b, n, c"%0*d".as_ptr(), pad, x) })
}

pub fn uint_to_string(x: u32, pad: i32) -> Vec<u8> {
    // SAFETY: format matches the (int, unsigned) varargs; buffer bound passed.
    format_with(|b, n| unsafe { snprintf(b, n, c"%0*u".as_ptr(), pad, x) })
}

pub fn ulong_to_string(x: u64, pad: i32) -> Vec<u8> {
    // SAFETY: format matches the (int, unsigned long) varargs; buffer bound passed.
    format_with(|b, n| unsafe { snprintf(b, n, c"%0*lu".as_ptr(), pad, x as c_ulong) })
}

/// The wide C++ twin formats an unsigned long with `%ld`, so values >= 2^63 print negative.
pub fn ulong_to_string_wide(x: u64, pad: i32) -> Vec<u8> {
    // SAFETY: format matches the (int, long) varargs; buffer bound passed.
    format_with(|b, n| unsafe { snprintf(b, n, c"%0*ld".as_ptr(), pad, x as c_long) })
}

pub fn float_to_string(x: f32, decimals: i32, remove_zeros: bool) -> Vec<u8> {
    let decimals = if decimals < 0 { 20 } else { decimals };
    // SAFETY: format matches the (int, double) varargs; buffer bound passed.
    let mut s =
        format_with(|b, n| unsafe { snprintf(b, n, c"%.*f".as_ptr(), decimals, f64::from(x)) });
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

/// Out-of-range lead bytes hang the C++; here each becomes U+FFFD. Missing tail bytes read as 0.
pub fn utf8_to_wide(s: &[u8]) -> Vec<u32> {
    let mut out = Vec::with_capacity(s.len());
    let mut i = 0;
    while i < s.len() {
        let b = |k: usize| u32::from(s.get(i + k).copied().unwrap_or(0));
        let lead = b(0);
        let cont = |k: usize| b(k) & 0x3f;
        let (v, n) = match lead {
            0x00..=0x7f => (lead, 1),
            0xc0..=0xdf => ((lead & 0x1f) << 6 | cont(1), 2),
            0xe0..=0xef => ((lead & 0x0f) << 12 | cont(1) << 6 | cont(2), 3),
            0xf0..=0xf7 => (
                (lead & 0x07) << 18 | cont(1) << 12 | cont(2) << 6 | cont(3),
                4,
            ),
            0xf8..=0xfb => (
                (lead & 0x03) << 24 | cont(1) << 18 | cont(2) << 12 | cont(3) << 6 | cont(4),
                5,
            ),
            0xfc..=0xfd => (
                (lead & 0x01) << 30
                    | cont(1) << 24
                    | cont(2) << 18
                    | cont(3) << 12
                    | cont(4) << 6
                    | cont(5),
                6,
            ),
            _ => (0xfffd, 1),
        };
        out.push(v);
        i += n;
    }
    out
}

/// Units >= 0x80000000 produce nothing, as in the C++.
pub fn wide_to_utf8(s: &[u32]) -> Vec<u8> {
    let mut out = Vec::with_capacity(s.len());
    for &c in s {
        let lead = |mask: u32, shift: u32, tag: u32| ((c & mask) >> shift | tag) as u8;
        let cont = |mask: u32, shift: u32| ((c & mask) >> shift | 0x80) as u8;
        match c {
            0..0x80 => out.push(c as u8),
            0x80..0x800 => out.extend([lead(0x7c0, 6, 0xc0), cont(0x3f, 0)]),
            0x800..0x10000 => out.extend([lead(0xf000, 12, 0xe0), cont(0xfc0, 6), cont(0x3f, 0)]),
            0x10000..0x200000 => out.extend([
                lead(0x1c0000, 18, 0xf0),
                cont(0x3f000, 12),
                cont(0xfc0, 6),
                cont(0x3f, 0),
            ]),
            0x200000..0x4000000 => out.extend([
                lead(0x3000000, 24, 0xf8),
                cont(0xfc000, 18),
                cont(0x3f000, 12),
                cont(0xfc0, 6),
                cont(0x3f, 0),
            ]),
            0x4000000..0x80000000 => out.extend([
                lead(0x40000000, 30, 0xfc),
                cont(0x3f000000, 24),
                cont(0xfc000, 18),
                cont(0x3f000, 12),
                cont(0xfc0, 6),
                cont(0x3f, 0),
            ]),
            _ => {}
        }
    }
    out
}

/// Wide hash over the unit values; the narrow `GetHash` reads out of bounds and stays in C++.
pub fn hash_wide(s: &[u32]) -> u32 {
    if s.is_empty() {
        return 0;
    }
    let mut h = s.len() as u32;
    let mut pairs = s.chunks_exact(2);
    for p in &mut pairs {
        let &[p0, p1] = p else { continue };
        h = h.wrapping_add(p0);
        let t = (p1 << 11) ^ h;
        h = (h << 16) ^ t;
        h = h.wrapping_add(h >> 11);
    }
    if let [last] = pairs.remainder() {
        h = h.wrapping_add(*last);
        h ^= h << 10;
        h = h.wrapping_add(h >> 1);
    }
    h ^= h << 3;
    h = h.wrapping_add(h >> 5);
    h ^= h << 4;
    h = h.wrapping_add(h >> 17);
    h ^= h << 25;
    h.wrapping_add(h >> 6)
}

fn encode_chunk(c: [u8; 4]) -> [u8; 6] {
    let [c0, c1, c2, c3] = c;
    let v = [
        c0,
        c1,
        c2,
        c3,
        (c2 >> 2) & 0x30 | (c0 >> 4) & 0x0c,
        (c3 >> 2) & 0x30 | (c1 >> 4) & 0x0c,
    ];
    v.map(|x| {
        let x = x & 63;
        if x == 63 { x } else { x + 64 }
    })
}

/// `None` unless the length is a multiple of 4.
pub fn encode_data(data: &[u8]) -> Option<Vec<u8>> {
    if !data.len().is_multiple_of(4) {
        return None;
    }
    Some(
        data.chunks_exact(4)
            .filter_map(|c| <[u8; 4]>::try_from(c).ok())
            .flat_map(encode_chunk)
            .collect(),
    )
}

/// `false` unless `dest.len()` is a multiple of 4 and `s` holds 6 chars per 4 bytes.
pub fn decode_data(s: &[u8], dest: &mut [u8]) -> bool {
    if !dest.len().is_multiple_of(4) || s.len() < (dest.len() >> 2) * 6 {
        return false;
    }
    for (chunk, out) in s.chunks_exact(6).zip(dest.chunks_exact_mut(4)) {
        let &[c0, c1, c2, c3, c4, c5] = chunk else {
            continue;
        };
        let front = u32::from_le_bytes([c0, c1, c2, c3]);
        let back = i32::from(i16::from_le_bytes([c4, c5]));
        let hi = ((back << 18) | (back << 4)) as u32 & 0xc0c0_c0c0;
        out.copy_from_slice(&((front & 0x3f3f_3f3f) | hi).to_le_bytes());
    }
    true
}

#[cfg(test)]
mod tests {
    use super::*;

    fn s(x: &str) -> &[u8] {
        x.as_bytes()
    }

    #[test]
    fn paths() {
        assert_eq!(file_ext(s("archive.tar.gz")), s("gz"));
        assert_eq!(file_ext(s("a.b/c")), s(""));
        assert_eq!(file_name(s("C:\\a\\b\\c.txt")), s("c.txt"));
        assert_eq!(file_path(s("/a/b/c.txt")), s("/a/b/"));
        assert_eq!(file_path(s("c.txt")), s(""));
        assert_eq!(file_path(s("a/b/noext")), s("a/b/noext"));
        assert_eq!(set_file_ext(s("test.exe"), s(".txt")), s("test.txt"));
        assert_eq!(set_file_ext(s("test.exe"), s("")), s("test"));
        assert_eq!(set_file_path(s("x/y.txt"), s("/p")), s("/p/y.txt"));
        assert_eq!(add_slash_at_end(s("foo"), b'/'), s("foo/"));
        assert_eq!(add_slash_at_end(s(""), b'/'), s(""));
        assert_eq!(remove_slash_at_end(s("foo\\")), s("foo"));
        assert_eq!(
            relative_path(s("/a/b/c/file.txt"), s("/a/b/x/y")),
            s("../../c/file.txt/")
        );
        assert_eq!(relative_path(s("a/b"), s("z/y")), s("a/b"));
    }

    #[test]
    fn substr_quirks() {
        assert_eq!(sub(s("abc"), 1, -1, false), Some(s("bc")));
        assert_eq!(sub(s("abc"), 1, 99, false), Some(s("bc")));
        assert_eq!(sub(s("abc"), 3, 1, false), Some(s("")));
        assert_eq!(sub(s("abc"), -1, -1, false), None);
        assert_eq!(sub(s(""), -1, -1, true), Some(s("")));
        assert_eq!(sub(s(""), -1, -1, false), None);
    }

    #[test]
    fn positions() {
        assert_eq!(first_pos(s("abcabc"), s("bc")), 1);
        assert_eq!(last_pos(s("abcabc"), s("bc")), 4);
        assert_eq!(last_pos(s("abc"), s("")), 2);
        assert_eq!(first_pos(s("ab"), s("bc")), -1);
        assert_eq!(numeric_suffix(s("door12")), (s("12"), 4));
        assert_eq!(numeric_suffix(s("123")), (s(""), -1));
    }

    #[test]
    fn splitting_and_numbers() {
        let t: Vec<_> = split(s("1, 2,3\n4"), None, true).collect();
        assert_eq!(t, [s("1"), s("2"), s("3"), s("4")]);
        let mut v = [0f32; 3];
        assert!(floats_exact(s("1.5 2.5 3.5"), &mut v));
        assert_eq!(v, [1.5, 2.5, 3.5]);
        assert!(!floats_exact(s("1.5 2.5"), &mut v));
        assert_eq!(token_to_int(s("  -12abc")), -12);
        assert_eq!(token_to_int(s("99999999999")), 1_215_752_191);
        assert!(to_bool(s("TRUE")) && !to_bool(s("yes")));
    }

    #[test]
    fn formatting() {
        assert_eq!(int_to_string(5, 3), s("005"));
        assert_eq!(int_to_string(5, -3), s("5  "));
        assert_eq!(float_to_string(1.5, 2, true), s("1.5"));
        assert_eq!(float_to_string(1.0, 2, true), s("1"));
        assert_eq!(float_to_string(1.0, 2, false), s("1.00"));
        assert_eq!(float_to_string(0.1, -1, false), s("0.10000000149011611938"));
        assert_eq!(float_to_string(100.0, 0, true), s("100"));
        assert_eq!(ulong_to_string_wide(u64::MAX, 0), s("-1"));
    }

    #[test]
    fn wildcard() {
        assert!(matches_wildcard(
            s("*shutter_door*Hydroponics"),
            s("shutter_door_5_Hydroponics_1")
        ));
        assert!(!matches_wildcard(s("door*"), s("shutter_door")));
        assert!(matches_wildcard(
            s("*_lockdown*BioMech"),
            s("biomech_door1panel_1_lockdown_BioMech")
        ));
        assert!(!matches_wildcard(s("Biomech*"), s("biomech_door")));
        assert!(!matches_wildcard(s("abc"), s("abcd")));
    }

    #[test]
    fn utf8_round_trip() {
        let w = utf8_to_wide("aé€😀".as_bytes());
        assert_eq!(w, [0x61, 0xe9, 0x20ac, 0x1f600]);
        assert_eq!(wide_to_utf8(&w), "aé€😀".as_bytes());
        assert_eq!(utf8_to_wide(&[0x80]), [0xfffd]);
    }

    #[test]
    fn text_encoding_round_trip() {
        let data = [1u8, 2, 3, 250, 255, 0, 17, 128];
        let enc = encode_data(&data).unwrap();
        assert_eq!(enc.len(), 12);
        assert!(enc.iter().all(|&c| (63..127).contains(&c)));
        let mut back = [0u8; 8];
        assert!(decode_data(&enc, &mut back));
        assert_eq!(back, data);
        assert!(encode_data(&[1, 2, 3]).is_none());
    }

    #[test]
    fn hash_wide_is_stable() {
        assert_eq!(hash_wide(&[]), 0);
        assert_ne!(hash_wide(&[1]), hash_wide(&[1, 0]));
    }
}
