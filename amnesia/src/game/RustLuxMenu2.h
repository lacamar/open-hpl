#ifndef LUX_RUST_MENU2_H
#define LUX_RUST_MENU2_H

// C ABI of rust/src/luxmenu2/ffi.rs: main menu fades and exit flow, window switching, top menu
// plan and start/load/custom story rules.
// The state structs live in the C++ objects; Rust never calls back.

#include <cstddef>
#include <cstdint>
#include <string>

struct OhplGa7bMenu
{
	float mfFadeAlpha;
	float mfTopAlpha;
	float mfDescAlpha;
	bool mbExiting;
	bool mbFadeDesc;
	bool mbTopVisible;
	int mlExitMsg;
	int mlWindow;
};

struct OhplGa7bBase
{
	float mfFov;
	bool mbSetFov;
	bool mbExit;
};

struct OhplGa7bTimes
{
	float mfFadeIn, mfOutFast, mfOutSlow, mfFov, mfZoomedFov;
};

enum eOhplGa7bPlanFlag
{
	eOhplGa7bPlan_PTest = 1,
	eOhplGa7bPlan_Map = 2,
	eOhplGa7bPlan_SaveExists = 4,
	eOhplGa7bPlan_Hard = 8,
	eOhplGa7bPlan_ExitDirect = 16,
	eOhplGa7bPlan_Dead = 32,
	eOhplGa7bPlan_NoCustom = 64,
};

enum eOhplGa7bLabel
{
	eOhplGa7bLabel_Continue,
	eOhplGa7bLabel_Back,
	eOhplGa7bLabel_Start,
	eOhplGa7bLabel_Load,
	eOhplGa7bLabel_Save,
	eOhplGa7bLabel_Custom,
	eOhplGa7bLabel_Options,
	eOhplGa7bLabel_Profile,
	eOhplGa7bLabel_Exit,
	eOhplGa7bLabel_ToMenu,
	eOhplGa7bLabel_AndSave,
};

enum eOhplGa7bMenuExit
{
	eOhplGa7bExitMenu_None,
	eOhplGa7bExitMenu_ReturnToGame,
	eOhplGa7bExitMenu_Window,
};

enum eOhplGa7bExitFlag
{
	eOhplGa7bXm_ZoomSound = 1,
	eOhplGa7bXm_Achievement = 2,
};

enum eOhplGa7bOnExit
{
	eOhplGa7bOme_None,
	eOhplGa7bOme_DemoEnd,
	eOhplGa7bOme_EngineExit,
	eOhplGa7bOme_Return,
	eOhplGa7bOme_Start,
	eOhplGa7bOme_Continue,
	eOhplGa7bOme_ToMenu,
	eOhplGa7bOme_AndSave,
};

extern "C" {
void ohpl_ga7b_menu_new(OhplGa7bMenu *s);
void ohpl_ga7b_menu_enter(OhplGa7bMenu *s);
void ohpl_ga7b_menu_update_base(OhplGa7bMenu *s, float dt, const OhplGa7bTimes *t, bool hasCamera, OhplGa7bBase *out);
bool ohpl_ga7b_menu_update_top(OhplGa7bMenu *s, float dt, float fadeIn, float fadeOut, int saveFocus);
int ohpl_ga7b_menu_win_begin(OhplGa7bMenu *s, int win);
bool ohpl_ga7b_menu_win_commit(OhplGa7bMenu *s, int win);
bool ohpl_ga7b_menu_draw_focus(int win);
int ohpl_ga7b_menu_exit_pressed(OhplGa7bMenu *s, bool mapLoaded);
int ohpl_ga7b_menu_exit_menu(OhplGa7bMenu *s, int msg);
int ohpl_ga7b_menu_on_exit(int msg, bool demoOnAllExit);
int ohpl_ga7b_menu_top_press(int val);
size_t ohpl_ga7b_menu_plan(uint32_t flags, int *out, size_t cap);
void ohpl_ga7b_menu_font_fit(const float *lengths, const int *chars, size_t n, float screenX, float fontX, float fontY, float *out);
bool ohpl_ga7b_menu_subst_hash(const uint32_t *text, size_t n, int64_t num, void (*sink)(void *, const uint32_t *, size_t), void *ctx);
int ohpl_ga7b_menu_pay_save(int tinderboxes);
bool ohpl_ga7b_popup_ok(int val);

}

inline void OhplGa7bWideSink(void *apCtx, const uint32_t *apData, size_t alLen)
{
	((std::wstring *)apCtx)->assign((const wchar_t *)apData, alLen);
}

inline void OhplGa7bStrSink(void *apCtx, const unsigned char *apData, size_t alLen)
{
	((std::string *)apCtx)->assign((const char *)apData, alLen);
}

#endif
