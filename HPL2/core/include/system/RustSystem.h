#ifndef HPL_RUST_SYSTEM_H
#define HPL_RUST_SYSTEM_H

// C ABI of rust/src/system/ffi.rs.

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "system/RustXml.h"

struct OhplSerHost
{
	void (*log)(int, const uint8_t*, size_t);
	void (*top_class)(void*, void (*)(void*, const uint8_t*, size_t), void*);
	void (*destroy)(void*);
	void* (*tmp_new)(unsigned, size_t);
	void (*tmp_free)(unsigned, void*);
	const uint8_t* (*str_get)(void*, size_t*);
	void (*str_set)(void*, const uint8_t*, size_t);
	const uint32_t* (*wstr_get)(void*, size_t*);
	void (*wstr_set)(void*, const uint32_t*, size_t);
	size_t string_size;
	size_t wstring_size;
	size_t (*cont_size)(void*);
	void (*cont_clear)(void*);
	void (*cont_add_ptr)(void*, void*);
	void (*cont_add_class)(void*, void*);
	void* (*cont_iter)(void*);
	bool (*iter_has_next)(void*);
	void* (*iter_next)(void*);
	void (*iter_free)(void*);
};

struct OhplSerField
{
	const char* name;
	const char* class_name;
	size_t offset;
	size_t size;
	unsigned type;
	unsigned main_type;
	size_t array_size;
};

struct OhplLogicTimer
{
	double localTime;
	double localTimeAdd;
	double speedMul;
	int maxUpdates;
	int updateCount;
};

struct OhplHcField
{
	const uint8_t* key;
	size_t key_n;
	const uint8_t* val;
	size_t val_n;
};

struct OhplHcSplitter;

extern "C" {
	void ohpl_ser_register(const char* name, const char* parent, const OhplSerField* fields, size_t n, size_t size, void* (*create)());
	void ohpl_ser_save(const OhplSerHost* host, OhplXmlDoc* doc, void* obj, const char* root);
	void ohpl_ser_load(const OhplSerHost* host, const OhplXmlDoc* doc, void* obj);

	void ohpl_lt_init(OhplLogicTimer* t, int updatesPerSec, unsigned long now);
	void ohpl_lt_reset(OhplLogicTimer* t, unsigned long now);
	bool ohpl_lt_want_update(OhplLogicTimer* t, unsigned long now);
	void ohpl_lt_end_update_loop(OhplLogicTimer* t, unsigned long now);
	void ohpl_lt_set_updates_per_sec(OhplLogicTimer* t, int updatesPerSec, unsigned long now);
	int ohpl_lt_updates_per_sec(OhplLogicTimer* t);
	float ohpl_lt_step_size(OhplLogicTimer* t);

	void ohpl_hc_parse(const char* s, size_t n, void (*sink)(void*, const char*, size_t, const char*, size_t), void* ctx);
	void ohpl_hc_json_escape(const char* s, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_hc_response_json(bool ok, const char* err, size_t errN, const OhplHcField* fields, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	OhplHcSplitter* ohpl_hc_splitter_new();
	void ohpl_hc_splitter_free(OhplHcSplitter* s);
	void ohpl_hc_splitter_push(OhplHcSplitter* s, const char* data, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);

	void ohpl_diag_json_escape(const char* s, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_diag_num(float x, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_diag_pixel_stats(const float* px, int w, int h, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_diag_frame_stats(const unsigned char* px, int w, int h, int bpp, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_diag_gamma_lut(float gamma, unsigned char* out);
}

#endif // HPL_RUST_SYSTEM_H
