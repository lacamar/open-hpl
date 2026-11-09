#ifndef HPL_RUST_PREPROCESS_H
#define HPL_RUST_PREPROCESS_H

// C ABI of rust/src/preprocess.rs.

#include <cstddef>

struct ohpl_pp_kv
{
	const char* k;
	size_t kn;
	const char* v;
	size_t vn;
};

extern "C" {
	bool ohpl_pp_parse(const char* input, size_t input_n,
					   const ohpl_pp_kv* env, size_t env_n, const ohpl_pp_kv* cur, size_t cur_n,
					   bool (*include)(void*, const char*, size_t, void (*)(void*, const char*, size_t), void*),
					   void (*log)(void*, bool, const char*, size_t), void* ctx,
					   void (*out)(void*, const char*, size_t), void* out_ctx,
					   void (*defines)(void*, const char*, size_t, const char*, size_t), void* defines_ctx);
}

#endif
