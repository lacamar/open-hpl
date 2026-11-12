//! `cVertexBufferOGL_VBO` / `cVertexBufferOGL_Array` GL calls. The element data stays in the C++
//! shell (callers hold pointers into it); the GL names and element descriptors live here.

use core::ffi::c_void;

use super::Store;
use crate::backend::gl_ctx::ctx::{Sources, run_ops_with};
use crate::gl::tables;
use crate::gl::vbuf::{self, Elem, MAX_OPS, Op};
use crate::sys::gl as g;

pub(crate) static VBS: Store<Vb> = Store::new(2);

/// Data of one element array: pointer and byte size.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Data {
    pub ptr: *const c_void,
    pub bytes: usize,
}

pub struct Vb {
    vbo: bool,
    elems: Vec<Elem>,
    flags: Vec<u32>,
    element_handle: u32,
}

impl Vb {
    pub fn new(vbo: bool) -> Self {
        Self {
            vbo,
            elems: Vec::new(),
            flags: Vec::new(),
            element_handle: 0,
        }
    }

    pub fn add_element(&mut self, kind: i32, format: i32, element_num: i32, flag: u32) {
        self.elems.push(Elem {
            kind,
            format,
            element_num,
            handle: 0,
        });
        self.flags.push(flag);
    }

    pub fn needs_compile(&self, index_count: usize) -> bool {
        self.element_handle == 0 && index_count > 0
    }

    pub fn handle_of(&self, i: usize) -> u32 {
        self.elems.get(i).map_or(0, |e| e.handle)
    }

    /// `CompileSpecific` of the VBO variant.
    ///
    /// # Safety
    /// `data` holds one entry per element; `indices` holds `index_count` entries.
    pub unsafe fn compile(
        &mut self,
        usage: i32,
        data: *const Data,
        indices: *const u32,
        index_count: usize,
    ) {
        let usage = tables::usage(usage);
        for (i, e) in self.elems.iter_mut().enumerate() {
            // SAFETY: caller contract.
            let d = unsafe { *data.add(i) };
            gl!(glGenBuffersARB(1, &mut e.handle));
            gl!(glBindBufferARB(g::GL_ARRAY_BUFFER_ARB, e.handle));
            gl!(glBufferDataARB(
                g::GL_ARRAY_BUFFER_ARB,
                d.bytes as isize,
                d.ptr,
                usage
            ));
            gl!(glBindBufferARB(g::GL_ARRAY_BUFFER_ARB, 0));
        }
        gl!(glGenBuffersARB(1, &mut self.element_handle));
        gl!(glBindBufferARB(
            g::GL_ELEMENT_ARRAY_BUFFER_ARB,
            self.element_handle
        ));
        gl!(glBufferDataARB(
            g::GL_ELEMENT_ARRAY_BUFFER_ARB,
            (index_count * 4) as isize,
            indices.cast(),
            usage
        ));
        gl!(glBindBufferARB(g::GL_ELEMENT_ARRAY_BUFFER_ARB, 0));
    }

    /// `UpdateData` of the VBO variant, after the shell compiled when `needs_compile`.
    ///
    /// # Safety
    /// As for `compile`; `data` entries of elements outside `types` are not read.
    pub unsafe fn update(
        &mut self,
        usage: i32,
        types: u32,
        with_indices: bool,
        data: *const Data,
        indices: *const u32,
        index_count: usize,
    ) {
        let usage = tables::usage(usage);
        for (i, e) in self.elems.iter().enumerate() {
            let flag = self.flags.get(i).copied().unwrap_or(0);
            if types & flag != 0 && e.handle > 0 {
                // SAFETY: caller contract.
                let d = unsafe { *data.add(i) };
                gl!(glBindBufferARB(g::GL_ARRAY_BUFFER_ARB, e.handle));
                gl!(glBufferDataARB(
                    g::GL_ARRAY_BUFFER_ARB,
                    d.bytes as isize,
                    core::ptr::null(),
                    usage
                ));
                gl!(glBufferDataARB(
                    g::GL_ARRAY_BUFFER_ARB,
                    d.bytes as isize,
                    d.ptr,
                    usage
                ));
            }
        }
        gl!(glBindBufferARB(g::GL_ARRAY_BUFFER_ARB, 0));
        if with_indices {
            gl!(glBindBufferARB(
                g::GL_ELEMENT_ARRAY_BUFFER_ARB,
                self.element_handle
            ));
            gl!(glBufferDataARB(
                g::GL_ELEMENT_ARRAY_BUFFER_ARB,
                (index_count * 4) as isize,
                indices.cast(),
                usage
            ));
            gl!(glBindBufferARB(g::GL_ELEMENT_ARRAY_BUFFER_ARB, 0));
        }
    }

    /// `SetVertexStates`; `ptrs` holds one array pointer per element for the client-array variant.
    ///
    /// # Safety
    /// `ptrs` is null or holds one entry per element.
    pub unsafe fn bind(&self, ptrs: *const *const c_void) {
        let mut ops = [Op::default(); MAX_OPS];
        let n = if self.vbo {
            vbuf::bind_vbo(&self.elems, &mut ops)
        } else {
            vbuf::bind_array(&self.elems, &mut ops)
        };
        run(&ops, n, ptrs);
    }

    pub fn unbind(&self) {
        let mut ops = [Op::default(); MAX_OPS];
        let n = vbuf::unbind(&self.elems, &mut ops);
        run(&ops, n, core::ptr::null());
    }

    /// `Draw`; `indices` is the client index array of the non-VBO variant.
    pub fn draw(
        &self,
        own: i32,
        requested: i32,
        element_num: i32,
        index_count: i32,
        indices: *const u32,
    ) {
        let mode = vbuf::draw_mode(own, requested);
        let count = vbuf::draw_count(element_num, index_count);
        if self.vbo {
            gl!(glBindBufferARB(
                g::GL_ELEMENT_ARRAY_BUFFER_ARB,
                self.element_handle
            ));
            gl!(glDrawElements(
                mode,
                count,
                g::GL_UNSIGNED_INT,
                core::ptr::null()
            ));
            gl!(glBindBufferARB(g::GL_ELEMENT_ARRAY_BUFFER_ARB, 0));
        } else {
            gl!(glDrawElements(
                mode,
                count,
                g::GL_UNSIGNED_INT,
                indices.cast()
            ));
        }
    }
}

fn run(ops: &[Op; MAX_OPS], n: usize, ptrs: *const *const c_void) {
    let src = Sources {
        element: ptrs,
        batch_vertex: core::ptr::null(),
        batch_texcoord: core::ptr::null(),
    };
    // SAFETY: element pointers are only handed to GL; ops index at most the element count.
    unsafe { run_ops_with(super::table(), ops.get(..n).unwrap_or(&[]), &src) };
}

/// `DrawIndices`.
pub fn draw_indices(own: i32, requested: i32, indices: *const u32, count: i32) {
    let mode = vbuf::draw_mode(own, requested);
    gl!(glDrawElements(
        mode,
        count,
        g::GL_UNSIGNED_INT,
        indices.cast()
    ));
}

impl Drop for Vb {
    fn drop(&mut self) {
        if !self.vbo {
            return;
        }
        for e in &self.elems {
            gl!(glDeleteBuffersARB(1, &e.handle));
        }
        gl!(glDeleteBuffersARB(1, &self.element_handle));
    }
}
