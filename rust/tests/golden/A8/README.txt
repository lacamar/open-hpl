A8 Amnesia save tables behind OHPL_IMPL=rust.
tables.txt: field tables of the 112 Amnesia kBeginSerialize classes (name, parent, field, class, type, main, array size).
Recorded from the running C++ path: OHPL_A8_TABLE_DUMP=<file> makes the Rust flush append every C++ table it sees for a class
it owns; sorted by class name that is tables.txt (verified equal, 112 classes, order-independent compare).
Replay: cargo test luxsave::a8 (Rust tables == golden, refine merge/fallback, A5 prop state <-> save-data round trip).
Live: scripts/amnesia-save-roundtrip.sh equivalent (load A.sav, save B, load B, save C) with OHPL_IMPL unset vs rust:
B.sav byte-equal except the time-varying mColor/mDiffuseColor/mfCurrentMusicTime values, which differ identically between two cpp runs.
