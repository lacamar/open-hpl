#ifndef HPL_RUST_MESH_MM_H
#define HPL_RUST_MESH_MM_H

// C ABI of rust/src/graphics/mm_ffi.rs and rust/src/material/mm_ffi.rs: cSubMesh, cMesh and cMaterial state.

#include <cstddef>
#include <cstdint>
#include "math/MathTypes.h"

namespace hpl {

struct OhplH7Mat;
struct OhplMmSub;
struct OhplMmMesh;
struct OhplMmCollider;
struct OhplMmPair { unsigned vtx, bone; float weight; };

struct OhplMmVbView { size_t indexNum; const unsigned *indices; size_t vertexNum; const float *positions; int stride; };

// Thunks over engine classes still in C++; field order mirrors rust/src/graphics/mm.rs `Host`.
// Deleted with cMaterialManager, cSkeleton, cAnimation, cNode3D, iVertexBuffer users.
struct OhplMmHost
{
	void (*varsDelete)(void*);
	void (*textureDestroy)(void*, void*);
	void (*materialDestroy)(void*, void*);
	void (*vbDelete)(void*);
	void (*vbView)(void*, OhplMmVbView*);
	void (*warning)(const unsigned char*, size_t);
	void (*meshName)(void*, const unsigned char**, size_t*);
	void (*subDelete)(void*);
	int (*skeletonBoneNum)(void*);
	void (*skeletonBindPos)(void*, int, cVector3f*);
	void (*skeletonDelete)(void*);
	void (*animationDelete)(void*);
	void *(*nodeNew)();
	void (*nodeDelete)(void*);
	void (*nodeName)(void*, const unsigned char**, size_t*);
	void *(*shapeBox)(void*, const cVector3f*, cMatrixf*);
	void *(*shapeSphere)(void*, const cVector3f*, cMatrixf*);
	void *(*shapeCylinder)(void*, float, float, cMatrixf*);
	void *(*shapeCapsule)(void*, float, float, cMatrixf*);
	void *(*shapeCompound)(void*, void *const*, size_t);
};

const OhplMmHost *OhplMmHostGet();

extern "C" {

OhplH7Mat *ohpl_mm_mat_new(const OhplMmHost*, void *type, void *resources);
void ohpl_mm_mat_free(OhplH7Mat*);
void ohpl_mm_mat_set_type(OhplH7Mat*, void *type);
void ohpl_mm_mat_compile(OhplH7Mat*);
void ohpl_mm_mat_compile_texture_lookup(OhplH7Mat*);
void *ohpl_mm_mat_vars_object(const OhplH7Mat*);
void ohpl_mm_mat_load_vars(const OhplH7Mat*, void *varsObject);
void ohpl_mm_mat_set_destroy_type_specifics(bool);
bool ohpl_mm_mat_destroy_type_specifics();

OhplMmSub *ohpl_mm_sub_new(const OhplMmHost*, void *manager, const char *name, size_t len);
void ohpl_mm_sub_free(OhplMmSub*);
void ohpl_mm_sub_set_material(OhplMmSub*, void *material);
void ohpl_mm_sub_compile(OhplMmSub*);
int ohpl_mm_sub_pair_num(const OhplMmSub*);
OhplMmPair *ohpl_mm_sub_pair(OhplMmSub*, int);
void ohpl_mm_sub_resize_pairs(OhplMmSub*, int);
void ohpl_mm_sub_add_pair(OhplMmSub*, unsigned vtx, unsigned bone, float weight);
void ohpl_mm_sub_clear_pairs(OhplMmSub*);
OhplMmCollider *ohpl_mm_sub_create_collider(OhplMmSub*, int kind);
OhplMmCollider *ohpl_mm_sub_collider(OhplMmSub*, int);
int ohpl_mm_sub_collider_num(const OhplMmSub*);
void *ohpl_mm_sub_create_collide_shape(OhplMmSub*, void *world);
void *ohpl_mm_collide_shape(const OhplMmHost*, OhplMmCollider*, void *world, const cVector3f *sizeMul, cMatrixf *offset);

OhplMmMesh *ohpl_mm_mesh_new(const OhplMmHost*, void *owner, void *manager);
void ohpl_mm_mesh_free(OhplMmMesh*);
void ohpl_mm_mesh_add_sub(OhplMmMesh*, void *shell, OhplMmSub *sub, const char *name, size_t len);
void *ohpl_mm_mesh_sub(const OhplMmMesh*, unsigned);
int ohpl_mm_mesh_sub_num(const OhplMmMesh*);
int ohpl_mm_mesh_sub_index(const OhplMmMesh*, const char *name, size_t len);
void *ohpl_mm_mesh_sub_by_name(const OhplMmMesh*, const char *name, size_t len);
int ohpl_mm_mesh_triangle_count(const OhplMmMesh*);
void ohpl_mm_mesh_set_skeleton(OhplMmMesh*, void*);
void *ohpl_mm_mesh_skeleton(const OhplMmMesh*);
void ohpl_mm_mesh_add_animation(OhplMmMesh*, void *anim, const char *name, size_t len);
void *ohpl_mm_mesh_animation(const OhplMmMesh*, int);
int ohpl_mm_mesh_animation_num(const OhplMmMesh*);
int ohpl_mm_mesh_animation_index(const OhplMmMesh*, const char *name, size_t len);
void ohpl_mm_mesh_clear_animations(OhplMmMesh*, bool deleteAll);
void ohpl_mm_mesh_compile_bones(OhplMmMesh*);
float ohpl_mm_mesh_radius(const OhplMmMesh*, int);
void *ohpl_mm_mesh_root(const OhplMmMesh*);
void ohpl_mm_mesh_add_node(OhplMmMesh*, void*);
int ohpl_mm_mesh_node_num(const OhplMmMesh*);
void *ohpl_mm_mesh_node(const OhplMmMesh*, int);
void *ohpl_mm_mesh_node_by_name(const OhplMmMesh*, const char *name, size_t len);

}

}

#endif
