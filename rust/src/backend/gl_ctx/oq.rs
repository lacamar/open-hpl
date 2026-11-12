//! `cOcclusionQueryOGL`: the state the C++ shell holds and the `gl*` calls on it.

use super::ctx::table;
use crate::sys::gl as g;
use crate::sys::gl::GlFns;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Query {
    pub fetched: bool,
    pub last_samples: i32,
    pub id: u32,
}

macro_rules! gl {
    ($t:expr, $f:ident($($a:expr),*)) => {
        if let Some(f) = $t.$f {
            // SAFETY: entry point resolved for the current context with its C signature.
            unsafe { f($($a),*) }
        }
    };
}

impl Query {
    pub fn create(gl: &GlFns) -> Self {
        let mut q = Self {
            fetched: true,
            last_samples: 0,
            id: 0,
        };
        gl!(gl, glGenQueriesARB(1, &mut q.id));
        q
    }

    pub fn destroy(&self, gl: &GlFns) {
        gl!(gl, glDeleteQueriesARB(1, &self.id));
    }

    pub fn begin(&mut self, gl: &GlFns) {
        gl!(gl, glBeginQueryARB(g::GL_SAMPLES_PASSED_ARB, self.id));
        self.fetched = false;
    }

    pub fn end(&self, gl: &GlFns) {
        gl!(gl, glEndQueryARB(g::GL_SAMPLES_PASSED_ARB));
    }

    pub fn fetch(&mut self, gl: &GlFns) -> bool {
        if self.fetched {
            return true;
        }
        let is_query = match gl.glIsQuery {
            // SAFETY: entry point resolved for the current context.
            Some(f) => unsafe { f(self.id) },
            None => g::GL_TRUE,
        };
        if is_query == g::GL_FALSE {
            self.fetched = true;
            self.last_samples = 0;
            return true;
        }
        let mut available = 0;
        gl!(
            gl,
            glGetQueryObjectivARB(self.id, g::GL_QUERY_RESULT_AVAILABLE_ARB, &mut available)
        );
        if available == 0 {
            return false;
        }
        gl!(
            gl,
            glGetQueryObjectivARB(self.id, g::GL_QUERY_RESULT_ARB, &mut self.last_samples)
        );
        self.fetched = true;
        true
    }
}

/// The table every query uses: the one `Ctx::load` resolved.
pub fn global() -> Option<&'static GlFns> {
    table()
}
