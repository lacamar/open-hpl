H9 golden vectors: cRendererDeferred::LoadData fog and light program-combo setup (feature tables, default defines, variable ids). Recorded from the C++ path.

combos.txt  cProgramComboManager state after OHPL_IMPL=cpp setup: 2 fog configs (packed depth on/off) and 504 light configs
            (shadow quality x 32/64-bit G-buffer x 3/4 targets x 7 jitter sizes x 6 jitter sample counts).
            "combos" = combination count (C++ only, from ohpl_combo_setup), "mode" = name and shader files, "f" = feature (stage mask, needed flags),
            "d" = default define (sorted, floats as %f), "v" = variable name and id in registration order.
Equal for OHPL_IMPL=cpp and rust (506 configs, 0 diffs); cargo replays it (render::h9_tests) without the C++.

Not covered: the rest of LoadData (textures, frame buffers, other programs) and the per-frame executor stay C++ calls on iLowLevelGraphics;
per-frame decisions were already Rust plans (G6) and are unconditional, so there is no cpp/rust frame trace difference to record.

Record (needs a build dir of this tree with `make HPL2`):
  harness/gen.sh <build dir> h9_gen.cpp <out>/h9_gen
  harness/compare.sh <out>/h9_gen rust/tests/golden/H9
