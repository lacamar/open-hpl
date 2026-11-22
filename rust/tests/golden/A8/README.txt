A8 Amnesia save tables.
tables.txt: field tables of the 115 Amnesia/engine kBeginSerialize classes (name, parent, field, class, type, main, array size).
Recorded from the running C++ path (OHPL_A8_TABLE_DUMP, since removed); 112 at A8, +3 engine classes (cAreaEntity, cStartPosEntity, cScriptVar) at SR.
Replay: cargo test luxsave::a8 (Rust tables == golden, gcc layouts, A5 prop state <-> save-data round trip).
Layouts: rust/src/luxsave/layout_gcc.txt (SR; gcc offsets of the classes whose C++ tables SR deleted).
Live: scripts/amnesia-save-roundtrip.sh (load A.sav, save B, load B, save C): 0 structure diffs.
