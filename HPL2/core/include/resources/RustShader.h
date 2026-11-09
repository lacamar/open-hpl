#ifndef HPL_RUST_SHADER_H
#define HPL_RUST_SHADER_H

// C ABI of rust/src/shader.rs.

#include <cstddef>

struct ohpl_shader_kv
{
	const char* k;
	size_t kn;
	const char* v;
	size_t vn;
};

extern "C" {
	void ohpl_shader_fallback_name(const char* name, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_shader_patch_hpsl(const char* file, size_t file_n, const char* data, size_t data_n, bool hw_srgb,
								const char* debug, size_t debug_n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_shader_fix_bc5(const char* data, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_shader_texture_bindings(const char* text, size_t n, void (*cb)(void*, const char*, size_t, int), void* ctx);
	void ohpl_shader_sampler_units(const ohpl_shader_kv* vars, size_t n, void (*cb)(void*, const char*, size_t, int), void* ctx);
}

#endif
