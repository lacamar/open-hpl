//! The Amnesia/AMFP script API bodies. Each function is registered with AngelScript from `a1_table`,
//! marshals its script arguments, runs its logic here and reaches still-C++ game objects through
//! the engine host table (`ops`, implemented by `LuxScriptHostA1.cpp`).

pub(super) mod ops;

pub(super) mod enemy;
pub(super) mod entity;
pub(super) mod fx;
pub(super) mod misc;
pub(super) mod player;
pub(super) mod prop;
pub(super) mod stubs;
pub(super) mod world;

use super::a1::{Arg, host, raw_bytes};
use super::lookup::{self, Wild};
use std::ffi::c_void;

pub(super) trait P: Copy {
    type Raw;
    const K: char;
    /// # Safety
    /// `r` is the raw argument the engine passed.
    unsafe fn from_raw(r: Self::Raw) -> Self;
    fn arg(&self) -> Arg;
}

impl P for i32 {
    type Raw = i32;
    const K: char = 'i';
    unsafe fn from_raw(r: i32) -> i32 {
        r
    }
    fn arg(&self) -> Arg {
        Arg::i(*self)
    }
}

impl P for f32 {
    type Raw = f32;
    const K: char = 'f';
    unsafe fn from_raw(r: f32) -> f32 {
        r
    }
    fn arg(&self) -> Arg {
        Arg::f(*self)
    }
}

impl P for bool {
    type Raw = bool;
    const K: char = 'b';
    unsafe fn from_raw(r: bool) -> bool {
        r
    }
    fn arg(&self) -> Arg {
        Arg::b(*self)
    }
}

/// A `string &` script argument.
#[derive(Clone, Copy)]
pub(super) struct S<'a>(pub &'a [u8]);

impl P for S<'_> {
    type Raw = *const c_void;
    const K: char = 's';
    unsafe fn from_raw(r: *const c_void) -> Self {
        // SAFETY: the engine keeps the string alive for the call.
        S(unsafe { raw_bytes(r) })
    }
    fn arg(&self) -> Arg {
        Arg::bytes(self.0)
    }
}

pub(super) trait R {
    type Raw;
    const K: char;
    fn into_raw(self) -> Self::Raw;
    fn arg(&self) -> Arg;
}

impl R for () {
    type Raw = ();
    const K: char = 'v';
    fn into_raw(self) {}
    fn arg(&self) -> Arg {
        Arg::default()
    }
}

impl R for i32 {
    type Raw = i32;
    const K: char = 'i';
    fn into_raw(self) -> i32 {
        self
    }
    fn arg(&self) -> Arg {
        Arg::i(*self)
    }
}

impl R for f32 {
    type Raw = f32;
    const K: char = 'f';
    fn into_raw(self) -> f32 {
        self
    }
    fn arg(&self) -> Arg {
        Arg::f(*self)
    }
}

impl R for bool {
    type Raw = bool;
    const K: char = 'b';
    fn into_raw(self) -> bool {
        self
    }
    fn arg(&self) -> Arg {
        Arg::b(*self)
    }
}

/// A `string &` result: a pointer to a C++-owned `std::string`.
pub(super) struct RStr(pub *const c_void);

impl R for RStr {
    type Raw = *const c_void;
    const K: char = 'p';
    fn into_raw(self) -> *const c_void {
        self.0
    }
    fn arg(&self) -> Arg {
        Arg::ptr(self.0)
    }
}

/// Defines a registered script function: `sf!(Name(a: S, b: f32) -> bool { .. })`. The body may
/// `return`; the call is traced (`OHPL_A1B_TRACE`) after it finishes.
macro_rules! sf {
    ($name:ident($($a:ident: $t:ty),*) -> $r:ty $body:block) => {
        #[allow(non_snake_case, clippy::too_many_arguments, clippy::redundant_closure_call)]
        pub(in crate::luxscript) unsafe extern "C" fn $name(
            $($a: <$t as $crate::luxscript::fns::P>::Raw),*
        ) -> <$r as $crate::luxscript::fns::R>::Raw {
            $(
                // SAFETY: the engine passes the declared argument types.
                let $a = unsafe { <$t as $crate::luxscript::fns::P>::from_raw($a) };
            )*
            let r: $r = (|| -> $r { $body })();
            if $crate::luxscript::trace::enabled() {
                let ks: &[char] = &[$(<$t as $crate::luxscript::fns::P>::K),*];
                let kinds: String = ks.iter().collect();
                $crate::luxscript::trace::line(
                    stringify!($name),
                    &kinds,
                    &[$($crate::luxscript::fns::P::arg(&$a)),*],
                    <$r as $crate::luxscript::fns::R>::K,
                    &$crate::luxscript::fns::R::arg(&r),
                );
            }
            $crate::luxscript::fns::R::into_raw(r)
        }
    };
    ($name:ident($($a:ident: $t:ty),*) $body:block) => {
        sf!($name($($a: $t),*) -> () $body);
    };
}
pub(super) use sf;

/// A C++ entity (`iLuxEntity *`).
#[derive(Clone, Copy, PartialEq)]
pub(super) struct Ent(pub *const c_void);

impl Ent {
    pub fn a(self) -> Arg {
        Arg::ptr(self.0)
    }
}

pub(super) const NULL_STR: i32 = 0;
pub(super) const GLOBAL_TEMP: i32 = 1;
pub(super) const LOCAL_NULL: i32 = 2;

/// One of the host's long-lived script strings (`gsScriptNull` and friends).
pub(super) fn temp(slot: i32, b: &[u8]) -> RStr {
    RStr(call(ops::TEMP_STR, &[Arg::i(slot), s(b)]).p)
}

pub(super) fn null_str() -> RStr {
    temp(NULL_STR, b"")
}

pub(super) fn call(op: ops::Op, args: &[Arg]) -> Arg {
    host(op as u32, args)
}

pub(super) fn s(b: &[u8]) -> Arg {
    Arg::bytes(b)
}

fn msg(op: ops::Op, text: &[u8]) {
    call(op, &[s(text)]);
}

pub(super) fn warning(text: &str) {
    msg(ops::WARNING, text.as_bytes());
}

pub(super) fn error(text: &str) {
    msg(ops::ERROR, text.as_bytes());
}

pub(super) fn with_name(fmt: &str, name: &[u8], tail: &str) -> String {
    format!("{fmt}'{}'{tail}", String::from_utf8_lossy(name))
}

pub(super) const TYPE_ANY: i32 = lookup::ENTITY_TYPE_ANY;
pub(super) const TYPE_AREA: i32 = lookup::ENTITY_TYPE_AREA;
pub(super) const TYPE_PROP: i32 = 0;
pub(super) const TYPE_ENEMY: i32 = 2;

pub(super) fn map_exists() -> bool {
    call(ops::MAP_EXISTS, &[]).as_b()
}

/// `cLuxScriptHandler::GetEntities`.
pub(super) fn entities(name: &[u8], ty: i32, sub: i32) -> Option<Vec<Ent>> {
    if !map_exists() {
        error("GetEntities(..) failed! No map was set!\n");
        return None;
    }
    if !lookup::has_wildcard(name) {
        let e = call(ops::MAP_ENTITY_TYPED, &[s(name), Arg::i(ty), Arg::i(sub)]);
        if e.p.is_null() {
            warning(&format!(
                "Entity '{}' with type {ty} and subtype {sub} does not exist!\n",
                String::from_utf8_lossy(name)
            ));
            return None;
        }
        return Some(vec![Ent(e.p)]);
    }
    let wild = Wild::new(name);
    let n = call(ops::MAP_ENTITIES, &[]).as_i();
    let mut out = Vec::new();
    for i in 0..n {
        let e = Ent(call(ops::MAP_ENTITY_NTH, &[Arg::i(i)]).p);
        let matches_type = call(ops::ENT_IS_TYPE, &[e.a(), Arg::i(ty), Arg::i(sub)]).as_b();
        // SAFETY: the host returns the entity's own name string.
        let en = unsafe { raw_bytes(call(ops::ENT_NAME, &[e.a()]).p) };
        if matches_type && wild.matches(en) {
            out.push(e);
        }
    }
    if out.is_empty() {
        warning(&format!(
            "Could not find any entities with string '{}'\n",
            String::from_utf8_lossy(name)
        ));
        return None;
    }
    Some(out)
}

/// `cLuxScriptHandler::GetEntity`.
pub(super) fn entity(name: &[u8], ty: i32, sub: i32) -> Option<Ent> {
    if !map_exists() {
        error("GetEntity(..) failed! No map was set!\n");
        return None;
    }
    let e = call(ops::MAP_ENTITY, &[s(name)]);
    if e.p.is_null() {
        warning(&with_name("Entity ", name, " does not exist!\n"));
        return None;
    }
    let ent = Ent(e.p);
    if !call(ops::ENT_IS_TYPE, &[ent.a(), Arg::i(ty), Arg::i(sub)]).as_b() {
        warning(&format!(
            "Entity '{}' is not correct type! ({ty} {sub})\n",
            String::from_utf8_lossy(name)
        ));
        return None;
    }
    Some(ent)
}

/// The `BEGIN_SET_PROPERTY` loop: `f` runs for each entity of the type, none if the lookup failed.
pub(super) fn each(name: &[u8], ty: i32, sub: i32, mut f: impl FnMut(Ent)) {
    if let Some(list) = entities(name, ty, sub) {
        list.into_iter().for_each(&mut f);
    }
}
