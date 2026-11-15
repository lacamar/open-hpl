#ifndef LUX_RUST_MENU_HOST_H
#define LUX_RUST_MENU_HOST_H

// C++ side of rust/src/luxmenu/host.rs: widget, GUI set and engine ops for the options and key
// config windows, and the Rust entry points those two window shells forward to.

#include "LuxBase.h"
#include <cstddef>
#include <cstdint>

struct OhplA10aArgs
{
	size_t p[6];
	float f[6];
	int i[4];
	const uint32_t* s[4];
	size_t n[4];
};

struct OhplA10aStr
{
	uint32_t n;
	char b[60];
};

struct OhplA10aOpts
{
	int showSubtitles, showEffectSubtitles, showHints, showDeathHints, showCrosshair, focusIconStyle, showCommentary;
	int resX, resY, display, fullscreen, vsync, showFps;
	float paperWhite, peakNits;
	int guiScale, texQuality, texFilter;
	float aniso;
	int edgeSmooth, shadows, shadowQuality, shadowRes, parallax, worldReflection, refraction;
	int ssao, ssaoSamples, ssaoRes, bloom, imageTrail, sepia, radialBlur, insanity;
	float gamma;
	int invertMouse, smoothMouse;
	float mouseSens;
	int padInvert;
	float padSens, volume;
	int soundDev;
	OhplA10aStr lang, renderer, color;
};

struct OhplA10aCb
{
	int lval;
	float fval;
	int key;
	float padValue;
	int padType;
	int padId;
	int msg;
};

extern "C"
{
	int ohpl_a10a_set_host(size_t (*apHost)(int, OhplA10aArgs*), size_t alOptsSize);
	uint64_t ohpl_a10a_opt_new(size_t alSet);
	size_t ohpl_a10a_opt_create_gui(uint64_t alMe);
	void ohpl_a10a_opt_exit_pressed(uint64_t alMe);
	void ohpl_a10a_opt_on_set_active(uint64_t alMe, int alActive);
	void ohpl_a10a_opt_free(uint64_t alMe);
	uint64_t ohpl_a10a_kc_new(size_t alSet);
	size_t ohpl_a10a_kc_create_gui(uint64_t alMe);
	void ohpl_a10a_kc_exit_pressed(uint64_t alMe);
	void ohpl_a10a_kc_on_set_active(uint64_t alMe, int alActive);
	void ohpl_a10a_kc_free(uint64_t alMe);
	int ohpl_a10a_cb(size_t alToken, size_t alWidget, const OhplA10aCb* apData);
}

void OhplA10aHostInit();
void OhplA10aDumpWindow(const char* asName, iWidget* apRoot);

#endif
