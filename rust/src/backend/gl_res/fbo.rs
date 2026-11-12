//! `cFrameBufferGL` and `cDepthStencilBufferGL` GL calls.

use super::{Store, caps_packed};
use crate::gl::c;
use crate::gl::fbo::{self, Attach, DrawPlan};
use crate::sys::gl as g;
use crate::types::Vector2l;

pub(crate) static FBS: Store<Fb> = Store::new(5);
pub(crate) static DSBS: Store<Dsb> = Store::new(6);

pub struct Fb {
    pub id: u32,
}

impl Default for Fb {
    fn default() -> Self {
        Self::new()
    }
}

impl Fb {
    pub fn new() -> Self {
        let mut id = 0;
        gl!(glGenFramebuffersEXT(1, &mut id));
        Self { id }
    }

    /// `CheckIfNullTexture` for a null texture.
    pub fn detach_texture(&self, attachment: i32, index: i32) {
        gl!(glBindFramebufferEXT(g::GL_FRAMEBUFFER_EXT, self.id));
        gl!(glFramebufferTexture2DEXT(
            g::GL_FRAMEBUFFER_EXT,
            attachment.wrapping_add(index) as u32,
            g::GL_TEXTURE_2D,
            0,
            0
        ));
        gl!(glBindFramebufferEXT(g::GL_FRAMEBUFFER_EXT, 0));
    }

    /// `AttachTexture`; the plan goes back so the shell can update its slots.
    #[allow(clippy::too_many_arguments)]
    pub fn attach_texture(
        &self,
        attachment: i32,
        index: i32,
        tex_type: i32,
        mip: i32,
        extra: i32,
        tex_size: Vector2l,
        tex_handle: u32,
    ) -> Attach {
        let at = fbo::plan_attach(attachment as u32, index, tex_type, mip, extra, tex_size);
        gl!(glBindFramebufferEXT(g::GL_FRAMEBUFFER_EXT, self.id));
        if at.call == fbo::CALL_TEXTURE_2D {
            gl!(glFramebufferTexture2DEXT(
                g::GL_FRAMEBUFFER_EXT,
                at.attachment,
                at.target,
                tex_handle,
                mip
            ));
        } else if at.call == fbo::CALL_TEXTURE_3D {
            gl!(glFramebufferTexture3DEXT(
                g::GL_FRAMEBUFFER_EXT,
                at.attachment,
                at.target,
                tex_handle,
                mip,
                extra
            ));
        }
        gl!(glBindFramebufferEXT(g::GL_FRAMEBUFFER_EXT, 0));
        at
    }

    /// `SetDepthStencilBuffer`; a null buffer leaves the framebuffer bound, as the C++ does.
    pub fn set_depth_stencil(&self, buffer: Option<(u32, i32, i32)>) {
        gl!(glBindFramebufferEXT(g::GL_FRAMEBUFFER_EXT, self.id));
        let Some((rb, depth_bits, stencil_bits)) = buffer else {
            gl!(glFramebufferRenderbufferEXT(
                g::GL_FRAMEBUFFER_EXT,
                c::DEPTH_ATTACHMENT_EXT,
                g::GL_RENDERBUFFER_EXT,
                0
            ));
            gl!(glFramebufferRenderbufferEXT(
                g::GL_FRAMEBUFFER_EXT,
                g::GL_STENCIL_ATTACHMENT_EXT,
                g::GL_RENDERBUFFER_EXT,
                0
            ));
            return;
        };
        if depth_bits > 0 {
            gl!(glFramebufferRenderbufferEXT(
                g::GL_FRAMEBUFFER_EXT,
                c::DEPTH_ATTACHMENT_EXT,
                g::GL_RENDERBUFFER_EXT,
                rb
            ));
        }
        if stencil_bits > 0 {
            gl!(glFramebufferRenderbufferEXT(
                g::GL_FRAMEBUFFER_EXT,
                g::GL_STENCIL_ATTACHMENT_EXT,
                g::GL_RENDERBUFFER_EXT,
                rb
            ));
        }
        gl!(glBindFramebufferEXT(g::GL_FRAMEBUFFER_EXT, 0));
    }

    /// `CompileAndValidate`: the status class for the shell's error message.
    pub fn compile(&self, occupied: u32) -> u32 {
        gl!(glBindFramebufferEXT(g::GL_FRAMEBUFFER_EXT, self.id));
        let plan: DrawPlan = fbo::plan_draw_buffers(occupied);
        match plan.mode {
            fbo::DRAW_NONE => {
                gl!(glDrawBuffer(g::GL_NONE));
                gl!(glReadBuffer(g::GL_NONE));
            }
            fbo::DRAW_SINGLE => {
                gl!(glDrawBuffer(plan.buffers[0]));
                gl!(glReadBuffer(plan.buffers[0]));
            }
            _ => gl!(glDrawBuffers(plan.count, plan.buffers.as_ptr())),
        }
        let status = glr!(glCheckFramebufferStatusEXT(g::GL_FRAMEBUFFER_EXT) or 0);
        let class = fbo::status_class(status);
        gl!(glBindFramebufferEXT(g::GL_FRAMEBUFFER_EXT, 0));
        class
    }
}

impl Drop for Fb {
    fn drop(&mut self) {
        gl!(glDeleteFramebuffersEXT(1, &self.id));
    }
}

pub struct Dsb {
    pub id: u32,
}

impl Dsb {
    /// `cDepthStencilBufferGL` constructor: the plan kind goes back for the shell's error message.
    pub fn new(size: Vector2l, depth_bits: i32, stencil_bits: i32) -> (Self, u32) {
        let mut s = Self { id: 0 };
        let p = fbo::plan_depth_stencil(depth_bits, stencil_bits, caps_packed());
        if matches!(p.kind, fbo::DS_PACKED | fbo::DS_DEPTH | fbo::DS_STENCIL) {
            gl!(glGenRenderbuffersEXT(1, &mut s.id));
            gl!(glBindRenderbufferEXT(g::GL_RENDERBUFFER_EXT, s.id));
            gl!(glRenderbufferStorageEXT(
                g::GL_RENDERBUFFER_EXT,
                p.format,
                size.x,
                size.y
            ));
        }
        (s, p.kind)
    }
}

impl Drop for Dsb {
    fn drop(&mut self) {
        if self.id != 0 {
            gl!(glDeleteRenderbuffersEXT(1, &self.id));
        }
    }
}
