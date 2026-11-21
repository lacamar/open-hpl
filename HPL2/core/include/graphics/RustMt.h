#ifndef HPL_RUST_MT_H
#define HPL_RUST_MT_H

// C ABI of rust/src/material/{mt,pcm}.rs and the engine calls they make (rust/src/material/mt_host.rs).

#include <cstddef>
#include <cstdint>
#include "graphics/RustMaterial.h"

namespace hpl {

	struct OhplMtKV
	{
		const unsigned char *name; size_t nameLen;
		const unsigned char *value; size_t valueLen;
	};

	struct OhplMtHostCtx
	{
		void *renderer;
		void *object;
		void **fields;
		int *fieldNum;
	};

	enum { eOhplMtCode_TerrainDecal = 8, eOhplMtCode_Blank = 9 };

	struct OhplMtHost
	{
		void *(*progCreate)(void*, const unsigned char*, size_t);
		bool (*progLink)(void*);
		void (*progDelete)(void*);
		void (*progSetShaders)(void*, void*, void*);
		void (*progSetOwned)(void*, void*);
		void *(*progShader)(void*, int);
		void (*progSetUserId)(void*, uint32_t);
		uint32_t (*progUserId)(void*);
		void (*progName)(void*, const unsigned char**, size_t*);
		void (*progVarId)(void*, const unsigned char*, size_t, int);
		void (*progApply)(void*, const OhplUniform*, size_t);
		void *(*shaderCreate)(void*, const unsigned char*, size_t, int, const OhplMtKV*, size_t, bool);
		void (*shaderDestroy)(void*, void*);
		void (*shaderSetUserId)(void*, uint32_t);
		uint32_t (*shaderUserId)(void*);
		void (*shaderName)(void*, const unsigned char**, size_t*);
		void (*error)(const unsigned char*, size_t);
		void (*globals)(void*, float, OhplGlobals*);
		void (*renderView)(void*, OhplRenderView*);
		void *(*objView)(void*, OhplObjView*);
		void (*hostVt)(OhplMtHostCtx*, OhplHostVt*);
		void (*varsVt)(void*, OhplVarsVt*);
		void *(*varsNew)(int);
		void *(*varsPod)(int, void*);
		void *(*varsObjectNew)();
		void *(*texCreate2D)(void*, const unsigned char*, size_t, bool);
		void (*texDestroy)(void*, void*);
		void *(*texWhiteNew)(void*);
		void (*texWhiteFree)(void*, void*);
		void (*texInfo)(void*, int*, int*, bool*);
		void *(*specialTex)(void*, int);
		bool (*fileExists)(void*, const unsigned char*, size_t);
	};

	const OhplMtHost *OhplMtHostGet();

	extern "C" {

	void *ohpl_pcm_new(const OhplMtHost*, const unsigned char *name, size_t n, void *graphics, void *resources, size_t modes);
	void ohpl_pcm_free(void*);
	void ohpl_pcm_set_name(void*, const unsigned char*, size_t);
	int ohpl_pcm_combination_num(void*, int mode);
	void *ohpl_pcm_generate_program(void*, int mode, int flags);
	void ohpl_pcm_setup_mode(void*, int mode, const unsigned char *name, size_t nameN, const unsigned char *vtx, size_t vtxN,
							 const unsigned char *frag, size_t fragN, const OhplMtKV *features, const uint32_t *stages, size_t featureN,
							 const OhplMtKV *defaults, size_t defaultN);
	void ohpl_pcm_add_variable_id(void*, const unsigned char*, size_t, int id, int mode);
	void ohpl_pcm_destroy_generated_program(void*, int mode, void *prog);
	void ohpl_pcm_destroy_generated_shader(void*, int mode, void *shader, int type);
	void *ohpl_pcm_create_shader(void*, const unsigned char*, size_t, int type, const OhplMtKV*, size_t, bool hasVars, bool addToList);
	void *ohpl_pcm_create_program(void*, const unsigned char*, size_t, bool addToList);
	void ohpl_pcm_destroy_shader(void*, void*);
	void ohpl_pcm_destroy_program(void*, void*);
	void *ohpl_pcm_program_from_names(void*, const unsigned char *name, size_t nameN, const unsigned char *vtx, size_t vtxN,
									  const unsigned char *frag, size_t fragN, const OhplMtKV*, size_t, bool hasVars, bool addToList);
	void *ohpl_pcm_program_from_shaders(void*, const unsigned char *name, size_t nameN, void *vtx, void *frag, bool addToList);
	void ohpl_pcm_destroy_all(void*);

	void *ohpl_mt_new(const OhplMtHost*, int code, void *graphics, void *resources);
	void ohpl_mt_free(void*);
	void ohpl_mt_set_name(void*, const unsigned char*, size_t);
	void ohpl_mt_name(void*, const unsigned char**, size_t*);
	bool ohpl_mt_is_translucent(void*);
	bool ohpl_mt_is_decal(void*);
	bool ohpl_mt_has_type_specifics(void*, int mode);
	int ohpl_mt_code(void*);
	int ohpl_mt_used_texture_num(void*);
	int ohpl_mt_used_texture(void*, int);
	void ohpl_mt_load_data(void*);
	void ohpl_mt_destroy_data(void*);
	void ohpl_mt_reload(void*);
	void *ohpl_mt_render_program(void*, void *matState, int mode, void *renderer);
	void ohpl_mt_setup_type(void*, int mode, void *prog, void *renderer);
	void ohpl_mt_setup_material(void*, int mode, void *prog, void *matState, void *renderer);
	void ohpl_mt_setup_object(void*, int mode, void *prog, void *object, void *renderer);
	void ohpl_mt_load_variables(void*, void *matState, void *varsObject);
	void ohpl_mt_set_light_probes(bool);
	void ohpl_mt_set_world_fog(bool underwater, bool secondary);
	uint32_t ohpl_mt_get_world_fog();
	void ohpl_mt_set_depth_of_field(bool, float, float, float, float);
	bool ohpl_mt_get_light_probes();
	bool ohpl_mt_get_depth_of_field();
	void ohpl_mt_get_dof_params(float*);

	}
}

#endif
