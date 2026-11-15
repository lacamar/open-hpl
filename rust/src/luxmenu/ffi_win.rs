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
        Kind::Main => super::mainmenu::callback(&cb, w, data),
    };
    i32::from(handled)
}
