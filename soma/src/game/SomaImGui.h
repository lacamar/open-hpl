#ifndef SOMA_IMGUI_H
#define SOMA_IMGUI_H

#include "hpl.h"
#include "RustSomaS8d.h"

using namespace hpl;

struct cSomaCameraTextureState
{
	tString msName, msAttached;
	cVector2l mvSize;
	unsigned mlFPS;
	float mfFOV, mfNear, mfFar;
	cMatrixf mtxRotation;
	cVector3f mvPosition;
};
std::vector<cSomaCameraTextureState> SomaGetCameraTextures();
iTexture *SomaGetCameraTexture(const tString &asName);
void SomaRestoreCameraTexture(const cSomaCameraTextureState &aState);
void SomaUpdateCameraTextures();
void SomaDestroyCameraTextures();
void SomaInstallImGuiHost();

enum { kSomaImInt, kSomaImFloat, kSomaImVec, kSomaImCol };

// The ImGui of the Rust port; a pointer to one of these is the Rust object itself.
class cSomaImGui
{
public:
	cSomaImGui() = delete;
	static cSomaImGui *Create(const tString &asName, cGuiSet *apSet) { return R(ohpl_s8d_new((const unsigned char *)asName.data(), asName.size(), (size_t)apSet)); }
	static cSomaImGui *GetCurrent() { return R(ohpl_s8d_current()); }
	static void SetCurrent(cSomaImGui *apImGui) { ohpl_s8d_set_current((OhplS8dIm *)apImGui); }
	static cSomaImGui *GetInputFocus() { return R(ohpl_s8d_input_focus()); }
	static cSomaImGui *GetScriptInputFocus() { return R(ohpl_s8d_script_input_focus()); }
	static cSomaImGui *GetPrevInputFocus() { return R(ohpl_s8d_prev_input_focus()); }
	static void SetInputFocus(cSomaImGui *apImGui, bool abShowMouse) { ohpl_s8d_set_input_focus(apImGui ? apImGui->Raw() : NULL, abShowMouse); }
	static void UpdateFocusHistory() { ohpl_s8d_update_focus_history(); }
	static bool GameHudFocus() { return ohpl_s8d_game_hud_focus(); }
	static void SetGameHudFocus(bool abX) { ohpl_s8d_set_game_hud_focus(abX); }

	void Begin(float afTimeStep) { ohpl_s8d_begin(Raw(), afTimeStep); }
	void End() { ohpl_s8d_end(Raw()); }
	void DrawAll() { ohpl_s8d_draw_all(Raw()); }
	tString DebugOps(size_t alMax)
	{
		tString s;
		ohpl_s8d_debug_ops(Raw(), alMax, +[](void *c, const unsigned char *p, size_t n) { ((tString *)c)->append((const char *)p, n); }, &s);
		return s;
	}

	cGuiSet *GetSet() { return (cGuiSet *)ohpl_s8d_set(Raw()); }
	const tString &GetName() { return *(const tString *)ohpl_s8d_name(Raw()); }
	int GetDrawnOpNum() { return (int)ohpl_s8d_drawn(Raw()); }
	void SendMousePosition(const cVector2l &avPos, const cVector2l &avRel) { ohpl_s8d_send_mouse(Raw(), avPos.x, avPos.y, avRel.x, avRel.y); }
	void SendMouseVirtualPosition(const cVector2f &avPos, const cVector2f &avRel) { ohpl_s8d_send_mouse_virtual(Raw(), avPos.v, avRel.v); }
	cVector2f GetMousePosition() { float *p = ohpl_s8d_mouse_pos(Raw()); return cVector2f(p[0], p[1]); }
	cVector2f GetCursor3D() { float *p = ohpl_s8d_cursor_3d(Raw()); return cVector2f(p[0], p[1]); }
	void SetCursor3D(const cVector2f &avPos) { float *p = ohpl_s8d_cursor_3d(Raw()); p[0] = avPos.x, p[1] = avPos.y; }
	void SetScreenClear(const cColor &aClear, const cColor &aOffline)
	{
		memcpy(ohpl_s8d_screen_clear(Raw(), false), aClear.v, 16);
		memcpy(ohpl_s8d_screen_clear(Raw(), true), aOffline.v, 16);
	}
	cColor GetScreenClear(bool abOffline) { float *p = ohpl_s8d_screen_clear(Raw(), abOffline); return cColor(p[0], p[1], p[2], p[3]); }
	bool GetShowMouse() { return ohpl_s8d_show_mouse(Raw()); }
	void SetShowMouse(bool abX) { ohpl_s8d_set_show_mouse(Raw(), abX); }

	// Same layout as the Rust State; SomaSave writes and reads these
	struct cState
	{
		int mlInt;
		float mfFloat;
		cVector3f mvVec;
		cColor mCol;
		bool mbSetInt, mbSetFloat, mbSetVec, mbSetCol, mbInFocus;
		int mlDefInt;
		float mfDefFloat;
		bool mbDefInt, mbDefFloat;
	};
	typedef OhplS8dTimer cTimer;
	cState *State(uint64_t alId) { return (cState *)ohpl_s8d_state(Raw(), alId); }
	size_t StateCount() { return ohpl_s8d_state_count(Raw()); }
	cState *StateAt(size_t alIdx, uint64_t *apId) { return (cState *)ohpl_s8d_state_at(Raw(), alIdx, apId); }
	size_t TimerCount() { return ohpl_s8d_timer_count(Raw()); }
	const cTimer *TimerAt(size_t alIdx) { return ohpl_s8d_timer_at(Raw(), alIdx); }
	void PushTimer(const cTimer &aTimer) { ohpl_s8d_timer_push(Raw(), &aTimer); }
	template <class F> F GetState(uint64_t alId, int alKind, const F &aDefault)
	{
		F out;
		ohpl_s8d_state_op(Raw(), alId, 0, alKind, &aDefault, &out);
		return out;
	}
	template <class F> void SetState(uint64_t alId, int alKind, const F &aValue) { ohpl_s8d_state_op(Raw(), alId, 1, alKind, &aValue, NULL); }
	template <class F> void IncState(uint64_t alId, int alKind, const F &aValue) { ohpl_s8d_state_op(Raw(), alId, 2, alKind, &aValue, NULL); }
	void Fade(uint64_t alId, int alType, const float *apGoal, float afTime, int alEasing, bool abReplace) { ohpl_s8d_fade(Raw(), alId, alType, apGoal, afTime, alEasing, abReplace); }
	void StopFade(uint64_t alId) { ohpl_s8d_stop_fade(Raw(), alId); }

private:
	static cSomaImGui *R(OhplS8dIm *p) { return (cSomaImGui *)p; }
	OhplS8dIm *Raw() { return (OhplS8dIm *)this; }
};

inline cSomaImGui *SomaHudImGui() { return (cSomaImGui *)ohpl_s8d_hud(); }
inline void SomaDrawImGuis() { ohpl_s8d_draw_imguis(); }
inline void SomaRenderImGuis() { ohpl_s8d_render_imguis(); }
inline void SomaPreloadGuiGfx(const tString &asFile, int alType) { ohpl_s8d_preload_gfx((const unsigned char *)asFile.data(), asFile.size(), alType); }
tWString SomaParseString(const tWString &asText);
void *SomaNewScriptStruct(const char *apType);
void *SomaNewOwnedScriptStruct(const char *apType);
const tString *SomaIntern(const tString &asStr);

#endif // SOMA_IMGUI_H
