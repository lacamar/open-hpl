//! `cGpuShaderManager::CreateShader` flow; parsing, transpiling and compiling stay host calls.

use super::h3_ops::{Create, Flow, Str, WStr, c, k, null};
use crate::shader::fallback_name;
use std::ffi::c_void;

/// `SHADER_POST` bits.
pub const POST_HPSL_BINDINGS: i64 = 1;
pub const POST_SAMPLER_UNITS: i64 = 2;

impl Flow<'_> {
    fn shader_transpile(&mut self, name: &[u8], ty: i32) -> bool {
        let mut cr = Create::new(k::SHADER_TRANSPILE);
        cr.name = Str::of(name);
        cr.i[0] = ty;
        self.ops.create(&mut cr);
        cr.status == 0
    }

    fn shader_string(&mut self, name: &[u8], path: &[u32], ty: i32) -> *mut c_void {
        let mut cr = Create::new(k::SHADER_STRING);
        cr.name = Str::of(name);
        cr.path = WStr::of(path);
        cr.i[0] = ty;
        self.ops.create(&mut cr)
    }

    fn shader_transpile_failed(&mut self, name: &[u8], hpsl: &[u8]) {
        let why = self.ops.text(c::SHADER_ERROR, null(), &[]);
        self.ops.err(&[
            b"Couldn't transpile HPSL shader '",
            name,
            b"' (from '",
            hpsl,
            b"'): ",
            &why,
            b"\n",
        ]);
    }

    pub fn gpu_shader(
        &mut self,
        name: &[u8],
        ty: i32,
        vars: *mut c_void,
        has_transpile: bool,
        fragment: i32,
    ) -> *mut c_void {
        self.begin(name);
        let shader = if vars.is_null() {
            let Some(s) = self.shader_plain(name, ty, has_transpile) else {
                self.end();
                return null();
            };
            s
        } else {
            let Some(s) = self.shader_with_vars(name, ty, vars, has_transpile, fragment) else {
                self.end();
                return null();
            };
            self.end();
            return s;
        };
        if shader.is_null() {
            self.ops.err(&[b"Couldn't load program '", name, b"'\n"]);
        } else {
            self.inc_user(shader);
        }
        self.end();
        shader
    }

    /// `None`: the load failed (the error is logged) and the tab closes.
    fn shader_with_vars(
        &mut self,
        name: &[u8],
        ty: i32,
        vars: *mut c_void,
        has_transpile: bool,
        fragment: i32,
    ) -> Option<*mut c_void> {
        let mut path = self.ops.file_path(name, false).0;
        let mut hpsl = Vec::new();
        let mut fallback = false;
        if path.is_empty() && has_transpile {
            hpsl = fallback_name(name);
            let hp = self.ops.file_path(&hpsl, false).0;
            if !hp.is_empty() {
                path = hp;
                fallback = true;
            }
        }
        if path.is_empty() {
            self.ops
                .err(&[b"Couldn't find file '", name, b"' in resources\n"]);
            return None;
        }
        let mut cr = Create::new(k::SHADER_PARSE);
        cr.name = Str::of(name);
        cr.name2 = Str::of(&hpsl);
        cr.path = WStr::of(&path);
        cr.i = [0, i32::from(fallback), 0, 0];
        cr.ptr = vars;
        self.ops.create(&mut cr);
        if fallback && !self.shader_transpile(name, ty) {
            self.shader_transpile_failed(name, &hpsl);
            return None;
        }
        let shader = self.shader_string(name, &path, ty);
        if shader.is_null() {
            self.ops.err(&[b"Couldn't create program '", name, b"'\n"]);
            return None;
        }
        let bits = if fallback { POST_HPSL_BINDINGS } else { 0 }
            | if fragment != 0 { POST_SAMPLER_UNITS } else { 0 };
        self.ops.call(c::SHADER_POST, shader, bits);
        Some(shader)
    }

    fn shader_plain(&mut self, name: &[u8], ty: i32, has_transpile: bool) -> Option<*mut c_void> {
        let lk = self.find_loaded(name, false);
        let mut shader = lk.res;
        if shader.is_null() && !lk.path.is_empty() {
            let mut cr = Create::new(k::SHADER_FILE);
            cr.name = Str::of(name);
            cr.path = WStr::of(&lk.path);
            cr.i[0] = ty;
            shader = self.ops.create(&mut cr);
            if shader.is_null() {
                self.ops.err(&[b"Couldn't create program '", name, b"'\n"]);
                return None;
            }
            self.add(shader, true, true);
        } else if shader.is_null() && lk.path.is_empty() && has_transpile {
            let hpsl = fallback_name(name);
            let hp = self.ops.file_path(&hpsl, false).0;
            if !hp.is_empty() {
                let mut cr = Create::new(k::SHADER_PARSE);
                cr.name = Str::of(name);
                cr.name2 = Str::of(&hpsl);
                cr.path = WStr::of(&hp);
                cr.i = [1, 1, 0, 0];
                self.ops.create(&mut cr);
                if self.shader_transpile(name, ty) {
                    shader = self.shader_string(name, &hp, ty);
                    if shader.is_null() {
                        self.ops.err(&[
                            b"Couldn't create program '",
                            name,
                            b"' (from transpiled HPSL '",
                            &hpsl,
                            b"')\n",
                        ]);
                    } else {
                        self.ops.call(c::SHADER_POST, shader, POST_HPSL_BINDINGS);
                        self.add(shader, true, true);
                    }
                } else {
                    self.shader_transpile_failed(name, &hpsl);
                }
            }
        }
        Some(shader)
    }
}
