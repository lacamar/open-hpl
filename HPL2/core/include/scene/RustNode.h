#ifndef HPL_RUST_NODE_H
#define HPL_RUST_NODE_H

// C ABI of rust/src/scene/ffi_node.rs.

#include "math/RustTypes.h"
#include <cstdint>

namespace hpl {
extern "C" {

void ohpl_scene_inverse(const cMatrixf*, cMatrixf*);
void ohpl_scene_calc_scale(const cMatrixf*, cVector3f*);

}
}

#endif
