//! Raw OpenGL bindings: a function-pointer table loaded through `SDL_GL_GetProcAddress`, and the enums the engine passes.
#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals)]

use core::ffi::{c_char, c_void};

pub type GetProcAddress = unsafe extern "C" fn(*const c_char) -> *mut c_void;

pub type GLDEBUGPROC = unsafe extern "C" fn(
    source: u32,
    type_: u32,
    id: u32,
    severity: u32,
    length: i32,
    message: *const i8,
    userParam: *const c_void,
);

pub const GL_ACTIVE_TEXTURE: u32 = 0x84E0;
pub const GL_ACTIVE_UNIFORMS: u32 = 0x8B86;
pub const GL_ALL_ATTRIB_BITS: u32 = 0xFFFFF;
pub const GL_ALPHA: u32 = 0x1906;
pub const GL_ALPHA_TEST: u32 = 0xBC0;
pub const GL_ARRAY_BUFFER: u32 = 0x8892;
pub const GL_ARRAY_BUFFER_ARB: u32 = 0x8892;
pub const GL_ARRAY_BUFFER_BINDING: u32 = 0x8894;
pub const GL_BACK: u32 = 0x405;
pub const GL_BLEND: u32 = 0xBE2;
pub const GL_CCW: u32 = 0x901;
pub const GL_CLIP_PLANE0: u32 = 0x3000;
pub const GL_COLOR_ARRAY: u32 = 0x8076;
pub const GL_COLOR_ATTACHMENT0_EXT: u32 = 0x8CE0;
pub const GL_COLOR_BUFFER_BIT: u32 = 0x4000;
pub const GL_COMBINE_ARB: u32 = 0x8570;
pub const GL_COMPILE_STATUS: u32 = 0x8B81;
pub const GL_CULL_FACE: u32 = 0xB44;
pub const GL_CURRENT_PROGRAM: u32 = 0x8B8D;
pub const GL_CW: u32 = 0x900;
pub const GL_DEBUG_OUTPUT: u32 = 0x92E0;
pub const GL_DEBUG_OUTPUT_SYNCHRONOUS: u32 = 0x8242;
pub const GL_DEBUG_TYPE_ERROR: u32 = 0x824C;
pub const GL_DEPTH_ATTACHMENT_EXT: u32 = 0x8D00;
pub const GL_DEPTH_BUFFER_BIT: u32 = 0x100;
pub const GL_DEPTH_CLAMP: u32 = 0x864F;
pub const GL_DEPTH_COMPONENT: u32 = 0x1902;
pub const GL_DEPTH_TEST: u32 = 0xB71;
pub const GL_EDGE_FLAG_ARRAY: u32 = 0x8079;
pub const GL_ELEMENT_ARRAY_BUFFER_ARB: u32 = 0x8893;
pub const GL_FLOAT: u32 = 0x1406;
pub const GL_FRAGMENT_SHADER: u32 = 0x8B30;
pub const GL_FRAMEBUFFER_BINDING: u32 = 0x8CA6;
pub const GL_FRAMEBUFFER_EXT: u32 = 0x8D40;
pub const GL_FRONT: u32 = 0x404;
pub const GL_GREEN: u32 = 0x1904;
pub const GL_INDEX_ARRAY: u32 = 0x8077;
pub const GL_INFO_LOG_LENGTH: u32 = 0x8B84;
pub const GL_INVALID_OPERATION: u32 = 0x502;
pub const GL_LEQUAL: u32 = 0x203;
pub const GL_LINEAR: u32 = 0x2601;
pub const GL_LINES: u32 = 1;
pub const GL_LINE_STRIP: u32 = 3;
pub const GL_LINK_STATUS: u32 = 0x8B82;
pub const GL_MAX_CLIP_PLANES: u32 = 0xD32;
pub const GL_MAX_COLOR_ATTACHMENTS_EXT: u32 = 0x8CDF;
pub const GL_MAX_DRAW_BUFFERS: u32 = 0x8824;
pub const GL_MAX_TEXTURE_COORDS_ARB: u32 = 0x8871;
pub const GL_MAX_TEXTURE_IMAGE_UNITS_ARB: u32 = 0x8872;
pub const GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: u32 = 0x84FF;
pub const GL_MODELVIEW: u32 = 0x1700;
pub const GL_MULTISAMPLE_ARB: u32 = 0x809D;
pub const GL_NEAREST: u32 = 0x2600;
pub const GL_NICEST: u32 = 0x1102;
pub const GL_NONE: u32 = 0;
pub const GL_NORMAL_ARRAY: u32 = 0x8075;
pub const GL_NO_ERROR: u32 = 0;
pub const GL_PERSPECTIVE_CORRECTION_HINT: u32 = 0xC50;
pub const GL_POLYGON_OFFSET_FILL: u32 = 0x8037;
pub const GL_PROJECTION: u32 = 0x1701;
pub const GL_QUADS: u32 = 7;
pub const GL_QUERY_RESULT: u32 = 0x8866;
pub const GL_QUERY_RESULT_ARB: u32 = 0x8866;
pub const GL_QUERY_RESULT_AVAILABLE_ARB: u32 = 0x8867;
pub const GL_RED: u32 = 0x1903;
pub const GL_RENDERBUFFER_EXT: u32 = 0x8D41;
pub const GL_RENDERER: u32 = 0x1F01;
pub const GL_REPLACE: u32 = 0x1E01;
pub const GL_RGBA: u32 = 0x1908;
pub const GL_RGBA8: u32 = 0x8058;
pub const GL_SAMPLES_PASSED_ARB: u32 = 0x8914;
pub const GL_SCISSOR_TEST: u32 = 0xC11;
pub const GL_SECONDARY_COLOR_ARRAY: u32 = 0x845E;
pub const GL_SHADING_LANGUAGE_VERSION: u32 = 0x8B8C;
pub const GL_SMOOTH: u32 = 0x1D01;
pub const GL_STENCIL_ATTACHMENT_EXT: u32 = 0x8D20;
pub const GL_STENCIL_BITS: u32 = 0xD57;
pub const GL_STENCIL_BUFFER_BIT: u32 = 0x400;
pub const GL_STENCIL_TEST: u32 = 0xB90;
pub const GL_STENCIL_TEST_TWO_SIDE_EXT: u32 = 0x8910;
pub const GL_TEXTURE: u32 = 0x1702;
pub const GL_TEXTURE0: u32 = 0x84C0;
pub const GL_TEXTURE0_ARB: u32 = 0x84C0;
pub const GL_TEXTURE1_ARB: u32 = 0x84C1;
pub const GL_TEXTURE_2D: u32 = 0xDE1;
pub const GL_TEXTURE_BINDING_2D: u32 = 0x8069;
pub const GL_TEXTURE_COMPARE_FUNC: u32 = 0x884D;
pub const GL_TEXTURE_COMPARE_MODE: u32 = 0x884C;
pub const GL_TEXTURE_COORD_ARRAY: u32 = 0x8078;
pub const GL_TEXTURE_ENV: u32 = 0x2300;
pub const GL_TEXTURE_ENV_COLOR: u32 = 0x2201;
pub const GL_TEXTURE_ENV_MODE: u32 = 0x2200;
pub const GL_TEXTURE_MAG_FILTER: u32 = 0x2800;
pub const GL_TEXTURE_MAX_ANISOTROPY_EXT: u32 = 0x84FE;
pub const GL_TEXTURE_MAX_LEVEL: u32 = 0x813D;
pub const GL_TEXTURE_MIN_FILTER: u32 = 0x2801;
pub const GL_TEXTURE_SWIZZLE_R: u32 = 0x8E42;
pub const GL_TEXTURE_SWIZZLE_RGBA: u32 = 0x8E46;
pub const GL_TEXTURE_WRAP_R: u32 = 0x8072;
pub const GL_TEXTURE_WRAP_S: u32 = 0x2802;
pub const GL_TEXTURE_WRAP_T: u32 = 0x2803;
pub const GL_TIME_ELAPSED: u32 = 0x88BF;
pub const GL_TRIANGLES: u32 = 4;
pub const GL_TRIANGLE_STRIP: u32 = 5;
pub const GL_UNSIGNED_BYTE: u32 = 0x1401;
pub const GL_UNSIGNED_INT: u32 = 0x1405;
pub const GL_VENDOR: u32 = 0x1F00;
pub const GL_VERSION: u32 = 0x1F02;
pub const GL_VERTEX_ARRAY: u32 = 0x8074;
pub const GL_VERTEX_SHADER: u32 = 0x8B31;
pub const GL_EXTENSIONS: u32 = 0x1F03;
pub const GL_TRUE: u8 = 1;
pub const GL_FALSE: u8 = 0;

macro_rules! gl_table {
    ($($name:ident($($arg:ident: $ty:ty),*) $(-> $ret:ty)?;)*) => {
        #[derive(Default)]
        pub struct GlFns {
            $(pub $name: Option<unsafe extern "C" fn($($ty),*) $(-> $ret)?>,)*
        }

        pub const GL_FN_NAMES: &[&str] = &[$(stringify!($name)),*];

        impl GlFns {
            /// Resolves every entry point through `get`; missing ones stay `None`.
            ///
            /// # Safety
            /// `get` must be callable with NUL-terminated names and return either null or a function
            /// with the C signature of that GL entry point for the current context.
            pub unsafe fn load(get: GetProcAddress) -> Self {
                Self {
                    $($name: unsafe {
                        core::mem::transmute::<*mut c_void, Option<unsafe extern "C" fn($($ty),*) $(-> $ret)?>>(
                            get(concat!(stringify!($name), "\0").as_ptr().cast()),
                        )
                    },)*
                }
            }

            pub fn loaded(&self) -> usize {
                0 $(+ usize::from(self.$name.is_some()))*
            }
        }
    };
}

const _: () = assert!(size_of::<Option<unsafe extern "C" fn()>>() == size_of::<*mut c_void>());

gl_table! {
    glActiveStencilFaceEXT(face: u32);
    glActiveTexture(texture: u32);
    glActiveTextureARB(texture: u32);
    glAlphaFunc(func: u32, ref_: f32);
    glAttachShader(program: u32, shader: u32);
    glBegin(mode: u32);
    glBeginQuery(target: u32, id: u32);
    glBeginQueryARB(target: u32, id: u32);
    glBindBuffer(target: u32, buffer: u32);
    glBindBufferARB(target: u32, buffer: u32);
    glBindFramebufferEXT(target: u32, framebuffer: u32);
    glBindRenderbufferEXT(target: u32, renderbuffer: u32);
    glBindTexture(target: u32, texture: u32);
    glBlendFunc(sfactor: u32, dfactor: u32);
    glBlendFuncSeparateEXT(sfactorRGB: u32, dfactorRGB: u32, sfactorAlpha: u32, dfactorAlpha: u32);
    glBufferDataARB(target: u32, size: isize, data: *const c_void, usage: u32);
    glCheckFramebufferStatusEXT(target: u32) -> u32;
    glClear(mask: u32);
    glClearColor(red: f32, green: f32, blue: f32, alpha: f32);
    glClearDepth(depth: f64);
    glClearStencil(s: i32);
    glClientActiveTextureARB(texture: u32);
    glClipPlane(plane: u32, equation: *const f64);
    glColor4f(red: f32, green: f32, blue: f32, alpha: f32);
    glColorMask(red: u8, green: u8, blue: u8, alpha: u8);
    glColorPointer(size: i32, type_: u32, stride: i32, pointer: *const c_void);
    glCompileShader(shader: u32);
    glCompressedTexImage1DARB(target: u32, level: i32, internalformat: u32, width: i32, border: i32, imageSize: i32, data: *const c_void);
    glCompressedTexImage2DARB(target: u32, level: i32, internalformat: u32, width: i32, height: i32, border: i32, imageSize: i32, data: *const c_void);
    glCompressedTexImage3DARB(target: u32, level: i32, internalformat: u32, width: i32, height: i32, depth: i32, border: i32, imageSize: i32, data: *const c_void);
    glCopyTexSubImage2D(target: u32, level: i32, xoffset: i32, yoffset: i32, x: i32, y: i32, width: i32, height: i32);
    glCreateProgram() -> u32;
    glCreateShader(type_: u32) -> u32;
    glCullFace(mode: u32);
    glDebugMessageCallback(callback: Option<GLDEBUGPROC>, userParam: *const c_void);
    glDeleteBuffersARB(n: i32, buffers: *const u32);
    glDeleteFramebuffersEXT(n: i32, framebuffers: *const u32);
    glDeleteProgram(program: u32);
    glDeleteQueriesARB(n: i32, ids: *const u32);
    glDeleteRenderbuffersEXT(n: i32, renderbuffers: *const u32);
    glDeleteShader(shader: u32);
    glDeleteTextures(n: i32, textures: *const u32);
    glDepthFunc(func: u32);
    glDepthMask(flag: u8);
    glDetachShader(program: u32, shader: u32);
    glDisable(cap: u32);
    glDisableClientState(array: u32);
    glDrawBuffer(mode: u32);
    glDrawBuffers(n: i32, bufs: *const u32);
    glDrawElements(mode: u32, count: i32, type_: u32, indices: *const c_void);
    glEnable(cap: u32);
    glEnableClientState(array: u32);
    glEnd();
    glEndQuery(target: u32);
    glEndQueryARB(target: u32);
    glFinish();
    glFlush();
    glFramebufferRenderbufferEXT(target: u32, attachment: u32, renderbuffertarget: u32, renderbuffer: u32);
    glFramebufferTexture2DEXT(target: u32, attachment: u32, textarget: u32, texture: u32, level: i32);
    glFramebufferTexture3DEXT(target: u32, attachment: u32, textarget: u32, texture: u32, level: i32, zoffset: i32);
    glFrontFace(mode: u32);
    glGenBuffersARB(n: i32, buffers: *mut u32);
    glGenerateMipmapEXT(target: u32);
    glGenFramebuffersEXT(n: i32, framebuffers: *mut u32);
    glGenQueries(n: i32, ids: *mut u32);
    glGenQueriesARB(n: i32, ids: *mut u32);
    glGenRenderbuffersEXT(n: i32, renderbuffers: *mut u32);
    glGenTextures(n: i32, textures: *mut u32);
    glGetActiveUniform(program: u32, index: u32, maxLength: i32, length: *mut i32, size: *mut i32, type_: *mut u32, name: *mut i8);
    glGetError() -> u32;
    glGetFloatv(pname: u32, params: *mut f32);
    glGetIntegerv(pname: u32, params: *mut i32);
    glGetProgramInfoLog(program: u32, bufSize: i32, length: *mut i32, infoLog: *mut i8);
    glGetProgramiv(program: u32, pname: u32, param: *mut i32);
    glGetQueryObjectivARB(id: u32, pname: u32, params: *mut i32);
    glGetQueryObjectui64v(id: u32, pname: u32, params: *mut u64);
    glGetShaderInfoLog(shader: u32, bufSize: i32, length: *mut i32, infoLog: *mut i8);
    glGetShaderiv(shader: u32, pname: u32, param: *mut i32);
    glGetString(name: u32) -> *const u8;
    glGetTexImage(target: u32, level: i32, format: u32, type_: u32, pixels: *mut c_void);
    glGetUniformfv(program: u32, location: i32, params: *mut f32);
    glGetUniformLocation(program: u32, name: *const i8) -> i32;
    glHint(target: u32, mode: u32);
    glIsQuery(id: u32) -> u8;
    glLinkProgram(program: u32);
    glLoadIdentity();
    glLoadMatrixf(m: *const f32);
    glMatrixMode(mode: u32);
    glMultiTexCoord2fARB(target: u32, s: f32, t: f32);
    glMultiTexCoord3fARB(target: u32, s: f32, t: f32, r: f32);
    glNormalPointer(type_: u32, stride: i32, pointer: *const c_void);
    glOrtho(left: f64, right: f64, bottom: f64, top: f64, zNear: f64, zFar: f64);
    glPolygonOffset(factor: f32, units: f32);
    glPopAttrib();
    glPopMatrix();
    glPushAttrib(mask: u32);
    glPushMatrix();
    glReadBuffer(mode: u32);
    glReadPixels(x: i32, y: i32, width: i32, height: i32, format: u32, type_: u32, pixels: *mut c_void);
    glRenderbufferStorageEXT(target: u32, internalformat: u32, width: i32, height: i32);
    glScissor(x: i32, y: i32, width: i32, height: i32);
    glSecondaryColorPointerEXT(size: i32, type_: u32, stride: i32, pointer: *const c_void);
    glShadeModel(mode: u32);
    glShaderSource(shader: u32, count: i32, string: *const *const i8, length: *const i32);
    glStencilFunc(func: u32, ref_: i32, mask: u32);
    glStencilFuncSeparateATI(frontfunc: u32, backfunc: u32, ref_: i32, mask: u32);
    glStencilMask(mask: u32);
    glStencilOp(fail: u32, zfail: u32, zpass: u32);
    glStencilOpSeparateATI(face: u32, sfail: u32, dpfail: u32, dppass: u32);
    glTexCoord2f(s: f32, t: f32);
    glTexCoord3f(s: f32, t: f32, r: f32);
    glTexCoordPointer(size: i32, type_: u32, stride: i32, pointer: *const c_void);
    glTexEnvfv(target: u32, pname: u32, params: *const f32);
    glTexEnvi(target: u32, pname: u32, param: i32);
    glTexImage1D(target: u32, level: i32, internalformat: i32, width: i32, border: i32, format: u32, type_: u32, pixels: *const c_void);
    glTexImage2D(target: u32, level: i32, internalformat: i32, width: i32, height: i32, border: i32, format: u32, type_: u32, pixels: *const c_void);
    glTexImage3D(target: u32, level: i32, internalFormat: i32, width: i32, height: i32, depth: i32, border: i32, format: u32, type_: u32, pixels: *const c_void);
    glTexParameterf(target: u32, pname: u32, param: f32);
    glTexParameteri(target: u32, pname: u32, param: i32);
    glTexParameteriv(target: u32, pname: u32, params: *const i32);
    glTexSubImage1D(target: u32, level: i32, xoffset: i32, width: i32, format: u32, type_: u32, pixels: *const c_void);
    glTexSubImage2D(target: u32, level: i32, xoffset: i32, yoffset: i32, width: i32, height: i32, format: u32, type_: u32, pixels: *const c_void);
    glTexSubImage3D(target: u32, level: i32, xoffset: i32, yoffset: i32, zoffset: i32, width: i32, height: i32, depth: i32, format: u32, type_: u32, pixels: *const c_void);
    glUniform1f(location: i32, v0: f32);
    glUniform1fv(location: i32, count: i32, value: *const f32);
    glUniform1i(location: i32, v0: i32);
    glUniform2f(location: i32, v0: f32, v1: f32);
    glUniform3f(location: i32, v0: f32, v1: f32, v2: f32);
    glUniform4f(location: i32, v0: f32, v1: f32, v2: f32, v3: f32);
    glUniformMatrix4fv(location: i32, count: i32, transpose: u8, value: *const f32);
    glUseProgram(program: u32);
    glVertex2f(x: f32, y: f32);
    glVertex3f(x: f32, y: f32, z: f32);
    glVertexPointer(size: i32, type_: u32, stride: i32, pointer: *const c_void);
    glViewport(x: i32, y: i32, width: i32, height: i32);
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::ffi::{c_char, c_void};

    unsafe extern "C" fn any_symbol(_: *const c_char) -> *mut c_void {
        any_symbol as *mut c_void
    }

    unsafe extern "C" fn no_symbol(_: *const c_char) -> *mut c_void {
        core::ptr::null_mut()
    }

    #[test]
    fn table_loads_every_name() {
        let all = unsafe { GlFns::load(any_symbol) };
        assert_eq!(all.loaded(), GL_FN_NAMES.len());
        let none = unsafe { GlFns::load(no_symbol) };
        assert_eq!(none.loaded(), 0);
    }

    #[test]
    fn names_are_unique_gl_symbols() {
        assert_eq!(GL_FN_NAMES.len(), 135);
        let mut sorted = GL_FN_NAMES.to_vec();
        sorted.sort_unstable();
        sorted.dedup();
        assert_eq!(sorted.len(), GL_FN_NAMES.len());
        assert!(GL_FN_NAMES.iter().all(|n| n.starts_with("gl")));
    }

    #[test]
    fn constants() {
        assert_eq!(i64::from(GL_ACTIVE_TEXTURE), 34016);
        assert_eq!(i64::from(GL_ACTIVE_UNIFORMS), 35718);
        assert_eq!(i64::from(GL_ALL_ATTRIB_BITS), 1048575);
        assert_eq!(i64::from(GL_ALPHA), 6406);
        assert_eq!(i64::from(GL_ALPHA_TEST), 3008);
        assert_eq!(i64::from(GL_ARRAY_BUFFER), 34962);
        assert_eq!(i64::from(GL_ARRAY_BUFFER_ARB), 34962);
        assert_eq!(i64::from(GL_ARRAY_BUFFER_BINDING), 34964);
        assert_eq!(i64::from(GL_BACK), 1029);
        assert_eq!(i64::from(GL_BLEND), 3042);
        assert_eq!(i64::from(GL_CCW), 2305);
        assert_eq!(i64::from(GL_CLIP_PLANE0), 12288);
        assert_eq!(i64::from(GL_COLOR_ARRAY), 32886);
        assert_eq!(i64::from(GL_COLOR_ATTACHMENT0_EXT), 36064);
        assert_eq!(i64::from(GL_COLOR_BUFFER_BIT), 16384);
        assert_eq!(i64::from(GL_COMBINE_ARB), 34160);
        assert_eq!(i64::from(GL_COMPILE_STATUS), 35713);
        assert_eq!(i64::from(GL_CULL_FACE), 2884);
        assert_eq!(i64::from(GL_CURRENT_PROGRAM), 35725);
        assert_eq!(i64::from(GL_CW), 2304);
        assert_eq!(i64::from(GL_DEBUG_OUTPUT), 37600);
        assert_eq!(i64::from(GL_DEBUG_OUTPUT_SYNCHRONOUS), 33346);
        assert_eq!(i64::from(GL_DEBUG_TYPE_ERROR), 33356);
        assert_eq!(i64::from(GL_DEPTH_ATTACHMENT_EXT), 36096);
        assert_eq!(i64::from(GL_DEPTH_BUFFER_BIT), 256);
        assert_eq!(i64::from(GL_DEPTH_CLAMP), 34383);
        assert_eq!(i64::from(GL_DEPTH_COMPONENT), 6402);
        assert_eq!(i64::from(GL_DEPTH_TEST), 2929);
        assert_eq!(i64::from(GL_EDGE_FLAG_ARRAY), 32889);
        assert_eq!(i64::from(GL_ELEMENT_ARRAY_BUFFER_ARB), 34963);
        assert_eq!(i64::from(GL_FLOAT), 5126);
        assert_eq!(i64::from(GL_FRAGMENT_SHADER), 35632);
        assert_eq!(i64::from(GL_FRAMEBUFFER_BINDING), 36006);
        assert_eq!(i64::from(GL_FRAMEBUFFER_EXT), 36160);
        assert_eq!(i64::from(GL_FRONT), 1028);
        assert_eq!(i64::from(GL_GREEN), 6404);
        assert_eq!(i64::from(GL_INDEX_ARRAY), 32887);
        assert_eq!(i64::from(GL_INFO_LOG_LENGTH), 35716);
        assert_eq!(i64::from(GL_INVALID_OPERATION), 1282);
        assert_eq!(i64::from(GL_LEQUAL), 515);
        assert_eq!(i64::from(GL_LINEAR), 9729);
        assert_eq!(i64::from(GL_LINES), 1);
        assert_eq!(i64::from(GL_LINE_STRIP), 3);
        assert_eq!(i64::from(GL_LINK_STATUS), 35714);
        assert_eq!(i64::from(GL_MAX_CLIP_PLANES), 3378);
        assert_eq!(i64::from(GL_MAX_COLOR_ATTACHMENTS_EXT), 36063);
        assert_eq!(i64::from(GL_MAX_DRAW_BUFFERS), 34852);
        assert_eq!(i64::from(GL_MAX_TEXTURE_COORDS_ARB), 34929);
        assert_eq!(i64::from(GL_MAX_TEXTURE_IMAGE_UNITS_ARB), 34930);
        assert_eq!(i64::from(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT), 34047);
        assert_eq!(i64::from(GL_MODELVIEW), 5888);
        assert_eq!(i64::from(GL_MULTISAMPLE_ARB), 32925);
        assert_eq!(i64::from(GL_NEAREST), 9728);
        assert_eq!(i64::from(GL_NICEST), 4354);
        assert_eq!(i64::from(GL_NONE), 0);
        assert_eq!(i64::from(GL_NORMAL_ARRAY), 32885);
        assert_eq!(i64::from(GL_NO_ERROR), 0);
        assert_eq!(i64::from(GL_PERSPECTIVE_CORRECTION_HINT), 3152);
        assert_eq!(i64::from(GL_POLYGON_OFFSET_FILL), 32823);
        assert_eq!(i64::from(GL_PROJECTION), 5889);
        assert_eq!(i64::from(GL_QUADS), 7);
        assert_eq!(i64::from(GL_QUERY_RESULT), 34918);
        assert_eq!(i64::from(GL_QUERY_RESULT_ARB), 34918);
        assert_eq!(i64::from(GL_QUERY_RESULT_AVAILABLE_ARB), 34919);
        assert_eq!(i64::from(GL_RED), 6403);
        assert_eq!(i64::from(GL_RENDERBUFFER_EXT), 36161);
        assert_eq!(i64::from(GL_RENDERER), 7937);
        assert_eq!(i64::from(GL_REPLACE), 7681);
        assert_eq!(i64::from(GL_RGBA), 6408);
        assert_eq!(i64::from(GL_RGBA8), 32856);
        assert_eq!(i64::from(GL_SAMPLES_PASSED_ARB), 35092);
        assert_eq!(i64::from(GL_SCISSOR_TEST), 3089);
        assert_eq!(i64::from(GL_SECONDARY_COLOR_ARRAY), 33886);
        assert_eq!(i64::from(GL_SHADING_LANGUAGE_VERSION), 35724);
        assert_eq!(i64::from(GL_SMOOTH), 7425);
        assert_eq!(i64::from(GL_STENCIL_ATTACHMENT_EXT), 36128);
        assert_eq!(i64::from(GL_STENCIL_BITS), 3415);
        assert_eq!(i64::from(GL_STENCIL_BUFFER_BIT), 1024);
        assert_eq!(i64::from(GL_STENCIL_TEST), 2960);
        assert_eq!(i64::from(GL_STENCIL_TEST_TWO_SIDE_EXT), 35088);
        assert_eq!(i64::from(GL_TEXTURE), 5890);
        assert_eq!(i64::from(GL_TEXTURE0), 33984);
        assert_eq!(i64::from(GL_TEXTURE0_ARB), 33984);
        assert_eq!(i64::from(GL_TEXTURE1_ARB), 33985);
        assert_eq!(i64::from(GL_TEXTURE_2D), 3553);
        assert_eq!(i64::from(GL_TEXTURE_BINDING_2D), 32873);
        assert_eq!(i64::from(GL_TEXTURE_COMPARE_FUNC), 34893);
        assert_eq!(i64::from(GL_TEXTURE_COMPARE_MODE), 34892);
        assert_eq!(i64::from(GL_TEXTURE_COORD_ARRAY), 32888);
        assert_eq!(i64::from(GL_TEXTURE_ENV), 8960);
        assert_eq!(i64::from(GL_TEXTURE_ENV_COLOR), 8705);
        assert_eq!(i64::from(GL_TEXTURE_ENV_MODE), 8704);
        assert_eq!(i64::from(GL_TEXTURE_MAG_FILTER), 10240);
        assert_eq!(i64::from(GL_TEXTURE_MAX_ANISOTROPY_EXT), 34046);
        assert_eq!(i64::from(GL_TEXTURE_MAX_LEVEL), 33085);
        assert_eq!(i64::from(GL_TEXTURE_MIN_FILTER), 10241);
        assert_eq!(i64::from(GL_TEXTURE_SWIZZLE_R), 36418);
        assert_eq!(i64::from(GL_TEXTURE_SWIZZLE_RGBA), 36422);
        assert_eq!(i64::from(GL_TEXTURE_WRAP_R), 32882);
        assert_eq!(i64::from(GL_TEXTURE_WRAP_S), 10242);
        assert_eq!(i64::from(GL_TEXTURE_WRAP_T), 10243);
        assert_eq!(i64::from(GL_TIME_ELAPSED), 35007);
        assert_eq!(i64::from(GL_TRIANGLES), 4);
        assert_eq!(i64::from(GL_TRIANGLE_STRIP), 5);
        assert_eq!(i64::from(GL_UNSIGNED_BYTE), 5121);
        assert_eq!(i64::from(GL_UNSIGNED_INT), 5125);
        assert_eq!(i64::from(GL_VENDOR), 7936);
        assert_eq!(i64::from(GL_VERSION), 7938);
        assert_eq!(i64::from(GL_VERTEX_ARRAY), 32884);
        assert_eq!(i64::from(GL_VERTEX_SHADER), 35633);
        assert_eq!(i64::from(GL_EXTENSIONS), 7939);
        assert_eq!(i64::from(GL_TRUE), 1);
        assert_eq!(i64::from(GL_FALSE), 0);
    }
}
