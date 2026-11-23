#ifndef HPL_RUST_IMPL_H
#define HPL_RUST_IMPL_H

// C ABI of rust/src/imageload/ffi.rs and rust/src/sdlsys/ffi.rs.

#include <cstddef>
#include <cstdint>

extern "C" {

struct OhplDdsBlockPlan {
	unsigned format;
	unsigned block_size;
	int bytes_per_pixel;
	bool cube;
	unsigned width;
	unsigned height;
	unsigned mips;
	unsigned images;
	uint64_t data_offset;
};
struct OhplDdsAlphaPlan {
	unsigned width;
	unsigned height;
	unsigned row_bytes;
};
struct OhplDdsLayout {
	int images;
	int mips;
	bool setup_data;
};
struct OhplFontHeader {
	int line_height;
	int base;
	float height;
	float size_ratio_x;
};
struct OhplYuvPlanes {
	const unsigned char* y;
	size_t y_len;
	const unsigned char* u;
	size_t u_len;
	const unsigned char* v;
	size_t v_len;
	int y_stride;
	int uv_stride;
	unsigned offset_x;
	unsigned offset_y;
};
struct OhplYuvTables {
	const unsigned char* to_r;
	const unsigned char* to_b;
	const unsigned short* g_uv;
	const unsigned char* g_y_uv;
};

bool ohpl_sys_looks_like_steam_install(const char* path, size_t len, const char* cwd);
bool ohpl_sys_renderer_is_vulkan(const char* name);

}

enum { kOhplEnvAlsoftNull = 1, kOhplEnvVideoDriverX11 = 2, kOhplEnvDisableScreenSaver = 4 };
enum { kOhplArgCwd = 0, kOhplArgSkip = 1, kOhplArgGameDir = 2, kOhplArgCommand = 3 };

#endif
