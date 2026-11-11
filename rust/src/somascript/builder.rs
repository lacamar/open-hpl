//! Script include resolution and preprocessing (`SomaScriptBuilder.cpp`): the `.hps` index, include
//! order, compat patches, `[nosave]` bookkeeping and the bytecode cache key.

use super::patches::PATCHES;
use super::strings::find;

use std::collections::{BTreeMap, BTreeSet};
use std::ffi::OsStr;
use std::fs;
use std::os::unix::ffi::OsStrExt;
use std::path::Path;
use std::sync::{Mutex, PoisonError};

static NOSAVE: Mutex<BTreeSet<Vec<u8>>> = Mutex::new(BTreeSet::new());

pub(crate) fn is_nosave(key: &[u8]) -> bool {
    NOSAVE
        .lock()
        .unwrap_or_else(PoisonError::into_inner)
        .contains(key)
}

fn path(b: &[u8]) -> &Path {
    Path::new(OsStr::from_bytes(b))
}

pub(crate) fn lower(s: &[u8]) -> Vec<u8> {
    s.to_ascii_lowercase()
}

pub struct Builder {
    by_rel: BTreeMap<Vec<u8>, Vec<u8>>,
    by_base: BTreeMap<Vec<u8>, Vec<u8>>,
    game: Vec<u8>,
}

#[derive(Default)]
pub(crate) struct Collected {
    pub sections: Vec<(Vec<u8>, Vec<u8>)>,
    pub missing: Vec<u8>,
    pub warnings: Vec<Vec<u8>>,
}

fn index(
    root: &[u8],
    rel: &[u8],
    by_rel: &mut BTreeMap<Vec<u8>, Vec<u8>>,
    by_base: &mut BTreeMap<Vec<u8>, Vec<u8>>,
) {
    let dir = [root, b"/", rel].concat();
    let Ok(rd) = fs::read_dir(path(&dir)) else {
        return;
    };
    for ent in rd.flatten() {
        let name = ent.file_name().as_bytes().to_vec();
        let rel_name = if rel.is_empty() {
            name.clone()
        } else {
            [rel, b"/", &name].concat()
        };
        let full = [root, b"/", &rel_name].concat();
        let Ok(meta) = fs::metadata(path(&full)) else {
            continue;
        };
        if meta.is_dir() {
            index(root, &rel_name, by_rel, by_base);
        } else {
            let lname = lower(&name);
            if lname.len() > 4 && lname.ends_with(b".hps") {
                by_rel.insert(lower(&rel_name), full.clone());
                by_base.entry(lname).or_insert(full);
            }
        }
    }
}

fn is_space(c: u8) -> bool {
    matches!(c, b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r')
}

fn is_word(c: u8) -> bool {
    c.is_ascii_alphanumeric() || c == b'_'
}

/// `^\s*(?:shared\s+|abstract\s+|mixin\s+)*class\s+(\w+)`
fn class_name(line: &[u8]) -> Option<&[u8]> {
    let skip_ws = |mut p: usize| {
        while line.get(p).copied().is_some_and(is_space) {
            p += 1;
        }
        p
    };
    let mut p = skip_ws(0);
    'modifiers: loop {
        for kw in [b"shared".as_slice(), b"abstract", b"mixin"] {
            if line.get(p..p + kw.len()) == Some(kw) {
                let q = skip_ws(p + kw.len());
                if q > p + kw.len() {
                    p = q;
                    continue 'modifiers;
                }
            }
        }
        break;
    }
    if line.get(p..p + 5) != Some(b"class".as_slice()) {
        return None;
    }
    let q = skip_ws(p + 5);
    if q == p + 5 {
        return None;
    }
    let mut e = q;
    while line.get(e).copied().is_some_and(is_word) {
        e += 1;
    }
    (e > q).then(|| line.get(q..e).unwrap_or_default())
}

fn find_first_not_of(s: &[u8], set: &[u8], from: usize) -> usize {
    s.get(from..)
        .and_then(|t| t.iter().position(|c| !set.contains(c)))
        .map_or(usize::MAX, |p| p + from)
}

fn find_byte(s: &[u8], c: u8, from: usize) -> usize {
    s.get(from..)
        .and_then(|t| t.iter().position(|&x| x == c))
        .map_or(usize::MAX, |p| p + from)
}

fn find_first_of(s: &[u8], set: &[u8], from: usize) -> usize {
    s.get(from..)
        .and_then(|t| t.iter().position(|c| set.contains(c)))
        .map_or(usize::MAX, |p| p + from)
}

fn find_last_not_of(s: &[u8], set: &[u8], upto: usize) -> usize {
    let end = upto.saturating_add(1).min(s.len());
    s.get(..end)
        .and_then(|t| t.iter().rposition(|c| !set.contains(c)))
        .unwrap_or(usize::MAX)
}

fn find_last_of(s: &[u8], set: &[u8], upto: usize) -> usize {
    let end = upto.saturating_add(1).min(s.len());
    s.get(..end)
        .and_then(|t| t.iter().rposition(|c| set.contains(c)))
        .unwrap_or(usize::MAX)
}

fn replace_all(code: &mut Vec<u8>, from: &[u8], to: &[u8]) {
    let mut p = find(code, from, 0);
    while let Some(at) = p {
        let end = (at + from.len()).min(code.len());
        code.splice(at..end, to.iter().copied());
        p = find(code, from, at + to.len());
    }
}

pub(crate) fn apply_patches(
    code: &mut Vec<u8>,
    file: &[u8],
    game: &[u8],
    warnings: &mut Vec<Vec<u8>>,
) {
    let lfile = lower(file);
    let (is_soma, is_bunker) = (game == b"soma", game == b"bunker");
    for patch in PATCHES {
        let [pat_file, from, to] = patch.map(str::as_bytes);
        let suffix = if let Some(s) = pat_file.strip_prefix(b"soma:") {
            if !is_soma {
                continue;
            }
            s
        } else if let Some(s) = pat_file.strip_prefix(b"!soma:") {
            if is_soma {
                continue;
            }
            s
        } else {
            pat_file
        };
        if !lfile.ends_with(suffix) {
            continue;
        }
        let mut to = to.to_vec();
        let ms: &[u8] = if is_bunker {
            b", false, false, true, \"\", cString_To16Char(\"\"), true"
        } else {
            b""
        };
        let sl: &[u8] = if is_bunker {
            b", false, true, true"
        } else {
            b""
        };
        replace_all(&mut to, b"$MS", ms);
        replace_all(&mut to, b"$SL", sl);
        let mut hits = 0;
        let mut p = find(code, from, 0);
        while let Some(at) = p {
            let end = (at + from.len()).min(code.len());
            code.splice(at..end, to.iter().copied());
            hits += 1;
            p = find(code, from, at + to.len());
        }
        if hits == 0 {
            let shown = from.get(..60.min(from.len())).unwrap_or_default();
            let cut = shown.iter().position(|&c| c == 0).unwrap_or(shown.len());
            let mut w = b"Script compat patch for '".to_vec();
            w.extend_from_slice(suffix);
            w.extend_from_slice(b"' found no '");
            w.extend_from_slice(shown.get(..cut).unwrap_or_default());
            w.extend_from_slice(b"'\n");
            warnings.push(w);
        }
    }
}

/// Blanks `#include` lines and `[nosave]`/`[volatile]` attributes (keeping line numbers) and returns
/// the quoted include names in order.
pub(crate) fn preprocess(code: &mut [u8]) -> Vec<Vec<u8>> {
    let mut includes = Vec::new();
    let mut class: Vec<u8> = Vec::new();
    let mut pos = 0usize;
    while pos < code.len() {
        let mut end = find_byte(code, b'\n', pos);
        if end == usize::MAX {
            end = code.len();
        }
        let mut first = find_first_not_of(code, b" \t", pos);
        let line = code.get(pos..end).unwrap_or_default();
        if find(line, b"class", 0).is_some()
            && let Some(n) = class_name(line)
        {
            class = n.to_vec();
        }
        while first < end && code.get(first) == Some(&b'[') {
            let close = find_byte(code, b']', first);
            if close == usize::MAX || close >= end {
                break;
            }
            let value = code.get(first + 1..close).unwrap_or_default();
            let decl_end = find_first_of(code, b";=(", close);
            if (value == b"nosave" || value == b"volatile") && decl_end != usize::MAX {
                let name_end = find_last_not_of(code, b" \t\r\n", decl_end - 1);
                let name_start = find_last_of(code, b" \t\r\n&@", name_end);
                if name_end != usize::MAX && name_start != usize::MAX && name_start < name_end {
                    let mut key = class.clone();
                    key.extend_from_slice(b"::");
                    key.extend_from_slice(code.get(name_start + 1..=name_end).unwrap_or_default());
                    NOSAVE
                        .lock()
                        .unwrap_or_else(PoisonError::into_inner)
                        .insert(key);
                }
            }
            for c in code.get_mut(first..=close).unwrap_or_default() {
                *c = b' ';
            }
            first = find_first_not_of(code, b" \t", close + 1);
        }
        if first < end && code.get(first..first + 8) == Some(b"#include".as_slice()) {
            let q1 = find_byte(code, b'"', first);
            let q2 = if q1 == usize::MAX {
                q1
            } else {
                find_byte(code, b'"', q1 + 1)
            };
            if q2 != usize::MAX && q2 < end {
                includes.push(code.get(q1 + 1..q2).unwrap_or_default().to_vec());
            }
            for c in code.get_mut(pos..end).unwrap_or_default() {
                if *c != b'\r' {
                    *c = b' ';
                }
            }
        }
        pos = end + 1;
    }
    includes
}

impl Builder {
    pub(crate) fn new(game_dir: &[u8], game: &[u8]) -> Self {
        let mut b = Self {
            by_rel: BTreeMap::new(),
            by_base: BTreeMap::new(),
            game: game.to_vec(),
        };
        index(
            &[game_dir, b"/script"].concat(),
            b"",
            &mut b.by_rel,
            &mut b.by_base,
        );
        b
    }

    pub(crate) fn resolve(&self, include: &[u8], from_file: &[u8]) -> Vec<u8> {
        let key: Vec<u8> = lower(include)
            .into_iter()
            .map(|c| if c == b'\\' { b'/' } else { c })
            .collect();
        if let Some(p) = self.by_rel.get(&key) {
            return p.clone();
        }
        let dir_len = from_file
            .iter()
            .rposition(|&c| c == b'/')
            .map_or(0, |p| p + 1);
        let candidate = [from_file.get(..dir_len).unwrap_or_default(), include].concat();
        if fs::File::open(path(&candidate)).is_ok() {
            return candidate;
        }
        let base = match key.iter().rposition(|&c| c == b'/') {
            Some(p) => key.get(p + 1..).unwrap_or_default(),
            None => &key,
        };
        self.by_base.get(base).cloned().unwrap_or_default()
    }

    fn add_file(&self, out: &mut Collected, file: &[u8], included: &mut BTreeSet<Vec<u8>>) -> bool {
        if !included.insert(file.to_vec()) {
            return true;
        }
        let Ok(mut f) = fs::File::open(path(file)) else {
            return false;
        };
        let mut code = Vec::new();
        // a directory opens but reads nothing, like the C++ stream
        let _ = std::io::Read::read_to_end(&mut f, &mut code);
        apply_patches(&mut code, file, &self.game, &mut out.warnings);
        let includes = preprocess(&mut code);
        out.sections.push((file.to_vec(), code));
        for inc in includes {
            let p = self.resolve(&inc, file);
            if p.is_empty() {
                if out.missing.is_empty() {
                    out.missing = [inc.as_slice(), b" (from ", file, b")"].concat();
                }
                continue;
            }
            self.add_file(out, &p, included);
        }
        true
    }

    /// Sections of `entry` and its includes in `AddScriptSection` order, or `None` if it cannot be opened.
    pub(crate) fn collect(&self, entry: &[u8]) -> Option<Collected> {
        let mut out = Collected::default();
        let mut included = BTreeSet::new();
        self.add_file(&mut out, entry, &mut included).then_some(out)
    }
}

pub(crate) fn fnv(data: &[u8], hash: u64) -> u64 {
    data.iter()
        .fold(hash, |h, &c| (h ^ u64::from(c)).wrapping_mul(1099511628211))
}

pub(crate) const FNV_BASIS: u64 = 14695981039346656037;

/// Bytecode cache key: the binary's size and mtime, then every section's name (with NUL) and text.
pub(crate) fn cache_key(
    exe_size: u64,
    mtime_sec: i64,
    mtime_nsec: i64,
    sections: &[(Vec<u8>, Vec<u8>)],
) -> String {
    let mut h = fnv(
        &[mtime_sec.to_ne_bytes(), mtime_nsec.to_ne_bytes()].concat(),
        FNV_BASIS,
    );
    h = fnv(&exe_size.to_ne_bytes(), h);
    for (name, text) in sections {
        h = fnv(text, fnv(&[name.as_slice(), &[0]].concat(), h));
    }
    format!("{h:016x}")
}

pub(crate) fn cache_file_name(entry: &[u8]) -> String {
    format!("{:016x}.asb", fnv(entry, FNV_BASIS))
}
