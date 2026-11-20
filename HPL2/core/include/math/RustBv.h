#ifndef HPL_RUST_BV_H
#define HPL_RUST_BV_H

// C ABI of rust/src/math/ffi_bv.rs. OhplBv* is the first field (m_mtxTransform) of a cBoundingVolume,
// OhplFrustum* a cFrustum; the Rust structs mirror both layouts.

#include "math/RustTypes.h"

namespace hpl {
	struct OhplBv;
	struct OhplFrustum;

extern "C" {

void ohpl_bv_init(OhplBv*);
void ohpl_bv_copy(OhplBv* dst, const OhplBv* src);
void ohpl_bv_assign(OhplBv* dst, const OhplBv* src);
void ohpl_bv_drop(OhplBv*);
void ohpl_bv_update_size(OhplBv*);
void ohpl_bv_set_local_min_max(OhplBv*, const cVector3f* min, const cVector3f* max);
void ohpl_bv_set_size(OhplBv*, const cVector3f*);
void ohpl_bv_set_position(OhplBv*, const cVector3f*);
void ohpl_bv_set_transform(OhplBv*, const cMatrixf*);
void ohpl_bv_local_center(const OhplBv*, cVector3f* out);
void ohpl_bv_world_center(OhplBv*, cVector3f* out);
void ohpl_bv_add_array_points(OhplBv*, const float* array, int count);
void ohpl_bv_create_from_points(OhplBv*, int stride);

void ohpl_frustum_init(OhplFrustum*);
void ohpl_frustum_setup_perspective(OhplFrustum*, const cMatrixf* proj, const cMatrixf* view, float far, float near, float fov,
	float aspect, const cVector3f* origin, bool inf, const cMatrixf* customFar, bool oblique);
void ohpl_frustum_setup_ortho(OhplFrustum*, const cMatrixf* proj, const cMatrixf* view, float far, float near,
	const cVector2f* viewSize, const cVector3f* origin, bool inf);
bool ohpl_frustum_collide_point(OhplFrustum*, const cVector3f*);
int ohpl_frustum_collide_bv(const OhplFrustum*, OhplBv*);
int ohpl_frustum_collide_node(const OhplFrustum*, const cVector3f* center, float radius, const cVector3f* min, const cVector3f* max);
int ohpl_frustum_collide_frustum(OhplFrustum*, OhplFrustum*);
int ohpl_frustum_collide_frustum_sphere(const OhplFrustum*, const cVector3f*, float);
int ohpl_frustum_collide_sphere(const OhplFrustum*, const cVector3f*, float, int maxPlanes);
int ohpl_frustum_collide_aabb(const OhplFrustum*, const cVector3f* min, const cVector3f* max);
bool ohpl_frustum_check_sphere_near_plane(const OhplFrustum*, const cVector3f*, float);
bool ohpl_frustum_check_aabb_near_plane(const OhplFrustum*, const cVector3f* min, const cVector3f* max);
bool ohpl_frustum_check_bv_near_plane(const OhplFrustum*, OhplBv*);
bool ohpl_frustum_check_frustum_near_plane(const OhplFrustum*, const OhplFrustum*);
bool ohpl_frustum_check_line(OhplFrustum*, const cVector3f* start, const cVector3f* end, cVector3f* out);

}
}

#endif
