//! Callback tokens shared by the options and key config windows: a widget callback carries one
//! token, `dispatch` maps it back to the window, entry and callback id.

use std::collections::BTreeMap;
use std::sync::{Mutex, MutexGuard, PoisonError};

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct CbData {
    pub lval: i32,
    pub fval: f32,
    pub key: i32,
    pub pad_value: f32,
    pub pad_type: i32,
    pub pad_id: i32,
    pub msg: i32,
}

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug)]
pub enum Kind {
    Options,
    KeyConfig,
    Entry,
}

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug)]
pub struct Cb {
    pub kind: Kind,
    pub owner: u64,
    pub aux: u64,
    pub id: u16,
}

#[derive(Default)]
struct Tokens {
    list: Vec<Cb>,
    index: BTreeMap<Cb, usize>,
}

static TOKENS: Mutex<Tokens> = Mutex::new(Tokens {
    list: Vec::new(),
    index: BTreeMap::new(),
});

fn tokens() -> MutexGuard<'static, Tokens> {
    TOKENS.lock().unwrap_or_else(PoisonError::into_inner)
}

/// Stable non-zero token for `cb`.
pub fn token(cb: Cb) -> usize {
    let mut t = tokens();
    if let Some(&i) = t.index.get(&cb) {
        return i;
    }
    t.list.push(cb);
    let i = t.list.len();
    t.index.insert(cb, i);
    i
}

pub fn resolve(token: usize) -> Option<Cb> {
    tokens().list.get(token.checked_sub(1)?).copied()
}

/// Lock a registry, recovering from a poisoned lock (a panic aborts the process anyway).
pub fn lock<T>(m: &Mutex<T>) -> MutexGuard<'_, T> {
    m.lock().unwrap_or_else(PoisonError::into_inner)
}
