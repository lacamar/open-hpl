A3: player states (LuxPlayerState*).

golden.bin: u32 words, every ohpl_a3_* entry of 100 harness cases: state before (diff), args, host calls in order (op, io in/out),
result, state after (diff). Pointers are harness object ids (0xABCD0000|id).
Replayed by rust/src/luxplayer/a3_tests.rs (exact in release, NaN sign/payload tolerant at opt-level 0).
Recorded while the C++ states still existed (seeds 1-5 x 12000 cases, about 1.9M steps, 0 diffs against the original C++);
the differential harness was removed with the flip.
