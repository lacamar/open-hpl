H5 golden vectors: cFrameBitmap (atlas placement, border blits, reorganisation) and
cProgramComboManager (program names, shader variable lists), recorded from the C++ path.

atlas_<seed>.txt        ops ("-o" lines) with the results the C++ printed after each
trace_atlas_<seed>.txt  CreateFromBitmap lines of the recording low-level graphics (texture uploads, hashes)
combo_<seed>.txt        ops and results ("create" = CreateShader arguments, vars as the sorted map)
atlas seeds 110 128 136 154 188 hit the reorganisation fallback (re-add in previous order)
source pixels of add n: byte k = n*131 + k*7 + (k>>8)*13 (mod 256), RGBA
cargo test (graphics::tests_h5) replays every file against the Rust code without the C++.

Record (needs a build dir of this tree with `make TraceRecorderTests`):
  harness/build.sh <repo> <build dir> <out>      builds <out>/h5_trace
  harness/compare.sh <out> 200                   OHPL_IMPL=cpp vs rust, stdout + GPU trace, per seed
  copy <out>/atlas.<seed>.cpp.out, combo.<seed>.cpp.out and the CreateFromBitmap lines of atlas.<seed>.cpp.err
