//! Recovered script API (`SomaScriptApi.cpp`): `script_api.txt` parsing, declaration matching,
//! enum rebasing and the stub/dummy decisions. Registration with AngelScript stays in C++.

use super::cfmt;
use super::strings::find;

use std::collections::BTreeMap;

const NPOS: usize = usize::MAX;

fn find_byte(s: &[u8], c: u8, from: usize) -> usize {
    s.get(from..)
        .and_then(|t| t.iter().position(|&x| x == c))
        .map_or(NPOS, |p| p + from)
}

fn find_first_of(s: &[u8], set: &[u8], from: usize) -> usize {
    s.get(from..)
        .and_then(|t| t.iter().position(|c| set.contains(c)))
        .map_or(NPOS, |p| p + from)
}

/// `find_last_of(set, upto)`: a position at or before `upto`.
fn find_last_of(s: &[u8], set: &[u8], upto: usize) -> usize {
    let end = upto.saturating_add(1).min(s.len());
    s.get(..end)
        .and_then(|t| t.iter().rposition(|c| set.contains(c)))
        .unwrap_or(NPOS)
}

fn find_last_not_of(s: &[u8], set: &[u8]) -> usize {
    s.iter().rposition(|c| !set.contains(c)).unwrap_or(NPOS)
}

/// `substr(pos, len)` with a position past the end clamped to it.
fn substr(s: &[u8], pos: usize, len: usize) -> &[u8] {
    let start = pos.min(s.len());
    let end = start.saturating_add(len).min(s.len());
    s.get(start..end).unwrap_or_default()
}

/// AngelScript 2.28 accepted defaults on `&out` parameters; 2.38 spells "discard" as `= void`.
pub(crate) fn out_defaults_to_void(s: &[u8]) -> Vec<u8> {
    let mut s = s.to_vec();
    let mut p = find(&s, b"&out", 0);
    while let Some(at) = p {
        let eq = find_byte(&s, b'=', at);
        let comma = find_first_of(&s, b",)", at);
        if eq != NPOS && eq <= comma {
            let mut depth = 0;
            let mut e = eq + 1;
            while let Some(&c) = s.get(e) {
                if c == b'(' || c == b'<' {
                    depth += 1;
                } else if (c == b')' || c == b'>') && depth > 0 {
                    depth -= 1;
                } else if (c == b',' || c == b')') && depth == 0 {
                    break;
                }
                e += 1;
            }
            s.splice(eq..e.min(s.len()), b"= void".iter().copied());
        }
        p = find(&s, b"&out", at + 4);
    }
    s
}

/// Engine objects are registered NOCOUNT, where auto handles are meaningless and rejected.
pub(crate) fn no_auto_handles(s: &[u8]) -> Vec<u8> {
    let mut s = s.to_vec();
    while let Some(p) = find(&s, b"@+", 0) {
        s.remove(p + 1);
    }
    s
}

fn decl_type(s: &[u8], is_param: bool) -> Vec<u8> {
    let eq = find_byte(s, b'=', 0);
    let s = s.get(..eq.min(s.len())).unwrap_or_default();
    let mut toks: Vec<Vec<u8>> = Vec::new();
    let mut tok: Vec<u8> = Vec::new();
    let mut i = 0;
    while i <= s.len() {
        let c = s.get(i).copied().unwrap_or(b' ');
        if c == b'&' || c == b'@' {
            if tok.is_empty()
                && let Some(t) = toks.pop()
            {
                tok = t;
            }
            tok.push(c);
            while c == b'&' && s.get(i + 1) == Some(&b' ') {
                i += 1;
            }
        } else if c != b' ' {
            tok.push(c);
        } else if !tok.is_empty() {
            toks.push(std::mem::take(&mut tok));
        }
        i += 1;
    }
    let const_only = toks.len() == 2 && toks.first().is_some_and(|t| t == b"const");
    if is_param && toks.len() >= 2 && !const_only {
        toks.pop();
    }
    toks.join(&b' ')
}

#[derive(Default)]
pub(crate) struct DeclSig {
    pub name: Vec<u8>,
    pub tail: Vec<u8>,
    pub types: Vec<Vec<u8>>,
    pub defaults: Vec<bool>,
}

pub(crate) fn parse_decl(s: &[u8]) -> DeclSig {
    let paren = find_byte(s, b'(', 0);
    let name_start = find_last_of(s, b" &@", paren).wrapping_add(1);
    let mut sig = DeclSig {
        name: substr(s, name_start, paren.wrapping_sub(name_start)).to_vec(),
        ..DeclSig::default()
    };
    sig.types.push(decl_type(substr(s, 0, name_start), false));
    sig.defaults.push(false);
    let mut depth = 0;
    let mut start = paren.wrapping_add(1);
    let mut i = start;
    while let Some(&c) = s.get(i) {
        if c == b'(' || c == b'<' {
            depth += 1;
        } else if (c == b')' || c == b'>') && depth > 0 {
            depth -= 1;
        } else if c == b',' || c == b')' {
            let first = s
                .get(start..)
                .and_then(|t| t.iter().position(|&x| x != b' '))
                .map_or(NPOS, |p| p + start);
            if first < i {
                sig.types.push(decl_type(substr(s, start, i - start), true));
                sig.defaults.push(find_byte(s, b'=', start) < i);
            }
            start = i + 1;
            if c == b')' {
                break;
            }
        }
        i += 1;
    }
    sig.tail = decl_type(s.get(i + 1..).unwrap_or_default(), false);
    sig
}

/// Index of the candidate a native declaration is bound to instead of itself, if any.
pub(crate) fn native_decl_pick(
    cands: &[&[u8]],
    is_enum: &dyn Fn(&[u8]) -> bool,
    decl: &[u8],
) -> Option<usize> {
    let sig = parse_decl(decl);
    let is_int = |s: &[u8]| s == b"int" || s == b"uint";
    let mut matched: Option<usize> = None;
    let mut longer: Option<usize> = None;
    let mut longer_count = 0;
    for (n, cand_decl) in cands.iter().enumerate() {
        let cand = parse_decl(cand_decl);
        if cand.types.len() < sig.types.len() || cand.tail != sig.tail {
            continue;
        }
        let is_longer = cand.types.len() > sig.types.len();
        if is_longer && !cand.defaults.get(sig.types.len()).copied().unwrap_or(false) {
            continue;
        }
        let (mut same, mut compatible) = (true, true);
        for (i, (a, b)) in sig.types.iter().zip(&cand.types).enumerate() {
            if a == b {
                continue;
            }
            same = false;
            if i == 0 && b == b"void" && (a == b"bool" || a == b"float" || is_int(a)) {
                continue;
            }
            if !(is_int(a) && is_enum(b)) && !(is_int(b) && is_enum(a)) {
                compatible = false;
            }
        }
        if same && !is_longer {
            return None;
        }
        if compatible && is_longer {
            longer = Some(n);
            longer_count += 1;
        } else if compatible {
            if matched.is_some() {
                return None;
            }
            matched = Some(n);
        }
    }
    if matched.is_none() && longer_count == 1 {
        matched = longer;
    }
    matched
}

#[derive(Clone, Debug, Default)]
pub(crate) struct Enum {
    pub name: Vec<u8>,
    pub values: Vec<(Vec<u8>, i32)>,
}

fn is_last(s: &[u8]) -> bool {
    s.len() > 9 && s.ends_with(b"_LastEnum")
}

/// Native code uses SOMA's enum values; later games insert names in the middle.
pub(crate) fn rebase_enums(enums: &mut [Enum], base: &[Enum]) {
    for e in enums {
        for b in base {
            if b.name != e.name {
                continue;
            }
            let mut map: BTreeMap<&[u8], i32> = BTreeMap::new();
            for (n, v) in &b.values {
                map.entry(n.as_slice()).or_insert(*v);
            }
            let changed = e.values.iter().fold(false, |acc, (n, v)| {
                acc | map.get(n.as_slice()).is_some_and(|m| m != v)
            });
            if !changed {
                continue;
            }
            let mut next = b.values.iter().fold(0i32, |acc, (n, v)| {
                acc.max(v.wrapping_add(i32::from(!is_last(n))))
            });
            for (n, v) in e.values.iter_mut().filter(|(n, _)| !is_last(n)) {
                *v = match map.get(n.as_slice()) {
                    Some(m) => *m,
                    None => {
                        next = next.wrapping_add(1);
                        next.wrapping_sub(1)
                    }
                };
            }
            for (_, v) in e.values.iter_mut().filter(|(n, _)| is_last(n)) {
                *v = next;
            }
        }
    }
}

pub(crate) const REC_ENUM: u32 = 0;
pub(crate) const REC_ENUM_VALUE: u32 = 1;
pub(crate) const REC_TYPE: u32 = 2;
pub(crate) const REC_BEHAVIOUR: u32 = 3;
pub(crate) const REC_METHOD: u32 = 4;
pub(crate) const REC_PROP: u32 = 5;
pub(crate) const REC_CAST: u32 = 6;
pub(crate) const REC_GLOBAL: u32 = 7;
pub(crate) const REC_GLOBAL_PROP: u32 = 8;

/// One parsed line: record kind, owning type index, two strings and a number.
pub(crate) struct Record<'a> {
    pub kind: u32,
    pub type_idx: usize,
    pub a: Vec<u8>,
    pub b: &'a [u8],
    pub n: i32,
}

/// `std::getline(ss, s, '\t')` pieces: a trailing empty piece is not produced.
fn split_tabs(line: &[u8]) -> Vec<&[u8]> {
    let mut v: Vec<&[u8]> = line.split(|&c| c == b'\t').collect();
    if v.last().is_some_and(|l| l.is_empty()) {
        v.pop();
    }
    v
}

/// Missing fields of a short line read as empty.
fn field<'a>(v: &[&'a [u8]], i: usize) -> &'a [u8] {
    v.get(i).copied().unwrap_or_default()
}

pub(crate) fn load(text: &[u8], mut emit: impl FnMut(Record)) {
    let mut type_idx: BTreeMap<Vec<u8>, usize> = BTreeMap::new();
    let mut types = 0usize;
    let mut cur: Option<usize> = None;
    let mut have_enum = false;
    let mut lines: Vec<&[u8]> = text.split(|&c| c == b'\n').collect();
    if lines.last().is_some_and(|l| l.is_empty()) {
        lines.pop();
    }
    for line in lines {
        if line.is_empty() || line.first() == Some(&b'#') {
            continue;
        }
        let v = split_tabs(line);
        let tag = field(&v, 0);
        let rec = |kind, type_idx, a: Vec<u8>, b, n| Record {
            kind,
            type_idx,
            a,
            b,
            n,
        };
        match tag {
            b"E" => {
                have_enum = true;
                emit(rec(REC_ENUM, 0, field(&v, 1).to_vec(), &[], 0));
            }
            b"V" if have_enum => emit(rec(
                REC_ENUM_VALUE,
                0,
                field(&v, 1).to_vec(),
                &[],
                cfmt::c_atoi(field(&v, 2)),
            )),
            b"T" => {
                let name = field(&v, 1);
                let mut size = cfmt::c_atoi(field(&v, 3));
                if name == b"cVector2f" && size == 0 {
                    size = 8;
                }
                type_idx.insert(name.to_vec(), types);
                types += 1;
                emit(rec(REC_TYPE, 0, name.to_vec(), field(&v, 2), size));
            }
            b"O" => cur = Some(type_idx.get(field(&v, 1)).copied().unwrap_or(0)),
            b"B" => {
                if let Some(t) = cur {
                    emit(rec(
                        REC_BEHAVIOUR,
                        t,
                        field(&v, 1).to_vec(),
                        field(&v, 2),
                        0,
                    ));
                }
            }
            b"M" => {
                if let Some(t) = cur {
                    let d = out_defaults_to_void(&no_auto_handles(field(&v, 1)));
                    emit(rec(REC_METHOD, t, d, &[], 0));
                }
            }
            b"P" => {
                if let Some(t) = cur {
                    emit(rec(
                        REC_PROP,
                        t,
                        field(&v, 1).to_vec(),
                        &[],
                        cfmt::c_atoi(field(&v, 2)),
                    ));
                }
            }
            b"C" => {
                if let Some(t) = cur {
                    emit(rec(REC_CAST, t, field(&v, 1).to_vec(), &[], 0));
                }
            }
            b"G" => {
                let d = out_defaults_to_void(&no_auto_handles(field(&v, 1)));
                emit(rec(REC_GLOBAL, 0, d, &[], 0));
            }
            b"GP" => emit(rec(REC_GLOBAL_PROP, 0, field(&v, 1).to_vec(), &[], 0)),
            _ => {}
        }
    }
}

/// Lookups return null ("not found") instead of a placeholder object.
pub(crate) fn is_lookup_name(name: &[u8]) -> bool {
    find(name, b"By", 0).is_some()
        || name.starts_with(b"Find")
        || find(name, b"Latest", 0).is_some()
        || find(name, b"FromName", 0).is_some()
        || find(name, b"FromID", 0).is_some()
}

pub(crate) const STUB_VOID: u32 = 0;
pub(crate) const STUB_DEFAULT_INSTANCE: u32 = 1;
pub(crate) const STUB_HANDLE: u32 = 2;
pub(crate) const STUB_CONSTRUCT: u32 = 3;

pub(crate) fn stub_return_kind(is_void: bool, inout_ref: bool, is_handle: bool) -> u32 {
    if is_void {
        STUB_VOID
    } else if inout_ref {
        STUB_DEFAULT_INSTANCE
    } else if is_handle {
        STUB_HANDLE
    } else {
        STUB_CONSTRUCT
    }
}

/// Script structs: reference types with a factory that are not backed by HPL2 objects.
pub(crate) fn is_script_struct(kind: &[u8], name: &[u8], has_factory: bool) -> bool {
    const ENGINE: [&[u8]; 5] = [
        b"cBoundingVolume",
        b"cCollideData",
        b"cFrustum",
        b"cAINodeIterator",
        b"cBinaryBuffer",
    ];
    kind == b"ref" && !ENGINE.contains(&name) && has_factory
}

pub(crate) fn is_copy_ctor(params: &[u8], type_name: &[u8]) -> bool {
    let with = |sep: &[u8]| [b"const ".as_slice(), type_name, sep].concat();
    params == with(b" &in") || params == with(b"&in")
}

pub(crate) fn is_primitive_type(name: &[u8], is_enum: &dyn Fn(&[u8]) -> bool) -> bool {
    const PRIM: [&[u8]; 11] = [
        b"bool", b"int", b"uint", b"int8", b"int16", b"int64", b"uint8", b"uint16", b"uint64",
        b"float", b"double",
    ];
    PRIM.contains(&name) || is_enum(name)
}

/// Declarations of the `get_`/`set_` accessors that stand in for a property with an unknown offset.
pub(crate) fn property_accessors(
    decl: &[u8],
    is_enum: &dyn Fn(&[u8]) -> bool,
) -> Option<(Vec<u8>, Option<Vec<u8>>)> {
    let mut d = decl;
    while let Some((&last, rest)) = d.split_last() {
        if last == b' ' || last == b';' {
            d = rest;
        } else {
            break;
        }
    }
    let name_pos = find_last_of(d, b" \t@&", NPOS);
    if name_pos == NPOS {
        return None;
    }
    let name = d.get(name_pos + 1..).unwrap_or_default();
    let mut ty = d.get(..=name_pos).unwrap_or_default();
    while let Some((&b' ', rest)) = ty.split_last() {
        ty = rest;
    }
    let is_const = ty.starts_with(b"const ");
    let bare = if is_const {
        ty.get(6..).unwrap_or_default()
    } else {
        ty
    };
    let is_handle = bare.last() == Some(&b'@');
    let get = [bare, b" get_", name, b"() const property"].concat();
    if is_const {
        return Some((get, None));
    }
    let arg = if is_handle || is_primitive_type(bare, is_enum) {
        bare.to_vec()
    } else {
        [b"const ".as_slice(), bare, b" &in"].concat()
    };
    let set = [b"void set_".as_slice(), name, b"(", &arg, b") property"].concat();
    Some((get, Some(set)))
}

/// Property name of a member declaration.
pub(crate) fn prop_name(decl: &[u8]) -> Vec<u8> {
    let end = find_last_not_of(decl, b" ;");
    let start = find_last_of(decl, b" @&", end);
    substr(decl, start.wrapping_add(1), end.wrapping_sub(start)).to_vec()
}

/// Declaration name (`ParseDecl(decl).msName`).
pub(crate) fn decl_name(decl: &[u8]) -> Vec<u8> {
    parse_decl(decl).name
}

/// Stub call counts, most called first; ties by name descending (`std::sort` on reversed pairs).
pub(crate) fn sort_stub_counts(entries: &mut [(i32, Vec<u8>)]) {
    entries.sort_by(|a, b| b.cmp(a));
}
