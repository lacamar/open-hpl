//! S2: AngelScript access for the script natives: the `asIScriptGeneric` frame, native registration
//! with the generic calling convention, and engine/type queries (all through `cshim/as_shim_s2.cpp`).
#![cfg_attr(test, allow(dead_code))]

use super::s2_api;
use std::ffi::{CStr, CString, c_char, c_int, c_void};

pub const ALREADY_REGISTERED: c_int = -13;
pub const OBJ_REF: u32 = 1;
pub const OBJ_VALUE: u32 = 1 << 1;
pub const OBJ_NOCOUNT: u32 = 1 << 18;
pub const OBJ_POD: u32 = 1 << 3;
pub const OBJ_SCRIPT_OBJECT: u32 = 1 << 21;
pub const OBJ_ENUM: u32 = 1 << 26;
pub const TYPEID_BOOL: i32 = 1;
pub const TYPEID_FLOAT: i32 = 10;
pub const TYPEID_DOUBLE: i32 = 11;
pub const OBJ_APP_PRIMITIVE: u64 = 1 << 13;
pub const TYPEID_OBJHANDLE: i32 = 0x4000_0000;
pub const TYPEID_VOID: i32 = 0;
pub const TM_INOUTREF: u32 = 3;
pub const BEHAVE_CONSTRUCT: c_int = 0;
pub const BEHAVE_DESTRUCT: c_int = 2;
pub const BEHAVE_FACTORY: c_int = 3;
pub const BEHAVE_ADDREF: c_int = 5;
pub const BEHAVE_RELEASE: c_int = 6;

// SAFETY (all blocks below): plain calls into the shim; pointers are valid for the call.
unsafe extern "C" {
    pub fn ohpl_as2_g_func(g: *mut c_void) -> *mut c_void;
    pub fn ohpl_as2_g_engine(g: *mut c_void) -> *mut c_void;
    pub fn ohpl_as2_g_object(g: *mut c_void) -> *mut c_void;
    pub fn ohpl_as2_g_arg_object(g: *mut c_void, i: u32) -> *mut c_void;
    pub fn ohpl_as2_g_arg_addr(g: *mut c_void, i: u32) -> *mut c_void;
    pub fn ohpl_as2_g_ret_loc(g: *mut c_void) -> *mut c_void;
    pub fn ohpl_as2_g_set_ret_addr(g: *mut c_void, p: *mut c_void);
    pub fn ohpl_as2_g_arg_count(g: *mut c_void) -> u32;
    pub fn ohpl_as2_g_arg_type_id(g: *mut c_void, i: u32) -> c_int;
    pub fn ohpl_as2_g_aux(g: *mut c_void) -> *mut c_void;
    fn ohpl_as2_reg_global_function(
        e: *mut c_void,
        decl: *const c_char,
        f: *const c_void,
        aux: *mut c_void,
    ) -> c_int;
    fn ohpl_as2_reg_object_method(
        e: *mut c_void,
        ty: *const c_char,
        decl: *const c_char,
        f: *const c_void,
        aux: *mut c_void,
    ) -> c_int;
    fn ohpl_as2_reg_object_behaviour(
        e: *mut c_void,
        b: c_int,
        ty: *const c_char,
        decl: *const c_char,
        f: *const c_void,
        aux: *mut c_void,
    ) -> c_int;
    fn ohpl_as2_reg_object_property(
        e: *mut c_void,
        ty: *const c_char,
        decl: *const c_char,
        offset: c_int,
        composite: c_int,
        indirect: c_int,
    ) -> c_int;
    fn ohpl_as2_reg_interface(e: *mut c_void, name: *const c_char) -> c_int;
    fn ohpl_as2_reg_interface_method(e: *mut c_void, i: *const c_char, d: *const c_char) -> c_int;
    fn ohpl_as2_reg_string_factory(e: *mut c_void, ty: *const c_char) -> c_int;
    fn ohpl_as2_type_by_name(e: *mut c_void, name: *const c_char) -> *mut c_void;
    fn ohpl_as2_type_by_id(e: *mut c_void, id: c_int) -> *mut c_void;
    fn ohpl_as2_func_by_id(e: *mut c_void, id: c_int) -> *mut c_void;
    fn ohpl_as2_global_func_by_decl(e: *mut c_void, d: *const c_char) -> *mut c_void;
    fn ohpl_as2_global_func_count(e: *mut c_void) -> u32;
    fn ohpl_as2_global_func_by_index(e: *mut c_void, i: u32) -> *mut c_void;
    fn ohpl_as2_global_prop_index(e: *mut c_void, name: *const c_char) -> c_int;
    fn ohpl_as2_primitive_size(e: *mut c_void, id: c_int) -> c_int;
    fn ohpl_as2_create_object(e: *mut c_void, t: *mut c_void) -> *mut c_void;
    fn ohpl_as2_type_flags(t: *mut c_void) -> u32;
    fn ohpl_as2_type_size(t: *mut c_void) -> c_int;
    fn ohpl_as2_type_id(t: *mut c_void) -> c_int;
    fn ohpl_as2_type_name(t: *mut c_void) -> *const c_char;
    fn ohpl_as2_type_prop_count(t: *mut c_void) -> u32;
    fn ohpl_as2_type_prop(
        t: *mut c_void,
        i: u32,
        name: *mut *const c_char,
        id: *mut c_int,
        off: *mut c_int,
    ) -> c_int;
    fn ohpl_as2_type_method_by_decl(t: *mut c_void, d: *const c_char) -> *mut c_void;
    fn ohpl_as2_func_name(f: *mut c_void) -> *const c_char;
    fn ohpl_as2_func_decl(f: *mut c_void, obj: c_int, ns: c_int, names: c_int) -> *const c_char;
    fn ohpl_as2_func_return(f: *mut c_void, flags: *mut u32) -> c_int;
    fn ohpl_as2_func_param_count(f: *mut c_void) -> u32;
    fn ohpl_as2_func_param(f: *mut c_void, i: u32, id: *mut c_int, fl: *mut u32) -> c_int;
    fn ohpl_as2_func_object_type(f: *mut c_void) -> *mut c_void;
    fn ohpl_as2_func_set_user(f: *mut c_void, p: *mut c_void, key: usize);
    fn ohpl_as2_func_user(f: *mut c_void, key: usize) -> *mut c_void;
    pub fn ohpl_as2_str_ctor(mem: *mut c_void, p: *const u8, n: usize);
    pub fn ohpl_as2_str_dtor(s: *mut c_void);
    pub fn ohpl_as2_str_assign(s: *mut c_void, p: *const u8, n: usize);
    pub fn ohpl_as2_str_append(s: *mut c_void, p: *const u8, n: usize);
    pub fn ohpl_as2_str_resize(s: *mut c_void, n: usize);
    pub fn ohpl_as2_wstr_ctor(mem: *mut c_void, p: *const u32, n: usize);
    pub fn ohpl_as2_wstr_dtor(s: *mut c_void);
    pub fn ohpl_as2_wstr_assign(s: *mut c_void, p: *const u32, n: usize);
    pub fn ohpl_as2_wstr_append(s: *mut c_void, p: *const u32, n: usize);
    pub fn ohpl_as2_wstr_resize(s: *mut c_void, n: usize);
    pub fn ohpl_as2_wstr_data(s: *const c_void) -> *const u32;
    pub fn ohpl_as2_wstr_len(s: *const c_void) -> usize;
    pub fn ohpl_as2_dump_decls(e: *mut c_void, path: *const c_char) -> c_int;
    pub fn ohpl_as2_call_corpus(
        e: *mut c_void,
        seed: u32,
        calls: u32,
        path: *const c_char,
    ) -> c_int;
    fn ohpl_as_str_data(s: *const c_void) -> *const c_char;
    fn ohpl_as_str_len(s: *const c_void) -> usize;
    fn ohpl_as_reg_object_type(e: *mut c_void, n: *const c_char, size: c_int, flags: u64) -> c_int;
    fn ohpl_as_reg_enum(e: *mut c_void, n: *const c_char) -> c_int;
    fn ohpl_as_reg_enum_value(
        e: *mut c_void,
        n: *const c_char,
        v: *const c_char,
        x: c_int,
    ) -> c_int;
    fn ohpl_as_reg_global_property(e: *mut c_void, d: *const c_char, p: *mut c_void) -> c_int;
    fn ohpl_as_set_engine_property(e: *mut c_void, prop: c_int, v: usize) -> c_int;
}

pub const MAX_ARGS: usize = 16;

/// The arguments of one native call as the engine laid them out (`GetAddressOfArg` per slot).
pub struct Frame {
    pub this: *mut c_void,
    addr: [*mut c_void; MAX_ARGS],
    objs: [*mut c_void; MAX_ARGS],
    pub ret: *mut c_void,
    pub ret_addr: std::cell::Cell<Option<*mut c_void>>,
}

impl Frame {
    /// # Safety
    /// `g` is the `asIScriptGeneric*` of the running call.
    unsafe fn from_generic(g: *mut c_void) -> Self {
        let mut f = Self::mock(std::ptr::null_mut(), std::ptr::null_mut());
        // SAFETY: caller contract; the accessors only read the call frame.
        unsafe {
            f.this = ohpl_as2_g_object(g);
            f.ret = ohpl_as2_g_ret_loc(g);
            let n = (ohpl_as2_g_arg_count(g) as usize).min(MAX_ARGS);
            for (i, slot) in f.addr.iter_mut().enumerate().take(n) {
                *slot = ohpl_as2_g_arg_addr(g, i as u32);
            }
            for (i, slot) in f.objs.iter_mut().enumerate().take(n) {
                *slot = ohpl_as2_g_arg_object(g, i as u32);
            }
        }
        f
    }

    pub fn mock(this: *mut c_void, ret: *mut c_void) -> Self {
        Self {
            this,
            addr: [std::ptr::null_mut(); MAX_ARGS],
            objs: [std::ptr::null_mut(); MAX_ARGS],
            ret,
            ret_addr: std::cell::Cell::new(None),
        }
    }

    #[cfg(test)]
    pub(crate) fn set_slot(&mut self, i: usize, p: *mut c_void) {
        if let Some(s) = self.addr.get_mut(i) {
            *s = p;
        }
    }

    fn slot(&self, i: usize) -> *mut c_void {
        self.addr.get(i).copied().unwrap_or(std::ptr::null_mut())
    }

    /// A value-type argument passed by value (`GetArgObject`).
    pub fn obj_arg(&self, i: usize) -> *mut c_void {
        self.objs.get(i).copied().unwrap_or(std::ptr::null_mut())
    }

    /// `const T &in` argument.
    pub fn rf<T>(&self, i: usize) -> &T {
        // SAFETY: the registered declaration passes a reference to a live `T` in slot `i`.
        unsafe { &*(*self.slot(i).cast::<*const T>()) }
    }

    /// `T &out` / `T &inout` argument.
    #[allow(clippy::mut_from_ref)]
    pub fn rm<T>(&self, i: usize) -> &mut T {
        // SAFETY: as `rf`; the engine hands out the only reference for the call.
        unsafe { &mut *(*self.slot(i).cast::<*mut T>()) }
    }

    #[allow(clippy::mut_from_ref)]
    pub fn me<T>(&self) -> &mut T {
        // SAFETY: a method registration always has its object.
        unsafe { &mut *self.this.cast::<T>() }
    }

    pub fn ret<T>(&self, v: T) {
        // SAFETY: the return location is storage for the declared return type.
        unsafe { self.ret.cast::<T>().write(v) }
    }

    /// Constructor result: the object memory is `this`.
    pub fn ret_at<T>(&self, v: T) {
        // SAFETY: a constructor registration gets uninitialised storage for the type.
        unsafe { self.this.cast::<T>().write(v) }
    }

    /// The pointer an `&in`/handle argument holds (the referenced object).
    pub fn ptr_arg(&self, i: usize) -> *mut c_void {
        // SAFETY: the slot holds a pointer.
        unsafe { *self.slot(i).cast::<*mut c_void>() }
    }

    pub fn ret_string(&self, b: &[u8]) {
        // SAFETY: the return location is uninitialised storage for a `tString`.
        unsafe { ohpl_as2_str_ctor(self.ret, b.as_ptr(), b.len()) }
    }

    pub fn ret_wstring(&self, u: &[u32]) {
        // SAFETY: the return location is uninitialised storage for a `tWString`.
        unsafe { ohpl_as2_wstr_ctor(self.ret, u.as_ptr(), u.len()) }
    }

    /// Primitive or enum argument stored in its slot.
    pub fn arg<T: Copy>(&self, i: usize) -> T {
        // SAFETY: primitives and enums live in their argument slot.
        unsafe { *self.slot(i).cast::<T>() }
    }

    pub fn ret_ref<T>(&self, p: *mut T) {
        self.ret_addr.set(Some(p.cast()));
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Kind {
    Func,
    Method,
    Plain,
}

type NativeFn = Box<dyn Fn(&Frame)>;

pub struct Native {
    kind: Kind,
    f: NativeFn,
}

unsafe extern "C" fn tramp(g: *mut c_void) {
    // SAFETY: the engine passes its generic frame; `aux` is the leaked `Native` of the registration.
    unsafe {
        let Some(n) = ohpl_as2_g_aux(g).cast::<Native>().as_ref() else {
            return;
        };
        if n.kind == Kind::Method && s2_api::is_dummy(ohpl_as2_g_object(g)) {
            s2_api::stub_call(g);
            return;
        }
        let fr = Frame::from_generic(g);
        (n.f)(&fr);
        if let Some(p) = fr.ret_addr.get() {
            ohpl_as2_g_set_ret_addr(g, p);
        }
    }
}

pub fn cs(s: &[u8]) -> CString {
    CString::new(s.iter().copied().filter(|&b| b != 0).collect::<Vec<u8>>()).unwrap_or_default()
}

#[derive(Clone, Copy)]
pub struct Engine(pub *mut c_void);

impl Engine {
    fn leak(kind: Kind, f: NativeFn) -> *mut c_void {
        Box::into_raw(Box::new(Native { kind, f })).cast()
    }

    #[cfg(test)]
    pub fn reg_func(self, decl: &[u8], f: NativeFn) -> c_int {
        testreg::record(None, decl, f)
    }

    #[cfg(not(test))]
    pub fn reg_func(self, decl: &[u8], f: NativeFn) -> c_int {
        let d = cs(decl);
        // SAFETY: strings outlive the call; `tramp` is a generic-convention function.
        unsafe {
            ohpl_as2_reg_global_function(
                self.0,
                d.as_ptr(),
                tramp as *const c_void,
                Self::leak(Kind::Func, f),
            )
        }
    }

    #[cfg(test)]
    pub fn reg_method(self, ty: &[u8], decl: &[u8], _kind: Kind, f: NativeFn) -> c_int {
        testreg::record(Some(ty), decl, f)
    }

    #[cfg(not(test))]
    pub fn reg_method(self, ty: &[u8], decl: &[u8], kind: Kind, f: NativeFn) -> c_int {
        let (t, d) = (cs(ty), cs(decl));
        // SAFETY: as `reg_func`.
        unsafe {
            ohpl_as2_reg_object_method(
                self.0,
                t.as_ptr(),
                d.as_ptr(),
                tramp as *const c_void,
                Self::leak(kind, f),
            )
        }
    }

    #[cfg(test)]
    pub fn reg_behaviour(self, _b: c_int, ty: &[u8], decl: &[u8], f: NativeFn) -> c_int {
        testreg::record(Some(ty), decl, f)
    }

    #[cfg(not(test))]
    pub fn reg_behaviour(self, b: c_int, ty: &[u8], decl: &[u8], f: NativeFn) -> c_int {
        let (t, d) = (cs(ty), cs(decl));
        // SAFETY: as `reg_func`.
        let r = unsafe {
            ohpl_as2_reg_object_behaviour(
                self.0,
                b,
                t.as_ptr(),
                d.as_ptr(),
                tramp as *const c_void,
                Self::leak(Kind::Plain, f),
            )
        };
        if r < 0 && std::env::var_os("OHPL_S2_TRACE").is_some() {
            eprintln!(
                "s2 behaviour {} {}: {r}",
                String::from_utf8_lossy(ty),
                String::from_utf8_lossy(decl)
            );
        }
        r
    }

    /// A registration whose function is supplied by C++ (`void(asIScriptGeneric*)`).
    pub fn reg_func_raw(self, decl: &[u8], f: *const c_void) -> c_int {
        let d = cs(decl);
        // SAFETY: `f` is a generic-convention function.
        unsafe { ohpl_as2_reg_global_function(self.0, d.as_ptr(), f, std::ptr::null_mut()) }
    }

    pub fn reg_method_raw(self, ty: &[u8], decl: &[u8], f: *const c_void) -> c_int {
        let (t, d) = (cs(ty), cs(decl));
        // SAFETY: as `reg_func_raw`.
        unsafe {
            ohpl_as2_reg_object_method(self.0, t.as_ptr(), d.as_ptr(), f, std::ptr::null_mut())
        }
    }

    pub fn reg_behaviour_raw(self, b: c_int, ty: &[u8], decl: &[u8], f: *const c_void) -> c_int {
        let (t, d) = (cs(ty), cs(decl));
        // SAFETY: as `reg_func_raw`.
        unsafe {
            ohpl_as2_reg_object_behaviour(
                self.0,
                b,
                t.as_ptr(),
                d.as_ptr(),
                f,
                std::ptr::null_mut(),
            )
        }
    }

    pub fn reg_property(
        self,
        ty: &[u8],
        decl: &[u8],
        offset: i32,
        composite: i32,
        indirect: bool,
    ) -> c_int {
        let (t, d) = (cs(ty), cs(decl));
        // SAFETY: strings outlive the call.
        unsafe {
            ohpl_as2_reg_object_property(
                self.0,
                t.as_ptr(),
                d.as_ptr(),
                offset,
                composite,
                c_int::from(indirect),
            )
        }
    }

    pub fn reg_interface(self, name: &[u8]) -> c_int {
        let n = cs(name);
        // SAFETY: string outlives the call.
        unsafe { ohpl_as2_reg_interface(self.0, n.as_ptr()) }
    }

    pub fn reg_interface_method(self, i: &[u8], decl: &[u8]) -> c_int {
        let (i, d) = (cs(i), cs(decl));
        // SAFETY: strings outlive the call.
        unsafe { ohpl_as2_reg_interface_method(self.0, i.as_ptr(), d.as_ptr()) }
    }

    pub fn reg_string_factory(self, ty: &[u8]) -> c_int {
        let t = cs(ty);
        // SAFETY: string outlives the call.
        unsafe { ohpl_as2_reg_string_factory(self.0, t.as_ptr()) }
    }

    pub fn reg_object_type(self, name: &[u8], size: i32, flags: u64) -> c_int {
        let n = cs(name);
        // SAFETY: string outlives the call.
        unsafe { ohpl_as_reg_object_type(self.0, n.as_ptr(), size, flags) }
    }

    pub fn reg_enum(self, name: &[u8]) -> c_int {
        let n = cs(name);
        // SAFETY: string outlives the call.
        unsafe { ohpl_as_reg_enum(self.0, n.as_ptr()) }
    }

    pub fn reg_enum_value(self, name: &[u8], value: &[u8], v: i32) -> c_int {
        let (n, s) = (cs(name), cs(value));
        // SAFETY: strings outlive the call.
        unsafe { ohpl_as_reg_enum_value(self.0, n.as_ptr(), s.as_ptr(), v) }
    }

    /// # Safety
    /// `p` outlives the engine.
    #[cfg(test)]
    pub unsafe fn reg_global_property(self, _decl: &[u8], _p: *mut c_void) -> c_int {
        0
    }

    /// # Safety
    /// `p` outlives the engine.
    #[cfg(not(test))]
    pub unsafe fn reg_global_property(self, decl: &[u8], p: *mut c_void) -> c_int {
        let d = cs(decl);
        // SAFETY: caller contract.
        unsafe { ohpl_as_reg_global_property(self.0, d.as_ptr(), p) }
    }

    pub fn set_property(self, prop: c_int, v: usize) {
        // SAFETY: plain call.
        unsafe { ohpl_as_set_engine_property(self.0, prop, v) };
    }

    pub fn type_by_name(self, name: &[u8]) -> Option<Type> {
        let n = cs(name);
        // SAFETY: string outlives the call.
        Type::new(unsafe { ohpl_as2_type_by_name(self.0, n.as_ptr()) })
    }

    pub fn type_by_id(self, id: i32) -> Option<Type> {
        // SAFETY: plain call.
        Type::new(unsafe { ohpl_as2_type_by_id(self.0, id) })
    }

    pub fn func_by_id(self, id: i32) -> Option<Func> {
        // SAFETY: plain call.
        Func::new(unsafe { ohpl_as2_func_by_id(self.0, id) })
    }

    pub fn global_func_by_decl(self, decl: &[u8]) -> Option<Func> {
        let d = cs(decl);
        // SAFETY: string outlives the call.
        Func::new(unsafe { ohpl_as2_global_func_by_decl(self.0, d.as_ptr()) })
    }

    pub fn global_func_names(self) -> Vec<Vec<u8>> {
        // SAFETY: plain calls; indices are in range.
        let n = unsafe { ohpl_as2_global_func_count(self.0) };
        (0..n)
            .filter_map(|i| Func::new(unsafe { ohpl_as2_global_func_by_index(self.0, i) }))
            .map(Func::name)
            .collect()
    }

    pub fn has_global_prop(self, name: &[u8]) -> bool {
        let n = cs(name);
        // SAFETY: string outlives the call.
        unsafe { ohpl_as2_global_prop_index(self.0, n.as_ptr()) >= 0 }
    }

    pub fn primitive_size(self, id: i32) -> i32 {
        // SAFETY: plain call.
        unsafe { ohpl_as2_primitive_size(self.0, id) }
    }

    pub fn create_object(self, t: Type) -> *mut c_void {
        // SAFETY: plain call.
        unsafe { ohpl_as2_create_object(self.0, t.0) }
    }
}

#[derive(Clone, Copy)]
pub struct Type(pub *mut c_void);

pub struct Prop {
    pub name: Vec<u8>,
    pub type_id: i32,
    pub offset: i32,
}

impl Type {
    fn new(p: *mut c_void) -> Option<Self> {
        (!p.is_null()).then_some(Self(p))
    }

    pub fn flags(self) -> u32 {
        // SAFETY: plain call.
        unsafe { ohpl_as2_type_flags(self.0) }
    }

    pub fn size(self) -> i32 {
        // SAFETY: plain call.
        unsafe { ohpl_as2_type_size(self.0) }
    }

    pub fn id(self) -> i32 {
        // SAFETY: plain call.
        unsafe { ohpl_as2_type_id(self.0) }
    }

    pub fn name(self) -> Vec<u8> {
        // SAFETY: the engine returns a NUL-terminated name.
        unsafe { CStr::from_ptr(ohpl_as2_type_name(self.0)) }
            .to_bytes()
            .to_vec()
    }

    pub fn props(self) -> Vec<Prop> {
        // SAFETY: plain calls; indices are in range and names NUL-terminated.
        unsafe {
            (0..ohpl_as2_type_prop_count(self.0))
                .map(|i| {
                    let (mut name, mut id, mut off) = (std::ptr::null(), 0, 0);
                    ohpl_as2_type_prop(self.0, i, &mut name, &mut id, &mut off);
                    let name = if name.is_null() {
                        Vec::new()
                    } else {
                        CStr::from_ptr(name).to_bytes().to_vec()
                    };
                    Prop {
                        name,
                        type_id: id,
                        offset: off,
                    }
                })
                .collect()
        }
    }

    pub fn has_method(self, decl: &[u8]) -> bool {
        let d = cs(decl);
        // SAFETY: string outlives the call.
        unsafe { !ohpl_as2_type_method_by_decl(self.0, d.as_ptr()).is_null() }
    }
}

#[derive(Clone, Copy)]
pub struct Func(pub *mut c_void);

impl Func {
    fn new(p: *mut c_void) -> Option<Self> {
        (!p.is_null()).then_some(Self(p))
    }

    pub fn name(self) -> Vec<u8> {
        // SAFETY: the engine returns a NUL-terminated name.
        unsafe { CStr::from_ptr(ohpl_as2_func_name(self.0)) }
            .to_bytes()
            .to_vec()
    }

    pub fn decl(self, obj: bool, ns: bool, names: bool) -> Vec<u8> {
        // SAFETY: the engine returns a NUL-terminated declaration.
        unsafe {
            CStr::from_ptr(ohpl_as2_func_decl(
                self.0,
                obj.into(),
                ns.into(),
                names.into(),
            ))
        }
        .to_bytes()
        .to_vec()
    }

    pub fn return_type(self) -> (i32, u32) {
        let mut fl = 0;
        // SAFETY: plain call.
        let id = unsafe { ohpl_as2_func_return(self.0, &mut fl) };
        (id, fl)
    }

    pub fn param_count(self) -> u32 {
        // SAFETY: plain call.
        unsafe { ohpl_as2_func_param_count(self.0) }
    }

    pub fn param(self, i: u32) -> (i32, u32) {
        let (mut id, mut fl) = (0, 0);
        // SAFETY: plain call.
        unsafe { ohpl_as2_func_param(self.0, i, &mut id, &mut fl) };
        (id, fl)
    }

    pub fn object_type(self) -> Option<Type> {
        // SAFETY: plain call.
        Type::new(unsafe { ohpl_as2_func_object_type(self.0) })
    }

    pub fn set_user(self, p: usize, key: usize) {
        // SAFETY: plain call; the value is only compared against null.
        unsafe { ohpl_as2_func_set_user(self.0, p as *mut c_void, key) };
    }

    pub fn user(self, key: usize) -> usize {
        // SAFETY: plain call.
        unsafe { ohpl_as2_func_user(self.0, key) as usize }
    }
}

/// The bytes of a `std::string` argument.
///
/// # Safety
/// `p` is a live `std::string`.
pub unsafe fn str_bytes<'a>(p: *const c_void) -> &'a [u8] {
    // SAFETY: caller contract.
    unsafe {
        let n = ohpl_as_str_len(p);
        if n == 0 {
            &[]
        } else {
            std::slice::from_raw_parts(ohpl_as_str_data(p).cast::<u8>(), n)
        }
    }
}

/// The units of a `std::wstring` argument.
///
/// # Safety
/// `p` is a live `std::wstring`.
pub unsafe fn wstr_units<'a>(p: *const c_void) -> &'a [u32] {
    // SAFETY: caller contract.
    unsafe {
        let n = ohpl_as2_wstr_len(p);
        if n == 0 {
            &[]
        } else {
            std::slice::from_raw_parts(ohpl_as2_wstr_data(p), n)
        }
    }
}

/// `SOMA_FUNC` and friends: registration through the recovered declarations.
pub struct Soma(pub Engine);

impl Soma {
    pub fn func(&self, decl: &str, f: impl Fn(&Frame) + 'static) {
        let d = s2_api::active_native_decl(None, decl.as_bytes());
        self.0.reg_func(&d, Box::new(f));
    }

    pub fn func_new(&self, decl: &str, f: impl Fn(&Frame) + 'static) {
        let d = s2_api::active_native_decl(None, decl.as_bytes());
        #[cfg(not(test))]
        if self.0.global_func_by_decl(&d).is_some() {
            return;
        }
        self.0.reg_func(&d, Box::new(f));
    }

    pub fn method(&self, ty: &str, decl: &str, f: impl Fn(&Frame) + 'static) {
        let d = s2_api::active_native_decl(Some(ty.as_bytes()), decl.as_bytes());
        self.0
            .reg_method(ty.as_bytes(), &d, Kind::Method, Box::new(f));
    }

    pub fn construct(&self, ty: &str, decl: &str, f: impl Fn(&Frame) + 'static) {
        self.0.reg_behaviour(
            BEHAVE_CONSTRUCT,
            ty.as_bytes(),
            decl.as_bytes(),
            Box::new(f),
        );
    }

    /// A direct registration (`asCALL_CDECL*` natives): no recovered-declaration remap, no stub fallback.
    pub fn plain(&self, ty: &str, decl: &str, f: impl Fn(&Frame) + 'static) {
        self.0
            .reg_method(ty.as_bytes(), decl.as_bytes(), Kind::Plain, Box::new(f));
    }
}

/// The NUL-terminated data of a `std::string`.
///
/// # Safety
/// `p` is a live `std::string`.
pub unsafe fn str_cptr(p: *const c_void) -> *const c_char {
    // SAFETY: caller contract.
    unsafe { ohpl_as_str_data(p) }
}

#[cfg(test)]
pub(crate) mod testreg {
    use super::{Frame, NativeFn};
    use std::cell::RefCell;
    use std::ffi::c_int;

    pub(crate) type Entry = (Option<Vec<u8>>, Vec<u8>, NativeFn);

    thread_local! {
        pub(crate) static REG: RefCell<Vec<Entry>> = const { RefCell::new(Vec::new()) };
    }

    pub(super) fn record(ty: Option<&[u8]>, decl: &[u8], f: NativeFn) -> c_int {
        REG.with(|r| {
            r.borrow_mut()
                .push((ty.map(<[u8]>::to_vec), decl.to_vec(), f))
        });
        0
    }

    // the C++ std::string shim is not linked into unit tests
    #[unsafe(no_mangle)]
    pub extern "C" fn ohpl_as2_str_ctor(_mem: *mut std::ffi::c_void, _p: *const u8, _n: usize) {}

    #[unsafe(no_mangle)]
    pub extern "C" fn ohpl_as2_wstr_ctor(_mem: *mut std::ffi::c_void, _p: *const u32, _n: usize) {}

    #[allow(dead_code)]
    pub(crate) fn call(f: &NativeFn, fr: &Frame) {
        f(fr);
    }
}
