#ifndef HPL_RUST_MESH_H
#define HPL_RUST_MESH_H

// C ABI of rust/src/graphics/ffi.rs.

#include "math/RustTypes.h"
#include "graphics/GraphicsTypes.h"
#include <cstddef>
#include <cstdint>

namespace hpl {
	static_assert(sizeof(cKeyFrame) == 32 && offsetof(cKeyFrame, rotation) == 12 && offsetof(cKeyFrame, time) == 28, "cKeyFrame layout");
	static_assert(sizeof(cVertexBonePair) == 12 && offsetof(cVertexBonePair, boneIdx) == 4 && offsetof(cVertexBonePair, weight) == 8, "cVertexBonePair layout");

	struct OhplMeshData
	{
		const cVector3f* positions;
		const cVector3f* normals;
		const cVector3f* texcoords;
		uint32_t vertexCount;
		const unsigned int* indices;
		uint32_t indexCount;
	};

extern "C" {

OhplMeshData* ohpl_meshgen_box(const cVector3f*);
OhplMeshData* ohpl_meshgen_sky_box(float);
OhplMeshData* ohpl_meshgen_sphere(float, int, int);
OhplMeshData* ohpl_meshgen_cylinder(const cVector2f*, int);
OhplMeshData* ohpl_meshgen_capsule(const cVector2f*, int, int);
OhplMeshData* ohpl_meshgen_cone(const cVector2f*, int);
int ohpl_meshgen_plane(const cVector3f*, const cVector3f*, const cVector2f*, OhplMeshData**);
OhplMeshData* ohpl_meshgen_wireframe(const unsigned int*, size_t);
void ohpl_meshgen_free(OhplMeshData*);

float ohpl_anim_frames_at_time(const cKeyFrame* const*, size_t, float, float, float, size_t*, size_t*);
void ohpl_anim_interpolate(const cKeyFrame* const*, size_t, float, float, float, cKeyFrame*);
size_t ohpl_anim_insert_position(const cKeyFrame* const*, size_t, float, float, bool*);

bool ohpl_submesh_one_sided(const unsigned int*, size_t, const float*, size_t, int, cVector3f*, cVector3f*);
uint32_t ohpl_submesh_compile_bone_pairs(const cVertexBonePair*, size_t, size_t, float*, unsigned char*);
void ohpl_mesh_bone_radii(const cVector3f*, float*, size_t, const cVertexBonePair*, size_t, const float*, size_t, int);

}
}

#endif
