//! Script runtime bookkeeping (`SomaScriptRuntime.cpp`): API file lookup, callback declaration
//! forms, `Exec` module choice and the stub report. The engine calls stay in C++.

use super::api::sort_stub_counts;
use super::strings::find;

use std::ffi::OsStr;
use std::fs::File;
use std::os::unix::ffi::OsStrExt;
use std::path::Path;

pub(crate) struct ApiLookup<'a> {
    pub game: &'a [u8],
    pub current_game: &'a [u8],
    pub env: Option<&'a [u8]>,
    pub exe: Option<&'a [u8]>,
    pub source_dir: Option<&'a [u8]>,
}

pub(crate) fn api_candidates(q: &ApiLookup) -> Vec<Vec<u8>> {
    let is_current = q.game == q.current_game;
    let mut v: Vec<Vec<u8>> = Vec::new();
    if let Some(env) = q.env.filter(|_| is_current) {
        v.push(env.to_vec());
    }
    if let Some(exe) = q.exe.filter(|e| !e.is_empty()) {
        let dir_len = exe.iter().rposition(|&c| c == b'/').map_or(0, |p| p + 1);
        let dir = exe.get(..dir_len).unwrap_or_default();
        if is_current {
            v.push([dir, b"script_api.txt"].concat());
        }
        v.push([dir, b"../share/open-hpl/", q.game, b"/script_api.txt"].concat());
    }
    v.push(
        [
            b"/usr/share/open-hpl/".as_slice(),
            q.game,
            b"/script_api.txt",
        ]
        .concat(),
    );
    if let Some(src) = q.source_dir {
        v.push([src, b"/", q.game, b"/data/script_api.txt"].concat());
    }
    v
}

/// First candidate that opens; empty if none.
pub(crate) fn find_api_file(q: &ApiLookup) -> Vec<u8> {
    api_candidates(q)
        .into_iter()
        .find(|c| File::open(Path::new(OsStr::from_bytes(c))).is_ok())
        .unwrap_or_default()
}

/// `$Func` names a global function of the object's module; the marker is not part of the declaration.
pub(crate) fn split_global_marker(decl: &[u8]) -> (bool, Vec<u8>) {
    match decl.iter().position(|&c| c == b'$') {
        None => (false, decl.to_vec()),
        Some(p) => {
            let mut d = decl.to_vec();
            d.remove(p);
            (true, d)
        }
    }
}

pub(crate) struct CallbackDecls {
    pub global: bool,
    pub func: Vec<u8>,
    pub with_arg: Vec<u8>,
    pub no_arg: Vec<u8>,
}

pub(crate) fn callback_decls(name: &[u8]) -> CallbackDecls {
    let global = name.len() > 1 && name.first() == Some(&b'$');
    let func = if global {
        name.get(1..).unwrap_or_default()
    } else {
        name
    };
    CallbackDecls {
        global,
        func: func.to_vec(),
        with_arg: [b"void ".as_slice(), func, b"(const tString &in)"].concat(),
        no_arg: [b"void ".as_slice(), func, b"()"].concat(),
    }
}

pub(crate) fn exec_source(code: &[u8]) -> Vec<u8> {
    [b"void __exec() {\n".as_slice(), code, b"\n}"].concat()
}

pub(crate) fn exec_message(msg: &[u8], row: i32, col: i32) -> Vec<u8> {
    [msg, format!(" ({row}:{col})\n").as_bytes()].concat()
}

pub(crate) fn script_files_error(keys: &[&[u8]]) -> Vec<u8> {
    let mut e = b"script files:".to_vec();
    for k in keys {
        e.push(b' ');
        e.extend_from_slice(k);
    }
    e
}

/// First loaded module whose file name contains `needle`.
pub(crate) fn pick_module(modules: &[(&[u8], bool)], needle: &[u8]) -> Option<usize> {
    modules
        .iter()
        .position(|(key, loaded)| *loaded && find(key, needle, 0).is_some())
}

/// Report lines: the total, then the `top` most called stubs.
pub(crate) fn stub_report(counts: &[(&[u8], i32)], top: i32) -> Vec<Vec<u8>> {
    let mut v: Vec<(i32, Vec<u8>)> = counts.iter().map(|(n, c)| (*c, n.to_vec())).collect();
    sort_stub_counts(&mut v);
    let mut lines = vec![
        format!(
            "SOMA script: {} unimplemented API functions called\n",
            v.len()
        )
        .into_bytes(),
    ];
    for (count, name) in v.iter().take(usize::try_from(top).unwrap_or(0)) {
        lines.push([format!("  {count:6}  ").as_bytes(), name, b"\n"].concat());
    }
    lines
}
