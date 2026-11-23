#ifndef LUX_RUST_LUX_FX_A12A_H
#define LUX_RUST_LUX_FX_A12A_H

// C ABI of rust/src/luxfx/fxffi.rs and host.rs: the effect handler, effect renderer and insanity
// post effect clock live in Rust; the C++ shells hold a handle and forward. OhplA12aHost is the
// engine side of host.rs.

#include <cstddef>
#include <cstdint>
#include "RustLuxFx.h"

struct OhplA12aBytes
{
	const unsigned char *p;
	size_t n;
};

typedef void (*OhplA12aSink)(void*, const unsigned char*, size_t);
typedef void (*OhplA12aSink32)(void*, const unsigned*, size_t);

struct OhplA12aHost
{
	int (*snd_valid)(size_t, int);
	size_t (*snd_play_gui)(OhplA12aBytes, int*);
	size_t (*snd_play_3d)(OhplA12aBytes, const float*, float, float, int*);
	void (*snd_fade_out)(size_t, float);
	void (*snd_stop)(size_t);
	void (*snd_pause)(size_t, int);
	void (*snd_volmul)(size_t, float);
	double (*snd_elapsed)(size_t);
	void (*snd_set_elapsed)(size_t, double);
	void (*snd_commentary)(int);
	void (*snd_reset_world)();
	void (*gui_sound)(OhplA12aBytes);
	void (*translate)(OhplA12aBytes, OhplA12aBytes, OhplA12aSink32, void*);
	void (*wrap)(size_t, float, float, const float*, const unsigned*, size_t, OhplA12aSink32, void*);
	size_t (*font_load)();
	void (*font_destroy)(size_t);
	void (*hud)(float*);
	void (*draw_font)(size_t, const float*, const float*, const float*, int, const unsigned*, size_t);
	float (*cfg_f32)(OhplA12aBytes, OhplA12aBytes, float);
	void (*cfg_vec2)(OhplA12aBytes, OhplA12aBytes, float, float*);
	void (*cfg_str)(OhplA12aBytes, OhplA12aBytes, OhplA12aBytes, OhplA12aSink, void*);
	void (*cfg_color)(OhplA12aBytes, OhplA12aBytes, float*);
	int (*subtitles)();
	double (*game_time)();
	void (*player)(int, float, float, float);
	void (*map)(int, int, OhplA12aBytes);
	void (*post)(int, int, float, float);
	int (*r_reflection)(size_t);
	int (*r_collides)(size_t, size_t);
	void (*r_outline)(size_t, size_t, OhplGa9bOutline*);
	void (*r_clip)(size_t, const float*, const float*, int*);
	void (*r_replay)(size_t, const size_t*, size_t, const OhplGa9bROp*, size_t, const int*);
	void (*p_replay)(size_t, size_t, size_t, const OhplGa9bPOp*, size_t);
	size_t (*dump)(int, float);
};

struct OhplA12aFxState
{
	OhplGa9bFade fade;
	OhplGa9bFlash flash, sanity;
	OhplGa9bAmount trail, sepia, radial;
	float radialStart;
	OhplGa9bEmotion emotion;
	uint32_t shakeActive, shakeCount;
	uint32_t voiceActive, voicePaused, voiceCount, voiceRows, voiceQueue, voiceEntries;
	float voiceVol;
	uint32_t commentaryActive;
	int32_t commentaryIcon;
	uint32_t playerPaused;
};
static_assert(sizeof(OhplA12aFxState) == 196, "OhplA12aFxState layout");

struct OhplA12aStr
{
	const unsigned char *p;
	size_t n;
};

struct OhplA12aVoiceIn
{
	OhplA12aStr voice, effect, cat, entries[7];
	float delays[7];
	int32_t usePos;
	float pos[3];
	float minDist, maxDist;
	int32_t priority, removeInterrupted;
};
static_assert(sizeof(OhplA12aVoiceIn) == 224, "OhplA12aVoiceIn layout");

struct OhplA12aTexts
{
	const unsigned *p;
	size_t n;
};

struct OhplA12aVoiceSave
{
	OhplA12aTexts texts[7];
	float delays[7];
	OhplA12aStr voice, effect;
	int32_t usePos;
	float pos[3];
	float minDist, maxDist;
	int32_t priority;
	double currentTime, interruptedAt;
};
static_assert(sizeof(OhplA12aVoiceSave) == 224, "OhplA12aVoiceSave layout");

enum
{
	eOhplA12aFx_Fade,
	eOhplA12aFx_Flash,
	eOhplA12aFx_SanityFlash,
	eOhplA12aFx_Voice,
	eOhplA12aFx_ImageTrail,
	eOhplA12aFx_Shake,
	eOhplA12aFx_Sepia,
	eOhplA12aFx_RadialBlur,
	eOhplA12aFx_Emotion,
	eOhplA12aFx_Commentary,
};

enum
{
	eOhplA12aPost_RadialBlur,
	eOhplA12aPost_Sepia,
	eOhplA12aPost_ImageTrail,
};

extern "C"
{
	void ohpl_a12a_set_host(const OhplA12aHost *apHost);
	int ohpl_a12a_dump_gated();

	uint64_t ohpl_a12a_fx_new();
	void ohpl_a12a_fx_free(uint64_t h);
	void ohpl_a12a_fx_update(uint64_t h, float dt);
	void ohpl_a12a_fx_draw(uint64_t h);
	void ohpl_a12a_fx_reset(uint64_t h);
	void ohpl_a12a_fx_map_leave(uint64_t h);
	void ohpl_a12a_fx_action(uint64_t h, int pressed);
	void ohpl_a12a_fx_fonts(uint64_t h, int load);
	void ohpl_a12a_fx_paused_set(uint64_t h, int on);
	int ohpl_a12a_fx_paused_get(uint64_t h);
	int ohpl_a12a_fx_active(uint64_t h, int which);
	int ohpl_a12a_fx_fade(uint64_t h, int op, float t);
	void ohpl_a12a_fx_flash_start(uint64_t h, float a, float b, float c);
	void ohpl_a12a_fx_sanity(uint64_t h, int op, float dt, size_t set);
	void ohpl_a12a_fx_fade_to(uint64_t h, int kind, float goal, float speed);
	void ohpl_a12a_fx_radial_dist(uint64_t h, float d);
	void ohpl_a12a_fx_shake(uint64_t h, float amount, float time, float fadeIn, float fadeOut);
	void ohpl_a12a_fx_emotion_start(uint64_t h, const char *cat, size_t cn, const char *entry, size_t en, const char *sound, size_t sn);
	void ohpl_a12a_fx_commentary(uint64_t h, int op, const char *file, size_t n, int icon);
	void ohpl_a12a_fx_voice_add(uint64_t h, const OhplA12aVoiceIn *v);
	int ohpl_a12a_fx_voice(uint64_t h, int op, float arg);
	void ohpl_a12a_fx_voice_cb_set(uint64_t h, const char *p, size_t n);
	void ohpl_a12a_fx_voice_cb_get(uint64_t h, OhplA12aSink sink, void *ctx);
	void ohpl_a12a_fx_get_state(uint64_t h, OhplA12aFxState *out);
	void ohpl_a12a_fx_set_state(uint64_t h, const OhplA12aFxState *st);
	size_t ohpl_a12a_fx_voice_count(uint64_t h);
	int ohpl_a12a_fx_voice_get(uint64_t h, size_t i, OhplA12aVoiceSave *out);
	void ohpl_a12a_fx_voice_clear(uint64_t h);
	void ohpl_a12a_fx_voice_push(uint64_t h, const OhplA12aVoiceSave *v);

	uint64_t ohpl_a12a_rend_new(uint32_t programs);
	void ohpl_a12a_rend_free(uint64_t h);
	void ohpl_a12a_rend_update(uint64_t h, float dt);
	void ohpl_a12a_rend_clear(uint64_t h, int op);
	void ohpl_a12a_rend_add(uint64_t h, int kind, size_t obj, float alpha);
	void ohpl_a12a_rend_render(uint64_t h, size_t fns);

	uint64_t ohpl_a12a_ins_new();
	void ohpl_a12a_ins_free(uint64_t h);
	void ohpl_a12a_ins_update(uint64_t h, float dt);
	void ohpl_a12a_ins_set(uint64_t h, int which, float v);
	void ohpl_a12a_ins_get(uint64_t h, float *out);
	void ohpl_a12a_ins_render(uint64_t h, float sx, float sy, int programOk, size_t effect, size_t input, size_t fb);
}

void OhplA12aInitHost();

#endif
