corpus_soma.txt: seeded calls of the SOMA value-type, cMath and cString natives, recorded from the C++ natives before the flip.
One line per call: declaration, call index, "in" objects/primitives (hex), "out" return value and the objects after the call.
Replayed by somascript::s2_tests::corpus_matches_golden through mock frames (numeric and POD natives; string, random and easing lines are skipped there).
Re-record from the Rust natives: OHPL_S2_CORPUS=<out.txt> OPENHPL_SOMA_GAME_DIR=<scratch> OPENHPL_SOMA_SCRIPT_CHECK=<x.json>
  OPENHPL_SOMA_SCRIPT_API=soma/data/script_api.txt OPENHPL_SOMA_SCRIPT_CHECK_FILE=/nonexistent Soma.bin.aarch64
Before the flip the cpp and rust runs were byte-identical (soma/rebirth/bunker 3714/3756/3756 calls; 49520 calls over the three games).
