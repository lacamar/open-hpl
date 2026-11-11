#ifndef SOMA_RUST_SOMA_SCRIPT_H
#define SOMA_RUST_SOMA_SCRIPT_H

// C ABI of rust/src/somascript/. Inputs are (pointer, length); outputs go through sinks.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct OhplGs6Str
{
	const unsigned char *ptr;
	size_t len;
};

struct OhplGs6Builder;
struct OhplGs6Enums;

// Script struct state edited in place by Rust: cPidController<float> (N = 1) and <cVector3f> (N = 3)
template <int N> struct OhplGs6Pid
{
	float p, i, d;
	int error_num, cur, last_plus_one;
	float last_error[N], last_derivative[N], last_integral[N];
	float errors[64][N];
	float time_steps[64];
};

typedef void (*OhplGs6Sink8)(void *, const unsigned char *, size_t);
typedef void (*OhplGs6Sink32)(void *, const unsigned int *, size_t);
typedef void (*OhplGs6SinkI32)(void *, const int *, size_t);
typedef void (*OhplGs6SectionSink)(void *, const unsigned char *, size_t, const unsigned char *, size_t);
typedef void (*OhplGs6RecordSink)(void *, unsigned int, size_t, const unsigned char *, size_t, const unsigned char *, size_t, int);

enum
{
	kOhplGs6RecEnum, kOhplGs6RecEnumValue, kOhplGs6RecType, kOhplGs6RecBehaviour, kOhplGs6RecMethod,
	kOhplGs6RecProp, kOhplGs6RecCast, kOhplGs6RecGlobal, kOhplGs6RecGlobalProp,
};
enum { kOhplGs6StubVoid, kOhplGs6StubDefaultInstance, kOhplGs6StubHandle, kOhplGs6StubConstruct };
enum
{
	kOhplGs6FieldString, kOhplGs6FieldBool, kOhplGs6FieldInt, kOhplGs6FieldFloat, kOhplGs6FieldVector2f,
	kOhplGs6FieldVector3f, kOhplGs6FieldMatrix, kOhplGs6FieldColor, kOhplGs6FieldId, kOhplGs6FieldVector4f,
};
enum { kOhplGs6ScopeVar, kOhplGs6ScopeArg, kOhplGs6ScopeReturn };
enum
{
	kOhplGs6ToStrFloat, kOhplGs6ToStrFloats, kOhplGs6ToStrInts, kOhplGs6ToStrRectF, kOhplGs6ToStrRectL, kOhplGs6ToStrPlane,
	kOhplGs6ToStrMatrix, kOhplGs6ToStrId, kOhplGs6ToStrInt, kOhplGs6ToStrUint, kOhplGs6ToStrBool,
};

extern "C" {
	// strings
	void ohpl_gs6_float_to_string(float f, int decimals, bool remove_zeros, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_int_padded(int x, int padding, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_clock(float s, bool h, bool m, bool sec, bool c, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_last_char(const unsigned char *s, size_t n, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_numeric_suffix(const unsigned char *s, size_t n, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_numeric_suffix_w(const unsigned int *s, size_t n, OhplGs6Sink32 sink, void *ctx);
	void ohpl_gs6_widen(const unsigned char *s, size_t n, OhplGs6Sink32 sink, void *ctx);
	void ohpl_gs6_replace_w(const unsigned int *s, size_t n, const unsigned int *old, size_t old_n, const unsigned int *nw, size_t new_n,
							OhplGs6Sink32 sink, void *ctx);
	int ohpl_gs6_find_char(const unsigned char *s, size_t n, unsigned char c, bool last);
	int ohpl_gs6_find_str(const unsigned char *s, size_t n, const unsigned char *sub, size_t m, bool last);
	int ohpl_gs6_find_str_w(const unsigned int *s, size_t n, const unsigned int *sub, size_t m, bool last);
	int ohpl_gs6_count_chars(const unsigned char *s, size_t n, const unsigned char *c, size_t m);
	unsigned int ohpl_gs6_hash(const unsigned char *s, size_t n);
	int ohpl_gs6_numeric_suffix_int(const unsigned char *s, size_t n, int dflt);
	int ohpl_gs6_numeric_suffix_int_w(const unsigned int *s, size_t n, int dflt);
	void ohpl_gs6_split(const unsigned char *s, size_t n, const unsigned char *sep, size_t m, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_split_w(const unsigned int *s, size_t n, const unsigned int *sep, size_t m, OhplGs6Sink32 sink, void *ctx);
	void ohpl_gs6_float_vec(const unsigned char *s, size_t n, const unsigned char *sep, size_t m, void (*sink)(void *, float), void *ctx);
	void ohpl_gs6_int_vec(const unsigned char *s, size_t n, const unsigned char *sep, size_t m, void (*sink)(void *, int), void *ctx);
	void ohpl_gs6_tostr(unsigned int kind, const float *f, size_t nf, const int *i, size_t ni, OhplGs6Sink8 sink, void *ctx);

	// math
	float ohpl_gs6_sigmoid_curve(float x);
	float ohpl_gs6_interpolate_sigmoid(float a, float b, float t);
	float ohpl_gs6_interpolate_linear(float a, float b, float t);
	float ohpl_gs6_interpolate_cosine(float a, float b, float t);
	float ohpl_gs6_interpolate_cubic(float x0, float x1, float x2, float x3, float t);
	float ohpl_gs6_noise_perlin_1d(float x);
	float ohpl_gs6_round(float x);
	int ohpl_gs6_get_bit(int n);
	int ohpl_gs6_set_bit_flag(int flags, int bit, bool set);
	bool ohpl_gs6_get_bit_flag(int flags, int bit);
	float ohpl_gs6_vector2_dot(const float *a, const float *b);
	void ohpl_gs6_vector2_rotate(const float *v, float angle, float *out);
	float ohpl_gs6_vector3_max_element(const float *v);
	void ohpl_gs6_catmull_rom2(const float *p0, const float *p1, const float *p2, const float *p3, float t, float *out);
	void ohpl_gs6_catmull_rom3(const float *p0, const float *p1, const float *p2, const float *p3, float t, float *out);
	void ohpl_gs6_sphere_surface_point(const float *seed, float radius, float *out);
	void ohpl_gs6_random_sphere_surface_point(float radius, float *out);
	void ohpl_gs6_random_circle_edge_point(float radius, float *out);
	void ohpl_gs6_project_on_plane(const float *n, const float *v, float *out);
	void ohpl_gs6_rgb_to_hex(const float *c, unsigned char *out);
	bool ohpl_gs6_aabb_sphere_collision(const float *mn, const float *mx, const float *c, float r);
	void ohpl_gs6_pid1_set_error_num(void *obj, int num);
	void ohpl_gs6_pid1_reset(void *obj);
	void ohpl_gs6_pid1_output(void *obj, const float *error, float dt, float *out);
	void ohpl_gs6_pid3_set_error_num(void *obj, int num);
	void ohpl_gs6_pid3_reset(void *obj);
	void ohpl_gs6_pid3_output(void *obj, const float *error, float dt, float *out);

	// builder
	OhplGs6Builder *ohpl_gs6_builder_new(const unsigned char *game_dir, size_t n, const unsigned char *game, size_t m);
	void ohpl_gs6_builder_free(OhplGs6Builder *b);
	void ohpl_gs6_builder_resolve(const OhplGs6Builder *b, const unsigned char *include, size_t n, const unsigned char *from, size_t m,
								  OhplGs6Sink8 sink, void *ctx);
	bool ohpl_gs6_builder_collect(const OhplGs6Builder *b, const unsigned char *entry, size_t n, OhplGs6SectionSink section,
								  OhplGs6Sink8 missing, OhplGs6Sink8 warning, void *ctx);
	bool ohpl_gs6_is_nosave(const unsigned char *key, size_t n);
	void ohpl_gs6_cache_key(uint64_t exe_size, int64_t mtime_sec, int64_t mtime_nsec, const unsigned char *const *names, const size_t *name_lens,
							const unsigned char *const *codes, const size_t *code_lens, size_t count, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_cache_file_name(const unsigned char *entry, size_t n, OhplGs6Sink8 sink, void *ctx);

	// api
	void ohpl_gs6_api_load(const unsigned char *text, size_t n, OhplGs6RecordSink sink, void *ctx);
	void ohpl_gs6_decl_name(const unsigned char *decl, size_t n, OhplGs6Sink8 sink, void *ctx);
	ptrdiff_t ohpl_gs6_native_decl_pick(const OhplGs6Str *cands, size_t n_cands, const OhplGs6Str *enums, size_t n_enums, const unsigned char *decl,
										size_t decl_len);
	OhplGs6Enums *ohpl_gs6_enums_new();
	void ohpl_gs6_enums_free(OhplGs6Enums *e);
	void ohpl_gs6_enums_add(OhplGs6Enums *e, const unsigned char *name, size_t n);
	void ohpl_gs6_enums_add_value(OhplGs6Enums *e, const unsigned char *name, size_t n, int value);
	void ohpl_gs6_enums_rebase(OhplGs6Enums *e, const OhplGs6Enums *base);
	void ohpl_gs6_enums_values(const OhplGs6Enums *e, OhplGs6SinkI32 sink, void *ctx);
	bool ohpl_gs6_is_lookup_name(const unsigned char *name, size_t n);
	unsigned int ohpl_gs6_stub_return_kind(bool is_void, bool inout_ref, bool is_handle);
	bool ohpl_gs6_is_script_struct(const unsigned char *kind, size_t kind_len, const unsigned char *name, size_t name_len, bool has_factory);
	bool ohpl_gs6_is_copy_ctor(const unsigned char *params, size_t params_len, const unsigned char *name, size_t name_len);
	bool ohpl_gs6_property_accessors(const unsigned char *decl, size_t decl_len, const OhplGs6Str *enums, size_t n_enums, OhplGs6Sink8 get,
									 OhplGs6Sink8 set, void *ctx);
	void ohpl_gs6_prop_name(const unsigned char *decl, size_t n, OhplGs6Sink8 sink, void *ctx);

	// globals
	void ohpl_gs6_var_set(unsigned int scope, const unsigned char *name, size_t name_len, int idx, unsigned int field, const unsigned int *words,
						  const unsigned char *str, size_t str_len);
	void ohpl_gs6_var_get(unsigned int scope, const unsigned char *name, size_t name_len, int idx, unsigned int field, unsigned int *out);
	void ohpl_gs6_var_get_str(unsigned int scope, const unsigned char *name, size_t name_len, int idx, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_reset_return();
	void ohpl_gs6_reset_all();
	void ohpl_gs6_serialize_vars(OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_deserialize_vars(const unsigned char *data, size_t n);
	bool ohpl_gs6_secret_code(const unsigned char *key, const unsigned char *x, size_t n);

	// runtime and check
	void ohpl_gs6_find_api_file(OhplGs6Str game, OhplGs6Str current_game, OhplGs6Str env, OhplGs6Str exe, OhplGs6Str source_dir, OhplGs6Sink8 sink,
								void *ctx);
	bool ohpl_gs6_split_global_marker(const unsigned char *decl, size_t n, OhplGs6Sink8 sink, void *ctx);
	bool ohpl_gs6_callback_decls(const unsigned char *name, size_t n, OhplGs6Sink8 func, OhplGs6Sink8 with_arg, OhplGs6Sink8 no_arg, void *ctx);
	void ohpl_gs6_exec_source(const unsigned char *code, size_t n, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_exec_message(const unsigned char *msg, size_t n, int row, int col, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_script_files_error(const OhplGs6Str *keys, size_t n, OhplGs6Sink8 sink, void *ctx);
	ptrdiff_t ohpl_gs6_pick_module(const OhplGs6Str *keys, const bool *loaded, size_t n, const unsigned char *needle, size_t needle_len);
	void ohpl_gs6_stub_report(const OhplGs6Str *names, const int *counts, size_t n, int top, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_stub_order(const OhplGs6Str *names, const int *counts, size_t n, size_t *out);
	void ohpl_gs6_json_escape(const unsigned char *s, size_t n, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_from_script_dir(const unsigned char *s, size_t n, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_config_scripts(const OhplGs6Str *files, size_t n, OhplGs6Sink8 sink, void *ctx);
	void ohpl_gs6_map_scripts(const unsigned char *dir, size_t n, OhplGs6Sink8 sink, void *ctx);
}

namespace OhplGs6
{
	inline const unsigned char *U(const std::string &s) { return (const unsigned char *)s.data(); }
	inline const unsigned char *U(const char *s) { return (const unsigned char *)s; }
	inline OhplGs6Str Str(const std::string &s) { return {U(s), s.size()}; }
	inline OhplGs6Str StrNull() { return {nullptr, 0}; }

	inline void ToString(void *c, const unsigned char *p, size_t n) { ((std::string *)c)->assign((const char *)p, n); }
	inline void ToWString(void *c, const unsigned int *p, size_t n) { ((std::wstring *)c)->assign((const wchar_t *)p, n); }
	inline void ToInts(void *c, const int *p, size_t n) { ((std::vector<int> *)c)->assign(p, p + n); }
	inline void AppendString(void *c, const unsigned char *p, size_t n) { ((std::vector<std::string> *)c)->emplace_back((const char *)p, n); }
	inline void AppendWString(void *c, const unsigned int *p, size_t n) { ((std::vector<std::wstring> *)c)->emplace_back((const wchar_t *)p, n); }

	template <class F> std::string Call8(F f)
	{
		std::string s;
		f(ToString, &s);
		return s;
	}
	template <class F> std::wstring Call32(F f)
	{
		std::wstring s;
		f(ToWString, &s);
		return s;
	}
}

#endif
