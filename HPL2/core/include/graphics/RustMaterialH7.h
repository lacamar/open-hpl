#ifndef HPL_RUST_MATERIAL_H7_H
#define HPL_RUST_MATERIAL_H7_H

// C ABI of rust/src/material/h7_ffi.rs (cMaterial state, material type registry, iPostEffect base state; OHPL_IMPL=rust).

#include <cstddef>
#include <cstdint>
#include "math/MathTypes.h"

namespace hpl {

struct OhplH7Mat;
struct OhplH7Pe;

extern "C" {

OhplH7Mat* ohpl_h7_mat_new(bool translucent);
void ohpl_h7_mat_free(OhplH7Mat*);
void ohpl_h7_mat_set_translucent(OhplH7Mat*, bool);
int ohpl_h7_mat_set_blend(OhplH7Mat*, int mode);
int ohpl_h7_mat_set_alpha(OhplH7Mat*, int mode);
bool ohpl_h7_mat_set_depth_test(OhplH7Mat*, bool on);
void ohpl_h7_mat_add_uv_anim(OhplH7Mat*, int kind, float speed, float amp, int axis);
void ohpl_h7_mat_clear_uv_anims(OhplH7Mat*, cMatrixf* uvOut);
void ohpl_h7_mat_update(OhplH7Mat*, float dt, cMatrixf* uvOut);

int ohpl_h7_type_kind(const char* name, size_t len);
size_t ohpl_h7_type_count();
uint32_t ohpl_h7_type_flags(int kind);
size_t ohpl_h7_type_used_textures(int kind, int* out, size_t cap);

bool ohpl_h7_pe_type_known(const char* name, size_t len);
OhplH7Pe* ohpl_h7_pe_new(bool hasType, const char* typeName, size_t len);
void ohpl_h7_pe_free(OhplH7Pe*);
void ohpl_h7_pe_set_disabled(OhplH7Pe*, bool);
bool ohpl_h7_pe_is_disabled(const OhplH7Pe*);
bool ohpl_h7_pe_is_active(const OhplH7Pe*);
bool ohpl_h7_pe_set_active(OhplH7Pe*, bool);
bool ohpl_h7_pe_accepts(const OhplH7Pe*, const char* paramsName, size_t len);
void ohpl_h7_pe_begin_render(OhplH7Pe*);
void ohpl_h7_pe_mark_final_used(OhplH7Pe*);
bool ohpl_h7_pe_needs_final_copy(const OhplH7Pe*, bool last);

}

}

#endif
