#ifndef HPL_RUST_H1_H
#define HPL_RUST_H1_H

// C ABI of rust/src/system/h1.rs. Used when OHPL_IMPL=rust.

#include <cstddef>

struct OhplSerHost;

extern "C" {
	void ohpl_h1_ser_defer(const char* name, const char* parent, size_t size, void* (*create)());
	bool ohpl_h1_ser_save_file(const OhplSerHost* host, void* obj, const wchar_t* path, size_t pathLen, const char* root, size_t rootLen, bool compress);
	bool ohpl_h1_ser_load_file(const OhplSerHost* host, void* obj, const wchar_t* path, size_t pathLen, bool compress);
	void ohpl_h1_clone_file(const char* src, const char* dst);
	bool ohpl_h1_data_dir(void (*sink)(void*, const unsigned char*, size_t), void* ctx);
	int ohpl_h1_spawn(const char* file, const char* const* argv, size_t n);
}

#endif
