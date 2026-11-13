use super::a1::{Arg, Func, HostFn, OP_WARNING, set_host};
use super::a1_gen::{HOST_OPS, OP_NAMES, invoke, table};
use std::ffi::c_void;
use std::fmt::Write as _;
use std::path::PathBuf;
use std::sync::Mutex;

static TRACE: Mutex<String> = Mutex::new(String::new());
static OPS: Mutex<Vec<(&'static str, &'static str, char)>> = Mutex::new(Vec::new());

fn golden(name: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests/golden/A1")
        .join(name)
}

fn show(kind: char, a: &Arg) -> String {
    match kind {
        's' => {
            let b = if a.p.is_null() {
                &[][..]
            } else {
                // SAFETY: the recorder is only called with live slices from the corpus.
                unsafe { std::slice::from_raw_parts(a.p.cast::<u8>(), a.n) }
            };
            format!("{:?}", String::from_utf8_lossy(b))
        }
        'f' => format!("f{:08x}", a.as_f().to_bits()),
        'i' => a.as_i().to_string(),
        'b' => u8::from(a.as_b()).to_string(),
        _ => "?".into(),
    }
}

unsafe extern "C" fn recorder(op: u32, args: *const Arg, n: usize, ret: *mut Arg) {
    let mut t = TRACE.lock().unwrap();
    if op == OP_WARNING {
        // SAFETY: one string argument.
        let a = unsafe { *args };
        let _ = writeln!(t, "  host warning {}", show('s', &a));
        return;
    }
    let ops = OPS.lock().unwrap();
    let (name, kinds, r) = ops[op as usize];
    // SAFETY: `n` arguments from the caller.
    let a = unsafe { std::slice::from_raw_parts(args, n) };
    assert_eq!(kinds.len(), n, "{name}");
    let shown: Vec<String> = kinds.chars().zip(a).map(|(k, a)| show(k, a)).collect();
    let _ = writeln!(t, "  host {op} {name}({})", shown.join(","));
    // SAFETY: `ret` is the caller's slot.
    let ret = unsafe { &mut *ret };
    match r {
        'f' => *ret = Arg::f(op as f32 + 0.5),
        'i' => *ret = Arg::i(op as i32 + 7),
        'b' => *ret = Arg::b(op.is_multiple_of(2)),
        'p' => ret.p = std::ptr::from_ref(&0u8).cast(),
        _ => {}
    }
}

struct Rng(u64);

impl Rng {
    fn next(&mut self) -> u64 {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        self.0
    }

    fn pick<'a, T>(&mut self, v: &'a [T]) -> &'a T {
        &v[(self.next() % v.len() as u64) as usize]
    }
}

const STRINGS: [&[u8]; 8] = [
    b"",
    b"a",
    b"Door_1",
    b"wild*",
    b"two words",
    "\u{c5}ge".as_bytes(),
    b"x\0y",
    b"Body_Mesh_01",
];
const FLOATS: [u32; 9] = [
    0x0000_0000,
    0x8000_0000,
    0x3f80_0000,
    0xbfc0_0000,
    0x3dcc_cccd,
    0x42c8_0000,
    0x3a83_126f,
    0x7f7f_ffff,
    0x7fc0_0000,
];
const INTS: [i32; 7] = [-1, 0, 1, 2, 7, 255, -1000];

fn corpus() -> String {
    let t = table();
    *OPS.lock().unwrap() = {
        let mut v = vec![("", "", 'v'); HOST_OPS];
        for f in &t {
            if let Some(slot) = v.get_mut(f.op as usize).filter(|s| s.0.is_empty()) {
                *slot = (f.name, f.kinds, f.ret);
            }
        }
        v
    };
    TRACE.lock().unwrap().clear();
    let host: HostFn = recorder;
    set_host(Some(host));
    let mut rng = Rng(0xA1A1_A1A1_A1A1_A1A1);
    let mut out = String::new();
    for (idx, f) in t.iter().enumerate() {
        for k in 0..4 {
            let strs: Vec<Vec<u8>> = f
                .kinds
                .chars()
                .map(|_| rng.pick(&STRINGS).to_vec())
                .collect();
            let mut args = Vec::new();
            let mut shown = Vec::new();
            for (c, s) in f.kinds.chars().zip(&strs) {
                let a = match c {
                    's' => Arg {
                        p: std::ptr::from_ref(s).cast::<c_void>(),
                        n: s.len(),
                        u: 0,
                    },
                    'f' => Arg::f(f32::from_bits(*rng.pick(&FLOATS))),
                    'i' => Arg::i(*rng.pick(&INTS)),
                    _ => Arg::b(rng.next() & 1 == 1),
                };
                shown.push(if c == 's' {
                    format!("{:?}", String::from_utf8_lossy(s))
                } else {
                    show(c, &a)
                });
                args.push(a);
            }
            TRACE.lock().unwrap().clear();
            // SAFETY: string arguments are `Vec<u8>` test strings.
            let r = unsafe { invoke(idx, &args) };
            let _ = writeln!(
                out,
                "{idx}.{k} {}({}) = {}",
                f.name,
                shown.join(","),
                match f.ret {
                    'p' => "ptr".into(),
                    'v' => "-".into(),
                    c => show(c, &r),
                }
            );
            out.push_str(&TRACE.lock().unwrap());
        }
    }
    set_host(None);
    out
}

#[test]
fn declarations_match_the_cpp_registrations() {
    let t = table();
    let got: String = t
        .iter()
        .map(|f: &Func| format!("{} -> {}\n", f.decl, f.name))
        .collect();
    let want = std::fs::read_to_string(golden("decls.txt")).unwrap();
    assert_eq!(got, want);
    assert_eq!(t.len(), 322);
    assert_eq!(OP_NAMES.len(), HOST_OPS);
}

#[test]
fn host_effects_match_golden() {
    let got = corpus();
    let path = golden("trace.txt");
    if std::env::var_os("A1_BLESS").is_some() {
        std::fs::write(&path, &got).unwrap();
    }
    let want = std::fs::read_to_string(&path).unwrap();
    assert!(got == want, "trace differs from {}", path.display());
}
