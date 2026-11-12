H15 body/joint bookkeeping (OHPL_IMPL=rust) is checked against the L4 vectors: ../L4/newton_scenes.txt.
scripts/l4-diff.sh: cpp vs rust dumps identical, 11 scenes x 600 steps, equal to the L4 golden.
Unit replay of book vs host slots: rust/src/physics/tests.rs (body_book_replaces_host_slots, joint_book_keeps_step_counters).
