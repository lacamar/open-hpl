#ifndef OHPL_AS_SHIM_ADDON_H
#define OHPL_AS_SHIM_ADDON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* what the Rust array/string/scripthelper add-ons need from the AngelScript interfaces */

void* ohpl_as3_active_ctx(void);
void ohpl_as3_lock(int acquire);

void* ohpl_as3_type_engine(void* t);
int ohpl_as3_type_sub_id(void* t);
void* ohpl_as3_type_sub(void* t);
uint64_t ohpl_as3_type_flags(void* t);
void ohpl_as3_type_addref(void* t);
void ohpl_as3_type_release(void* t);
void* ohpl_as3_type_user(void* t, uintptr_t key);
void ohpl_as3_type_set_user(void* t, void* p, uintptr_t key);
unsigned ohpl_as3_type_behaviour_count(void* t);
void* ohpl_as3_type_behaviour(void* t, unsigned i, int* beh);
unsigned ohpl_as3_type_factory_count(void* t);
void* ohpl_as3_type_factory(void* t, unsigned i);
unsigned ohpl_as3_type_method_count(void* t);
void* ohpl_as3_type_method(void* t, unsigned i);

int ohpl_as3_func_read_only(void* f);

int ohpl_as3_engine_prop(void* e, int prop);
void ohpl_as3_write_message(void* e, const char* section, int row, int col, int type, const char* msg);
const char* ohpl_as3_type_decl(void* e, int type_id, int ns);
int ohpl_as3_assign_object(void* e, void* dst, void* src, void* type);
void ohpl_as3_addref_object(void* e, void* obj, void* type);
void ohpl_as3_release_object(void* e, void* obj, void* type);
void ohpl_as3_notify_gc(void* e, void* obj, void* type);
void ohpl_as3_gc_enum(void* e, void* obj);
void ohpl_as3_gc_forward_enum(void* e, void* obj, void* type);
void* ohpl_as3_request_ctx(void* e);
void ohpl_as3_return_ctx(void* e, void* c);
void ohpl_as3_type_cleanup_cb(void* e, void (*cb)(void*), uintptr_t key);
int ohpl_as3_reg_default_array(void* e, const char* decl);

typedef struct OhplAs3StringFactory {
	const void* (*get)(const char* data, unsigned len);
	int (*release)(const void* s);
	int (*raw)(const void* s, char* data, unsigned* len);
} OhplAs3StringFactory;
int ohpl_as3_reg_string_factory(void* e, const char* type, const OhplAs3StringFactory* f);

void* ohpl_as3_ctx_engine(void* c);
int ohpl_as3_ctx_set_exception(void* c, const char* info);
int ohpl_as3_ctx_push(void* c);
int ohpl_as3_ctx_pop(void* c);
uint8_t ohpl_as3_ctx_ret_byte(void* c);
void* ohpl_as3_ctx_ret_addr_of(void* c);

int ohpl_as3_compile_function(void* m, const char* name, const char* code, void** out);
void ohpl_as3_func_release(void* f);

#ifdef __cplusplus
}
#endif
#endif
