corpus_soma.txt: seeded calls of the SOMA value-type, cMath and cString natives, recorded from the C++ natives
(OHPL_IMPL=cpp). One line per call: declaration, call index, "in" objects/primitives (hex), "out" return value
and the objects after the call. Replayed by somascript::s2_tests::corpus_matches_cpp through mock frames
(numeric and POD natives; string, random and easing lines are skipped there).
Recording: OHPL_IMPL=cpp OHPL_S2_CORPUS=<out.txt> OPENHPL_SOMA_GAME_DIR=<scratch> OPENHPL_SOMA_SCRIPT_CHECK=<x.json>
  OPENHPL_SOMA_SCRIPT_API=soma/data/script_api.txt OPENHPL_SOMA_SCRIPT_CHECK_FILE=/nonexistent Soma.bin.aarch64
Live differential: the same run with OHPL_IMPL=rust is byte-identical (soma/rebirth/bunker 3714/3756/3756 calls;
80 calls per function: 49520 calls identical). Declaration dumps (OHPL_S2_DECL_DUMP) are identical on all three games.
