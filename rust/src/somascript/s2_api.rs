//! S2: `cSomaScriptApi` on the Rust path: loads the recovered API and registers it (stubs, script
//! structs, property accessors) on an engine; owns the stub bookkeeping the natives consult.

use super::api;
use super::s2_as::{self, Engine, Func, Type};
use super::s2_host;
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};
use std::ffi::{c_int, c_void};
use std::sync::{Mutex, MutexGuard};

const STUB_USER_DATA: usize = 0x50b0;

fn lock<T>(m: &Mutex<T>) -> MutexGuard<'_, T> {
    m.lock().unwrap_or_else(std::sync::PoisonError::into_inner)
}

#[derive(Default)]
pub(crate) struct ApiType {
    name: Vec<u8>,
    kind: Vec<u8>,
    size: i32,
    behaviours: Vec<(Vec<u8>, Vec<u8>)>,
    methods: Vec<Vec<u8>>,
    props: Vec<(Vec<u8>, i32)>,
    casts: Vec<Vec<u8>>,
}

type EnumList = Vec<(Vec<u8>, Vec<(Vec<u8>, i32)>)>;

#[derive(Default)]
pub struct Api {
    enums: EnumList,
    types: Vec<ApiType>,
    globals: Vec<Vec<u8>>,
    global_props: Vec<Vec<u8>>,
    errors: Vec<Vec<u8>>,
    warnings: Vec<Vec<u8>>,
    remaps: Vec<Vec<u8>>,
    sigs: HashMap<Vec<u8>, Vec<Vec<u8>>>,
    enum_names: BTreeSet<Vec<u8>>,
    types_registered: bool,
}

static ACTIVE: Mutex<usize> = Mutex::new(0);

struct Indirect {
    pointer_offset: i32,
    size: i32,
    string_offsets: Vec<i32>,
    offsets: BTreeMap<Vec<u8>, i32>,
}

impl Default for Indirect {
    fn default() -> Self {
        Self {
            pointer_offset: -1,
            size: 0,
            string_offsets: Vec::new(),
            offsets: BTreeMap::new(),
        }
    }
}

#[derive(Default)]
struct State {
    dummies: HashSet<usize>,
    dummy_by_type: HashMap<i32, usize>,
    dummy_by_name: HashMap<Vec<u8>, usize>,
    defaults: HashMap<i32, usize>,
    struct_refs: HashMap<usize, i32>,
    stub_counts: BTreeMap<Vec<u8>, i32>,
    indirect: BTreeMap<Vec<u8>, Indirect>,
}

static STATE: Mutex<Option<State>> = Mutex::new(None);

fn with_state<R>(f: impl FnOnce(&mut State) -> R) -> R {
    let mut g = lock(&STATE);
    f(g.get_or_insert_with(State::default))
}

const VALUE_NATIVE: [&[u8]; 2] = [b"tString", b"tWString"];

pub(crate) fn is_value_type_native(name: &[u8]) -> bool {
    VALUE_NATIVE.contains(&name)
}

fn fail(api: &mut Api, what: &[u8], code: c_int) {
    let mut s = what.to_vec();
    s.extend_from_slice(format!(" ({code})").as_bytes());
    api.errors.push(s);
}

fn ok_reg(r: c_int) -> bool {
    r >= 0 || r == s2_as::ALREADY_REGISTERED
}

impl Api {
    pub fn load(&mut self, text: &[u8]) {
        api::load(text, |r| {
            let cur = r.type_idx;
            let a = r.a;
            match r.kind {
                api::REC_ENUM => self.enums.push((a, Vec::new())),
                api::REC_ENUM_VALUE => {
                    if let Some(e) = self.enums.last_mut() {
                        e.1.push((a, r.n));
                    }
                }
                api::REC_TYPE => self.types.push(ApiType {
                    name: a,
                    kind: r.b.to_vec(),
                    size: r.n,
                    ..ApiType::default()
                }),
                api::REC_BEHAVIOUR => {
                    if let Some(t) = self.types.get_mut(cur) {
                        t.behaviours.push((a, r.b.to_vec()));
                    }
                }
                api::REC_METHOD => {
                    if let Some(t) = self.types.get_mut(cur) {
                        t.methods.push(a);
                    }
                }
                api::REC_PROP => {
                    if let Some(t) = self.types.get_mut(cur) {
                        t.props.push((a, r.n));
                    }
                }
                api::REC_CAST => {
                    if let Some(t) = self.types.get_mut(cur) {
                        t.casts.push(a);
                    }
                }
                api::REC_GLOBAL => self.globals.push(a),
                api::REC_GLOBAL_PROP => self.global_props.push(a),
                _ => {}
            }
        });
        for t in &mut self.types {
            for p in &mut t.props {
                p.1 = s2_host::native_offset(&t.name, &p.0, p.1);
            }
        }
    }

    pub fn rebase_enums(&mut self, base: &Api) {
        let conv = |l: &EnumList| -> Vec<api::Enum> {
            l.iter()
                .map(|(n, v)| api::Enum {
                    name: n.clone(),
                    values: v.clone(),
                })
                .collect()
        };
        let mut mine = conv(&self.enums);
        api::rebase_enums(&mut mine, &conv(&base.enums));
        let mut it = mine.iter().flat_map(|e| e.values.iter().map(|v| v.1));
        for e in &mut self.enums {
            for v in &mut e.1 {
                if let Some(x) = it.next() {
                    v.1 = x;
                }
            }
        }
    }

    pub fn native_decl(&mut self, ty: Option<&[u8]>, decl: &[u8]) -> Vec<u8> {
        if self.sigs.is_empty() {
            for t in &self.types {
                for m in &t.methods {
                    let k = [t.name.as_slice(), b"::", &api::decl_name(m)].concat();
                    self.sigs.entry(k).or_default().push(m.clone());
                }
            }
            for g in &self.globals {
                let k = [b"::".as_slice(), &api::decl_name(g)].concat();
                self.sigs.entry(k).or_default().push(g.clone());
            }
            for e in &self.enums {
                self.enum_names.insert(e.0.clone());
            }
        }
        let tname = ty.unwrap_or_default();
        let key = [tname, b"::", &api::decl_name(decl)].concat();
        let Some(cands) = self.sigs.get(&key) else {
            return decl.to_vec();
        };
        let refs: Vec<&[u8]> = cands.iter().map(Vec::as_slice).collect();
        let enums = &self.enum_names;
        let Some(pick) = api::native_decl_pick(&refs, &|s| enums.contains(s), decl) else {
            return decl.to_vec();
        };
        let Some(m) = cands.get(pick).cloned() else {
            return decl.to_vec();
        };
        let mut r = tname.to_vec();
        r.extend_from_slice(b"::");
        r.extend_from_slice(decl);
        r.extend_from_slice(b" -> ");
        r.extend_from_slice(&m);
        self.remaps.push(r);
        m
    }

    fn is_script_struct(t: &ApiType) -> bool {
        let factory = t
            .behaviours
            .iter()
            .any(|b| b.0 == b"factory" || b.0 == b"FactoryDefault");
        api::is_script_struct(&t.kind, &t.name, factory)
    }

    fn property_accessors(&mut self, e: Engine, ty: &[u8], decl: &[u8]) {
        let names: BTreeSet<&[u8]> = self.enums.iter().map(|x| x.0.as_slice()).collect();
        let Some((get, set)) = api::property_accessors(decl, &|s| names.contains(s)) else {
            return;
        };
        for d in std::iter::once(get).chain(set) {
            let r = e.reg_method_raw(ty, &d, stub as *const c_void);
            if !ok_reg(r) {
                let w = [ty, b"::", &d].concat();
                fail(self, &w, r);
            }
        }
    }

    pub fn register_types(&mut self, e: Engine) {
        set_active(self);
        if self.types_registered {
            return;
        }
        self.types_registered = true;
        let enums = std::mem::take(&mut self.enums);
        for (name, values) in &enums {
            let r = e.reg_enum(name);
            if !ok_reg(r) {
                fail(self, &[b"enum ".as_slice(), name].concat(), r);
            }
            for (vn, vv) in values {
                let r = e.reg_enum_value(name, vn, *vv);
                if !ok_reg(r) {
                    fail(self, &[b"enum value ".as_slice(), vn].concat(), r);
                }
            }
        }
        self.enums = enums;
        let types = std::mem::take(&mut self.types);
        for t in &types {
            let name = &t.name;
            if is_value_type_native(name) || e.type_by_name(name).is_some() {
                continue;
            }
            let r = if t.kind == b"interface" {
                e.reg_interface(name)
            } else if t.kind == b"value" {
                let flags = u64::from(s2_as::OBJ_VALUE | s2_as::OBJ_POD) | s2_as::OBJ_APP_PRIMITIVE;
                e.reg_object_type(name, t.size, flags)
            } else if t.kind == b"template" {
                continue;
            } else if Self::is_script_struct(t) {
                let r = e.reg_object_type(name, 0, u64::from(s2_as::OBJ_REF));
                if r >= 0 {
                    e.reg_behaviour_raw(
                        s2_as::BEHAVE_ADDREF,
                        name,
                        b"void f()",
                        struct_add_ref as *const c_void,
                    );
                    e.reg_behaviour_raw(
                        s2_as::BEHAVE_RELEASE,
                        name,
                        b"void f()",
                        struct_release as *const c_void,
                    );
                }
                r
            } else {
                e.reg_object_type(name, 0, u64::from(s2_as::OBJ_REF | s2_as::OBJ_NOCOUNT))
            };
            if !ok_reg(r) {
                fail(self, &[b"type ".as_slice(), name].concat(), r);
            }
        }
        self.types = types;
    }

    pub fn register(&mut self, e: Engine) -> usize {
        self.register_types(e);
        let types = std::mem::take(&mut self.types);
        for t in &types {
            if t.kind == b"template" {
                continue;
            }
            let Some(pinfo) = e.type_by_name(&t.name) else {
                continue;
            };
            self.register_type(e, t, pinfo);
        }
        self.register_casts(e, &types);
        self.types = types;
        self.register_globals(e);
        self.errors.len()
    }

    fn register_type(&mut self, e: Engine, t: &ApiType, pinfo: Type) {
        let name = &t.name;
        if t.kind == b"interface" {
            for m in &t.methods {
                let r = e.reg_interface_method(name, m);
                if !ok_reg(r) {
                    fail(self, &[name.as_slice(), b"::", m].concat(), r);
                }
            }
            return;
        }
        if !is_value_type_native(name) && !s2_host::has_native_behaviours(name) {
            for (bk, bp) in &t.behaviours {
                let r = if *bk == b"construct" && t.kind == b"value" {
                    let decl = [b"void f(".as_slice(), bp, b")"].concat();
                    let f = if api::is_copy_ctor(bp, name) {
                        copy_construct_pod
                    } else {
                        stub_construct
                    };
                    e.reg_behaviour_raw(s2_as::BEHAVE_CONSTRUCT, name, &decl, f as *const c_void)
                } else if (*bk == b"factory" || *bk == b"FactoryDefault") && t.kind == b"ref" {
                    let params: &[u8] = if *bk == b"factory" { bp } else { b"" };
                    let decl = [name.as_slice(), b"@ f(", params, b")"].concat();
                    e.reg_behaviour_raw(
                        s2_as::BEHAVE_FACTORY,
                        name,
                        &decl,
                        stub_factory as *const c_void,
                    )
                } else {
                    continue;
                };
                if !ok_reg(r) {
                    fail(
                        self,
                        &[name.as_slice(), b" behaviour ", bk, b"(", bp, b")"].concat(),
                        r,
                    );
                }
            }
        }
        let assign_decl = [b"opAssign(const ".as_slice(), name].concat();
        for m in &t.methods {
            if pinfo.has_method(m) {
                continue;
            }
            let layout_assign = t.kind == b"ref"
                && !t.props.is_empty()
                && super::strings::find(m, &assign_decl, 0).is_some();
            let f: unsafe extern "C" fn(*mut c_void) =
                if layout_assign { member_assign } else { stub };
            let r = e.reg_method_raw(name, m, f as *const c_void);
            if r >= 0 && !layout_assign {
                mark_stub(e, r);
            }
            if !ok_reg(r) {
                fail(self, &[name.as_slice(), b"::", m].concat(), r);
            }
        }
        self.register_props(e, t, pinfo);
    }

    fn register_props(&mut self, e: Engine, t: &ApiType, pinfo: Type) {
        let name = &t.name;
        let has_indirect = with_state(|s| s.indirect.contains_key(name));
        let mut string_area = 0;
        if has_indirect {
            for p in &t.props {
                string_area = string_area.max(p.1 + 32);
            }
        }
        for (decl, off) in &t.props {
            let pname = api::prop_name(decl);
            if pinfo.props().iter().any(|p| p.name == pname) {
                continue;
            }
            if *off >= 0 && has_indirect {
                let is_string = decl.starts_with(b"tString ");
                let mut offset = *off;
                if is_string {
                    offset = string_area;
                    string_area += 32;
                }
                let ptr_off = with_state(|s| {
                    let p = s.indirect.entry(name.clone()).or_default();
                    if is_string {
                        p.string_offsets.push(offset);
                    }
                    let key = decl
                        .rsplit(|&c| c == b' ')
                        .next()
                        .unwrap_or_default()
                        .to_vec();
                    p.offsets.insert(key, offset);
                    p.size = p.size.max(offset + 32);
                    p.pointer_offset
                });
                let r = e.reg_property(name, decl, offset, ptr_off, true);
                if !ok_reg(r) {
                    fail(self, &[name.as_slice(), b" prop ", decl].concat(), r);
                }
            } else if *off >= 0 {
                let r = e.reg_property(name, decl, *off, 0, false);
                if !ok_reg(r) {
                    fail(self, &[name.as_slice(), b" prop ", decl].concat(), r);
                }
            } else {
                self.property_accessors(e, name, decl);
            }
        }
    }

    fn register_casts(&mut self, e: Engine, types: &[ApiType]) {
        for t in types {
            for cast in &t.casts {
                let name = &t.name;
                let Some(other) = e.type_by_name(cast) else {
                    continue;
                };
                if other.flags() & s2_as::OBJ_REF == 0 {
                    continue;
                }
                let up = [cast.as_slice(), b"@ opImplCast()"].concat();
                let down = [name.as_slice(), b"@ opCast()"].concat();
                let Some(cur) = e.type_by_name(name) else {
                    continue;
                };
                if cur.has_method(&up) || other.has_method(&down) {
                    continue;
                }
                let r = e.reg_method_raw(name, &up, stub_cast_self as *const c_void);
                if !ok_reg(r) {
                    fail(self, &[name.as_slice(), b"::", &up].concat(), r);
                }
                let r = e.reg_method_raw(cast, &down, stub_cast_self as *const c_void);
                if !ok_reg(r) {
                    fail(self, &[cast.as_slice(), b"::", &down].concat(), r);
                }
            }
        }
    }

    fn register_globals(&mut self, e: Engine) {
        let native_names: HashSet<Vec<u8>> = e.global_func_names().into_iter().collect();
        let globals = std::mem::take(&mut self.globals);
        for g in &globals {
            if e.global_func_by_decl(g).is_some() {
                continue;
            }
            let paren = g.iter().position(|&c| c == b'(').unwrap_or(g.len());
            let head = g.get(..paren).unwrap_or_default();
            let start = head
                .iter()
                .rposition(|c| b" &@".contains(c))
                .map_or(0, |x| x + 1);
            let name = head.get(start..).unwrap_or_default();
            if native_names.contains(name) {
                let w = [
                    b"native overload of '".as_slice(),
                    name,
                    b"' does not match API declaration: ",
                    g,
                ]
                .concat();
                self.warnings.push(w);
            }
            let r = e.reg_func_raw(g, stub as *const c_void);
            if r >= 0 {
                mark_stub(e, r);
            }
            if !ok_reg(r) {
                fail(self, g, r);
            }
        }
        self.globals = globals;
        let props = std::mem::take(&mut self.global_props);
        for g in &props {
            let name = g.rsplit(|&c| c == b' ').next().unwrap_or_default();
            if e.has_global_prop(name) {
                continue;
            }
            // SAFETY: the block is intentionally never freed; it backs the script global for the engine's life.
            let mem = unsafe { libc_calloc(256) };
            // SAFETY: `mem` outlives the engine (leaked).
            let r = unsafe { e.reg_global_property(g, mem) };
            if !ok_reg(r) {
                fail(self, g, r);
            }
        }
        self.global_props = props;
    }
}

unsafe extern "C" {
    fn calloc(n: usize, size: usize) -> *mut c_void;
    fn free(p: *mut c_void);
}

/// # Safety
/// The returned block is zeroed and owned by the caller.
unsafe fn libc_calloc(size: usize) -> *mut c_void {
    // SAFETY: plain allocation.
    unsafe { calloc(1, size) }
}

fn set_active(a: &mut Api) {
    *lock(&ACTIVE) = std::ptr::from_mut(a) as usize;
}

pub(crate) fn clear_active(a: *const Api) {
    let mut g = lock(&ACTIVE);
    if *g == a as usize {
        *g = 0;
    }
}

pub(crate) fn active_native_decl(ty: Option<&[u8]>, decl: &[u8]) -> Vec<u8> {
    let p = *lock(&ACTIVE);
    // SAFETY: `ACTIVE` points at a live `Api` owned by the C++ forwarder until `clear_active`.
    match unsafe { (p as *mut Api).as_mut() } {
        Some(a) => a.native_decl(ty, decl),
        None => decl.to_vec(),
    }
}

fn mark_stub(e: Engine, id: c_int) {
    if let Some(f) = e.func_by_id(id) {
        f.set_user(1, STUB_USER_DATA);
    }
}

pub(crate) fn is_stub(f: Func) -> bool {
    f.user(STUB_USER_DATA) != 0
}

pub(crate) fn is_dummy(p: *mut c_void) -> bool {
    !p.is_null() && with_state(|s| s.dummies.contains(&(p as usize)))
}

fn memset0(p: *mut c_void, n: usize) {
    // SAFETY: callers pass engine-sized, writable memory.
    unsafe { std::ptr::write_bytes(p.cast::<u8>(), 0, n) };
}

fn construct_default_at(e: Engine, type_id: i32, mem: *mut c_void) {
    let Some(t) = e.type_by_id(type_id) else {
        memset0(mem, usize::try_from(e.primitive_size(type_id)).unwrap_or(0));
        return;
    };
    if t.flags() & s2_as::OBJ_POD != 0 {
        memset0(mem, usize::try_from(t.size()).unwrap_or(0));
        return;
    }
    let name = t.name();
    // SAFETY: `mem` is uninitialised storage for the string type.
    unsafe {
        if name == b"tString" {
            s2_as::ohpl_as2_str_ctor(mem, b"".as_ptr(), 0);
        } else if name == b"tWString" {
            s2_as::ohpl_as2_wstr_ctor(mem, [].as_ptr(), 0);
        } else {
            memset0(mem, usize::try_from(t.size()).unwrap_or(0));
        }
    }
}

fn default_instance(e: Engine, type_id: i32) -> *mut c_void {
    if let Some(&p) = with_state(|s| s.defaults.get(&type_id).copied()).as_ref() {
        return p as *mut c_void;
    }
    let t = e.type_by_id(type_id);
    let obj = match t {
        Some(t) if t.flags() & s2_as::OBJ_VALUE != 0 && t.flags() & s2_as::OBJ_POD == 0 => {
            e.create_object(t)
        }
        _ => {
            let size = t.map_or_else(|| e.primitive_size(type_id), Type::size);
            // SAFETY: plain allocation.
            unsafe { libc_calloc(usize::try_from(size).ok().filter(|&n| n > 0).unwrap_or(64)) }
        }
    };
    with_state(|s| s.defaults.insert(type_id, obj as usize));
    obj
}

fn apply_defaults(d: &s2_host::Defaults, obj: *mut u8) {
    // SAFETY: `obj` is a struct block of at least the defaults' size.
    unsafe {
        std::ptr::copy_nonoverlapping(d.bytes.as_ptr(), obj.add(16), d.bytes.len());
        for &off in d.string_offsets {
            *obj.offset(off as isize).cast::<*const c_void>() = s2_host::intern_empty();
        }
    }
}

fn construct_members(e: Engine, t: Type, obj: *mut u8) {
    for p in t.props() {
        if p.type_id & s2_as::TYPEID_OBJHANDLE != 0 {
            continue;
        }
        let Some(pt) = e.type_by_id(p.type_id) else {
            continue;
        };
        // SAFETY: properties lie within the zeroed struct block.
        let at = unsafe { obj.offset(p.offset as isize) };
        let n = pt.name();
        // SAFETY: `at` is uninitialised storage for the string type.
        unsafe {
            if n == b"tString" {
                s2_as::ohpl_as2_str_ctor(at.cast(), b"".as_ptr(), 0);
            } else if n == b"tWString" {
                s2_as::ohpl_as2_wstr_ctor(at.cast(), [].as_ptr(), 0);
            } else if let Some(d) = s2_host::struct_defaults(&n) {
                apply_defaults(&d, at);
            } else {
                construct_members(e, pt, at);
            }
        }
    }
}

fn dummy_for_return(e: Engine, f: Func, type_id: i32) -> *mut c_void {
    let Some(t) = e.type_by_id(type_id & !s2_as::TYPEID_OBJHANDLE) else {
        return std::ptr::null_mut();
    };
    let fl = t.flags();
    if fl & s2_as::OBJ_SCRIPT_OBJECT != 0 || fl & s2_as::OBJ_NOCOUNT == 0 {
        return std::ptr::null_mut();
    }
    if api::is_lookup_name(&f.name()) {
        return std::ptr::null_mut();
    }
    let tid = t.id();
    if let Some(p) = with_state(|s| s.dummy_by_type.get(&tid).copied()) {
        return p as *mut c_void;
    }
    // SAFETY: plain allocation.
    let p = unsafe { libc_calloc(4096) };
    construct_members(e, t, p.cast());
    with_state(|s| {
        s.dummy_by_type.insert(tid, p as usize);
        s.dummies.insert(p as usize);
    });
    p
}

pub(crate) fn dummy_of(e: Engine, ty: &[u8]) -> *mut c_void {
    if let Some(p) = with_state(|s| s.dummy_by_name.get(ty).copied()).filter(|&p| p != 0) {
        return p as *mut c_void;
    }
    let Some(t) = e.type_by_name(ty) else {
        return std::ptr::null_mut();
    };
    // SAFETY: plain allocation.
    let p = unsafe { libc_calloc(4096) };
    construct_members(e, t, p.cast());
    with_state(|s| {
        s.dummy_by_name.insert(ty.to_vec(), p as usize);
        s.dummies.insert(p as usize);
    });
    p
}

unsafe fn g_engine(g: *mut c_void) -> Engine {
    // SAFETY: `g` is the running generic frame.
    Engine(unsafe { s2_as::ohpl_as2_g_engine(g) })
}

unsafe fn g_func(g: *mut c_void) -> Func {
    // SAFETY: `g` is the running generic frame.
    Func(unsafe { s2_as::ohpl_as2_g_func(g) })
}

unsafe fn count_stub(g: *mut c_void) {
    // SAFETY: `g` is the running generic frame.
    let d = unsafe { g_func(g) }.decl(true, false, false);
    with_state(|s| *s.stub_counts.entry(d).or_insert(0) += 1);
}

fn release_struct(p: *mut c_void) {
    let key = p as usize;
    let free_it = with_state(|s| match s.struct_refs.get_mut(&key) {
        Some(n) => {
            *n -= 1;
            if *n == 0 {
                s.struct_refs.remove(&key);
                true
            } else {
                false
            }
        }
        None => false,
    });
    if free_it {
        // SAFETY: the block came from `calloc` in `new_script_struct` and the last reference is gone.
        unsafe { free(p) };
    }
}

/// # Safety
/// `g` is the running generic frame.
pub(crate) unsafe fn release_handle_args(g: *mut c_void) {
    // SAFETY: caller contract.
    let f = unsafe { g_func(g) };
    for i in 0..f.param_count() {
        let (id, fl) = f.param(i);
        if id & s2_as::TYPEID_OBJHANDLE != 0 && fl == 0 {
            // SAFETY: a handle argument slot holds an object pointer.
            release_struct(unsafe { *s2_as::ohpl_as2_g_arg_addr(g, i).cast::<*mut c_void>() });
        }
    }
}

/// # Safety
/// `g` is the running generic frame of a stubbed function.
pub(crate) unsafe fn stub_call(g: *mut c_void) {
    // SAFETY: caller contract for every call below.
    unsafe {
        count_stub(g);
        release_handle_args(g);
        let (f, e) = (g_func(g), g_engine(g));
        let (id, fl) = f.return_type();
        let ret = s2_as::ohpl_as2_g_ret_loc(g);
        match api::stub_return_kind(
            id == s2_as::TYPEID_VOID,
            fl & s2_as::TM_INOUTREF != 0,
            id & s2_as::TYPEID_OBJHANDLE != 0,
        ) {
            api::STUB_VOID => {}
            api::STUB_DEFAULT_INSTANCE => {
                s2_as::ohpl_as2_g_set_ret_addr(
                    g,
                    default_instance(e, id & !s2_as::TYPEID_OBJHANDLE),
                );
            }
            api::STUB_HANDLE => s2_as::ohpl_as2_g_set_ret_addr(g, dummy_for_return(e, f, id)),
            _ => construct_default_at(e, id, ret),
        }
    }
}

unsafe extern "C" fn stub(g: *mut c_void) {
    // SAFETY: the engine passes its generic frame.
    unsafe { stub_call(g) }
}

unsafe extern "C" fn stub_construct(g: *mut c_void) {
    // SAFETY: the engine passes its generic frame of a constructor.
    unsafe {
        count_stub(g);
        let (f, e) = (g_func(g), g_engine(g));
        if let Some(t) = f.object_type() {
            construct_default_at(e, t.id(), s2_as::ohpl_as2_g_object(g));
        }
    }
}

unsafe extern "C" fn copy_construct_pod(g: *mut c_void) {
    // SAFETY: the engine passes its generic frame of a POD copy constructor.
    unsafe {
        let size = g_func(g).object_type().map_or(0, Type::size);
        std::ptr::copy_nonoverlapping(
            s2_as::ohpl_as2_g_arg_object(g, 0).cast::<u8>(),
            s2_as::ohpl_as2_g_object(g).cast::<u8>(),
            usize::try_from(size).unwrap_or(0),
        );
    }
}

pub(crate) fn new_script_struct(ty: &[u8]) -> *mut c_void {
    // SAFETY: plain allocation.
    let p = unsafe { libc_calloc(4096) };
    if let Some(d) = s2_host::struct_defaults(ty) {
        apply_defaults(&d, p.cast());
    }
    p
}

pub(crate) fn new_owned_script_struct(ty: &[u8]) -> *mut c_void {
    let p = new_script_struct(ty);
    with_state(|s| s.struct_refs.insert(p as usize, 1));
    p
}

unsafe extern "C" fn struct_add_ref(g: *mut c_void) {
    // SAFETY: the engine passes its generic frame.
    let o = unsafe { s2_as::ohpl_as2_g_object(g) } as usize;
    with_state(|s| {
        if let Some(n) = s.struct_refs.get_mut(&o) {
            *n += 1;
        }
    });
}

unsafe extern "C" fn struct_release(g: *mut c_void) {
    // SAFETY: the engine passes its generic frame.
    release_struct(unsafe { s2_as::ohpl_as2_g_object(g) });
}

const ZEROED_FACTORIES: [&[u8]; 5] = [
    b"cLuxClosestEntityData",
    b"cLuxClosestCharCollider",
    b"cLuxSoundExtraData",
    b"cLuxScreenTextFormatParameters",
    b"cPhysicsContactData",
];

unsafe extern "C" fn stub_factory(g: *mut c_void) {
    // SAFETY: the engine passes its generic frame of a factory.
    unsafe {
        let (f, e) = (g_func(g), g_engine(g));
        let t = e.type_by_id(f.return_type().0 & !s2_as::TYPEID_OBJHANDLE);
        let name = t.map(Type::name);
        let defaults = name.as_deref().and_then(s2_host::struct_defaults);
        let zeroed = name
            .as_deref()
            .is_some_and(|n| ZEROED_FACTORIES.contains(&n));
        if (defaults.is_none() && !zeroed) || s2_as::ohpl_as2_g_arg_count(g) > 0 {
            count_stub(g);
        }
        let obj = libc_calloc(4096).cast::<u8>();
        if let Some(d) = &defaults {
            apply_defaults(d, obj);
        } else if let Some(t) = t {
            construct_members(e, t, obj);
        }
        if name.as_deref() == Some(b"cLuxScreenTextFormatParameters") {
            for o in [16usize, 360, 364, 368] {
                obj.add(o).cast::<f32>().write_unaligned(-1.0);
            }
        }
        if t.is_some_and(|t| t.flags() & s2_as::OBJ_NOCOUNT == 0) {
            with_state(|s| s.struct_refs.insert(obj as usize, 1));
        }
        s2_as::ohpl_as2_g_ret_loc(g).cast::<*mut u8>().write(obj);
    }
}

fn assign_members(e: Engine, t: Type, dst: *mut u8, src: *const u8) {
    for p in t.props() {
        // SAFETY: properties lie within the struct blocks.
        let (d, s) = unsafe { (dst.offset(p.offset as isize), src.offset(p.offset as isize)) };
        if p.type_id & s2_as::TYPEID_OBJHANDLE != 0 {
            // SAFETY: a handle member is one pointer.
            unsafe {
                d.cast::<*const u8>()
                    .write_unaligned(s.cast::<*const u8>().read_unaligned())
            };
            continue;
        }
        match e.type_by_id(p.type_id) {
            None => {
                let n = usize::try_from(e.primitive_size(p.type_id)).unwrap_or(0);
                // SAFETY: both blocks hold the primitive.
                unsafe { std::ptr::copy(s, d, n) };
            }
            Some(pt) => {
                let n = pt.name();
                // SAFETY: the members were constructed by `construct_members`.
                unsafe {
                    if n == b"tString" {
                        s2_as::ohpl_as2_str_assign(
                            d.cast(),
                            s2_as::str_bytes(s.cast()).as_ptr(),
                            s2_as::str_bytes(s.cast()).len(),
                        );
                    } else if n == b"tWString" {
                        let u = s2_as::wstr_units(s.cast());
                        s2_as::ohpl_as2_wstr_assign(d.cast(), u.as_ptr(), u.len());
                    } else if pt.flags() & s2_as::OBJ_POD != 0 {
                        std::ptr::copy(s, d, usize::try_from(pt.size()).unwrap_or(0));
                    } else {
                        assign_members(e, pt, d, s);
                    }
                }
            }
        }
    }
}

unsafe extern "C" fn member_assign(g: *mut c_void) {
    // SAFETY: the engine passes its generic frame of `opAssign`.
    unsafe {
        let (f, e) = (g_func(g), g_engine(g));
        let this = s2_as::ohpl_as2_g_object(g);
        let arg = s2_as::ohpl_as2_g_arg_object(g, 0);
        if let Some(t) = f.object_type() {
            if let Some(d) = s2_host::struct_defaults(&t.name()) {
                if this != arg {
                    std::ptr::copy_nonoverlapping(
                        arg.cast::<u8>().add(16),
                        this.cast::<u8>().add(16),
                        d.size - 16,
                    );
                }
            } else {
                assign_members(e, t, this.cast(), arg.cast());
            }
        }
        s2_as::ohpl_as2_g_set_ret_addr(g, this);
    }
}

unsafe extern "C" fn stub_cast_self(g: *mut c_void) {
    // SAFETY: the engine passes its generic frame.
    unsafe {
        s2_as::ohpl_as2_g_ret_loc(g)
            .cast::<*mut c_void>()
            .write(s2_as::ohpl_as2_g_object(g))
    };
}

pub(crate) fn set_indirect_props(ty: &[u8], pointer_offset: i32) {
    with_state(|s| s.indirect.entry(ty.to_vec()).or_default().pointer_offset = pointer_offset);
}

pub(crate) fn indirect_prop_offset(ty: &[u8], name: &[u8]) -> i32 {
    with_state(|s| {
        s.indirect
            .entry(ty.to_vec())
            .or_default()
            .offsets
            .get(name)
            .copied()
            .unwrap_or(-1)
    })
}

pub(crate) fn new_prop_block(ty: &[u8]) -> *mut c_void {
    let (size, strings) = with_state(|s| {
        let p = s.indirect.entry(ty.to_vec()).or_default();
        (p.size, p.string_offsets.clone())
    });
    // SAFETY: plain allocation; the string members are constructed in place.
    unsafe {
        let b = libc_calloc(usize::try_from(size).unwrap_or(0) + 64);
        for o in strings {
            s2_as::ohpl_as2_str_ctor(b.cast::<u8>().offset(o as isize).cast(), b"".as_ptr(), 0);
        }
        b
    }
}

pub(crate) fn free_prop_block(ty: &[u8], b: *mut c_void) {
    if b.is_null() {
        return;
    }
    let strings = with_state(|s| {
        s.indirect
            .entry(ty.to_vec())
            .or_default()
            .string_offsets
            .clone()
    });
    // SAFETY: the block came from `new_prop_block`.
    unsafe {
        for o in strings {
            s2_as::ohpl_as2_str_dtor(b.cast::<u8>().offset(o as isize).cast());
        }
        free(b);
    }
}

pub(crate) fn stub_counts() -> Vec<(Vec<u8>, i32)> {
    with_state(|s| s.stub_counts.iter().map(|(k, v)| (k.clone(), *v)).collect())
}

impl Api {
    pub fn errors(&self) -> &[Vec<u8>] {
        &self.errors
    }

    pub fn warnings(&self) -> &[Vec<u8>] {
        &self.warnings
    }

    pub fn remaps(&self) -> &[Vec<u8>] {
        &self.remaps
    }
}
