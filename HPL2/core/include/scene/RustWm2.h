#ifndef HPL_RUST_WM2_H
#define HPL_RUST_WM2_H

// C ABI of rust/src/scene/wm2_*.rs.

#include "math/RustTypes.h"
#include <cstddef>
#include <cstdint>

namespace hpl {

	struct OhplWm2Sub
	{
		bool graphics_updated;
		bool update_body;
		int32_t skinned_version;
		int32_t static_null_count;
		void *user_data;
		void *material;
		void *local_node;
		void *dyn_vb;
	};

	struct OhplWm2SkinIn
	{
		const float *bind_pos, *bind_normal, *bind_tangent;
		float *skin_pos, *skin_normal, *skin_tangent;
		const float *weights;
		const unsigned char *bones;
		const cMatrixf *matrices;
		int32_t vertex_count, pos_stride, matrix_count;
	};

	struct OhplWm2GateIn
	{
		int32_t mesh_version;
		bool sleeping;
	};

	struct OhplWm2SubHost
	{
		void *ctx;
		bool (*has_skeleton)(void*);
		void *(*copy_vb)(void*);
		void (*delete_vb)(void*);
		void (*destroy_material)(void*, void*);
		void (*update_mesh)(void*, float);
		bool (*skin_gate)(void*, OhplWm2GateIn*);
		void (*skin_in)(void*, OhplWm2SkinIn*);
		void (*upload_vb)(void*);
	};

	static_assert(sizeof(OhplWm2Sub) == 48 && offsetof(OhplWm2Sub, user_data) == 16, "WM2 sub layout");
	static_assert(sizeof(OhplWm2SkinIn) == 88 && sizeof(OhplWm2GateIn) == 8, "WM2 skin layout");

extern "C" {

OhplWm2Sub* ohpl_wm2_sub_new();
void ohpl_wm2_sub_init(OhplWm2Sub*, const OhplWm2SubHost*);
void ohpl_wm2_sub_free(OhplWm2Sub*, const OhplWm2SubHost*);
void ohpl_wm2_sub_set_material(OhplWm2Sub*, const OhplWm2SubHost*, void*, bool);
void ohpl_wm2_sub_update_gfx(OhplWm2Sub*, const OhplWm2SubHost*, float);
bool ohpl_wm2_sub_model_null(OhplWm2Sub*, bool, const cMatrixf*);

}
}

#endif
