#ifndef HPL_RUST_STRING_H
#define HPL_RUST_STRING_H

// C ABI of rust/src/string_ffi.rs. Inputs are (pointer, length); outputs go through the sink.

#include <cstddef>

static_assert(sizeof(wchar_t) == 4, "wide strings cross the ABI as u32");

extern "C" {
	bool ohpl_str_sub(const char* s, size_t n, int start, int count, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_file_ext(const char* s, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_file_name(const char* s, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_file_path(const char* s, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_set_file_ext(const char* s, size_t n, const char* e, size_t en, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_set_file_path(const char* s, size_t n, const char* p, size_t pn, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_relative_path(const char* s, size_t n, const char* r, size_t rn, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_add_slash_at_end(const char* s, size_t n, char slash, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_remove_slash_at_end(const char* s, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_last_char(const char* s, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_to_lower(const char* s, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_to_upper(const char* s, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_replace_char_to(const char* s, size_t n, const char* o, size_t on, const char* nw, size_t nn, void (*sink)(void*, const char*, size_t), void* ctx);
	int ohpl_str_first_pos(const char* s, size_t n, const char* c, size_t cn);
	int ohpl_str_last_pos(const char* s, size_t n, const char* c, size_t cn);
	int ohpl_str_count_chars(const char* s, size_t n, const char* c, size_t cn);
	void ohpl_str_split(const char* s, size_t n, const char* seps, size_t sn, bool crDefault, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_numeric_suffix(const char* s, size_t n, int* index, void (*sink)(void*, const char*, size_t), void* ctx);
	bool ohpl_str_sub_w(const wchar_t* s, size_t n, int start, int count, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_file_ext_w(const wchar_t* s, size_t n, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_file_name_w(const wchar_t* s, size_t n, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_file_path_w(const wchar_t* s, size_t n, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_set_file_ext_w(const wchar_t* s, size_t n, const wchar_t* e, size_t en, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_set_file_path_w(const wchar_t* s, size_t n, const wchar_t* p, size_t pn, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_relative_path_w(const wchar_t* s, size_t n, const wchar_t* r, size_t rn, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_add_slash_at_end_w(const wchar_t* s, size_t n, wchar_t slash, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_remove_slash_at_end_w(const wchar_t* s, size_t n, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_last_char_w(const wchar_t* s, size_t n, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_to_lower_w(const wchar_t* s, size_t n, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_to_upper_w(const wchar_t* s, size_t n, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_replace_char_to_w(const wchar_t* s, size_t n, const wchar_t* o, size_t on, const wchar_t* nw, size_t nn, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	int ohpl_str_first_pos_w(const wchar_t* s, size_t n, const wchar_t* c, size_t cn);
	int ohpl_str_last_pos_w(const wchar_t* s, size_t n, const wchar_t* c, size_t cn);
	int ohpl_str_count_chars_w(const wchar_t* s, size_t n, const wchar_t* c, size_t cn);
	void ohpl_str_split_w(const wchar_t* s, size_t n, const wchar_t* seps, size_t sn, bool crDefault, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_numeric_suffix_w(const wchar_t* s, size_t n, int* index, void (*sink)(void*, const wchar_t*, size_t), void* ctx);

	void ohpl_str_replace_string_to(const char* s, size_t n, const char* o, size_t on, const char* nw, size_t nn, void (*sink)(void*, const char*, size_t), void* ctx);
	bool ohpl_str_matches_wildcard(const char* p, size_t pn, const char* s, size_t n);
	int ohpl_str_to_int(const char* s);
	float ohpl_str_to_float(const char* s);
	bool ohpl_str_to_bool(const char* s);
	bool ohpl_str_to_floats_exact(const char* s, size_t n, float* out);
	bool ohpl_str_to_ints_exact(const char* s, size_t n, int* out);
	void ohpl_str_get_int_vec(const char* s, size_t n, const char* seps, size_t sn, void (*sink)(void*, int), void* ctx);
	void ohpl_str_get_float_vec(const char* s, size_t n, const char* seps, size_t sn, void (*sink)(void*, float), void* ctx);
	void ohpl_str_int_to_string(int x, int pad, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_int_to_string_w(int x, int pad, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_uint_to_string(unsigned x, int pad, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_ulong_to_string(unsigned long x, int pad, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_ulong_to_string_w(unsigned long x, int pad, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_float_to_string(float x, int decimals, bool removeZeros, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_float_to_string_w(float x, int decimals, bool removeZeros, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_utf8_to_wide(const char* s, size_t n, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	void ohpl_str_wide_to_utf8(const wchar_t* s, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_str_wide_from_int_list(const char* s, size_t n, void (*sink)(void*, const wchar_t*, size_t), void* ctx);
	unsigned ohpl_str_hash_w(const wchar_t* s, size_t n);
	bool ohpl_str_encode_data(const void* data, size_t n, void (*sink)(void*, const char*, size_t), void* ctx);
	bool ohpl_str_decode_data(const char* s, size_t n, void* dest, size_t destN);
}

#endif
