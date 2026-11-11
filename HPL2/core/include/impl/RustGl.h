#ifndef HPL_RUST_GL_H
#define HPL_RUST_GL_H

// C ABI of rust/src/gl/ffi.rs: GL enum tables, state filters, vertex-buffer, texture,
// framebuffer and program plans. No gl* call crosses the boundary.

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"

namespace hpl {

struct OhplGlV2 { int x, y; };
struct OhplGlV3 { int x, y, z; };
inline OhplGlV2 OhplGl(const cVector2l &v) { OhplGlV2 r = {v.x, v.y}; return r; }
inline OhplGlV3 OhplGl(const cVector3l &v) { OhplGlV3 r = {v.x, v.y, v.z}; return r; }

enum { kOhplGlMaxOps = 128, kOhplGlMaxTextureUnits = 16, kOhplGlMaxColorBuffers = 5 };

struct OhplGlState {
	bool colorWrite[4];
	bool depthWrite, depthTest, alphaTest, scissor, blend, cullActive;
	int depthFunc, alphaFunc, cullMode, activeUnit;
	float alphaRef;
	unsigned target[kOhplGlMaxTextureUnits];
};
static_assert(sizeof(OhplGlState) == 96, "OhplGlState layout");

struct OhplGlTexPlan {
	int activeUnit;
	unsigned disable;
	bool bind;
	unsigned enable;
};

struct OhplGlViewport {
	cVector2l total, size, pos;
	int gl[4];
};

struct OhplGlElem {
	int kind, format, elementNum;
	unsigned handle;
};

enum { kOhplGlOpClientActive = 1, kOhplGlOpEnable, kOhplGlOpDisable, kOhplGlOpBindBuffer, kOhplGlOpPointer };
enum { kOhplGlSrcOffset = 0, kOhplGlSrcElement, kOhplGlSrcBatchVertex, kOhplGlSrcBatchTexCoord };

struct OhplGlOp {
	unsigned kind, array;
	int size;
	unsigned glType;
	int stride;
	unsigned src, index;
};
static_assert(sizeof(OhplGlOp) == 28, "OhplGlOp layout");

struct OhplGlCreateIn {
	int type, format;
	cVector3l size, minSize;
	int numMips;
	unsigned downLevel;
	bool generateMips, checkResize;
};

struct OhplGlCreatePlan {
	bool warnPow2, compressed, resize;
	cVector3l size;
	int resizeBytes, startLevel, endLevel, maxLevel;
	bool buildMips;
};

struct OhplGlUpload {
	unsigned kind, target;
	int level;
	unsigned internal;
	cVector3l size;
	unsigned format, glType;
	bool swizzle;
	int bytes;
};

enum { kOhplGlMipDisable = 0, kOhplGlMipGenerate, kOhplGlMipBuild1D, kOhplGlMipBuild2D, kOhplGlMipNone };

struct OhplGlMipPlan {
	unsigned action, target, internal, format;
	bool disableMips;
	int memory;
};

struct OhplGlProps {
	unsigned minFilter;
	bool rect;
	unsigned wrap[3];
};

struct OhplGlClock {
	float count, dir;
};

enum { kOhplGlDsNone = 0, kOhplGlDsPacked, kOhplGlDsErrNoPacked, kOhplGlDsErrPackedBits, kOhplGlDsDepth, kOhplGlDsErrDepth, kOhplGlDsStencil, kOhplGlDsErrStencil };

struct OhplGlDsPlan {
	unsigned kind, format;
};

enum { kOhplGlCallNone = 0, kOhplGlCallTexture2D, kOhplGlCallTexture3D };
enum { kOhplGlSlotColor = 0, kOhplGlSlotDepth, kOhplGlSlotStencil, kOhplGlSlotNone };

struct OhplGlAttach {
	unsigned call, attachment, target, slot;
	int colorIndex;
	bool explicitMip;
	cVector2l firstSize;
};

enum { kOhplGlDrawNone = 0, kOhplGlDrawSingle, kOhplGlDrawMulti };

struct OhplGlDrawPlan {
	unsigned mode;
	int count;
	unsigned buffers[kOhplGlMaxColorBuffers];
};

enum {
	kOhplGlStatusComplete = 0, kOhplGlStatusUnsupported, kOhplGlStatusAttachment, kOhplGlStatusMissing,
	kOhplGlStatusDimensions, kOhplGlStatusFormats, kOhplGlStatusDrawBuffer, kOhplGlStatusReadBuffer, kOhplGlStatusUnknown
};

static_assert(sizeof(OhplGlElem) == 16 && sizeof(OhplGlTexPlan) == 16 && sizeof(OhplGlViewport) == 40, "gl plan layout");
static_assert(sizeof(OhplGlCreateIn) == 44 && sizeof(OhplGlCreatePlan) == 36 && sizeof(OhplGlUpload) == 44, "gl texture plan layout");
static_assert(sizeof(OhplGlMipPlan) == 24 && sizeof(OhplGlProps) == 20 && sizeof(OhplGlClock) == 8, "gl texture plan layout");
static_assert(sizeof(OhplGlAttach) == 32 && sizeof(OhplGlDrawPlan) == 28, "gl framebuffer plan layout");

struct OhplGlParams;

extern "C" {

unsigned ohpl_gl_vertex_array(int);
unsigned ohpl_gl_vertex_type(int);
unsigned ohpl_gl_draw_type_mode(int);
unsigned ohpl_gl_usage(int);
unsigned ohpl_gl_wrap(int);
unsigned ohpl_gl_pixel_format(int);
unsigned ohpl_gl_pixel_internal_format(int);
unsigned ohpl_gl_compression_format(int);
unsigned ohpl_gl_texture_target(int);
unsigned ohpl_gl_blend(int);
unsigned ohpl_gl_texture_param(int);
unsigned ohpl_gl_texture_op(int);
unsigned ohpl_gl_texture_source(int);
unsigned ohpl_gl_compare_mode(int);
unsigned ohpl_gl_compare_func(int);
unsigned ohpl_gl_texture_func(int);
unsigned ohpl_gl_compare(int);
unsigned ohpl_gl_stencil_op(int);
unsigned ohpl_gl_shader_type(int);
unsigned ohpl_gl_depth_renderbuffer(int);
unsigned ohpl_gl_stencil_renderbuffer(int);
unsigned ohpl_gl_srgb(unsigned);
unsigned ohpl_gl_texture_binding(unsigned);
int ohpl_gl_uniform_components(unsigned);

void ohpl_gl_state_new(OhplGlState*);
void ohpl_gl_state_setup(OhplGlState*);
bool ohpl_gl_state_color_write(OhplGlState*, bool, bool, bool, bool);
bool ohpl_gl_state_depth_write(OhplGlState*, bool);
bool ohpl_gl_state_depth_test(OhplGlState*, bool);
bool ohpl_gl_state_depth_func(OhplGlState*, int);
bool ohpl_gl_state_alpha_test(OhplGlState*, bool);
bool ohpl_gl_state_alpha_func(OhplGlState*, int, float);
bool ohpl_gl_state_scissor(OhplGlState*, bool);
bool ohpl_gl_state_blend(OhplGlState*, bool);
bool ohpl_gl_state_active_unit(OhplGlState*, unsigned);
void ohpl_gl_state_cull_active(OhplGlState*, bool);
void ohpl_gl_state_cull_mode(OhplGlState*, int);
void ohpl_gl_state_set_texture(OhplGlState*, unsigned unit, unsigned newTarget, bool hasTexture, bool multitexture, OhplGlTexPlan*);
size_t ohpl_gl_draw_targets(const int*, size_t, unsigned*, size_t);
void ohpl_gl_scissor_rect(int targetH, OhplGlV2 pos, OhplGlV2 size, int out[4]);
void ohpl_gl_viewport(OhplGlV2 target, OhplGlV2 pos, OhplGlV2 size, OhplGlViewport*);
void ohpl_gl_copy_to_texture(OhplGlV2 fbPos, OhplGlV2 fbSize, OhplGlV2 fbTotal, OhplGlV2 pos, OhplGlV2 size, OhplGlV2 texOffset, int texHeight, int out[6]);
void ohpl_gl_read_rect(OhplGlV2 fbPos, OhplGlV2 fbSize, OhplGlV2 fbTotal, OhplGlV2 pos, OhplGlV2 size, int out[4]);

size_t ohpl_gl_vb_bind_vbo(const OhplGlElem*, size_t, OhplGlOp*);
size_t ohpl_gl_vb_bind_array(const OhplGlElem*, size_t, OhplGlOp*);
size_t ohpl_gl_vb_unbind(const OhplGlElem*, size_t, OhplGlOp*);
size_t ohpl_gl_batch_states(unsigned flags, OhplGlOp*);
size_t ohpl_gl_batch_arrays(int stride, OhplGlOp*);
size_t ohpl_gl_batch_texture_unit(unsigned unit, bool active, int stride, OhplGlOp*);
unsigned ohpl_gl_vb_draw_mode(int own, int requested);
int ohpl_gl_vb_draw_count(int elementNum, int indexNum);
size_t ohpl_gl_vb_vec3_components(int elementNum, float x, float y, float z, float out[4]);
size_t ohpl_gl_vb_color_components(int format);
unsigned ohpl_gl_vb_copy_flags(unsigned requested, unsigned present);
int ohpl_gl_vb_vertex_num(int size, int elementNum, bool shadow);
int ohpl_gl_vb_bounding_check(unsigned flags, int format);
size_t ohpl_gl_vb_shadow_double_extra(size_t size);
void ohpl_gl_vb_shadow_double_fill(int format, unsigned* data, size_t total, size_t size);
unsigned ohpl_gl_vb_transform(const cMatrixf*, float* pos, size_t posLen, float* normal, size_t normalLen, float* tangent, size_t tangentLen, int vtxNum, int stride, bool shadow);

void ohpl_gl_tex_plan_create(const OhplGlCreateIn*, OhplGlCreatePlan*);
OhplGlV3 ohpl_gl_tex_level_size(OhplGlV3, int count);
void ohpl_gl_tex_plan_upload(int type, bool srgb, int format, int level, int dataSize, OhplGlV3 size, int face, OhplGlUpload*);
void ohpl_gl_tex_plan_mips(int type, int usage, bool fboExt, bool srgb, int format, OhplGlV3 size, int face, OhplGlMipPlan*);
void ohpl_gl_tex_plan_props(int type, bool useMips, int filter, const int wrap[3], OhplGlProps*);
bool ohpl_gl_tex_aniso_fits(int max, float degree);
void ohpl_gl_tex_filter_params(int filter, bool useMips, int type, unsigned out[2]);
bool ohpl_gl_tex_aniso_apply(bool hasData, bool cap, int max, float x, float current);
void ohpl_gl_clock_update(OhplGlClock*, size_t handles, float step, float frameTime, int mode);
void ohpl_gl_clock_next(OhplGlClock*, size_t handles, int mode);
void ohpl_gl_clock_prev(OhplGlClock*, size_t handles, int mode);
int64_t ohpl_gl_tex_handle_slot(bool hasData, size_t handles, float count);
OhplGlV3 ohpl_gl_tex_raw_size(OhplGlV3, int format, int* bytes);

OhplGlParams* ohpl_gl_params_new();
void ohpl_gl_params_free(OhplGlParams*);
int ohpl_gl_params_find(const OhplGlParams*, const char*, size_t);
int ohpl_gl_params_add(OhplGlParams*, const char*, size_t, int location);
bool ohpl_gl_params_location(const OhplGlParams*, int id, int* out);
int ohpl_gl_params_as_id_begin(const OhplGlParams*, const char*, size_t, int id);
bool ohpl_gl_params_as_id_finish(OhplGlParams*, const char*, size_t, int id, int location);
bool ohpl_gl_prog_bind(int* current, unsigned handle);
bool ohpl_gl_prog_unbind(int* current);
void ohpl_gl_prog_forget(int* current, unsigned handle);
void ohpl_gl_shader_code_lines(const char*, void (*)(void*, int, const char*, size_t), void*);
void ohpl_gl_shader_dump_name(int count, const char*, size_t, bool vertex, void (*)(void*, const unsigned char*, size_t), void*);

void ohpl_gl_fbo_plan_depth_stencil(int depthBits, int stencilBits, bool packedExt, OhplGlDsPlan*);
bool ohpl_gl_fbo_entry_accepts(int entry, int texType);
void ohpl_gl_fbo_plan_attach(unsigned attachment, int index, int texType, int mip, int extra, OhplGlV2 texSize, OhplGlAttach*);
void ohpl_gl_fbo_plan_draw_buffers(unsigned occupied, OhplGlDrawPlan*);
unsigned ohpl_gl_fbo_status_class(unsigned);

}

}

#endif
