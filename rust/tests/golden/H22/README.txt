H22 golden vectors: cEngine loop/init order, headless control server, DevHud draw sequence.

hc_*.txt     h22_hc transcript of cHeadlessControlServer (recorded from the original C++) over a fake cEngine: the script lines
             (">" send, ">>" raw chunk, settle, pump n, frames n, log, set, shader, loading) interleaved with
             what they caused ("<" reply line, "ev" SDL event, "inj" injected text, "state" engine effect).
             4 hand-built tables (every command, edge values, input, wait_frames/loading, parser torture)
             and 24 seeded fuzz streams (gen_hc.py). cargo test (engine::h22_tests) replays the script lines
             through the Rust server on a real unix socket against a mock host and compares every line.
run_<seed>.txt  trace of the original cEngine::Run loop (recorded before it was deleted)
             over a seeded host: every host operation with arguments and result, then the final Loop state.
             Replayed by engine::h22_tests::run_traces_match_cpp through eng::run and the C-ABI op table.
hud_<seed>.txt  trace of the original cDevHud::Render/RenderBugReport (recorded before they were deleted): the case line,
             then every GUI-set/font operation (SET_VIRTUAL, DRAW_GFX, DRAW_FONT, FONT_LENGTH, ...).
steps.txt    init/exit order of cEngine (recorded from the C++ source text before it was deleted); checked against
             eng::init_steps()/exit_steps().

Record (needs a build dir of this tree with `make TraceRecorderTests`):
  harness/build.sh <repo> <build dir> <out>        builds <out>/h22_hc
  harness/gen_hc.py <dir> 24                       scripts hc_*.script
  harness/verify_hc.sh <out> <script dir>          real cHeadlessControlServer transcripts against hc_*.txt
The key table in h22_tests.rs and the float rounding of pointer coordinates (sdl3_int) come from the installed
SDL2 compat layer on SDL3.
