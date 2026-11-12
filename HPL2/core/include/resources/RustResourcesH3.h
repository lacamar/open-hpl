#ifndef HPL_RUST_RESOURCES_H3_H
#define HPL_RUST_RESOURCES_H3_H

// C ABI of rust/src/resources/manager_ffi.rs.

#include <cstddef>
#include <cstdint>
#include "resources/RustResources.h"

struct OhplH3Mgr;

extern "C" {
	OhplH3Mgr* ohpl_h3_mgr_new();
	void ohpl_h3_mgr_free(OhplH3Mgr*);
	OhplResMap* ohpl_h3_mgr_map(OhplH3Mgr*);
	void ohpl_h3_mgr_add(OhplH3Mgr*, void* res, const uint32_t* path, size_t n, const OhplResMeta* meta);
	bool ohpl_h3_mgr_remove(OhplH3Mgr*, void* res);
	void* ohpl_h3_mgr_get(const OhplH3Mgr*, const uint32_t* path, size_t n);
	size_t ohpl_h3_mgr_len(const OhplH3Mgr*);
	void* ohpl_h3_mgr_first(OhplH3Mgr*);
	void ohpl_h3_mgr_eviction_tail(const OhplH3Mgr*, int64_t keep, void (*sink)(void*, void* const*, size_t), void* ctx);
	void ohpl_h3_begin_load(const uint8_t* name, size_t n, uint64_t now);
	bool ohpl_h3_end_load(uint64_t now, void (*sink)(void*, const uint8_t*, size_t), void* ctx, uint64_t* elapsed);
	int32_t ohpl_h3_tab_count();
}

#endif
