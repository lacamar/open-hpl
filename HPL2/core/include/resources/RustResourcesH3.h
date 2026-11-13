#ifndef HPL_RUST_RESOURCES_H3_H
#define HPL_RUST_RESOURCES_H3_H

// C ABI of rust/src/resources/manager_ffi.rs and h3_ffi.rs.

#include <cstddef>
#include <cstdint>
#include "resources/RustResources.h"

struct OhplH3Mgr;

struct OhplH3Str { const uint8_t* p; size_t n; };
struct OhplH3WStr { const uint32_t* p; size_t n; };

struct OhplH3Info
{
	OhplH3Str name;
	OhplH3WStr path;
	OhplResMeta* meta;
};

enum OhplH3Kind
{
	OHPL_H3_ANIMATION, OHPL_H3_ENT_FILE, OHPL_H3_MESH, OHPL_H3_SCRIPT, OHPL_H3_SOUND_DATA, OHPL_H3_SOUND_ENTITY,
	OHPL_H3_VIDEO, OHPL_H3_FONT_TTF, OHPL_H3_FONT_BITMAP, OHPL_H3_FONT_UNKNOWN, OHPL_H3_PS_DATA_FILE,
	OHPL_H3_PS_DATA_ELEMENT, OHPL_H3_PS_CREATE, OHPL_H3_TEX_SIMPLE, OHPL_H3_TEX_ANIM, OHPL_H3_TEX_CUBE,
	OHPL_H3_IMAGE, OHPL_H3_MATERIAL, OHPL_H3_CUSTOM_RESOLVER, OHPL_H3_IMG_BITMAP_FRAME, OHPL_H3_IMG_CUSTOM_FRAME,
	OHPL_H3_IMG_ADD_BITMAP, OHPL_H3_SHADER_FILE, OHPL_H3_SHADER_STRING, OHPL_H3_SHADER_PARSE, OHPL_H3_SHADER_TRANSPILE,
	OHPL_H3_MAT_NEW, OHPL_H3_MAT_TYPE, OHPL_H3_MAT_SET, OHPL_H3_MAT_TEXTURE, OHPL_H3_MAT_TEX_SETUP, OHPL_H3_MAT_VAR,
	OHPL_H3_MAT_VARS_APPLY, OHPL_H3_MAT_ADD_UV
};

enum OhplH3Call
{
	OHPL_H3C_DESTROY_VIRTUAL = 1, OHPL_H3C_IS_STREAM, OHPL_H3C_SOUND_SET_MGR, OHPL_H3C_MEM_SIZE, OHPL_H3C_UPDATE,
	OHPL_H3C_PRELOAD_SOUNDS, OHPL_H3C_IMG_FRAME_TEX, OHPL_H3C_IMG_FRAME_BMP, OHPL_H3C_FRAME_DEC_PIC,
	OHPL_H3C_BMPFRAME_DEC_PIC, OHPL_H3C_FRAME_IS_EMPTY, OHPL_H3C_FRAME_IS_CUSTOM, OHPL_H3C_BMPFRAME_HANDLE,
	OHPL_H3C_BMPFRAME_TEX, OHPL_H3C_BMPFRAME_IS_FULL, OHPL_H3C_BMPFRAME_IS_LOCKED, OHPL_H3C_BMPFRAME_ADDITIONS,
	OHPL_H3C_BMPFRAME_REORGANIZE, OHPL_H3C_BMPFRAME_FLUSH, OHPL_H3C_BMPFRAME_SET_LOCKED, OHPL_H3C_BMPFRAME_DELETE,
	OHPL_H3C_FRAME_DELETE, OHPL_H3C_FRAME_HANDLE, OHPL_H3C_BMP_WIDTH, OHPL_H3C_BMP_HEIGHT,
	OHPL_H3C_IMAGE_SET_PRELOADED, OHPL_H3C_PHYSICS_NAME_OF, OHPL_H3C_BMP_DELETE = 29, OHPL_H3C_SHADER_POST,
	OHPL_H3C_SHADER_ERROR, OHPL_H3C_MAT_COMPILE_LOOKUP, OHPL_H3C_MAT_SET_FILTER, OHPL_H3C_MAT_SET_ANISO,
	OHPL_H3C_FILE_BYTES, OHPL_H3C_MAT_TYPE_Q, OHPL_H3C_MAT_COMPILE
};

enum OhplH3Shutdown
{
	OHPL_H3S_ANIMATION, OHPL_H3S_ENT_FILE, OHPL_H3S_MESH, OHPL_H3S_SCRIPT, OHPL_H3S_SOUND_ENTITY, OHPL_H3S_SOUND,
	OHPL_H3S_FONT, OHPL_H3S_TEXTURE, OHPL_H3S_IMAGE, OHPL_H3S_MATERIAL, OHPL_H3S_GPU_SHADER, OHPL_H3S_PARTICLE
};

struct OhplH3Create
{
	int32_t kind;
	int32_t status;
	int32_t idx;
	OhplH3Str name;
	OhplH3Str name2;
	OhplH3WStr path;
	const OhplH3WStr* paths;
	size_t npaths;
	int32_t i[4];
	float f[4];
	void* ptr;
	void* ptr2;
};

struct OhplH3Ops
{
	void* ctx;
	void (*log)(void*, int32_t level, const uint8_t*, size_t);
	uint64_t (*now)(void*);
	const uint32_t* (*file_path)(void*, const uint8_t*, size_t, int32_t*, size_t*);
	void (*info)(void*, void* obj, OhplH3Info*);
	void (*destroy)(void*, void* obj);
	void (*load_done)(void*, const uint8_t*, size_t, uint64_t);
	void* (*create)(void*, OhplH3Create*);
	int64_t (*call)(void*, int32_t what, void* obj, int64_t i);
	void (*text)(void*, int32_t what, void* obj, OhplH3WStr path, void (*sink)(void*, const uint8_t*, size_t), void* sctx);
};

extern "C" {
	OhplH3Mgr* ohpl_h3_mgr_new();
	void ohpl_h3_mgr_free(OhplH3Mgr*);
	OhplResMap* ohpl_h3_mgr_map(OhplH3Mgr*);
	void ohpl_h3_mgr_add(OhplH3Mgr*, void* res, const uint32_t* path, size_t n, const OhplResMeta* meta);
	bool ohpl_h3_mgr_remove(OhplH3Mgr*, void* res);
	void* ohpl_h3_mgr_get(const OhplH3Mgr*, const uint32_t* path, size_t n);
	size_t ohpl_h3_mgr_len(const OhplH3Mgr*);
	void* ohpl_h3_mgr_first(OhplH3Mgr*);
	void ohpl_h3_mgr_eviction_tail(const OhplH3Mgr*, int64_t keep, void (*sink)(void*, void* const*, size_t), void* ctx);
	void ohpl_h3_begin_load(const uint8_t* name, size_t n, uint64_t now);
	bool ohpl_h3_end_load(uint64_t now, void (*sink)(void*, const uint8_t*, size_t), void* ctx, uint64_t* elapsed);
	int32_t ohpl_h3_tab_count();

	bool ohpl_h3_log_create_get();
	void ohpl_h3_log_create_set(bool);

	void ohpl_h3f_begin(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t);
	void ohpl_h3f_end(OhplH3Mgr*, const OhplH3Ops*);
	void ohpl_h3f_add(OhplH3Mgr*, const OhplH3Ops*, void* res, bool log, bool toSet);
	void ohpl_h3f_inc_user(OhplH3Mgr*, const OhplH3Ops*, void* res);
	void ohpl_h3f_tabs(OhplH3Mgr*, const OhplH3Ops*, void (*sink)(void*, const uint8_t*, size_t), void* ctx);
	void* ohpl_h3f_find_loaded(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, int32_t* count, void (*sink)(void*, const uint32_t*, size_t), void* ctx);
	void ohpl_h3f_destroy_all(OhplH3Mgr*, const OhplH3Ops*);
	void ohpl_h3f_destroy_unused(OhplH3Mgr*, const OhplH3Ops*, int64_t keep);
	void ohpl_h3f_shutdown(OhplH3Mgr*, const OhplH3Ops*, int32_t which);
	void ohpl_h3f_for_each(OhplH3Mgr*, const OhplH3Ops*, int32_t what, int64_t arg);
	void ohpl_h3f_destroy_standard(OhplH3Mgr*, const OhplH3Ops*, void* res);
	void* ohpl_h3f_model(OhplH3Mgr*, const OhplH3Ops*, bool mesh, const uint8_t*, size_t, const OhplH3Str* exts, size_t ne, int32_t flag);
	void* ohpl_h3f_ent_file(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t);
	void* ohpl_h3f_script(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, void* msgs);
	void* ohpl_h3f_font(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, int32_t size, int32_t first, int32_t last);
	void* ohpl_h3f_material(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t);
	void ohpl_h3f_material_blank(OhplH3Mgr*, const OhplH3Ops*, bool on);
	void ohpl_h3f_material_custom(OhplH3Mgr*, const OhplH3Ops*, void* res);
	void ohpl_h3f_material_physics_name(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, void (*sink)(void*, const uint8_t*, size_t), void* ctx);
	void* ohpl_h3f_sound_entity(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, bool hasResolver);
	void ohpl_h3f_sound_entity_add_custom(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, void* data);
	void ohpl_h3f_sound_entity_preload(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, bool hasResolver);
	void* ohpl_h3f_video(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t);
	void ohpl_h3f_video_destroy(OhplH3Mgr*, const OhplH3Ops*, void* res);
	void ohpl_h3f_video_update(OhplH3Mgr*, const OhplH3Ops*, int64_t dtBits);
	void* ohpl_h3f_sound(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, bool stream, bool loop, const OhplH3Str* formats, size_t nf);
	void ohpl_h3f_sound_destroy(OhplH3Mgr*, const OhplH3Ops*, void* res);
	void ohpl_h3f_sound_destroy_all(OhplH3Mgr*, const OhplH3Ops*);
	void* ohpl_h3f_particle_by_type(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, const uint8_t*, size_t, float, float, float);
	void* ohpl_h3f_particle_by_element(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, const uint8_t*, size_t, void* element, float, float, float);
	void ohpl_h3f_particle_add_data(OhplH3Mgr*, const OhplH3Ops*, void* res);
	void ohpl_h3f_particle_preload(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t);
	void ohpl_h3f_particle_destroy(OhplH3Mgr*, const OhplH3Ops*, void* res);
	void* ohpl_h3f_gpu_shader(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, int32_t type, void* vars, bool hasTranspile, int32_t fragment);
	void ohpl_h3f_gpu_destroy(OhplH3Mgr*, const OhplH3Ops*, void* res);
	void* ohpl_h3f_texture_simple(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, bool mip, int32_t usage, int32_t type, int32_t level, const OhplH3Str* exts, size_t ne);
	void* ohpl_h3f_texture_anim(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, bool mip, int32_t type, int32_t usage, int32_t level, const OhplH3Str* exts, size_t ne);
	void* ohpl_h3f_texture_cube(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, bool mip, int32_t usage, int32_t level, int32_t cubeType, const OhplH3Str* exts, size_t ne);
	void ohpl_h3f_texture_destroy(OhplH3Mgr*, const OhplH3Ops*, void* res);
	void ohpl_h3f_texture_update(OhplH3Mgr*, const OhplH3Ops*, int64_t dtBits);
	int32_t ohpl_h3f_mem_usage(OhplH3Mgr*, const OhplH3Ops*);
	void ohpl_h3f_image_init(OhplH3Mgr*, const OhplH3Ops*, int32_t w, int32_t h);
	void* ohpl_h3f_image_create_in_frame(OhplH3Mgr*, const OhplH3Ops*, const uint8_t*, size_t, int32_t handle, const OhplH3Str* exts, size_t ne);
	void* ohpl_h3f_image_from_bitmap(OhplH3Mgr*, const OhplH3Ops*, void* bmp, int32_t handle);
	void* ohpl_h3f_image_custom_frame(OhplH3Mgr*, const OhplH3Ops*, void* tex);
	void* ohpl_h3f_image_frame_texture(OhplH3Mgr*, const OhplH3Ops*, int32_t handle);
	int32_t ohpl_h3f_image_create_frame(OhplH3Mgr*, const OhplH3Ops*, int32_t w, int32_t h);
	void ohpl_h3f_image_set_frame_locked(OhplH3Mgr*, const OhplH3Ops*, int32_t handle, bool locked);
	int32_t ohpl_h3f_image_flush_all(OhplH3Mgr*, const OhplH3Ops*);
	void ohpl_h3f_image_reorganize_all(OhplH3Mgr*, const OhplH3Ops*);
	void ohpl_h3f_image_destroy(OhplH3Mgr*, const OhplH3Ops*, void* res);
}

#ifndef RM_STR
#define RM_STR(s) reinterpret_cast<const uint8_t*>((s).data()), (s).size()
#endif

#endif
