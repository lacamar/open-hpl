//! CPU side of `iVertexBufferOpenGL`: element arrays, index array, vertex flags and the passes
//! over them (add vertex, tangents, shadow double, transform, copy). The C++ class is a header of
//! forwarders over one handle; callers hold raw pointers into the arrays until the next resize.

use core::ffi::c_void;

use super::vb::{Data, Vb};
use crate::gl::vbuf::{self, ELEMENT_COUNT, FORMAT_BYTE, FORMAT_INT};
use crate::graphics::formats;
use crate::types::{Matrixf, Vector3f};

const NORMAL: i32 = 0;
const POSITION: i32 = 1;
const TEXTURE1_TANGENT: i32 = 4;
const TEXTURE0: i32 = 5;

#[derive(Clone)]
pub(super) enum Buf {
    F(Vec<f32>),
    I(Vec<i32>),
    B(Vec<u8>),
}

impl Buf {
    fn new(format: i32) -> Self {
        match format {
            FORMAT_INT => Self::I(Vec::new()),
            FORMAT_BYTE => Self::B(Vec::new()),
            _ => Self::F(Vec::new()),
        }
    }

    fn len(&self) -> usize {
        match self {
            Self::F(v) => v.len(),
            Self::I(v) => v.len(),
            Self::B(v) => v.len(),
        }
    }

    fn unit(&self) -> usize {
        match self {
            Self::B(_) => 1,
            _ => 4,
        }
    }

    fn reserve(&mut self, n: usize) {
        match self {
            Self::F(v) => v.reserve_exact(n),
            Self::I(v) => v.reserve_exact(n),
            Self::B(v) => v.reserve_exact(n),
        }
    }

    fn resize(&mut self, n: usize) {
        match self {
            Self::F(v) => v.resize(n, 0.0),
            Self::I(v) => v.resize(n, 0),
            Self::B(v) => v.resize(n, 0),
        }
    }

    /// `PushBack(const void*)`: the float is stored through the array's own element type.
    fn push(&mut self, f: f32) {
        match self {
            Self::F(v) => v.push(f),
            Self::I(v) => v.push(i32::from_ne_bytes(f.to_bits().to_ne_bytes())),
            Self::B(v) => v.push(f.to_ne_bytes().first().copied().unwrap_or(0)),
        }
    }

    /// `&v[0]`: null while nothing was allocated.
    fn ptr(&mut self) -> *mut c_void {
        match self {
            Self::F(v) if v.capacity() > 0 => v.as_mut_ptr().cast(),
            Self::I(v) if v.capacity() > 0 => v.as_mut_ptr().cast(),
            Self::B(v) if v.capacity() > 0 => v.as_mut_ptr().cast(),
            _ => core::ptr::null_mut(),
        }
    }
}

#[derive(Clone)]
pub(super) struct Array {
    kind: i32,
    flag: u32,
    format: i32,
    element_num: i32,
    program_var: i32,
    buf: Buf,
}

impl Array {
    fn bytes(&self) -> usize {
        self.buf.len().wrapping_mul(self.buf.unit())
    }
}

/// One element array as the Vulkan shell reads it.
#[repr(C)]
pub struct ArrayInfo {
    pub kind: i32,
    pub format: i32,
    pub element_num: i32,
    pub program_var: i32,
    pub size: usize,
    pub ptr: *mut c_void,
}

pub(super) struct Store {
    arrays: Vec<Array>,
    index: Vec<u32>,
    slot: [i8; ELEMENT_COUNT],
    flags: u32,
    shadow: bool,
    reserve_vtx: i32,
}

impl Default for Store {
    fn default() -> Self {
        Self {
            arrays: Vec::new(),
            index: Vec::new(),
            slot: [-1; ELEMENT_COUNT],
            flags: 0,
            shadow: false,
            reserve_vtx: 0,
        }
    }
}

type Snapshot = (Vec<Array>, Vec<u32>, bool);

impl Store {
    fn at(&self, kind: i32) -> Option<usize> {
        let s = *self.slot.get(usize::try_from(kind).ok()?)?;
        usize::try_from(s).ok()
    }

    fn arr(&self, kind: i32) -> Option<&Array> {
        self.arrays.get(self.at(kind)?)
    }

    fn arr_mut(&mut self, kind: i32) -> Option<&mut Array> {
        let i = self.at(kind)?;
        self.arrays.get_mut(i)
    }

    fn vertex_num(&self) -> i32 {
        self.arr(POSITION).map_or(0, |p| {
            vbuf::vertex_num(
                i32::try_from(p.buf.len()).unwrap_or(i32::MAX),
                p.element_num,
                self.shadow,
            )
        })
    }
}

impl Vb {
    pub fn with_reserve(vbo: bool, reserve_vtx: i32, reserve_idx: i32) -> Self {
        let mut v = Self::new(vbo);
        v.store.reserve_vtx = reserve_vtx;
        if reserve_idx > 0 {
            v.store
                .index
                .reserve_exact(usize::try_from(reserve_idx).unwrap_or(0));
        }
        v
    }

    /// `CreateElementArray`; returns the element flag, 0 when the flag is already present.
    pub fn create_array(
        &mut self,
        kind: i32,
        format: i32,
        element_num: i32,
        program_var: i32,
    ) -> u32 {
        let flag = formats::vertex_element_flag(kind);
        let s = &mut self.store;
        if flag & s.flags != 0 {
            return 0;
        }
        let Some(slot) = usize::try_from(kind).ok().filter(|k| *k < ELEMENT_COUNT) else {
            return 0;
        };
        s.flags |= flag;
        let mut buf = Buf::new(format);
        if s.reserve_vtx > 0 {
            buf.reserve(usize::try_from(element_num.wrapping_mul(s.reserve_vtx)).unwrap_or(0));
        }
        if let Some(sl) = s.slot.get_mut(slot) {
            *sl = i8::try_from(s.arrays.len()).unwrap_or(-1);
        }
        s.arrays.push(Array {
            kind,
            flag,
            format,
            element_num,
            program_var,
            buf,
        });
        self.add_element(kind, format, element_num, flag);
        flag
    }

    pub fn add_vec3(&mut self, kind: i32, v: Vector3f) {
        let Some(a) = self.store.arr_mut(kind) else {
            return;
        };
        let mut out = [0.0; 4];
        let n = vbuf::vec3_components(a.element_num, v, &mut out);
        for f in out.iter().take(n) {
            a.buf.push(*f);
        }
    }

    pub fn add_vec4(&mut self, kind: i32, v: [f32; 4]) {
        if let Some(a) = self.store.arr_mut(kind) {
            for f in v {
                a.buf.push(f);
            }
        }
    }

    pub fn add_color(&mut self, kind: i32, c: [f32; 4]) {
        if let Some(a) = self.store.arr_mut(kind)
            && vbuf::color_components(a.format) == 4
        {
            for f in c {
                a.buf.push(f);
            }
        }
    }

    pub fn add_index(&mut self, i: u32) {
        self.store.index.push(i);
    }

    pub fn index_num(&self) -> i32 {
        i32::try_from(self.store.index.len()).unwrap_or(i32::MAX)
    }

    pub fn index_ptr(&mut self) -> *mut u32 {
        if self.store.index.capacity() == 0 {
            return core::ptr::null_mut();
        }
        self.store.index.as_mut_ptr()
    }

    pub fn vertex_count(&self) -> i32 {
        self.store.vertex_num()
    }

    pub fn array_ptr(&mut self, kind: i32) -> *mut c_void {
        self.store
            .arr_mut(kind)
            .map_or(core::ptr::null_mut(), |a| a.buf.ptr())
    }

    pub fn array_info(&mut self, i: usize) -> Option<ArrayInfo> {
        let a = self.store.arrays.get_mut(i)?;
        Some(ArrayInfo {
            kind: a.kind,
            format: a.format,
            element_num: a.element_num,
            program_var: a.program_var,
            size: a.buf.len(),
            ptr: a.buf.ptr(),
        })
    }

    pub fn array_count(&self) -> usize {
        self.store.arrays.len()
    }

    /// Element count, format and program variable; `(0, LastEnum, 0)` when absent.
    pub fn element_desc(&self, kind: i32) -> (i32, i32, i32) {
        self.store
            .arr(kind)
            .map_or((0, 3, 0), |a| (a.element_num, a.format, a.program_var))
    }

    pub fn resize_array(&mut self, kind: i32, n: usize) {
        if let Some(a) = self.store.arr_mut(kind) {
            a.buf.resize(n);
        }
    }

    pub fn resize_indices(&mut self, n: usize) {
        self.store.index.resize(n, 0);
    }

    /// 0 ready, 1 no position element, 2 position is not float (`CreateBoundingVolume`).
    pub fn bounding_check(&self) -> i32 {
        vbuf::bounding_check(
            self.store.flags,
            self.store.arr(POSITION).map_or(0, |a| a.format),
        )
    }

    /// Tangent pass of `Compile(CreateTangents)`, after the tangent array exists.
    pub fn fill_tangents(&mut self) {
        let vtx = self.store.vertex_num();
        let n = self.index_num();
        let idx = self.index_ptr();
        let all = [POSITION, NORMAL, TEXTURE0, TEXTURE1_TANGENT]
            .iter()
            .all(|k| self.store.arr(*k).is_some());
        let Some(stride) = self.store.arr(POSITION).map(|a| a.element_num) else {
            return;
        };
        if !all {
            return;
        }
        if let Some(t) = self.store.arr_mut(TEXTURE1_TANGENT) {
            t.buf
                .resize(usize::try_from(vtx.wrapping_mul(4)).unwrap_or(0));
        }
        let pos = self.array_ptr(POSITION);
        let tex = self.array_ptr(TEXTURE0);
        let nor = self.array_ptr(NORMAL);
        let dst = self.array_ptr(TEXTURE1_TANGENT);
        // SAFETY: four distinct arrays sized as the C++ pass sized them; same pointer math.
        unsafe {
            crate::math::tangents::create_tri_tangent_vectors(
                dst.cast(),
                idx,
                n,
                pos.cast(),
                stride,
                tex.cast(),
                nor.cast(),
                vtx,
            );
        }
    }

    /// `CreateShadowDouble`; false when there is no position element.
    pub fn shadow_double(&mut self) -> bool {
        let Some(p) = self.store.arr_mut(POSITION) else {
            return false;
        };
        let size = p.buf.len();
        p.buf
            .resize(size.wrapping_add(vbuf::shadow_double_extra(size)));
        let total = p.buf.len();
        let format = p.format;
        let ptr = p.buf.ptr();
        if !ptr.is_null() && format != FORMAT_BYTE {
            // SAFETY: float and int buffers hold `total` four-byte units.
            let words = unsafe { core::slice::from_raw_parts_mut(ptr.cast::<u32>(), total) };
            vbuf::shadow_double_fill(format, words, size);
        }
        self.store.shadow = true;
        true
    }

    /// `Transform`; returns the element flags to re-upload, 0 when nothing was transformed.
    pub fn transform(&mut self, m: &Matrixf) -> u32 {
        let vtx = self.store.vertex_num();
        let shadow = self.store.shadow;
        let usable = |s: &mut Store, kind| -> Option<(*mut f32, usize)> {
            let a = s.arr_mut(kind)?;
            (a.format != FORMAT_BYTE).then(|| (a.buf.ptr().cast::<f32>(), a.buf.len()))
        };
        let Some(stride) = self.store.arr(POSITION).map(|a| a.element_num) else {
            return 0;
        };
        let Some((pp, pl)) = usable(&mut self.store, POSITION) else {
            return 0;
        };
        let normal = usable(&mut self.store, NORMAL);
        let tangent = usable(&mut self.store, TEXTURE1_TANGENT);
        // SAFETY (both views): distinct live allocations of `len` four-byte units.
        let view = |t: Option<(*mut f32, usize)>| {
            t.filter(|(p, _)| !p.is_null())
                .map(|(p, n)| unsafe { core::slice::from_raw_parts_mut(p, n) })
        };
        let pos: &mut [f32] = if pp.is_null() {
            &mut []
        } else {
            unsafe { core::slice::from_raw_parts_mut(pp, pl) }
        };
        vbuf::transform(m, pos, view(normal), view(tangent), vtx, stride, shadow)
    }

    /// The elements in `flags` (all present when `u32::MAX`), indices and shadow flag, for
    /// `fill_from`; the flags that will land in the copy.
    pub(super) fn snapshot(&self, flags: u32) -> (Snapshot, u32) {
        let want = vbuf::copy_flags(flags, self.store.flags);
        let arrays = self
            .store
            .arrays
            .iter()
            .filter(|a| a.flag & want != 0)
            .cloned()
            .collect();
        ((arrays, self.store.index.clone(), self.store.shadow), want)
    }

    /// Returns the element flags created here.
    pub(super) fn fill_from(&mut self, snap: Snapshot) -> u32 {
        let (arrays, index, shadow) = snap;
        let mut got = 0;
        for a in arrays {
            let f = self.create_array(a.kind, a.format, a.element_num, a.program_var);
            if f == 0 {
                continue;
            }
            got |= f;
            if let Some(d) = self.store.arr_mut(a.kind) {
                d.buf = a.buf;
            }
        }
        self.store.index = index;
        self.store.shadow = shadow;
        got
    }

    fn gl_data(&mut self, types: u32) -> Vec<Data> {
        self.store
            .arrays
            .iter_mut()
            .map(|a| {
                if a.flag & types != 0 {
                    Data {
                        ptr: a.buf.ptr().cast_const(),
                        bytes: a.bytes(),
                    }
                } else {
                    Data {
                        ptr: core::ptr::null(),
                        bytes: 0,
                    }
                }
            })
            .collect()
    }

    fn needs_compile_now(&self) -> bool {
        self.needs_compile(self.store.index.len())
    }

    /// `CompileSpecific` (VBO only).
    pub fn compile_gl(&mut self, usage: i32) {
        if !self.is_vbo() {
            return;
        }
        let data = self.gl_data(u32::MAX);
        let idx = self.index_ptr();
        let n = self.store.index.len();
        // SAFETY: one entry per element, `n` indices.
        unsafe { self.compile(usage, data.as_ptr(), idx, n) };
    }

    /// `UpdateData` (VBO only).
    pub fn update_gl(&mut self, usage: i32, types: u32, with_indices: bool) {
        if !self.is_vbo() {
            return;
        }
        if self.needs_compile_now() {
            self.compile_gl(usage);
        }
        let data = self.gl_data(types);
        let idx = if with_indices {
            self.index_ptr()
        } else {
            core::ptr::null_mut()
        };
        let n = self.store.index.len();
        // SAFETY: one entry per element, `n` indices.
        unsafe { self.update(usage, types, with_indices, data.as_ptr(), idx, n) };
    }

    pub fn bind_gl(&mut self, usage: i32) {
        if self.is_vbo() {
            if self.needs_compile_now() {
                self.compile_gl(usage);
            }
            // SAFETY: null element pointers are the VBO contract.
            unsafe { self.bind(core::ptr::null()) };
        } else {
            let ptrs: Vec<*const c_void> = self
                .store
                .arrays
                .iter_mut()
                .map(|a| a.buf.ptr().cast_const())
                .collect();
            // SAFETY: one pointer per element.
            unsafe { self.bind(ptrs.as_ptr()) };
        }
    }

    pub fn draw_gl(&mut self, own: i32, requested: i32, element_num: i32) {
        let n = self.index_num();
        let idx = self.index_ptr();
        self.draw(own, requested, element_num, n, idx);
    }
}
