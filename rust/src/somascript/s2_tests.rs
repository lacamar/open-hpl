//! Replays the C++ call corpus (`tests/golden/S2/corpus_soma.txt`, recorded from the C++ natives with
//! `OHPL_S2_CORPUS`) through the Rust natives with mock frames: numeric and POD value-type natives.

use super::s2_as::{Engine, Frame, testreg};
use super::s2_math;
use std::collections::HashMap;
use std::ffi::c_void;

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn unhex(s: &str) -> Vec<u8> {
    (0..s.len() / 2)
        .filter_map(|i| u8::from_str_radix(s.get(2 * i..2 * i + 2)?, 16).ok())
        .collect()
}

fn norm_param(p: &str) -> String {
    let p = p.split('=').next().unwrap_or_default().trim();
    let mut toks: Vec<&str> = p.split_whitespace().collect();
    if toks.len() > 1
        && toks
            .last()
            .is_some_and(|t| !t.starts_with('&') && !t.contains('&'))
        && toks.get(toks.len() - 2) != Some(&"const")
    {
        toks.pop();
    }
    toks.concat()
}

/// `TYPE::name(params)` with parameter names and defaults dropped.
fn key(ty: Option<&str>, decl: &str) -> Option<String> {
    let open = decl.find('(')?;
    let close = decl.rfind(')')?;
    let head = decl.get(..open)?.trim_end();
    let name = head.rsplit([' ', ':', '&']).next()?;
    let ty = match ty {
        Some(t) => t.to_string(),
        None => head.rfind("::").map_or(String::new(), |i| {
            head.get(..i)
                .unwrap_or_default()
                .rsplit(' ')
                .next()
                .unwrap_or_default()
                .to_string()
        }),
    };
    let params: Vec<String> = decl
        .get(open + 1..close)?
        .split(',')
        .filter(|p| !p.trim().is_empty())
        .map(norm_param)
        .collect();
    Some(format!("{ty}::{name}({})", params.join(",")))
}

struct Buf(Vec<u64>, usize);

impl Buf {
    fn new(b: &[u8]) -> Self {
        let mut v = vec![0u64; b.len().div_ceil(8).max(1)];
        // SAFETY: the vector holds at least `b.len()` bytes.
        unsafe { std::ptr::copy_nonoverlapping(b.as_ptr(), v.as_mut_ptr().cast::<u8>(), b.len()) };
        Self(v, b.len())
    }

    fn ptr(&mut self) -> *mut c_void {
        self.0.as_mut_ptr().cast()
    }

    fn bytes(&self) -> Vec<u8> {
        // SAFETY: the vector holds at least `self.1` bytes.
        unsafe { std::slice::from_raw_parts(self.0.as_ptr().cast::<u8>(), self.1) }.to_vec()
    }
}

fn item(s: &str) -> Option<(char, Vec<u8>)> {
    let (k, h) = s.split_once(':')?;
    Some((k.chars().next()?, unhex(h)))
}

#[test]
fn corpus_matches_cpp() {
    testreg::REG.with(|r| r.borrow_mut().clear());
    s2_math::register(Engine(std::ptr::null_mut()));
    let reg: HashMap<String, usize> = testreg::REG.with(|r| {
        r.borrow()
            .iter()
            .enumerate()
            .filter_map(|(i, (ty, decl, _))| {
                let t = ty.as_deref().map(String::from_utf8_lossy);
                key(t.as_deref(), &String::from_utf8_lossy(decl)).map(|k| (k, i))
            })
            .collect()
    });
    let text = std::fs::read_to_string(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/golden/S2/corpus_soma.txt"
    ))
    .unwrap_or_default();
    let (mut checked, mut skipped, mut bad) = (0usize, 0usize, Vec::new());
    for line in text.lines() {
        let f: Vec<&str> = line.split('\t').collect();
        let (Some(decl), Some(inp), Some(out)) = (f.first(), f.get(2), f.get(3)) else {
            continue;
        };
        if line.contains(" s:")
            || line.contains(" w:")
            || decl.contains("Random")
            || [
                "cScript_",
                "tID",
                "cLux_",
                "SlideDoor",
                "__print",
                " Log(",
                " Warning(",
                " Error(",
            ]
            .iter()
            .any(|p| decl.contains(p))
            || decl.contains("Easing")
            || !out.starts_with("out rc=0")
        {
            skipped += 1;
            continue;
        }
        let Some(k) = key(None, decl) else { continue };
        let Some(&idx) = reg.get(&k) else {
            skipped += 1;
            continue;
        };
        let items: Vec<(char, Vec<u8>)> = inp.split_whitespace().skip(1).filter_map(item).collect();
        let is_method = decl.contains("::");
        let (this_in, args_in) = if is_method {
            (items.first(), items.get(1..).unwrap_or_default())
        } else {
            (None, items.as_slice())
        };
        let mut this = this_in.map(|t| Buf::new(&t.1));
        let mut bufs: Vec<Buf> = args_in.iter().map(|a| Buf::new(&a.1)).collect();
        let mut ptrs: Vec<*mut c_void> = bufs.iter_mut().map(Buf::ptr).collect();
        let mut ret = Buf::new(&[0u8; 64]);
        let mut fr = Frame::mock(
            this.as_mut().map_or(std::ptr::null_mut(), Buf::ptr),
            ret.ptr(),
        );
        for (i, p) in ptrs.iter_mut().enumerate() {
            let slot = if is_ref_param(decl, i) {
                std::ptr::from_mut(p).cast()
            } else {
                *p
            };
            fr.set_slot(i, slot);
        }
        testreg::REG.with(|r| testreg::call(&r.borrow()[idx].2, &fr));
        let mut got: Vec<String> = Vec::new();
        let ret_item = out.split_whitespace().skip(2).collect::<Vec<_>>();
        let mut expect: Vec<String> = Vec::new();
        if let Some(p) = ret_item.iter().position(|t| *t == "ret") {
            let want = ret_item.get(p + 1).copied().unwrap_or_default();
            let (kind, w) = item(want).unwrap_or(('p', Vec::new()));
            let b = match fr.ret_addr.get() {
                // SAFETY: a reference return points at a buffer of this frame.
                Some(p) => unsafe { std::slice::from_raw_parts(p.cast::<u8>(), w.len()) }.to_vec(),
                None => ret.bytes().get(..w.len()).unwrap_or_default().to_vec(),
            };
            expect.push(format!("{kind}:{}", hex(&w)));
            got.push(format!("{kind}:{}", hex(&b)));
        }
        if let (Some(t), Some(p)) = (this.as_ref(), ret_item.iter().position(|t| *t == "this")) {
            expect.push(ret_item.get(p + 1).copied().unwrap_or_default().to_string());
            got.push(format!("o:{}", hex(&t.bytes())));
        }
        for (i, b) in bufs.iter().enumerate() {
            if let Some(p) = ret_item.iter().position(|t| *t == format!("a{i}")) {
                let w = ret_item.get(p + 1).copied().unwrap_or_default();
                expect.push(w.to_string());
                got.push(format!(
                    "{}:{}",
                    args_in.get(i).map_or('p', |a| a.0),
                    hex(&b.bytes())
                ));
            }
        }
        checked += 1;
        if got != expect && bad.len() < 5 {
            bad.push(format!("{line}\n  got {got:?}\n  want {expect:?}"));
        }
    }
    assert!(
        bad.is_empty(),
        "{} mismatches, first:\n{}",
        bad.len(),
        bad.join("\n")
    );
    assert!(
        checked > 1000,
        "only {checked} calls replayed ({skipped} skipped)"
    );
}

fn is_ref_param(decl: &str, i: usize) -> bool {
    let (Some(o), Some(c)) = (decl.find('('), decl.rfind(')')) else {
        return false;
    };
    decl.get(o + 1..c)
        .unwrap_or_default()
        .split(',')
        .nth(i)
        .is_some_and(|p| p.contains('&'))
}
