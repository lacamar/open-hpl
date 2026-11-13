H14 (CharacterBody, PhysicsRope, VerletParticle, PhysicsController) golden vectors.
h14_scenes.txt: PhysicsH14Tests dump (HPL2/tests/PhysicsH14Scenes.h), 17 scenes x 480-600 steps (push_2d, push_3d_impulse, push_custom_gravity, rope_motor added in pass 2), written by the C++ path before it was deleted.
  scene lines: rolling FNV-1a over every step's character, body, camera, entity and rope-particle state; indented lines: final positions (float bits).
  Identical to the pre-Rust-port C++ (HPL2 at 941bef3^) and to the Rust path.
h14_vectors.txt: every ohpl_h14_* call of a PhysicsH14Tests run (inputs, host answers in call order, outputs) as hex, replayed by rust/src/physics/tests_h14.rs.
Compare: scripts/h14-diff.sh <build-dir>
Record:  OHPL_H14_VEC=h14_vectors.txt PhysicsH14Tests.bin.aarch64
