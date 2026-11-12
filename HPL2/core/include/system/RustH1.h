#ifndef HPL_RUST_H1_H
#define HPL_RUST_H1_H

// C ABI of rust/src/system/h1.rs. Used when OHPL_IMPL=rust.

#include <cstddef>

extern "C" {
	void ohpl_h1_ser_defer(const char* name, const char* parent, const void* fields, size_t size, void* (*create)());
	void ohpl_h1_ser_flush();
	void ohpl_h1_clone_file(const char* src, const char* dst);
	bool ohpl_h1_data_dir(void (*sink)(void*, const unsigned char*, size_t), void* ctx);
	int ohpl_h1_spawn(const char* file, const char* const* argv, size_t n);
}

#endif
