#ifndef HPL_RUST_VK_H
#define HPL_RUST_VK_H

// C ABI of rust/src/vk/ffi.rs: Vulkan enum tables, pixel formats and conversions, mip plans,
// index conversion, uniform writes, sampler keys, ring allocation and the GLSL rewriter.
// No vk* call crosses the boundary.

#include <cstddef>
#include <cstdint>

namespace hpl {

struct OhplVkFormat { uint32_t format; uint32_t swizzle[4]; int convert; int srcComponents; };
struct OhplVkMipIn
{
	int size[3]; int numMips; unsigned downscale; int minSize[3];
	bool useMipmaps, compressed, depth, floating, cube, tex2d, rect;
};
struct OhplVkMipPlan { int size[3]; bool useMipmaps; int start; bool cpuResize; int halveSteps; bool provided; int levels; };
struct OhplVkRing { size_t offset; bool grow; size_t newSize; size_t used; };
struct OhplVkViewport { float viewport[4]; int scissor[2]; unsigned scissorSize[2]; };
struct OhplVkUniform { int offset, arrayStride, matrixColumns, count; bool isInt, isBool; int components; };
struct OhplVkSamplerKey
{
	unsigned mag, min, mip, u, v, w, cmpEnable, cmpOp;
	float maxAniso; unsigned anisoEnable; float maxLod;
};
static_assert(sizeof(OhplVkFormat) == 28 && sizeof(OhplVkMipIn) == 40 && sizeof(OhplVkMipPlan) == 36, "OhplVk layout");
static_assert(sizeof(OhplVkRing) == 32 && sizeof(OhplVkViewport) == 32 && sizeof(OhplVkUniform) == 24, "OhplVk layout");
static_assert(sizeof(OhplVkSamplerKey) == 44, "OhplVk layout");

struct OhplVkGlslSink
{
	void *ctx;
	void (*error)(void*, const char*, size_t);
	void (*program)(void*, const char*, size_t, const char*, size_t, unsigned, int);
	void (*sampler)(void*, const char*, size_t, const char*, size_t, int);
	void (*init)(void*, const char*, size_t, const float*, size_t);
};

extern "C" {
unsigned ohpl_vk_stencil_op(int);
unsigned ohpl_vk_blend_factor(int);
unsigned ohpl_vk_wrap(int);
int ohpl_vk_prim(int);
void ohpl_vk_format(int, bool, uint32_t, OhplVkFormat*);
uint32_t ohpl_vk_unorm_of_srgb(uint32_t);
size_t ohpl_vk_format_size(uint32_t);
uint16_t ohpl_vk_float_to_half(float);
bool ohpl_vk_convert_len(const OhplVkFormat*, size_t, size_t*, size_t*);
void ohpl_vk_convert_pixels(const OhplVkFormat*, const uint8_t*, size_t, size_t, uint8_t*, size_t);
int ohpl_vk_full_mip_count(int, int, int);
void ohpl_vk_halve_size(int, int, int*);
void ohpl_vk_halve_image(const uint8_t*, size_t, int, int, size_t, uint8_t*, size_t);
void ohpl_vk_mip_plan(const OhplVkMipIn*, OhplVkMipPlan*);
size_t ohpl_vk_converted_len(int, int);
bool ohpl_vk_convert_indices(int, const unsigned*, size_t, unsigned*, size_t);
uint64_t ohpl_vk_sampler_key(const OhplVkSamplerKey*);
void ohpl_vk_ring_alloc(size_t, size_t, size_t, size_t, OhplVkRing*);
void ohpl_vk_viewport(const int*, const int*, const int*, bool, const int*, const int*, OhplVkViewport*);
void ohpl_vk_gl_ortho(float, float, float, float, float, float, float*);
void ohpl_vk_write_matrix(float*, const float*, int);
void ohpl_vk_reflect_uniform(int, int, int, int, OhplVkUniform*);
size_t ohpl_vk_split_uniform_name(const char*, size_t, int*);
bool ohpl_vk_write_uniform(uint8_t*, size_t, const OhplVkUniform*, int, int, int, const float*);
bool ohpl_vk_write_int(uint8_t*, size_t, const OhplVkUniform*, int, int);
bool ohpl_vk_write_mat4(uint8_t*, size_t, const OhplVkUniform*, int, const float*);
void ohpl_vk_unpack_pixel(uint32_t, const uint8_t*, size_t, float*);
bool ohpl_vk_glsl_to_vulkan(const char*, size_t, const char*, size_t, const OhplVkGlslSink*);
}

}

#endif
