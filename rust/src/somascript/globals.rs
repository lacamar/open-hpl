//! Script global variables, call arguments and return value (`SomaScriptGlobals.cpp`).

use super::cfmt;
use super::stream::Stream;

use std::collections::BTreeMap;
use std::sync::{Mutex, MutexGuard, PoisonError};

#[derive(Clone, Debug, PartialEq)]
pub(crate) struct Variant {
    pub s: Vec<u8>,
    pub b: bool,
    pub i: i32,
    pub f: f32,
    pub v2: [f32; 2],
    pub v3: [f32; 3],
    pub v4: [f32; 4],
    pub m: [f32; 16],
    pub c: [f32; 4],
    pub id: (u8, i32, i32),
}

const IDENTITY: [f32; 16] = [
    1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
];

impl Default for Variant {
    fn default() -> Self {
        Self {
            s: Vec::new(),
            b: false,
            i: 0,
            f: 0.0,
            v2: [0.0; 2],
            v3: [0.0; 3],
            v4: [0.0; 4],
            m: IDENTITY,
            c: [0.0; 4],
            id: (0, 0, 0),
        }
    }
}

#[derive(Default)]
pub(crate) struct Globals {
    pub vars: BTreeMap<Vec<u8>, Variant>,
    pub args: BTreeMap<i32, Variant>,
    pub ret: Variant,
}

static GLOBALS: Mutex<Globals> = Mutex::new(Globals {
    vars: BTreeMap::new(),
    args: BTreeMap::new(),
    ret: Variant {
        s: Vec::new(),
        b: false,
        i: 0,
        f: 0.0,
        v2: [0.0; 2],
        v3: [0.0; 3],
        v4: [0.0; 4],
        m: IDENTITY,
        c: [0.0; 4],
        id: (0, 0, 0),
    },
});

pub(crate) fn lock() -> MutexGuard<'static, Globals> {
    GLOBALS.lock().unwrap_or_else(PoisonError::into_inner)
}

pub(crate) const FIELD_S: u32 = 0;
pub(crate) const FIELD_B: u32 = 1;
pub(crate) const FIELD_I: u32 = 2;
pub(crate) const FIELD_F: u32 = 3;
pub(crate) const FIELD_V2: u32 = 4;
pub(crate) const FIELD_V3: u32 = 5;
pub(crate) const FIELD_M: u32 = 6;
pub(crate) const FIELD_C: u32 = 7;
pub(crate) const FIELD_ID: u32 = 8;
pub(crate) const FIELD_V4: u32 = 9;

/// A field's value as raw 32-bit words (floats keep their bits), strings aside.
pub(crate) fn get_words(v: &Variant, field: u32) -> [u32; 16] {
    let mut w = [0u32; 16];
    let mut put = |src: &[f32]| {
        for (d, s) in w.iter_mut().zip(src) {
            *d = s.to_bits();
        }
    };
    match field {
        FIELD_B => w[0] = u32::from(v.b),
        FIELD_I => w[0] = v.i as u32,
        FIELD_F => put(&[v.f]),
        FIELD_V2 => put(&v.v2),
        FIELD_V3 => put(&v.v3),
        FIELD_V4 => put(&v.v4),
        FIELD_M => put(&v.m),
        FIELD_C => put(&v.c),
        FIELD_ID => {
            w[0] = u32::from(v.id.0);
            w[1] = v.id.1 as u32;
            w[2] = v.id.2 as u32;
        }
        _ => {}
    }
    w
}

pub(crate) fn set_words(v: &mut Variant, field: u32, w: &[u32; 16]) {
    let f = |i: usize| f32::from_bits(w.get(i).copied().unwrap_or(0));
    match field {
        FIELD_B => v.b = (w[0] & 0xff) != 0,
        FIELD_I => v.i = w[0] as i32,
        FIELD_F => v.f = f(0),
        FIELD_V2 => v.v2 = std::array::from_fn(f),
        FIELD_V3 => v.v3 = std::array::from_fn(f),
        FIELD_V4 => v.v4 = std::array::from_fn(f),
        FIELD_M => v.m = std::array::from_fn(f),
        FIELD_C => v.c = std::array::from_fn(f),
        FIELD_ID => v.id = (w[0] as u8, w[1] as i32, w[2] as i32),
        _ => {}
    }
}

pub(crate) fn serialize(g: &Globals) -> Vec<u8> {
    let mut out = Vec::new();
    for (name, v) in &g.vars {
        out.extend_from_slice(b"var\t");
        out.extend_from_slice(format!("{}:", name.len()).as_bytes());
        out.extend_from_slice(name);
        out.extend_from_slice(format!("{}:", v.s.len()).as_bytes());
        out.extend_from_slice(&v.s);
        out.extend_from_slice(format!(" {} {}", u8::from(v.b), v.i).as_bytes());
        let floats: Vec<f32> = [v.f]
            .iter()
            .chain(&v.v2)
            .chain(&v.v3)
            .chain(&v.v4)
            .chain(&v.c)
            .copied()
            .collect();
        for f in floats {
            out.push(b' ');
            out.extend_from_slice(&cfmt::fmt_g9(f));
        }
        out.extend_from_slice(format!(" {} {} {}", v.id.0, v.id.1, v.id.2).as_bytes());
        for f in v.m {
            out.push(b' ');
            out.extend_from_slice(&cfmt::fmt_g9(f));
        }
        out.push(b'\n');
    }
    out
}

/// Upper bound on a serialised string; the C++ would throw `std::length_error` or run out of memory.
const MAX_STRING: usize = 1 << 26;

fn read_str(s: &mut Stream) -> Vec<u8> {
    let mut len = 0usize;
    s.size(&mut len);
    s.get();
    if len > MAX_STRING {
        len = 0;
    }
    let mut v = s.read(len).to_vec();
    v.resize(len, 0);
    v
}

pub(crate) fn deserialize(g: &mut Globals, data: &[u8]) {
    g.vars.clear();
    let mut s = Stream::new(data);
    while let Some(key) = s.token() {
        if key != b"var" {
            s.ignore_line();
            continue;
        }
        s.get();
        let name = read_str(&mut s);
        let v = g.vars.entry(name).or_default();
        v.s = read_str(&mut s);
        s.boolean(&mut v.b);
        s.int(&mut v.i);
        s.float(&mut v.f);
        for x in
            v.v2.iter_mut()
                .chain(&mut v.v3)
                .chain(&mut v.v4)
                .chain(&mut v.c)
        {
            s.float(x);
        }
        // a failed read leaves 0, which then overwrites the stored byte
        let mut a = 0;
        s.int(&mut a);
        s.int(&mut v.id.1);
        s.int(&mut v.id.2);
        v.id.0 = a as u8;
        for x in &mut v.m {
            s.float(x);
        }
    }
}

/// `ShowSecretCode`: three bytes xored with the key become argument 0.
pub(crate) fn secret_code(g: &mut Globals, key: [u8; 3], x: &[u8]) -> bool {
    if x.len() < 3 {
        return false;
    }
    let code: Vec<u8> = x.iter().zip(key).map(|(a, b)| a ^ b).collect();
    g.args.entry(0).or_default().s = code;
    true
}
