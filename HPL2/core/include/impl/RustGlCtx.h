#ifndef HPL_RUST_GL_CTX_H
#define HPL_RUST_GL_CTX_H

// C ABI of rust/src/backend/gl_ctx/ffi.rs: the GL context behind cLowLevelGraphicsSDL and
// cOcclusionQueryOGL when OHPL_IMPL=rust. Every gl* call of those classes runs in Rust.

#include <cstddef>
#include <cstdint>
#include "impl/RustGl.h"

namespace hpl {

struct OhplGlCtx;

struct OhplGlCtxCfg {
	int width, height, display, bpp;
	bool fullscreen;
	int multisampling;
	bool headless;
	const char *caption;
	int outW, outH;
	bool outFallback;
};
static_assert(sizeof(OhplGlCtxCfg) == 56, "OhplGlCtxCfg layout");

struct OhplGlQuery {
	bool fetched;
	int lastSamples;
	unsigned id;
};
static_assert(sizeof(OhplGlQuery) == 12, "OhplGlQuery layout");

enum { kOhplGlLogInfo = 0, kOhplGlLogError, kOhplGlLogFatal, kOhplGlLogWarnBox };

extern "C" {

void ohpl_glctx_set_host_log(void (*)(int, const char*));
OhplGlCtx* ohpl_glctx_new();
void ohpl_glctx_free(OhplGlCtx*);
bool ohpl_glctx_create_window(OhplGlCtx*, OhplGlCtxCfg*);
size_t ohpl_glctx_caps_line(OhplGlCtx*, char*, size_t);
void ohpl_glctx_load(OhplGlCtx*);
void ohpl_glctx_debug_from_env(OhplGlCtx*);
void* ohpl_glctx_window(OhplGlCtx*);
void ohpl_glctx_show_cursor(OhplGlCtx*, bool);
void ohpl_glctx_window_grab(OhplGlCtx*, bool, bool);
void ohpl_glctx_relative_mouse(OhplGlCtx*, bool, bool);
void ohpl_glctx_set_caption(OhplGlCtx*, const char*);
bool ohpl_glctx_mouse_focus(OhplGlCtx*);
bool ohpl_glctx_input_focus(OhplGlCtx*);
bool ohpl_glctx_is_visible(OhplGlCtx*);
void ohpl_glctx_set_vsync(OhplGlCtx*, bool, bool);
bool ohpl_glctx_force_window_size(OhplGlCtx*, int, int);
bool ohpl_glctx_check_screen(OhplGlCtx*, OhplGlV2*);
void ohpl_glctx_setup_gl(OhplGlCtx*);
const unsigned char* ohpl_glctx_get_string(OhplGlCtx*, unsigned);
int ohpl_glctx_get_caps(OhplGlCtx*, int, bool);
void ohpl_glctx_set_multisampling_active(OhplGlCtx*, bool);

void ohpl_glctx_clear(OhplGlCtx*, unsigned);
void ohpl_glctx_set_clear_color(OhplGlCtx*, float, float, float, float);
void ohpl_glctx_set_clear_depth(OhplGlCtx*, float);
void ohpl_glctx_set_clear_stencil(OhplGlCtx*, int);
void ohpl_glctx_copy_to_texture(OhplGlCtx*, OhplGlV2, OhplGlV2, OhplGlV2, int, int);
void ohpl_glctx_read_rect(OhplGlCtx*, OhplGlV2, OhplGlV2, int (*)[4]);
void ohpl_glctx_read_pixels(OhplGlCtx*, const int (*)[4], int, void*);
void ohpl_glctx_bind_framebuffer(OhplGlCtx*, unsigned);
void ohpl_glctx_set_viewport(OhplGlCtx*, OhplGlV2, OhplGlV2, OhplGlV2);
void ohpl_glctx_set_draw_targets(OhplGlCtx*, const int*, int);
void ohpl_glctx_finish(OhplGlCtx*);
void ohpl_glctx_flush(OhplGlCtx*);
void ohpl_glctx_swap(OhplGlCtx*, float);

void ohpl_glctx_set_color_write(OhplGlCtx*, bool, bool, bool, bool);
void ohpl_glctx_set_depth_write(OhplGlCtx*, bool);
void ohpl_glctx_set_depth_test(OhplGlCtx*, bool);
void ohpl_glctx_set_depth_func(OhplGlCtx*, int);
void ohpl_glctx_set_alpha_test(OhplGlCtx*, bool);
void ohpl_glctx_set_alpha_func(OhplGlCtx*, int, float);
void ohpl_glctx_set_stencil_active(OhplGlCtx*, bool);
void ohpl_glctx_set_stencil_write_mask(OhplGlCtx*, unsigned);
void ohpl_glctx_set_stencil(OhplGlCtx*, int, int, unsigned, int, int, int);
void ohpl_glctx_set_stencil_two_side(OhplGlCtx*, int, int, int, unsigned, const int (*)[6]);
void ohpl_glctx_set_cull_active(OhplGlCtx*, bool);
void ohpl_glctx_set_cull_mode(OhplGlCtx*, int);
void ohpl_glctx_set_scissor_active(OhplGlCtx*, bool);
void ohpl_glctx_set_scissor_rect(OhplGlCtx*, int, OhplGlV2, OhplGlV2);
void ohpl_glctx_set_clip_plane(OhplGlCtx*, int, float, float, float, float);
void ohpl_glctx_set_clip_plane_active(OhplGlCtx*, int, bool);
void ohpl_glctx_set_blend_active(OhplGlCtx*, bool);
void ohpl_glctx_set_blend_func(OhplGlCtx*, int, int);
void ohpl_glctx_set_blend_func_separate(OhplGlCtx*, int, int, int, int);
void ohpl_glctx_set_depth_clamp_active(OhplGlCtx*, bool);
void ohpl_glctx_set_polygon_offset_active(OhplGlCtx*, bool);
void ohpl_glctx_set_polygon_offset(OhplGlCtx*, float, float);

void ohpl_glctx_push_matrix(OhplGlCtx*, int);
void ohpl_glctx_pop_matrix(OhplGlCtx*, int);
void ohpl_glctx_set_matrix(OhplGlCtx*, int, const float (*)[16]);
void ohpl_glctx_set_identity_matrix(OhplGlCtx*, int);
void ohpl_glctx_set_ortho(OhplGlCtx*, float, float, float, float, float, float);

void ohpl_glctx_set_texture(OhplGlCtx*, unsigned, int, bool, unsigned);
void ohpl_glctx_set_active_texture_unit(OhplGlCtx*, unsigned);
void ohpl_glctx_set_texture_env(OhplGlCtx*, int, int);
void ohpl_glctx_set_texture_constant_color(OhplGlCtx*, float, float, float, float);
void ohpl_glctx_set_color(OhplGlCtx*, float, float, float, float);

void ohpl_glctx_begin(OhplGlCtx*, unsigned);
void ohpl_glctx_end(OhplGlCtx*);
void ohpl_glctx_vertex3f(OhplGlCtx*, float, float, float);
void ohpl_glctx_color4f(OhplGlCtx*, float, float, float, float);
void ohpl_glctx_texcoord2f(OhplGlCtx*, float, float);
void ohpl_glctx_texcoord3f(OhplGlCtx*, float, float, float);
void ohpl_glctx_multitexcoord2f(OhplGlCtx*, unsigned, float, float);
void ohpl_glctx_multitexcoord3f(OhplGlCtx*, unsigned, float, float, float);

void ohpl_glctx_batch_texture_unit(OhplGlCtx*, unsigned, bool, int, const float*, float* const*);
void ohpl_glctx_flush_batch(OhplGlCtx*, bool, unsigned, int, int, const void*, const float*, float* const*);

void ohpl_glq_create(OhplGlQuery*);
void ohpl_glq_destroy(const OhplGlQuery*);
void ohpl_glq_begin(OhplGlQuery*);
void ohpl_glq_end(const OhplGlQuery*);
bool ohpl_glq_fetch(OhplGlQuery*);

}

} // namespace hpl

#endif
