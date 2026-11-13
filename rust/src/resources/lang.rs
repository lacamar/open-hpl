//! `cLanguageFile` (H2 pass 2): Rust owns the category/entry maps; the host allocates the
//! `std::wstring` entry texts and adds resource directories.

use super::h3_ops::{ERROR, WARNING, WStr};
use crate::string;
use crate::string_ffi::{Sink, units};
use crate::xml::{Doc, lang};
use std::cmp::Ordering;
use std::collections::BTreeMap;
use std::collections::btree_map::Entry;
use std::ffi::c_void;

/// `strcasecmp` ordering of the C++ `std::map` keys; a key keeps the spelling it was inserted with.
#[derive(Clone)]
struct CiKey(Vec<u8>);

impl Ord for CiKey {
    fn cmp(&self, other: &Self) -> Ordering {
        let l = |k: &Self| k.0.iter().map(u8::to_ascii_lowercase).collect::<Vec<_>>();
        l(self).cmp(&l(other))
    }
}

impl PartialOrd for CiKey {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl PartialEq for CiKey {
    fn eq(&self, other: &Self) -> bool {
        self.cmp(other) == Ordering::Equal
    }
}

impl Eq for CiKey {}

#[repr(C)]
pub struct LangOps {
    pub ctx: *mut c_void,
    pub log: unsafe extern "C" fn(*mut c_void, i32, *const u8, usize),
    pub add_dir: unsafe extern "C" fn(*mut c_void, WStr, bool),
    pub file_exists: unsafe extern "C" fn(*mut c_void, WStr) -> bool,
    /// Streams the file to `sink`; false when it cannot be opened, `read_ok` reports a short read.
    pub read_file:
        unsafe extern "C" fn(*mut c_void, WStr, Sink<u8>, *mut c_void, *mut bool) -> bool,
    pub text_new: unsafe extern "C" fn(*mut c_void, *const i32, usize) -> *mut c_void,
    pub text_free: unsafe extern "C" fn(*mut c_void, *mut c_void),
}

pub struct Lang {
    ops: LangOps,
    cats: BTreeMap<CiKey, BTreeMap<CiKey, usize>>,
}

struct Reader<'a> {
    lang: &'a mut Lang,
    alt: &'a [u32],
    file: &'a [u32],
    cur: Option<CiKey>,
}

impl lang::LangSink for Reader<'_> {
    fn directory(&mut self, path: &[u8], add_sub_dirs: bool) {
        let wide = string::utf8_to_wide(path);
        if !self.alt.is_empty() {
            self.lang.add_dir(&[self.alt, &wide].concat(), add_sub_dirs);
        }
        self.lang.add_dir(&wide, add_sub_dirs);
    }

    fn no_resources(&mut self) {
        let msg = [
            b"No resources element found in '".as_slice(),
            &string::wide_to_utf8(self.file),
            b"'\n",
        ]
        .concat();
        self.lang.log(WARNING, &msg);
    }

    fn category(&mut self, name: &[u8]) {
        let key = CiKey(name.to_vec());
        self.lang.cats.entry(key.clone()).or_default();
        self.cur = Some(key);
    }

    fn entry(&mut self, name: &[u8], text: &[i32]) {
        let Some(cat) = self.cur.as_ref().and_then(|k| self.lang.cats.get(k)) else {
            return;
        };
        if cat.contains_key(&CiKey(name.to_vec())) {
            return;
        }
        let t = self.lang.new_text(text);
        if let Some(cat) = self.cur.as_ref().and_then(|k| self.lang.cats.get_mut(k)) {
            cat.insert(CiKey(name.to_vec()), t.expose_provenance());
        }
    }
}

impl Lang {
    pub fn new(ops: LangOps) -> Self {
        Self {
            ops,
            cats: BTreeMap::new(),
        }
    }

    fn log(&self, level: i32, msg: &[u8]) {
        // SAFETY: the host contract of `LangOps`.
        unsafe { (self.ops.log)(self.ops.ctx, level, msg.as_ptr(), msg.len()) }
    }

    fn add_dir(&self, path: &[u32], sub: bool) {
        // SAFETY: the host contract of `LangOps`.
        unsafe { (self.ops.add_dir)(self.ops.ctx, WStr::of(path), sub) }
    }

    fn new_text(&self, text: &[i32]) -> *mut c_void {
        // SAFETY: the host contract of `LangOps`.
        unsafe { (self.ops.text_new)(self.ops.ctx, text.as_ptr(), text.len()) }
    }

    fn free_text(&self, t: usize) {
        // SAFETY: `t` came from `text_new`.
        unsafe { (self.ops.text_free)(self.ops.ctx, std::ptr::with_exposed_provenance_mut(t)) }
    }

    fn read(&self, path: &[u32]) -> Option<(Vec<u8>, bool)> {
        unsafe extern "C" fn put(ctx: *mut c_void, p: *const u8, n: usize) {
            // SAFETY: `ctx` is the `Vec` handed in below.
            unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(units(p, n)) }
        }
        let mut data = Vec::new();
        let mut ok = false;
        // SAFETY: the sink writes into `data`, alive for the call.
        let opened = unsafe {
            (self.ops.read_file)(
                self.ops.ctx,
                WStr::of(path),
                put,
                (&raw mut data).cast(),
                &raw mut ok,
            )
        };
        opened.then_some((data, ok))
    }

    /// `cLanguageFile::AddFromFile`.
    pub fn add_from_file(&mut self, file: &[u32], add_dirs: bool, alt: &[u32]) -> bool {
        let mut path = file.to_vec();
        if !alt.is_empty() {
            let joined = [alt, file].concat();
            // SAFETY: the host contract of `LangOps`.
            if unsafe { (self.ops.file_exists)(self.ops.ctx, WStr::of(&joined)) } {
                path = joined;
            }
        }
        let mut ret = false;
        if let Some((data, read_ok)) = self.read(&path) {
            let mut doc = Doc::new();
            ret = doc.load(&data, read_ok) && {
                let mut r = Reader {
                    lang: self,
                    alt,
                    file,
                    cur: None,
                };
                lang::read(&doc, add_dirs, &mut r).is_some()
            };
        }
        if !ret {
            let msg = [
                b"Couldn't load language file '".as_slice(),
                &string::wide_to_utf8(file),
                b"'\n",
            ]
            .concat();
            self.log(ERROR, &msg);
        }
        ret
    }

    /// The host text of `cat`/`name`, null (with the warning logged) when missing.
    pub fn translate(&self, cat: &[u8], name: &[u8]) -> *mut c_void {
        let Some(c) = self.cats.get(&CiKey(cat.to_vec())) else {
            let msg = [
                b"Could not find language file category '".as_slice(),
                cat,
                b"'\n",
            ]
            .concat();
            self.log(WARNING, &msg);
            return std::ptr::null_mut();
        };
        let Some(t) = c.get(&CiKey(name.to_vec())) else {
            let msg = [
                b"Could not find language file entry '".as_slice(),
                name,
                b"'\n",
            ]
            .concat();
            self.log(WARNING, &msg);
            return std::ptr::null_mut();
        };
        std::ptr::with_exposed_provenance_mut(*t)
    }

    pub fn has_entry(&self, cat: &[u8], name: &[u8]) -> bool {
        self.cats
            .get(&CiKey(cat.to_vec()))
            .is_some_and(|c| c.contains_key(&CiKey(name.to_vec())))
    }

    /// Inserts or replaces an entry, creating the category when missing.
    pub fn set_entry(&mut self, cat: &[u8], name: &[u8], text: &[i32]) {
        let t = self.new_text(text).expose_provenance();
        let old = match self
            .cats
            .entry(CiKey(cat.to_vec()))
            .or_default()
            .entry(CiKey(name.to_vec()))
        {
            Entry::Occupied(mut o) => Some(o.insert(t)),
            Entry::Vacant(v) => {
                v.insert(t);
                None
            }
        };
        if let Some(old) = old {
            self.free_text(old);
        }
    }

    /// Calls `f(category, entry, text)` in map order; a category with no entries is reported once with a null text.
    pub fn for_each(&self, mut f: impl FnMut(&[u8], &[u8], *mut c_void)) {
        for (c, entries) in &self.cats {
            if entries.is_empty() {
                f(&c.0, b"", std::ptr::null_mut());
            }
            for (e, t) in entries {
                f(&c.0, &e.0, std::ptr::with_exposed_provenance_mut(*t));
            }
        }
    }
}

impl Drop for Lang {
    fn drop(&mut self) {
        for t in self.cats.values().flat_map(BTreeMap::values) {
            self.free_text(*t);
        }
    }
}
