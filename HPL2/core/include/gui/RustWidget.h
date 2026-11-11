#ifndef HPL_RUST_WIDGET_H
#define HPL_RUST_WIDGET_H

// C ABI of rust/src/gui/ffi_widget.rs. The widget classes stay C++; Rust decides, C++ applies.

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"
#include "gui/GuiTypes.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"
#include "graphics/FontData.h"
#include "system/String.h"

namespace hpl {

static_assert(eGuiMouseButton_Left == 1 && eGuiMouseButton_Right == 4 && eGuiMouseButton_WheelUp == 8 &&
			  eGuiMouseButton_WheelDown == 16, "mouse button values");
static_assert(eUIButton_Primary == 1, "ui button value");
static_assert(eKey_Escape == 5, "escape key value");
static_assert(eKeyModifier_None == 0, "key modifier value");
static_assert(eFontAlign_Left == 0 && eFontAlign_Right == 1 && eFontAlign_Center == 2, "font align values");

enum { kOhplWdGfxSlots = 11, kOhplWdGfxImage = 9, kOhplWdGfxLabel = 10, kOhplWdMaxOps = 16 };
enum { kOhplWdOpGfx = 0, kOhplWdOpFont = 1, kOhplWdOpOffset = 2 };
enum { kOhplWdHasSize = 1, kOhplWdHasColor = 2 };
enum { kOhplWdFontDefault = 0, kOhplWdFontWindowLabel = 1 };

struct OhplWdGfxMetrics { cVector2f active; cVector2f image; };

struct OhplWdOp {
	uint32_t kind, gfx, text, font, flags;
	int32_t align, text_len;
	cVector3f pos;
	cVector2f size;
	cColor color;
};

struct OhplWdParentPos { cVector3f global, children_offset, scroll; };

struct OhplWdWindowLook {
	cVector2f size, corner_lu, corner_ru;
	float label_font_height;
	cVector3f label_offset;
};
struct OhplWdWindowDraw {
	OhplWdWindowLook look;
	cVector3f global;
	bool draw_label, focused, has_background, borders_outside;
	cVector2f font_size;
	cColor font_color, color_mul;
};

struct OhplWdSliderLayout { cVector2f h_size; cVector3f h_pos; cVector2f v_size; cVector3f v_pos; };
struct OhplWdChildBox { cVector3f pos; cVector2f size; bool visible; };
struct OhplWdScrollIn {
	cVector2f size, margin;
	cVector3f offset, max_coord;
	cVector2f h_size, v_size;
	bool has_h, has_v;
};
struct OhplWdSliderPlan { int32_t max_value, bar_value_size, button_add; bool set_value; int32_t value; bool active; };
struct OhplWdScrollPlan { cVector3f max_coord; OhplWdSliderPlan h, v; };
struct OhplWdFrameDraw {
	cVector3f global;
	cVector2f size;
	bool draw_frame, draw_background;
	float background_z;
	cColor background_color;
};

struct OhplWdButtonState { bool toggleable, pressed, repeat_active; int32_t repeat_times; float timer, repeat_period; };
struct OhplWdButtonAction { OhplWdButtonState state; bool ret, fire, store_data; };
struct OhplWdButtonDraw {
	cVector3f global;
	cVector2f size;
	bool pressed, enabled, has_image, has_default_font;
	cVector3f pressed_offset;
	cVector2f font_size;
	cColor font_color, color_mul;
	bool has_background;
};

struct OhplWdCheckLayout { cVector2f box_size, size; };
struct OhplWdLabelScroll { float timer, offset; bool down; };
struct OhplWdLabelDraw {
	cVector3f global;
	cVector2f size;
	int32_t align;
	bool word_wrap;
	int32_t max_chars;
	bool enabled, has_default_font;
	cVector2f font_size;
	cColor font_color, color_mul;
	float wrap_offset;
	int32_t text_len;
};
struct OhplWdDummyChild { cVector3f pos; cVector2f size; };
struct OhplWdGroupDraw {
	cVector3f global;
	cVector2f size;
	cVector3f header_pos;
	cVector2f header_size;
	bool has_background, header_breaks_border;
};

static_assert(sizeof(OhplWdGfxMetrics) == 16 && sizeof(OhplWdOp) == 64, "op layout");
static_assert(sizeof(OhplWdParentPos) == 36 && sizeof(OhplWdWindowLook) == 40 && sizeof(OhplWdWindowDraw) == 96, "window layout");
static_assert(sizeof(OhplWdSliderLayout) == 40 && sizeof(OhplWdChildBox) == 24 && sizeof(OhplWdScrollIn) == 60, "frame layout");
static_assert(sizeof(OhplWdSliderPlan) == 24 && sizeof(OhplWdScrollPlan) == 60 && sizeof(OhplWdFrameDraw) == 44, "frame plan layout");
static_assert(sizeof(OhplWdButtonState) == 16 && sizeof(OhplWdButtonAction) == 20 && sizeof(OhplWdButtonDraw) == 80, "button layout");
static_assert(sizeof(OhplWdCheckLayout) == 16 && sizeof(OhplWdLabelScroll) == 12 && sizeof(OhplWdLabelDraw) == 84, "label layout");
static_assert(sizeof(OhplWdDummyChild) == 20 && sizeof(OhplWdGroupDraw) == 44, "group layout");

extern "C" {

void ohpl_wd_global_position(const OhplWdParentPos* parent, bool affectedByScroll, const cVector3f* local, cVector3f* out);
void ohpl_wd_global_to_local(const cVector3f* pos, const OhplWdParentPos* parent, cVector3f* out);
void ohpl_wd_center_in_set(const cVector2f* set, const cVector2f* size, float globalZ, cVector3f* out);
int ohpl_wd_point_stage(bool parentOk, bool onlyClipped, bool clips);
bool ohpl_wd_point_in_box(const cVector2f* point, const cVector3f* global, const cVector2f* size);
int ohpl_wd_ui_arrow_slot(int arrow);
void ohpl_wd_rebase(const cVector3f* local, const cVector3f* global, bool add, cVector3f* out);
void ohpl_wd_rel_to_mouse(const cVector3f* global, const cVector2f* mouse, cVector2f* out);
size_t ohpl_wd_borders(OhplWdOp* ops, size_t cap, bool hasBg, bool outside, const cVector3f* pos, const cVector2f* size,
					   const OhplWdGfxMetrics* m);

size_t ohpl_wd_window_draw(OhplWdOp* ops, size_t cap, const OhplWdWindowDraw* d, const OhplWdGfxMetrics* m);
bool ohpl_wd_window_mouse_down(const OhplWdWindowLook* look, const cVector3f* global, const cVector2f* mouse, int val);
void ohpl_wd_window_drag_pos(const cVector3f* rel, const cVector2f* mouse, cVector3f* out);
bool ohpl_wd_window_key_closes(bool focused, bool closeOnEscape, int key, int modifier);
bool ohpl_wd_window_focus_lost(bool noFocusedWidget, bool connectedDown, bool connectedUp);
void ohpl_wd_window_menu_pos(const OhplWdWindowLook* look, cVector3f* out);
void ohpl_wd_window_button_pos(float width, float buttonSize, cVector3f* out);

void ohpl_wd_frame_slider_layout(const cVector2f* size, bool hVisible, bool vVisible, OhplWdSliderLayout* out);
void ohpl_wd_frame_scroll_plan(const OhplWdScrollIn* in, const OhplWdChildBox* children, size_t n, OhplWdScrollPlan* out);
bool ohpl_wd_frame_wheel(int val, int value, int buttonAdd, int* out);
size_t ohpl_wd_frame_draw(OhplWdOp* ops, size_t cap, const OhplWdFrameDraw* d, const OhplWdGfxMetrics* m);
size_t ohpl_wd_frame_draw_background(OhplWdOp* ops, size_t cap, const OhplWdFrameDraw* d, const OhplWdGfxMetrics* m);

float ohpl_wd_button_repeat_period(float freq);
void ohpl_wd_button_mouse_down(const OhplWdButtonState* s, bool enabled, int val, OhplWdButtonAction* out);
void ohpl_wd_button_mouse_up(const OhplWdButtonState* s, bool enabled, int val, bool mouseOver, OhplWdButtonAction* out);
bool ohpl_wd_button_release(bool toggleable, bool pressed);
void ohpl_wd_button_update(const OhplWdButtonState* s, float dt, OhplWdButtonAction* out);
bool ohpl_wd_ui_primary(int val);
size_t ohpl_wd_button_draw(OhplWdOp* ops, size_t cap, const OhplWdButtonDraw* d, const OhplWdGfxMetrics* m);

void ohpl_wd_check_layout(float boxWidth, float fontHeight, float labelPosX, const cVector2f* labelSize, OhplWdCheckLayout* out);
void ohpl_wd_check_label_pos(float boxWidth, float boxHeight, float fontHeight, cVector3f* out);
void ohpl_wd_check_box_pos(const cVector3f* global, float boxHeight, float activeHeight, cVector3f* out);
bool ohpl_wd_check_mouse_down(int val);
bool ohpl_wd_check_label_forward(int val, bool enabled, bool* forward);

void ohpl_wd_label_update(const OhplWdLabelScroll* s, float dt, float speedMul, float height, float rowsHeight, OhplWdLabelScroll* out);
size_t ohpl_wd_label_draw_text(OhplWdOp* ops, size_t cap, const OhplWdLabelDraw* d, const int* rows, size_t nRows,
							   float* rowsHeight, bool* rowsHeightSet);
void ohpl_wd_label_background(const cVector3f* global, const cVector2f* size, cVector3f* outPos, cVector2f* outSize);

bool ohpl_wd_image_mouse(int val);
void ohpl_wd_dummy_enclose(const OhplWdDummyChild* children, size_t n, cVector2f* out);
void ohpl_wd_group_header_pos(float headerOffset, float fontHeight, cVector3f* out);
size_t ohpl_wd_group_draw(OhplWdOp* ops, size_t cap, const OhplWdGroupDraw* d, const OhplWdGfxMetrics* m);

int ohpl_wd_file_type(const wchar_t* ext, size_t len);
uint64_t ohpl_wd_human_size(uint64_t size, size_t* unit);
size_t ohpl_wd_drive_letters(uint64_t mask, wchar_t* out, size_t cap);
int ohpl_wd_navigate_target(const wchar_t* path, size_t len);
int ohpl_wd_path_piece(const wchar_t* piece, size_t len, size_t depth);

}

// ---------------------------------------------------------------- glue

struct OhplWdParent {
	OhplWdParentPos pos;
	bool has;
};

struct OhplWdGfxTable {
	cGuiGfxElement* gfx[kOhplWdGfxSlots];
	OhplWdGfxMetrics m[kOhplWdGfxSlots];

	OhplWdGfxTable()
	{
		for(int i=0; i<kOhplWdGfxSlots; ++i) { gfx[i] = NULL; m[i].active = cVector2f(0); m[i].image = cVector2f(0); }
	}

	void Set(int slot, cGuiGfxElement* apGfx)
	{
		gfx[slot] = apGfx;
		if(apGfx)
		{
			m[slot].active = apGfx->GetActiveSize();
			m[slot].image = apGfx->GetImageSize();
		}
	}

	void SetFrame(cGuiGfxElement* apBackground, cGuiGfxElement* const* apBorders, cGuiGfxElement* const* apCorners)
	{
		Set(0, apBackground);
		for(int i=0; i<4; ++i) { Set(1+i, apBorders[i]); Set(5+i, apCorners[i]); }
	}
};

inline void OhplWdRun(cGuiSet* apSet, const OhplWdOp* apOps, size_t alCount, const OhplWdGfxTable& aTable,
					  const tWString* const* apTexts, iFontData* const* apFonts)
{
	for(size_t i=0; i<alCount; ++i)
	{
		const OhplWdOp& op = apOps[i];
		switch(op.kind)
		{
		case kOhplWdOpOffset:
			apSet->SetDrawOffset(op.pos);
			break;
		case kOhplWdOpGfx:
			if(op.flags & kOhplWdHasColor)
				apSet->DrawGfx(aTable.gfx[op.gfx], op.pos, (op.flags & kOhplWdHasSize) ? op.size : cVector2f(-1), op.color);
			else if(op.flags & kOhplWdHasSize)
				apSet->DrawGfx(aTable.gfx[op.gfx], op.pos, op.size);
			else
				apSet->DrawGfx(aTable.gfx[op.gfx], op.pos);
			break;
		case kOhplWdOpFont:
		{
			const tWString& sText = *apTexts[op.text];
			if(op.text_len >= 0)
				apSet->DrawFont(cString::SubW(sText, 0, op.text_len), apFonts[op.font], op.pos, op.size, op.color, (eFontAlign)op.align);
			else
				apSet->DrawFont(sText, apFonts[op.font], op.pos, op.size, op.color, (eFontAlign)op.align);
			break;
		}
		}
	}
}

}

#endif // HPL_RUST_WIDGET_H
