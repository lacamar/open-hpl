//! C ABI of the inventory shell (`amnesia/src/game/LuxInventory.h`): every `cLuxInventory` method, item accessor and
//! the widget callback entry. Item pointers are arena handle bits, 0 is null.

use super::event::Str;
use super::invgui;
use super::invhost::{self as host, OpFn};
use super::invobj::{self as obj, Inv, ItemRef, bits, from_bits, g};
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink = unsafe extern "C" fn(Ctx, *const u8, usize);

fn b<'a>(s: Str) -> &'a [u8] {
    if s.n == 0 || s.p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes a live `n`-byte buffer.
    unsafe { std::slice::from_raw_parts(s.p, s.n) }
}

fn wide<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes a live `n`-unit buffer.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn sink(s: Sink, c: Ctx, v: &[u8]) {
    // SAFETY: `s` accepts `c`.
    unsafe { s(c, v.as_ptr(), v.len()) }
}

fn with_item<R>(h: usize, f: impl FnOnce(&obj::Item) -> R) -> Option<R> {
    let item: ItemRef = from_bits(h)?;
    g()?.items.get(item).map(f)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_set_host(f: Option<OpFn>) {
    host::set_host(f);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_new() {
    let mut i = Inv::blank();
    invgui::build(&mut i);
    obj::install(Box::new(i));
    invgui::create_gui();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_free() {
    obj::drop_all();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_load_fonts() {
    invgui::load_fonts();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_reset() {
    obj::reset();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_on_game_start() {
    obj::on_game_start();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_update(dt: f32) {
    obj::update(dt);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_on_enter() {
    obj::on_enter_container();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_on_leave() {
    obj::on_leave_container();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_on_draw(frame_time: f32) {
    invgui::on_draw(frame_time);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_exit_pressed() {
    obj::exit_pressed();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_load_script() {
    obj::load_script();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_gamepad_changed() {
    obj::gamepad_changed();
}

/// # Safety
/// `remove` is null or writable.
#[allow(clippy::too_many_arguments)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_inv_add_item(
    name: Str,
    ty: i32,
    sub: Str,
    image: Str,
    amount: f32,
    val: Str,
    extra: Str,
    remove: *mut i32,
) -> usize {
    let mut flag = 0;
    let r = obj::add_item(
        &obj::NewItem {
            name: b(name),
            ty,
            sub: b(sub),
            image: b(image),
            amount,
            val: b(val),
            extra: b(extra),
        },
        &mut flag,
    );
    if !remove.is_null() {
        // SAFETY: writable per the contract.
        unsafe { remove.write(flag) };
    }
    bits(r)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_remove_by_name(name: Str) {
    obj::remove_item_by_name(b(name));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_remove(h: usize) {
    if let Some(h) = from_bits(h) {
        obj::remove_item(h);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_remove_of_type(t: i32) {
    obj::remove_item_of_type(t);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_item_index(h: usize) -> i32 {
    g().map_or(-1, |i| i.index_of(from_bits(h)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_get_by_name(name: Str) -> usize {
    bits(obj::get_item_by_name(b(name)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_get_by_subtype(sub: Str) -> usize {
    bits(obj::get_item_from_subtype(b(sub)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_has_of_type(t: i32) -> i32 {
    i32::from(obj::has_item_of_type(t))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_item_at(idx: i32) -> usize {
    let h = usize::try_from(idx)
        .ok()
        .and_then(|n| g().and_then(|i| i.order.get(n).copied()));
    bits(h)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_item_num() -> i32 {
    g().map_or(0, |i| i.order.len() as i32)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_add_combine(name: Str, a: Str, bb: Str, func: Str, auto_destroy: i32) {
    obj::add_combine(b(name), b(a), b(bb), b(func), auto_destroy != 0);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_remove_combine(name: Str) {
    obj::remove_combine(b(name));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_combine_num() -> i32 {
    g().map_or(0, |i| i.combine.len() as i32)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_find_combine(a: Str, bb: Str) -> i32 {
    obj::find_combine(b(a), b(bb)).map_or(-1, |i| i as i32)
}

/// Field `which` (0 name, 1 item a, 2 item b, 3 function) of combine callback `idx`.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_combine_str(idx: i32, which: i32, s: Sink, c: Ctx) {
    let v = usize::try_from(idx)
        .ok()
        .and_then(|n| g().and_then(|i| i.combine.get(n).cloned()));
    let Some(cb) = v else { return };
    sink(
        s,
        c,
        match which {
            0 => &cb.name,
            1 => &cb.a,
            2 => &cb.b,
            _ => &cb.func,
        },
    );
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_combine_auto(idx: i32) -> i32 {
    usize::try_from(idx)
        .ok()
        .and_then(|n| g().and_then(|i| i.combine.get(n).map(|c| i32::from(c.auto_destroy))))
        .unwrap_or(0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_run_script(cmd: Str) {
    obj::run_script(b(cmd));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_recompile(s: Sink, c: Ctx) -> i32 {
    let (ok, out) = obj::recompile_script();
    sink(s, c, &out);
    i32::from(ok)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_open_journal() {
    obj::open_journal();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_set_disabled(v: i32) {
    if let Some(i) = g() {
        i.disabled = v != 0;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_disabled() -> i32 {
    g().map_or(0, |i| i32::from(i.disabled))
}

/// # Safety
/// The wide buffer is a live `n`-unit array.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_inv_set_message(t: *const u32, n: usize, life: f32) {
    obj::set_message_text(wide(t, n), life);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_message_active() -> i32 {
    g().map_or(0, |i| i32::from(i.st.message_active != 0))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_picked() -> usize {
    bits(g().and_then(|i| i.picked))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_set_enter_from_journal(v: i32) {
    if let Some(i) = g() {
        i.st.enter_from_journal = u8::from(v != 0);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_set_equipped(h: usize) {
    obj::set_equipped(from_bits(h));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_equipped() -> usize {
    bits(g().and_then(|i| i.equipped))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_type_from_string(name: Str) -> i32 {
    obj::item_type_from_string(b(name))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_get_set() -> usize {
    g().map_or(0, |i| i.gui.set)
}

/// `which`: 0 name, 1 subtype, 2 value, 3 extra, 4 image name, 5 name entry, 6 description entry.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_item_str(h: usize, which: i32, s: Sink, c: Ctx) {
    let v = with_item(h, |it| match which {
        0 => it.name.clone(),
        1 => it.sub.clone(),
        2 => it.val.clone(),
        3 => it.extra.clone(),
        4 => it.image_name.clone(),
        5 => it.name_entry.clone(),
        _ => it.desc_entry.clone(),
    });
    sink(s, c, &v.unwrap_or_default());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_item_type(h: usize) -> i32 {
    with_item(h, |it| it.ty).unwrap_or(0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_item_count(h: usize) -> i32 {
    with_item(h, |it| it.count).unwrap_or(0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_item_amount(h: usize) -> f32 {
    with_item(h, |it| it.amount).unwrap_or(0.0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_item_image(h: usize) -> usize {
    with_item(h, |it| it.image).unwrap_or(0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_item_set_count(h: usize, n: i32) {
    if let Some(it) = from_bits(h).and_then(|r| g().and_then(|i| i.items.get_mut(r))) {
        it.count = n;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_item_set_entries(h: usize, name: Str, desc: Str) {
    if let Some(it) = from_bits(h).and_then(|r| g().and_then(|i| i.items.get_mut(r))) {
        it.name_entry = b(name).to_vec();
        it.desc_entry = b(desc).to_vec();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_type_show_pickup(t: i32) -> i32 {
    obj::info_of(t).map_or(0, |i| i32::from(i.show_pickup))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_set_show_journal(v: i32) {
    if let Some(i) = g() {
        i.show_journal_on_pickup = v != 0;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_cb(token: usize, w: usize, lval: i32, fval: f32) -> i32 {
    i32::from(invgui::dispatch(token, w, lval, fval))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_type_has_count(t: i32) -> i32 {
    obj::info_of(t).map_or(0, |i| i32::from(i.has_count))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_type_max_count(t: i32) -> i32 {
    obj::info_of(t).map_or(0, |i| i.max_count)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_type_has_max(t: i32) -> i32 {
    i32::from(super::itemtype::has_max_amount(t))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_inv_type_name(t: i32, s: Sink, c: Ctx) {
    sink(s, c, obj::info_of(t).map_or(&[][..], |i| i.name));
}
