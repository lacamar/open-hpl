#ifndef HPL_RUST_SCN_H
#define HPL_RUST_SCN_H

// C ABI of rust/src/scene/scn.rs (cScene and cViewport are Rust-owned).

#include <cstddef>
#include <cstdint>
#include "math/MathTypes.h"

namespace hpl {

class cScene;
class cViewport;
struct OhplScn;

struct OhplScnWorldArgs
{
	void *graphics, *resources, *sound, *physics, *scene, *system, *ai;
};

struct OhplScnHost
{
	void* (*settings_new)();
	void (*settings_delete)(void*);
	void (*settings_reset)(void*);
	void* (*camera_new)(void*);
	void (*camera_delete)(void*);
	void* (*world_new)(const OhplScnWorldArgs*, const char*, size_t);
	void (*world_delete)(void*);
	void* (*world_load)(void*, const char*, size_t, uint32_t);
	void* (*main_renderer)(void*);
	void (*frame_count_inc)();
	void (*renderer_render)(void*, float, void*, void*, void*, void*, bool, void*);
	void* (*post_texture)(void*);
	bool (*composite_active)(void*);
	void (*composite_render)(void*, float, void*, void*, void*);
	void (*set_frame_buffer)(void*, void*, const int*, const int*);
	bool (*gui_is_3d)(void*);
	int (*gui_priority)(void*);
	void (*gui_render)(void*, void*);
	void (*viewport_callback)(void*, int);
	void (*set_listener)(void*, const cVector3f*, const cVector3f*, const cVector3f*);
	void (*log)(bool, const char*, size_t);
};

void OhplScnRegisterHost();

extern "C" {
void ohpl_scn_set_host(const OhplScnHost*);
OhplScn* ohpl_scn_new(cScene*, void*, void*, void*, void*, void*, void*, void*);
void ohpl_scn_free(OhplScn*);
void ohpl_scn_render(OhplScn*, float, uint32_t);
void ohpl_scn_post_update(OhplScn*, float);
cViewport* ohpl_scn_create_viewport(OhplScn*, void*, void*, bool);
void ohpl_scn_destroy_viewport(OhplScn*, cViewport*);
bool ohpl_scn_viewport_exists(const OhplScn*, cViewport*);
const void* ohpl_scn_viewports(const OhplScn*);
void ohpl_scn_set_current_listener(OhplScn*, cViewport*);
void* ohpl_scn_create_camera(OhplScn*);
void ohpl_scn_destroy_camera(OhplScn*, void*);
void* ohpl_scn_load_world(OhplScn*, const char*, size_t, uint32_t);
void* ohpl_scn_create_world(OhplScn*, const char*, size_t);
void ohpl_scn_destroy_world(OhplScn*, void*);
bool ohpl_scn_world_exists(const OhplScn*, void*);

void ohpl_vp_add_gui(cViewport*, void*);
void ohpl_vp_remove_gui(cViewport*, void*);
void ohpl_vp_add_callback(cViewport*, void*);
void ohpl_vp_remove_callback(cViewport*, void*);
void ohpl_vp_add_renderer_callback(cViewport*, void*);
void ohpl_vp_remove_renderer_callback(cViewport*, void*);
void ohpl_vp_set_world(cViewport*, void*);
void ohpl_vp_run_callbacks(cViewport*, int);
}

}

#endif
