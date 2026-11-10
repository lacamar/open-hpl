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

bool ohpl_dds_plan_block(const unsigned char* header, size_t len, uint64_t fileSize, bool noCompression, OhplDdsBlockPlan* out);
int ohpl_dds_mip_size(const OhplDdsBlockPlan* plan, unsigned mip);
bool ohpl_dds_plan_alpha(const unsigned char* header, size_t len, uint64_t fileSize, OhplDdsAlphaPlan* out);

unsigned ohpl_devil_pixel_format_to_hpl(unsigned il);
unsigned ohpl_devil_pixel_format_from_hpl(unsigned format);
unsigned ohpl_devil_dxt_to_hpl(unsigned il);
unsigned ohpl_devil_type_from_extension(const unsigned* ext, size_t len);
void ohpl_devil_dds_layout(int ilImages, int ilMips, int cubeFlags, OhplDdsLayout* out);
int ohpl_devil_misc_image_count(int ilImages);

void ohpl_theora_fill_tables(unsigned char* toR, unsigned char* toB, unsigned short* gUv, unsigned char* gYUv);
size_t ohpl_theora_frame_len(int w, int h);
bool ohpl_theora_draw_frame(unsigned char* out, size_t outLen, const OhplYuvPlanes* planes, const OhplYuvTables* tables, int w, int h);

OhplFontHeader ohpl_font_header(int outline, int lineHeight, int base);
long long ohpl_font_glyph_count(int largestId);
bool ohpl_font_glyph_id_valid(int id, size_t glyphCount);
int ohpl_font_glyph_advance(int xadvance, int outline);
bool ohpl_font_page_valid(int page, size_t pages);

bool ohpl_sys_steam_root(const char* real, void (*sink)(void*, const char*, size_t), void* ctx);
bool ohpl_sys_vdf_path(const char* line, size_t len, void (*sink)(void*, const char*, size_t), void* ctx);
bool ohpl_sys_looks_like_steam_install(const char* path, size_t len, const char* cwd);
uint64_t ohpl_sys_remount_flags(uint64_t stFlag);
unsigned ohpl_sys_classify_arg(const char* arg, bool gotGameDir);
unsigned ohpl_sys_engine_env(bool headless, bool alsoftSet, bool video, bool videodriverSet, bool displaySet);
bool ohpl_sys_renderer_is_vulkan(const char* name);
void ohpl_sys_script_message(const char* section, int row, int col, unsigned kind, const char* message, void (*sink)(void*, const char*, size_t), void* ctx);
void ohpl_sys_script_chunks(const char* msg, size_t len, void (*sink)(void*, const char*, size_t), void* ctx);

}

enum { kOhplEnvAlsoftNull = 1, kOhplEnvVideoDriverX11 = 2, kOhplEnvDisableScreenSaver = 4 };
enum { kOhplArgCwd = 0, kOhplArgSkip = 1, kOhplArgGameDir = 2, kOhplArgCommand = 3 };

#endif
