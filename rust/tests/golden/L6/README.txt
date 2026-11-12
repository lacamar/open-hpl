L6 mesh loaders (MeshLoaderMSH, MeshLoaderCollada, MeshLoaderAssimp).

msh.txt anm.txt fbx.txt fbxanim.txt dae.txt daeanim.txt: one line per file of the TDD, AMFP, SOMA,
Rebirth and Bunker installs: "<fnv32 of the path relative to steamapps/common> <low 32 bits of the
mesh hash>", sorted. The hash (gen/gen.cpp, FNV-1a 64) covers vertex arrays, indices, bone pairs and
skeleton, animations, submesh/material/node names, transforms and the log lines of the load.
"x<log hash>" marks a load that fails (same log on both paths). Recorded from the C++ loaders:
  fbx      LoadMesh of every .fbx (assimp)
  fbxanim  LoadAnimation of every .fbx (.anm companion)
  msh      LoadMesh of every .msh
  anm      MSH LoadAnimation of every .anm
  dae      LoadMesh of every .dae without <controller> (skinned ones crash the C++ without an MSH cache)
  daeanim  LoadAnimation of every 4th of those (NR%12<3 of the sorted list)

gen/check.sh <build-dir> [stride] loads the files with OHPL_IMPL=cpp and =rust, diffs the outputs and
compares them with these files.

streams.txt: call streams of the Rust path into a recording builder for a sample of .msh/.dae;
`cargo test` replays it (OHPL_L6_RECORD=1 rewrites it).
