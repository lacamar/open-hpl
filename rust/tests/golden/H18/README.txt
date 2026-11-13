H18 golden vectors: cGuiSet focus/attention/default-nav stacks, tab order, window order and input
dispatch (OnMouse*/OnKey*/OnUI*), recorded from the C++ path with dummy widgets and a null skin.

set_<seed>.txt    h18_set stdout. "w" lines = widget setup (parent, flags, focus nav, global pos/size as f32 hex),
                  "op" = one scripted call, "ev" = widget callback fired (message, data, result),
                  "=" = state after the op (focus, attention, default nav, tab widget, tooltip, mouse, over flags,
                  list order, global z). cargo test (gui::tests_set) replays every file through set::Core with a
                  model of the dummy widgets and compares every line, without the C++.
skin_<name>.txt   Rust skin parse of each installed .skin (7 files: Amnesia TDD x2, MfP x2, Rebirth, Bunker, SOMA),
                  compact dump of attributes, fonts and gfx. gui::tests_set re-parses the real files when present.
                  Regenerate: H18_GEN_SKIN=1 cargo test --lib gui::tests_set
draw_<seed>.txt   h18_draw stdout: RecordingLowLevelGraphics trace of cGuiSet::DrawGfx / clip regions / Render
                  (textures, materials, blend, scissor, quad batches). Compared OHPL_IMPL=cpp vs rust only
                  (no cargo replay: the Rust draw core is exercised through the C++ host).

Record (needs a build dir of this tree with `make TraceRecorderTests`):
  harness/build.sh <repo> <build dir> <out>     builds <out>/h18_set and <out>/h18_draw
  harness/compare.sh <out> 200                  OHPL_IMPL=cpp vs rust, stdout per seed, both harnesses
  H18_SEED=<n> OHPL_IMPL=cpp <out>/h18_set > set_<nnn>.txt   (seeds 1..24 committed)
  H18_SEED=<n> OHPL_IMPL=cpp <out>/h18_draw > draw_<nnn>.txt (seeds 1..8 committed)
