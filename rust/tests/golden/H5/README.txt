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

H5b (trait Gfx, Draw, GlGfx, ForeignGfx, FontTable)

scn_<seed>.trace  cRecordingLowLevelGraphics trace of the seeded Gfx scenario (rust/src/graphics/h5b_scn.rs,
                  3 rounds, every trait operation), driven through the C++ facade RustGfxFacade.cpp
scn_<seed>.res    the scenario's query log (values a caller branches on)
imm_<seed>.txt    primitive stream + batch flush hashes of LowLevelGraphicsImmediate.inl (the real .inl built
                  against a recording class), 100 random draw/batch calls
font_<seed>.txt   iFontData glyph table: GetLength / GetWordWrapRows with a seeded font (hex float bits)
cargo test graphics::tests_h5b, tests_h5b_imm, tests_h5b_font replay them against TraceGfx / the Draw
defaults / FontTable without the C++. OHPL_IMPL=cpp and =rust give identical font output (harness).

Record (build dir with `make TraceRecorderTests`, rust staticlib built with `cargo build --release`):
  harness/h5b_build.sh <repo> <build dir> <libohpl.a> <out> facade h5b_run   (also h5b_imm; h5b_font takes the
        extra source HPL2/core/sources/graphics/FontData.cpp as 7th argument when the archive predates it)
  H5B_SEED=<n> H5B_ROUNDS=3 H5B_OUT=<out>/s<n> <out>/h5b_run   -> .trace, .log (stored here as .res)
  H5B_SEED=<n> H5B_CALLS=100 H5B_OUT=<file> <out>/h5b_imm
  H5B_SEED=<n> OHPL_IMPL=cpp|rust <out>/h5b_font
  cargo test graphics::tests_h5b::dump -- --ignored   (H5B_DUMP=<dir>) writes the Rust side for diffing
