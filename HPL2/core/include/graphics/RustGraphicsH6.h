#ifndef HPL_RUST_GRAPHICS_H6_H
#define HPL_RUST_GRAPHICS_H6_H

// C ABI of rust/src/graphics/ffi_h6.rs (skeleton, animation track, mesh name indices; OHPL_IMPL=rust).

#include <cstddef>
#include <cstdint>
#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"

namespace hpl {

struct OhplH6Skel;
struct OhplH6Track;
struct OhplH6Anim;
struct OhplH6Mesh;

extern "C" {

OhplH6Skel* ohpl_h6_skel_new();
void ohpl_h6_skel_free(OhplH6Skel*);
int ohpl_h6_skel_create_child(OhplH6Skel*, int parent, const char* name, size_t nameLen, const char* sid, size_t sidLen);
int ohpl_h6_skel_num(const OhplH6Skel*);
int ohpl_h6_skel_by_index(const OhplH6Skel*, int idx);
int ohpl_h6_skel_index_by_name(const OhplH6Skel*, const char*, size_t);
int ohpl_h6_skel_index_by_sid(const OhplH6Skel*, const char*, size_t);
void ohpl_h6_bone_set_transform(OhplH6Skel*, int, const cMatrixf*);
void ohpl_h6_bone_set_unscaled(OhplH6Skel*, int, const cMatrixf*);
void ohpl_h6_bone_set_inv_bind(OhplH6Skel*, int, const cMatrixf*);
void ohpl_h6_bone_needs_update_unscaled(OhplH6Skel*, int);
void ohpl_h6_bone_detach(OhplH6Skel*, int);
const cMatrixf* ohpl_h6_bone_local(OhplH6Skel*, int);
const cMatrixf* ohpl_h6_bone_local_unscaled(OhplH6Skel*, int);
const cMatrixf* ohpl_h6_bone_world(OhplH6Skel*, int);
const cMatrixf* ohpl_h6_bone_inv_world(OhplH6Skel*, int);
const cMatrixf* ohpl_h6_bone_world_unscaled(OhplH6Skel*, int);
const cMatrixf* ohpl_h6_bone_inv_bind(OhplH6Skel*, int);

OhplH6Track* ohpl_h6_track_new();
void ohpl_h6_track_free(OhplH6Track*);
void ohpl_h6_track_reserve(OhplH6Track*, int);
cKeyFrame* ohpl_h6_track_create(OhplH6Track*, float time);
void ohpl_h6_track_clear(OhplH6Track*);
int ohpl_h6_track_num(const OhplH6Track*);
cKeyFrame* ohpl_h6_track_frame(const OhplH6Track*, int);
void ohpl_h6_track_interpolate(const OhplH6Track*, float length, float time, cKeyFrame* out);
float ohpl_h6_track_frames_at_time(const OhplH6Track*, float length, float time, size_t* idxA, size_t* idxB);

OhplH6Anim* ohpl_h6_anim_new();
void ohpl_h6_anim_free(OhplH6Anim*);
void ohpl_h6_anim_add_track(OhplH6Anim*, const char*, size_t);
int ohpl_h6_anim_track_by_name(const OhplH6Anim*, const char*, size_t);

OhplH6Mesh* ohpl_h6_mesh_new();
void ohpl_h6_mesh_free(OhplH6Mesh*);
void ohpl_h6_mesh_add_sub(OhplH6Mesh*, const char*, size_t);
int ohpl_h6_mesh_sub_index(const OhplH6Mesh*, const char*, size_t);
int ohpl_h6_mesh_sub_by_name(const OhplH6Mesh*, const char*, size_t);
void ohpl_h6_mesh_add_animation(OhplH6Mesh*, const char*, size_t);
int ohpl_h6_mesh_animation_index(const OhplH6Mesh*, const char*, size_t);
void ohpl_h6_mesh_clear_animations(OhplH6Mesh*);

}
}

#endif
