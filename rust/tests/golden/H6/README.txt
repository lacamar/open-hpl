H6 golden vectors: cSkeleton/cBone, cAnimation/cAnimationTrack and cMesh lookups, recorded from the C++ path.

skel_<seed>.txt anim_<seed>.txt mesh_<seed>.txt   "<op>\t<result>" per line (floats as 8-digit hex bits,
  bone/track/sub-mesh ids are creation order, -1 = null). Seeds 0-11 of 300 compared.
census.txt   "<kind> <fnv32(path)> <low 32 bits of the h6 hash> <bones> <tracks>" of every .msh/.dae/.fbx/.anm of the
  TDD, AMFP, SOMA, Rebirth and Bunker installs that has a skeleton or tracks (list: see census.sh).
creator_trace.txt   cMeshCreator over the recording low-level graphics (h6_creator.cpp), identical for cpp and rust.
cargo test (graphics::tests_h6) replays the scripts against the Rust state without the C++.

Record (needs a build dir of this tree with `make HPL2`; `make HPL2TraceRecorder` for h6_creator):
  harness/gen.sh <build dir> h6_script.cpp <out>/h6_script    (also h6_gen.cpp, h6_creator.cpp)
  harness/compare.sh <out>/h6_script 300 <dir>                OHPL_IMPL=cpp vs rust per scenario, copies seeds 0-11
  harness/census.sh <out>/h6_gen <msh|dae|fbx|anm|daeanim|fbxanim> <list> <out dir>
h6 hash: bone local/world/inverse/inverse-bind matrices, name/sid lookups, sub-mesh and animation lookups,
  triangle count, track interpolation and key-frame pair at 11 times per track.
Skinned .dae crash the C++ loader without an MSH cache and are left out (as in L6).
  harness/compact.py <census dir> census.txt
