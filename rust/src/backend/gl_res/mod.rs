//! GL textures, vertex buffers, framebuffers and programs (L3): every `gl*` call of
//! `cSDLTexture`, `iVertexBufferOpenGL` and its variants, `cFrameBufferGL` and `cGLSL*` and the
//! ownership of the GL names. The C++ classes are shells holding arena handles; decisions live in
//! `crate::gl`, entry points come from the table `gl_ctx` loaded.

#[cfg(not(test))]
use core::sync::atomic::Ordering;
use std::sync::{Mutex, MutexGuard, OnceLock, PoisonError};

use crate::obj::{Arena, Handle};
use crate::sys::gl::GlFns;

macro_rules! gl {
    ($f:ident($($a:expr),*)) => {
        if let Some(f) = $crate::backend::gl_res::table().$f {
            // SAFETY: entry point resolved for the current context with its C signature.
            unsafe { f($($a),*) }
        }
    };
}

macro_rules! glr {
    ($f:ident($($a:expr),*) or $d:expr) => {
        match $crate::backend::gl_res::table().$f {
            // SAFETY: entry point resolved for the current context with its C signature.
            Some(f) => unsafe { f($($a),*) },
            None => $d,
        }
    };
}

pub mod fbo;
pub mod ffi;
pub mod prog;
#[cfg(test)]
mod tests;
pub mod tex;
pub mod vb;

#[cfg(not(test))]
static CAPS: core::sync::atomic::AtomicU8 = core::sync::atomic::AtomicU8::new(0);

#[cfg(test)]
thread_local! {
    static CAPS: core::cell::Cell<u8> = const { core::cell::Cell::new(0) };
}

fn caps_bits() -> u8 {
    #[cfg(not(test))]
    return CAPS.load(Ordering::Relaxed);
    #[cfg(test)]
    CAPS.get()
}

/// The `GLEW_EXT_framebuffer_object` / `GLEW_EXT_packed_depth_stencil` flags, from the L2 caps.
pub fn set_caps(framebuffer_object: bool, packed_depth_stencil: bool) {
    let bits = u8::from(framebuffer_object) | (u8::from(packed_depth_stencil) << 1);
    #[cfg(not(test))]
    CAPS.store(bits, Ordering::Relaxed);
    #[cfg(test)]
    CAPS.set(bits);
}

pub(crate) fn caps_fbo() -> bool {
    caps_bits() & 1 != 0
}

pub(crate) fn caps_packed() -> bool {
    caps_bits() & 2 != 0
}

#[cfg(test)]
thread_local! {
    pub(crate) static TEST_GL: core::cell::Cell<Option<&'static GlFns>> =
        const { core::cell::Cell::new(None) };
}

/// The process-wide table of the loaded context (empty before `ohpl_glctx_load`).
pub(crate) fn table() -> &'static GlFns {
    #[cfg(test)]
    if let Some(t) = TEST_GL.get() {
        return t;
    }
    static EMPTY: OnceLock<GlFns> = OnceLock::new();
    super::gl_ctx::ctx::table().unwrap_or_else(|| EMPTY.get_or_init(GlFns::default))
}

/// Arena behind a lock; the lock is never held across a call back into C++.
pub(crate) struct Store<T>(u64, OnceLock<Mutex<Arena<T>>>);

impl<T> Store<T> {
    /// `tag` keeps the ids of different stores apart.
    pub const fn new(tag: u64) -> Self {
        Self(tag << 56, OnceLock::new())
    }

    fn untag(&self, h: u64) -> Option<Handle<T>> {
        if h & (0xFF << 56) != self.0 {
            return None;
        }
        Handle::from_bits(h & !(0xFF << 56))
    }

    fn arena(&self) -> MutexGuard<'_, Arena<T>> {
        self.1
            .get_or_init(Mutex::default)
            .lock()
            .unwrap_or_else(PoisonError::into_inner)
    }

    pub fn insert(&self, v: T) -> u64 {
        self.arena().insert(v).map_or(0, |h| h.to_bits() | self.0)
    }

    pub fn with<R>(&self, h: u64, f: impl FnOnce(&mut T) -> R) -> Option<R> {
        let h = self.untag(h)?;
        self.arena().get_mut(h).map(f)
    }

    pub fn remove(&self, h: u64) -> Option<T> {
        self.arena().remove(self.untag(h)?)
    }
}
