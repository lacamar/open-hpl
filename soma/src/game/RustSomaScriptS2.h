#ifndef SOMA_RUST_SOMA_SCRIPT_S2_H
#define SOMA_RUST_SOMA_SCRIPT_S2_H

// C ABI of rust/src/somascript/s2_*: the script natives and the API registration (OHPL_IMPL=rust).

#include <cstddef>
#include <cstdint>

#include "RustSomaScript.h"
#include "impl/RustBackends.h"

struct OhplS2Api;

struct OhplS2Defaults
{
	size_t size;
	const unsigned char *bytes;
	size_t nstr;
	const int *str_offsets;
};

typedef void (*OhplS2CountSink)(void *, const unsigned char *, size_t, int);

// What Rust calls back into: C++-only containers and the natives of the other classes
struct OhplS2HostOps
{
	int (*struct_defaults)(const unsigned char *, size_t, OhplS2Defaults *);
	const void *(*intern_empty)();
	int (*native_offset)(const unsigned char *, size_t, const unsigned char *, size_t, int);
	void (*register_step)(void *, unsigned);
	void (*sset_reset)(void *);
	void (*sset_assign)(void *, void *);
	void (*sset_add)(void *, const void *);
	void (*sset_erase)(void *, const void *);
	int (*sset_count)(void *, const void *);
	int (*sset_size)(void *);
	void (*sset_to_array)(void *, void *);
	void (*arr_insert_last)(void *, void *);
	void (*arr_insert_at)(void *, unsigned, void *);
	void (*arr_remove_last)(void *);
	void (*arr_remove_at)(void *, unsigned);
	unsigned (*arr_size)(const void *);
	void (*arr_resize)(void *, unsigned);
	bool (*run_global_func)(const unsigned char *, size_t, const unsigned char *, size_t, const unsigned char *, size_t);
	size_t (*gen_count)();
	const void *(*gen_op)(size_t);
	bool (*global_funcs)(size_t, const unsigned char **, const unsigned char **, const unsigned char **);
	float (*easing)(int, float);
};

enum { kOhplS2StepLux, kOhplS2StepGlobals, kOhplS2StepClasses };
enum { kOhplS2ListErrors, kOhplS2ListWarnings, kOhplS2ListRemaps };

extern "C" {
	void ohpl_s2_install_host(const OhplS2HostOps *);
	void ohpl_s2_register_natives(void *engine);
	void ohpl_s2_configure_engine(void *engine);
	void ohpl_s2_register_strings(void *engine);
	void ohpl_s2_register_array_extras(void *engine);
	OhplS2Api *ohpl_s2_api_new();
	void ohpl_s2_api_free(OhplS2Api *);
	void ohpl_s2_api_load(OhplS2Api *, const unsigned char *, size_t);
	void ohpl_s2_api_rebase(OhplS2Api *, const OhplS2Api *);
	void ohpl_s2_api_register_types(OhplS2Api *, void *engine);
	int ohpl_s2_api_register(OhplS2Api *, void *engine);
	void ohpl_s2_api_list(const OhplS2Api *, unsigned, OhplGs6Sink8, void *);
	void ohpl_s2_native_decl(const unsigned char *, size_t, const unsigned char *, size_t, OhplGs6Sink8, void *);
	bool ohpl_s2_is_dummy(void *);
	void ohpl_s2_stub_call(void *generic);
	void ohpl_s2_release_handle_args(void *generic);
	void *ohpl_s2_dummy_of(void *engine, const char *type);
	void *ohpl_s2_new_struct(const char *type, bool owned);
	bool ohpl_s2_is_stub(void *func);
	void ohpl_s2_set_indirect(const unsigned char *, size_t, int);
	int ohpl_s2_indirect_offset(const unsigned char *, size_t, const unsigned char *, size_t);
	void *ohpl_s2_new_prop_block(const unsigned char *, size_t);
	void ohpl_s2_free_prop_block(const unsigned char *, size_t, void *);
	void ohpl_s2_stub_counts(OhplS2CountSink, void *);
	int ohpl_s2_dump_decls(void *engine, const char *path);
}

inline bool OhplS2Rust() { return hpl::OhplBackendRustRequested(); }
void OhplS2InstallHost();

#endif
