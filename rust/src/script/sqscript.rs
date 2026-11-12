//! `cSqScript`: one compiled module plus a context. File loading stays with the C++ shell.

use super::engine::{EngineRef, Output};
use super::sys;
use crate::sdlsys::script_chunks;
use std::ffi::{CString, c_void};

#[derive(Debug, PartialEq, Eq)]
pub enum BuildError {
    /// `AddScriptSection` failed.
    Section,
    /// `Build` failed; `chunks` are the log pieces of the compiler output.
    Build { chunks: Vec<Vec<u8>> },
}

pub struct Script {
    engine: EngineRef,
    output: *const Output,
    ctx: *mut c_void,
    module: *mut c_void,
    name: CString,
    funcs: Vec<*mut c_void>,
}

pub fn module_name(rand: i32, handle: i32) -> String {
    format!("Module_{rand}_{handle}")
}

impl Script {
    /// # Safety
    /// `output` points at the `Output` of `engine`'s owner and outlives the script.
    pub unsafe fn new(engine: EngineRef, output: *const Output, handle: i32, rand: i32) -> Self {
        // SAFETY: plain call.
        let ctx = unsafe { sys::ohpl_as_ctx_new(engine.as_raw()) };
        let name = CString::new(module_name(rand, handle)).unwrap_or_default();
        Self {
            engine,
            output,
            ctx,
            module: std::ptr::null_mut(),
            name,
            funcs: Vec::new(),
        }
    }

    fn output(&self) -> Option<&Output> {
        // SAFETY: `new` contract.
        unsafe { self.output.as_ref() }
    }

    /// Returns the compile messages (what `apCompileMessages` receives) with the result.
    pub fn build(&mut self, src: &[u8]) -> (Vec<u8>, Result<(), BuildError>) {
        // SAFETY: the name outlives the calls; the engine owns the module.
        self.module = unsafe {
            sys::ohpl_as_get_module(
                self.engine.as_raw(),
                self.name.as_ptr(),
                sys::GM_ALWAYS_CREATE,
            )
        };
        // SAFETY: `src` is valid for `len` reads and the section name is NUL-terminated.
        let added = unsafe {
            sys::ohpl_as_module_add_section(
                self.module,
                c"main".as_ptr(),
                src.as_ptr().cast(),
                src.len(),
            )
        };
        if added < 0 {
            return (Vec::new(), Err(BuildError::Section));
        }
        // SAFETY: plain call; message callbacks only touch the `Output` through a `RefCell`.
        let built = unsafe { sys::ohpl_as_module_build(self.module) };
        let msgs = self.output().map(Output::peek).unwrap_or_default();
        if built < 0 {
            let chunks = script_chunks(&msgs)
                .into_iter()
                .map(<[u8]>::to_vec)
                .collect::<Vec<_>>();
            if let Some(o) = self.output() {
                o.clear();
            }
            return (msgs, Err(BuildError::Build { chunks }));
        }
        if let Some(o) = self.output() {
            o.clear();
        }
        (msgs, Ok(()))
    }

    pub fn func_handle(&mut self, name: &str) -> i32 {
        let Ok(n) = CString::new(name) else { return -1 };
        if self.module.is_null() {
            return -1;
        }
        // SAFETY: the module is alive until `drop`.
        let f = unsafe { sys::ohpl_as_module_function_by_name(self.module, n.as_ptr()) };
        if f.is_null() {
            return -1;
        }
        self.funcs.push(f);
        i32::try_from(self.funcs.len() - 1).unwrap_or(-1)
    }

    /// Like the original, the execution result is ignored.
    pub fn run_line(&mut self, line: &str) -> bool {
        if let Ok(l) = CString::new(line) {
            // SAFETY: plain call; a null module is allowed by `ExecuteString`.
            unsafe { sys::ohpl_as_execute_string(self.engine.as_raw(), l.as_ptr(), self.module) };
        }
        true
    }

    pub fn run_handle(&mut self, handle: i32) -> bool {
        let Some(&f) = usize::try_from(handle).ok().and_then(|h| self.funcs.get(h)) else {
            return false;
        };
        // SAFETY: `f` belongs to the live module; results are ignored as in the original.
        unsafe {
            sys::ohpl_as_ctx_prepare(self.ctx, f);
            sys::ohpl_as_ctx_execute(self.ctx);
        }
        true
    }
}

impl Drop for Script {
    fn drop(&mut self) {
        // SAFETY: the module and context are discarded once.
        unsafe {
            sys::ohpl_as_discard_module(self.engine.as_raw(), self.name.as_ptr());
            sys::ohpl_as_ctx_release(self.ctx);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::module_name;

    #[test]
    fn module_name_matches_original() {
        assert_eq!(module_name(123456, 7), "Module_123456_7");
    }
}
