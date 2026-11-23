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


	struct OhplDecalCounts
	{
		int vertices;
		int triangles;
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

}
}

#endif
