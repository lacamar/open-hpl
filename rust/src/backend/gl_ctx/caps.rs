//! The `GLEW_*` flags `cLowLevelGraphicsSDL` reads, derived from `glGetString(GL_EXTENSIONS)`.
//! GLEW sets a flag when the extension is listed and every entry point of it resolves; the
//! entry point lists below are GLEW's.

use std::fmt::Write;

type Has<'a> = &'a dyn Fn(&str) -> bool;

macro_rules! caps {
    ($($field:ident => $ext:literal $(: $procs:expr)?;)*) => {
        #[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
        pub struct Caps {
            $(pub $field: bool,)*
        }

        impl Caps {
            /// `has_proc` answers whether the loader resolves a function name.
            pub fn from_extensions(ext: &str, has_proc: Has) -> Self {
                let listed = |name: &str| ext.split_ascii_whitespace().any(|e| e == name);
                Self {
                    $($field: listed(concat!("GL_", $ext)) $(&& ($procs)(has_proc))?,)*
                }
            }

            /// `GLCAPS ARB_multitexture=1 ...`, the same text on both paths for diffing.
            pub fn line(&self) -> String {
                let mut s = String::from("GLCAPS");
                $(let _ = write!(s, concat!(" ", $ext, "={}"), u8::from(self.$field));)*
                s
            }
        }
    };
}

fn all(names: &[&str], has: Has) -> bool {
    names.iter().all(|n| has(n))
}

fn multitexture(has: Has) -> bool {
    let mut ok = all(&["glActiveTextureARB", "glClientActiveTextureARB"], has);
    for n in 1..=4 {
        for t in ["d", "dv", "f", "fv", "i", "iv", "s", "sv"] {
            ok &= has(&format!("glMultiTexCoord{n}{t}ARB"));
        }
    }
    ok
}

fn gpu_shader4(has: Has) -> bool {
    let mut ok = all(
        &[
            "glBindFragDataLocationEXT",
            "glGetFragDataLocationEXT",
            "glGetUniformuivEXT",
            "glGetVertexAttribIivEXT",
            "glGetVertexAttribIuivEXT",
            "glVertexAttribIPointerEXT",
            "glVertexAttribI4bvEXT",
            "glVertexAttribI4svEXT",
            "glVertexAttribI4ubvEXT",
            "glVertexAttribI4usvEXT",
        ],
        has,
    );
    for n in 1..=4 {
        for t in ["ui", "uiv"] {
            ok &= has(&format!("glUniform{n}{t}EXT"));
        }
        for t in ["i", "iv", "ui", "uiv"] {
            ok &= has(&format!("glVertexAttribI{n}{t}EXT"));
        }
    }
    ok
}

caps! {
    multitexture => "ARB_multitexture": multitexture;
    vertex_buffer_object => "ARB_vertex_buffer_object": |h: Has| all(&[
        "glBindBufferARB", "glBufferDataARB", "glBufferSubDataARB", "glDeleteBuffersARB",
        "glGenBuffersARB", "glGetBufferParameterivARB", "glGetBufferPointervARB",
        "glGetBufferSubDataARB", "glIsBufferARB", "glMapBufferARB", "glUnmapBufferARB",
    ], h);
    stencil_two_side => "EXT_stencil_two_side": |h: Has| all(&["glActiveStencilFaceEXT"], h);
    separate_stencil => "ATI_separate_stencil": |h: Has| all(&[
        "glStencilFuncSeparateATI", "glStencilOpSeparateATI",
    ], h);
    anisotropic => "EXT_texture_filter_anisotropic";
    multisample => "ARB_multisample": |h: Has| all(&["glSampleCoverageARB"], h);
    texture_compression => "ARB_texture_compression": |h: Has| all(&[
        "glCompressedTexImage1DARB", "glCompressedTexImage2DARB", "glCompressedTexImage3DARB",
        "glCompressedTexSubImage1DARB", "glCompressedTexSubImage2DARB",
        "glCompressedTexSubImage3DARB", "glGetCompressedTexImageARB",
    ], h);
    s3tc => "EXT_texture_compression_s3tc";
    generate_mipmap => "SGIS_generate_mipmap";
    framebuffer_object => "EXT_framebuffer_object": |h: Has| all(&[
        "glBindFramebufferEXT", "glBindRenderbufferEXT", "glCheckFramebufferStatusEXT",
        "glDeleteFramebuffersEXT", "glDeleteRenderbuffersEXT", "glFramebufferRenderbufferEXT",
        "glFramebufferTexture1DEXT", "glFramebufferTexture2DEXT", "glFramebufferTexture3DEXT",
        "glGenFramebuffersEXT", "glGenRenderbuffersEXT", "glGenerateMipmapEXT",
        "glGetFramebufferAttachmentParameterivEXT", "glGetRenderbufferParameterivEXT",
        "glIsFramebufferEXT", "glIsRenderbufferEXT", "glRenderbufferStorageEXT",
    ], h);
    packed_depth_stencil => "EXT_packed_depth_stencil";
    texture_float => "ARB_texture_float";
    fragment_program => "ARB_fragment_program";
    fragment_shader => "ARB_fragment_shader";
    nv_vertex_program3 => "NV_vertex_program3";
    ati_shader_texture_lod => "ATI_shader_texture_lod";
    gpu_shader4 => "EXT_gpu_shader4": gpu_shader4;
    ati_fragment_shader => "ATI_fragment_shader": |h: Has| all(&[
        "glAlphaFragmentOp1ATI", "glAlphaFragmentOp2ATI", "glAlphaFragmentOp3ATI",
        "glBeginFragmentShaderATI", "glBindFragmentShaderATI", "glColorFragmentOp1ATI",
        "glColorFragmentOp2ATI", "glColorFragmentOp3ATI", "glDeleteFragmentShaderATI",
        "glEndFragmentShaderATI", "glGenFragmentShadersATI", "glPassTexCoordATI",
        "glSampleMapATI", "glSetFragmentShaderConstantATI",
    ], h);
    blend_func_separate => "EXT_blend_func_separate": |h: Has| all(&["glBlendFuncSeparateEXT"], h);
    occlusion_query => "ARB_occlusion_query": |h: Has| all(&[
        "glBeginQueryARB", "glDeleteQueriesARB", "glEndQueryARB", "glGenQueriesARB",
        "glGetQueryObjectivARB", "glGetQueryObjectuivARB", "glGetQueryivARB", "glIsQueryARB",
    ], h);
}
