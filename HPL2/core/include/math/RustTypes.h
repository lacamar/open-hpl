#ifndef HPL_RUST_TYPES_H
#define HPL_RUST_TYPES_H

// Layout contract with rust/src/types.rs.

#include <cstddef>
#include "math/MathTypes.h"
#include "graphics/Color.h"

namespace hpl {
	static_assert(sizeof(cVector2f) == 8 && sizeof(cVector2l) == 8 && offsetof(cVector2f, y) == 4, "Vector2 layout");
	static_assert(sizeof(cVector3f) == 12 && sizeof(cVector3l) == 12 && offsetof(cVector3f, z) == 8, "Vector3 layout");
	static_assert(sizeof(cColor) == 16 && offsetof(cColor, a) == 12, "cColor layout");
	static_assert(sizeof(cMatrixf) == 64 && alignof(cMatrixf) == 4, "cMatrixf layout");
	static_assert(sizeof(cQuaternion) == 16 && offsetof(cQuaternion, w) == 12, "cQuaternion layout");
}

#endif
