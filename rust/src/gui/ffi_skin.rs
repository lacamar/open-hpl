//! C ABI for `cGui`, `cGuiSkin`, `cGuiGfxElement` and the GUI materials, declared in
//! `gui/RustGuiSkin.h`.

use std::ffi::{c_char, c_void};

use super::gfx::{self, AnimInput, AnimState, FlipSource, Tex};
use super::guicore::{self, SkinKind};
use super::skin::{self, Skin, SkinError};
use crate::string_ffi::units;

fn kind(k: i32) -> Option<SkinKind> {
    match k {
        0 => Some(SkinKind::Font),
        1 => Some(SkinKind::Attribute),
        2 => Some(SkinKind::Gfx),
        _ => None,
    }
}

/// `cGui::GetSkin{Font,Attribute,Gfx}FromString`: the table length (`LastEnum`) when unknown.
///
/// # Safety
/// `name` is readable for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_skin_from_name(k: i32, name: *const u8, len: usize) -> u32 {
    let Some(kind) = kind(k) else {
        return 0;
    };
    // SAFETY: caller contract.
    kind.from_name(unsafe { units(name, len) })
}

/// `cGui::GetSkin{Font,Attribute,Gfx}String`, delivered through `sink`.
///
/// # Safety
/// `sink` accepts `ctx` and a slice.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_skin_name(
    k: i32,
    index: u32,
    sink: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    ctx: *mut c_void,
) {
    let name = kind(k).map_or("Unknown", |kind| kind.name(index));
    // SAFETY: caller contract.
    unsafe { sink(ctx, name.as_ptr(), name.len()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gui_material_slot(ty: u32) -> u32 {
    guicore::material_slot(ty)
}

/// The blend factors of a GUI material; false for an unknown material.
///
/// # Safety
/// `src` and `dst` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_material_blend(ty: u32, src: *mut i32, dst: *mut i32) -> bool {
    let Some((s, d)) = guicore::material_blend(ty) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        *src = s;
        *dst = d;
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gui_mouse_to_mouse(button: i32) -> i32 {
    guicore::gui_mouse_to_mouse(button)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gui_mouse_to_gui(button: i32) -> i32 {
    guicore::mouse_to_gui_mouse(button)
}

#[repr(C)]
pub struct OhplGuiAnimInput {
    animation_count: usize,
    buffer_len: usize,
    current_animation: i32,
    ty: i32,
    frame_length: f32,
    time_step: f32,
    paused: i32,
}

/// `cGuiGfxElement::Update`: the buffered image to show, or -1.
///
/// # Safety
/// `state` and `input` are readable and, for `state`, writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_gfx_update(
    state: *mut AnimState,
    input: *const OhplGuiAnimInput,
) -> i64 {
    // SAFETY: caller contract.
    let (Some(state), Some(input)) = (unsafe { (state.as_mut(), input.as_ref()) }) else {
        return -1;
    };
    let input = AnimInput {
        animation_count: input.animation_count,
        current_animation: input.current_animation,
        paused: input.paused != 0,
        ty: input.ty,
        frame_length: input.frame_length,
        buffer_len: input.buffer_len,
        time_step: input.time_step,
    };
    gfx::update(state, &input).map_or(-1, |i| i64::try_from(i).unwrap_or(-1))
}

/// # Safety
/// `state` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_gfx_restart(state: *mut AnimState) {
    // SAFETY: caller contract.
    if let Some(state) = unsafe { state.as_mut() } {
        gfx::restart(state);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gui_gfx_animation_time(time: f32, has_length: bool, length: f32) -> f32 {
    gfx::animation_time(time, has_length.then_some(length))
}

/// # Safety
/// `tex` holds 8 writable floats (four UV pairs); `image_size` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_gfx_texture_uv(
    tex: *mut Tex,
    size_x: f32,
    size_y: f32,
    rect: bool,
    start_x: f32,
    start_y: f32,
    end_x: f32,
    end_y: f32,
    image_size: *mut [f32; 2],
) {
    // SAFETY: caller contract.
    let (Some(tex), Some(image_size)) = (unsafe { (tex.as_mut(), image_size.as_mut()) }) else {
        return;
    };
    *image_size = gfx::texture_uv(
        tex,
        [size_x, size_y],
        rect,
        [start_x, start_y],
        [end_x, end_y],
    );
}

/// `source`: 0 image (`image` readable), 1 texture, 2 neither.
///
/// # Safety
/// `tex` holds 8 writable floats; `image` holds 8 readable floats when `source` is 0.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_gfx_flip_uv(
    tex: *mut Tex,
    flip: bool,
    source: i32,
    image: *const Tex,
) {
    // SAFETY: caller contract.
    let Some(tex) = (unsafe { tex.as_mut() }) else {
        return;
    };
    // SAFETY: caller contract.
    let image = unsafe { image.as_ref() };
    let source = match (source, image) {
        (0, Some(image)) => FlipSource::Image(image),
        (1, _) => FlipSource::Texture,
        _ => FlipSource::Nothing,
    };
    gfx::set_flip_uv_y(tex, flip, &source);
}

/// # Safety
/// `last` is readable and writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_gfx_flush_changed(last: *mut i32, count: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { last.as_mut() }.is_some_and(|last| gfx::flush_changed(last, count))
}

#[repr(C)]
pub struct OhplSkinAttribute {
    type_name: *const c_char,
    value: [f32; 3],
}

#[repr(C)]
pub struct OhplSkinFont {
    type_name: *const c_char,
    file: *const c_char,
    size: [f32; 2],
    color: [f32; 4],
}

#[repr(C)]
pub struct OhplSkinGfx {
    type_name: *const c_char,
    file: *const c_char,
    /// Null unless the material name is unknown (lower-cased).
    material_warn: *const c_char,
    anim_warn: *const c_char,
    offset: [f32; 2],
    active_size: [f32; 2],
    color: [f32; 4],
    frame_time: f32,
    material: u32,
    anim_mode: i32,
    animated: i32,
    has_active_size: i32,
}

#[repr(C)]
pub struct OhplSkinView {
    attributes: *const OhplSkinAttribute,
    attribute_count: usize,
    fonts: *const OhplSkinFont,
    font_count: usize,
    gfx: *const OhplSkinGfx,
    gfx_count: usize,
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<OhplSkinAttribute>() == 24);
    assert!(size_of::<OhplSkinFont>() == 40);
    assert!(size_of::<OhplSkinGfx>() == 88);
    assert!(size_of::<OhplSkinView>() == 48);
    assert!(size_of::<OhplGuiAnimInput>() == 40);
    assert!(size_of::<AnimState>() == 12);
};

/// Parsed skin with the NUL-terminated strings the views point into.
pub struct SkinHandle {
    strings: Vec<Vec<u8>>,
    attributes: Vec<OhplSkinAttribute>,
    fonts: Vec<OhplSkinFont>,
    gfx: Vec<OhplSkinGfx>,
    view: OhplSkinView,
}

impl SkinHandle {
    /// Pointer to a NUL-terminated copy; the copy's heap buffer stays put as `strings` grows.
    fn keep(&mut self, s: &[u8]) -> *const c_char {
        let mut owned = Vec::with_capacity(s.len() + 1);
        owned.extend_from_slice(s);
        owned.push(0);
        let p = owned.as_ptr().cast();
        self.strings.push(owned);
        p
    }

    fn build(skin: &Skin) -> Box<Self> {
        let mut h = Box::new(Self {
            strings: Vec::new(),
            attributes: Vec::new(),
            fonts: Vec::new(),
            gfx: Vec::new(),
            view: OhplSkinView {
                attributes: std::ptr::null(),
                attribute_count: 0,
                fonts: std::ptr::null(),
                font_count: 0,
                gfx: std::ptr::null(),
                gfx_count: 0,
            },
        });
        for a in &skin.attributes {
            let type_name = h.keep(&a.type_name);
            h.attributes.push(OhplSkinAttribute {
                type_name,
                value: a.value,
            });
        }
        for f in &skin.fonts {
            let (type_name, file) = (h.keep(&f.type_name), h.keep(&f.file));
            h.fonts.push(OhplSkinFont {
                type_name,
                file,
                size: f.size,
                color: f.color,
            });
        }
        for g in &skin.gfx {
            let (type_name, file) = (h.keep(&g.type_name), h.keep(&g.file));
            let material_warn = g
                .material_warn
                .as_deref()
                .map_or(std::ptr::null(), |w| h.keep(w));
            let anim_warn = g
                .anim_warn
                .as_deref()
                .map_or(std::ptr::null(), |w| h.keep(w));
            h.gfx.push(OhplSkinGfx {
                type_name,
                file,
                material_warn,
                anim_warn,
                offset: g.offset,
                active_size: g.active_size,
                color: g.color,
                frame_time: g.frame_time,
                material: g.material,
                anim_mode: g.anim_mode,
                animated: i32::from(g.animated),
                has_active_size: i32::from(g.has_active_size()),
            });
        }
        h.view = OhplSkinView {
            attributes: h.attributes.as_ptr(),
            attribute_count: h.attributes.len(),
            fonts: h.fonts.as_ptr(),
            font_count: h.fonts.len(),
            gfx: h.gfx.as_ptr(),
            gfx_count: h.gfx.len(),
        };
        h
    }
}

/// `cGuiSkin::LoadFromFile` parsing. `status`: 0 ok, 1 XML error, 2 missing section (a null
/// dereference in C++). Null unless ok.
///
/// # Safety
/// `data` is readable for `len` bytes; `status` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_skin_parse(
    data: *const u8,
    len: usize,
    read_ok: bool,
    status: *mut i32,
) -> *mut SkinHandle {
    // SAFETY: caller contract.
    let result = skin::parse(unsafe { units(data, len) }, read_ok);
    let (code, handle) = match result {
        Ok(skin) => (0, Box::into_raw(SkinHandle::build(&skin))),
        Err(SkinError::Xml) => (1, std::ptr::null_mut()),
        Err(SkinError::MissingSection) => (2, std::ptr::null_mut()),
    };
    // SAFETY: caller contract.
    if let Some(status) = unsafe { status.as_mut() } {
        *status = code;
    }
    handle
}

/// The arrays live until `ohpl_gui_skin_free`.
///
/// # Safety
/// `handle` is live from `ohpl_gui_skin_parse`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_skin_view(handle: *const SkinHandle) -> *const OhplSkinView {
    // SAFETY: caller contract.
    unsafe { handle.as_ref() }.map_or(std::ptr::null(), |h| &raw const h.view)
}

/// # Safety
/// `handle` is from `ohpl_gui_skin_parse`, freed once, or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_skin_free(handle: *mut SkinHandle) {
    if !handle.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(handle) });
    }
}
