//! `cSerializeClass`: the reflection walk behind save games. C++ keeps the offset tables and the
//! object model (virtual calls, `std::string`, containers) and reaches them through [`Host`];
//! this module owns class lookup, type dispatch and the XML shape, which must stay byte-identical.
//!
//! UB in the C++ that is not reproduced: null class or object dereferences are skipped,
//! missing vector components read as 0 instead of uninitialised heap, array loads stop at the
//! declared array size, and temporaries for string containers are constructed objects.

use crate::string::{self, split, token_to_float, token_to_int};
use crate::xml::{DOC, Doc, NodeId, c_str};
use std::collections::BTreeMap;
use std::sync::{Arc, Mutex, PoisonError};

pub type Obj = *mut u8;
pub type CreateFn = unsafe extern "C" fn() -> Obj;

pub const T_BOOL: u32 = 1;
pub const T_INT32: u32 = 2;
pub const T_FLOAT32: u32 = 3;
pub const T_STRING: u32 = 4;
pub const T_VEC2L: u32 = 5;
pub const T_VEC2F: u32 = 6;
pub const T_VEC3L: u32 = 7;
pub const T_VEC3F: u32 = 8;
pub const T_MATRIXF: u32 = 9;
pub const T_COLOR: u32 = 10;
pub const T_RECT2L: u32 = 11;
pub const T_RECT2F: u32 = 12;
pub const T_PLANEF: u32 = 13;
pub const T_WSTRING: u32 = 14;
pub const T_CLASS: u32 = 100;
pub const T_CLASS_PTR: u32 = 101;
pub const T_NULL: u32 = 0xFFFF;

pub const M_VAR: u32 = 1;
pub const M_ARRAY: u32 = 2;
pub const M_CONTAINER: u32 = 3;

#[derive(Debug)]
pub struct Field {
    pub name: Vec<u8>,
    pub class_name: Vec<u8>,
    pub offset: usize,
    pub size: usize,
    pub ty: u32,
    pub main: u32,
    pub array_size: usize,
}

#[derive(Debug)]
pub struct Class {
    pub name: Vec<u8>,
    pub parent: Vec<u8>,
    /// Up to, not including, the `T_NULL` terminator.
    pub fields: Vec<Arc<Field>>,
    pub size: usize,
    pub create: Option<CreateFn>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Level {
    Log,
    Warning,
    Error,
}

/// The C++ object model. Pointers are objects the registered offset tables describe.
pub trait Host {
    fn log(&self, level: Level, msg: &[u8]);
    /// `Serialize_GetTopClass()`.
    fn top_class(&self, obj: Obj) -> Vec<u8>;
    /// `hplDelete` of an `iSerializable`.
    fn destroy(&self, obj: Obj);
    /// A scratch element for a container of `ty` (`SizeOfType` bytes), freed by `tmp_free`.
    fn tmp_new(&self, ty: u32, size: usize) -> Obj;
    fn tmp_free(&self, ty: u32, p: Obj);
    fn str_get(&self, p: Obj) -> Vec<u8>;
    fn str_set(&self, p: Obj, v: &[u8]);
    fn wstr_get(&self, p: Obj) -> Vec<u32>;
    fn wstr_set(&self, p: Obj, v: &[u32]);
    fn string_size(&self) -> usize;
    fn wstring_size(&self) -> usize;
    fn cont_size(&self, cont: Obj) -> usize;
    fn cont_clear(&self, cont: Obj);
    /// `AddVoidPtr(&ptr)`.
    fn cont_add_ptr(&self, cont: Obj, ptr: Obj);
    /// `AddVoidClass(elem)`: copies the element at `elem`.
    fn cont_add_class(&self, cont: Obj, elem: Obj);
    fn cont_iter(&self, cont: Obj) -> Obj;
    fn iter_next(&self, it: Obj) -> Option<Obj>;
    fn iter_free(&self, it: Obj);
}

type Registry = BTreeMap<Vec<u8>, Arc<Class>>;
static REGISTRY: Mutex<Registry> = Mutex::new(BTreeMap::new());

/// `SetUpData`: `map::insert`, so the first registration of a name wins.
pub fn register(class: Class) {
    REGISTRY
        .lock()
        .unwrap_or_else(PoisonError::into_inner)
        .entry(class.name.clone())
        .or_insert_with(|| Arc::new(class));
}

fn lookup(name: &[u8]) -> Option<Arc<Class>> {
    REGISTRY
        .lock()
        .unwrap_or_else(PoisonError::into_inner)
        .get(name)
        .cloned()
}

fn rd<T: Copy>(base: Obj, off: usize) -> T {
    // SAFETY: `Walker::new` contract: `base + off` lies inside a live object laid out as the
    // registered offset tables say; unaligned reads tolerate packed fields.
    unsafe { base.wrapping_add(off).cast::<T>().read_unaligned() }
}

fn wr<T>(base: Obj, off: usize, v: T) {
    // SAFETY: as `rd`.
    unsafe { base.wrapping_add(off).cast::<T>().write_unaligned(v) }
}

fn int_fields(ty: u32) -> usize {
    match ty {
        T_VEC2L => 2,
        T_VEC3L => 3,
        T_RECT2L => 4,
        _ => 0,
    }
}

fn float_fields(ty: u32) -> usize {
    match ty {
        T_VEC2F => 2,
        T_VEC3F => 3,
        T_MATRIXF => 16,
        T_COLOR | T_RECT2F | T_PLANEF => 4,
        _ => 0,
    }
}

unsafe extern "C" {
    fn snprintf(buf: *mut std::ffi::c_char, n: usize, fmt: *const std::ffi::c_char, ...) -> i32;
}

/// C `%f`.
fn fmt_f(x: f32) -> Vec<u8> {
    let mut buf = [0_u8; 64];
    // SAFETY: format matches the (double) vararg; the buffer bound is passed.
    let n = unsafe {
        snprintf(
            buf.as_mut_ptr().cast(),
            buf.len(),
            c"%f".as_ptr(),
            f64::from(x),
        )
    };
    let len = usize::try_from(n).unwrap_or(0).min(buf.len() - 1);
    buf.get(..len).unwrap_or_default().to_vec()
}

fn join(parts: impl Iterator<Item = Vec<u8>>) -> Vec<u8> {
    let mut out = Vec::new();
    for (i, p) in parts.enumerate() {
        if i > 0 {
            out.push(b' ');
        }
        out.extend(p);
    }
    out
}

/// `SizeOfType`.
fn size_of_type<H: Host>(h: &H, ty: u32) -> usize {
    match ty {
        T_BOOL => 1,
        T_INT32 | T_FLOAT32 => 4,
        T_STRING => h.string_size(),
        T_VEC2L | T_VEC2F => 8,
        T_VEC3L | T_VEC3F => 12,
        T_MATRIXF => 64,
        T_COLOR | T_RECT2L | T_RECT2F | T_PLANEF => 16,
        T_WSTRING => h.wstring_size(),
        _ => 0,
    }
}

/// Caller-checked capability to walk objects: see [`Walker::new`].
pub struct Walker<'a, H: Host> {
    h: &'a H,
}

struct FieldIter {
    class: Option<Arc<Class>>,
    idx: usize,
}

impl<'a, H: Host> Walker<'a, H> {
    /// # Safety
    /// Every object pointer later passed in, and every container, iterator and element pointer
    /// the host hands back, is live and laid out as the registered classes describe.
    pub unsafe fn new(h: &'a H) -> Self {
        Self { h }
    }

    fn warn(&self, msg: String) {
        self.h.log(Level::Warning, msg.as_bytes());
    }

    fn get_class(&self, name: &[u8]) -> Option<Arc<Class>> {
        let c = lookup(name);
        if c.is_none() {
            self.warn(format!(
                "Serialize class '{}' not found!\n",
                String::from_utf8_lossy(name)
            ));
        }
        c
    }

    fn switch_to_parent(&self, it: &mut FieldIter) {
        let Some(c) = &it.class else { return };
        if c.parent.is_empty() {
            return;
        }
        if let Some(p) = self.get_class(&c.parent) {
            it.class = Some(p);
            it.idx = 0;
        }
    }

    /// `cSerializeMemberFieldIterator`: a class without fields of its own starts at its parent,
    /// and a class running out of fields continues with its parent (one level at a time).
    fn fields(&self, class: Option<Arc<Class>>) -> FieldIter {
        let mut it = FieldIter { class, idx: 0 };
        if it.class.as_ref().is_some_and(|c| c.fields.is_empty()) {
            self.switch_to_parent(&mut it);
        }
        it
    }

    fn next_field(&self, it: &mut FieldIter) -> Option<Arc<Field>> {
        let f = it.class.as_ref()?.fields.get(it.idx)?.clone();
        it.idx += 1;
        if it
            .class
            .as_ref()
            .is_some_and(|c| c.fields.get(it.idx).is_none())
        {
            self.switch_to_parent(it);
        }
        Some(f)
    }

    fn member_field(&self, name: &[u8], class: &Arc<Class>) -> Option<Arc<Field>> {
        let mut it = self.fields(Some(class.clone()));
        while let Some(f) = self.next_field(&mut it) {
            if f.name == name {
                return Some(f);
            }
        }
        self.warn(format!(
            "Couldn't find member field '{}' in class '{}'\n",
            String::from_utf8_lossy(name),
            String::from_utf8_lossy(&class.name)
        ));
        None
    }

    /// `ValueToString`; unknown types give an empty string.
    fn value_to_string(&self, base: Obj, off: usize, ty: u32) -> Vec<u8> {
        let p = base.wrapping_add(off);
        match ty {
            T_BOOL => {
                let v: &[u8] = if rd::<u8>(p, 0) == 0 {
                    b"false"
                } else {
                    b"true"
                };
                v.to_vec()
            }
            T_INT32 => rd::<i32>(p, 0).to_string().into_bytes(),
            T_FLOAT32 => fmt_f(rd(p, 0)),
            T_STRING => c_str(&self.h.str_get(p)).to_vec(),
            T_VEC2L | T_VEC3L | T_RECT2L => {
                join((0..int_fields(ty)).map(|i| rd::<i32>(p, i * 4).to_string().into_bytes()))
            }
            T_VEC2F | T_VEC3F | T_MATRIXF | T_COLOR | T_RECT2F | T_PLANEF => {
                join((0..float_fields(ty)).map(|i| fmt_f(rd(p, i * 4))))
            }
            T_WSTRING => {
                let mut out = Vec::new();
                for w in self.h.wstr_get(p) {
                    out.extend((w as i32).to_string().into_bytes());
                    out.push(b' ');
                }
                out
            }
            _ => Vec::new(),
        }
    }

    /// `StringToValue`; `val` is `None` for a missing attribute.
    fn string_to_value(&self, base: Obj, off: usize, ty: u32, val: Option<&[u8]>) {
        let p = base.wrapping_add(off);
        let text = val.unwrap_or_default();
        match ty {
            T_BOOL => wr(p, 0, u8::from(val.is_some_and(string::to_bool))),
            T_INT32 => wr(p, 0, val.map_or(0, token_to_int)),
            T_FLOAT32 => wr(p, 0, val.map_or(0.0, token_to_float)),
            T_STRING => self.h.str_set(p, text),
            T_VEC2L | T_VEC3L | T_RECT2L => {
                let v: Vec<i32> = split(text, None, true).map(token_to_int).collect();
                for i in 0..int_fields(ty) {
                    wr(p, i * 4, v.get(i).copied().unwrap_or(0));
                }
            }
            T_VEC2F | T_VEC3F | T_MATRIXF | T_COLOR | T_RECT2F | T_PLANEF => {
                let v: Vec<f32> = split(text, None, true).map(token_to_float).collect();
                for i in 0..float_fields(ty) {
                    wr(p, i * 4, v.get(i).copied().unwrap_or(0.0));
                }
            }
            T_WSTRING => {
                let v: Vec<u32> = split(text, None, true)
                    .map(|t| token_to_int(t) as u32)
                    .collect();
                self.h.wstr_set(p, &v);
            }
            _ => {}
        }
    }

    // ---- save ----

    /// `SaveToFile` up to the XML document: a `root` element holding the object.
    pub fn save_document(&self, doc: &mut Doc, obj: Obj, root: &[u8]) {
        let root = doc.add_element(DOC, root);
        self.save_to_element(doc, root, obj, b"", false);
    }

    fn save_to_element(&self, doc: &mut Doc, parent: NodeId, obj: Obj, name: &[u8], ptr: bool) {
        if obj.is_null() {
            return;
        }
        let el = doc.add_element(parent, if ptr { b"class_ptr" } else { b"class" });
        let top = self.h.top_class(obj);
        doc.set_attribute(el, b"type", &top);
        doc.set_attribute(el, b"name", name);
        let mut it = self.fields(self.get_class(&top));
        while let Some(f) = self.next_field(&mut it) {
            match f.main {
                M_VAR => self.save_variable(doc, el, &f, obj),
                M_ARRAY => self.save_array(doc, el, &f, obj),
                M_CONTAINER => self.save_container(doc, el, &f, obj),
                _ => {}
            }
        }
    }

    fn save_variable(&self, doc: &mut Doc, parent: NodeId, f: &Field, obj: Obj) {
        match f.ty {
            T_CLASS => {
                self.save_to_element(doc, parent, obj.wrapping_add(f.offset), &f.name, false)
            }
            T_CLASS_PTR => {
                let p: Obj = rd(obj, f.offset);
                self.save_to_element(doc, parent, p, &f.name, true);
            }
            _ => {
                let el = doc.add_element(parent, b"var");
                doc.set_attribute(el, b"type", f.ty.to_string().as_bytes());
                doc.set_attribute(el, b"name", &f.name);
                let v = self.value_to_string(obj, f.offset, f.ty);
                doc.set_attribute(el, b"val", &v);
            }
        }
    }

    fn save_array(&self, doc: &mut Doc, parent: NodeId, f: &Field, obj: Obj) {
        let data = obj.wrapping_add(f.offset);
        let el = doc.add_element(parent, b"array");
        doc.set_attribute(el, b"type", f.ty.to_string().as_bytes());
        doc.set_attribute(el, b"name", &f.name);
        doc.set_attribute(el, b"size", (f.array_size as i32).to_string().as_bytes());
        match f.ty {
            T_CLASS => {
                let top = self.h.top_class(data);
                let Some(class) = self.get_class(&top) else {
                    return;
                };
                for i in 0..f.array_size {
                    let at = data.wrapping_add(class.size.wrapping_mul(i));
                    self.save_to_element(doc, el, at, b"", false);
                }
            }
            T_CLASS_PTR => {
                let first: Obj = rd(data, 0);
                if first.is_null() {
                    self.warn(format!(
                        "Array {} is NULL!\n",
                        String::from_utf8_lossy(&f.name)
                    ));
                    return;
                }
                let top = self.h.top_class(first);
                doc.set_attribute(el, b"class_type", &top);
                for i in 0..f.array_size {
                    let p: Obj = rd(data, i.wrapping_mul(size_of::<usize>()));
                    self.save_to_element(doc, el, p, b"", true);
                }
            }
            _ => {
                let step = size_of_type(self.h, f.ty);
                for i in 0..f.array_size {
                    let var = doc.add_element(el, b"var");
                    let v = self.value_to_string(data, step.wrapping_mul(i), f.ty);
                    doc.set_attribute(var, b"val", &v);
                }
            }
        }
    }

    fn save_container(&self, doc: &mut Doc, parent: NodeId, f: &Field, obj: Obj) {
        let cont = obj.wrapping_add(f.offset);
        let it = self.h.cont_iter(cont);
        let el = doc.add_element(parent, b"container");
        doc.set_attribute(el, b"type", f.ty.to_string().as_bytes());
        doc.set_attribute(el, b"name", &f.name);
        if f.ty == T_CLASS {
            doc.set_attribute(el, b"class_type", &f.class_name);
        }
        while let Some(p) = self.h.iter_next(it) {
            match f.ty {
                T_CLASS => self.save_to_element(doc, el, p, b"", false),
                T_CLASS_PTR => {
                    let q: Obj = rd(p, 0);
                    self.save_to_element(doc, el, q, b"", true);
                }
                _ => {
                    let var = doc.add_element(el, b"var");
                    let v = self.value_to_string(p, 0, f.ty);
                    doc.set_attribute(var, b"val", &v);
                }
            }
        }
        self.h.iter_free(it);
    }

    // ---- load ----

    /// `LoadFromFile` after the document is parsed: the first `class` under the root.
    pub fn load_document(&self, doc: &Doc, obj: Obj) {
        let class = doc
            .first_child_element(DOC, None)
            .and_then(|root| doc.first_child_element(root, Some(b"class")));
        if let Some(el) = class {
            self.load_from_element(doc, el, obj);
        }
    }

    fn load_from_element(&self, doc: &Doc, el: NodeId, obj: Obj) {
        if obj.is_null() {
            return;
        }
        let Some(class) = self.get_class(&self.h.top_class(obj)) else {
            return;
        };
        let mut child = doc.first_child_element(el, None);
        while let Some(m) = child {
            match c_str(&doc.node(m).value) {
                b"var" => self.load_variable(doc, m, obj, &class),
                b"array" => self.load_array(doc, m, obj, &class),
                b"class" => self.load_class(doc, m, obj, &class),
                b"class_ptr" => self.load_class_pointer(doc, m, obj, &class),
                b"container" => self.load_container(doc, m, obj, &class),
                _ => {}
            }
            child = doc.next_sibling_element(m, None);
        }
    }

    fn attr_type(doc: &Doc, el: NodeId) -> u32 {
        doc.attribute(el, b"type")
            .map_or(T_NULL, |v| token_to_int(v) as u32)
    }

    fn named_field(&self, doc: &Doc, el: NodeId, class: &Arc<Class>) -> Option<Arc<Field>> {
        let name = doc.attribute(el, b"name").unwrap_or_default();
        self.member_field(name, class)
    }

    fn load_variable(&self, doc: &Doc, el: NodeId, obj: Obj, class: &Arc<Class>) {
        let val = doc.attribute(el, b"val");
        let ty = Self::attr_type(doc, el);
        if let Some(f) = self.named_field(doc, el, class) {
            self.string_to_value(obj, f.offset, ty, val);
        }
    }

    fn load_array(&self, doc: &Doc, el: NodeId, obj: Obj, class: &Arc<Class>) {
        let ty = Self::attr_type(doc, el);
        let Some(f) = self.named_field(doc, el, class) else {
            return;
        };
        let data = obj.wrapping_add(f.offset);
        let kids = std::iter::successors(doc.first_child_element(el, None), |&k| {
            doc.next_sibling_element(k, None)
        })
        .take(f.array_size);
        match f.ty {
            T_CLASS => {
                let Some(c) = self.get_class(&self.h.top_class(data)) else {
                    return;
                };
                for (i, k) in kids.enumerate() {
                    self.load_from_element(doc, k, data.wrapping_add(c.size.wrapping_mul(i)));
                }
            }
            T_CLASS_PTR => {
                for (i, k) in kids.enumerate() {
                    let slot = data.wrapping_add(i.wrapping_mul(size_of::<usize>()));
                    let name = doc.attribute(k, b"type").unwrap_or_default();
                    let Some(c) = self.get_class(name) else {
                        continue;
                    };
                    let Some(create) = c.create else { continue };
                    let old: Obj = rd(slot, 0);
                    if !old.is_null() {
                        self.h.destroy(old);
                    }
                    // SAFETY: registered creation function of a live class.
                    let fresh = unsafe { create() };
                    wr(slot, 0, fresh);
                    self.load_from_element(doc, k, fresh);
                }
            }
            _ => {
                let step = size_of_type(self.h, ty);
                for (i, k) in kids.enumerate() {
                    let val = doc.attribute(k, b"val");
                    self.string_to_value(data, i.wrapping_mul(step), ty, val);
                }
            }
        }
    }

    fn load_class(&self, doc: &Doc, el: NodeId, obj: Obj, class: &Arc<Class>) {
        if let Some(f) = self.named_field(doc, el, class) {
            self.load_from_element(doc, el, obj.wrapping_add(f.offset));
        }
    }

    fn load_class_pointer(&self, doc: &Doc, el: NodeId, obj: Obj, class: &Arc<Class>) {
        let ty_name = doc.attribute(el, b"type").unwrap_or_default();
        let null = doc.attribute(el, b"null").is_some_and(string::to_bool);
        let Some(f) = self.named_field(doc, el, class) else {
            return;
        };
        if null {
            wr(obj, f.offset, std::ptr::null_mut::<u8>());
            return;
        }
        let mut cur: Obj = rd(obj, f.offset);
        if cur.is_null() {
            let Some(create) = self.get_class(ty_name).and_then(|c| c.create) else {
                return;
            };
            // SAFETY: registered creation function of a live class.
            cur = unsafe { create() };
            wr(obj, f.offset, cur);
        }
        self.load_from_element(doc, el, cur);
    }

    fn load_container(&self, doc: &Doc, el: NodeId, obj: Obj, class: &Arc<Class>) {
        let ty = Self::attr_type(doc, el);
        let Some(f) = self.named_field(doc, el, class) else {
            return;
        };
        let cont = obj.wrapping_add(f.offset);
        let kids = || {
            std::iter::successors(doc.first_child_element(el, None), |&k| {
                doc.next_sibling_element(k, None)
            })
        };
        match f.ty {
            T_CLASS => {
                let name = doc.attribute(el, b"class_type").unwrap_or_default();
                self.h.cont_clear(cont);
                let Some(c) = self.get_class(name) else {
                    return;
                };
                let Some(create) = c.create else { return };
                for k in kids() {
                    // SAFETY: registered creation function of a live class.
                    let fresh = unsafe { create() };
                    self.load_from_element(doc, k, fresh);
                    self.h.cont_add_class(cont, fresh);
                    self.h.destroy(fresh);
                }
            }
            T_CLASS_PTR => {
                let it = self.h.cont_iter(cont);
                while let Some(p) = self.h.iter_next(it) {
                    self.h.destroy(rd::<Obj>(p, 0));
                }
                self.h.iter_free(it);
                if self.h.cont_size(cont) > 0 {
                    self.h.cont_clear(cont);
                }
                for k in kids() {
                    let name = doc.attribute(k, b"type").unwrap_or_default();
                    let Some(c) = self.get_class(name) else {
                        continue;
                    };
                    let Some(create) = c.create else { continue };
                    // SAFETY: registered creation function of a live class.
                    let fresh = unsafe { create() };
                    self.load_from_element(doc, k, fresh);
                    self.h.cont_add_ptr(cont, fresh);
                }
            }
            _ => {
                self.h.cont_clear(cont);
                for k in kids() {
                    let val = doc.attribute(k, b"val");
                    let tmp = self.h.tmp_new(ty, size_of_type(self.h, ty));
                    self.string_to_value(tmp, 0, ty, val);
                    self.h.cont_add_class(cont, tmp);
                    self.h.tmp_free(ty, tmp);
                }
            }
        }
    }
}
