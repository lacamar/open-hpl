H12 (particle emitter data, emitter frames) golden vectors.
ps_emitters.txt: HPL2/tests/SceneH12Tests.cpp dump of 139 sampled emitters (every 22nd of 2880 in the 1606 real .ps files of Amnesia TDD, Rebirth, Bunker, SOMA, MfP, plus 8 synthetic edge cases), written by the C++ path (OHPL_IMPL=cpp).
  emitter <id>, a <attr>\t<value> = element attributes, d <name> <fields> = parsed cParticleEmitterData_UserData members (float bits), sim <seed> + f <n> / p <slot> <pos color size> = 24 emitter frames at 1/30 s, libc rand() seeded.
  Identical for OHPL_IMPL=rust. The full corpus (2880 emitters, 413 simulated) is compared by scripts/h12-diff.sh.
Replay: cargo test scene::tests_h12 (no C++, no game data).
Compare: scripts/h12-diff.sh <build-dir>   (reads Steam .ps files read-only)
Record:  scripts/h12-diff.sh <build-dir> record

Pass 2 (particle system lifecycle, emitter shell, billboard, beam, rope, flare, sound-entity routing):
dec.txt: "dec <probe> <inputs> -> <result>", the original C++ expressions of HPL2/tests/SceneH12bTests.cpp over input grids (float bits hex); the Rust gates in h12b_psys.rs / h12b_ent.rs must give the same string. Replay: cargo test scene::tests_h12b.
Object scenes (cParticleSystem over 536 real .ps files x 40 frames, cBillboard, cBeam, cLensFlare, RecordingLowLevelGraphics trace hash) are compared cpp vs rust by scripts/h12b-diff.sh <build-dir> [record]; they are not recorded (3.8 MB).
