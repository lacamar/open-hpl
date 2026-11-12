H7 golden vectors: cMaterial state, material type registry, iPostEffect base state, post-effect passes, .mat census. Recorded from the C++ path.

mat_<seed>.txt  cMaterial over the 10 registered types: SetBlendMode/AlphaMode/DepthTest/UV animations/Update/SetType; floats as 8-digit hex bits.
pe_<seed>.txt   iPostEffect base (SetActive/SetDisabled/SetParams/GetParams) over stub effects and types.
reg.txt         iMaterialType registry: flags, used textures, user variable defaults per type name.
census.txt      G = OhplGlobals bytes; M = "fnv32(path) fnv64(outputs)" for every .mat of SOMA, TDD, Rebirth, Bunker and AMFP
                (10306: type, depth/blend, uv animations, LoadVariables, GetVariableValues, program selection, texture units, compile);
                U = one line per distinct setup (kind, texture mask, blend, uv flag, vars bytes with padding zeroed, hash of the Rust-side
                outputs). cargo test (material::h7_tests) replays the U lines through ohpl_mat_program/texture_unit/compile.
pass_<seed>.txt cPostEffectComposite::Render over the real post effect types and the recording low-level graphics: frame summary + GPU trace.
                Equal for OHPL_IMPL=cpp and rust (not replayed by cargo, it needs the C++ shell).
Seeds 0-11 of 300 compared (mat, pe, pass).

Record (needs a build dir of this tree with `make HPL2 HPL2TraceRecorder`):
  harness/gen.sh <build dir> h7_gen.cpp <out>/h7_gen
  harness/compare.sh <out>/h7_gen 300 <dir>      OHPL_IMPL=cpp vs rust per scenario, copies seeds 0-11
  harness/census.sh <out>/h7_gen <dir>           census.cpp / census.rust over the Steam data (read-only)
  H7_SCENARIO=reg <out>/h7_gen script           reg.txt
