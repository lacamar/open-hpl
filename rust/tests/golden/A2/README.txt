core.bin: 3000 recorded Rust-driver calls (vitals, spin_head, update_camera/head_spin/terror/focus_alpha): Core before, host traffic, Core after.
Recorded: harness/build.sh new; A2_REC=core.bin OHPL_IMPL=rust ./harness_new cfg.txt 0 1500 40
C++ vs Rust (OHPL_IMPL unset vs rust), cases 0-7000 x 30-60 steps (300k steps): 0 diffs. Replay: cargo test a2_core_replay.
helpers.bin: Rust-driver calls of the helper state (collapse, camdir, hurt, lookat, flashback, light level, darkness, lantern): State before, host traffic (query/op/fill), State after, return value. 25 call kinds; calls whose host re-enters a driver are left out (the differential covers them).
Recorded: harness/build.sh new; A2_REC_B=helpers.bin OHPL_IMPL=rust ./harness_new cfg.txt 0 7000 60
Differential (harness_orig = rust-port-batch3 bodies, harness_new OHPL_IMPL unset, harness_new OHPL_IMPL=rust), cases 0-7000 x 60 steps (420k steps): 0 diffs. Replay: cargo test a2b_helpers_replay.
