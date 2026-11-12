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

void* ohpl_h5_atlas_new(int w, int h, int handle);
void ohpl_h5_atlas_free(void*);
int ohpl_h5_atlas_pic_count(const void*);
int ohpl_h5_atlas_additions(const void*);
int ohpl_h5_atlas_handle(const void*);
bool ohpl_h5_atlas_full(const void*);
void ohpl_h5_atlas_set_full(void*, bool);
bool ohpl_h5_atlas_updated(const void*);
void ohpl_h5_atlas_set_updated(void*, bool);
bool ohpl_h5_atlas_need_reorg(const void*);
void ohpl_h5_atlas_set_need_reorg(void*, bool);
bool ohpl_h5_atlas_locked(const void*);
void ohpl_h5_atlas_set_locked(void*, bool);
bool ohpl_h5_atlas_take_updated(void*);
bool ohpl_h5_atlas_take_need_reorg(void*);
bool ohpl_h5_atlas_minimum_fit(const cRect2l* src, const cRect2l* dest);
bool ohpl_h5_atlas_add(void* atlas, const OhplH5Surface* dst, unsigned char* dstData, size_t dstLen,
					   const OhplH5Surface* src, const unsigned char* srcData, size_t srcLen,
					   bool linked, OhplH5Added* out);
void ohpl_h5_atlas_reorganize(void* atlas, const OhplH5Surface* dst, unsigned char* dstData, size_t dstLen,
							  const cRect2l* rects, size_t n, cRect2l* outRects, bool* outFit,
							  size_t* outOrder, size_t* outCount);

void* ohpl_h5_combo_new(const unsigned char* name, size_t n, size_t modes);
void ohpl_h5_combo_free(void*);
void ohpl_h5_combo_set_name(void*, const unsigned char* name, size_t n);
void ohpl_h5_combo_setup_mode(void*, int mode, const unsigned char* name, size_t nameN,
							  const unsigned char* vtx, size_t vtxN, const unsigned char* frag, size_t fragN);
void ohpl_h5_combo_add_feature(void*, int mode, unsigned shaders, const unsigned char* var, size_t n);
void ohpl_h5_combo_add_default(void*, int mode, const unsigned char* name, size_t nameN,
							   const unsigned char* value, size_t valueN);
void ohpl_h5_combo_program_name(const void*, int mode, int flags, OhplH5ByteSink sink, void* ctx);
void ohpl_h5_combo_shader_file(const void*, int mode, unsigned shaderType, OhplH5ByteSink sink, void* ctx);
void ohpl_h5_combo_shader_vars(const void*, int mode, int flags, unsigned shaderType, OhplH5VarSink sink, void* ctx);

}
}

#endif
