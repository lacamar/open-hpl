#ifndef HPL_RUST_DECAL_H
#define HPL_RUST_DECAL_H

// C ABI of rust/src/graphics/ffi_decal.rs.

#include "math/RustTypes.h"
#include <cstddef>
#include <cstdint>

namespace hpl {

	struct OhplDecalClip
	{
		cPlanef planes[6];
		cMatrixf world;
		cMatrixf normalRot;
		float offset;
		int maxTris;
	};

	struct OhplDecalMesh
	{
		const float* positions;
		size_t positionLen;
		uint32_t posStride;
		const float* normals;
		size_t normalLen;
		uint32_t nrmStride;
		const unsigned int* indices;
		uint32_t indexNum;
	};

	struct OhplDecalCounts
	{
		int vertices;
		int triangles;
	};

	struct OhplDecalOut
	{
		const cVector3f* positions;
		const cVector3f* normals;
		uint32_t vertexCount;
		const unsigned int* indices;
		uint32_t indexCount;
	};

	struct OhplDecalUv
	{
		cVector3f position;
		cVector3f right;
		cVector3f forward;
		float sizeX;
		float sizeZ;
		cVector2l subDiv;
		int current;
	};

	static_assert(sizeof(OhplDecalClip) == 232 && sizeof(OhplDecalCounts) == 8 && sizeof(OhplDecalUv) == 56, "decal ABI layout");

extern "C" {

OhplDecalOut* ohpl_decal_clip_mesh(const OhplDecalClip*, const OhplDecalMesh*, OhplDecalCounts*);
void ohpl_decal_out_free(OhplDecalOut*);
void ohpl_decal_uvs(const float*, size_t, uint32_t, uint32_t, const OhplDecalUv*, cVector3f*);

}
}

#endif
