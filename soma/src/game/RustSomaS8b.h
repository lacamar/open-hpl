#ifndef RUST_SOMA_S8B_H
#define RUST_SOMA_S8B_H

// C ABI of rust/src/somaui/ffi_s8b.rs: tone mapping state lives in Rust, every engine call goes through OhplS8bHost.

#include <cstddef>
#include <cstdint>

struct OhplS8bIo
{
	uint64_t p[2];
	int32_t i[4];
	float f[24];
	const uint8_t *s;
	size_t n;
	uint64_t r;
};
static_assert(sizeof(OhplS8bIo) == 152, "OhplS8bIo layout");

struct OhplS8bHost
{
	void *ctx;
	void (*call)(void *, uint32_t, OhplS8bIo *);
};

enum eOhplS8bOp
{
	kS8b_GradingTex = 1, kS8b_NoiseTex, kS8b_TexDestroy, kS8b_TexName, kS8b_WorldGradingName, kS8b_WorldTone, kS8b_AreaCount,
	kS8b_AreaGet, kS8b_CurrentWorld, kS8b_CurrentViewport, kS8b_ToneObject, kS8b_CameraPos, kS8b_SetTone, kS8b_SetLayers,
	kS8b_SetGrading, kS8b_UserBloom, kS8b_SetBloom, kS8b_SetGrain, kS8b_SetSrgb,
};

extern "C"
{
	void ohpl_s8b_tone_install(const OhplS8bHost *host);
	void ohpl_s8b_tone_new();
	void ohpl_s8b_tone_map_loaded(size_t world);
	void ohpl_s8b_tone_update(float dt);
	bool ohpl_s8b_tone_world(float *exposure, float *white_cut);
	void ohpl_s8b_tone_fade_exposure(float exposure, float white_cut, float time);
	void ohpl_s8b_tone_register(void *engine);
}

#endif
