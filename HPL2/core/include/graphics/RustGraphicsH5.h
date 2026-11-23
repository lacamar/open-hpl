#ifndef HPL_RUST_GRAPHICS_H5_H
#define HPL_RUST_GRAPHICS_H5_H

// C ABI of rust/src/graphics/ffi_h5.rs (frame atlas, program combo settings).

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"

namespace hpl {

struct OhplH5Surface {
	cVector3l size;
	int format;
	int bytes_per_pixel;
};
static_assert(sizeof(OhplH5Surface) == 20, "OhplH5Surface layout");

struct OhplH5Added {
	cRect2l rect;
	bool found_empty;
};
static_assert(sizeof(OhplH5Added) == 20, "OhplH5Added layout");

typedef void (*OhplH5ByteSink)(void*, const unsigned char*, size_t);
typedef void (*OhplH5VarSink)(void*, const unsigned char*, size_t, const unsigned char*, size_t);

extern "C" {

}
}

#endif
