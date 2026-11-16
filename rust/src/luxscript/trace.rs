//! OHPL_A1B_TRACE=<file>: one line per script-function call, `Name(args)=ret`, written after the call returns.

use super::a1::{Arg, raw_bytes};
use std::fmt::Write as _;
use std::fs::File;
use std::io::Write;
use std::sync::{Mutex, OnceLock};

static OUT: OnceLock<Option<Mutex<File>>> = OnceLock::new();

fn out() -> Option<&'static Mutex<File>> {
    OUT.get_or_init(|| {
        let p = std::env::var_os("OHPL_A1B_TRACE")?;
        File::create(p).ok().map(Mutex::new)
    })
    .as_ref()
}

pub fn enabled() -> bool {
    out().is_some()
}

fn show(kind: char, a: &Arg, s: &mut String) {
    let _ = match kind {
        's' => {
            let b = if a.p.is_null() {
                &[][..]
            } else {
                // SAFETY: string arguments carry their own length.
                unsafe { std::slice::from_raw_parts(a.p.cast::<u8>(), a.n) }
            };
            write!(s, "{:?}", String::from_utf8_lossy(b))
        }
        // SAFETY: a returned `string &` points to a live script string.
        'p' => write!(
            s,
            "{:?}",
            String::from_utf8_lossy(unsafe { raw_bytes(a.p) })
        ),
        'f' => write!(s, "f{:08x}", a.as_f().to_bits()),
        'i' => write!(s, "{}", a.as_i()),
        'b' => write!(s, "{}", u8::from(a.as_b())),
        _ => Ok(()),
    };
}

pub fn line(name: &str, kinds: &str, args: &[Arg], rk: char, ret: &Arg) {
    let Some(f) = out() else { return };
    let mut s = String::from(name);
    s.push('(');
    for (i, (k, a)) in kinds.chars().zip(args).enumerate() {
        if i > 0 {
            s.push(',');
        }
        show(k, a, &mut s);
    }
    s.push(')');
    if rk != 'v' {
        s.push('=');
        show(rk, ret, &mut s);
    }
    s.push('\n');
    if let Ok(mut g) = f.lock() {
        let _ = g.write_all(s.as_bytes());
    }
}
