#ifndef HPL_RUST_RESOURCES_H4_H
#define HPL_RUST_RESOURCES_H4_H

// C ABI of rust/src/resources/h4_ffi.rs, h4_handler.rs and vars.rs.

#include <cstddef>
#include <cstdint>
#include "resources/RustResourcesH3.h"

struct OhplH4Res;
struct OhplH2Vars;
struct OhplLoaderTable;

enum OhplH4Handler
{
	OHPL_H4_MESH_LOAD, OHPL_H4_MESH_SAVE, OHPL_H4_ANIMATION_LOAD, OHPL_H4_BITMAP_LOAD, OHPL_H4_BITMAP_SAVE,
	OHPL_H4_VIDEO_LOAD, OHPL_H4_WORLD_LOAD, OHPL_H4_MESH_SET_FULL_PATH, OHPL_H4_BITMAP_SET_FILE_NAME, OHPL_H4_MESH_CACHE_USABLE
};

enum OhplH4What
{
	OHPL_H4W_MESH_HANDLER, OHPL_H4W_BITMAP_HANDLER, OHPL_H4W_WORLD_HANDLER, OHPL_H4W_VIDEO_HANDLER,
	OHPL_H4W_IMAGE, OHPL_H4W_GPU_SHADER, OHPL_H4W_PARTICLE, OHPL_H4W_SOUND, OHPL_H4W_FONT, OHPL_H4W_SCRIPT,
	OHPL_H4W_TEXTURE, OHPL_H4W_MATERIAL, OHPL_H4W_MESH, OHPL_H4W_SOUND_ENTITY, OHPL_H4W_ANIMATION, OHPL_H4W_VIDEO,
	OHPL_H4W_ENT_FILE, OHPL_H4W_FILE_SEARCHER, OHPL_H4W_LANGUAGE, OHPL_H4W_XML_DOC, OHPL_H4W_BIN_BUFFER,
	OHPL_H4W_ENTITY_LOADER, OHPL_H4W_AREA_LOADER, OHPL_H4W_LOADERS_ADD, OHPL_H4W_LOW_LEVEL_SYSTEM,
	OHPL_H4W_UPDATE_MANAGER, OHPL_H4W_HAS_LANGUAGE
};

struct OhplH4HandlerOps
{
	void* ctx;
	void (*log)(void*, int32_t level, const uint8_t*, size_t);
	void* (*call)(void*, int32_t what, void* loader, OhplH3WStr file, int64_t a, void* b);
};

struct OhplH4ResOps
{
	void* ctx;
	void (*log)(void*, int32_t level, const uint8_t*, size_t);
	void* (*create)(void*, int32_t what, OhplH3WStr arg);
	void (*destroy)(void*, int32_t what, void* obj);
	int64_t (*call)(void*, int32_t what, void* obj, int64_t arg);
	const uint32_t* (*file_path)(void*, const uint8_t*, size_t, int32_t*, size_t*);
	void (*add_dir)(void*, OhplH3WStr dir, OhplH3Str mask, bool subDirs);
	void (*clear_dirs)(void*);
	bool (*read_dirs)(void*, OhplH3Str file, void (*each)(void*, OhplH3Str path, bool subDirs), void* ectx);
};

extern "C" {
	void* ohpl_h4_handler_find(const OhplLoaderTable*, const OhplH4HandlerOps*, const uint8_t* name, size_t n);
	void* ohpl_h4_handler_run(const OhplLoaderTable*, const OhplH4HandlerOps*, int32_t what, const uint32_t* file, size_t n, int64_t a, void* b);

	OhplH4Res* ohpl_h4_res_new(const OhplH4ResOps*);
	void ohpl_h4_res_free(OhplH4Res*);
	void ohpl_h4_res_init(OhplH4Res*);
	void ohpl_h4_res_update(const OhplH4Res*, int64_t dtBits);
	void ohpl_h4_res_shutdown(OhplH4Res*);
	bool ohpl_h4_res_add_dir(const OhplH4Res*, const uint32_t* dir, size_t dn, bool sub, const uint8_t* mask, size_t mn);
	void ohpl_h4_res_clear_dirs(const OhplH4Res*);
	void ohpl_h4_res_add_loader(OhplH4Res*, bool area, const uint8_t* name, size_t n, void* loader, bool setDefault);
	void* ohpl_h4_res_get_loader(const OhplH4Res*, bool area, const uint8_t* name, size_t n);
	bool ohpl_h4_res_dirs_file(const OhplH4Res*, const uint8_t* file, size_t n, const uint32_t* alt, size_t an);
	void* ohpl_h4_res_load_xml(OhplH4Res*, const uint8_t* name, size_t n);
	void* ohpl_h4_res_load_bin(OhplH4Res*, const uint8_t* name, size_t n);
	void ohpl_h4_res_destroy_xml(OhplH4Res*, void* doc);
	void ohpl_h4_res_destroy_bin(OhplH4Res*, void* buf);

	OhplH2Vars* ohpl_h2_vars_new();
	void ohpl_h2_vars_free(OhplH2Vars*);
	OhplH2Vars* ohpl_h2_vars_clone(const OhplH2Vars*);
	void ohpl_h2_vars_clear(OhplH2Vars*);
	void ohpl_h2_vars_set(OhplH2Vars*, const uint8_t* n, size_t nn, const uint8_t* v, size_t vn);
	void ohpl_h2_vars_insert(OhplH2Vars*, const uint8_t* n, size_t nn, const uint8_t* v, size_t vn);
	bool ohpl_h2_vars_get(const OhplH2Vars*, const uint8_t* n, size_t nn, void (*sink)(void*, const uint8_t*, size_t), void* ctx);
	void ohpl_h2_vars_add_bool(OhplH2Vars*, const uint8_t* n, size_t nn, bool x);
	void ohpl_h2_vars_add_int(OhplH2Vars*, const uint8_t* n, size_t nn, int32_t x);
	void ohpl_h2_vars_add_float(OhplH2Vars*, const uint8_t* n, size_t nn, float x);
	void ohpl_h2_vars_add_floats(OhplH2Vars*, const uint8_t* n, size_t nn, const float* x, size_t count);
	bool ohpl_h2_vars_get_bool(const OhplH2Vars*, const uint8_t* n, size_t nn, bool d);
	int32_t ohpl_h2_vars_get_int(const OhplH2Vars*, const uint8_t* n, size_t nn, int32_t d);
	float ohpl_h2_vars_get_float(const OhplH2Vars*, const uint8_t* n, size_t nn, float d);
	bool ohpl_h2_vars_get_floats(const OhplH2Vars*, const uint8_t* n, size_t nn, float* out, size_t count);
}

#endif
