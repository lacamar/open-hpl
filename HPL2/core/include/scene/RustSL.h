#ifndef HPL_RUST_SL_H
#define HPL_RUST_SL_H
// C ABI of rust/src/scene/sl.rs. The mirrors are the Rust-owned state of the light, billboard, beam,
// lens flare and fog shells; sizes are asserted here and in sl.rs.
#include "math/RustTypes.h"
#include "scene/RustLight.h"
#include "scene/RustSceneH11.h"
#include <cstdint>

namespace hpl {
	struct OhplSLFog
	{
		cColor color;
		float brightness;
		cVector3f size;
		float start, end, falloffExp;
		bool backsideOutside, backsideInside, underwater, skybox;
		float noiseStrength, noiseSize;
		cVector3f noiseTurbulence;
		bool lighten, heightBased, exponential;
		float density, heightDensity, heightHorizon;
		cMatrixf model;
	};

	struct OhplSLBb
	{
		uintptr_t material, vtx, haloBv, matMgr;
		cMatrixf haloOcclusion, temp;
		int kind;
		cVector2f size;
		cVector3f axis;
		int lastRenderCount;
		cVector3f haloSourceSize;
		int haloBvMatrixCount;
		float forwardOffset;
		cColor color;
		float haloAlpha, brightness;
		bool isHalo, haloSizeUpdated;
	};

	struct OhplSLBeamEnd
	{
		cColor color;
		uintptr_t beam;
	};

	struct OhplSLBeam
	{
		uintptr_t material, vtx, end, matMgr, fileSearcher;
		int startTransformCount, endTransformCount;
		cMatrixf temp;
		int lastRenderCount;
		cVector2f size;
		cVector3f axis, mid;
		bool tileHeight, multiplyAlpha;
		cColor color;
	};

	struct OhplSLLight
	{
		uintptr_t world, texMgr, fileSearcher, falloffMap, gobo, tracker;
		cColor diffuse, defaultDiffuse, specular;
		OhplLightAnim anim;
		cVector3f maskCenter, maskSize;
		int lightType, shadowRes;
		float shadowBlur, sourceRadius, radius, falloffPow, spotFalloffPow, translucency, brightness, defaultBrightness;
		unsigned int maskId, shadowCastersAffected;
		float shadowBiasMul, shadowDistance, shadowSlopeBiasMul;
		bool occlusionCull, goboSpecular, castDiffuse, castSpecular, maskBox, castShadows;
	};

	struct OhplSLSpot
	{
		OhplSLLight light;
		cMatrixf projection, viewProj, view;
		uintptr_t frustum, spotFalloffMap;
		float fov, aspect, nearClip, tanHalfFov, cosHalfFov;
		bool fovUpdated;
		OhplH11SpotCtl ctl;
	};

	struct OhplSLBox
	{
		OhplSLLight light;
		cVector3f size;
		int blend, prio;
		cColor ambientSky, ambientGround;
		float weight, bevel, falloffPow;
		bool useSh;
		cVector3f probeOffset;
		cVector3f bands[9], fadeFrom[9];
		float fadeT, fadeSpeed;
	};

	struct OhplSLFlareType
	{
		uintptr_t parent;
		int kind;
		cMatrixf model;
	};

	struct OhplSLFlare
	{
		uintptr_t matMgr, lowLevel, vtx, multiIrisVtx, occlusionBv;
		uintptr_t types[3], material[3];
		cMatrixf sizeRot[3], occluder, billboard;
		bool active[3];
		cColor color[3];
		cVector2f size[3];
		cVector2l atlasGrid;
		int seed, count;
		float sizeByDist;
		bool useParentMesh, shrinkWhenOccluded, mulMultiIris;
		cVector2f rangeMax, rangeMin, glareRange;
		float outerFov, innerFov, glareFov, outerCos, innerCos, glareCos, glareBrightness, glareStareAt, fade, fadeRate, brightness;
		cVector3f sourceSize;
		OhplFlareVis vis;
	};

	static_assert(sizeof(OhplSLFog) == 148 && sizeof(OhplSLBb) == 240 && sizeof(OhplSLBeamEnd) == 24, "SL pod layout");
	static_assert(sizeof(OhplSLBeam) == 168 && sizeof(OhplSLLight) == 336, "SL pod layout");
	static_assert(sizeof(OhplSLSpot) == 584 && sizeof(OhplSLBox) == 640, "SL pod layout");

	static_assert(sizeof(OhplSLFlare) == 608 && sizeof(OhplSLFlareType) == 80, "SL pod layout");

	struct OhplSLHostVt
	{
		void *ctx;
		float (*ease)(void*, float);
		void (*setBrightness)(void*, float);
		void (*setRadius)(void*, float);
		void (*setDiffuse)(void*, const cColor*);
		void (*flickerSound)(void*, bool);
		void (*flickerEvent)(void*, bool);
	};

extern "C" {
OhplSLFlareType* ohpl_sl_flare_type_new(uintptr_t, int);
void ohpl_sl_flare_type_free(OhplSLFlareType*);
OhplSLFlare* ohpl_sl_flare_new(int);
void ohpl_sl_flare_free(OhplSLFlare*);
void ohpl_sl_flare_fade(OhplSLFlare*, bool, float);
void ohpl_sl_flare_update(OhplSLFlare*, float);
void ohpl_sl_flare_update_visibility(OhplSLFlare*, float, int);
void ohpl_sl_flare_set_outer_fov(OhplSLFlare*, float);
void ohpl_sl_flare_set_inner_fov(OhplSLFlare*, float);
void ohpl_sl_flare_set_glare_fov(OhplSLFlare*, float);
void ohpl_sl_flare_set_size_by_dist(OhplSLFlare*, float);
float ohpl_sl_flare_bv_radius(const OhplSLFlare*);
int ohpl_sl_flare_first_active(const OhplSLFlare*);
bool ohpl_sl_flare_any_active(const OhplSLFlare*);
void ohpl_sl_flare_color(const OhplSLFlare*, const OhplFlareEnt*, const OhplFlareView*, int, float, cColor*);
void ohpl_sl_flare_glare(const OhplSLFlare*, const OhplFlareEnt*, const OhplFlareView*, float, cColor*, cVector3f*);
cMatrixf* ohpl_sl_flare_size_rotation(OhplSLFlare*, const OhplFlareEnt*, const OhplFlareView*, int, float);
cMatrixf* ohpl_sl_flare_billboard(OhplSLFlare*, const OhplFlareEnt*, const OhplFlareView*);
bool ohpl_sl_flare_type_color(const OhplSLFlare*, const OhplFlareEnt*, const OhplFlareView*, int, float, cColor*);
bool ohpl_sl_flare_collide_box(const OhplSLFlare*, const OhplFlareView*, const cVector3f*, const cVector3f*, const cVector3f*,
							   const cVector3f*, cVector3f*, cVector3f*);
void ohpl_sl_flare_set_target(OhplSLFlare*, float, float, const cVector3f*, const cVector3f*);
int ohpl_sl_flare_multi_iris(const OhplSLFlare*, const OhplFlareEnt*, const OhplFlareView*, float, float*, float*, float*, int);

OhplSLFog* ohpl_sl_fog_new();
void ohpl_sl_fog_free(OhplSLFog*);
void ohpl_sl_fog_set_size(OhplSLFog*, const cVector3f*, cVector3f*);
const cMatrixf* ohpl_sl_fog_model(OhplSLFog*, const cMatrixf*);

OhplSLBb* ohpl_sl_bb_new(int, const cVector2f*);
void ohpl_sl_bb_free(OhplSLBb*);
bool ohpl_sl_bb_set_color(OhplSLBb*, const cColor*);
bool ohpl_sl_bb_set_halo_alpha(OhplSLBb*, float);
bool ohpl_sl_bb_set_brightness(OhplSLBb*, float);
bool ohpl_sl_bb_set_axis(OhplSLBb*, const cVector3f*, float*);
uint32_t ohpl_sl_bb_halo_gates(OhplSLBb*, int);
const cMatrixf* ohpl_sl_bb_model(OhplSLBb*, const cMatrixf*, const cVector3f*, const cVector3f*, bool*);
void ohpl_sl_bb_colors(const OhplSLBb*, bool, float*);

OhplSLBeamEnd* ohpl_sl_beam_end_new(uintptr_t);
void ohpl_sl_beam_end_free(OhplSLBeamEnd*);
OhplSLBeam* ohpl_sl_beam_new();
void ohpl_sl_beam_free(OhplSLBeam*);
void ohpl_sl_beam_set_file(OhplSLBeam*, const char*, size_t);
const char* ohpl_sl_beam_file(const OhplSLBeam*, size_t*);
bool ohpl_sl_beam_set_color(OhplSLBeam*, const cColor*, float*);
bool ohpl_sl_beam_set_end_color(OhplSLBeam*, OhplSLBeamEnd*, const cColor*, float*);
void ohpl_sl_beam_bounds(const OhplSLBeam*, const cVector3f*, const cVector3f*, cVector3f*, cVector3f*);
bool ohpl_sl_beam_geometry(OhplSLBeam*, int, const cVector3f*, const cVector3f*, float*, float*);
const cMatrixf* ohpl_sl_beam_model(OhplSLBeam*, const cMatrixf*, const cVector3f*, const cVector3f*);

OhplSLLight* ohpl_sl_light_new(int);
void ohpl_sl_light_free(OhplSLLight*);
void ohpl_sl_set_hpl3_visibility(bool);
bool ohpl_sl_hpl3_visibility();
float ohpl_sl_spot_shadow_near();
void ohpl_sl_spot_set_shadow_near(float);
bool ohpl_sl_light_is_lit(const OhplSLLight*);
bool ohpl_sl_light_is_visible(const OhplSLLight*, bool);
bool ohpl_sl_light_set_diffuse(OhplSLLight*, const cColor*);
bool ohpl_sl_light_set_brightness(OhplSLLight*, float);
void ohpl_sl_light_fade_brightness_to(OhplSLLight*, float, float);
void ohpl_sl_light_fade_to(OhplSLLight*, const cColor*, float, float);
void ohpl_sl_light_set_flicker(OhplSLLight*, const OhplFlickerCfg*, const char *const (*)[4], const size_t (*)[4]);
const char* ohpl_sl_light_string(OhplSLLight*, int, size_t*);
void ohpl_sl_light_update(OhplSLLight*, const OhplSLHostVt*, float);
void ohpl_sl_light_attach_billboard(OhplSLLight*, uintptr_t, const cColor*);
void ohpl_sl_light_remove_billboard(OhplSLLight*, uintptr_t);
bool ohpl_sl_light_has_billboard(OhplSLLight*, uintptr_t);
size_t ohpl_sl_light_billboard_count(OhplSLLight*);
uintptr_t ohpl_sl_light_billboard_at(OhplSLLight*, size_t, cColor*);
void ohpl_sl_light_attach_ps(OhplSLLight*, uintptr_t);
bool ohpl_sl_light_remove_ps(OhplSLLight*, uintptr_t);
size_t ohpl_sl_light_ps_count(OhplSLLight*);
uintptr_t ohpl_sl_light_ps_at(OhplSLLight*, size_t);
void ohpl_sl_light_cache_add(OhplSLLight*, uintptr_t, int);
bool ohpl_sl_light_cache_valid(OhplSLLight*, uintptr_t, int);
bool ohpl_sl_light_cache_unchanged(OhplSLLight*, const OhplH11Caster*, int);
void ohpl_sl_light_cache_set(OhplSLLight*, const OhplH11Caster*, int);
void ohpl_sl_light_cache_clear(OhplSLLight*);
void ohpl_sl_box_add_set(OhplSLBox*, const char*, size_t, const cVector3f*, size_t);
bool ohpl_sl_box_fade_set(OhplSLBox*, const char*, size_t, float);
void ohpl_sl_box_fade_step(OhplSLBox*, float);
bool ohpl_sl_box_fading(const OhplSLBox*);
}
}
#endif
