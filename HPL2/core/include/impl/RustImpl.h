#ifndef HPL_RUST_IMPL_H
#define HPL_RUST_IMPL_H

// C ABI of rust/src/imageload/ffi.rs and rust/src/sdlsys/ffi.rs.

#include <cstddef>
#include <cstdint>

extern "C" {


bool ohpl_sys_looks_like_steam_install(const char* path, size_t len, const char* cwd);
bool ohpl_sys_renderer_is_vulkan(const char* name);

}

enum { kOhplEnvAlsoftNull = 1, kOhplEnvVideoDriverX11 = 2, kOhplEnvDisableScreenSaver = 4 };
enum { kOhplArgCwd = 0, kOhplArgSkip = 1, kOhplArgGameDir = 2, kOhplArgCommand = 3 };

#endif
