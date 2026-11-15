//! C ABI of the options and key config window shells, mirrored in
//! `amnesia/src/game/RustMenuHost.h`.

use super::host::{self, HostFn, Wd};
use super::keyconfig_win as kcw;
use super::options_win::{self as ow, Vals};
use super::store::{CbData, Kind, resolve};

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10a_set_host(f: HostFn, opts_size: usize) -> i32 {
    if opts_size != size_of::<Vals>() {
        return 0;
    }
    host::set_host(f);
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10a_opt_new(set: usize) -> u64 {
    ow::create(set)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10a_opt_create_gui(me: u64) -> usize {
    ow::create_gui(me)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10a_opt_exit_pressed(me: u64) {
    ow::exit_pressed(me);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10a_opt_on_set_active(me: u64, active: i32) {
    ow::on_set_active(me, active != 0);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10a_opt_free(me: u64) {
    ow::free(me);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10a_kc_new(set: usize) -> u64 {
    kcw::create(set)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10a_kc_create_gui(me: u64) -> usize {
    kcw::create_gui(me)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10a_kc_exit_pressed(me: u64) {
    kcw::exit_pressed(me);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10a_kc_on_set_active(me: u64, active: i32) {
    kcw::on_set_active(me, active != 0);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10a_kc_free(me: u64) {
    kcw::free(me);
}

/// # Safety
/// `data` points to a live callback record.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a10a_cb(token: usize, widget: usize, data: *const CbData) -> i32 {
    // SAFETY: per the contract.
    let Some(data) = (unsafe { data.as_ref() }) else {
        return 1;
    };
    let Some(cb) = resolve(token) else {
        return 1;
    };
    let w = Wd(widget);
    let handled = match cb.kind {
        Kind::Options => ow::callback(&cb, w, data),
        Kind::KeyConfig | Kind::Entry => kcw::callback(&cb, w, data),
    };
    i32::from(handled)
}
