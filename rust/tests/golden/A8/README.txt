A8 Amnesia save tables.
tables.txt: field tables of the 112 Amnesia kBeginSerialize classes (name, parent, field, class, type, main, array size).
Recorded from the running C++ path: OHPL_A8_TABLE_DUMP=<file> makes the Rust flush append every C++ table it sees for a class
it owns; sorted by class name that is tables.txt (112 classes, order-independent compare).
Replay: cargo test luxsave::a8 (Rust tables == golden, refine merge/fallback, A5 prop state <-> save-data round trip).
Live: scripts/amnesia-save-roundtrip.sh (load A.sav, save B, load B, save C): 0 structure diffs.
