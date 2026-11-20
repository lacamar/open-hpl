#ifndef HPL_RUST_GL_RES_H
#define HPL_RUST_GL_RES_H

// C ABI of rust/src/backend/gl_res/ffi.rs: GL texture, vertex buffer, framebuffer and GLSL
// resources behind OHPL_IMPL=rust. The C++ classes keep their CPU data and hold arena handles.

#include <cstddef>
#include <cstdint>
#include "impl/RustGl.h"

namespace hpl {

struct OhplTexIo {
	int type, usage, filter, wrap[3], compareMode, compareFunc, animMode, pixelFormat;
	unsigned downLevel;
	int memory;
	cVector3l size, minSize;
	float frameTime, aniso;
	bool useMips, srgb, compressed;
};
static_assert(sizeof(OhplTexIo) == 84, "OhplTexIo layout");

struct OhplTexLevel { const unsigned char *data; int size; };
struct OhplTexFace { const OhplTexLevel *levels; int numMips; cVector3l size; int format; };
struct OhplTexRes { int warnPow2; };
struct OhplVbArr { int kind, format, elementNum, programVar; size_t size; void *ptr; };
struct OhplDsbOut { unsigned id, kind; };
static_assert(sizeof(OhplTexLevel) == 16 && sizeof(OhplTexFace) == 32 && sizeof(OhplVbArr) == 32, "gl res layout");

extern "C" {

uint64_t ohpl_gltex_new();
void ohpl_gltex_free(uint64_t);
void ohpl_gltex_generate_handles(uint64_t, int);
size_t ohpl_gltex_handle_count(uint64_t);
bool ohpl_gltex_contains_data(uint64_t);
unsigned ohpl_gltex_handle(uint64_t, float count);
bool ohpl_gltex_create_faces(uint64_t, OhplTexIo*, int idx, const OhplTexFace*, int n, OhplTexRes*);
bool ohpl_gltex_create_raw(uint64_t, OhplTexIo*, OhplGlV3 size, int format, const void *data);
void ohpl_gltex_create_raw_end(uint64_t, const OhplTexIo*);
void ohpl_gltex_set_raw_data(uint64_t, const OhplTexIo*, int level, OhplGlV3 offset, OhplGlV3 size, int format, const void *data);
bool ohpl_gltex_read_pixels(uint64_t, const OhplTexIo*, float count, bool depth, float *out);
void ohpl_gltex_set_filter(uint64_t, OhplTexIo*, int filter);
void ohpl_gltex_apply_aniso(uint64_t, const OhplTexIo*, float x);
void ohpl_gltex_set_wrap(uint64_t, OhplTexIo*, unsigned mask, int mode);
void ohpl_gltex_set_compare(uint64_t, OhplTexIo*, bool func, int value);
void ohpl_gltex_set_red_from_alpha(uint64_t, const OhplTexIo*, bool on);
void ohpl_gltex_auto_mipmaps(uint64_t, const OhplTexIo*);
int ohpl_gltex_props_begin(uint64_t, OhplTexIo*, int idx);
void ohpl_gltex_props_aniso(uint64_t, const OhplTexIo*);
void ohpl_gltex_props_end(uint64_t, const OhplTexIo*);

uint64_t ohpl_vb_new(bool vbo, int reserveVtx, int reserveIdx);
void ohpl_vb_free(uint64_t);
unsigned ohpl_vb_create_array(uint64_t, int kind, int format, int elementNum, int programVar);
void ohpl_vb_add_vec3(uint64_t, int kind, float x, float y, float z);
void ohpl_vb_add_vec4(uint64_t, int kind, float x, float y, float z, float w);
void ohpl_vb_add_color(uint64_t, int kind, float r, float g, float b, float a);
void ohpl_vb_add_index(uint64_t, unsigned);
void ohpl_vb_fill_tangents(uint64_t);
bool ohpl_vb_shadow_double(uint64_t);
unsigned ohpl_vb_transform(uint64_t, const cMatrixf*);
unsigned ohpl_vb_copy_into(uint64_t src, uint64_t dst, unsigned flags);
void* ohpl_vb_array_ptr(uint64_t, int kind);
unsigned* ohpl_vb_indices(uint64_t);
int ohpl_vb_index_num(uint64_t);
int ohpl_vb_vertex_num(uint64_t);
size_t ohpl_vb_array_count(uint64_t);
bool ohpl_vb_array_at(uint64_t, size_t i, OhplVbArr*);
void ohpl_vb_element_desc(uint64_t, int kind, int out[3]);
void ohpl_vb_resize_array(uint64_t, int kind, int n);
void ohpl_vb_resize_indices(uint64_t, int n);
int ohpl_vb_bounding_check(uint64_t);
void ohpl_vb_compile(uint64_t, int usage);
void ohpl_vb_update(uint64_t, int usage, unsigned types, bool withIndices);
void ohpl_vb_bind(uint64_t, int usage);
void ohpl_vb_unbind(uint64_t);
void ohpl_vb_draw(uint64_t, int own, int requested, int elementNum);
void ohpl_glvb_draw_indices(int own, int requested, const unsigned *indices, int count);

uint64_t ohpl_glfb_new(unsigned *id);
void ohpl_glfb_free(uint64_t);
void ohpl_glfb_detach_texture(uint64_t, int attachment, int index);
void ohpl_glfb_attach_texture(uint64_t, int attachment, int index, int texType, int mip, int extra, OhplGlV2 texSize, unsigned texHandle, OhplGlAttach*);
void ohpl_glfb_set_depth_stencil(uint64_t, bool hasBuffer, unsigned renderbuffer, int depthBits, int stencilBits);
unsigned ohpl_glfb_compile(uint64_t, unsigned occupied);
uint64_t ohpl_gldsb_new(OhplGlV2 size, int depthBits, int stencilBits, OhplDsbOut*);
void ohpl_gldsb_free(uint64_t);

uint64_t ohpl_glsh_new(int type, unsigned *id);
void ohpl_glsh_free(uint64_t);
bool ohpl_glsh_compile(uint64_t, const char *source);
const unsigned char* ohpl_glsh_log(uint64_t, size_t *len);
const unsigned char* ohpl_glsh_dump_log(uint64_t, size_t *len);

uint64_t ohpl_glprog_new(unsigned *id);
void ohpl_glprog_free(uint64_t, int64_t shader0, int64_t shader1);
void ohpl_glprog_attach(uint64_t, unsigned shader);
bool ohpl_glprog_link(uint64_t);
const unsigned char* ohpl_glprog_log(uint64_t, size_t *len);
const unsigned char* ohpl_glprog_dump_log(uint64_t, size_t *len);
void ohpl_glprog_use(uint64_t);
void ohpl_glprog_use_current(uint64_t);
void ohpl_glprog_link_sampler(uint64_t, const char *name, int unit);
bool ohpl_glprog_set_sampler(uint64_t, const char *name, int unit);
void ohpl_glprog_bind(uint64_t);
void ohpl_glprog_unbind();
int ohpl_glprog_variable_id(uint64_t, const char *name, size_t n);
bool ohpl_glprog_variable_as_id(uint64_t, const char *name, size_t n, int id);
bool ohpl_glprog_set_int(uint64_t, int id, int x);
bool ohpl_glprog_set_float(uint64_t, int id, float x);
bool ohpl_glprog_set_vec2(uint64_t, int id, float x, float y);
bool ohpl_glprog_set_vec3(uint64_t, int id, float x, float y, float z);
bool ohpl_glprog_set_vec4(uint64_t, int id, float x, float y, float z, float w);
bool ohpl_glprog_set_float_array(uint64_t, int id, const float *values, int count);
bool ohpl_glprog_set_matrix(uint64_t, int id, const float *m);
int ohpl_glprog_uniform_count(uint64_t);
int ohpl_glprog_uniform_at(uint64_t, unsigned i, char (*name)[128], float (*vals)[16]);
int ohpl_glprog_current();

}

} // namespace hpl

#endif
