//! `cGLSLShader` and `cGLSLProgram` GL calls. Messages, timing and diagnostics stay in the shell;
//! info logs come back as bytes.

use core::sync::atomic::{AtomicI32, Ordering};

use super::Store;
use crate::gl::glsl::{self, AsId, Params};
use crate::gl::tables;
use crate::sys::gl as g;

pub(crate) static SHADERS: Store<Shader> = Store::new(3);
pub(crate) static PROGRAMS: Store<Prog> = Store::new(4);

/// `cGLSLProgram::mlCurrentProgram`.
static CURRENT: AtomicI32 = AtomicI32::new(0);

pub struct Shader {
    pub id: u32,
    log: Vec<u8>,
    dump: Vec<u8>,
}

impl Shader {
    pub fn new(shader_type: i32) -> Self {
        let id = glr!(glCreateShader(tables::shader_type(shader_type)) or 0);
        Self {
            id,
            log: Vec::new(),
            dump: Vec::new(),
        }
    }

    /// `CreateFromString` up to the status check; the failure log is kept for `log`.
    pub fn compile(&mut self, source: *const core::ffi::c_char) -> bool {
        gl!(glShaderSource(
            self.id,
            1,
            &source.cast::<i8>(),
            core::ptr::null()
        ));
        gl!(glCompileShader(self.id));
        let mut status = 0;
        gl!(glGetShaderiv(self.id, g::GL_COMPILE_STATUS, &mut status));
        self.log.clear();
        let mut len = 0;
        gl!(glGetShaderiv(self.id, g::GL_INFO_LOG_LENGTH, &mut len));
        if status == i32::from(g::GL_FALSE) && len > 1 {
            self.log = vec![0; usize::try_from(len).unwrap_or(0)];
            gl!(glGetShaderInfoLog(
                self.id,
                len,
                core::ptr::null_mut(),
                self.log.as_mut_ptr().cast()
            ));
        }
        status != i32::from(g::GL_FALSE)
    }

    pub fn log(&self) -> &[u8] {
        &self.log
    }

    /// `LogShaderInfoLog`: the NUL-terminated log, `None` when empty.
    pub fn dump_log(&mut self) -> Option<&[u8]> {
        let mut len = 0;
        gl!(glGetShaderiv(self.id, g::GL_INFO_LOG_LENGTH, &mut len));
        if len <= 0 {
            return None;
        }
        self.dump = vec![0; usize::try_from(len).unwrap_or(0)];
        let mut written = 0;
        gl!(glGetShaderInfoLog(
            self.id,
            len,
            &mut written,
            self.dump.as_mut_ptr().cast()
        ));
        Some(&self.dump)
    }
}

impl Drop for Shader {
    fn drop(&mut self) {
        gl!(glDeleteShader(self.id));
    }
}

pub struct Prog {
    pub id: u32,
    params: Params,
    log: Vec<u8>,
    dump: Vec<u8>,
}

pub fn current() -> i32 {
    CURRENT.load(Ordering::Relaxed)
}

fn bind_id(handle: u32) {
    let mut cur = current();
    let due = glsl::bind(&mut cur, handle);
    CURRENT.store(cur, Ordering::Relaxed);
    if due {
        gl!(glUseProgram(handle));
    }
}

pub fn unbind() {
    let mut cur = current();
    let due = glsl::unbind(&mut cur);
    CURRENT.store(cur, Ordering::Relaxed);
    if due {
        gl!(glUseProgram(0));
    }
}

impl Default for Prog {
    fn default() -> Self {
        Self::new()
    }
}

impl Prog {
    pub fn new() -> Self {
        Self {
            id: glr!(glCreateProgram() or 0),
            params: Params::default(),
            log: Vec::new(),
            dump: Vec::new(),
        }
    }

    pub fn attach(&self, shader: u32) {
        gl!(glAttachShader(self.id, shader));
    }

    pub fn detach(&self, shader: u32) {
        gl!(glDetachShader(self.id, shader));
    }

    /// `glLinkProgram` and the status check; the failure log is kept for `log`.
    pub fn link(&mut self) -> bool {
        gl!(glLinkProgram(self.id));
        let mut status = 0;
        gl!(glGetProgramiv(self.id, g::GL_LINK_STATUS, &mut status));
        self.log.clear();
        let mut len = 0;
        gl!(glGetProgramiv(self.id, g::GL_INFO_LOG_LENGTH, &mut len));
        if status == i32::from(g::GL_FALSE) && len > 1 {
            self.log = vec![0; usize::try_from(len).unwrap_or(0)];
            gl!(glGetProgramInfoLog(
                self.id,
                len,
                core::ptr::null_mut(),
                self.log.as_mut_ptr().cast()
            ));
        }
        status != i32::from(g::GL_FALSE)
    }

    pub fn log(&self) -> &[u8] {
        &self.log
    }

    /// `LogProgramInfoLog`: the NUL-terminated log, `None` when empty.
    pub fn dump_log(&mut self) -> Option<&[u8]> {
        let mut len = 0;
        gl!(glGetProgramiv(self.id, g::GL_INFO_LOG_LENGTH, &mut len));
        if len <= 0 {
            return None;
        }
        self.dump = vec![0; usize::try_from(len).unwrap_or(0)];
        let mut written = 0;
        gl!(glGetProgramInfoLog(
            self.id,
            len,
            &mut written,
            self.dump.as_mut_ptr().cast()
        ));
        Some(&self.dump)
    }

    pub fn use_program(&self) {
        gl!(glUseProgram(self.id));
    }

    pub fn restore_current(&self) {
        gl!(glUseProgram(current() as u32));
    }

    fn uniform(&self, name: *const core::ffi::c_char) -> i32 {
        glr!(glGetUniformLocation(self.id, name.cast()) or -1)
    }

    /// Link-time `glUniform1i` for an active sampler.
    pub fn link_sampler(&self, name: *const core::ffi::c_char, unit: i32) {
        let loc = self.uniform(name);
        if loc < 0 {
            return;
        }
        gl!(glUniform1i(loc, unit));
    }

    pub fn set_sampler(&self, name: *const core::ffi::c_char, unit: i32) -> bool {
        let loc = self.uniform(name);
        if loc < 0 {
            return false;
        }
        bind_id(self.id);
        gl!(glUniform1i(loc, unit));
        unbind();
        true
    }

    pub fn variable_id(&mut self, name: &[u8], cname: *const core::ffi::c_char) -> i32 {
        if let Some(i) = self.params.find(name) {
            return i32::try_from(i).unwrap_or(-1);
        }
        let loc = self.uniform(cname);
        if loc < 0 {
            return -1;
        }
        self.params.add(name, loc)
    }

    pub fn variable_as_id(
        &mut self,
        name: &[u8],
        cname: *const core::ffi::c_char,
        id: i32,
    ) -> bool {
        match self.params.as_id_begin(name, id) {
            AsId::Done(done) => done,
            AsId::Lookup => {
                let loc = self.uniform(cname);
                self.params.as_id_finish(name, id, loc)
            }
        }
    }

    /// Location of a known parameter, binding the program first when it is not the current one.
    fn location(&self, id: i32) -> Option<i32> {
        let loc = self.params.location(id)?;
        if current() as u32 != self.id {
            bind_id(self.id);
        }
        Some(loc)
    }

    pub fn set_with(&self, id: i32, f: impl FnOnce(i32)) -> bool {
        match self.location(id) {
            Some(loc) => {
                f(loc);
                true
            }
            None => false,
        }
    }

    pub fn uniform_count(&self) -> i32 {
        let mut n = 0;
        gl!(glGetProgramiv(self.id, g::GL_ACTIVE_UNIFORMS, &mut n));
        n
    }

    /// One `DumpUniforms` row: the name is written NUL-terminated, the value count returned.
    pub fn uniform_at(&self, i: u32, name: &mut [u8; 128], vals: &mut [f32; 16]) -> i32 {
        let (mut size, mut ty) = (0, 0);
        gl!(glGetActiveUniform(
            self.id,
            i,
            128,
            core::ptr::null_mut(),
            &mut size,
            &mut ty,
            name.as_mut_ptr().cast()
        ));
        *vals = [0.0; 16];
        let loc = self.uniform(name.as_ptr().cast());
        gl!(glGetUniformfv(self.id, loc, vals.as_mut_ptr()));
        tables::uniform_components(ty)
    }
}

impl Drop for Prog {
    fn drop(&mut self) {
        let mut cur = current();
        glsl::forget(&mut cur, self.id);
        CURRENT.store(cur, Ordering::Relaxed);
        gl!(glDeleteProgram(self.id));
    }
}

pub fn bind(id: u32) {
    bind_id(id);
}
