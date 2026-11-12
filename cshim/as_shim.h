#ifndef OHPL_AS_SHIM_H
#define OHPL_AS_SHIM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handles: engine, module, context, function are the AngelScript interface pointers. */

typedef struct OhplAsMsg {
	const char* section;
	int32_t row, col, type;
	const char* message;
} OhplAsMsg;

typedef void (*OhplAsMsgFn)(const OhplAsMsg*, void*);

void* ohpl_as_engine_new(void);
int ohpl_as_engine_release(void* e);
int ohpl_as_engine_shutdown(void* e);
int ohpl_as_set_message_cb(void* e, OhplAsMsgFn fn, void* ctx);
int ohpl_as_set_engine_property(void* e, int prop, size_t value);
int ohpl_as_set_default_namespace(void* e, const char* ns);

int ohpl_as_reg_global_function(void* e, const char* decl, void* fn, int conv);
int ohpl_as_reg_global_property(void* e, const char* decl, void* ptr);
int ohpl_as_reg_object_type(void* e, const char* name, int size, uint64_t flags);
int ohpl_as_reg_object_property(void* e, const char* obj, const char* decl, int offset);
int ohpl_as_reg_object_method(void* e, const char* obj, const char* decl, void* fn, int conv);
int ohpl_as_reg_object_behaviour(void* e, int behaviour, const char* obj, const char* decl, void* fn, int conv);
int ohpl_as_reg_enum(void* e, const char* name);
int ohpl_as_reg_enum_value(void* e, const char* name, const char* value, int v);
int ohpl_as_reg_funcdef(void* e, const char* decl);
int ohpl_as_reg_typedef(void* e, const char* ty, const char* decl);
int ohpl_as_type_id_by_decl(void* e, const char* decl);

void* ohpl_as_get_module(void* e, const char* name, int flag);
int ohpl_as_discard_module(void* e, const char* name);
int ohpl_as_module_add_section(void* m, const char* name, const char* code, size_t len);
int ohpl_as_module_build(void* m);
void* ohpl_as_module_function_by_name(void* m, const char* name);
unsigned ohpl_as_module_function_count(void* m);
void* ohpl_as_module_function_by_index(void* m, unsigned i);
unsigned ohpl_as_module_global_count(void* m);
int ohpl_as_module_global_index_by_name(void* m, const char* name);

const char* ohpl_as_func_name(void* f);
const char* ohpl_as_func_decl(void* f, int obj, int ns, int names);
int ohpl_as_func_param_count(void* f);

void* ohpl_as_ctx_new(void* e);
int ohpl_as_ctx_release(void* c);
int ohpl_as_ctx_prepare(void* c, void* func);
int ohpl_as_ctx_unprepare(void* c);
int ohpl_as_ctx_execute(void* c);
int ohpl_as_ctx_abort(void* c);
int ohpl_as_ctx_state(void* c);
int ohpl_as_ctx_set_object(void* c, void* obj);
int ohpl_as_ctx_set_arg_dword(void* c, unsigned arg, uint32_t v);
int ohpl_as_ctx_set_arg_qword(void* c, unsigned arg, uint64_t v);
int ohpl_as_ctx_set_arg_float(void* c, unsigned arg, float v);
int ohpl_as_ctx_set_arg_double(void* c, unsigned arg, double v);
int ohpl_as_ctx_set_arg_address(void* c, unsigned arg, void* v);
int ohpl_as_ctx_set_arg_object(void* c, unsigned arg, void* v);
uint32_t ohpl_as_ctx_ret_dword(void* c);
uint64_t ohpl_as_ctx_ret_qword(void* c);
float ohpl_as_ctx_ret_float(void* c);
double ohpl_as_ctx_ret_double(void* c);
void* ohpl_as_ctx_ret_address(void* c);
void* ohpl_as_ctx_ret_object(void* c);
const char* ohpl_as_ctx_exception_string(void* c);
int ohpl_as_ctx_exception_line(void* c, int* col, const char** section);

/* Add-ons (string, array, scripthelper) are compiled into HPL2; it installs this table before first use. */
typedef struct OhplAsAddons {
	void (*register_addons)(void* engine);
	int (*execute_string)(void* engine, const char* code, void* module);
	void* (*str_new)(const char* p, size_t n);
	unsigned (*array_size)(const void* a);
	void* (*array_at)(void* a, unsigned i);
} OhplAsAddons;

void ohpl_as_set_addons(const OhplAsAddons* t);
void ohpl_as_register_addons(void* e);
int ohpl_as_execute_string(void* e, const char* code, void* module);
const char* ohpl_as_str_data(const void* s);
size_t ohpl_as_str_len(const void* s);
void* ohpl_as_str_new(const char* p, size_t n);
unsigned ohpl_as_array_size(const void* a);
void* ohpl_as_array_at(void* a, unsigned i);

#ifdef __cplusplus
}
#endif
#endif
