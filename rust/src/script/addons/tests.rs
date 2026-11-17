//! Array logic against a mock engine: element storage, bounds exceptions, reference counting of handles,
//! comparison through `opCmp`/`opEquals`, sort order. Real-engine behaviour is covered by the script
//! suite replay in `tests/golden_eas.rs`.

use super::array::{Array, release, template_callback};
use super::host::*;
use std::cell::{Cell, RefCell};
use std::collections::HashMap;

const T_INT: P = 1;
const T_FLOAT: P = 2;
const T_HANDLES: P = 3;
const T_OBJS: P = 4;
const T_PLAIN: P = 5;
const T_ELEM: P = 10;
const T_PLAIN_ELEM: P = 11;
const ID_ELEM: i32 = 0x0400_0007;
const ID_PLAIN: i32 = 0x0400_0008;
const OPCMP: P = 100;
const OPEQ: P = 101;

#[derive(Default)]
struct Mock {
    exc: RefCell<Vec<String>>,
    rc: RefCell<HashMap<P, i32>>,
    key: RefCell<HashMap<P, i32>>,
    user: RefCell<HashMap<P, P>>,
    next: Cell<P>,
    cmp_calls: Cell<u32>,
    notified: Cell<u32>,
}

impl Mock {
    fn leak() -> &'static Mock {
        let m = Box::leak(Box::<Mock>::default());
        m.next.set(1000);
        m
    }

    fn obj(&self, k: i32) -> P {
        let p = self.next.get();
        self.next.set(p + 8);
        self.rc.borrow_mut().insert(p, 1);
        self.key.borrow_mut().insert(p, k);
        p
    }

    fn rc(&self, p: P) -> i32 {
        self.rc.borrow().get(&p).copied().unwrap_or(0)
    }

    fn k(&self, p: P) -> i32 {
        self.key.borrow().get(&p).copied().unwrap_or(-1)
    }

    fn take_exc(&self) -> Vec<String> {
        std::mem::take(&mut *self.exc.borrow_mut())
    }
}

impl Host for Mock {
    fn active_ctx(&self) -> P {
        1
    }
    fn throw(&self, msg: &str) {
        self.exc.borrow_mut().push(msg.to_string());
    }
    fn lock(&self, _: bool) {}
    fn type_engine(&self, _: P) -> P {
        99
    }
    fn type_flags(&self, ty: P) -> u64 {
        match ty {
            T_ELEM | T_PLAIN_ELEM => OBJ_REF,
            _ => OBJ_REF | OBJ_GC,
        }
    }
    fn type_id(&self, ty: P) -> i32 {
        match ty {
            T_ELEM => ID_ELEM,
            T_PLAIN_ELEM => ID_PLAIN,
            _ => 0,
        }
    }
    fn type_name(&self, ty: P) -> String {
        match ty {
            T_ELEM => "Obj".into(),
            T_PLAIN_ELEM => "Plain".into(),
            _ => "?".into(),
        }
    }
    fn type_size(&self, _: P) -> usize {
        8
    }
    fn sub_type(&self, ty: P) -> P {
        match ty {
            T_HANDLES | T_OBJS => T_ELEM,
            T_PLAIN => T_PLAIN_ELEM,
            _ => 0,
        }
    }
    fn sub_type_id(&self, ty: P) -> i32 {
        match ty {
            T_INT => TYPEID_INT32,
            T_FLOAT => TYPEID_FLOAT,
            T_HANDLES => ID_ELEM | TYPEID_OBJHANDLE,
            T_OBJS => ID_ELEM,
            _ => ID_PLAIN,
        }
    }
    fn type_by_id(&self, _: P, id: i32) -> P {
        if id & 0xFF == 7 { T_ELEM } else { T_PLAIN_ELEM }
    }
    fn type_addref(&self, _: P) {}
    fn type_release(&self, _: P) {}
    fn type_user(&self, ty: P) -> P {
        self.user.borrow().get(&ty).copied().unwrap_or(0)
    }
    fn set_type_user(&self, ty: P, p: P) {
        self.user.borrow_mut().insert(ty, p);
    }
    fn methods(&self, ty: P) -> Vec<FuncInfo> {
        if ty != T_ELEM {
            return Vec::new();
        }
        let f = |f, name: &[u8], ret_id| FuncInfo {
            f,
            name: name.to_vec(),
            ret_id,
            ret_flags: 0,
            params: 1,
            read_only: true,
            p0_id: ID_ELEM,
            p0_flags: TM_INREF | TM_CONST,
        };
        vec![
            f(OPCMP, b"opCmp", TYPEID_INT32),
            f(OPEQ, b"opEquals", TYPEID_BOOL),
        ]
    }
    fn has_default_ctor(&self, _: P) -> bool {
        true
    }
    fn has_default_factory(&self, ty: P) -> bool {
        ty == T_ELEM
    }
    fn engine_prop(&self, _: P, _: i32) -> i32 {
        0
    }
    fn message(&self, _: P, text: &str) {
        self.exc.borrow_mut().push(format!("msg {text}"));
    }
    fn type_decl(&self, _: P, _: i32) -> String {
        "Obj".into()
    }
    fn create_object(&self, _: P, ty: P) -> P {
        if ty == T_ELEM { self.obj(0) } else { 0 }
    }
    fn assign_object(&self, _: P, dst: P, src: P, _: P) {
        let k = self.k(src);
        self.key.borrow_mut().insert(dst, k);
    }
    fn addref_object(&self, _: P, obj: P, _: P) {
        if let Some(r) = self.rc.borrow_mut().get_mut(&obj) {
            *r += 1;
        }
    }
    fn release_object(&self, _: P, obj: P, _: P) {
        if let Some(r) = self.rc.borrow_mut().get_mut(&obj) {
            *r -= 1;
        }
    }
    fn notify_gc(&self, _: P, _: P, _: P) {
        self.notified.set(self.notified.get() + 1);
    }
    fn gc_enum(&self, _: P, _: P) {}
    fn gc_forward(&self, _: P, _: P, _: P) {}
    fn hndl_assign(&self, _: P, _: P, _: P, _: P) -> bool {
        false
    }
    fn begin_ctx(&self, _: P, _: bool) -> CmpCtx {
        CmpCtx {
            ctx: 1,
            nested: true,
        }
    }
    fn end_ctx(&self, _: P, _: CmpCtx, _: bool) {}
    fn call_eq(&self, _: CmpCtx, f: P, obj: P, arg: P) -> bool {
        f == OPEQ && self.k(obj) == self.k(arg)
    }
    fn call_cmp(&self, _: CmpCtx, f: P, obj: P, arg: P) -> Option<i32> {
        self.cmp_calls.set(self.cmp_calls.get() + 1);
        (f == OPCMP).then(|| (self.k(obj) - self.k(arg)).signum())
    }
    fn call_less(&self, _: CmpCtx, _: P, a: P, b: P) -> Option<bool> {
        // by address: both point at i32 elements
        // SAFETY: the tests pass element addresses of live int arrays.
        Some(unsafe { (a as *const i32).read() < (b as *const i32).read() })
    }
}

fn new_array(m: &'static Mock, ty: P, len: u32) -> &'static Array {
    let a = Array::new(m, ty);
    // SAFETY: fresh array, leaked for the test.
    let r = unsafe { &*a };
    r.init_len(len);
    r
}

fn ints(a: &Array) -> Vec<i32> {
    (0..a.size())
        // SAFETY: element slots of an int array.
        .map(|i| unsafe { a.at(i).cast::<i32>().read() })
        .collect()
}

fn push(a: &Array, v: i32) {
    let mut x = v;
    a.insert_last(std::ptr::from_mut(&mut x) as P);
}

fn handles(m: &'static Mock, ks: &[i32]) -> (&'static Array, Vec<P>) {
    let a = new_array(m, T_HANDLES, 0);
    let objs: Vec<P> = ks.iter().map(|&k| m.obj(k)).collect();
    for &o in &objs {
        let mut h = o;
        a.insert_last(std::ptr::from_mut(&mut h) as P);
    }
    (a, objs)
}

fn order(m: &Mock, a: &Array) -> Vec<i32> {
    (0..a.size())
        // SAFETY: element slots of a handle array hold pointers.
        .map(|i| m.k(unsafe { a.at(i).cast::<usize>().read() }))
        .collect()
}

#[test]
fn int_array_operations() {
    let m = Mock::leak();
    let a = new_array(m, T_INT, 0);
    for v in [5, 3, 9, 1, 3] {
        push(a, v);
    }
    assert_eq!(ints(a), [5, 3, 9, 1, 3]);
    let mut x = 10;
    a.insert_at(0, std::ptr::from_mut(&mut x) as P);
    x = 11;
    a.insert_at(3, std::ptr::from_mut(&mut x) as P);
    assert_eq!(ints(a), [10, 5, 3, 11, 9, 1, 3]);
    a.remove_at(1);
    a.remove_last();
    a.remove_range(1, 2);
    assert_eq!(ints(a), [10, 9, 1]);
    a.resize(5);
    assert_eq!(ints(a), [10, 9, 1, 0, 0]);
    a.sort(0, 5, true);
    assert_eq!(ints(a), [0, 0, 1, 9, 10]);
    a.sort(0, 5, false);
    a.sort(1, 3, true);
    assert_eq!(ints(a), [10, 0, 1, 9, 0]);
    a.reverse();
    assert_eq!(ints(a), [0, 9, 1, 0, 10]);
    x = 9;
    assert_eq!(a.find(0, std::ptr::from_mut(&mut x) as P), 1);
    assert_eq!(a.find(2, std::ptr::from_mut(&mut x) as P), -1);
    assert!(m.take_exc().is_empty());
    a.resize(0xFFFF_FFF0);
    assert_eq!(m.take_exc(), ["Too large array size"]);
    assert!(a.at(5).is_null() || a.size() > 5);
}

#[test]
fn bounds_raise_script_exceptions() {
    let m = Mock::leak();
    let a = new_array(m, T_INT, 3);
    assert!(a.at(3).is_null());
    a.remove_at(3);
    a.insert_at(4, 0);
    a.remove_range(4, 1);
    a.sort(2, 5, true);
    a.sort(3, 2, true);
    assert_eq!(
        m.take_exc(),
        std::iter::repeat_n("Index out of bounds".to_string(), 6).collect::<Vec<_>>()
    );
    a.remove_range(1, 0);
    a.sort(3, 1, true);
    assert!(m.take_exc().is_empty());
}

#[test]
fn array_equality_and_copy() {
    let m = Mock::leak();
    let (a, b) = (new_array(m, T_INT, 0), new_array(m, T_INT, 0));
    for v in [1, 2, 3] {
        push(a, v);
        push(b, v);
    }
    assert!(a.equals_array(b));
    push(b, 4);
    assert!(!a.equals_array(b));
    a.copy_from(b);
    assert_eq!(ints(a), [1, 2, 3, 4]);
    assert!(a.equals_array(b));
    a.insert_at_array(1, b);
    assert_eq!(ints(a), [1, 1, 2, 3, 4, 2, 3, 4]);
    let c = new_array(m, T_INT, 0);
    for v in [1, 2, 3, 4] {
        push(c, v);
    }
    c.insert_at_array(2, c);
    assert_eq!(ints(c), [1, 2, 1, 2, 3, 4, 3, 4]);
    let f = new_array(m, T_FLOAT, 0);
    for v in [0.5f32, f32::NAN] {
        let mut x = v;
        f.insert_last(std::ptr::from_mut(&mut x) as P);
    }
    let g = new_array(m, T_FLOAT, 0);
    for v in [0.5f32, f32::NAN] {
        let mut x = v;
        g.insert_last(std::ptr::from_mut(&mut x) as P);
    }
    assert!(!f.equals_array(g));
}

#[test]
fn handles_are_reference_counted() {
    let m = Mock::leak();
    let (a, objs) = handles(m, &[3, 1, 2]);
    assert_eq!(m.notified.get(), 1);
    assert!(objs.iter().all(|&o| m.rc(o) == 2));
    a.remove_at(0);
    assert_eq!(m.rc(objs[0]), 1);
    let b = new_array(m, T_HANDLES, 0);
    b.copy_from(a);
    assert_eq!(b.size(), 2);
    assert!(objs[1..].iter().all(|&o| m.rc(o) == 3));
    let mut h = 0usize;
    b.set_value(0, std::ptr::from_mut(&mut h) as P);
    assert_eq!(m.rc(objs[1]), 2);
    // SAFETY: the last reference of each array.
    unsafe {
        release(std::ptr::from_ref(a).cast_mut());
        release(std::ptr::from_ref(b).cast_mut());
    }
    assert!(objs.iter().all(|&o| m.rc(o) == 1));
}

#[test]
fn objects_sort_and_find_through_op_cmp() {
    let m = Mock::leak();
    let (a, objs) = handles(m, &[4, 1, 3, 1, 2]);
    a.sort(0, 5, true);
    assert_eq!(order(m, a), [1, 1, 2, 3, 4]);
    a.sort(0, 5, false);
    assert_eq!(order(m, a), [4, 3, 2, 1, 1]);
    a.sort(1, 3, true);
    assert_eq!(order(m, a), [4, 1, 2, 3, 1]);
    let probe = m.obj(3);
    let mut h = probe;
    assert_eq!(a.find(0, std::ptr::from_mut(&mut h) as P), 3);
    assert_eq!(a.find_by_ref(0, std::ptr::from_mut(&mut h) as P), -1);
    let mut h = objs[2];
    assert_eq!(a.find_by_ref(0, std::ptr::from_mut(&mut h) as P), 3);
    assert!(m.take_exc().is_empty());
}

#[test]
fn null_handles_sort_first() {
    let m = Mock::leak();
    let (a, _) = handles(m, &[2, 1]);
    let mut null = 0usize;
    a.insert_at(1, std::ptr::from_mut(&mut null) as P);
    a.insert_last(std::ptr::from_mut(&mut null) as P);
    a.sort(0, 4, true);
    assert_eq!(order(m, a)[..2], [-1, -1]);
    a.sort(0, 4, false);
    assert_eq!(order(m, a)[2..], [-1, -1]);
}

#[test]
fn value_objects_are_created_and_assigned() {
    let m = Mock::leak();
    let a = new_array(m, T_OBJS, 3);
    let live = |m: &Mock| m.rc.borrow().len();
    assert_eq!(live(m), 3);
    let src = m.obj(42);
    a.insert_at(1, src);
    assert_eq!(a.size(), 4);
    assert_eq!(m.k(a.at(1) as P), 42);
    assert_eq!(m.k(a.at(0) as P), 0);
    a.resize(1);
    assert_eq!(a.size(), 1);
}

#[test]
fn missing_comparison_methods_are_reported() {
    let m = Mock::leak();
    let a = new_array(m, T_PLAIN, 2);
    a.sort(0, 2, true);
    let mut h = 0usize;
    assert_eq!(a.find(0, std::ptr::from_mut(&mut h) as P), -1);
    assert_eq!(
        m.take_exc(),
        [
            "Type 'Plain' does not have a matching opCmp method",
            "Type 'Plain' does not have a matching opEquals or opCmp method"
        ]
    );
}

#[test]
fn callback_sort_selects_in_place() {
    let m = Mock::leak();
    let a = new_array(m, T_INT, 0);
    for v in [5, 3, 9, 1, 3] {
        push(a, v);
    }
    a.sort_callback(1, 1, 3);
    assert_eq!(ints(a), [5, 1, 3, 9, 3]);
}

#[test]
fn template_callback_rules() {
    let m = Mock::leak();
    let mut skip = false;
    assert!(template_callback(m, T_INT, &mut skip));
    assert!(skip);
    skip = false;
    assert!(template_callback(m, T_HANDLES, &mut skip));
    assert!(skip, "a handle to a non-GC application type needs no GC");
    assert!(template_callback(m, T_OBJS, &mut skip));
}
