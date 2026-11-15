A4 (Amnesia enemies): golden vectors for rust/src/luxenemy/a4_*.rs.

golden.bin   324 entry calls recorded from the C++ shells: arguments, POD and enemy state before/after, every host call
             (registers in/out, state changes it caused), return value. Replayed by `cargo test luxenemy::a4` (a4_tests.rs).
Recorded while the C++ enemy base still existed (600 cases from the game data, 0 diffs against the original C++);
the differential harness was removed with the flip. a4_replay_gen.rs is the generated replay code.
