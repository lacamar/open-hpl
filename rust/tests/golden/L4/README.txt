Newton backend (L4) golden vectors, recorded from the former C++ path.
newton_scenes.txt: PhysicsNewtonTests scenes (HPL2/tests/PhysicsNewtonScenes.h), 600 steps at 1/60, bit-exact.
  scene/end lines: rolling FNV-1a over every step's body matrix, velocities and sleep state, per scene.
  indented lines: body matrices (raw float bits) every 100 steps and at the end.
  hinge lines: 64 input floats, then the hinge row geometry of cPhysicsJointHingeNewton::SubmitConstraints.
Record:  OHPL_L4_DUMP=out.txt OHPL_L4_VERBOSE=1 PhysicsNewtonTests.bin.aarch64
Compare: scripts/l4-diff.sh (runs the binary on the Rust backend, cmp's the dump against newton_scenes.txt).
