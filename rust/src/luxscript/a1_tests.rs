use super::a1::{Arg, HostFn, set_host};
use super::a1_table::table;
use super::fns::{self, ops};
use std::collections::HashMap;
use std::ffi::c_void;
use std::path::PathBuf;
use std::sync::Mutex;

fn golden(name: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests/golden/A1")
        .join(name)
}

#[test]
fn declarations_match_the_cpp_registrations() {
    let got: String = table()
        .iter()
        .map(|(decl, name, _)| format!("{decl} -> {name}\n"))
        .collect();
    let want = std::fs::read_to_string(golden("decls.txt")).unwrap();
    assert_eq!(got, want);
    assert_eq!(table().len(), 322);
}

#[test]
fn host_ops_match_cpp() {
    let cpp = std::fs::read_to_string(
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../amnesia/src/game/LuxScriptHostA1.cpp"),
    )
    .unwrap();
    let body = cpp
        .split("enum OhplA1Op\n{\n")
        .nth(1)
        .and_then(|s| s.split("\n};").next())
        .unwrap();
    let names: Vec<&str> = body
        .lines()
        .map(|l| l.trim().trim_end_matches(','))
        .collect();
    assert_eq!(names, ops::NAMES);
    for n in ops::NAMES {
        assert!(cpp.contains(&format!("case {n}:")), "no host case for {n}");
    }
}

/// A host with one map holding script variables and named entities.
// boxed so the pointers handed to scripts stay put
#[allow(clippy::vec_box, clippy::box_collection)]
#[derive(Default)]
struct Fake {
    vars: HashMap<Vec<u8>, Box<Vec<u8>>>,
    ents: Vec<(Vec<u8>, i32)>,
    log: Vec<String>,
    names: Vec<Box<Vec<u8>>>,
}

static FAKE: Mutex<Option<Fake>> = Mutex::new(None);
static SERIAL: Mutex<()> = Mutex::new(());

unsafe extern "C" fn fake_host(op: u32, args: *const Arg, n: usize, ret: *mut Arg) {
    // SAFETY: the caller passes `n` arguments and a writable result.
    let (a, r) = unsafe { (std::slice::from_raw_parts(args, n), &mut *ret) };
    let name =
        |i: usize| unsafe { std::slice::from_raw_parts(a[i].p.cast::<u8>(), a[i].n) }.to_vec();
    let mut g = FAKE.lock().unwrap();
    let f = g.as_mut().unwrap();
    let op = ops::NAMES[op as usize];
    f.log.push(op.to_string());
    match op {
        "MAP_EXISTS" => *r = Arg::b(true),
        "MAP_VAR" | "GLOBAL_VAR" => {
            r.p = f.vars.get(&name(0)).map_or(std::ptr::null(), |v| {
                std::ptr::from_ref::<Vec<u8>>(v).cast()
            });
        }
        "VAR_SET" => {
            // SAFETY: the script passes a variable pointer from MAP_VAR.
            *unsafe { &mut *a[0].p.cast_mut().cast::<Vec<u8>>() } = name(1);
        }
        "TEMP_STR" => {
            f.names.push(Box::new(name(1)));
            r.p = std::ptr::from_ref::<Vec<u8>>(f.names.last().unwrap()).cast();
        }
        "MAP_ENTITIES" => *r = Arg::i(f.ents.len() as i32),
        "MAP_ENTITY_NTH" => r.p = (a[0].as_i() as usize + 1) as *const c_void,
        "ENT_IS_TYPE" => *r = Arg::b(true),
        "ENT_NAME" => {
            f.names
                .push(Box::new(f.ents[a[0].p as usize - 1].0.clone()));
            r.p = std::ptr::from_ref::<Vec<u8>>(f.names.last().unwrap()).cast();
        }
        _ => {}
    }
}

fn with_fake<T>(fake: Fake, f: impl FnOnce() -> T) -> (T, Fake) {
    let _serial = SERIAL
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    *FAKE.lock().unwrap() = Some(fake);
    let h: HostFn = fake_host;
    set_host(Some(h));
    let out = f();
    set_host(None);
    let fake = FAKE.lock().unwrap().take().unwrap();
    (out, fake)
}

fn sv(s: &str) -> *const c_void {
    Box::leak(Box::new(s.as_bytes().to_vec())) as *const Vec<u8> as *const c_void
}

#[test]
fn local_vars_round_trip_through_the_host() {
    let mut fake = Fake::default();
    fake.vars.insert(b"N".to_vec(), Box::default());
    let (got, _) = with_fake(fake, || unsafe {
        fns::misc::SetLocalVarInt(sv("N"), 5);
        fns::misc::AddLocalVarInt(sv("N"), 3);
        fns::misc::AddLocalVarFloat(sv("N"), 0.5);
        (
            fns::misc::GetLocalVarInt(sv("N")),
            fns::misc::GetLocalVarFloat(sv("N")),
        )
    });
    assert_eq!(got, (8, 8.5));
}

#[test]
fn missing_local_var_reports_and_reads_empty() {
    let (got, fake) = with_fake(Fake::default(), || unsafe {
        fns::misc::GetLocalVarInt(sv("Nope"))
    });
    assert_eq!(got, 0);
    assert!(fake.log.contains(&"ERROR".to_string()));
}

#[test]
fn wildcard_selects_entities_by_name_parts() {
    let fake = Fake {
        ents: vec![
            (b"door_a".to_vec(), 0),
            (b"key_b".to_vec(), 0),
            (b"door_c".to_vec(), 0),
        ],
        ..Fake::default()
    };
    let (got, _) = with_fake(fake, || {
        fns::entities(b"door*", 0, -1).map_or(0, |l| l.len())
    });
    assert_eq!(got, 2);
}

fn parse_args(s: &str) -> Vec<String> {
    let (mut out, mut cur, mut quoted, mut esc) = (Vec::new(), String::new(), false, false);
    for c in s.chars() {
        match (quoted, esc, c) {
            (true, true, _) => {
                cur.push('\\');
                cur.push(c);
                esc = false;
            }
            (true, false, '\\') => esc = true,
            (true, false, '"') => quoted = false,
            (false, _, '"') => quoted = true,
            (false, _, ',') => out.push(std::mem::take(&mut cur)),
            _ => cur.push(c),
        }
    }
    if !cur.is_empty() || !out.is_empty() {
        out.push(cur);
    }
    out
}

fn float_arg(a: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(&a[1..], 16).unwrap())
}

/// Replays the variable calls of every recorded real-engine trace against the Rust bodies: reads of
/// a variable the trace itself wrote must return what the C++ returned.
#[test]
fn recorded_variable_calls_replay() {
    let dir = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("tests/golden/A1b");
    let (mut checked, mut files) = (0, 0);
    for entry in std::fs::read_dir(dir).unwrap() {
        let text = std::fs::read_to_string(entry.unwrap().path()).unwrap();
        files += 1;
        let mut fake = Fake::default();
        let mut written = std::collections::HashSet::new();
        for line in text.lines() {
            let Some((call, ret)) = line
                .split_once(")=")
                .map(|(c, r)| (c, Some(r)))
                .or(line.strip_suffix(')').map(|c| (c, None)))
            else {
                continue;
            };
            let Some((name, args)) = call.split_once('(') else {
                continue;
            };
            let a = parse_args(args);
            let is_var = ["LocalVar", "GlobalVar"].iter().any(|k| name.contains(k));
            if !is_var {
                continue;
            }
            let key = a[0].clone().into_bytes();
            let scoped = (name.contains("Global"), key.clone());
            fake.vars.entry(key.clone()).or_default();
            let want = ret.map(str::to_string);
            let (got, back) = with_fake(fake, || unsafe {
                let k = sv(&a[0]);
                match name {
                    "SetLocalVarInt" | "SetGlobalVarInt" => {
                        (if name.contains("Global") {
                            fns::misc::SetGlobalVarInt
                        } else {
                            fns::misc::SetLocalVarInt
                        })(k, a[1].parse().unwrap());
                        None
                    }
                    "SetLocalVarFloat" | "SetGlobalVarFloat" => {
                        (if name.contains("Global") {
                            fns::misc::SetGlobalVarFloat
                        } else {
                            fns::misc::SetLocalVarFloat
                        })(k, float_arg(&a[1]));
                        None
                    }
                    "AddLocalVarInt" | "AddGlobalVarInt" => {
                        (if name.contains("Global") {
                            fns::misc::AddGlobalVarInt
                        } else {
                            fns::misc::AddLocalVarInt
                        })(k, a[1].parse().unwrap());
                        None
                    }
                    "AddLocalVarFloat" | "AddGlobalVarFloat" => {
                        (if name.contains("Global") {
                            fns::misc::AddGlobalVarFloat
                        } else {
                            fns::misc::AddLocalVarFloat
                        })(k, float_arg(&a[1]));
                        None
                    }
                    "GetLocalVarInt" => Some(fns::misc::GetLocalVarInt(k).to_string()),
                    "GetGlobalVarInt" => Some(fns::misc::GetGlobalVarInt(k).to_string()),
                    "GetLocalVarFloat" => {
                        Some(format!("f{:08x}", fns::misc::GetLocalVarFloat(k).to_bits()))
                    }
                    _ => None,
                }
            });
            fake = back;
            if name.ends_with("String") {
                written.remove(&scoped);
            } else if name.starts_with("Set") {
                written.insert(scoped);
            } else if written.contains(&scoped)
                && let (Some(g), Some(w)) = (got, want)
            {
                assert_eq!(g, w, "{line}");
                checked += 1;
            }
        }
    }
    assert_eq!(files, 53);
    assert!(checked > 100, "{checked}");
    println!("replayed variable reads: {checked}");
}
