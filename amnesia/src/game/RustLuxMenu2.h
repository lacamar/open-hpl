#ifndef LUX_RUST_MENU2_H
#define LUX_RUST_MENU2_H

// C ABI of rust/src/luxmenu2/ffi.rs: main menu fades and exit flow, window switching, top menu
// plan, start/load/custom story rules, premenu sequencing, credits, demo end and load screen
// timing. The state structs live in the C++ objects; Rust never calls back.

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

struct OhplGa7bPre
{
	float mfAlpha;
	float mfTimer;
	int mlSection;
	int mlState;
	bool mbExit;
};

struct OhplGa7bStep
{
	int mlAct;
	int mlSection;
};

struct OhplGa7bRow
{
	uint32_t mlLen;
	uint32_t mlFirst;
};

struct OhplGa7bCredits
{
	float mfY;
	float mfTime;
	float mfFadeAlpha;
	int mlState;
	int mlEndNum;
};

struct OhplGa7bDrawRow
{
	uint32_t mlRow;
	bool mbHeader;
	float mfY;
	float mfAlpha;
};

struct OhplGa7bDemoEnd
{
	float mfFadeAlpha;
	float mfTime;
	bool mbExiting;
	bool mbWebsite;
};

struct OhplGa7bLoad
{
	float mfAlpha;
	float mfLoadingAlpha;
	float mfExtraTime;
	int mlState;
	int mlRandNum;
	int mlPrevBits;
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

enum eOhplGa7bPreAct
{
	eOhplGa7bPreAct_None,
	eOhplGa7bPreAct_SetTimer,
	eOhplGa7bPreAct_Final,
	eOhplGa7bPreAct_Section,
	eOhplGa7bPreAct_CheckShow,
};

enum eOhplGa7bPreState
{
	eOhplGa7bPre_Initial,
	eOhplGa7bPre_FadeIn,
	eOhplGa7bPre_FadeOut,
	eOhplGa7bPre_FastFadeOut,
	eOhplGa7bPre_Show,
	eOhplGa7bPre_Final,
};

enum eOhplGa7bCreditsUpdate
{
	eOhplGa7bCr_StopMusic = 1,
	eOhplGa7bCr_Finish = 2,
};

enum eOhplGa7bDemoEndUpdate
{
	eOhplGa7bDe_Exit = 1,
	eOhplGa7bDe_Browser = 2,
};

enum eOhplGa7bStoryFallback
{
	eOhplGa7bFb_Name = 1,
	eOhplGa7bFb_Author = 2,
	eOhplGa7bFb_Desc = 4,
};

enum eOhplGa7bUi
{
	eOhplGa7bUi_Primary = 1,
	eOhplGa7bUi_Secondary = 2,
};

enum eOhplGa7bFolder
{
	eOhplGa7bFolder_Custom = 1,
	eOhplGa7bFolder_Profile = 2,
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

int ohpl_ga7b_pg_exit_target(bool customStoryActive, bool ok);
int ohpl_ga7b_pg_ui_switch(int val);
int ohpl_ga7b_pg_mode_gold(bool hard);
void ohpl_ga7b_pg_sort_saves(const void *dates, size_t n, uint32_t *out);
int ohpl_ga7b_pg_load_index(int selected, size_t count);
int ohpl_ga7b_pg_press_ok(int selected);
float ohpl_ga7b_pg_button_width(const float *lengths, size_t n);
void ohpl_ga7b_pg_story_buttons(bool saveExists, float startY, const float *heights, size_t n, bool *active, float *y);
uint32_t ohpl_ga7b_pg_story_fallbacks(bool nameEmpty, bool authorEmpty, bool descEmpty);
uint32_t ohpl_ga7b_pg_start_folders(bool customExists, bool profileExists);

void ohpl_ga7b_pm_new(OhplGa7bPre *s);
void ohpl_ga7b_pm_enter(OhplGa7bPre *s);
void ohpl_ga7b_pm_update_actions(OhplGa7bPre *s, float dt);
void ohpl_ga7b_pm_update_state(OhplGa7bPre *s, bool firstStart, const bool *firstOnly, size_t n, OhplGa7bStep *out);
void ohpl_ga7b_pm_check_show(OhplGa7bPre *s, bool hasText, bool hasGamma);
void ohpl_ga7b_pm_button_pressed(OhplGa7bPre *s, bool hasSection, bool allowSkipping);
float ohpl_ga7b_pm_music_fade_speed(float fadeTime);
void ohpl_ga7b_pm_gamma_slider(int *out);
int ohpl_ga7b_pm_gamma_to_slider(float gamma, int maxValue);
float ohpl_ga7b_pm_slider_to_gamma(int value, int maxValue);
int ohpl_ga7b_pm_gamma_arrow(int arrow);
void ohpl_ga7b_pm_background_rect(float w, float h, float *out);
int ohpl_ga7b_pm_parse_align(const unsigned char *name, size_t n);

void ohpl_ga7b_cr_new(OhplGa7bCredits *s);
void ohpl_ga7b_cr_reset(OhplGa7bCredits *s);
uint32_t ohpl_ga7b_cr_update(OhplGa7bCredits *s, float dt, const OhplGa7bRow *rows, size_t n, float scrollSpeed, float fadeSpeed);
bool ohpl_ga7b_cr_exit_pressed(OhplGa7bCredits *s);
size_t ohpl_ga7b_cr_draw_rows(float y0, const OhplGa7bRow *rows, size_t n, float addNormal, float addHeader, OhplGa7bDrawRow *out, size_t cap);
int ohpl_ga7b_cr_end_code(int endNum, bool ptest);

void ohpl_ga7b_de_new(OhplGa7bDemoEnd *s);
void ohpl_ga7b_de_reset(OhplGa7bDemoEnd *s);
uint32_t ohpl_ga7b_de_update(OhplGa7bDemoEnd *s, float dt);
void ohpl_ga7b_de_exit(OhplGa7bDemoEnd *s, bool website);
void ohpl_ga7b_de_button_color(float *fade, bool hover, float dt, const float *base, float *out);
void ohpl_ga7b_de_message_rows(size_t count, float textY, float fontH, float *out);

void ohpl_ga7b_ls_new(OhplGa7bLoad *s);
void ohpl_ga7b_ls_reset(OhplGa7bLoad *s);
void ohpl_ga7b_ls_setup(OhplGa7bLoad *s, bool same, int randNum);
void ohpl_ga7b_ls_load_done(OhplGa7bLoad *s, bool hasWanted, float wanted, float loadTime);
void ohpl_ga7b_ls_exit_pressed(OhplGa7bLoad *s);
bool ohpl_ga7b_ls_update_game(OhplGa7bLoad *s, float dt, float fadeOutTime);
void ohpl_ga7b_ls_text_suffix(OhplGa7bLoad *s, void (*sink)(void *, const unsigned char *, size_t), void *ctx);
float ohpl_ga7b_ls_text_y(float loadingAlpha, float withImage, float alone);
float ohpl_ga7b_ls_centered(float size, float center);
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
