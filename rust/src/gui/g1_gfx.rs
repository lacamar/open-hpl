//! `cGuiGfxElement` and `cGuiGfxAnimation`, owned here. C++ holds only the pointer (opaque class,
//! inline forwarders in `gui/GuiGfxElement.h`); textures, images and the resource managers stay
//! C++ objects reached through [`Engine`].

use std::ffi::c_void;
use std::ptr;
use std::sync::atomic::{AtomicPtr, Ordering};

use super::draw::GfxInfo;
use super::gfx::{self, AnimInput, AnimState, FlipSource, Tex};
use super::list::GfxVertex;
use crate::types::{Color, Vector2f, Vector3f};

pub const MAX_TEXTURES: usize = 4;

#[repr(C)]
#[derive(Default)]
pub struct TexInfo {
    pub width: f32,
    pub height: f32,
    pub rect: bool,
}

#[repr(C)]
#[derive(Default)]
pub struct ImageInfo {
    pub texture: usize,
    pub tex: [[f32; 3]; 4],
    pub width: i32,
    pub height: i32,
    pub update_count: i32,
}

/// Engine objects the element reaches: `iTexture`, `cFrameSubImage`, the image and texture managers.
#[repr(C)]
pub struct Engine {
    pub tex_info: unsafe extern "C" fn(usize, *mut TexInfo),
    pub image_info: unsafe extern "C" fn(usize, *mut ImageInfo),
    pub image_flush: unsafe extern "C" fn(usize),
    pub image_destroy: unsafe extern "C" fn(*mut c_void, usize),
    pub texture_destroy: unsafe extern "C" fn(*mut c_void, usize),
}

static ENGINE: AtomicPtr<Engine> = AtomicPtr::new(ptr::null_mut());

/// # Safety
/// `engine` is a static table that outlives every call into this module.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_engine(engine: *mut Engine) {
    ENGINE.store(engine, Ordering::Release);
}

fn engine() -> Option<&'static Engine> {
    // SAFETY: `ohpl_g1_set_engine` contract.
    unsafe { ENGINE.load(Ordering::Acquire).as_ref() }
}

pub struct Anim {
    frame_length: f32,
    ty: i32,
}

impl Anim {
    pub fn set_frame_length(&mut self, len: f32) {
        self.frame_length = len;
    }

    pub fn set_type(&mut self, ty: i32) {
        self.ty = ty;
    }
}

pub struct Element {
    pub vtx: [GfxVertex; 4],
    image_size: Vector2f,
    offset: Vector3f,
    active_size: Vector2f,
    /// Read by the draw list at render time, so it lives in the pinned element.
    pub material: *const c_void,
    pub textures: [*const c_void; MAX_TEXTURES],
    images: [usize; MAX_TEXTURES],
    destroy_texture: bool,
    image_buffer: Vec<usize>,
    #[allow(clippy::vec_box)] // C++ keeps the animation pointers
    animations: Vec<Box<Anim>>,
    current_animation: i32,
    state: AnimState,
    paused: bool,
    flip: bool,
    update_count: i32,
    texture_num: usize,
    resources: *mut c_void,
}

fn vertex(x: f32, y: f32) -> GfxVertex {
    let z = Vector3f::default();
    GfxVertex {
        pos: Vector3f { x, y, z: 0.0 },
        tex: Vector3f { x, y, z: 0.0 },
        tan: z,
        norm: z,
        col: Color {
            r: 1.0,
            g: 1.0,
            b: 1.0,
            a: 1.0,
        },
    }
}

fn v3(t: [f32; 3]) -> Vector3f {
    Vector3f {
        x: t[0],
        y: t[1],
        z: t[2],
    }
}

impl Element {
    fn new(resources: *mut c_void) -> Self {
        Self {
            vtx: [
                vertex(0.0, 0.0),
                vertex(1.0, 0.0),
                vertex(1.0, 1.0),
                vertex(0.0, 1.0),
            ],
            image_size: Vector2f::default(),
            offset: Vector3f::default(),
            active_size: Vector2f::default(),
            material: ptr::null(),
            textures: [ptr::null(); MAX_TEXTURES],
            images: [0; MAX_TEXTURES],
            destroy_texture: true,
            image_buffer: Vec::new(),
            animations: Vec::new(),
            current_animation: 0,
            state: AnimState::default(),
            paused: false,
            flip: false,
            update_count: -1,
            texture_num: 0,
            resources,
        }
    }

    pub fn set_offset(&mut self, v: Vector3f) {
        self.offset = v;
    }

    pub fn set_active_size(&mut self, v: Vector2f) {
        self.active_size = v;
    }

    pub fn animation_count(&self) -> usize {
        self.animations.len()
    }

    pub fn animation(&mut self, i: usize) -> *mut Anim {
        self.animations
            .get_mut(i)
            .map_or(ptr::null_mut(), |a| &raw mut **a)
    }

    pub fn offset(&self) -> Vector3f {
        self.offset
    }

    pub fn image_size(&self) -> Vector2f {
        self.image_size
    }

    pub fn update(&mut self, dt: f32) {
        let anim = usize::try_from(self.current_animation)
            .ok()
            .and_then(|i| self.animations.get(i));
        let input = AnimInput {
            animation_count: self.animations.len(),
            current_animation: self.current_animation,
            paused: self.paused,
            ty: anim.map_or(0, |a| a.ty),
            frame_length: anim.map_or(0.0, |a| a.frame_length),
            buffer_len: self.image_buffer.len(),
            time_step: dt,
        };
        if let Some(i) = gfx::update(&mut self.state, &input)
            && let Some(&image) = self.image_buffer.get(i)
        {
            self.set_image(image, 0);
        }
    }

    fn set_image(&mut self, image: usize, num: usize) {
        let Some(eng) = engine() else { return };
        if image == 0 {
            return;
        }
        let mut info = ImageInfo::default();
        // SAFETY: `image` is a live cFrameSubImage owned by the image manager.
        unsafe { (eng.image_info)(image, &mut info) };
        if let Some(slot) = self.images.get_mut(num) {
            *slot = image;
        }
        if let Some(slot) = self.textures.get_mut(num) {
            *slot = info.texture as *const c_void;
        }
        for (v, t) in self.vtx.iter_mut().zip(info.tex) {
            v.tex = v3(t);
        }
        if num == 0 {
            self.image_size = Vector2f {
                x: info.width as f32,
                y: info.height as f32,
            };
            if self.flip {
                self.set_flip(true);
            }
        }
    }

    pub fn add_image(&mut self, image: usize) {
        self.set_image(image, self.texture_num);
        self.active_size = self.image_size;
        self.texture_num += 1;
    }

    pub fn add_texture(&mut self, texture: usize, start: [f32; 2], end: [f32; 2]) {
        let Some(eng) = engine() else { return };
        if let Some(slot) = self.textures.get_mut(self.texture_num) {
            *slot = texture as *const c_void;
        }
        if self.texture_num == 0 {
            let mut info = TexInfo::default();
            // SAFETY: `texture` is a live iTexture.
            unsafe { (eng.tex_info)(texture, &mut info) };
            let mut tex: Tex = [[0.0; 2]; 4];
            let size = gfx::texture_uv(&mut tex, [info.width, info.height], info.rect, start, end);
            self.image_size = Vector2f {
                x: size[0],
                y: size[1],
            };
            for (v, t) in self.vtx.iter_mut().zip(tex) {
                v.tex.x = t[0];
                v.tex.y = t[1];
            }
            if self.flip {
                self.set_flip(true);
            }
        }
        self.active_size = self.image_size;
        self.texture_num += 1;
    }

    pub fn add_image_to_buffer(&mut self, image: usize) {
        if self.image_buffer.is_empty() {
            self.set_image(image, 0);
        }
        self.image_buffer.push(image);
    }

    pub fn play_animation(&mut self, num: i32) {
        if self.current_animation == num {
            return;
        }
        self.current_animation = num;
        gfx::restart(&mut self.state);
        if let Some(&image) = usize::try_from(self.state.active_image)
            .ok()
            .and_then(|i| self.image_buffer.get(i))
        {
            self.set_image(image, 0);
        }
    }

    pub fn set_animation_time(&mut self, time: f32) {
        let anim = usize::try_from(self.current_animation)
            .ok()
            .and_then(|i| self.animations.get(i));
        if self.current_animation >= 0 && anim.is_none() {
            return;
        }
        self.state.current_frame = gfx::animation_time(time, anim.map(|a| a.frame_length));
    }

    pub fn set_color(&mut self, c: Color) {
        for v in &mut self.vtx {
            v.col = c;
        }
    }

    pub fn set_flip(&mut self, flip: bool) {
        self.flip = flip;
        let mut tex: Tex = [[0.0; 2]; 4];
        for (t, v) in tex.iter_mut().zip(&self.vtx) {
            *t = [v.tex.x, v.tex.y];
        }
        let first = self.images.first().copied().unwrap_or(0);
        let mut image_tex: Tex = [[0.0; 2]; 4];
        let source = if let (true, Some(eng)) = (first != 0, engine()) {
            let mut info = ImageInfo::default();
            // SAFETY: a stored image is live while the element holds it.
            unsafe { (eng.image_info)(first, &mut info) };
            for (t, v) in image_tex.iter_mut().zip(info.tex) {
                *t = [v[0], v[1]];
            }
            FlipSource::Image(&image_tex)
        } else if self.textures.first().is_some_and(|t| !t.is_null()) {
            FlipSource::Texture
        } else {
            FlipSource::Nothing
        };
        gfx::set_flip_uv_y(&mut tex, flip, &source);
        for (v, t) in self.vtx.iter_mut().zip(tex) {
            v.tex.x = t[0];
            v.tex.y = t[1];
        }
    }

    pub fn flush(&mut self) {
        let main = self.images.first().copied().unwrap_or(0);
        if main == 0 {
            return;
        }
        let Some(eng) = engine() else { return };
        for &image in self.images.iter().take(self.texture_num) {
            if image != 0 {
                // SAFETY: stored images are live.
                unsafe { (eng.image_flush)(image) };
            }
        }
        let mut info = ImageInfo::default();
        // SAFETY: as above.
        unsafe { (eng.image_info)(main, &mut info) };
        if gfx::flush_changed(&mut self.update_count, info.update_count) {
            for (v, t) in self.vtx.iter_mut().zip(info.tex) {
                v.tex = v3(t);
            }
            if self.flip {
                self.set_flip(true);
            }
        }
    }
}

impl Drop for Element {
    fn drop(&mut self) {
        let Some(eng) = engine() else { return };
        if !self.image_buffer.is_empty() {
            for &image in &self.image_buffer {
                // SAFETY: the image manager owns it until this call.
                unsafe { (eng.image_destroy)(self.resources, image) };
            }
            return;
        }
        for i in 0..self.texture_num {
            let image = self.images.get(i).copied().unwrap_or(0);
            let tex = self.textures.get(i).copied().unwrap_or(ptr::null());
            if image != 0 {
                // SAFETY: as above.
                unsafe { (eng.image_destroy)(self.resources, image) };
            } else if !tex.is_null() && self.destroy_texture {
                // SAFETY: as above.
                unsafe { (eng.texture_destroy)(self.resources, tex as usize) };
            }
        }
    }
}

/// # Safety
/// `e` is null or from `ohpl_g1_gfx_new` and live.
unsafe fn el<'a>(e: *mut Element) -> Option<&'a mut Element> {
    // SAFETY: caller contract.
    unsafe { e.as_mut() }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_g1_gfx_new(resources: *mut c_void) -> *mut Element {
    Box::into_raw(Box::new(Element::new(resources)))
}

/// # Safety
/// `e` is from `ohpl_g1_gfx_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_free(e: *mut Element) {
    if !e.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(e) });
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_update(e: *mut Element, dt: f32) {
    if let Some(e) = unsafe { el(e) } {
        e.update(dt);
    }
}

/// # Safety
/// `e` is live; `image` is a live cFrameSubImage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_add_image(e: *mut Element, image: usize) {
    if let Some(e) = unsafe { el(e) } {
        e.add_image(image);
    }
}

/// # Safety
/// `e` is live; `texture` is a live iTexture; `start` and `end` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_add_texture(
    e: *mut Element,
    texture: usize,
    start: *const Vector2f,
    end: *const Vector2f,
) {
    // SAFETY: caller contract.
    if let (Some(e), Some(s), Some(en)) = unsafe { (el(e), start.as_ref(), end.as_ref()) } {
        e.add_texture(texture, [s.x, s.y], [en.x, en.y]);
    }
}

/// # Safety
/// `e` is live; `image` is a live cFrameSubImage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_add_image_to_buffer(e: *mut Element, image: usize) {
    if let Some(e) = unsafe { el(e) } {
        e.add_image_to_buffer(image);
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_texture_num(e: *mut Element) -> i32 {
    unsafe { el(e) }.map_or(0, |e| i32::try_from(e.texture_num).unwrap_or(0))
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_set_destroy_texture(e: *mut Element, v: bool) {
    if let Some(e) = unsafe { el(e) } {
        e.destroy_texture = v;
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_destroy_texture(e: *mut Element) -> bool {
    unsafe { el(e) }.is_some_and(|e| e.destroy_texture)
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_texture(e: *mut Element, i: i32) -> *const c_void {
    unsafe { el(e) }
        .and_then(|e| e.textures.get(usize::try_from(i).ok()?).copied())
        .unwrap_or(ptr::null())
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_image(e: *mut Element, i: i32) -> usize {
    unsafe { el(e) }
        .and_then(|e| e.images.get(usize::try_from(i).ok()?).copied())
        .unwrap_or(0)
}

/// # Safety
/// `e` is live; `v` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_set_offset(e: *mut Element, v: *const Vector3f) {
    // SAFETY: caller contract.
    if let (Some(e), Some(v)) = unsafe { (el(e), v.as_ref()) } {
        e.offset = *v;
    }
}

/// Pinned for the element's life.
///
/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_offset(e: *mut Element) -> *const Vector3f {
    unsafe { el(e) }.map_or(ptr::null(), |e| &raw const e.offset)
}

/// # Safety
/// `e` is live; `v` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_set_active_size(e: *mut Element, v: *const Vector2f) {
    // SAFETY: caller contract.
    if let (Some(e), Some(v)) = unsafe { (el(e), v.as_ref()) } {
        e.active_size = *v;
    }
}

/// Pinned for the element's life.
///
/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_active_size(e: *mut Element) -> *const Vector2f {
    unsafe { el(e) }.map_or(ptr::null(), |e| &raw const e.active_size)
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_image_size(e: *mut Element, out: *mut Vector2f) {
    // SAFETY: caller contract.
    if let (Some(e), Some(out)) = unsafe { (el(e), out.as_mut()) } {
        *out = e.image_size;
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_create_animation(e: *mut Element) -> *mut Anim {
    let Some(e) = (unsafe { el(e) }) else {
        return ptr::null_mut();
    };
    let mut a = Box::new(Anim {
        frame_length: 1.0,
        ty: gfx::ANIM_LOOP,
    });
    let p = &raw mut *a;
    e.animations.push(a);
    p
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_play_animation(e: *mut Element, n: i32) {
    if let Some(e) = unsafe { el(e) } {
        e.play_animation(n);
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_animation(e: *mut Element, i: i32) -> *mut Anim {
    unsafe { el(e) }
        .and_then(|e| {
            let a = e.animations.get_mut(usize::try_from(i).ok()?)?;
            Some(&raw mut **a)
        })
        .unwrap_or(ptr::null_mut())
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_animation_num(e: *mut Element) -> i32 {
    unsafe { el(e) }.map_or(0, |e| i32::try_from(e.animations.len()).unwrap_or(0))
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_set_animation_time(e: *mut Element, t: f32) {
    if let Some(e) = unsafe { el(e) } {
        e.set_animation_time(t);
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_set_paused(e: *mut Element, v: bool) {
    if let Some(e) = unsafe { el(e) } {
        e.paused = v;
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_paused(e: *mut Element) -> bool {
    unsafe { el(e) }.is_some_and(|e| e.paused)
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_set_material(e: *mut Element, m: *const c_void) {
    if let Some(e) = unsafe { el(e) } {
        e.material = m;
    }
}

/// # Safety
/// `e` is live; `c` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_set_color(e: *mut Element, c: *const Color) {
    // SAFETY: caller contract.
    if let (Some(e), Some(c)) = unsafe { (el(e), c.as_ref()) } {
        e.set_color(*c);
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_set_flip(e: *mut Element, v: bool) {
    if let Some(e) = unsafe { el(e) } {
        e.set_flip(v);
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_flip(e: *mut Element) -> bool {
    unsafe { el(e) }.is_some_and(|e| e.flip)
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_flush(e: *mut Element) {
    if let Some(e) = unsafe { el(e) } {
        e.flush();
    }
}

/// The original stores frame numbers but never reads them.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_g1_anim_add_frame(_a: *mut Anim, _n: i32) {}

/// # Safety
/// `a` is a live animation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_anim_set_type(a: *mut Anim, ty: i32) {
    // SAFETY: caller contract.
    if let Some(a) = unsafe { a.as_mut() } {
        a.ty = ty;
    }
}

/// # Safety
/// `a` is a live animation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_anim_set_frame_length(a: *mut Anim, len: f32) {
    // SAFETY: caller contract.
    if let Some(a) = unsafe { a.as_mut() } {
        a.frame_length = len;
    }
}

/// What the draw list reads from an element; the pointers stay valid for its life.
///
/// # Safety
/// `e` is live; `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gfx_info(e: *mut Element, out: *mut GfxInfo) {
    // SAFETY: caller contract.
    if let (Some(e), Some(out)) = unsafe { (el(e), out.as_mut()) } {
        *out = GfxInfo {
            offset: e.offset,
            image_size: e.image_size,
            material: &raw const e.material,
            texture0: e.textures.as_ptr(),
            vtx: e.vtx.as_ptr(),
        };
    }
}
