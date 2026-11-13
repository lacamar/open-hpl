#ifndef HPL_RUST_RESOURCES_H2_H
#define HPL_RUST_RESOURCES_H2_H

// C ABI of rust/src/resources/ffi.rs.

#include <cstddef>
#include <cstdint>

struct OhplH2Searcher;
struct OhplH2Config;
struct OhplH2Lang;

struct OhplH2LangOps
{
	void* ctx;
	void (*log)(void*, int32_t level, const uint8_t*, size_t);
	void (*add_dir)(void*, const uint32_t* p, size_t n, bool subDirs);
	bool (*file_exists)(void*, const uint32_t* p, size_t n);
	bool (*read_file)(void*, const uint32_t* p, size_t n, void (*sink)(void*, const uint8_t*, size_t), void* sctx, bool* readOk);
	void* (*text_new)(void*, const int32_t*, size_t);
	void (*text_free)(void*, void*);
};

extern "C" {
	OhplH2Lang* ohpl_h2_lang_new(const OhplH2LangOps*);
	void ohpl_h2_lang_free(OhplH2Lang*);
	bool ohpl_h2_lang_add_file(OhplH2Lang*, const uint32_t* file, size_t n, bool addDirs, const uint32_t* alt, size_t an);
	void* ohpl_h2_lang_translate(const OhplH2Lang*, const uint8_t* cat, size_t cn, const uint8_t* name, size_t nn);
	bool ohpl_h2_lang_has(const OhplH2Lang*, const uint8_t* cat, size_t cn, const uint8_t* name, size_t nn);
	void ohpl_h2_lang_set(OhplH2Lang*, const uint8_t* cat, size_t cn, const uint8_t* name, size_t nn, const int32_t* text, size_t tn);
	void ohpl_h2_lang_each(const OhplH2Lang*, void (*f)(void*, const uint8_t*, size_t, const uint8_t*, size_t, void*), void* ctx);
	OhplH2Searcher* ohpl_h2_fs_new();
	void ohpl_h2_fs_free(OhplH2Searcher*);
	void ohpl_h2_fs_add_dir(OhplH2Searcher*, const uint32_t* dir, size_t dn, const char* mask, size_t mn, bool subDirs);
	void ohpl_h2_fs_clear(OhplH2Searcher*);
	int64_t ohpl_h2_fs_get(const OhplH2Searcher*, const char* name, size_t n, int* equalCount);
	const uint32_t* ohpl_h2_fs_path(const OhplH2Searcher*, uint32_t id, size_t* len);

	OhplH2Config* ohpl_h2_cfg_new();
	void ohpl_h2_cfg_free(OhplH2Config*);
	int ohpl_h2_cfg_load(OhplH2Config*, const uint32_t* file, size_t n, bool useCrc);
	int ohpl_h2_cfg_save(const OhplH2Config*, const uint32_t* file, size_t n, bool useCrc);
	void ohpl_h2_cfg_set(OhplH2Config*, const char* level, size_t ln, const char* name, size_t nn, const char* val, size_t vn);
	bool ohpl_h2_cfg_get(const OhplH2Config*, const char* level, size_t ln, const char* name, size_t nn, void (*sink)(void*, const char*, size_t), void* ctx);
}

#endif
