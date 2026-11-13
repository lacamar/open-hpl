A3: player states (LuxPlayerState*).

golden.bin: u32 words, every ohpl_a3_* entry of 100 harness cases recorded from the Rust side: state before (diff), args,
host calls in order (op, io in/out), result, state after (diff). Pointers are harness object ids (0xABCD0000|id).
Replayed without C++ by rust/src/luxplayer/a3_tests.rs (exact in release, NaN sign/payload tolerant at opt-level 0).

harness/: C++ differential. orig = the C++ of A3_BASE (default rust-port-batch3: T0 kernels, OHPL_IMPL unset),
new = this tree with OHPL_IMPL=rust, state function texts extracted verbatim (gen.py) against fake engine classes (prelude.h).
Per step a hash of the call log (every fake engine call, bit patterns), results and state dumps (incl. PID buffers).

  cd rust && CARGO_TARGET_DIR=$A3/cargo cargo build --release --offline
  tests/golden/A3/harness/build.sh both        # A3 defaults to ~/.cache/open-hpl/port-a3
  $A3/harness/harness_orig 12000 <seed> > o.txt; $A3/harness/harness_new 12000 <seed> > n.txt; cmp o.txt n.txt
  A3_GOLDEN=tests/golden/A3/golden.bin $A3/harness/harness_new 100 5     # regenerate

Last run: seeds 1-5 x 12000 cases (about 1.9M steps), 0 diffs.
