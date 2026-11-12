H14 (CharacterBody, PhysicsRope, VerletParticle, PhysicsController) golden vectors.
h14_scenes.txt: PhysicsH14Tests dump (HPL2/tests/PhysicsH14Scenes.h), 13 scenes x 480-600 steps, written by the C++ path (OHPL_IMPL=cpp).
  scene lines: rolling FNV-1a over every step's character, body, camera, entity and rope-particle state; indented lines: final positions (float bits).
  Identical to the pre-Rust-port C++ (HPL2 at 941bef3^) and to OHPL_IMPL=rust.
h14_vectors.txt: every ohpl_h14_* call of an OHPL_IMPL=rust run (inputs, host answers in call order, outputs) as hex, replayed by rust/src/physics/tests_h14.rs.
Compare: scripts/h14-diff.sh <build-dir>
Record:  OHPL_IMPL=rust OHPL_H14_VEC=h14_vectors.txt PhysicsH14Tests.bin.aarch64
