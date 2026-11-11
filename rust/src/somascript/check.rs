//! Script check tool helpers (`SomaScriptCheck.cpp`).

use super::strings::find;

use std::ffi::OsStr;
use std::fs;
use std::os::unix::ffi::OsStrExt;
use std::path::Path;

pub(crate) fn json_escape(s: &[u8]) -> Vec<u8> {
    let mut o = Vec::with_capacity(s.len());
    for &c in s {
        if c == b'"' || c == b'\\' {
            o.push(b'\\');
        }
        if c == b'\n' {
            o.extend_from_slice(b"\\n");
            continue;
        }
        if c < 0x20 {
            continue;
        }
        o.push(c);
    }
    o
}

/// Error text from its `/script/` component on (without the leading slash).
pub(crate) fn from_script_dir(msg: &[u8]) -> &[u8] {
    let start = find(msg, b"/script/", 0).map_or(0, |p| p + 1);
    msg.get(start..).unwrap_or_default()
}

/// `"([^"]+\.hps)"` (case-insensitive) matches in one config file, in order.
fn quoted_scripts(text: &[u8], out: &mut Vec<Vec<u8>>) {
    let mut p = 0;
    while let Some(open) = text
        .get(p..)
        .and_then(|t| t.iter().position(|&c| c == b'"'))
    {
        let open = p + open;
        let Some(close) = text
            .get(open + 1..)
            .and_then(|t| t.iter().position(|&c| c == b'"'))
            .map(|c| open + 1 + c)
        else {
            return;
        };
        let body = text.get(open + 1..close).unwrap_or_default();
        if body.len() > 4
            && body
                .get(body.len() - 4..)
                .is_some_and(|e| e.eq_ignore_ascii_case(b".hps"))
        {
            out.push(body.to_vec());
            p = close + 1;
        } else {
            p = open + 1;
        }
    }
}

/// Sorted, de-duplicated script names quoted in the given config files.
pub(crate) fn config_scripts(files: &[&[u8]]) -> Vec<Vec<u8>> {
    let mut out = Vec::new();
    for f in files {
        quoted_scripts(f, &mut out);
    }
    out.sort();
    out.dedup();
    out
}

/// `.hps` files below `dir` (not entering `maps`), at most four levels deep, in directory order.
pub(crate) fn map_scripts(dir: &[u8], depth: u32, out: &mut Vec<Vec<u8>>) {
    let Ok(rd) = fs::read_dir(Path::new(OsStr::from_bytes(dir))) else {
        return;
    };
    if depth > 3 {
        return;
    }
    for ent in rd.flatten() {
        let name = ent.file_name().as_bytes().to_vec();
        if name == b"maps" {
            continue;
        }
        let full = [dir, b"/", &name].concat();
        if name.len() > 4 && name.ends_with(b".hps") {
            out.push(full);
        } else if ent.file_type().is_ok_and(|t| t.is_dir() || t.is_symlink()) {
            map_scripts(&full, depth + 1, out);
        }
    }
}
