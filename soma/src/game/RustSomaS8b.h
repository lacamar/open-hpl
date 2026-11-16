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

enum eOhplS8cOp
{
	kS8c_NewShell = 32,
	kS8c_DeleteShell, kS8c_IsActive, kS8c_SetActive, kS8c_SetDisabled, kS8c_IsDisabled, kS8c_CompAdd, kS8c_CompRemove,
	kS8c_CompNum, kS8c_CompGet, kS8c_ViewportComp, kS8c_FrameTime, kS8c_TrailAlpha, kS8c_ProgramFromShaders,
	kS8c_ShaderNew, kS8c_ShaderSampler, kS8c_ProgramNew, kS8c_ProgramShader, kS8c_ProgramLink, kS8c_UniF, kS8c_UniV2,
	kS8c_UniV3, kS8c_UniV4, kS8c_UniArr, kS8c_UniMat, kS8c_SetFlat, kS8c_SetBlend, kS8c_SetChannel, kS8c_SetRange,
	kS8c_SetTexture, kS8c_SetProgram, kS8c_SetFinalFb, kS8c_SetFb, kS8c_DrawQuad, kS8c_FbResult, kS8c_TexSize,
	kS8c_TexBilinear, kS8c_ScreenSize, kS8c_CreateTarget, kS8c_DestroyTex, kS8c_DestroyFb, kS8c_CompSetFb,
	kS8c_ClearWhite, kS8c_SetDepthTest, kS8c_SetDepthWrite, kS8c_SetCull, kS8c_SetMatrixNull, kS8c_SetVb,
	kS8c_DrawCurrent, kS8c_MatCreate, kS8c_MatDestroy, kS8c_MatTex, kS8c_XmlTentacle, kS8c_EntNew, kS8c_EntAnim,
	kS8c_EntPlay, kS8c_EntAnimIndex, kS8c_EntBaseSpeed, kS8c_EntState, kS8c_EntUpdate, kS8c_EntVb, kS8c_EntDelete,
	kS8c_DumpOn, kS8c_Log, kS8c_LogEnd,
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
	size_t ohpl_s8c_render(size_t shell, size_t tex, size_t fb);
	void ohpl_s8c_reset(size_t shell);
	void ohpl_s8c_register(void *engine);
}

#endif
