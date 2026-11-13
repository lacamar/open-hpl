H12 (particle emitter data, emitter frames) golden vectors.
ps_emitters.txt: HPL2/tests/SceneH12Tests.cpp dump of 139 sampled emitters (every 22nd of 2880 in the 1606 real .ps files of Amnesia TDD, Rebirth, Bunker, SOMA, MfP, plus 8 synthetic edge cases), written by the C++ path (OHPL_IMPL=cpp).
  emitter <id>, a <attr>\t<value> = element attributes, d <name> <fields> = parsed cParticleEmitterData_UserData members (float bits), sim <seed> + f <n> / p <slot> <pos color size> = 24 emitter frames at 1/30 s, libc rand() seeded.
  Identical for OHPL_IMPL=rust. The full corpus (2880 emitters, 413 simulated) is compared by scripts/h12-diff.sh.
Replay: cargo test scene::tests_h12 (no C++, no game data).
Compare: scripts/h12-diff.sh <build-dir>   (reads Steam .ps files read-only)
Record:  scripts/h12-diff.sh <build-dir> record
