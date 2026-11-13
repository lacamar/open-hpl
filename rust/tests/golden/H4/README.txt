h4.txt: "<game>:<map rel path><TAB>lines<TAB>fnv64" for every .hpm of SOMA, Amnesia Rebirth and Amnesia The Bunker (95 maps), plus
"synth<TAB>lines<TAB>fnv64". Recorded by harness/h4_probe.cpp, which drives the iHpmBuilder parse and applies the original C++
expressions of cHpmWorldBuilder (static object / plane / entity / area / light mask / exposure area / detail mesh transforms,
batch cells, combine-candidate rule). The synthetic corpus (400 LCG cases) replays the original std::map grouping of
CombineStaticMeshes, the submesh collider-name rules, the static batch flush rule and cell floors.
Replayed by world_hpm::h4_golden (cargo test h4_golden); maps are skipped per game when the Steam data is absent
(OHPL_H4_CORPUS overrides the common dir). H4_DUMP=1 prints every line in both probes for diffing.
Regenerate: harness/build.sh BUILD_DIR OUT; harness/compare.sh OUT (BUILD_DIR: configured amnesia/src build with HPL2 built).
Runtime creation sequence: OHPL_H4_SEQDUMP=<file> makes cHpmWorldBuilder append one line per created object
(type, name, file, matrix, flags); run with OHPL_IMPL=cpp and OHPL_IMPL=rust and diff.
