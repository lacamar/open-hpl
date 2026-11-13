#ifndef HPL_RUST_GRAPHICS_H5B_H
#define HPL_RUST_GRAPHICS_H5B_H

// C ABI of rust/src/graphics/h5b_ffi.rs and the facade that lets the Rust `Gfx` trait drive a C++
// iLowLevelGraphics (the retained Vulkan backend). Operation numbers follow rust/src/graphics/h5b_ops.rs.

#include <cstddef>
#include <cstdint>

namespace hpl {

class iLowLevelGraphics;

enum OhplH5bOp {
	kOhplH5b_INIT,
	kOhplH5b_GPU_PROGRAM_FORMAT,
	kOhplH5b_CAPS,
	kOhplH5b_RENDERER_DESCRIPTION,
	kOhplH5b_WINDOW_HANDLE,
	kOhplH5b_SET_COLOR_OUTPUT,
	kOhplH5b_COLOR_OUTPUT,
	kOhplH5b_HDR_HEADROOM,
	kOhplH5b_SCREEN_FLOAT_STATS,
	kOhplH5b_SHOW_CURSOR,
	kOhplH5b_SET_WINDOW_GRAB,
	kOhplH5b_SET_RELATIVE_MOUSE,
	kOhplH5b_SET_WINDOW_CAPTION,
	kOhplH5b_WINDOW_MOUSE_FOCUS,
	kOhplH5b_WINDOW_INPUT_FOCUS,
	kOhplH5b_WINDOW_IS_VISIBLE,
	kOhplH5b_MULTISAMPLING,
	kOhplH5b_SCREEN_SIZE_FLOAT,
	kOhplH5b_SCREEN_SIZE_INT,
	kOhplH5b_CHECK_UPDATE_SCREEN_SIZE,
	kOhplH5b_FORCE_WINDOW_SIZE,
	kOhplH5b_FULLSCREEN_ACTIVE,
	kOhplH5b_SET_VSYNC,
	kOhplH5b_SET_MULTISAMPLING_ACTIVE,
	kOhplH5b_SET_GAMMA,
	kOhplH5b_GAMMA,
	kOhplH5b_CREATE_FONT,
	kOhplH5b_CREATE_PROGRAM,
	kOhplH5b_CREATE_SHADER,
	kOhplH5b_CREATE_TEXTURE,
	kOhplH5b_CREATE_VERTEX_BUFFER,
	kOhplH5b_CREATE_FRAME_BUFFER,
	kOhplH5b_CREATE_DEPTH_STENCIL,
	kOhplH5b_CREATE_OCCLUSION_QUERY,
	kOhplH5b_DESTROY_FONT,
	kOhplH5b_DESTROY_PROGRAM,
	kOhplH5b_DESTROY_SHADER,
	kOhplH5b_DESTROY_TEXTURE,
	kOhplH5b_DESTROY_VERTEX_BUFFER,
	kOhplH5b_DESTROY_FRAME_BUFFER,
	kOhplH5b_DESTROY_DEPTH_STENCIL,
	kOhplH5b_DESTROY_OCCLUSION_QUERY,
	kOhplH5b_CLEAR_FRAME_BUFFER,
	kOhplH5b_SET_CLEAR_COLOR,
	kOhplH5b_SET_CLEAR_DEPTH,
	kOhplH5b_SET_CLEAR_STENCIL,
	kOhplH5b_FLUSH_RENDERING,
	kOhplH5b_WAIT_AND_FINISH_RENDERING,
	kOhplH5b_SWAP_BUFFERS,
	kOhplH5b_COPY_FRAME_BUFFER_TO_TEXTURE,
	kOhplH5b_COPY_FRAME_BUFFER_TO_BITMAP,
	kOhplH5b_SET_CURRENT_FRAME_BUFFER,
	kOhplH5b_CURRENT_FRAME_BUFFER,
	kOhplH5b_SET_FRAME_BUFFER_DRAW_TARGETS,
	kOhplH5b_SET_COLOR_WRITE_ACTIVE,
	kOhplH5b_SET_DEPTH_WRITE_ACTIVE,
	kOhplH5b_SET_CULL_ACTIVE,
	kOhplH5b_SET_CULL_MODE,
	kOhplH5b_SET_DEPTH_TEST_ACTIVE,
	kOhplH5b_SET_DEPTH_TEST_FUNC,
	kOhplH5b_SET_ALPHA_TEST_ACTIVE,
	kOhplH5b_SET_ALPHA_TEST_FUNC,
	kOhplH5b_SET_STENCIL_ACTIVE,
	kOhplH5b_SET_STENCIL_WRITE_MASK,
	kOhplH5b_SET_STENCIL,
	kOhplH5b_SET_STENCIL_TWO_SIDE,
	kOhplH5b_SET_SCISSOR_ACTIVE,
	kOhplH5b_SET_SCISSOR_RECT,
	kOhplH5b_SET_CLIP_PLANE,
	kOhplH5b_CLIP_PLANE,
	kOhplH5b_SET_CLIP_PLANE_ACTIVE,
	kOhplH5b_SET_COLOR,
	kOhplH5b_SET_BLEND_ACTIVE,
	kOhplH5b_SET_BLEND_FUNC,
	kOhplH5b_SET_BLEND_FUNC_SEPARATE,
	kOhplH5b_SET_POLYGON_OFFSET_ACTIVE,
	kOhplH5b_SET_DEPTH_CLAMP_ACTIVE,
	kOhplH5b_SET_POLYGON_OFFSET,
	kOhplH5b_PUSH_MATRIX,
	kOhplH5b_POP_MATRIX,
	kOhplH5b_SET_IDENTITY_MATRIX,
	kOhplH5b_SET_MATRIX,
	kOhplH5b_SET_ORTHO_SIZE,
	kOhplH5b_SET_ORTHO_BOX,
	kOhplH5b_SET_TEXTURE,
	kOhplH5b_SET_ACTIVE_TEXTURE_UNIT,
	kOhplH5b_SET_TEXTURE_ENV,
	kOhplH5b_SET_TEXTURE_CONSTANT_COLOR,
	kOhplH5b_TEX_CREATE_FROM_BITMAP,
	kOhplH5b_TEX_CREATE_ANIM,
	kOhplH5b_TEX_CREATE_CUBE,
	kOhplH5b_TEX_CREATE_RAW,
	kOhplH5b_TEX_SET_RAW,
	kOhplH5b_TEX_UPDATE,
	kOhplH5b_TEX_SET_FILTER,
	kOhplH5b_TEX_SET_ANISOTROPY,
	kOhplH5b_TEX_SET_WRAP_S,
	kOhplH5b_TEX_SET_WRAP_T,
	kOhplH5b_TEX_SET_WRAP_R,
	kOhplH5b_TEX_SET_WRAP_STR,
	kOhplH5b_TEX_SET_COMPARE_MODE,
	kOhplH5b_TEX_SET_COMPARE_FUNC,
	kOhplH5b_TEX_SET_RED_FROM_ALPHA,
	kOhplH5b_TEX_AUTO_MIPMAPS,
	kOhplH5b_TEX_HAS_ANIMATION,
	kOhplH5b_TEX_NEXT_FRAME,
	kOhplH5b_TEX_PREV_FRAME,
	kOhplH5b_TEX_TIME_COUNT,
	kOhplH5b_TEX_SET_TIME_COUNT,
	kOhplH5b_TEX_HANDLE,
	kOhplH5b_TEX_SIZE,
	kOhplH5b_TEX_PIXELS_RGBA_FLOAT,
	kOhplH5b_VB_CREATE_ELEMENT_ARRAY,
	kOhplH5b_VB_ADD_VEC3,
	kOhplH5b_VB_ADD_VEC4,
	kOhplH5b_VB_ADD_COLOR,
	kOhplH5b_VB_ADD_INDEX,
	kOhplH5b_VB_COMPILE,
	kOhplH5b_VB_UPDATE,
	kOhplH5b_VB_CREATE_SHADOW_DOUBLE,
	kOhplH5b_VB_TRANSFORM,
	kOhplH5b_VB_DRAW,
	kOhplH5b_VB_DRAW_INDICES,
	kOhplH5b_VB_BIND,
	kOhplH5b_VB_UNBIND,
	kOhplH5b_VB_CREATE_COPY,
	kOhplH5b_VB_BOUNDING_VOLUME,
	kOhplH5b_VB_ELEMENT_NUM,
	kOhplH5b_VB_ELEMENT_FORMAT,
	kOhplH5b_VB_ELEMENT_VAR,
	kOhplH5b_VB_VERTEX_NUM,
	kOhplH5b_VB_INDEX_NUM,
	kOhplH5b_VB_RESIZE_ARRAY,
	kOhplH5b_VB_RESIZE_INDICES,
	kOhplH5b_SHADER_FROM_FILE,
	kOhplH5b_SHADER_FROM_STRING,
	kOhplH5b_PROG_SET_SHADER,
	kOhplH5b_PROG_LINK,
	kOhplH5b_PROG_DUMP_UNIFORMS,
	kOhplH5b_PROG_BIND,
	kOhplH5b_PROG_UNBIND,
	kOhplH5b_PROG_SET_SAMPLER_TO_UNIT,
	kOhplH5b_PROG_VARIABLE_ID,
	kOhplH5b_PROG_VARIABLE_AS_ID,
	kOhplH5b_PROG_SET_INT,
	kOhplH5b_PROG_SET_FLOAT,
	kOhplH5b_PROG_SET_VEC2,
	kOhplH5b_PROG_SET_VEC3,
	kOhplH5b_PROG_SET_VEC4,
	kOhplH5b_PROG_SET_FLOAT_ARRAY,
	kOhplH5b_PROG_SET_MATRIX,
	kOhplH5b_PROG_SET_MATRIX_API,
	kOhplH5b_FB_SET_TEXTURE_2D,
	kOhplH5b_FB_SET_TEXTURE_3D,
	kOhplH5b_FB_SET_TEXTURE_CUBE,
	kOhplH5b_FB_SET_DEPTH_TEXTURE_2D,
	kOhplH5b_FB_SET_DEPTH_TEXTURE_CUBE,
	kOhplH5b_FB_SET_DEPTH_STENCIL,
	kOhplH5b_FB_COMPILE_AND_VALIDATE,
	kOhplH5b_FB_POST_BIND_UPDATE,
	kOhplH5b_OQ_BEGIN,
	kOhplH5b_OQ_END,
	kOhplH5b_OQ_FETCH_RESULTS,
	kOhplH5b_OQ_SAMPLE_COUNT,
	kOhplH5b_FONT_FROM_FONT_FILE,
	kOhplH5b_FONT_FROM_BITMAP_FILE,
	kOhplH5b_DRAW_TRIANGLE,
	kOhplH5b_DRAW_QUAD_POS,
	kOhplH5b_DRAW_QUAD_UV,
	kOhplH5b_DRAW_QUAD_UV2,
	kOhplH5b_DRAW_QUAD_VTX,
	kOhplH5b_DRAW_QUAD_VTX_COL,
	kOhplH5b_DRAW_QUAD_VTX_Z,
	kOhplH5b_DRAW_QUAD_VTX_Z_COL,
	kOhplH5b_DRAW_QUAD_MULTITEX,
	kOhplH5b_DRAW_LINE,
	kOhplH5b_DRAW_LINE_COLS,
	kOhplH5b_DRAW_BOX_MIN_MAX,
	kOhplH5b_DRAW_SPHERE,
	kOhplH5b_DRAW_SPHERE_AXES,
	kOhplH5b_DRAW_LINE_QUAD_RECT,
	kOhplH5b_DRAW_LINE_QUAD_POS,
	kOhplH5b_ADD_VERTEX_TO_BATCH,
	kOhplH5b_ADD_VERTEX_TO_BATCH_TRANSLATED,
	kOhplH5b_ADD_VERTEX_TO_BATCH_MATRIX,
	kOhplH5b_ADD_VERTEX_TO_BATCH_SIZE2D,
	kOhplH5b_ADD_VERTEX_TO_BATCH_RAW,
	kOhplH5b_ADD_INDEX_TO_BATCH,
	kOhplH5b_ADD_TEXCOORD_TO_BATCH,
	kOhplH5b_SET_BATCH_TEXTURE_UNIT_ACTIVE,
	kOhplH5b_FLUSH_TRI_BATCH,
	kOhplH5b_FLUSH_QUAD_BATCH,
	kOhplH5b_CLEAR_BATCH,
	kOhplH5b_Count
};

struct OhplH5bFontHdr {
	unsigned short first, last;
	float height;
	float ratio[2];
};
static_assert(sizeof(OhplH5bFontHdr) == 16, "OhplH5bFontHdr layout");

struct OhplH5bGlyph {
	void *gfx;
	float offset[2];
	float size[2];
	float advance;
};
static_assert(sizeof(OhplH5bGlyph) == 32, "OhplH5bGlyph layout");

extern "C" {

typedef uint64_t (*OhplH5bCallFn)(void *ctx, uint32_t op, const uint64_t *args, size_t n, uint64_t *out);

// Registers / drops the process-wide Rust Gfx over a facade.
void ohpl_h5b_gfx_attach_foreign(void *ctx, OhplH5bCallFn call);
// GL: state, draws and the batch run on the Rust context gl; resources go through the facade.
void ohpl_h5b_gfx_attach_gl(void *gl, void *ctx, OhplH5bCallFn call);
void ohpl_h5b_gfx_detach();
// Runs the seeded scenario against the facade; copies the result log to out and returns its length.
size_t ohpl_h5b_run_foreign(void *ctx, OhplH5bCallFn call, uint64_t seed, uint32_t rounds, char *out, size_t cap);
// iFontData glyph table mirror (rust/src/graphics/h5b_font.rs).
uint64_t ohpl_h5b_font_new();
void ohpl_h5b_font_free(uint64_t h);
void ohpl_h5b_font_set(uint64_t h, const OhplH5bFontHdr *hdr, const OhplH5bGlyph *glyphs, const unsigned char *present, size_t n);
bool ohpl_h5b_font_glyph(uint64_t h, size_t i, OhplH5bGlyph *out);
float ohpl_h5b_font_length(uint64_t h, float sizeX, const uint32_t *s, size_t n);
bool ohpl_h5b_font_word_wrap(uint64_t h, float maxLength, float sizeX, const uint32_t *s, size_t n, void (*sink)(void*, const uint32_t*, size_t), void *ctx);

// Runs n random draw and batch calls (Draw trait only) against the facade.
void ohpl_h5b_run_draws(void *ctx, OhplH5bCallFn call, uint64_t seed, uint32_t n);

}

// Forwards Gfx operations to an iLowLevelGraphics. Resource ids are creation order per kind;
// Wrap* adopts objects created elsewhere and returns their id.
class cRustGfxFacade
{
public:
	explicit cRustGfxFacade(iLowLevelGraphics *apLlg);
	~cRustGfxFacade();
	static uint64_t Call(void *apSelf, uint32_t alOp, const uint64_t *apArgs, size_t alCount, uint64_t *apOut);

	// Id of an object created elsewhere (a resource manager), registered on first use.
	uint32_t TextureId(class iTexture *apTex);
	uint32_t VertexBufferId(class iVertexBuffer *apVb);
	uint32_t ProgramId(class iGpuProgram *apProg);
	uint32_t FrameBufferId(class iFrameBuffer *apFb);

private:
	struct cImpl;
	cImpl *mpImpl;
};

// Process-wide Rust Gfx over the low-level graphics: the Rust GL context when apGl is set, else the
// facade alone (Vulkan). No-op unless OHPL_IMPL=rust.
struct OhplGlCtx;
void OhplH5bAttachGfx(iLowLevelGraphics *apLlg, OhplGlCtx *apGl);
void OhplH5bDetachGfx();
uint32_t OhplH5bTextureId(class iTexture *apTex);
uint32_t OhplH5bVertexBufferId(class iVertexBuffer *apVb);
uint32_t OhplH5bProgramId(class iGpuProgram *apProg);
uint32_t OhplH5bFrameBufferId(class iFrameBuffer *apFb);

}

#endif
