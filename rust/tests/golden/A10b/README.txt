A10b: LuxPreMenu, LuxCredits, LuxDemoEnd, LuxLoadScreenHandler, LuxProgressLogHandler.

*.txt.z: zlib-compressed OHPL_A10B_DUMP traces recorded from the C++ classes running Amnesia (headless,
traced step 1/60 s) before they were ported. Line format: `ev <event> args` is what the engine did to the
class, other lines are the engine calls the class made, `-> result` the answer. Pointers are numbered by first
appearance, floats are `f<hex bits>`.
  boot-amnesia / boot-amfp: scripts/a10b-scenario.sh boot (pre-menu sections to the main menu)
  play-amnesia: scripts/a10b-scenario.sh play (map change with load screen, credits)
Recorded with commit f7e73db plus a temporary trace patch of the five .cpp files (not committed).
Replay: luxmenu2::tests_a10b (cargo test) feeds the events to the Rust classes with a host that answers from the
trace and requires the same lines back. dump-diff.py compares two live dumps (draw frames are keyed by update
count: the engine's draw/update ratio follows the wall clock).
The demo end is not built into the shipped games (LUX_DEMO_VERSION), so it has a scripted unit test only.
