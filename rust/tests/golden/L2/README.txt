L2 golden vectors: cLowLevelGraphicsSDL, cOcclusionQueryOGL (GL context, window, caps).

trace_{A,B}.txt  GL and SDL call trace of the C++ path for the calls the Rust context replaces
                 (A: full extension list, B: ATI separate stencil, no two-side stencil/blend separate/multisample)
ffi_{A,B}.txt    ohpl_glctx_* / ohpl_glq_* calls the C++ shell makes for the same script ("@gen n" sets the id counter)
caps_{A,B}.txt   GLCAPS line and GetCaps values from the GLEW flags of the C++ path
harness/         fake GL/SDL, script and generators; cargo test replays ffi_* against trace_* without C++

Record (needs a build dir of this tree):
  harness/build.sh <repo> <build dir> <out>      builds <out>/l2_trace, writes <out>/fakegl.rs and <out>/replay_gen.rs (copies live in src/backend/gl_ctx/tests/)
  harness/compare.sh <out>                       OHPL_IMPL=cpp vs rust full traces, prints compared/differing line counts
  harness/record.py <out> rust/tests/golden/L2
