//! `cGuiSkin` and `cGuiSkinFont`, owned here; C++ holds only the skin pointer (opaque class) and
//! reads `cGuiSkinFont` fields directly (shared `repr(C)` layout).

use std::ffi::c_void;
use std::ptr;

use super::g1_gfx::{Anim, Element};
use super::g1_gui::{self, Gui};
use super::g1_host::{self, ERROR, WARNING};
use super::guicore::SkinKind;
use super::skin;
use crate::string;
use crate::types::{Color, Vector2f, Vector3f};

/// Layout shared with `cGuiSkinFont` in `gui/GuiSkin.h`.
#[repr(C)]
pub struct SkinFont {
    pub font: usize,
    pub size: Vector2f,
    pub color: Color,
    pub material: usize,
    gui: *mut c_void,
}

pub struct Skin {
    name: Vec<u8>,
    gui: *mut Gui,
    gfx: Vec<*mut Element>,
    fonts: Vec<Option<Box<SkinFont>>>,
    attributes: Vec<Vector3f>,
}

fn text(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

impl Skin {
    pub fn new(name: &[u8], gui: *mut Gui) -> Box<Skin> {
        let none = |k: SkinKind| usize::try_from(k.from_name(b"")).unwrap_or(0);
        Box::new(Skin {
            name: name.to_vec(),
            gui,
            gfx: vec![ptr::null_mut(); none(SkinKind::Gfx)],
            fonts: (0..none(SkinKind::Font)).map(|_| None).collect(),
            attributes: vec![Vector3f::default(); none(SkinKind::Attribute)],
        })
    }
}

impl Drop for Skin {
    fn drop(&mut self) {
        for &g in &self.gfx {
            if !g.is_null() {
                g1_gui::destroy_gfx(self.gui, g);
            }
        }
    }
}

fn read_all(path: &[u32]) -> Option<(Vec<u8>, bool)> {
    let h = g1_host::host()?;
    let mut data: Vec<u8> = Vec::new();
    unsafe extern "C" fn sink(ctx: *mut c_void, p: *const u8, n: usize) {
        // SAFETY: `ctx` is the Vec below; `p` holds `n` bytes.
        if let Some(v) = unsafe { ctx.cast::<Vec<u8>>().as_mut() } {
            v.extend_from_slice(unsafe { std::slice::from_raw_parts(p, n) });
        }
    }
    // SAFETY: host contract.
    let r = unsafe {
        (h.read_file)(
            path.as_ptr(),
            path.len(),
            sink,
            ptr::from_mut(&mut data).cast(),
        )
    };
    (r != 0).then_some((data, r == 1))
}

/// `cGuiSkin::LoadFromFile`. A skin file with a missing section dereferences null in C++; here it
/// fails like an XML error.
fn load(skin: *mut Skin, file: &[u32]) -> bool {
    // SAFETY: `skin` is live and only this thread touches it.
    let Some(s) = (unsafe { skin.as_mut() }) else {
        return false;
    };
    let shown = text(&string::wide_to_utf8(file));
    let Some((data, read_ok)) = read_all(file) else {
        g1_host::log(ERROR, &format!("Couldn't load skin file '{shown}'!\n"));
        return false;
    };
    let Ok(parsed) = skin::parse(&data, read_ok) else {
        g1_host::log(ERROR, &format!("Couldn't load skin file '{shown}'!\n"));
        return false;
    };
    let name = text(&s.name);
    let gui = s.gui;

    let mut loaded = vec![false; s.attributes.len()];
    for a in &parsed.attributes {
        let Some(t) = g1_gui::skin_index(gui, SkinKind::Attribute, &a.type_name) else {
            continue;
        };
        if let Some(v) = s.attributes.get_mut(t) {
            *v = Vector3f {
                x: a.value[0],
                y: a.value[1],
                z: a.value[2],
            };
        }
        if let Some(l) = loaded.get_mut(t) {
            *l = true;
        }
    }
    for (i, l) in loaded.iter().enumerate() {
        if !l {
            let n = SkinKind::Attribute.name(u32::try_from(i).unwrap_or(u32::MAX));
            g1_host::log(
                ERROR,
                &format!("Could not find gui skin '{name}' attribute {n}!\n"),
            );
        }
    }

    for f in &parsed.fonts {
        let Some(t) = g1_gui::skin_index(gui, SkinKind::Font, &f.type_name) else {
            continue;
        };
        let res = g1_gui::resources(gui);
        let font = g1_host::host().map_or(0, |h| {
            // SAFETY: host contract.
            unsafe { (h.font_create)(res, f.file.as_ptr(), f.file.len()) }
        });
        if let Some(slot) = s.fonts.get_mut(t) {
            *slot = Some(Box::new(SkinFont {
                font,
                size: Vector2f {
                    x: f.size[0],
                    y: f.size[1],
                },
                color: Color {
                    r: f.color[0],
                    g: f.color[1],
                    b: f.color[2],
                    a: f.color[3],
                },
                material: 0,
                gui: g1_gui::shell(gui),
            }));
        }
    }
    for (i, f) in s.fonts.iter().enumerate() {
        if f.is_none() {
            let n = SkinKind::Font.name(u32::try_from(i).unwrap_or(u32::MAX));
            g1_host::log(
                ERROR,
                &format!("Could not find gui skin '{name}' font {n}!\n"),
            );
        }
    }

    for g in &parsed.gfx {
        let Some(t) = g1_gui::skin_index(gui, SkinKind::Gfx, &g.type_name) else {
            continue;
        };
        let color = Color {
            r: g.color[0],
            g: g.color[1],
            b: g.color[2],
            a: g.color[3],
        };
        let material = i32::try_from(g.material).unwrap_or(0);
        if let Some(w) = &g.material_warn {
            g1_host::log(
                WARNING,
                &format!("Material type '{}' does not exist!\n", text(w)),
            );
        }
        if let Some(w) = &g.anim_warn {
            g1_host::log(
                WARNING,
                &format!("Animation type '{}' does not exist!\n", text(w)),
            );
        }
        let e = if g.file.is_empty() {
            g1_gui::create_gfx_filled_rect(gui, color, material, true)
        } else if g.animated {
            g1_gui::create_gfx_image_buffer(gui, &g.file, material, true, color, true)
        } else {
            g1_gui::create_gfx_image(gui, &g.file, material, color, true)
        };
        if e.is_null() {
            continue;
        }
        // SAFETY: `e` was just created and is live.
        if let Some(el) = unsafe { e.as_mut() } {
            if g.animated && el.animation_count() > 0 {
                let a: *mut Anim = el.animation(0);
                // SAFETY: owned by `el`.
                if let Some(a) = unsafe { a.as_mut() } {
                    a.set_frame_length(g.frame_time);
                    a.set_type(g.anim_mode);
                }
            }
            if g.has_active_size() {
                el.set_active_size(Vector2f {
                    x: g.active_size[0],
                    y: g.active_size[1],
                });
            }
            el.set_offset(Vector3f {
                x: g.offset[0],
                y: g.offset[1],
                z: 0.0,
            });
        }
        if let Some(slot) = s.gfx.get_mut(t) {
            *slot = e;
        }
    }
    for (i, g) in s.gfx.iter().enumerate() {
        if g.is_null() {
            let n = SkinKind::Gfx.name(u32::try_from(i).unwrap_or(u32::MAX));
            g1_host::log(
                ERROR,
                &format!("Could not find gui skin '{name}' gfx {n}!\n"),
            );
        }
    }
    true
}

pub(crate) fn create(gui: *mut Gui, name: &[u8], file: &[u32]) -> *mut Skin {
    let skin = Box::into_raw(Skin::new(name, gui));
    if load(skin, file) {
        skin
    } else {
        // SAFETY: just created here.
        drop(unsafe { Box::from_raw(skin) });
        ptr::null_mut()
    }
}

/// # Safety
/// `s` is a live skin.
unsafe fn sk<'a>(s: *mut Skin) -> Option<&'a mut Skin> {
    // SAFETY: caller contract.
    unsafe { s.as_mut() }
}

/// # Safety
/// `s` is a live skin.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_skin_name(s: *mut Skin, len: *mut usize) -> *const u8 {
    // SAFETY: caller contract.
    match (unsafe { sk(s) }, unsafe { len.as_mut() }) {
        (Some(s), Some(len)) => {
            *len = s.name.len();
            s.name.as_ptr()
        }
        _ => ptr::null(),
    }
}

/// # Safety
/// `s` is a live skin.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_skin_gfx(s: *mut Skin, ty: i32) -> *mut Element {
    unsafe { sk(s) }
        .and_then(|s| s.gfx.get(usize::try_from(ty).ok()?).copied())
        .unwrap_or(ptr::null_mut())
}

/// # Safety
/// `s` is a live skin.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_skin_font(s: *mut Skin, ty: i32) -> *mut SkinFont {
    unsafe { sk(s) }
        .and_then(|s| {
            let f = s.fonts.get_mut(usize::try_from(ty).ok()?)?.as_mut()?;
            Some(&raw mut **f)
        })
        .unwrap_or(ptr::null_mut())
}

/// Pinned for the skin's life.
///
/// # Safety
/// `s` is a live skin.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_skin_attribute(s: *mut Skin, ty: i32) -> *const Vector3f {
    unsafe { sk(s) }
        .and_then(|s| {
            s.attributes
                .get(usize::try_from(ty).ok()?)
                .map(ptr::from_ref)
        })
        .unwrap_or(ptr::null())
}

/// # Safety
/// `s` is a live skin; `path` readable for `len` wide characters.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_skin_load(s: *mut Skin, path: *const u32, len: usize) -> bool {
    // SAFETY: caller contract.
    load(s, unsafe { std::slice::from_raw_parts(path, len) })
}
