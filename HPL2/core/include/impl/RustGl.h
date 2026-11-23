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

bool ohpl_gl_tex_aniso_fits(int max, float degree);
bool ohpl_gl_tex_aniso_apply(bool hasData, bool cap, int max, float x, float current);
void ohpl_gl_clock_update(OhplGlClock*, size_t handles, float step, float frameTime, int mode);
void ohpl_gl_clock_next(OhplGlClock*, size_t handles, int mode);
void ohpl_gl_clock_prev(OhplGlClock*, size_t handles, int mode);

void ohpl_gl_shader_code_lines(const char*, void (*)(void*, int, const char*, size_t), void*);
void ohpl_gl_shader_dump_name(int count, const char*, size_t, bool vertex, void (*)(void*, const unsigned char*, size_t), void*);

bool ohpl_gl_fbo_entry_accepts(int entry, int texType);

}

}

#endif
