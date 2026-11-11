#ifndef HPL_RUST_LIGHT_H
#define HPL_RUST_LIGHT_H

// C ABI of rust/src/scene/ffi_light.rs. Layouts mirror light.rs / lensflare.rs / rope.rs.

#include "math/RustTypes.h"
#include <cstddef>
#include <cstdint>

namespace hpl {
	struct OhplLightAnim
	{
		float bright_fade_time, bright_fade_count, bright_fade_start, bright_fade_dest;
		cColor col_add;
		float radius_add;
		cColor dest_col;
		float dest_radius, fade_time;
		float flicker_on_min_length, flicker_off_min_length, flicker_on_max_length, flicker_off_max_length;
		cColor flicker_off_color;
		float flicker_off_radius;
		float flicker_on_fade_min_length, flicker_on_fade_max_length;
		float flicker_off_fade_min_length, flicker_off_fade_max_length;
		cColor flicker_on_color;
		float flicker_on_radius, flicker_time, flicker_state_length;
		bool flickering, flicker_fade, flicker_on;
	};

	struct OhplLightLive
	{
		cColor diffuse;
		float radius, brightness;
	};

	struct OhplFlickerCfg
	{
		cColor off_color;
		float off_radius, on_min_length, on_max_length, off_min_length, off_max_length,
				on_fade_min_length, on_fade_max_length, off_fade_min_length, off_fade_max_length;
		bool fade;
	};

	struct OhplLightHostVt
	{
		void *ctx;
		float (*ease)(void*, float);
		void (*set_brightness)(void*, float);
		void (*set_radius)(void*, float);
		void (*set_diffuse)(void*, const cColor*);
		void (*assign_diffuse)(void*, const cColor*);
		void (*raw_radius)(void*, float);
		void (*flicker_sound)(void*, bool);
		void (*flicker_event)(void*, bool);
	};

	struct OhplFlareIn
	{
		bool active[3];
		cColor color[3];
		cVector2f size[3];
		cVector2f range_max, range_min, glare_range;
		cVector2l atlas_grid;
		int32_t multi_iris_seed, multi_iris_count;
		float size_by_dist, outer_cos, inner_cos, glare_cos, glare_brightness, glare_stare_at, fade, brightness;
		bool shrink_when_occluded, mul_iris_with_glare;
	};

	struct OhplFlareEnt
	{
		cVector3f pos, axis;
	};

	struct OhplFlareView
	{
		cVector3f origin;
		cMatrixf view, proj;
		float aspect;
	};

	struct OhplFlareVis
	{
		float target, vis, vel;
		int32_t frame;
	};

	struct OhplRopeParams
	{
		cVector3f forward;
		float radius, segment_length, tile_amount, tile_size;
	};

	static_assert(sizeof(OhplLightAnim) == 144 && offsetof(OhplLightAnim, flickering) == 140, "OhplLightAnim layout");
	static_assert(sizeof(OhplLightLive) == 24 && sizeof(OhplFlickerCfg) == 56, "light cfg layout");
	static_assert(sizeof(OhplFlareVis) == 16 && sizeof(OhplRopeParams) == 28, "flare/rope layout");
	static_assert(offsetof(OhplFlareIn, color) == 4 && offsetof(OhplFlareIn, brightness) == 144 && sizeof(OhplFlareIn) == 152, "OhplFlareIn layout");
	static_assert(sizeof(OhplFlareView) == 144 && sizeof(OhplFlareEnt) == 24, "OhplFlareView layout");

extern "C" {

void ohpl_light_update(OhplLightAnim*, OhplLightLive*, bool, bool, OhplLightHostVt*, float);
void ohpl_light_fade_brightness_to(OhplLightAnim*, const OhplLightLive*, float, float);
void ohpl_light_fade_to(OhplLightAnim*, const OhplLightLive*, const cColor*, float, float);
void ohpl_light_set_flicker(OhplLightAnim*, const OhplLightLive*, const OhplFlickerCfg*);
void ohpl_light_spot_projection(float, float, float, float, float, cMatrixf*);
float ohpl_light_spot_near(float, float);
void ohpl_light_spot_half_fov(float, float*, float*);
void ohpl_light_spot_view_proj(const cMatrixf*, const cMatrixf*, cMatrixf*);
float ohpl_light_box_fade_speed(float);
bool ohpl_light_box_fade_step(cVector3f*, const cVector3f*, const cVector3f*, float*, float, float);

float ohpl_flare_fade_in_rate(float, float);
float ohpl_flare_fade_out_rate(float, float);
float ohpl_flare_update_fade(float, float, float);
float ohpl_flare_outer_cos(float, float);
void ohpl_flare_set_inner_fov(float, float, float*, float*);
float ohpl_flare_clamp01(float);
void ohpl_flare_update_visibility(OhplFlareVis*, float, int);
float ohpl_flare_bv_radius(const bool*, const cVector2f*);
void ohpl_flare_color(const OhplFlareIn*, const OhplFlareEnt*, const OhplFlareView*, int, float, cColor*);
void ohpl_flare_glare(const OhplFlareIn*, const OhplFlareEnt*, const OhplFlareView*, float, cColor*, cVector3f*);
void ohpl_flare_size_rotation(const OhplFlareIn*, const OhplFlareEnt*, const OhplFlareView*, int, float, cMatrixf*);
void ohpl_flare_billboard(const OhplFlareIn*, const OhplFlareEnt*, const OhplFlareView*, cMatrixf*);
bool ohpl_flare_type_color(const OhplFlareIn*, const OhplFlareEnt*, const OhplFlareView*, int, float, cColor*);
float ohpl_flare_occlusion_target(float, float, const cVector3f*, const cVector3f*);
bool ohpl_flare_collide_box(const OhplFlareIn*, const OhplFlareView*, const cVector3f*, const cVector3f*,
							const cVector3f*, const cVector3f*, cVector3f*, cVector3f*);
int ohpl_flare_multi_iris(const OhplFlareIn*, const OhplFlareEnt*, const OhplFlareView*, float, float*, float*,
						  float*, int);

void ohpl_beam_set_edge_colors(const cColor*, bool, float*);
void ohpl_beam_bounds(const cVector3f*, const cVector3f*, float, cVector3f*, cVector3f*);
void ohpl_beam_geometry(const cVector3f*, const cVector3f*, float, float, bool, float*, float*, cVector3f*, cVector3f*);
void ohpl_beam_model_matrix(const cMatrixf*, const cVector3f*, const cVector3f*, const cVector3f*, const cVector3f*,
							cMatrixf*);

void ohpl_rope_set_colors(const cColor*, bool, float*, int);
void ohpl_rope_bounds(const cVector3f*, int, float, cVector3f*, cVector3f*);
int ohpl_rope_quads(const cVector3f*, int, const OhplRopeParams*, float*, float*, float*, float*, int);

}
}

#endif
