#ifndef HPL_RUST_PLATFORM_H
#define HPL_RUST_PLATFORM_H

// C ABI of rust/src/platform_ffi.rs.

#include <cstddef>

#include "system/SystemTypes.h"

static_assert(hpl::eSystemPath_Personal == 0 && hpl::eSystemPath_XDGDataHome == 1 && hpl::eSystemPath_XDGConfigHome == 2
	&& hpl::eSystemPath_XDGCacheHome == 3 && hpl::eSystemPath_XDGStateHome == 4 && hpl::eSystemPath_XDGPictures == 5,
	"eSystemPath values cross the ABI");

extern "C" {
	bool ohpl_platform_special_path(unsigned kind, void (*sink)(void*, const unsigned char*, size_t), void* ctx);
}

#endif
