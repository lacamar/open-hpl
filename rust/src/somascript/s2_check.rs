//! S2: `RunSomaScriptCheck` on the Rust path: compile every game script against the recovered API
//! and write the JSON report (`scripts/soma-script-check.sh`).

use super::builder::Builder;
use super::s2_api::{self, Api};
use super::s2_as::{self, Engine};
use super::s2_builder;
use super::s2_entry;
use super::s2_host;
use super::{api, check, s2_strings};
use crate::script::sys::{self, AsMsg};
use std::collections::{BTreeMap, HashSet};
use std::ffi::{CStr, c_char, c_int, c_void};
use std::io::Write;
use std::os::unix::ffi::OsStrExt;
use std::path::Path;

unsafe extern "C" {
    fn ohpl_as_engine_shutdown(e: *mut c_void) -> c_int;
    fn ohpl_as_get_module(e: *mut c_void, name: *const c_char, flag: c_int) -> *mut c_void;
    fn ohpl_as_discard_module(e: *mut c_void, name: *const c_char) -> c_int;
    fn ohpl_as2_module_called_funcs(
        m: *mut c_void,
        cb: unsafe extern "C" fn(*mut c_void, c_int),
        ctx: *mut c_void,
    );
}

#[derive(Default)]
struct Messages {
    errors: Vec<Vec<u8>>,
    context: Vec<u8>,
}

unsafe extern "C" fn on_message(m: *const AsMsg, ctx: *mut c_void) {
    // SAFETY: the engine passes its message; `ctx` is the `Messages` registered in `run`.
    let (Some(m), Some(st)) = (unsafe { m.as_ref() }, unsafe {
        ctx.cast::<Messages>().as_mut()
    }) else {
        return;
    };
    // SAFETY: the engine passes NUL-terminated strings (or null for the section).
    let text = |p: *const c_char| {
        if p.is_null() {
            Vec::new()
        } else {
            unsafe { CStr::from_ptr(p) }.to_bytes().to_vec()
        }
    };
    let message = text(m.message);
    if m.kind == sys::MSG_INFO {
        st.context.clone_from(&message);
    }
    if m.kind == sys::MSG_ERROR {
        let mut e = text(m.section);
        e.extend_from_slice(format!(":{}:{}: ", m.row, m.col).as_bytes());
        e.extend_from_slice(&message);
        if m.row <= 1 {
            e.extend_from_slice(&[b" [".as_slice(), &st.context, b"]"].concat());
        }
        st.errors.push(e);
    }
}

fn path(b: &[u8]) -> &Path {
    Path::new(std::ffi::OsStr::from_bytes(b))
}

fn json(s: &[u8]) -> Vec<u8> {
    check::json_escape(s)
}

struct StubScan<'a> {
    e: Engine,
    seen: HashSet<Vec<u8>>,
    counts: &'a mut BTreeMap<Vec<u8>, i32>,
}

unsafe extern "C" fn on_called(ctx: *mut c_void, id: c_int) {
    // SAFETY: `ctx` is the `StubScan` of `collect_stub_calls`.
    let Some(s) = (unsafe { ctx.cast::<StubScan>().as_mut() }) else {
        return;
    };
    if let Some(f) = s.e.func_by_id(id).filter(|&f| s2_api::is_stub(f)) {
        let name = f.decl(true, false, false);
        if s.seen.insert(name.clone()) {
            *s.counts.entry(name).or_insert(0) += 1;
        }
    }
}

/// Stubbed application functions a module calls, counted once per module.
fn collect_stub_calls(e: Engine, module: *mut c_void, counts: &mut BTreeMap<Vec<u8>, i32>) {
    let mut scan = StubScan {
        e,
        seen: HashSet::new(),
        counts,
    };
    // SAFETY: `scan` outlives the call.
    unsafe {
        ohpl_as2_module_called_funcs(module, on_called, std::ptr::from_mut(&mut scan).cast())
    };
}

fn config_scripts(game_dir: &[u8]) -> Vec<Vec<u8>> {
    const CFGS: [&str; 5] = [
        "EntityTypes.cfg",
        "Modules.cfg",
        "PlayerStates.cfg",
        "game.cfg",
        "Effects.cfg",
    ];
    let texts: Vec<Vec<u8>> = CFGS
        .iter()
        .map(|c| {
            std::fs::read(path(&[game_dir, b"/config/", c.as_bytes()].concat())).unwrap_or_default()
        })
        .collect();
    let refs: Vec<&[u8]> = texts.iter().map(Vec::as_slice).collect();
    check::config_scripts(&refs)
}

fn setup_engine(e: Engine, api: &mut Api, api_text: &[u8]) {
    // SAFETY: `e` is the engine created by `run`.
    unsafe { s2_entry::ohpl_s2_configure_engine(e.0) };
    s2_strings::register_types(e);
    s2_host::register_array(e.0);
    s2_strings::register_array_extras(e);
    api.load(api_text);
    api.register_types(e);
    // SAFETY: as above.
    unsafe { s2_entry::ohpl_s2_register_natives(e.0) };
    api.register(e);
}

fn stderr_lines(prefix: &str, l: &[Vec<u8>]) {
    let mut err = std::io::stderr().lock();
    for s in l {
        let _ = err.write_all(prefix.as_bytes());
        let _ = err.write_all(s);
        let _ = err.write_all(b"\n");
    }
}

pub(crate) fn run(game_dir: &[u8], api_file: &[u8], report: &[u8]) -> c_int {
    // SAFETY: plain call.
    let e = Engine(unsafe { sys::ohpl_as_engine_new() });
    if e.0.is_null() {
        return 1;
    }
    let mut msgs = Box::<Messages>::default();
    // SAFETY: `msgs` outlives the engine (the engine is shut down before it drops).
    unsafe { sys::ohpl_as_set_message_cb(e.0, on_message, std::ptr::from_mut(&mut *msgs).cast()) };
    let Ok(api_text) = std::fs::read(path(api_file)) else {
        eprintln!(
            "cannot read script API '{}'",
            String::from_utf8_lossy(api_file)
        );
        return 1;
    };
    let mut api = Api::default();
    setup_engine(e, &mut api, &api_text);
    if let Some(p) = std::env::var_os("OHPL_S2_DECL_DUMP") {
        let c = s2_as::cs(p.as_bytes());
        // SAFETY: plain call.
        unsafe { s2_as::ohpl_as2_dump_decls(e.0, c.as_ptr()) };
    }
    if let Some(p) = std::env::var_os("OHPL_S2_CORPUS") {
        corpus(e, p.as_bytes());
    }
    stderr_lines("warning: ", api.warnings());
    stderr_lines("remap: ", api.remaps());
    let mut api_errors: Vec<Vec<u8>> = api.errors().to_vec();
    api_errors.append(&mut msgs.errors);

    let builder = Builder::new(game_dir, &s2_host::game_id());
    if let Some(f) = std::env::var_os("OPENHPL_SOMA_SCRIPT_CHECK_FILE") {
        let r = s2_builder::build(&builder, b"", e, b"check_file", f.as_bytes(), None);
        stderr_lines("", &msgs.errors);
        // SAFETY: the engine is released once.
        unsafe { ohpl_as_engine_shutdown(e.0) };
        return c_int::from(r < 0);
    }
    let mut entries = config_scripts(game_dir);
    for s in &mut entries {
        let resolved = builder.resolve(s, &[game_dir, b"/script/"].concat());
        *s = if resolved.is_empty() {
            [game_dir, b"/script/", s.as_slice()].concat()
        } else {
            resolved
        };
    }
    check::map_scripts(&[game_dir, b"/maps"].concat(), 0, &mut entries);

    let mut out: Vec<u8> = b"{\"api_errors\":[".to_vec();
    for (i, s) in api_errors.iter().enumerate() {
        out.extend_from_slice(
            &[
                if i > 0 { b",".as_slice() } else { b"" },
                b"\"",
                &json(s),
                b"\"",
            ]
            .concat(),
        );
    }
    out.extend_from_slice(b"],\"modules\":{");
    let mut stub_modules: BTreeMap<Vec<u8>, i32> = BTreeMap::new();
    let (mut ok, mut missing_n) = (0usize, 0usize);
    for (i, entry) in entries.iter().enumerate() {
        msgs.errors.clear();
        let rel = entry.get(game_dir.len() + 1..).unwrap_or_default();
        let sep: &[u8] = if i > 0 { b"," } else { b"" };
        if std::fs::File::open(path(entry)).is_err() {
            missing_n += 1;
            out.extend_from_slice(
                &[sep, b"\"", &json(rel), b"\":{\"ok\":true,\"missing\":true}"].concat(),
            );
            continue;
        }
        let mut missing = Vec::new();
        let name = format!("check_{i}");
        let r = s2_builder::build(&builder, b"", e, name.as_bytes(), entry, Some(&mut missing));
        if r >= 0 {
            ok += 1;
        }
        out.extend_from_slice(
            &[
                sep,
                b"\"",
                &json(rel),
                b"\":{\"ok\":",
                if r >= 0 { b"true".as_slice() } else { b"false" },
                b",\"missing_include\":\"",
                &json(&missing),
                format!("\",\"error_count\":{},\"errors\":[", msgs.errors.len()).as_bytes(),
            ]
            .concat(),
        );
        for (j, m) in msgs.errors.iter().take(20).enumerate() {
            out.extend_from_slice(
                &[
                    if j > 0 { b",".as_slice() } else { b"" },
                    b"\"",
                    &json(check::from_script_dir(m)),
                    b"\"",
                ]
                .concat(),
            );
        }
        out.extend_from_slice(b"]}");
        let cname = s2_as::cs(name.as_bytes());
        // SAFETY: plain calls; the name is NUL-terminated.
        let module = unsafe { ohpl_as_get_module(e.0, cname.as_ptr(), 0) };
        if !module.is_null() {
            collect_stub_calls(e, module, &mut stub_modules);
        }
        // SAFETY: as above.
        unsafe { ohpl_as_discard_module(e.0, cname.as_ptr()) };
    }
    let total = entries.len() - missing_n;
    let mut stubs: Vec<(i32, Vec<u8>)> =
        stub_modules.iter().map(|(k, v)| (*v, k.clone())).collect();
    api::sort_stub_counts(&mut stubs);
    out.extend_from_slice(b"},\"stub_calls\":{");
    for (i, (n, name)) in stubs.iter().enumerate() {
        out.extend_from_slice(
            &[
                if i > 0 { b",".as_slice() } else { b"" },
                b"\"",
                &json(name),
                format!("\":{n}").as_bytes(),
            ]
            .concat(),
        );
    }
    out.extend_from_slice(
        format!(
            "}},\"summary\":{{\"stubs_referenced\":{},\"modules\":{total},\"compiled\":{ok},\"not_shipped\":{missing_n},\"api_errors\":{}}}}}\n",
            stubs.len(),
            api_errors.len()
        )
        .as_bytes(),
    );
    let _ = std::fs::write(path(report), &out);
    println!(
        "script check: {ok}/{total} modules compile ({missing_n} named in config but not shipped), {} API registration errors -> {}",
        api_errors.len(),
        String::from_utf8_lossy(report)
    );
    // SAFETY: the engine is released once.
    unsafe { ohpl_as_engine_shutdown(e.0) };
    c_int::from(!(ok == total && api_errors.is_empty()))
}

/// Seeded call corpus of the natives plus their side effects (script globals, underwater state).
fn corpus(e: Engine, out: &[u8]) {
    let calls = std::env::var("OHPL_S2_CORPUS_CALLS")
        .ok()
        .and_then(|v| v.parse().ok())
        .unwrap_or(6);
    let c = s2_as::cs(out);
    // SAFETY: plain call.
    unsafe { s2_as::ohpl_as2_call_corpus(e.0, 20_260_101, calls, c.as_ptr()) };
    let vars = super::globals::serialize(&super::globals::lock());
    let mut line = format!("# state vars={}:", vars.len());
    for b in &vars {
        line.push_str(&format!("{b:x},"));
    }
    let underwater = s2_host::misc(s2_host::MISC_UNDERWATER_GET, 0, b"");
    let users = s2_host::misc(s2_host::MISC_UNDERWATER_USERS, 0, b"");
    line.push_str(&format!(" underwater={underwater} users={users}\n"));
    if let Ok(mut f) = std::fs::OpenOptions::new().append(true).open(path(out)) {
        let _ = f.write_all(line.as_bytes());
    }
}

/// # Safety
/// Pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_run_check(
    dir: *const u8,
    dn: usize,
    api: *const u8,
    an: usize,
    report: *const u8,
    rn: usize,
) -> c_int {
    use crate::string_ffi::units;
    // SAFETY: caller contract.
    unsafe { run(units(dir, dn), units(api, an), units(report, rn)) }
}

/// # Safety
/// `builder` is a live `Builder`; `engine` a live engine; strings valid for their lengths; `missing` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_builder_build(
    builder: *const Builder,
    cache: *const u8,
    cn: usize,
    engine: *mut c_void,
    module: *const u8,
    mn: usize,
    entry: *const u8,
    en: usize,
    missing: crate::string_ffi::Sink<u8>,
    ctx: *mut c_void,
) -> c_int {
    use crate::string_ffi::{emit, units};
    // SAFETY: caller contract.
    unsafe {
        let Some(b) = builder.as_ref() else { return -1 };
        let mut m = Vec::new();
        let r = s2_builder::build(
            b,
            units(cache, cn),
            Engine(engine),
            units(module, mn),
            units(entry, en),
            Some(&mut m),
        );
        emit(missing, ctx, &m);
        r
    }
}
