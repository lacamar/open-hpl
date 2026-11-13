core.bin: 3000 recorded Rust-driver calls (vitals, spin_head, update_camera/head_spin/terror/focus_alpha): Core before, host traffic, Core after.
Recorded: harness/build.sh new; A2_REC=core.bin OHPL_IMPL=rust ./harness_new cfg.txt 0 1500 40
C++ vs Rust (OHPL_IMPL unset vs rust), cases 0-7000 x 30-60 steps (300k steps): 0 diffs. Replay: cargo test a2_core_replay.
