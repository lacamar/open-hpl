H22 golden vectors: cEngine loop/init order, headless control server, DevHud draw sequence.

hc_*.txt     h22_hc transcript of the original C++ cHeadlessControlServer over a fake cEngine: the script lines
             (">" send, ">>" raw chunk, settle, pump n, frames n, log, set, shader, loading) interleaved with
             what they caused ("<" reply line, "ev" SDL event, "inj" injected text, "state" engine effect).
             4 hand-built tables (every command, edge values, input, wait_frames/loading, parser torture)
             and 24 seeded fuzz streams (gen_hc.py). cargo test (engine::h22_tests) replays the script lines
             through the Rust server on a real unix socket against a mock host and compares every line.
run_<seed>.txt  h22_run trace of the original cEngine::Run loop (extracted from Engine.cpp by extract_run.py)
             over a seeded host: every host operation with arguments and result, then the final Loop state.
             Replayed by engine::h22_tests::run_traces_match_cpp through eng::run and the C-ABI op table.
hud_<seed>.txt  h22_hud trace of the original cDevHud::Render/RenderBugReport (extract_hud.py): the case line,
             then every GUI-set/font operation (SET_VIRTUAL, DRAW_GFX, DRAW_FONT, FONT_LENGTH, ...).
steps.txt    init/exit order of cEngine (gen_steps.py reads it from the C++ source text); checked against
             eng::init_steps()/exit_steps().

Record (needs a build dir of this tree with `make TraceRecorderTests`):
  harness/build.sh <repo> <build dir> <out>        builds <out>/h22_hc, h22_run, h22_hud
  harness/gen_hc.py <dir> 24                       scripts hc_*.script
  harness/compare_hc.sh <out> <script dir>         OHPL_IMPL=cpp vs rust transcripts (REC=<dir> keeps the cpp ones)
  harness/compare_run.sh <out> 300                 original loop vs ohpl_h22_engine_run (REC=<dir> REC_N=20)
  harness/compare_hud.sh <out> 500                 original DevHud::Render vs ohpl_h22_hud_render (REC=<dir> REC_N=60)
  harness/gen_steps.py HPL2/core/sources/engine/Engine.cpp > steps.txt
The key table in h22_tests.rs and the float rounding of pointer coordinates (sdl3_int) come from the installed
SDL2 compat layer on SDL3.
