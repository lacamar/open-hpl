H11 (cMeshEntity, cSubMeshEntity, cAnimationState, iLight point/spot/box, cFogArea) golden vectors, recorded from the C++ path
(OHPL_IMPL=cpp, pristine HEAD abe230f libraries) by harness/h11_script.cpp.

mesh_<name>.txt   21 real meshes + animations of TDD, Rebirth, Bunker and SOMA (harness/scenarios.txt; skeleton, bone-less
                  node meshes, .msh/.anm/.fbx/.dae_anim animations), 300 random ops each (UpdateLogic, Play/PlayFadeTo/Stop, state
                  setters, skeleton physics and fade, rest pose, SetStatic, UpdateGraphicsForFrame, bounds, CalculateTransformFromSkeleton).
light_<n>.txt     12 scripts (point/spot/box) of 300 ops: diffuse/radius/brightness/fade/flicker/matrix/mask/shadow-caster cache.
fog_<n>.txt       3 scripts of 120 ops (SetSize, matrix, properties).
Lines: o = op, = = result, e = callback event (AfterAnimationUpdate, OnAnimationEvent) in call order, s/x = animation state and
  event setup, h = hash of animation state weights/times/flags and skeleton-physics state, r/c = rest-pose and
  transform-from-skeleton vectors, "= lit" light visibility, "= fog" bounding size.
  ~ lines (bone/node matrices, bounding volumes, skinned vertices, light frustum/projection hashes) are C++-only output, compared
  live by harness/run.sh (cpp vs rust: 36 files, 10260 ops, 0 differences; harness/soak.sh: 75 runs, 61800 ops, 0 differences).
Replay: cargo test scene::tests_h11 (mock entity around rust/src/scene/h11.rs, no C++).
Compare: scripts/h11-diff.sh <build-dir>
Record:  harness/gen.sh <baseline build> h11_script.cpp <bin> <baseline tree>; harness/run.sh <bin> <dir> cpp
Not covered: skeleton bodies/colliders (the harness has no physics world), light particle/sound flicker callbacks (no world).
