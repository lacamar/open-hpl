#ifndef HPL_RUST_GRAPHICS_H
#define HPL_RUST_GRAPHICS_H

// C ABI of rust/src/graphics/ffi.rs.

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"
#include "math/RustMath.h"

namespace hpl {

struct OhplSurface {
	cVector3l size;
	int format;
	int bytes_per_pixel;
};
static_assert(sizeof(OhplSurface) == 20, "OhplSurface layout");

struct OhplGlyphs {
	const void* const* glyphs;
	size_t count;
	unsigned short first;
	unsigned short last;
	size_t advance_offset;
};
static_assert(sizeof(wchar_t) == 4, "wide text crosses the ABI as u32");

extern "C" {

int ohpl_gfx_channels(int);
int ohpl_gfx_bytes_per_pixel(int);
bool ohpl_gfx_is_compressed(int);
bool ohpl_gfx_is_depth(int);
bool ohpl_gfx_is_floating_point(int);
unsigned int ohpl_gfx_vertex_element_flag(int);
int ohpl_gfx_vertex_format_byte_size(int);
int ohpl_gfx_vertex_element_texture_unit(int);

void ohpl_bitmap_clear(unsigned char* data, size_t len, long long count, int bpp, int format, const unsigned char* color4);
void ohpl_bitmap_blit(const OhplSurface* dst, unsigned char* dstData, size_t dstLen,
					  const OhplSurface* src, const unsigned char* srcData, size_t srcLen,
					  const cVector3l* destPos, const cVector3l* srcSize, const cVector3l* srcPos);

void* ohpl_packer_new(int w, int h);
void ohpl_packer_free(void* p);
void ohpl_packer_clear(void* p, int w, int h);
bool ohpl_packer_insert(void* p, int w, int h, cRect2l* rect, bool* foundEmpty);
void ohpl_packer_reorganize_order(const int* wh, size_t n, size_t* out);

void ohpl_subimage_uvs(const cRect2l* rect, const cVector2l* srcSize, cVector2f* out4);
void ohpl_font_glyph_metrics(const cVector2l* offset, const cVector2l* size, const cVector2l* fontSize, int advance, const cVector2f* ratio, float* out5);
bool ohpl_font_word_wrap(const OhplGlyphs* g, float maxLength, float sizeX, const wchar_t* s, size_t n, void (*sink)(void*, const wchar_t*, size_t), void* ctx);

bool ohpl_scatter_disk(int size, int samples, bool sort, unsigned char* out, size_t len);

}
}

#endif
