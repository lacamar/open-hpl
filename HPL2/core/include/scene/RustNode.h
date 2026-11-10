#ifndef HPL_RUST_NODE_H
#define HPL_RUST_NODE_H

// C ABI of rust/src/scene/ffi_node.rs.

#include "math/RustTypes.h"
#include <cstdint>

namespace hpl {
extern "C" {

void ohpl_scene_world_to_local(const cMatrixf*, const cMatrixf*, cMatrixf*);
void ohpl_scene_node_update_matrix(const cMatrixf*, const cQuaternion*, const cVector3f*, cMatrixf*);
void ohpl_scene_node_add_rotation(const cVector3f*, int, const cQuaternion*, cQuaternion*);
void ohpl_scene_scaled_world(const cMatrixf*, const cVector3f*, cMatrixf*);
void ohpl_scene_inverse(const cMatrixf*, cMatrixf*);
void ohpl_scene_calc_scale(const cMatrixf*, cVector3f*);
void ohpl_scene_camera_view_matrix(int, const cVector3f*, float, float, float, const cMatrixf*, cMatrixf*);
bool ohpl_scene_camera_move_matrix(int, int, float, float, cMatrixf*);
void ohpl_scene_camera_projection(int, float, float, float, float, bool, const cVector2f*, cMatrixf*);
bool ohpl_scene_camera_unproject(int, float, float, float, const cVector2f*, const cVector2f*, const cVector2f*,
								 const cMatrixf*, const cVector3f*, const cVector3f*, const cVector3f*, const cVector3f*,
								 cVector3f*, cVector3f*);

}
}

#endif
