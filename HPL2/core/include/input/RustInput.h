#ifndef HPL_RUST_INPUT_H
#define HPL_RUST_INPUT_H

// C ABI of rust/src/input_ffi.rs; layouts mirror rust/src/input.rs.

#include <cstddef>
#include <cstdint>
#include <string>

#include "input/InputTypes.h"

struct OhplInputEvent {
	uint32_t kind;
	int32_t sym;
	int32_t key_mod;
	uint8_t state;
	uint8_t button;
	int16_t axis_value;
	int32_t x, y, xrel, yrel, wheel_y, which;
	uint8_t text[32];
};
static_assert(sizeof(OhplInputEvent) == 72 && offsetof(OhplInputEvent, x) == 16 && offsetof(OhplInputEvent, which) == 36
	&& offsetof(OhplInputEvent, text) == 40, "OhplInputEvent layout");

enum {
	eOhplInputKind_Key = 1, eOhplInputKind_Text, eOhplInputKind_MouseMotion, eOhplInputKind_MouseButton,
	eOhplInputKind_Wheel, eOhplInputKind_PadAxis, eOhplInputKind_PadButton, eOhplInputKind_PadRemoved
};

struct OhplVec2l { int32_t x, y; };

struct OhplActionState {
	bool becameTriggered, isTriggered, doubleDown, doubleTriggered, isDown;
	double timeCount;
};
static_assert(sizeof(OhplActionState) == 16 && offsetof(OhplActionState, timeCount) == 8, "OhplActionState layout");

static_assert(sizeof(hpl::cKeyPress) == 12 && offsetof(hpl::cKeyPress, mlUnicode) == 4 && offsetof(hpl::cKeyPress, mlModifier) == 8,
	"cKeyPress layout");
static_assert(sizeof(hpl::cGamepadInputData) == 16 && offsetof(hpl::cGamepadInputData, mInputType) == 4
	&& offsetof(hpl::cGamepadInputData, mlInputId) == 8 && offsetof(hpl::cGamepadInputData, mfInputValue) == 12,
	"cGamepadInputData layout");
static_assert(hpl::eGamepadInputType_Button == 0 && hpl::eGamepadInputType_Axis == 2 && hpl::eGamepadInputType_LastEnum == 4,
	"input types cross the ABI");

struct OhplSubSpec {
	uint32_t kind;
	int32_t pad, id, range;
	float min, max;
};
static_assert(sizeof(OhplSubSpec) == 24, "OhplSubSpec layout");
enum { eOhplSub_Key = 1, eOhplSub_Mouse, eOhplSub_PadButton, eOhplSub_PadAxis, eOhplSub_PadHat };
static_assert(hpl::eKey_LastEnum == 230 && hpl::eMouseButton_LastEnum == 9, "registry device bounds");

struct OhplInputReg;
struct OhplKeyboard;
struct OhplMouse;
struct OhplGamepad;

extern "C" {
	OhplKeyboard* ohpl_input_kb_new();
	void ohpl_input_kb_free(OhplKeyboard*);
	void ohpl_input_kb_update(OhplKeyboard*, const OhplInputEvent*, size_t, int mod_state);
	bool ohpl_input_kb_key_is_down(OhplKeyboard*, int key);
	bool ohpl_input_kb_key_is_pressed(OhplKeyboard*);
	bool ohpl_input_kb_key_is_released(OhplKeyboard*);
	bool ohpl_input_kb_pop_pressed(OhplKeyboard*, hpl::cKeyPress* out);
	bool ohpl_input_kb_pop_released(OhplKeyboard*, hpl::cKeyPress* out);

	OhplMouse* ohpl_input_mouse_new();
	void ohpl_input_mouse_free(OhplMouse*);
	bool ohpl_input_mouse_take_first_time(OhplMouse*);
	void ohpl_input_mouse_update(OhplMouse*, const OhplInputEvent*, size_t, bool headless, OhplVec2l rel_state);
	bool ohpl_input_mouse_button_is_down(OhplMouse*, int button);
	OhplVec2l ohpl_input_mouse_abs(OhplMouse*);
	OhplVec2l ohpl_input_mouse_take_rel(OhplMouse*);

	OhplGamepad* ohpl_input_pad_new(int index, int instance, size_t buttons, size_t axes, int flush_threshold);
	void ohpl_input_pad_free(OhplGamepad*);
	size_t ohpl_input_pad_num_buttons(OhplGamepad*);
	size_t ohpl_input_pad_num_axes(OhplGamepad*);
	void ohpl_input_pad_clear_state(OhplGamepad*);
	float ohpl_input_pad_time_since_active(OhplGamepad*, unsigned long now);
	void ohpl_input_pad_update(OhplGamepad*, const OhplInputEvent*, size_t, unsigned long now);
	bool ohpl_input_pad_has_updates(OhplGamepad*);
	bool ohpl_input_pad_button_is_pressed(OhplGamepad*);
	bool ohpl_input_pad_button_is_released(OhplGamepad*);
	bool ohpl_input_pad_axes_updated(OhplGamepad*);
	bool ohpl_input_pad_pop_update(OhplGamepad*, hpl::cGamepadInputData* out);
	bool ohpl_input_pad_pop_button(OhplGamepad*, hpl::cGamepadInputData* out);
	bool ohpl_input_pad_pop_released(OhplGamepad*, hpl::cGamepadInputData* out);
	bool ohpl_input_pad_pop_axis(OhplGamepad*, hpl::cGamepadInputData* out);
	bool ohpl_input_pad_button_is_down(OhplGamepad*, int button);
	float ohpl_input_pad_axis_value(OhplGamepad*, int axis);
	float ohpl_input_pad_dead_zone();
	void ohpl_input_pad_set_dead_zone(float);

	OhplActionState ohpl_input_action_new();
	bool ohpl_input_action_is_down(OhplActionState*);
	bool ohpl_input_action_was_triggered(OhplActionState*);
	bool ohpl_input_action_became_triggered(OhplActionState*);
	bool ohpl_input_action_double_triggered(OhplActionState*, float limit);
	void ohpl_input_action_update(OhplActionState*, bool any_down, float time_step);
	void ohpl_input_action_reset(OhplActionState*, bool any_down);

	OhplInputReg* ohpl_input_reg_new();
	void ohpl_input_reg_free(OhplInputReg*);
	void ohpl_input_reg_set_devices(OhplInputReg*, const OhplKeyboard*, const OhplMouse*);
	void ohpl_input_reg_set_pads(OhplInputReg*, const OhplGamepad* const*, size_t);
	uint64_t ohpl_input_reg_create(OhplInputReg*, const unsigned char* name, size_t len, int id, void* user);
	void ohpl_input_reg_destroy(OhplInputReg*, uint64_t);
	uint64_t ohpl_input_reg_find_name(OhplInputReg*, const unsigned char* name, size_t len);
	uint64_t ohpl_input_reg_find_id(OhplInputReg*, int id);
	void* ohpl_input_reg_user(OhplInputReg*, uint64_t);
	void ohpl_input_reg_add_sub(OhplInputReg*, uint64_t, const OhplSubSpec*);
	void ohpl_input_reg_clear_subs(OhplInputReg*, uint64_t);
	bool ohpl_input_reg_is_down(OhplInputReg*, uint64_t);
	bool ohpl_input_reg_was_triggered(OhplInputReg*, uint64_t);
	bool ohpl_input_reg_became_triggered(OhplInputReg*, uint64_t);
	bool ohpl_input_reg_double_triggered(OhplInputReg*, uint64_t, float limit);
	void ohpl_input_reg_reset(OhplInputReg*, uint64_t);
	void ohpl_input_reg_update(OhplInputReg*, float timeStep);
	void ohpl_input_reg_update_action(OhplInputReg*, uint64_t, float timeStep);
	void ohpl_input_reg_reset_all(OhplInputReg*);
	void* ohpl_input_reg_pop_registered(OhplInputReg*);
	bool ohpl_input_reg_first_down(OhplInputReg*, uint32_t* kind, int32_t* id);

	typedef void (*OhplInputSink)(void*, const unsigned char*, size_t);
	void ohpl_input_key_name(int, OhplInputSink, void*);
	void ohpl_input_modifier_name(int, OhplInputSink, void*);
	void ohpl_input_mouse_button_name(int, OhplInputSink, void*);
	void ohpl_input_pad_button_name(int, OhplInputSink, void*);
	void ohpl_input_pad_axis_name(int, OhplInputSink, void*);
	void ohpl_input_pad_axis_range_name(int, OhplInputSink, void*);
	void ohpl_input_pad_hat_name(int, OhplInputSink, void*);
	void ohpl_input_pad_hat_state_name(int, OhplInputSink, void*);
	void ohpl_input_pad_ball_name(int, OhplInputSink, void*);
	int ohpl_input_key_from_name(const unsigned char*, size_t);
	int ohpl_input_mouse_button_from_name(const unsigned char*, size_t);
	int ohpl_input_pad_button_from_name(const unsigned char*, size_t);
	int ohpl_input_pad_axis_from_name(const unsigned char*, size_t);
	int ohpl_input_pad_axis_range_from_name(const unsigned char*, size_t);
	int ohpl_input_pad_hat_from_name(const unsigned char*, size_t);
	int ohpl_input_pad_hat_state_from_name(const unsigned char*, size_t);
	int ohpl_input_pad_ball_from_name(const unsigned char*, size_t);
	int ohpl_input_pad_axis_range_from_value(float);
}

namespace hpl {
	inline void OhplInputAssign(void* apCtx, const unsigned char* apData, size_t alLen)
	{
		static_cast<std::string*>(apCtx)->assign(reinterpret_cast<const char*>(apData), alLen);
	}

	template<class F> std::string OhplInputName(F aFn, int alValue)
	{
		std::string sOut;
		aFn(alValue, OhplInputAssign, &sOut);
		return sOut;
	}

	inline int OhplInputFromName(int (*aFn)(const unsigned char*, size_t), const std::string& asName)
	{
		return aFn(reinterpret_cast<const unsigned char*>(asName.data()), asName.size());
	}
}

#endif
