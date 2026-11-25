//! The `array<T>` script type: elements are stored inline for primitives and enums and as pointers for
//! handles and objects, exactly as the AngelScript add-on laid them out, so addresses handed to scripts
//! behave the same. All state sits in cells because script callbacks (comparisons, destructors) can
//! reach the array while one of its methods is running.

use super::host::*;
use crate::graphics::stdsort;
use std::alloc::{Layout, alloc, dealloc};
use std::cell::Cell;
use std::cmp::Ordering as Ord_;
use std::ptr;
use std::sync::atomic::{AtomicI32, Ordering};

pub const CACHE_KEY: usize = 1000;
const MAX_ELEMS: u32 = 0xFFFF_FFF4;

/// `opCmp` and `opEquals` of an element type, found once per template instance.
#[derive(Default)]
pub struct Cache {
    cmp: P,
    eq: P,
    cmp_rc: i32,
    eq_rc: i32,
}

/// Frees the cache stored in a type's user data.
///
/// # Safety
/// `p` is null or a pointer stored by `Array::precache`.
pub unsafe fn free_cache(p: P) {
    if p != 0 {
        // SAFETY: caller contract; created with `Box::into_raw`.
        drop(unsafe { Box::from_raw(p as *mut Cache) });
    }
}

fn prim_size(id: i32) -> usize {
    match id {
        TYPEID_BOOL | TYPEID_INT8 | TYPEID_UINT8 => 1,
        TYPEID_INT16 | TYPEID_UINT16 => 2,
        TYPEID_INT64 | TYPEID_UINT64 | TYPEID_DOUBLE => 8,
        _ => 4,
    }
}

fn is_prim(id: i32) -> bool {
    id & !TYPEID_MASK_SEQNBR == 0
}

enum Num {
    I(i64),
    U(u64),
    F(f32),
    D(f64),
}

/// # Safety
/// `p` points at a value of the primitive type `id`.
unsafe fn read_num(id: i32, p: *const u8) -> Num {
    // SAFETY: caller contract; unaligned reads, the engine's own references can sit anywhere.
    unsafe {
        match id {
            TYPEID_BOOL => Num::U(u64::from(p.read_unaligned() != 0)),
            TYPEID_INT8 => Num::I(i64::from(p.cast::<i8>().read_unaligned())),
            TYPEID_INT16 => Num::I(i64::from(p.cast::<i16>().read_unaligned())),
            TYPEID_INT64 => Num::I(p.cast::<i64>().read_unaligned()),
            TYPEID_UINT8 => Num::U(u64::from(p.read_unaligned())),
            TYPEID_UINT16 => Num::U(u64::from(p.cast::<u16>().read_unaligned())),
            TYPEID_UINT32 => Num::U(u64::from(p.cast::<u32>().read_unaligned())),
            TYPEID_UINT64 => Num::U(p.cast::<u64>().read_unaligned()),
            TYPEID_FLOAT => Num::F(p.cast::<f32>().read_unaligned()),
            TYPEID_DOUBLE => Num::D(p.cast::<f64>().read_unaligned()),
            _ => Num::I(i64::from(p.cast::<i32>().read_unaligned())),
        }
    }
}

fn num_cmp(a: &Num, b: &Num) -> Option<Ord_> {
    match (a, b) {
        (Num::I(x), Num::I(y)) => Some(x.cmp(y)),
        (Num::U(x), Num::U(y)) => Some(x.cmp(y)),
        (Num::F(x), Num::F(y)) => x.partial_cmp(y),
        (Num::D(x), Num::D(y)) => x.partial_cmp(y),
        _ => None,
    }
}

pub struct Array {
    rc: AtomicI32,
    gc: Cell<bool>,
    host: &'static dyn Host,
    ty: P,
    sub_id: Cell<i32>,
    es: Cell<usize>,
    buf: Cell<*mut u8>,
    num: Cell<u32>,
    max: Cell<u32>,
}

impl Array {
    /// A new array object with one reference; the caller fills it with one of the `init_*` methods.
    pub fn new(host: &'static dyn Host, ty: P) -> *mut Array {
        host.type_addref(ty);
        let a = Array {
            rc: AtomicI32::new(1),
            gc: Cell::new(false),
            host,
            ty,
            sub_id: Cell::new(0),
            es: Cell::new(0),
            buf: Cell::new(ptr::null_mut()),
            num: Cell::new(0),
            max: Cell::new(0),
        };
        a.precache();
        a.es.set(if a.sub_id.get() & TYPEID_MASK_OBJECT != 0 {
            size_of::<usize>()
        } else {
            prim_size(a.sub_id.get())
        });
        Box::into_raw(Box::new(a))
    }

    fn engine(&self) -> P {
        self.host.type_engine(self.ty)
    }

    fn notify_gc(&self) {
        if self.host.type_flags(self.ty) & OBJ_GC != 0 {
            self.host
                .notify_gc(self.engine(), ptr::from_ref(self) as P, self.ty);
        }
    }

    pub fn init_len(&self, length: u32) {
        if !self.check_max(length) {
            return;
        }
        self.install(length);
        self.notify_gc();
    }

    /// `buf` is the engine's list buffer: a count followed by the elements.
    ///
    /// # Safety
    /// `buf` is a list buffer of this array's subtype, writable (handles are taken from it).
    pub unsafe fn init_list(&self, buf: *mut u8) {
        let engine = self.engine();
        // SAFETY: caller contract; the count is the first dword.
        let length = unsafe { buf.cast::<u32>().read_unaligned() };
        if !self.check_max(length) {
            return;
        }
        let es = self.es.get();
        let n = length as usize;
        let elems = buf.wrapping_add(4);
        let sub = self.host.sub_type(self.ty);
        let id = self.sub_id.get();
        if id & TYPEID_MASK_OBJECT == 0 {
            self.install(length);
            if length > 0 {
                // SAFETY: the buffer holds `length` primitives, ours has room for as many.
                unsafe { ptr::copy_nonoverlapping(elems, self.buf.get(), n * es) };
            }
        } else if id & TYPEID_OBJHANDLE != 0 || self.host.type_flags(sub) & OBJ_REF != 0 {
            // handles are taken over: ref types are stored as handles too
            self.sub_id.set(id | TYPEID_OBJHANDLE);
            self.install(length);
            self.sub_id.set(id);
            if length > 0 {
                // SAFETY: as above; the source pointers are cleared so the engine does not release them.
                unsafe {
                    ptr::copy_nonoverlapping(elems, self.buf.get(), n * es);
                    ptr::write_bytes(elems, 0, n * es);
                }
            }
        } else {
            self.install(length);
            let obj_size = self.host.type_size(sub);
            for k in 0..length {
                let obj = self.at(k);
                let src = elems.wrapping_add(k as usize * obj_size);
                self.host.assign_object(engine, obj as P, src as P, sub);
            }
        }
        self.notify_gc();
    }

    pub fn init_default(&self, length: u32, def: P) {
        if !self.check_max(length) {
            return;
        }
        self.install(length);
        self.notify_gc();
        for n in 0..self.num.get() {
            self.set_value(n, def);
        }
    }

    pub fn elem_type_id(&self) -> i32 {
        self.sub_id.get()
    }

    pub fn size(&self) -> u32 {
        self.num.get()
    }

    pub fn is_empty(&self) -> bool {
        self.num.get() == 0
    }

    fn is_obj_value(&self) -> bool {
        let id = self.sub_id.get();
        id & TYPEID_MASK_OBJECT != 0 && id & TYPEID_OBJHANDLE == 0
    }

    fn slot(&self, b: *mut u8, i: u32) -> *mut u8 {
        b.wrapping_add(i as usize * self.es.get())
    }

    fn rd(&self, b: *mut u8, i: u32) -> usize {
        // SAFETY: callers only pass slots inside a live buffer of pointer-sized elements, 8-aligned.
        unsafe { self.slot(b, i).cast::<usize>().read() }
    }

    fn wr(&self, b: *mut u8, i: u32, v: usize) {
        // SAFETY: as `rd`.
        unsafe { self.slot(b, i).cast::<usize>().write(v) }
    }

    fn layout(&self, n: u32) -> Option<Layout> {
        Layout::from_size_align((self.es.get() * n as usize).max(8), 8).ok()
    }

    fn alloc_buf(&self, n: u32) -> *mut u8 {
        match self.layout(n) {
            // SAFETY: the layout has a non-zero size.
            Some(l) => unsafe { alloc(l) },
            None => ptr::null_mut(),
        }
    }

    fn free_buf(&self, b: *mut u8, max: u32) {
        if let (false, Some(l)) = (b.is_null(), self.layout(max)) {
            // SAFETY: allocated by `alloc_buf` with the same element count.
            unsafe { dealloc(b, l) };
        }
    }

    /// `CreateBuffer`: a buffer of `n` constructed elements.
    fn install(&self, n: u32) {
        let b = self.alloc_buf(n);
        if b.is_null() {
            self.host.throw("Out of memory");
            return;
        }
        self.buf.set(b);
        self.num.set(n);
        self.max.set(n);
        self.construct(b, 0, n);
    }

    fn construct(&self, b: *mut u8, start: u32, end: u32) {
        if self.is_obj_value() {
            let engine = self.engine();
            let sub = self.host.sub_type(self.ty);
            for d in start..end {
                let o = self.host.create_object(engine, sub);
                self.wr(b, d, o);
                if o == 0 {
                    for k in d..end {
                        self.wr(b, k, 0);
                    }
                    return;
                }
            }
        } else if end > start {
            // SAFETY: the range lies inside the buffer.
            unsafe {
                ptr::write_bytes(
                    self.slot(b, start),
                    0,
                    (end - start) as usize * self.es.get(),
                )
            };
        }
    }

    fn destruct(&self, b: *mut u8, start: u32, end: u32) {
        if self.sub_id.get() & TYPEID_MASK_OBJECT == 0 {
            return;
        }
        let engine = self.engine();
        let sub = self.host.sub_type(self.ty);
        for d in start..end {
            let o = self.rd(b, d);
            if o != 0 {
                self.host.release_object(engine, o, sub);
            }
        }
    }

    fn check_max(&self, n: u32) -> bool {
        let max = MAX_ELEMS
            .checked_div(u32::try_from(self.es.get()).unwrap_or(0))
            .unwrap_or(MAX_ELEMS);
        if n > max {
            self.host.throw("Too large array size");
            return false;
        }
        true
    }

    pub fn at(&self, i: u32) -> *mut u8 {
        let b = self.buf.get();
        if b.is_null() || i >= self.num.get() {
            self.host.throw("Index out of bounds");
            return ptr::null_mut();
        }
        if self.is_obj_value() {
            self.rd(b, i) as *mut u8
        } else {
            self.slot(b, i)
        }
    }

    pub fn reserve(&self, n: u32) {
        let b = self.buf.get();
        if b.is_null() || n <= self.max.get() || !self.check_max(n) {
            return;
        }
        let nb = self.alloc_buf(n);
        if nb.is_null() {
            self.host.throw("Out of memory");
            return;
        }
        // SAFETY: both buffers hold at least `num` elements.
        unsafe { ptr::copy_nonoverlapping(b, nb, self.num.get() as usize * self.es.get()) };
        self.free_buf(b, self.max.get());
        self.buf.set(nb);
        self.max.set(n);
    }

    pub fn resize(&self, n: u32) {
        if !self.check_max(n) || self.buf.get().is_null() {
            return;
        }
        self.resize_at((n as i32).wrapping_sub(self.num.get() as i32), u32::MAX);
    }

    pub fn remove_range(&self, start: u32, mut count: u32) {
        if count == 0 {
            return;
        }
        let b = self.buf.get();
        let num = self.num.get();
        if b.is_null() || start > num {
            self.host.throw("Index out of bounds");
            return;
        }
        if start.wrapping_add(count) > num {
            count = num - start;
        }
        self.destruct(b, start, start + count);
        let es = self.es.get();
        // SAFETY: both ranges lie inside the buffer.
        unsafe {
            ptr::copy(
                self.slot(b, start + count),
                self.slot(b, start),
                (num - start - count) as usize * es,
            )
        };
        self.num.set(num - count);
    }

    /// The internal `Resize(delta, at)`: opens or closes a gap of `delta` elements at `at`.
    fn resize_at(&self, mut delta: i32, mut at: u32) {
        let b = self.buf.get();
        let num = self.num.get();
        if b.is_null() {
            return;
        }
        if delta < 0 {
            if delta.wrapping_neg() > num as i32 {
                delta = -(num as i32);
            }
            let top = (num as i32).wrapping_add(delta) as u32;
            if at > top {
                at = top;
            }
        } else if delta > 0 {
            if !self.check_max(num.wrapping_add(delta as u32)) {
                return;
            }
            if at > num {
                at = num;
            }
        }
        if delta == 0 {
            return;
        }
        let es = self.es.get();
        let new_num = num.wrapping_add(delta as u32);
        if self.max.get() < new_num {
            let nb = self.alloc_buf(new_num);
            if nb.is_null() {
                self.host.throw("Out of memory");
                return;
            }
            // SAFETY: the new buffer holds `new_num` elements; both source ranges lie inside the old one.
            unsafe {
                ptr::copy_nonoverlapping(b, nb, at as usize * es);
                if at < num {
                    ptr::copy_nonoverlapping(
                        self.slot(b, at),
                        self.slot(nb, at + delta as u32),
                        (num - at) as usize * es,
                    );
                }
            }
            self.construct(nb, at, at + delta as u32);
            self.free_buf(b, self.max.get());
            self.buf.set(nb);
            self.num.set(new_num);
            self.max.set(new_num);
        } else if delta < 0 {
            let gap = delta.unsigned_abs();
            self.destruct(b, at, at + gap);
            // SAFETY: both ranges lie inside the buffer.
            unsafe {
                ptr::copy(
                    self.slot(b, at + gap),
                    self.slot(b, at),
                    (num - (at + gap)) as usize * es,
                )
            };
            self.num.set(new_num);
        } else {
            // SAFETY: the tail moves within the buffer, capacity was checked above.
            unsafe {
                ptr::copy(
                    self.slot(b, at),
                    self.slot(b, at + delta as u32),
                    (num - at) as usize * es,
                )
            };
            self.construct(b, at, at + delta as u32);
            self.num.set(new_num);
        }
    }

    pub fn insert_at(&self, index: u32, value: P) {
        if index > self.num.get() {
            self.host.throw("Index out of bounds");
            return;
        }
        self.resize_at(1, index);
        self.set_value(index, value);
    }

    pub fn insert_at_array(&self, index: u32, other: &Array) {
        if index > self.num.get() {
            self.host.throw("Index out of bounds");
            return;
        }
        if self.ty != other.ty {
            self.host.throw("Mismatching array types");
            return;
        }
        let elements = other.size();
        self.resize_at(elements as i32, index);
        if !ptr::eq(other, self) {
            for n in 0..other.size() {
                self.set_value(index + n, other.at(n) as P);
            }
        } else {
            for n in 0..index {
                self.set_value(index + n, other.at(n) as P);
            }
            for (m, n) in ((index + elements)..other.size()).enumerate() {
                self.set_value(index + index + m as u32, other.at(n) as P);
            }
        }
    }

    pub fn insert_last(&self, value: P) {
        self.insert_at(self.num.get(), value);
    }

    pub fn remove_at(&self, index: u32) {
        if index >= self.num.get() {
            self.host.throw("Index out of bounds");
            return;
        }
        self.resize_at(-1, index);
    }

    pub fn remove_last(&self) {
        self.remove_at(self.num.get().wrapping_sub(1));
    }

    pub fn set_value(&self, index: u32, value: P) {
        let p = self.at(index);
        if p.is_null() {
            return;
        }
        let id = self.sub_id.get();
        let engine = self.engine();
        if id & !TYPEID_MASK_SEQNBR != 0 && id & TYPEID_OBJHANDLE == 0 {
            let sub = self.host.sub_type(self.ty);
            if self.host.type_flags(sub) & OBJ_ASHANDLE != 0
                && self.host.hndl_assign(engine, sub, p as P, value)
            {
                return;
            }
            self.host.assign_object(engine, p as P, value, sub);
        } else if id & TYPEID_OBJHANDLE != 0 {
            let sub = self.host.sub_type(self.ty);
            let src = {
                // SAFETY: `value` points at a handle (one pointer) per the `const T&in` contract.
                unsafe { (value as *const usize).read() }
            };
            // SAFETY: `p` is an element slot, pointer-sized.
            let old = unsafe { p.cast::<usize>().replace(src) };
            self.host.addref_object(engine, src, sub);
            if old != 0 {
                self.host.release_object(engine, old, sub);
            }
        } else if value != 0 {
            // SAFETY: `value` points at a primitive of the element size.
            unsafe { ptr::copy(value as *const u8, p, self.es.get()) };
        }
    }

    pub fn copy_from(&self, other: &Array) {
        if ptr::eq(other, self) || other.ty != self.ty {
            return;
        }
        self.resize(other.num.get());
        self.copy_buffer(other);
    }

    fn copy_buffer(&self, src: &Array) {
        let (d, s) = (self.buf.get(), src.buf.get());
        let count = self.num.get().min(src.num.get());
        if d.is_null() || s.is_null() || count == 0 {
            return;
        }
        let engine = self.engine();
        let sub = self.host.sub_type(self.ty);
        let id = self.sub_id.get();
        if id & TYPEID_OBJHANDLE != 0 {
            for i in 0..count {
                let tmp = self.rd(d, i);
                let v = src.rd(s, i);
                self.wr(d, i, v);
                if v != 0 {
                    self.host.addref_object(engine, v, sub);
                }
                if tmp != 0 {
                    self.host.release_object(engine, tmp, sub);
                }
            }
        } else if id & TYPEID_MASK_OBJECT != 0 {
            let as_handle = self.host.type_flags(sub) & OBJ_ASHANDLE != 0;
            for i in 0..count {
                let (dst, from) = (self.rd(d, i), src.rd(s, i));
                if as_handle && self.host.hndl_assign(engine, sub, dst, from) {
                    continue;
                }
                self.host.assign_object(engine, dst, from, sub);
            }
        } else {
            // SAFETY: both buffers hold at least `count` elements.
            unsafe { ptr::copy_nonoverlapping(s, d, count as usize * self.es.get()) };
        }
    }

    fn cache(&self) -> Option<&Cache> {
        let c = self.host.type_user(self.ty);
        // SAFETY: the user data is a `Cache` stored by `precache` and freed with the type.
        unsafe { (c as *const Cache).as_ref() }
    }

    fn precache(&self) {
        let sub_id = self.host.sub_type_id(self.ty);
        self.sub_id.set(sub_id);
        if is_prim(sub_id) || self.host.type_user(self.ty) != 0 {
            return;
        }
        self.host.lock(true);
        if self.host.type_user(self.ty) != 0 {
            self.host.lock(false);
            return;
        }
        let mut cache = Cache::default();
        let must_const = sub_id & TYPEID_HANDLETOCONST != 0;
        let sub = self.host.type_by_id(self.engine(), sub_id);
        for f in self.host.methods(sub) {
            if f.params != 1 || (must_const && !f.read_only) || f.ret_flags != 0 {
                continue;
            }
            let is_cmp = f.ret_id == TYPEID_INT32 && f.name == b"opCmp";
            let is_eq = f.ret_id == TYPEID_BOOL && f.name == b"opEquals";
            if !is_cmp && !is_eq {
                continue;
            }
            let keep = !(TYPEID_OBJHANDLE | TYPEID_HANDLETOCONST);
            if f.p0_id & keep != sub_id & keep {
                continue;
            }
            if f.p0_flags & TM_INREF != 0 {
                if f.p0_id & TYPEID_OBJHANDLE != 0 || (must_const && f.p0_flags & TM_CONST == 0) {
                    continue;
                }
            } else if f.p0_id & TYPEID_OBJHANDLE != 0 {
                if must_const && f.p0_id & TYPEID_HANDLETOCONST == 0 {
                    continue;
                }
            } else {
                continue;
            }
            let (slot, rc) = if is_cmp {
                (&mut cache.cmp, &mut cache.cmp_rc)
            } else {
                (&mut cache.eq, &mut cache.eq_rc)
            };
            if *slot != 0 || *rc != 0 {
                *slot = 0;
                *rc = MULTIPLE_FUNCTIONS;
            } else {
                *slot = f.f;
            }
        }
        if cache.eq == 0 && cache.eq_rc == 0 {
            cache.eq_rc = NO_FUNCTION;
        }
        if cache.cmp == 0 && cache.cmp_rc == 0 {
            cache.cmp_rc = NO_FUNCTION;
        }
        self.host
            .set_type_user(self.ty, Box::into_raw(Box::new(cache)) as P);
        self.host.lock(false);
    }

    fn sub_name(&self) -> String {
        let t = self.host.type_by_id(self.engine(), self.sub_id.get());
        self.host.type_name(t)
    }

    fn equals(&self, a: *const u8, b: *const u8, c: Option<CmpCtx>, cache: Option<&Cache>) -> bool {
        let id = self.sub_id.get();
        if is_prim(id) {
            // SAFETY: both point at primitives of the element type.
            let (x, y) = unsafe { (read_num(id, a), read_num(id, b)) };
            return num_cmp(&x, &y) == Some(Ord_::Equal);
        }
        let handle = id & TYPEID_OBJHANDLE != 0;
        let (obj, arg) = if handle {
            // SAFETY: both point at handles.
            let (x, y) = unsafe { (a.cast::<usize>().read(), b.cast::<usize>().read()) };
            if x == y {
                return true;
            }
            (x, y)
        } else {
            (a as usize, b as usize)
        };
        let (Some(c), Some(cache)) = (c, cache) else {
            return false;
        };
        if cache.eq != 0 {
            return self.host.call_eq(c, cache.eq, obj, arg);
        }
        if cache.cmp != 0 {
            return self.host.call_cmp(c, cache.cmp, obj, arg) == Some(0);
        }
        false
    }

    fn with_cmp_ctx<R>(&self, pooled: bool, f: impl FnOnce(Option<CmpCtx>) -> R) -> R {
        let engine = self.engine();
        let c = if is_prim(self.sub_id.get()) {
            None
        } else {
            Some(self.host.begin_ctx(engine, pooled))
        };
        let r = f(c);
        if let Some(c) = c {
            self.host.end_ctx(engine, c, pooled);
        }
        r
    }

    pub fn equals_array(&self, other: &Array) -> bool {
        if self.ty != other.ty || self.size() != other.size() {
            return false;
        }
        self.with_cmp_ctx(false, |c| {
            let cache = self.cache();
            (0..self.size()).all(|n| self.equals(self.at(n), other.at(n), c, cache))
        })
    }

    fn no_match(&self, multiple: bool, what: &str) {
        let name = self.sub_name();
        self.host.throw(&if multiple {
            format!("Type '{name}' has multiple matching {what} methods")
        } else {
            format!("Type '{name}' does not have a matching {what} method")
        });
    }

    pub fn find(&self, start_at: u32, value: P) -> i32 {
        let id = self.sub_id.get();
        let cache = if is_prim(id) { None } else { self.cache() };
        if !is_prim(id) && cache.is_none_or(|c| c.cmp == 0 && c.eq == 0) {
            self.no_match(
                cache.is_some_and(|c| c.eq_rc == MULTIPLE_FUNCTIONS),
                "opEquals or opCmp",
            );
            return -1;
        }
        self.with_cmp_ctx(false, |c| {
            for i in start_at..self.size() {
                if self.equals(self.at(i), value as *const u8, c, cache) {
                    return i as i32;
                }
            }
            -1
        })
    }

    pub fn find_by_ref(&self, start_at: u32, r: P) -> i32 {
        let handle = self.sub_id.get() & TYPEID_OBJHANDLE != 0;
        let target = if handle {
            // SAFETY: `r` points at a handle.
            unsafe { (r as *const usize).read() }
        } else {
            r
        };
        for i in start_at..self.size() {
            let p = self.at(i);
            let here = if handle {
                // SAFETY: element slots of a handle array hold pointers.
                unsafe { p.cast::<usize>().read() }
            } else {
                p as usize
            };
            if here == target {
                return i as i32;
            }
        }
        -1
    }

    pub fn reverse(&self) {
        let size = self.size();
        if size < 2 {
            return;
        }
        let b = self.buf.get();
        let es = self.es.get();
        for i in 0..size >> 1 {
            let (x, y) = (self.slot(b, i), self.slot(b, size - i - 1));
            // SAFETY: both slots lie inside the buffer and are distinct.
            unsafe { ptr::swap_nonoverlapping(x, y, es) };
        }
    }

    pub fn sort(&self, start_at: u32, count: u32, asc: bool) {
        let id = self.sub_id.get();
        let cache = if is_prim(id) { None } else { self.cache() };
        if !is_prim(id) && cache.is_none_or(|c| c.cmp == 0) {
            self.no_match(
                cache.is_some_and(|c| c.cmp_rc == MULTIPLE_FUNCTIONS),
                "opCmp",
            );
            return;
        }
        if count < 2 {
            return;
        }
        let (start, end) = (start_at as i32, start_at.wrapping_add(count) as i32);
        let num = self.num.get() as i32;
        if start < 0 || start >= num || end > num {
            self.host.throw("Index out of bounds");
            return;
        }
        let (start, end) = (start as u32, end as u32);
        if is_prim(id) {
            self.insertion_sort(start, end, asc);
            return;
        }
        let cmp = cache.map_or(0, |c| c.cmp);
        self.with_cmp_ctx(true, |c| {
            let b = self.buf.get();
            let mut v: Vec<usize> = (start..end).map(|i| self.rd(b, i)).collect();
            stdsort::sort(&mut v, |&a, &b| {
                let (a, b) = if asc { (a, b) } else { (b, a) };
                if a == 0 {
                    return true;
                }
                if b == 0 {
                    return false;
                }
                match (c, cmp) {
                    (Some(c), f) if f != 0 => self.host.call_cmp(c, f, a, b).is_some_and(|r| r < 0),
                    _ => false,
                }
            });
            if self.buf.get() == b && self.num.get() >= end {
                for (k, x) in (start..end).zip(v) {
                    self.wr(b, k, x);
                }
            }
        });
    }

    fn insertion_sort(&self, start: u32, end: u32, asc: bool) {
        let id = self.sub_id.get();
        let b = self.buf.get();
        let es = self.es.get();
        let mut tmp = [0u8; 16];
        for i in start + 1..end {
            // SAFETY: slots inside the buffer; `tmp` holds an element (at most 8 bytes).
            unsafe { ptr::copy_nonoverlapping(self.slot(b, i), tmp.as_mut_ptr(), es) };
            let mut j = i as i64 - 1;
            while j >= i64::from(start) {
                let at_j = self.slot(b, j as u32);
                // SAFETY: both point at primitives of the element type.
                let (x, y) = unsafe { (read_num(id, tmp.as_ptr()), read_num(id, at_j)) };
                let (x, y) = if asc { (x, y) } else { (y, x) };
                if num_cmp(&x, &y) != Some(Ord_::Less) {
                    break;
                }
                // SAFETY: neighbouring slots inside the buffer.
                unsafe { ptr::copy_nonoverlapping(at_j, self.slot(b, j as u32 + 1), es) };
                j -= 1;
            }
            // SAFETY: slot j+1 is inside the buffer.
            unsafe { ptr::copy_nonoverlapping(tmp.as_ptr(), self.slot(b, (j + 1) as u32), es) };
        }
    }

    /// Selection sort driven by a script `less` callback.
    pub fn sort_callback(&self, func: P, start: u32, count: u32) {
        if count < 2 {
            return;
        }
        let num = self.num.get();
        let end = if u64::from(start) + u64::from(count) >= 1 << 32 {
            u32::MAX
        } else {
            start + count
        }
        .min(num);
        if start >= num {
            self.host.throw("Index out of bounds");
            return;
        }
        let engine = self.engine();
        let c = self.host.begin_ctx(engine, true);
        let mut i = start;
        while i + 1 < end {
            let mut best = i;
            for j in i + 1..end {
                let r = self
                    .host
                    .call_less(c, func, self.at(j) as P, self.at(best) as P);
                match r {
                    None => break,
                    Some(true) => best = j,
                    Some(false) => {}
                }
            }
            if best != i && !self.buf.get().is_null() {
                let b = self.buf.get();
                // SAFETY: both slots are inside the buffer, distinct.
                unsafe {
                    ptr::swap_nonoverlapping(self.slot(b, i), self.slot(b, best), self.es.get())
                };
            }
            i += 1;
        }
        self.host.end_ctx(engine, c, true);
    }

    pub fn enum_references(&self, engine: P) {
        if self.sub_id.get() & TYPEID_MASK_OBJECT == 0 || self.buf.get().is_null() {
            return;
        }
        let sub = self.host.type_by_id(engine, self.sub_id.get());
        let flags = self.host.type_flags(sub);
        let b = self.buf.get();
        for n in 0..self.num.get() {
            let o = self.rd(b, n);
            if o == 0 {
                continue;
            }
            if flags & OBJ_REF != 0 {
                self.host.gc_enum(engine, o);
            } else if flags & OBJ_VALUE != 0 && flags & OBJ_GC != 0 {
                self.host.gc_forward(engine, o, sub);
            }
        }
    }

    pub fn add_ref(&self) {
        self.gc.set(false);
        self.rc.fetch_add(1, Ordering::AcqRel);
    }

    pub fn ref_count(&self) -> i32 {
        self.rc.load(Ordering::Acquire)
    }

    pub fn set_flag(&self) {
        self.gc.set(true);
    }

    pub fn flag(&self) -> bool {
        self.gc.get()
    }
}

/// Drops one reference; the array is destroyed with the last one.
///
/// # Safety
/// `a` came from `Array::new` and the caller owns a reference.
pub unsafe fn release(a: *mut Array) {
    // SAFETY: caller contract.
    let Some(r) = (unsafe { a.as_ref() }) else {
        return;
    };
    r.gc.set(false);
    if r.rc.fetch_sub(1, Ordering::AcqRel) == 1 {
        // SAFETY: that was the last reference; nothing else points at the box.
        drop(unsafe { Box::from_raw(a) });
    }
}

impl Drop for Array {
    fn drop(&mut self) {
        let b = self.buf.get();
        if !b.is_null() {
            self.destruct(b, 0, self.num.get());
            self.free_buf(b, self.max.get());
        }
        self.host.type_release(self.ty);
    }
}

/// Validates `array<T>` for the subtype `T` of the template instance `ti`; sets `dont_gc` when the
/// instance cannot take part in a reference cycle.
pub fn template_callback(host: &dyn Host, ti: P, dont_gc: &mut bool) -> bool {
    let id = host.sub_type_id(ti);
    if id == TYPEID_VOID {
        return false;
    }
    let engine = host.type_engine(ti);
    if id & TYPEID_MASK_OBJECT != 0 && id & TYPEID_OBJHANDLE == 0 {
        let sub = host.type_by_id(engine, id);
        let flags = host.type_flags(sub);
        if flags & OBJ_VALUE != 0 && flags & OBJ_POD == 0 {
            if !host.has_default_ctor(sub) {
                host.message(engine, "The subtype has no default constructor");
                return false;
            }
        } else if flags & OBJ_REF != 0 {
            let found = host.engine_prop(engine, EP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE) == 0
                && host.has_default_factory(sub);
            if !found {
                let decl = if sub == 0 {
                    "UNKNOWN".to_string()
                } else {
                    host.type_decl(engine, host.type_id(sub))
                };
                host.message(
                    engine,
                    &format!("The subtype '{decl}' has no default factory"),
                );
                return false;
            }
        }
        if flags & OBJ_GC == 0 {
            *dont_gc = true;
        }
    } else if id & TYPEID_OBJHANDLE == 0 {
        *dont_gc = true;
    } else {
        let flags = host.type_flags(host.type_by_id(engine, id));
        if flags & OBJ_GC == 0 {
            if flags & OBJ_SCRIPT_OBJECT != 0 {
                if flags & OBJ_NOINHERIT != 0 {
                    *dont_gc = true;
                }
            } else {
                *dont_gc = true;
            }
        }
    }
    true
}
