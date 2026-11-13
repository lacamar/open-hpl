#ifndef OHPL_AS_SHIM_S2_H
#define OHPL_AS_SHIM_S2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* asIScriptGeneric */
void* ohpl_as2_g_func(void* g);
void* ohpl_as2_g_engine(void* g);
void* ohpl_as2_g_object(void* g);
void* ohpl_as2_g_arg_object(void* g, unsigned i);
void* ohpl_as2_g_arg_addr(void* g, unsigned i);
void* ohpl_as2_g_ret_loc(void* g);
void ohpl_as2_g_set_ret_addr(void* g, void* p);
unsigned ohpl_as2_g_arg_count(void* g);
int ohpl_as2_g_arg_type_id(void* g, unsigned i);
void* ohpl_as2_g_aux(void* g);

/* registration; fn is a generic-convention function, aux reaches it through g_aux */
int ohpl_as2_reg_global_function(void* e, const char* decl, void* fn, void* aux);
int ohpl_as2_reg_object_method(void* e, const char* type, const char* decl, void* fn, void* aux);
int ohpl_as2_reg_object_behaviour(void* e, int behaviour, const char* type, const char* decl, void* fn, void* aux);
int ohpl_as2_reg_object_property(void* e, const char* type, const char* decl, int offset, int composite, int indirect);
int ohpl_as2_reg_interface(void* e, const char* name);
int ohpl_as2_reg_interface_method(void* e, const char* iface, const char* decl);
int ohpl_as2_reg_string_factory(void* e, const char* type);

/* engine and type queries */
void* ohpl_as2_type_by_name(void* e, const char* name);
void* ohpl_as2_type_by_id(void* e, int id);
void* ohpl_as2_func_by_id(void* e, int id);
void* ohpl_as2_global_func_by_decl(void* e, const char* decl);
unsigned ohpl_as2_global_func_count(void* e);
void* ohpl_as2_global_func_by_index(void* e, unsigned i);
int ohpl_as2_global_prop_index(void* e, const char* name);
int ohpl_as2_primitive_size(void* e, int type_id);
void* ohpl_as2_create_object(void* e, void* type);
uint32_t ohpl_as2_type_flags(void* t);
int ohpl_as2_type_size(void* t);
int ohpl_as2_type_id(void* t);
const char* ohpl_as2_type_name(void* t);
unsigned ohpl_as2_type_prop_count(void* t);
int ohpl_as2_type_prop(void* t, unsigned i, const char** name, int* type_id, int* offset);
void* ohpl_as2_type_method_by_decl(void* t, const char* decl);

/* functions */
const char* ohpl_as2_func_name(void* f);
const char* ohpl_as2_func_decl(void* f, int obj, int ns, int names);
int ohpl_as2_func_return(void* f, uint32_t* flags);
unsigned ohpl_as2_func_param_count(void* f);
int ohpl_as2_func_param(void* f, unsigned i, int* type_id, uint32_t* flags);
void* ohpl_as2_func_object_type(void* f);
void ohpl_as2_func_set_user(void* f, void* p, uintptr_t key);
void* ohpl_as2_func_user(void* f, uintptr_t key);

/* std::string / std::wstring (wchar_t units are 32-bit) */
void ohpl_as2_str_ctor(void* mem, const char* p, size_t n);
void ohpl_as2_str_dtor(void* s);
void ohpl_as2_str_assign(void* s, const char* p, size_t n);
void ohpl_as2_str_append(void* s, const char* p, size_t n);
void ohpl_as2_str_resize(void* s, size_t n);
void ohpl_as2_wstr_ctor(void* mem, const uint32_t* p, size_t n);
void ohpl_as2_wstr_dtor(void* s);
void ohpl_as2_wstr_assign(void* s, const uint32_t* p, size_t n);
void ohpl_as2_wstr_append(void* s, const uint32_t* p, size_t n);
void ohpl_as2_wstr_resize(void* s, size_t n);
const uint32_t* ohpl_as2_wstr_data(const void* s);
size_t ohpl_as2_wstr_len(const void* s);

/* diagnostics: seeded calls of the value-type and cMath/cString natives; one line per call with the
   arguments, the result and the objects after the call (hex) */
int ohpl_as2_call_corpus(void* e, unsigned seed, unsigned calls, const char* path);

/* diagnostics: every registered declaration, one per line, in registration order */
int ohpl_as2_dump_decls(void* e, const char* path);

#ifdef __cplusplus
}
#endif
#endif
