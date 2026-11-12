//! `cSDLTexture` GL calls. Each method replays the `gl*` sequence of the C++ member; the plans
//! come from `crate::gl::texture`. Fields the engine reads on `iTexture` travel in `Io`.

use core::ffi::c_void;

use super::{Store, caps_fbo};
use crate::gl::texture::{self as plan, CreateIn};
use crate::gl::{c, tables};
use crate::sys::gl as g;
use crate::sys::glu;
use crate::types::Vector3l;

const WRAP_CLAMP_TO_EDGE: i32 = 2;

pub(crate) static TEXS: Store<Tex> = Store::new(1);

/// `iTexture` members the GL code reads or writes, copied in and out around each call.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Io {
    pub tex_type: i32,
    pub usage: i32,
    pub filter: i32,
    pub wrap: [i32; 3],
    pub compare_mode: i32,
    pub compare_func: i32,
    pub anim_mode: i32,
    pub pixel_format: i32,
    pub down_level: u32,
    pub memory: i32,
    pub size: Vector3l,
    pub min_size: Vector3l,
    pub frame_time: f32,
    pub aniso: f32,
    pub use_mips: bool,
    pub srgb: bool,
    pub compressed: bool,
}

/// One `cBitmapData`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Level {
    pub data: *const u8,
    pub size: i32,
}

/// One image of a bitmap: its mip array and format.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Face {
    pub levels: *const Level,
    pub num_mips: i32,
    pub size: Vector3l,
    pub format: i32,
}

/// Outputs for the shell's log calls.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Res {
    pub warn_pow2: i32,
}

#[derive(Default)]
pub struct Tex {
    handles: Vec<u32>,
    contains_data: bool,
}

fn ptr_or_null(v: &[u8]) -> *const c_void {
    v.as_ptr().cast()
}

impl Tex {
    pub fn handle_count(&self) -> usize {
        self.handles.len()
    }

    pub fn handle(&self, i: usize) -> Option<u32> {
        self.handles.get(i).copied()
    }

    pub fn contains_data(&self) -> bool {
        self.contains_data
    }

    pub fn current(&self, count: f32) -> u32 {
        plan::handle_slot(self.contains_data, self.handles.len(), count)
            .and_then(|i| self.handles.get(i))
            .copied()
            .unwrap_or(0)
    }

    pub fn generate_handles(&mut self, n: i32) {
        let have = self.handles.len();
        let Ok(want) = usize::try_from(n) else { return };
        if have < want {
            self.handles.resize(want, 0);
            if let Some(first) = self.handles.get_mut(have..) {
                gl!(glGenTextures(
                    i32::try_from(first.len()).unwrap_or(0),
                    first.as_mut_ptr()
                ));
            }
        }
    }

    fn each(&self, mut f: impl FnMut(u32)) {
        for &h in &self.handles {
            f(h);
        }
    }

    /// `CreateTexture` for one image into the handle at `idx`.
    ///
    /// # Safety
    /// `face.levels` holds `face.num_mips` entries (at least the levels the plan uploads).
    #[allow(clippy::too_many_arguments)]
    pub unsafe fn create_texture(
        &mut self,
        io: &mut Io,
        gl_handle: u32,
        face: &Face,
        face_num: i32,
        check_resize: bool,
        res: &mut Res,
    ) -> bool {
        let p = plan::plan_create(&CreateIn {
            tex_type: io.tex_type,
            format: face.format,
            size: face.size,
            min_size: io.min_size,
            num_mips: face.num_mips,
            down_level: io.down_level,
            generate_mips: io.use_mips,
            check_resize,
        });
        if p.warn_pow2 {
            res.warn_pow2 += 1;
        }
        io.size = p.size;
        io.pixel_format = face.format;
        io.compressed = p.compressed;

        let target = tables::texture_target(io.tex_type);
        let gl_format = tables::pixel_format(face.format);
        // SAFETY: caller contract; level 0 exists.
        let top = unsafe { *face.levels };
        let mut resized = Vec::new();
        let resize_bytes = usize::try_from(p.resize_bytes).unwrap_or(0);
        if p.resize {
            resized = vec![0u8; resize_bytes];
            // SAFETY: GLU reads the source level and fills the buffer sized by the plan.
            unsafe {
                glu::gluScaleImage(
                    gl_format,
                    face.size.x,
                    face.size.y,
                    g::GL_UNSIGNED_BYTE,
                    top.data.cast(),
                    p.size.x,
                    p.size.y,
                    g::GL_UNSIGNED_BYTE,
                    resized.as_mut_ptr().cast(),
                );
            }
        }

        gl!(glEnable(target));
        gl!(glBindTexture(target, gl_handle));

        let mut ok = true;
        for i in p.start_level..p.end_level {
            let count = i - p.start_level;
            let v = plan::level_size(p.size, count);
            // SAFETY: caller contract.
            let lvl = unsafe { *face.levels.offset(i as isize) };
            let (data, size) = if count == 0 && p.resize {
                (ptr_or_null(&resized), p.resize_bytes)
            } else {
                (lvl.data.cast(), lvl.size)
            };
            if !self.copy_to_gl(io, count, data, size, v, face.format, face_num) {
                ok = false;
                break;
            }
        }

        if !ok {
            gl!(glDisable(target));
            return false;
        }

        self.contains_data = true;

        if p.max_level >= 0 {
            gl!(glTexParameteri(
                target,
                g::GL_TEXTURE_MAX_LEVEL,
                p.max_level
            ));
        }

        if p.build_mips {
            if p.resize {
                generate_mips(
                    io,
                    target,
                    face.format,
                    io.size,
                    ptr_or_null(&resized),
                    face_num,
                );
            } else {
                generate_mips(io, target, face.format, io.size, top.data.cast(), face_num);
            }
        }

        gl!(glDisable(target));
        true
    }

    #[allow(clippy::too_many_arguments)]
    fn copy_to_gl(
        &self,
        io: &mut Io,
        level: i32,
        data: *const c_void,
        data_size: i32,
        size: Vector3l,
        format: i32,
        face: i32,
    ) -> bool {
        let up = plan::plan_upload(io.tex_type, io.srgb, format, level, data_size, size, face);
        while glr!(glGetError() or 0) != g::GL_NO_ERROR {}
        io.memory = io.memory.wrapping_add(data_size);
        let s = up.size;
        match up.kind {
            1 => gl!(glCompressedTexImage1DARB(
                up.target,
                up.level,
                up.internal,
                s.x,
                0,
                up.bytes,
                data
            )),
            2 => {
                gl!(glCompressedTexImage2DARB(
                    up.target,
                    up.level,
                    up.internal,
                    s.x,
                    s.y,
                    0,
                    up.bytes,
                    data
                ));
                if up.swizzle {
                    let sw = [
                        g::GL_RED as i32,
                        g::GL_RED as i32,
                        g::GL_RED as i32,
                        g::GL_GREEN as i32,
                    ];
                    gl!(glTexParameteriv(
                        tables::texture_target(io.tex_type),
                        g::GL_TEXTURE_SWIZZLE_RGBA,
                        sw.as_ptr()
                    ));
                }
            }
            3 => gl!(glCompressedTexImage3DARB(
                up.target,
                up.level,
                up.internal,
                s.x,
                s.y,
                s.z,
                0,
                up.bytes,
                data
            )),
            4 => gl!(glTexImage1D(
                up.target,
                up.level,
                up.internal as i32,
                s.x,
                0,
                up.format,
                up.gl_type,
                data
            )),
            5 => gl!(glTexImage2D(
                up.target,
                up.level,
                up.internal as i32,
                s.x,
                s.y,
                0,
                up.format,
                up.gl_type,
                data
            )),
            6 => gl!(glTexImage3D(
                up.target,
                up.level,
                up.internal as i32,
                s.x,
                s.y,
                s.z,
                0,
                up.format,
                up.gl_type,
                data
            )),
            _ => {}
        }
        glr!(glGetError() or 0) == g::GL_NO_ERROR
    }

    /// `CreateFromRawData` up to the property setup (which the shell runs between).
    pub fn create_raw(
        &mut self,
        io: &mut Io,
        size: Vector3l,
        format: i32,
        data: *const c_void,
    ) -> bool {
        self.generate_handles(1);
        let (raw, bytes) = plan::raw_size(size, format);
        io.size = raw;
        io.pixel_format = format;
        let target = tables::texture_target(io.tex_type);
        let h0 = self.handles.first().copied().unwrap_or(0);
        gl!(glEnable(target));
        gl!(glBindTexture(target, h0));
        if !self.copy_to_gl(io, 0, data, bytes, size, format, 0) {
            gl!(glDisable(target));
            return false;
        }
        self.contains_data = true;
        if io.use_mips {
            generate_mips(io, target, format, size, data, 0);
        }
        true
    }

    /// # Safety
    /// `data` holds the bytes of the region.
    pub unsafe fn set_raw_data(
        &self,
        io: &Io,
        level: i32,
        off: Vector3l,
        size: Vector3l,
        format: i32,
        data: *const c_void,
    ) {
        if !self.contains_data || data.is_null() {
            return;
        }
        let target = tables::texture_target(io.tex_type);
        let gl_format = tables::pixel_format(format);
        gl!(glEnable(target));
        gl!(glBindTexture(
            target,
            self.handles.first().copied().unwrap_or(0)
        ));
        match io.tex_type {
            plan::TYPE_1D => gl!(glTexSubImage1D(
                target,
                level,
                off.x,
                size.x,
                gl_format,
                g::GL_UNSIGNED_BYTE,
                data
            )),
            plan::TYPE_2D | plan::TYPE_RECT => gl!(glTexSubImage2D(
                target,
                level,
                off.x,
                off.y,
                size.x,
                size.y,
                gl_format,
                g::GL_UNSIGNED_BYTE,
                data
            )),
            plan::TYPE_3D => gl!(glTexSubImage3D(
                target,
                level,
                off.x,
                off.y,
                off.z,
                size.x,
                size.y,
                size.z,
                gl_format,
                g::GL_UNSIGNED_BYTE,
                data
            )),
            _ => {}
        }
        gl!(glDisable(target));
    }

    /// # Safety
    /// `out` holds `size.x * size.y * 4` floats.
    pub unsafe fn read_pixels(&self, io: &Io, count: f32, depth: bool, out: *mut f32) -> bool {
        let handle = self.current(count);
        if handle == 0 {
            return false;
        }
        let target = tables::texture_target(io.tex_type);
        gl!(glBindTexture(target, handle));
        if depth {
            let n = usize::try_from(io.size.x.wrapping_mul(io.size.y)).unwrap_or(0);
            let mut d = vec![0f32; n];
            gl!(glGetTexImage(
                target,
                0,
                g::GL_DEPTH_COMPONENT,
                g::GL_FLOAT,
                d.as_mut_ptr().cast()
            ));
            for (i, v) in d.iter().enumerate() {
                for k in 0..3 {
                    // SAFETY: caller contract.
                    unsafe { *out.add(i * 4 + k) = *v };
                }
                // SAFETY: caller contract.
                unsafe { *out.add(i * 4 + 3) = 1.0 };
            }
        } else {
            gl!(glGetTexImage(
                target,
                0,
                g::GL_RGBA,
                g::GL_FLOAT,
                out.cast()
            ));
        }
        gl!(glBindTexture(target, 0));
        true
    }

    pub fn set_filter(&self, io: &mut Io, filter: i32) {
        if io.filter == filter {
            return;
        }
        io.filter = filter;
        if self.contains_data {
            let target = tables::texture_target(io.tex_type);
            let f = plan::filter_params(io.filter, io.use_mips, io.tex_type);
            gl!(glEnable(target));
            self.each(|h| {
                gl!(glBindTexture(target, h));
                if f[0] != 0 {
                    gl!(glTexParameteri(
                        target,
                        g::GL_TEXTURE_MIN_FILTER,
                        f[0] as i32
                    ));
                    gl!(glTexParameteri(
                        target,
                        g::GL_TEXTURE_MAG_FILTER,
                        f[1] as i32
                    ));
                }
            });
            gl!(glDisable(target));
        }
    }

    pub fn apply_aniso(&self, io: &Io, x: f32) {
        let target = tables::texture_target(io.tex_type);
        gl!(glEnable(target));
        self.each(|h| {
            gl!(glBindTexture(target, h));
            gl!(glTexParameterf(target, g::GL_TEXTURE_MAX_ANISOTROPY_EXT, x));
        });
        gl!(glDisable(target));
    }

    /// `mask` bit 0/1/2 selects S/T/R.
    pub fn set_wrap(&self, io: &mut Io, mask: u32, mode: i32) {
        if io.tex_type == plan::TYPE_RECT {
            return;
        }
        for (i, w) in io.wrap.iter_mut().enumerate() {
            if mask & (1 << i) != 0 {
                *w = mode;
            }
        }
        if !self.contains_data {
            return;
        }
        let target = tables::texture_target(io.tex_type);
        let gl_mode = tables::wrap(mode) as i32;
        gl!(glEnable(target));
        self.each(|h| {
            gl!(glBindTexture(target, h));
            if mask & 1 != 0 {
                gl!(glTexParameteri(target, g::GL_TEXTURE_WRAP_S, gl_mode));
            }
            if mask & 2 != 0 {
                gl!(glTexParameteri(target, g::GL_TEXTURE_WRAP_T, gl_mode));
            }
            if mask & 4 != 0 {
                gl!(glTexParameteri(target, g::GL_TEXTURE_WRAP_R, gl_mode));
            }
        });
        gl!(glDisable(target));
    }

    pub fn set_compare(&self, io: &mut Io, func: bool, value: i32) {
        if func {
            io.compare_func = value;
        } else {
            io.compare_mode = value;
        }
        if !self.contains_data {
            return;
        }
        let target = tables::texture_target(io.tex_type);
        let (pname, gl_value) = if func {
            (g::GL_TEXTURE_COMPARE_FUNC, tables::compare_func(value))
        } else {
            (g::GL_TEXTURE_COMPARE_MODE, tables::compare_mode(value))
        };
        gl!(glEnable(target));
        self.each(|h| {
            gl!(glBindTexture(target, h));
            gl!(glTexParameteri(target, pname, gl_value as i32));
        });
        gl!(glDisable(target));
    }

    pub fn set_red_from_alpha(&self, io: &Io, on: bool) {
        if !self.contains_data || io.tex_type != plan::TYPE_2D {
            return;
        }
        let mut prev = 0i32;
        gl!(glGetIntegerv(g::GL_TEXTURE_BINDING_2D, &mut prev));
        self.each(|h| {
            gl!(glBindTexture(g::GL_TEXTURE_2D, h));
            gl!(glTexParameteri(
                g::GL_TEXTURE_2D,
                g::GL_TEXTURE_SWIZZLE_R,
                if on {
                    g::GL_ALPHA as i32
                } else {
                    g::GL_RED as i32
                }
            ));
        });
        gl!(glBindTexture(g::GL_TEXTURE_2D, prev as u32));
    }

    pub fn auto_generate_mipmaps(&self, io: &Io) {
        if !io.use_mips || !caps_fbo() {
            return;
        }
        let target = tables::texture_target(io.tex_type);
        let binding = tables::texture_binding(target);
        let mut prev = 0i32;
        gl!(glGetIntegerv(binding, &mut prev));
        gl!(glEnable(target));
        self.each(|h| {
            gl!(glBindTexture(target, h));
            gl!(glGenerateMipmapEXT(target));
        });
        gl!(glDisable(target));
        gl!(glBindTexture(target, prev as u32));
    }

    /// `SetupProperties` up to the anisotropy decision; true for a rectangle texture.
    pub fn props_begin(&self, io: &mut Io, gl_handle: u32) -> Option<bool> {
        if !self.contains_data {
            return None;
        }
        let target = tables::texture_target(io.tex_type);
        gl!(glEnable(target));
        gl!(glBindTexture(target, gl_handle));
        let props = plan::plan_props(io.tex_type, io.use_mips, io.filter, io.wrap);
        gl!(glTexParameteri(
            target,
            g::GL_TEXTURE_MIN_FILTER,
            props.min_filter as i32
        ));
        gl!(glTexParameteri(
            target,
            g::GL_TEXTURE_MAG_FILTER,
            c::LINEAR as i32
        ));
        if props.rect {
            io.wrap = [WRAP_CLAMP_TO_EDGE; 3];
        }
        gl!(glTexParameteri(
            target,
            g::GL_TEXTURE_WRAP_S,
            props.wrap[0] as i32
        ));
        gl!(glTexParameteri(
            target,
            g::GL_TEXTURE_WRAP_T,
            props.wrap[1] as i32
        ));
        gl!(glTexParameteri(
            target,
            g::GL_TEXTURE_WRAP_R,
            props.wrap[2] as i32
        ));
        Some(props.rect)
    }

    pub fn props_aniso(&self, io: &Io) {
        let target = tables::texture_target(io.tex_type);
        gl!(glTexParameterf(
            target,
            g::GL_TEXTURE_MAX_ANISOTROPY_EXT,
            io.aniso
        ));
    }

    pub fn props_end(&self, io: &Io) {
        gl!(glDisable(tables::texture_target(io.tex_type)));
    }
}

impl Drop for Tex {
    fn drop(&mut self) {
        for h in &self.handles {
            gl!(glDeleteTextures(1, h));
        }
    }
}

fn generate_mips(
    io: &mut Io,
    target: u32,
    format: i32,
    size: Vector3l,
    data: *const c_void,
    face: i32,
) {
    let p = plan::plan_mips(
        io.tex_type,
        io.usage,
        caps_fbo(),
        io.srgb,
        format,
        size,
        face,
    );
    match p.action {
        plan::MIP_GENERATE => gl!(glGenerateMipmapEXT(target)),
        // SAFETY: GLU reads the level data the caller still owns.
        plan::MIP_BUILD_1D => unsafe {
            glu::gluBuild1DMipmaps(
                target,
                p.internal as i32,
                size.x,
                p.format,
                g::GL_UNSIGNED_BYTE,
                data,
            );
        },
        // SAFETY: as above.
        plan::MIP_BUILD_2D => unsafe {
            glu::gluBuild2DMipmaps(
                p.target,
                p.internal as i32,
                size.x,
                size.y,
                p.format,
                g::GL_UNSIGNED_BYTE,
                data,
            );
        },
        _ => {}
    }
    io.memory = io.memory.wrapping_add(p.memory);
    if p.disable_mips {
        io.use_mips = false;
    }
}
