//! List items (`cWidgetItem`, `cWidgetItemProperty`) owned by the list box or combo box that
//! holds them; the C++ facade passes the item pointers around opaquely.

#![allow(clippy::not_unsafe_ptr_arg_deref)] // C ABI of the facade: pointers are items and out-parameters the caller owns

use std::ffi::c_void;

use super::gw::{self, with};
use super::gw_h20::{self, P};
use super::set::W;

pub const PROP_STRING: i32 = 0;
pub const PROP_IMAGE: i32 = 1;

pub struct Prop {
    ty: i32,
    text: Vec<u32>,
    gfx: usize,
}

pub struct Item {
    cont: W,
    index: i32,
    selectable: bool,
    selected: bool,
    text: Vec<u32>,
    props: Vec<*mut Prop>,
    user_data: *mut c_void,
    user_value: i32,
}

impl Drop for Item {
    fn drop(&mut self) {
        for p in self.props.drain(..) {
            // SAFETY: from `Box::into_raw` in `add_prop`, freed once.
            drop(unsafe { Box::from_raw(p) });
        }
    }
}

fn it<'a>(p: *mut Item) -> Option<&'a mut Item> {
    // SAFETY: item pointers come from `add_item` and live until their container removes them.
    unsafe { p.as_mut() }
}

fn items(w: W) -> Vec<*mut Item> {
    gw::read(w, |g| g.items.clone())
}

/// The container's virtual `UpdateProperties`.
fn update_properties(w: W) {
    gw_h20::update_properties(w);
}

pub(crate) fn num(w: W) -> usize {
    gw::read(w, |g| g.items.len())
}

pub(crate) fn get(w: W, idx: i32) -> *mut Item {
    usize::try_from(idx)
        .ok()
        .and_then(|i| gw::read(w, |g| g.items.get(i).copied()))
        .unwrap_or(std::ptr::null_mut())
}

pub(crate) fn add(w: W, text: &[u32]) -> *mut Item {
    let idx = i32::try_from(num(w)).unwrap_or(i32::MAX);
    let p = Box::into_raw(Box::new(Item {
        cont: w,
        index: idx,
        selectable: true,
        selected: false,
        text: text.to_vec(),
        props: Vec::new(),
        user_data: std::ptr::null_mut(),
        user_value: 0,
    }));
    with(w, |g| g.items.push(p));
    update_properties(w);
    p
}

fn delete(p: *mut Item) {
    // SAFETY: from `add`, removed from its container before.
    drop(unsafe { Box::from_raw(p) });
}

pub(crate) fn remove_at(w: W, idx: i32) {
    let p = get(w, idx);
    if !p.is_null() {
        with(w, |g| g.items.retain(|x| *x != p));
        delete(p);
    }
    update_properties(w);
}

pub(crate) fn remove_text(w: W, text: &[u32]) {
    let hit = items(w)
        .into_iter()
        .find(|p| it(*p).is_some_and(|i| i.text == text));
    if let Some(p) = hit {
        with(w, |g| g.items.retain(|x| *x != p));
        delete(p);
    }
    update_properties(w);
}

pub(crate) fn clear(w: W) {
    for p in with(w, |g| std::mem::take(&mut g.items)).unwrap_or_default() {
        delete(p);
    }
    update_properties(w);
}

/// `~iWidgetItemContainer`: no property update.
pub(crate) fn free_all(w: W) {
    for p in with(w, |g| std::mem::take(&mut g.items)).unwrap_or_default() {
        delete(p);
    }
}

pub(crate) fn has(w: W, text: &[u32]) -> bool {
    items(w)
        .into_iter()
        .any(|p| it(p).is_some_and(|i| i.text == text))
}

pub(crate) fn set_selected(p: *mut Item, v: bool) {
    let Some(i) = it(p) else { return };
    if i.selected == v {
        return;
    }
    i.selected = v;
    let cont = i.cont;
    // SAFETY: as in `gw_h20::call`.
    unsafe {
        if let Some(P::List(l)) = gw_h20::ptr(cont) {
            if v {
                super::h20_ffi::ohpl_h20_lb_add_item_to_selection(gw_h20::vt(), l, p as usize);
            } else {
                super::h20_ffi::ohpl_h20_lb_remove_item_from_selection(gw_h20::vt(), l, p as usize);
            }
        }
    }
}

macro_rules! cffi {
    ($($name:ident ( $($a:ident : $t:ty),* ) $(-> $r:ty)? => $body:expr;)*) => {$(
        #[unsafe(no_mangle)]
        pub extern "C" fn $name($($a: $t),*) $(-> $r)? { $body }
    )*};
}

fn slice<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if p.is_null() {
        &[]
    } else {
        // SAFETY: the caller passes a readable range.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

cffi! {
    ohpl_gwi_num(w: W) -> usize => num(w);
    ohpl_gwi_get(w: W, idx: i32) -> *mut Item => get(w, idx);
    ohpl_gwi_add(w: W, p: *const u32, n: usize) -> *mut Item => add(w, slice(p, n));
    ohpl_gwi_remove_at(w: W, idx: i32) => remove_at(w, idx);
    ohpl_gwi_remove_text(w: W, p: *const u32, n: usize) => remove_text(w, slice(p, n));
    ohpl_gwi_clear(w: W) => clear(w);
    ohpl_gwi_has(w: W, p: *const u32, n: usize) -> bool => has(w, slice(p, n));
    ohpl_gwi_set_text_at(w: W, idx: i32, p: *const u32, n: usize) => {
        if let Some(i) = it(get(w, idx)) {
            i.text = slice(p, n).to_vec();
        }
    };
    ohpl_gwi_text(p: *mut Item, n: *mut usize) -> *const u32 => match it(p) {
        Some(i) => {
            // SAFETY: `n` is writable.
            unsafe { n.write(i.text.len()) };
            i.text.as_ptr()
        }
        None => {
            // SAFETY: `n` is writable.
            unsafe { n.write(0) };
            std::ptr::null()
        }
    };
    ohpl_gwi_set_text(p: *mut Item, t: *const u32, n: usize) => {
        if let Some(i) = it(p) {
            i.text = slice(t, n).to_vec();
        }
    };
    ohpl_gwi_index(p: *mut Item) -> i32 => it(p).map_or(0, |i| i.index);
    ohpl_gwi_set_index(p: *mut Item, v: i32) => {
        if let Some(i) = it(p) {
            i.index = v;
        }
    };
    ohpl_gwi_selectable(p: *mut Item) -> bool => it(p).is_some_and(|i| i.selectable);
    ohpl_gwi_set_selectable(p: *mut Item, v: bool) => {
        if let Some(i) = it(p) {
            i.selectable = v;
        }
    };
    ohpl_gwi_selected(p: *mut Item) -> bool => it(p).is_some_and(|i| i.selectable && i.selected);
    ohpl_gwi_set_selected(p: *mut Item, v: bool) => set_selected(p, v);
    ohpl_gwi_user_data(p: *mut Item) -> *mut c_void => it(p).map_or(std::ptr::null_mut(), |i| i.user_data);
    ohpl_gwi_set_user_data(p: *mut Item, d: *mut c_void) => {
        if let Some(i) = it(p) {
            i.user_data = d;
        }
    };
    ohpl_gwi_user_value(p: *mut Item) -> i32 => it(p).map_or(0, |i| i.user_value);
    ohpl_gwi_set_user_value(p: *mut Item, v: i32) => {
        if let Some(i) = it(p) {
            i.user_value = v;
        }
    };
    ohpl_gwi_prop_num(p: *mut Item) -> i32 => it(p).map_or(0, |i| i.props.len() as i32);
    ohpl_gwi_prop(p: *mut Item, idx: i32) -> *mut Prop => it(p)
        .and_then(|i| usize::try_from(idx).ok().and_then(|x| i.props.get(x).copied()))
        .unwrap_or(std::ptr::null_mut());
    ohpl_gwi_add_prop_text(p: *mut Item, t: *const u32, n: usize) => add_prop(p, PROP_STRING, slice(t, n), 0);
    ohpl_gwi_add_prop_gfx(p: *mut Item, g: usize) => add_prop(p, PROP_IMAGE, &[], g);
    ohpl_gwi_prop_type(q: *mut Prop) -> i32 => prop(q).map_or(0, |x| x.ty);
    ohpl_gwi_prop_gfx(q: *mut Prop) -> usize => prop(q).map_or(0, |x| x.gfx);
    ohpl_gwi_prop_text(q: *mut Prop, n: *mut usize) -> *const u32 => match prop(q) {
        Some(x) => {
            // SAFETY: `n` is writable.
            unsafe { n.write(x.text.len()) };
            x.text.as_ptr()
        }
        None => {
            // SAFETY: `n` is writable.
            unsafe { n.write(0) };
            std::ptr::null()
        }
    };
}

fn prop<'a>(q: *mut Prop) -> Option<&'a mut Prop> {
    // SAFETY: property pointers come from `add_prop` and live with their item.
    unsafe { q.as_mut() }
}

fn add_prop(p: *mut Item, ty: i32, text: &[u32], gfx: usize) {
    if let Some(i) = it(p) {
        i.props.push(Box::into_raw(Box::new(Prop {
            ty,
            text: text.to_vec(),
            gfx,
        })));
    }
}
