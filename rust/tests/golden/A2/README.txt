A2 player core and helpers: frozen goldens, replayed by Rust only (no C++ path left).
core.bin: 3000 recorded Rust-driver calls (vitals, spin_head, update_camera/head_spin/terror/focus_alpha): Core before, host traffic, Core after. Replay: cargo test a2_core_replay.
helpers.bin: Rust-driver calls of the helper state (collapse, camdir, hurt, lookat, flashback, light level, darkness, lantern): State before, host traffic (query/op/fill), State after, return value. 25 call kinds. Replay: cargo test a2b_helpers_replay.
Recorded from the C++ shells while both paths existed (cases 0-7000 x 30-60 steps, 0 diffs against the original C++); the C++ differential harness was removed with the flip.
