//! C ABI of the H5 graphics state, declared in `graphics/RustGraphicsH5.h`.

use std::ffi::c_void;

use super::atlas::{Atlas, Pix};
use super::bitmap::Surface;
use super::combo::ComboSettings;
use super::packer;
use crate::math::Rect2l;
use crate::string_ffi::{Sink, emit, units};
use crate::types::{Vector2f, Vector3l};

#[repr(C)]
pub struct OhplH5Surface {
    size: Vector3l,
    format: i32,
    bytes_per_pixel: i32,
}

fn surface(s: &OhplH5Surface) -> Surface {
    Surface {
        size: s.size,
        format: s.format,
        bytes_per_pixel: s.bytes_per_pixel,
    }
}

/// # Safety
/// `s` is valid; `data` is null or points to `len` writable bytes for the call.
unsafe fn pix<'a>(s: *const OhplH5Surface, data: *mut u8, len: usize) -> Option<Pix<'a>> {
    // SAFETY: caller contract.
    let s = unsafe { s.as_ref() }?;
    let data = if data.is_null() {
        None
    } else {
        // SAFETY: caller contract.
        Some(unsafe { std::slice::from_raw_parts_mut(data, len) })
    };
    Some(Pix {
        surface: surface(s),
        data,
    })
}

#[repr(C)]
pub struct OhplH5Added {
    rect: Rect2l,
    found_empty: bool,
}

macro_rules! atlas_flags {
    ($($get:ident $set:ident $f:ident;)*) => {$(
        /// # Safety
        /// `a` is a live atlas.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $get(a: *const c_void) -> bool {
            // SAFETY: caller contract.
            unsafe { a.cast::<Atlas>().as_ref() }.is_some_and(|a| a.$f)
        }

        /// # Safety
        /// `a` is a live atlas.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $set(a: *mut c_void, v: bool) {
            // SAFETY: caller contract.
            if let Some(a) = unsafe { a.cast::<Atlas>().as_mut() } {
                a.$f = v;
            }
        }
    )*};
}

atlas_flags! {
    ohpl_h5_atlas_full ohpl_h5_atlas_set_full full;
    ohpl_h5_atlas_updated ohpl_h5_atlas_set_updated updated;
    ohpl_h5_atlas_need_reorg ohpl_h5_atlas_set_need_reorg need_reorg;
    ohpl_h5_atlas_locked ohpl_h5_atlas_set_locked locked;
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h5_atlas_new(w: i32, h: i32, handle: i32) -> *mut c_void {
    Box::into_raw(Box::new(Atlas::new(w, h, handle))).cast()
}

/// # Safety
/// `a` is from `ohpl_h5_atlas_new`, freed once, or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_atlas_free(a: *mut c_void) {
    if !a.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(a.cast::<Atlas>()) });
    }
}

/// # Safety
/// `a` is a live atlas.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_atlas_pic_count(a: *const c_void) -> i32 {
    // SAFETY: caller contract.
    unsafe { a.cast::<Atlas>().as_ref() }.map_or(0, |a| a.pic_count)
}

/// # Safety
/// `a` is a live atlas.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_atlas_additions(a: *const c_void) -> i32 {
    // SAFETY: caller contract.
    unsafe { a.cast::<Atlas>().as_ref() }.map_or(0, |a| a.additions)
}

/// # Safety
/// `a` is a live atlas.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_atlas_handle(a: *const c_void) -> i32 {
    // SAFETY: caller contract.
    unsafe { a.cast::<Atlas>().as_ref() }.map_or(-1, |a| a.handle)
}

/// Returns whether the atlas was updated and clears the flag.
///
/// # Safety
/// `a` is a live atlas.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_atlas_take_updated(a: *mut c_void) -> bool {
    // SAFETY: caller contract.
    unsafe { a.cast::<Atlas>().as_mut() }.is_some_and(|a| std::mem::take(&mut a.updated))
}

/// Returns whether a reorganisation was requested and clears the flag.
///
/// # Safety
/// `a` is a live atlas.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_atlas_take_need_reorg(a: *mut c_void) -> bool {
    // SAFETY: caller contract.
    unsafe { a.cast::<Atlas>().as_mut() }.is_some_and(|a| std::mem::take(&mut a.need_reorg))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h5_atlas_minimum_fit(src: *const Rect2l, dest: *const Rect2l) -> bool {
    // SAFETY: caller passes two valid rects.
    match unsafe { (src.as_ref(), dest.as_ref()) } {
        (Some(s), Some(d)) => packer::minimum_fit(s, d),
        _ => false,
    }
}

/// Places `src` with its border blits; `linked` marks a subimage that already exists.
/// Returns whether it fit.
///
/// # Safety
/// `a` is live; the surfaces and buffers are valid for the call; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_atlas_add(
    a: *mut c_void,
    dst: *const OhplH5Surface,
    dst_data: *mut u8,
    dst_len: usize,
    src: *const OhplH5Surface,
    src_data: *const u8,
    src_len: usize,
    linked: bool,
    out: *mut OhplH5Added,
) -> bool {
    // SAFETY: caller contract.
    let (Some(a), Some(mut d), Some(s), Some(out)) = (unsafe {
        (
            a.cast::<Atlas>().as_mut(),
            pix(dst, dst_data, dst_len),
            src.as_ref(),
            out.as_mut(),
        )
    }) else {
        return false;
    };
    let sdata = if src_data.is_null() {
        None
    } else {
        // SAFETY: caller contract.
        Some(unsafe { std::slice::from_raw_parts(src_data, src_len) })
    };
    let r = a.add(&mut d, &surface(s), sdata, linked);
    match r {
        Some(r) => {
            out.rect = r.rect;
            out.found_empty = r.found_empty;
            true
        }
        None => false,
    }
}

/// Re-packs the `n` live image rects (inner, no border). `out_rects`/`out_fit` are indexed like
/// `rects`; `out_order` receives the placed indices in list order and `out_count` their number.
///
/// # Safety
/// `a` is live; `rects` has `n` rects; the outputs have room for `n` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_atlas_reorganize(
    a: *mut c_void,
    dst: *const OhplH5Surface,
    dst_data: *mut u8,
    dst_len: usize,
    rects: *const Rect2l,
    n: usize,
    out_rects: *mut Rect2l,
    out_fit: *mut bool,
    out_order: *mut usize,
    out_count: *mut usize,
) {
    // SAFETY: caller contract.
    let (Some(a), Some(mut d)) = (unsafe { a.cast::<Atlas>().as_mut() }, unsafe {
        pix(dst, dst_data, dst_len)
    }) else {
        return;
    };
    // SAFETY: caller contract.
    let input = unsafe { units(rects, n) };
    let r = a.reorganize(&mut d, input);
    // SAFETY: caller contract: outputs have room for `n` entries.
    unsafe {
        for (i, v) in r.rects.iter().enumerate() {
            *out_rects.add(i) = *v;
        }
        for (i, v) in r.fit.iter().enumerate() {
            *out_fit.add(i) = *v;
        }
        for (i, v) in r.order.iter().enumerate() {
            *out_order.add(i) = *v;
        }
        *out_count = r.order.len();
    }
}

/// # Safety
/// `name` has `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_combo_new(name: *const u8, n: usize, modes: usize) -> *mut c_void {
    // SAFETY: caller contract.
    let name = unsafe { units(name, n) };
    Box::into_raw(Box::new(ComboSettings::new(name, modes))).cast()
}

/// # Safety
/// `c` is from `ohpl_h5_combo_new`, freed once, or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_combo_free(c: *mut c_void) {
    if !c.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(c.cast::<ComboSettings>()) });
    }
}

/// # Safety
/// `c` is live; `name` has `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_combo_set_name(c: *mut c_void, name: *const u8, n: usize) {
    // SAFETY: caller contract.
    if let Some(c) = unsafe { c.cast::<ComboSettings>().as_mut() } {
        c.name = unsafe { units(name, n) }.to_vec();
    }
}

/// Mode names and shader file names, each as (pointer, length).
///
/// # Safety
/// `c` is live; every pointer has its length in bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_combo_setup_mode(
    c: *mut c_void,
    mode: i32,
    name: *const u8,
    name_n: usize,
    vtx: *const u8,
    vtx_n: usize,
    frag: *const u8,
    frag_n: usize,
) {
    // SAFETY: caller contract.
    let Some(c) = (unsafe { c.cast::<ComboSettings>().as_mut() }) else {
        return;
    };
    let Some(m) = c.mode_mut(mode) else { return };
    // SAFETY: caller contract.
    unsafe {
        m.name = units(name, name_n).to_vec();
        m.vtx_shader = units(vtx, vtx_n).to_vec();
        m.frag_shader = units(frag, frag_n).to_vec();
    }
    m.features.clear();
    m.defaults.clear();
}

/// Appends a feature (shader mask, variable name) to a mode.
///
/// # Safety
/// `c` is live; `var` has `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_combo_add_feature(
    c: *mut c_void,
    mode: i32,
    shaders: u32,
    var: *const u8,
    n: usize,
) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { c.cast::<ComboSettings>().as_mut() }.and_then(|c| c.mode_mut(mode)) {
        m.features
            .push((shaders, unsafe { units(var, n) }.to_vec()));
    }
}

/// Appends a default variable to a mode.
///
/// # Safety
/// `c` is live; the pointers have their lengths in bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_combo_add_default(
    c: *mut c_void,
    mode: i32,
    name: *const u8,
    name_n: usize,
    value: *const u8,
    value_n: usize,
) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { c.cast::<ComboSettings>().as_mut() }.and_then(|c| c.mode_mut(mode)) {
        m.defaults.push((
            unsafe { units(name, name_n) }.to_vec(),
            unsafe { units(value, value_n) }.to_vec(),
        ));
    }
}

/// # Safety
/// `c` is live; `sink` accepts `ctx` and bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_combo_program_name(
    c: *const c_void,
    mode: i32,
    flags: i32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    if let Some(c) = unsafe { c.cast::<ComboSettings>().as_ref() } {
        unsafe { emit(sink, ctx, &c.program_name(mode, flags)) };
    }
}

/// Shader file name of a mode; `shader_type` is `kPC_VertexBit` or `kPC_FragmentBit`.
///
/// # Safety
/// `c` is live; `sink` accepts `ctx` and bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_combo_shader_file(
    c: *const c_void,
    mode: i32,
    shader_type: u32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { c.cast::<ComboSettings>().as_ref() }.and_then(|c| c.mode(mode)) {
        let f = if shader_type == super::combo::VERTEX_BIT {
            &m.vtx_shader
        } else {
            &m.frag_shader
        };
        unsafe { emit(sink, ctx, f) };
    }
}

pub type VarSink = unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize);

/// Calls `sink(ctx, name, name_n, value, value_n)` per shader variable in `Add` order.
///
/// # Safety
/// `c` is live; `sink` accepts the arguments.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5_combo_shader_vars(
    c: *const c_void,
    mode: i32,
    flags: i32,
    shader_type: u32,
    sink: VarSink,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    if let Some(c) = unsafe { c.cast::<ComboSettings>().as_ref() } {
        for (k, v) in c.shader_vars(mode, flags, shader_type) {
            unsafe { sink(ctx, k.as_ptr(), k.len(), v.as_ptr(), v.len()) };
        }
    }
}

const _: () = {
    assert!(std::mem::size_of::<OhplH5Surface>() == 20);
    assert!(std::mem::size_of::<OhplH5Added>() == 20);
    assert!(std::mem::size_of::<Vector2f>() == 8);
};
