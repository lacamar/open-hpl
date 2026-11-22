#ifndef HPL_RUST_GX_H
#define HPL_RUST_GX_H

// C ABI of rust/src/graphics/gx_*.rs.

#include <cstddef>
#include <cstdint>

#include "math/MathTypes.h"
#include "graphics/Color.h"

namespace hpl {

	struct OhplGxDecal;
	struct OhplBv;

	struct OhplGxSubView
	{
		cMatrixf world;
		OhplBv* bv;
		const float* pos;
		size_t posLen;
		uint32_t posStride;
		const float* nrm;
		size_t nrmLen;
		uint32_t nrmStride;
		const unsigned int* idx;
		uint32_t idxNum;
	};

	struct OhplGxHost
	{
		void* (*vbCreate)(void* apLowLevel);
		bool (*vbCompile)(void* apVb, int32_t alFlags);
		void (*subView)(void* apSub, OhplGxSubView* apOut);
		void* (*makeMesh)(void* apRes, void* apVb, void* apMat);
	};

	struct OhplGxGraphics;

	struct OhplGxLlInit
	{
		int width, height, display, bpp, fullscreen, multisampling, gpuFormat;
		const char* caption;
		size_t captionLen;
		cVector2l pos;
	};

	struct OhplGxDsInfo
	{
		cVector2l size;
		int depthBits, stencilBits;
	};

	struct OhplGxGfxHost
	{
		void (*deleteObj)(int kind, void*);
		void (*fatal)(const char*, size_t);
		void (*llInit)(void* ll, const OhplGxLlInit*);
		bool (*llCheckScreen)(void* ll);
		void* (*llCreateTexture)(void* ll, const char*, size_t, int kind, int usage);
		void* (*llCreateFb)(void* ll, const char*, size_t);
		void* (*llCreateDs)(void* ll, const cVector2l*, int depth, int stencil);
		void* (*llCreateProg)(void* ll, const char*, size_t);
		void* (*creatorNew)(int kind, void* ll, void* res);
		void* (*compositeNew)(void* gfx);
		void (*resAddDir)(void* res, const char*, size_t, bool);
		void (*resAddCompatDir)(void* res);
		void (*resCheckFeatures)(void* res);
		bool (*resHasFile)(void* res, const char*, size_t);
		void (*resCompileTextureLookups)(void* res);
		void* (*rendererNew)(int kind, void* gfx, void* res);
		bool (*rendererLoad)(void*);
		void (*rendererDestroyData)(void*);
		void (*rendererUpdate)(void*, float);
		void (*rendererSetupFuncs)(void*, void* ll);
		void* (*matTypeNew)(int code, void* gfx, void* res);
		void (*matTypeSetName)(void*, const char*, size_t);
		void (*matTypeLoad)(void*);
		void (*matTypeDestroyData)(void*);
		void (*matTypeDestroy)(void*);
		void (*matTypeReload)(void*);
		void (*matDestroyTypeSpecifics)(bool);
		void* (*peTypeNew)(int kind, void* gfx, void* res);
		const char* (*peTypeName)(void*, size_t*);
		void* (*peTypeCreate)(void*, void* params);
		void (*peSetParams)(void*, void* params);
		const char* (*paramsName)(void*, size_t*);
		void (*texSetup)(void* tex, const cVector2l*, int format);
		bool (*fbAttach)(void* fb, void* tex);
		void (*dsInfo)(void*, OhplGxDsInfo*);
		void (*dsInc)(void*);
		bool (*dsDecHasUsers)(void*);
		void (*progSetRes)(void*, void* res);
		void* (*shaderCreate)(void* res, const char*, size_t, int kind, void* vars);
		void (*shaderDestroy)(void* res, void* shader);
		void (*progSetShader)(void*, int kind, void* shader);
		void (*progLink)(void*);
		void (*texFill)(void* tex, const cVector3l*, unsigned char*);
	};

	static_assert(sizeof(OhplGxLlInit) == 56 && sizeof(OhplGxDsInfo) == 16 && sizeof(OhplGxGfxHost) == 344, "GX gfx ABI layout");

	extern "C" {
		void ohpl_gx_set_host(const OhplGxHost*);
		void ohpl_gx_set_gfx_host(const OhplGxGfxHost*);

		OhplGxGraphics* ohpl_gx_gfx_new(void* apSelf, void* apLowLevel);
		void ohpl_gx_gfx_free(OhplGxGraphics*);
		int ohpl_gx_gfx_temp_texture_type();
		void ohpl_gx_gfx_set_temp_texture_type(int);
		bool ohpl_gx_gfx_init(OhplGxGraphics*, int, int, int, int, int, int, int, const char*, size_t, const cVector2l*, void*, unsigned);
		void ohpl_gx_gfx_update(OhplGxGraphics*, float);
		void* ohpl_gx_gfx_renderer(OhplGxGraphics*, int);
		void ohpl_gx_gfx_reload_renderer_data(OhplGxGraphics*);
		void* ohpl_gx_gfx_create_frame_buffer(OhplGxGraphics*, const char*, size_t);
		void ohpl_gx_gfx_destroy_frame_buffer(OhplGxGraphics*, void*);
		void* ohpl_gx_gfx_temp_frame_buffer(OhplGxGraphics*, const cVector2l*, int, int);
		void* ohpl_gx_gfx_create_depth_stencil(OhplGxGraphics*, const cVector2l*, int, int, bool);
		void* ohpl_gx_gfx_find_depth_stencil(OhplGxGraphics*, const cVector2l*, int, int);
		void ohpl_gx_gfx_destroy_depth_stencil(OhplGxGraphics*, void*);
		void* ohpl_gx_gfx_create_texture(OhplGxGraphics*, const char*, size_t, int, int);
		void ohpl_gx_gfx_destroy_texture(OhplGxGraphics*, void*);
		void* ohpl_gx_gfx_create_composite(OhplGxGraphics*);
		void ohpl_gx_gfx_destroy_composite(OhplGxGraphics*, void*);
		void ohpl_gx_gfx_add_post_effect_type(OhplGxGraphics*, void*);
		void* ohpl_gx_gfx_create_post_effect(OhplGxGraphics*, void*);
		void ohpl_gx_gfx_destroy_post_effect(OhplGxGraphics*, void*);
		void* ohpl_gx_gfx_create_gpu_program(OhplGxGraphics*, const char*, size_t);
		void* ohpl_gx_gfx_create_gpu_program_from_shaders(OhplGxGraphics*, const char*, size_t, const char*, size_t, const char*, size_t, void*);
		void ohpl_gx_gfx_destroy_gpu_program(OhplGxGraphics*, void*);
		void ohpl_gx_gfx_add_material_type(OhplGxGraphics*, void*, const char*, size_t);
		void* ohpl_gx_gfx_material_type(OhplGxGraphics*, const char*, size_t);
		const char* ohpl_gx_gfx_material_type_name(OhplGxGraphics*, size_t, size_t*);
		void ohpl_gx_gfx_reload_materials(OhplGxGraphics*);
		void* ohpl_gx_gfx_mesh_creator(OhplGxGraphics*);
		void* ohpl_gx_gfx_texture_creator(OhplGxGraphics*);
		void* ohpl_gx_gfx_decal_creator(OhplGxGraphics*);
		bool ohpl_gx_gfx_screen_is_setup(OhplGxGraphics*);
		void ohpl_gx_texgen_scatter(void* apTexture, int alSize, int alSamples, bool abSort, bool ab3D);

		OhplGxDecal* ohpl_gx_decal_new(void* apLowLevel, void* apResources);
		void ohpl_gx_decal_free(OhplGxDecal*);
		bool ohpl_gx_decal_add_submesh(OhplGxDecal*, void*);
		void ohpl_gx_decal_add_affected(OhplGxDecal*, void*);
		void ohpl_gx_decal_clear_meshes(OhplGxDecal*);
		void ohpl_gx_decal_set_max_tris(OhplGxDecal*, int);
		void ohpl_gx_decal_set_position(OhplGxDecal*, const cVector3f*);
		void ohpl_gx_decal_set_up(OhplGxDecal*, const cVector3f*, bool);
		void ohpl_gx_decal_set_right(OhplGxDecal*, const cVector3f*, bool);
		void ohpl_gx_decal_set_forward(OhplGxDecal*, const cVector3f*, bool);
		void ohpl_gx_decal_set_offset(OhplGxDecal*, float);
		void ohpl_gx_decal_set_size(OhplGxDecal*, const cVector3f*);
		void ohpl_gx_decal_set_material(OhplGxDecal*, const char*, size_t);
		void ohpl_gx_decal_set_color(OhplGxDecal*, const cColor*);
		void ohpl_gx_decal_set_sub_div(OhplGxDecal*, const cVector2l*);
		void ohpl_gx_decal_set_current_sub_div(OhplGxDecal*, int);
		bool ohpl_gx_decal_is_updated(OhplGxDecal*);
		bool ohpl_gx_decal_compile(OhplGxDecal*);
		bool ohpl_gx_decal_can_create(OhplGxDecal*);
		void* ohpl_gx_decal_create_mesh(OhplGxDecal*);
		void* ohpl_gx_decal_material(OhplGxDecal*);
	}

	static_assert(sizeof(OhplGxSubView) == 136 && sizeof(OhplGxHost) == 32, "GX ABI layout");
}

#endif
