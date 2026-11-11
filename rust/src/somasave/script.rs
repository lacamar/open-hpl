//! Script object (de)serialisation of `SomaSave.cpp`: properties by name and declaration, the
//! AngelScript engine reached through [`ScriptHost`] in the original call order.

use super::codec::{In, Out};
use std::collections::HashSet;

pub type Addr = usize;

pub const TYPEID_DOUBLE: i32 = 11;
pub const TYPEID_OBJHANDLE: i32 = 0x4000_0000;

/// What `WriteValue` / `ReadValue` find behind a non-handle, non-primitive type id.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Kind {
    None,
    Enum,
    ScriptObj,
    Str,
    WStr,
    Array,
    StrSet,
    Raw(usize),
    Other,
}

pub trait ScriptHost {
    /// Object behind a handle at `addr`, when it is a script object; None otherwise.
    fn handle_obj(&mut self, addr: Addr, type_id: i32) -> Option<Addr>;
    /// Reading a handle: creates the object when empty, returns it when its class is `class`.
    fn resolve_handle(&mut self, addr: Addr, type_id: i32, class: &[u8]) -> Option<Addr>;
    fn obj_class(&mut self, obj: Addr) -> Vec<u8>;
    fn prim_size(&mut self, type_id: i32) -> usize;
    fn kind(&mut self, type_id: i32) -> Kind;
    fn read_mem(&mut self, addr: Addr, n: usize) -> Vec<u8>;
    fn write_mem(&mut self, addr: Addr, data: &[u8]);
    fn string_get(&mut self, addr: Addr) -> Vec<u8>;
    fn string_set(&mut self, addr: Addr, s: &[u8]);
    fn wstring_get(&mut self, addr: Addr) -> Vec<u32>;
    fn wstring_set(&mut self, addr: Addr, s: &[u32]);
    fn array_len(&mut self, addr: Addr) -> u32;
    fn array_elem_type(&mut self, addr: Addr) -> i32;
    fn array_at(&mut self, addr: Addr, i: u32) -> Addr;
    fn array_resize(&mut self, addr: Addr, n: u32);
    fn set_items(&mut self, addr: Addr) -> Vec<Vec<u8>>;
    fn set_clear(&mut self, addr: Addr);
    fn set_insert(&mut self, addr: Addr, s: &[u8]);
    fn prop_count(&mut self, obj: Addr) -> u32;
    fn prop_saved(&mut self, obj: Addr, i: u32) -> bool;
    fn prop_name(&mut self, obj: Addr, i: u32) -> Vec<u8>;
    fn prop_type_id(&mut self, obj: Addr, i: u32) -> i32;
    fn prop_addr(&mut self, obj: Addr, i: u32) -> Addr;
    fn type_decl(&mut self, obj: Addr, type_id: i32) -> Vec<u8>;
}

pub fn write_value(
    h: &mut dyn ScriptHost,
    o: &mut Out,
    type_id: i32,
    addr: Addr,
    visited: &mut HashSet<Addr>,
) {
    if type_id & TYPEID_OBJHANDLE != 0 {
        match h.handle_obj(addr, type_id) {
            Some(obj) if !visited.contains(&obj) => {
                o.u8(1);
                let class = h.obj_class(obj);
                o.str(&class);
                write_object(h, o, obj, visited);
            }
            _ => o.u8(0),
        }
        return;
    }
    if type_id <= TYPEID_DOUBLE {
        let n = h.prim_size(type_id);
        let b = h.read_mem(addr, n);
        o.bytes(&b);
        return;
    }
    match h.kind(type_id) {
        Kind::None | Kind::Other => {}
        Kind::Enum => {
            let b = h.read_mem(addr, 4);
            o.bytes(&b);
        }
        Kind::ScriptObj => write_object(h, o, addr, visited),
        Kind::Str => {
            let s = h.string_get(addr);
            o.str(&s);
        }
        Kind::WStr => {
            let w = h.wstring_get(addr);
            o.len(w.len());
            for u in w {
                o.u32(u);
            }
        }
        Kind::Array => {
            let n = h.array_len(addr);
            o.u32(n);
            let mut i = 0;
            while i < h.array_len(addr) {
                let et = h.array_elem_type(addr);
                let a = h.array_at(addr, i);
                write_value(h, o, et, a, visited);
                i += 1;
            }
        }
        Kind::StrSet => {
            let items = h.set_items(addr);
            o.len(items.len());
            for s in items {
                o.str(&s);
            }
        }
        Kind::Raw(n) => {
            let b = h.read_mem(addr, n);
            o.bytes(&b);
        }
    }
}

pub fn read_value(h: &mut dyn ScriptHost, i: &mut In, version: i32, type_id: i32, addr: Addr) {
    if type_id & TYPEID_OBJHANDLE != 0 {
        if i.u8() == 0 {
            return;
        }
        let class = i.str();
        match h.resolve_handle(addr, type_id, class) {
            Some(obj) => read_object(h, i, version, obj),
            None => i.skip_object(),
        }
        return;
    }
    if type_id <= TYPEID_DOUBLE {
        let n = h.prim_size(type_id);
        read_mem(h, i, addr, n);
        return;
    }
    match h.kind(type_id) {
        Kind::None | Kind::Other => {}
        Kind::Enum => read_mem(h, i, addr, 4),
        Kind::ScriptObj => read_object(h, i, version, addr),
        Kind::Str => {
            let s = i.str();
            h.string_set(addr, s);
        }
        Kind::WStr => {
            let n = i.u32();
            if n > 1_000_000 {
                i.ok = false;
                return;
            }
            let mut w = vec![0u32; n as usize];
            if let Some(b) = i.take(n as usize * 4) {
                for (d, c) in w.iter_mut().zip(b.chunks_exact(4)) {
                    *d = c.try_into().map(u32::from_ne_bytes).unwrap_or(0);
                }
            }
            h.wstring_set(addr, &w);
        }
        Kind::Array => {
            let n = i.u32();
            if !i.ok || n > 1_000_000 {
                i.ok = false;
                return;
            }
            h.array_resize(addr, n);
            let mut k = 0;
            while k < n && i.ok {
                let et = h.array_elem_type(addr);
                let a = h.array_at(addr, k);
                read_value(h, i, version, et, a);
                k += 1;
            }
        }
        Kind::StrSet => {
            let n = if version >= 31 { i.u32() } else { 0 };
            h.set_clear(addr);
            let mut k = 0;
            while k < n && i.ok {
                let s = i.str();
                h.set_insert(addr, s);
                k += 1;
            }
        }
        Kind::Raw(n) => read_mem(h, i, addr, n),
    }
}

fn read_mem(h: &mut dyn ScriptHost, i: &mut In, addr: Addr, n: usize) {
    if let Some(b) = i.take(n) {
        h.write_mem(addr, b);
    }
}

pub fn write_object(h: &mut dyn ScriptHost, o: &mut Out, obj: Addr, visited: &mut HashSet<Addr>) {
    visited.insert(obj);
    let count = h.prop_count(obj);
    let props: Vec<u32> = (0..count).filter(|&k| h.prop_saved(obj, k)).collect();
    o.len(props.len());
    for k in props {
        let tid = h.prop_type_id(obj, k);
        let name = h.prop_name(obj, k);
        o.str(&name);
        let decl = h.type_decl(obj, tid);
        o.str(&decl);
        let mut value = Out::default();
        let a = h.prop_addr(obj, k);
        write_value(h, &mut value, tid, a, visited);
        o.len(value.s.len());
        o.bytes(&value.s);
    }
}

pub fn read_object(h: &mut dyn ScriptHost, i: &mut In, version: i32, obj: Addr) {
    let n = i.u32();
    let mut k = 0;
    while k < n && i.ok {
        let name = i.str();
        let decl = i.str();
        let len = i.u32() as usize;
        let end = i.p.saturating_add(len);
        for pi in 0..h.prop_count(obj) {
            let tid = h.prop_type_id(obj, pi);
            if name != h.prop_name(obj, pi).as_slice() {
                continue;
            }
            if decl != h.type_decl(obj, tid).as_slice() {
                continue;
            }
            let mut value = In {
                s: i.s,
                p: i.p,
                ok: true,
            };
            let a = h.prop_addr(obj, pi);
            read_value(h, &mut value, version, tid, a);
            break;
        }
        i.p = end;
        k += 1;
    }
}

pub fn write_script(h: &mut dyn ScriptHost, o: &mut Out, obj: Option<Addr>) {
    let mut visited = HashSet::new();
    o.u8(u8::from(obj.is_some()));
    if let Some(obj) = obj {
        let class = h.obj_class(obj);
        o.str(&class);
        write_object(h, o, obj, &mut visited);
    }
}

pub fn read_script(h: &mut dyn ScriptHost, i: &mut In, version: i32, obj: Option<Addr>) {
    if i.u8() == 0 {
        return;
    }
    let class = i.str();
    match obj {
        Some(obj) if class == h.obj_class(obj).as_slice() => read_object(h, i, version, obj),
        _ => i.skip_object(),
    }
}
