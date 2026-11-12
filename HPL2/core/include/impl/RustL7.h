#ifndef HPL_RUST_L7_H
#define HPL_RUST_L7_H

// C ABI of rust/src/script (stage L7): script engine and cSqScript. Used when OHPL_IMPL=rust.

#include <cstddef>

#include "impl/RustBackends.h"

struct OhplL7Engine;
struct OhplL7Script;

extern "C" {

OhplL7Engine* ohpl_l7_engine_new();
void ohpl_l7_engine_free(OhplL7Engine* e);
void* ohpl_l7_engine_raw(const OhplL7Engine* e);
bool ohpl_l7_add_func(const OhplL7Engine* e, const char* decl, const void* f);
bool ohpl_l7_add_var(const OhplL7Engine* e, const char* decl, void* p);

OhplL7Script* ohpl_l7_script_new(const OhplL7Engine* e, int handle, int rand);
void ohpl_l7_script_free(OhplL7Script* s);
// 0 ok, 1 AddScriptSection failed, 2 build failed
int ohpl_l7_script_build(OhplL7Script* s, const char* src, size_t len, void (*msg_sink)(void*, const char*, size_t), void* msg_ctx,
						 void (*chunk_sink)(void*, const char*, size_t), void* chunk_ctx);
int ohpl_l7_script_func_handle(OhplL7Script* s, const char* name);
bool ohpl_l7_script_run_line(OhplL7Script* s, const char* line);
bool ohpl_l7_script_run_handle(OhplL7Script* s, int handle);

// 0 when every registration kind round-trips through a script, else a bitmask of failures
unsigned ohpl_l7_selftest();

}

// installs the string/array/scripthelper add-ons into the shim; call before ohpl_l7_engine_new
void OhplL7InstallAddons();

#endif
