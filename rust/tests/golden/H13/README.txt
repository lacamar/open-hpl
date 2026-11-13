H13 (cWorld registry, .map loader) golden vectors.
reg_plan.txt: HPL2/tests/SceneH13Tests.cpp dump recorded from the C++ semantics (std::list / std::map oracle, original combine loops).
  reg <seed>: ent <i> <name> <id> pool, then op lines "<A|R|D|S|C|N|NL|I|X|F|T|G|Z> <kind> <ent> = <result>" with "s <kind> <n>: <ent...>" list dumps.
  walk <seed>: OhplH13Walk cursor walks ("w: <ent>[x]..." erase-while-walking / append, "all", "drops:" destroy order, "size") recorded from std::list; in-process equal for the Rust cursors (cargo replay skips them).
  plan <n>: per item "m <material> <shadow> <combine> b <physmat> <shadow> <char> <collides> <indexnum>", then pm/pb run lists "first-last".
  6 seeds x 400 ops, 400 plan cases. Identical for the Rust registry (the harness compares both in process).
maps.txt: one line per *.map of Amnesia TDD (39) and Machine for Pigs (17): HplMapParse (tinyxml document, C ABI builder) trace event count and FNV-1a hash.
Replay: cargo test scene::tests_h13 (registry and plans need no data; maps replay only when the Steam data is installed).
Compare: scripts/h13-diff.sh <build-dir>   (reads Steam .map files read-only)   Record: scripts/h13-diff.sh <build-dir> record
World load A/B: scripts/h13-mapdump.sh <map> (OHPL_H13_MAPDUMP written by cWorld::DumpH13 at the end of LoadWorld).
