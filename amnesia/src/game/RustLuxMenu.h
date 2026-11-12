#ifndef LUX_RUST_MENU_H
#define LUX_RUST_MENU_H

// C ABI of rust/src/luxmenu/ffi.rs: option tables and conversions, apply decisions, key rebinding
// and conflict rules, profile list rules of the options, key config and profile windows.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct OhplGa7aSub
{
	int kind, gtype, id;
	float value;
};
static_assert(sizeof(OhplGa7aSub) == 16, "OhplGa7aSub layout");

struct OhplGa7aStr
{
	const unsigned char *p;
	size_t n;
};

struct OhplGa7aWStr
{
	const unsigned *p;
	size_t n;
};

struct OhplGa7aOp
{
	int kind, a, b;
};

struct OhplGa7aDate
{
	int seconds, minutes, hours, month_day, month, year;
};

struct OhplGa7aMode
{
	int display, width, height, refresh;
};

typedef void (*OhplGa7aSink8)(void*, const unsigned char*, size_t);
typedef void (*OhplGa7aSink32)(void*, const unsigned*, size_t);
typedef void (*OhplGa7aSetColor)(void*, size_t, int, int);

inline void OhplGa7aStrSink(void *apCtx, const unsigned char *apData, size_t alLen)
{
	((std::string*)apCtx)->assign((const char*)apData, alLen);
}

inline void OhplGa7aWideSink(void *apCtx, const unsigned *apData, size_t alLen)
{
	((std::wstring*)apCtx)->assign((const wchar_t*)apData, alLen);
}

extern "C"
{
// profile
void ohpl_ga7a_pf_sort(const OhplGa7aDate *apDates, size_t alNum, unsigned *apOut);
void ohpl_ga7a_pf_trim(const unsigned *apName, size_t alLen, size_t *apStart, size_t *apEnd);
const unsigned *ohpl_ga7a_pf_illegal(size_t *apLen);
int ohpl_ga7a_pf_name_max();
int ohpl_ga7a_pf_select(int alNum, size_t alCount);
int ohpl_ga7a_pf_delete_plan(int alSelected, int abInUse, int abConfigDiffers, int *apNewSelection);
int ohpl_ga7a_pf_activate(int alItemNum);
int ohpl_ga7a_pf_ui_press(int abSecondary, int abListLocked, int abNameVisible);

// key config
int ohpl_ga7a_kc_key_pos(const OhplGa7aSub *apSubs, size_t alNum, int alKey);
int ohpl_ga7a_kc_mouse_pos(const OhplGa7aSub *apSubs, size_t alNum, int alButton);
int ohpl_ga7a_kc_pad_button_pos(const OhplGa7aSub *apSubs, size_t alNum, int alButton);
int ohpl_ga7a_kc_pad_hat_pos(const OhplGa7aSub *apSubs, size_t alNum, int alHat, int alState);
int ohpl_ga7a_kc_pad_axis_pos(const OhplGa7aSub *apSubs, size_t alNum, int alAxis, int alRange);
int ohpl_ga7a_kc_has_sub_actions(const OhplGa7aSub *apSubs, size_t alNum);
size_t ohpl_ga7a_kc_sharing(const OhplGa7aSub *apA, size_t alA, const OhplGa7aSub *apB, size_t alB, int *apOut);
void ohpl_ga7a_kc_set(OhplGa7aSub *apSubs, size_t alNum, int alPos, int alWhat, int alA, int alB);
void ohpl_ga7a_kc_from_string(const unsigned char *apStr, size_t alLen, int alValue, OhplGa7aSub *apOut);
void ohpl_ga7a_kc_label(const OhplGa7aSub *apSub, OhplGa7aSink8 apSink, void *apCtx);
void ohpl_ga7a_kc_fill_default(const OhplGa7aStr *apNames, const int *apValues, size_t alNum, OhplGa7aSub *apSubs, size_t alSubs, int abHasMacKey, int alMacKey);
void ohpl_ga7a_kc_fill_initial(const OhplGa7aStr *apNames, size_t alNum, OhplGa7aSub *apSubs, size_t alSubs);
int ohpl_ga7a_kc_update(const OhplGa7aSub *apSubs, size_t alNum, int abChanged, OhplGa7aOp *apOut, size_t alCap);
int ohpl_ga7a_kc_check(const OhplGa7aSub *apSubs, size_t alSubs, const size_t *apCounts, size_t alCategories, OhplGa7aSetColor apSet, void *apCtx);
int ohpl_ga7a_kc_binds_pad(const unsigned char *apName, size_t alLen);
int ohpl_ga7a_kc_key_press(int abWaiting, int alKey, int alEscape);
int ohpl_ga7a_kc_pad_press(int abWaiting, float afValue, int abIsButton, int alId, int alCancelButton);
int ohpl_ga7a_kc_ui_button(int abIsPrimary, int abFlag, int abNobodyWaiting);
int ohpl_ga7a_kc_ui_arrow(int abFlag);
int ohpl_ga7a_kc_click(int abNobodyWaiting, int abIsLeft);
int ohpl_ga7a_kc_set_waiting(int abHasNew, int abHasCurrent);
int ohpl_ga7a_kc_tip_flags(int abTipEmpty);
size_t ohpl_ga7a_kc_tip_fade(float afAlpha, int *apFlags, float afDt, float *apAlphas);
int ohpl_ga7a_kc_category_active(int alSelected, int alCategory);
int ohpl_ga7a_kc_category_button(int abWasWaiting, int alButton, int alPrev, int alNext, int alCategory, int alItems);
int ohpl_ga7a_kc_exit(int abPopupActive, int abCancelled);
int ohpl_ga7a_kc_ui_cancel(int abWasWaiting, int abIsSecondary);

// options
int ohpl_ga7a_op_aniso_index(float afX);
float ohpl_ga7a_op_aniso_from_index(int alX);
int ohpl_ga7a_op_ssao_index(int alSamples);
int ohpl_ga7a_op_ssao_from_index(int alX);
void ohpl_ga7a_op_range(int alKind, float *apMin, float *apStep, float *apMax);
void ohpl_ga7a_op_slider_setup(float afMin, float afMax, float afStep, int *apMaxValue, int *apBarSize);
int ohpl_ga7a_op_slider_to_int(float afValue, float afMin, float afMax, int alSliderMax);
float ohpl_ga7a_op_slider_to_value(int alValue, int alSliderMax, float afMin, float afMax);
void ohpl_ga7a_op_slider_label(float afValue, float afMin, float afMax, OhplGa7aSink32 apSink, void *apCtx);
void ohpl_ga7a_op_int_label(float afValue, OhplGa7aSink32 apSink, void *apCtx);
const char *ohpl_ga7a_op_renderer_name(int alX);
int ohpl_ga7a_op_renderer_index(const unsigned char *apName, size_t alLen);
const char *ohpl_ga7a_op_color_name(int alX);
int ohpl_ga7a_op_color_index(const unsigned char *apName, size_t alLen);
int ohpl_ga7a_op_gui_scale_index(int alScale, int alItems);
int ohpl_ga7a_op_flip_index(int alX, int alItems);
int ohpl_ga7a_op_language_index(const unsigned char *apLang, size_t alLen, const OhplGa7aWStr *apFiles, size_t alNum);
int ohpl_ga7a_op_sound_device_index(int alId, const int *apIds, size_t alNum, int alItems);
int ohpl_ga7a_op_multi_display(const OhplGa7aMode *apModes, size_t alNum);
size_t ohpl_ga7a_op_dedup_modes(const OhplGa7aMode *apModes, size_t alNum, unsigned *apSrc, int *apRefresh);
int ohpl_ga7a_op_current_mode(const OhplGa7aMode *apModes, size_t alNum, int alDisplay, int alWidth, int alHeight);
void ohpl_ga7a_op_resolution_label(int alWidth, int alHeight, int abCustom, OhplGa7aSink32 apSink, void *apCtx);
int ohpl_ga7a_op_needs_restart(int abSettingInitial, int abHasData, int abDataNeedsRestart);
int ohpl_ga7a_op_activate(int abKeyConfigOpen, int abSettingLanguage, int abInitialSet);
int ohpl_ga7a_op_toggle_gfx(int abBasicVisible);
int ohpl_ga7a_op_tip_step(int *apFlags, float afAlpha, float afDt, float *apDelta);
}

#endif // LUX_RUST_MENU_H
