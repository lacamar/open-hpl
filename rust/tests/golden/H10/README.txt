H10 (iEntity3D, cNode3D, cCamera) golden vectors.
h10_scenes.txt: HPL2/tests/SceneH10Tests.cpp dump, 30 entity/node hierarchy scenes (300 random ops each) and 15 camera scenes (400 ops), written by the C++ path (OHPL_IMPL=cpp).
  o = op, = = result (float bits), e = entity OnTransformUpdated / callback events in call order, h = state hash, ~ = C++-only output (bounding volume extents, frustum planes, unproject), not replayed.
  Identical for OHPL_IMPL=rust.
Replay: cargo test scene::tests_h10 (mock list shell around rust/src/scene/h10.rs, no C++).
Compare: scripts/h10-diff.sh <build-dir>
Record:  OHPL_H10_SEEDS=30 OHPL_IMPL=cpp OHPL_H10_DUMP=h10_scenes.txt SceneH10Tests.bin.aarch64
