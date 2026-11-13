//! Text form of the H21 host traffic: the cassette golden vectors record it from the Rust popups
//! driving the real C++ widgets, and `h21_tests` replays it against a mock host. Widget and
//! callback pointers print as their order of first appearance, so runs are address independent.

use super::h21_popup::{Cmd, OP_NAMES, Ret};
use std::fmt::Write;

#[derive(Default)]
pub struct Ids(Vec<usize>);

impl Ids {
    pub fn id(&mut self, raw: usize) -> usize {
        if raw == 0 {
            return 0;
        }
        match self.0.iter().position(|&x| x == raw) {
            Some(p) => p + 1,
            None => {
                self.0.push(raw);
                self.0.len()
            }
        }
    }
}

pub fn hex(s: &[u32]) -> String {
    let mut o = String::new();
    for (i, c) in s.iter().enumerate() {
        if i != 0 {
            o.push(',');
        }
        let _ = write!(o, "{c:x}");
    }
    o
}

/// # Safety
/// `p` is null or points to `n` code units.
pub unsafe fn units<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if p.is_null() {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

pub fn fmt_cmd(c: &Cmd, ids: &mut Ids) -> String {
    let mut o = format!(
        "> {}",
        OP_NAMES
            .get(usize::try_from(c.op).unwrap_or(usize::MAX))
            .copied()
            .unwrap_or("?")
    );
    for (k, v) in c.i.iter().enumerate().filter(|(_, v)| **v != 0) {
        let _ = write!(o, " i{k}={v}");
    }
    for (k, v) in c.w.iter().enumerate().filter(|(_, v)| **v != 0) {
        let _ = write!(o, " w{k}={}", ids.id(*v));
    }
    for (k, v) in c.f.iter().enumerate().filter(|(_, v)| v.to_bits() != 0) {
        let _ = write!(o, " f{k}={:08x}", v.to_bits());
    }
    for (k, v) in c.p.iter().enumerate().filter(|(_, v)| **v != 0) {
        let _ = write!(o, " p{k}={}", ids.id(*v));
    }
    for (k, (p, n)) in
        c.s.iter()
            .zip(&c.n)
            .enumerate()
            .filter(|(_, (p, _))| !p.is_null())
    {
        // SAFETY: a command's string pointers are valid for its length while it is formatted.
        let _ = write!(o, " s{k}={}", hex(unsafe { units(*p, *n) }));
    }
    o
}

pub fn fmt_ret(r: &Ret, ids: &mut Ids) -> String {
    let mut o = format!("< w={} i={} f=", ids.id(r.w), r.i);
    for (k, v) in r.f.iter().enumerate() {
        let _ = write!(o, "{}{:x}", if k == 0 { "" } else { "," }, v.to_bits());
    }
    o.push_str(" s=");
    for (k, (p, n)) in r.s.iter().zip(&r.n).enumerate() {
        if k != 0 {
            o.push(';');
        }
        if p.is_null() {
            o.push('-');
        } else {
            // SAFETY: the host's string is valid until its next command.
            o.push_str(&hex(unsafe { units(*p, *n) }));
        }
    }
    o
}
