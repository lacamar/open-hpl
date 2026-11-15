#ifndef RUST_SOMA_S8_H
#define RUST_SOMA_S8_H

// C ABI of rust/src/somaui/ffi_s8.rs: the splash and the gamma screen. Rust owns their state;
// every engine call goes through OhplS8Host.

#include <cstddef>
#include <cstdint>

struct OhplS8Arg
{
	uint64_t v;
	size_t n;
};

typedef void (*OhplS8Sink)(void*, const uint32_t*, size_t);

struct OhplS8Call
{
	int op;
	uint32_t na;
	const OhplS8Arg *a;
	uint64_t u[8];
	OhplS8Sink sink;
	void *sinkCtx;
};

struct OhplS8Host
{
	void *ctx;
	void (*call)(void*, OhplS8Call*);
};

enum eOhplS8Op
{
	kS8_SetNew = 1, kS8_SetDrawMouse, kS8_SetActive, kS8_GuiFocus, kS8_GuiFocusedIs, kS8_ViewportNew,
	kS8_ViewportAddSet, kS8_ViewportActive, kS8_GfxRect, kS8_GfxTex, kS8_GfxSize, kS8_GfxDestroy,
	kS8_DrawGfx, kS8_DrawFont, kS8_FontNew, kS8_FontWrap, kS8_ClipNew, kS8_ClipRect, kS8_ClipGet,
	kS8_ClipSet, kS8_SliderNew, kS8_ButtonNew, kS8_SliderSet, kS8_SliderGet, kS8_DefaultFocusNav,
	kS8_FocusedWidget, kS8_Music, kS8_Now, kS8_Screen, kS8_LoadMenu, kS8_HudImGui, kS8_BaseSplashDone,
	kS8_BaseGammaDone, kS8_SaveGamma, kS8_GammaGet, kS8_GammaSet, kS8_FrameBegin, kS8_ImGuiRender,
	kS8_FrameEnd, kS8_MousePoll, kS8_GuiMousePos, kS8_Click, kS8_KeyPoll, kS8_StateDir, kS8_ClipDelete,
};

extern "C"
{
	uint64_t ohpl_s8_splash_new(const OhplS8Host *host, uint64_t *hOut);
	void ohpl_s8_splash_delete(const OhplS8Host *host, uint64_t h);
	void ohpl_s8_splash_update(const OhplS8Host *host, uint64_t h, float dt);
	void ohpl_s8_splash_draw(const OhplS8Host *host, uint64_t h);
	void ohpl_s8_splash_load_draw(const OhplS8Host *host, uint64_t h, bool boot);
	bool ohpl_s8_splash_scripts_may_run(uint64_t h);
	void ohpl_s8_splash_start_load(const OhplS8Host *host, uint64_t h);
	void ohpl_s8_splash_set_load_bar(uint64_t h, const float *pos, const float *size);
	bool ohpl_s8_gamma_should_show(const OhplS8Host *host);
	uint64_t ohpl_s8_gamma_new(const OhplS8Host *host, uint64_t *hOut);
	void ohpl_s8_gamma_delete(uint64_t h);
	void ohpl_s8_gamma_update(const OhplS8Host *host, uint64_t h);
	void ohpl_s8_gamma_draw(const OhplS8Host *host, uint64_t h);
	bool ohpl_s8_gamma_cb(const OhplS8Host *host, uint64_t h, int tag);
}

#endif
