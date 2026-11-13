L3 golden vectors: cSDLTexture, iVertexBufferOpenGL (VBO, array), cFrameBufferGL, cDepthStencilBufferGL,
cGLSLShader, cGLSLProgram (GL calls and resource ownership in rust/src/backend/gl_res).

scripts/<name>.txt   seeded random scripts for harness/l3_trace.cpp (gen_script.py); "# sets: C" = fake extension set
trace_<name>_<set>.txt  GL call trace (gl*/glu* lines) of the former C++ path for the script (set A full, B limited, C = A + secondary color)
ffi_<name>_<set>.txt    shell-to-Rust call log (ohpl_gl{tex,vb,fb,dsb,sh,prog}_*, ohpl_glctx_get_caps) of the Rust path
                        handles are H<n>, pointers label+offset of "@range" buffers, strings hex
cargo test replays every ffi log against the recording fake GL and compares with the trace (no C++)

Record (needs a build dir of this tree, harness in ../L2/harness):
  gen_script.py <scripts> 10
  build_l3.sh <repo> <build dir> <out>        builds <out>/l3_trace, writes <out>/replay_gen.rs (copy to src/backend/gl_res/tests/)
  compare_l3.sh <out> <scripts>               Rust vs the committed golden traces incl. returned values
  compare_noglew.sh <out> <scripts>           with OHPL_NO_GLEW=1 (every GLEW pointer null)
  record_l3.py <out> <scripts> rust/tests/golden/L3 rust/src/backend/gl_res/tests
