A4 (Amnesia enemies): golden vectors for rust/src/luxenemy/a4_*.rs.

golden.bin   324 entry calls recorded from the real C++ shells (OHPL_IMPL=rust) in harness/: arguments, POD and
             enemy state before/after, every host call (registers in/out, state changes it caused), return value.
             Replayed by `cargo test luxenemy::a4` (a4_tests.rs) without the C++.
harness/     differential driver. build.sh builds three binaries: harness_orig (C++ at rust-port-batch3),
             harness_new (this tree) and harness_rec (recorder; genrec.py also writes a4_replay_gen.rs).
             run.sh diffs orig vs new (OHPL_IMPL=rust) over corpus.txt.gz (600 cases from the game data).
Regenerate:  CARGO_TARGET_DIR=$HOME/.cache/open-hpl/port-a4/cargo cargo build --release in rust/, bash harness/build.sh,
             bash harness/run.sh, then A4_REC=<path>/golden.bin OHPL_IMPL=rust $HOME/.cache/open-hpl/port-a4/harness/harness_rec corpus.txt 0 150
