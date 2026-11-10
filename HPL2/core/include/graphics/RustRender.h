#ifndef HPL_RUST_RENDER_H
#define HPL_RUST_RENDER_H

// C ABI of rust/src/render/ffi.rs.

#include <cstddef>
#include <cstdint>

namespace hpl {

	struct OhplVec2l { int x, y; };
	struct OhplVec2f { float x, y; };
	struct OhplVec3f { float x, y, z; };
	struct OhplMat4 { float m[4][4]; };

	enum { kOhplTextureUnits = 16 };

	struct OhplSolidKey
	{
		int alphaMode;
		float viewZ;
		uintptr_t progZ, texDiffuse, progDiffuse, vtx, mtx, obj;
		uintptr_t texUnits[kOhplTextureUnits];
	};

	struct OhplTransKey
	{
		float viewZ;
		int placement;
		unsigned planeCandidate;
		unsigned pad;
		OhplVec3f center, normal, point;
		OhplMat4 world;
	};

	struct OhplDecalKey
	{
		int sortOrder;
		unsigned pad;
		uintptr_t texIllum, vtx, mtx;
		OhplVec3f pos;
		float pad2;
	};

	struct OhplIllumKey
	{
		float illumAmount;
		unsigned pad;
		uintptr_t texIllum, vtx, mtx;
	};

	struct OhplCompileIn
	{
		unsigned flags;
		OhplVec3f origin;
		const OhplSolidKey* solid; size_t solidLen;
		OhplTransKey* trans; size_t transLen;
		const OhplDecalKey* decal; size_t decalLen;
		const OhplIllumKey* illum; size_t illumLen;
	};

	struct OhplCompileOut
	{
		unsigned *z, *diffuse, *trans, *decal, *illum;
	};

	// Mirrors rust/src/render/funcs.rs State (offsets asserted on both sides).
	struct OhplRenderState;

	enum eOhplRfLog
	{
		eOhplRfLog_Projection, eOhplRfLog_DepthTest, eOhplRfLog_DepthWrite, eOhplRfLog_DepthFunc, eOhplRfLog_CullActive,
		eOhplRfLog_CullMode, eOhplRfLog_StencilActive, eOhplRfLog_ScissorActive, eOhplRfLog_ScissorRect,
		eOhplRfLog_ChannelMode, eOhplRfLog_AlphaMode, eOhplRfLog_AlphaLimit, eOhplRfLog_BlendMode, eOhplRfLog_Program,
		eOhplRfLog_ProgramNull, eOhplRfLog_Texture, eOhplRfLog_VertexBuffer, eOhplRfLog_VertexBufferNull,
		eOhplRfLog_ModelMatrix, eOhplRfLog_ModelMatrixNull, eOhplRfLog_InvertCull, eOhplRfLog_FrameBufferPos,
		eOhplRfLog_FrameBufferDefault, eOhplRfLog_ClearPortionGlobal, eOhplRfLog_ClearPortionTarget, eOhplRfLog_ClearAll
	};

	struct OhplRfBackend
	{
		void* ctx;
		void (*depthTest)(void*, bool);
		void (*depthWrite)(void*, bool);
		void (*depthFunc)(void*, unsigned);
		void (*cullActive)(void*, bool);
		void (*cullMode)(void*, unsigned);
		void (*stencilActive)(void*, bool);
		void (*scissorActive)(void*, bool);
		void (*scissorRect)(void*, OhplVec2l, OhplVec2l);
		void (*colorWrite)(void*, bool, bool, bool, bool);
		void (*alphaTestActive)(void*, bool);
		void (*alphaTestGreaterOrEqual)(void*, float);
		void (*blendActive)(void*, bool);
		void (*blendFunc)(void*, unsigned, unsigned);
		void (*programBind)(void*, uintptr_t);
		void (*programUnbind)(void*, uintptr_t);
		void (*texture)(void*, int, uintptr_t);
		void (*vertexBufferUnbind)(void*, uintptr_t);
		void (*vertexBufferBind)(void*, uintptr_t);
		void (*setProjection)(void*, uintptr_t);
		void (*setModelMatrix)(void*, uintptr_t);
		void (*setFrameBuffer)(void*, uintptr_t, bool, OhplVec2l, OhplVec2l);
		void (*frameBufferSize)(void*, uintptr_t, OhplVec2l*);
		void (*clearFrameBuffer)(void*, unsigned);
		void (*log)(void*, int, int64_t, int64_t, int64_t, int64_t, float);
	};

	struct OhplComboHandle;

	extern "C"
	{
		void ohpl_renderlist_compile(const OhplCompileIn* in, const OhplCompileOut* out);

		void ohpl_rf_init(OhplRenderState*, uintptr_t nullMatrix, bool log, bool useGlobalScissor, OhplVec2l globalPos,
						  OhplVec2l globalSize, OhplVec2l screen, OhplVec2f screenFloat, OhplVec2l frameBufferSize,
						  OhplVec2l targetSize);
		void ohpl_rf_exit(OhplRenderState*, const OhplRfBackend*);
		void ohpl_rf_flat_projection_set(OhplRenderState*, const OhplRfBackend*, uintptr_t nullMatrix);
		void ohpl_rf_model_view_set(OhplRenderState*, uintptr_t nullMatrix);
		void ohpl_rf_set_projection_matrix(OhplRenderState*, const OhplRfBackend*, uintptr_t nullMatrix, uintptr_t matrix);
		bool ohpl_rf_set_depth_test(OhplRenderState*, const OhplRfBackend*, bool);
		bool ohpl_rf_set_depth_write(OhplRenderState*, const OhplRfBackend*, bool);
		bool ohpl_rf_set_depth_test_func(OhplRenderState*, const OhplRfBackend*, unsigned);
		bool ohpl_rf_set_cull_active(OhplRenderState*, const OhplRfBackend*, bool);
		bool ohpl_rf_set_cull_mode(OhplRenderState*, const OhplRfBackend*, unsigned, bool checkInverted);
		bool ohpl_rf_set_stencil_active(OhplRenderState*, const OhplRfBackend*, bool);
		bool ohpl_rf_set_scissor_active(OhplRenderState*, const OhplRfBackend*, bool, OhplVec2l targetPos);
		bool ohpl_rf_set_scissor_rect(OhplRenderState*, const OhplRfBackend*, OhplVec2l pos, OhplVec2l size, bool autoEnable,
									  OhplVec2l targetPos);
		bool ohpl_rf_set_channel_mode(OhplRenderState*, const OhplRfBackend*, unsigned);
		bool ohpl_rf_set_alpha_mode(OhplRenderState*, const OhplRfBackend*, unsigned);
		bool ohpl_rf_set_alpha_limit(OhplRenderState*, const OhplRfBackend*, float);
		bool ohpl_rf_set_blend_mode(OhplRenderState*, const OhplRfBackend*, unsigned);
		bool ohpl_rf_set_program(OhplRenderState*, const OhplRfBackend*, uintptr_t);
		void ohpl_rf_set_texture(OhplRenderState*, const OhplRfBackend*, int unit, uintptr_t);
		void ohpl_rf_set_texture_range(OhplRenderState*, const OhplRfBackend*, uintptr_t, int first, int last);
		void ohpl_rf_set_vertex_buffer(OhplRenderState*, const OhplRfBackend*, uintptr_t);
		void ohpl_rf_set_matrix(OhplRenderState*, const OhplRfBackend*, uintptr_t);
		void ohpl_rf_set_invert_cull_mode(OhplRenderState*, const OhplRfBackend*, bool);
		void ohpl_rf_set_frame_buffer(OhplRenderState*, const OhplRfBackend*, uintptr_t fb, bool usePosAndSize,
									  bool useGlobalScissor, OhplVec2l targetPos);
		void ohpl_rf_clear_frame_buffer(OhplRenderState*, const OhplRfBackend*, unsigned flags, bool usePosAndSize,
										OhplVec2l targetPos);

		OhplComboHandle* ohpl_combo_new(size_t modes);
		void ohpl_combo_free(OhplComboHandle*);
		int ohpl_combo_setup(OhplComboHandle*, int mode, const unsigned* stages, size_t n);
		int ohpl_combo_combinations(OhplComboHandle*, int mode);
		unsigned ohpl_combo_valid_bits(OhplComboHandle*, int mode, unsigned flags, unsigned stage);
		bool ohpl_combo_program_acquire(OhplComboHandle*, int mode, unsigned flags, uintptr_t* out);
		void ohpl_combo_program_insert(OhplComboHandle*, int mode, unsigned flags, uintptr_t handle);
		int ohpl_combo_program_release(OhplComboHandle*, int mode, unsigned flags, uintptr_t* out);
		bool ohpl_combo_shader_acquire(OhplComboHandle*, int mode, unsigned stage, unsigned bits, uintptr_t* out);
		void ohpl_combo_shader_insert(OhplComboHandle*, int mode, unsigned stage, unsigned bits, uintptr_t handle);
		int ohpl_combo_shader_release(OhplComboHandle*, int mode, unsigned stage, unsigned bits, uintptr_t* out);
		bool ohpl_combo_pop_program(OhplComboHandle*, int mode, uintptr_t* out);
		bool ohpl_combo_pop_shader(OhplComboHandle*, int mode, unsigned stage, uintptr_t* out);
	}
}

#endif
