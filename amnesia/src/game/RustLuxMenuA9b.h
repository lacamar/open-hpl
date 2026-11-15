#ifndef LUX_RUST_MENU_A9B_H
#define LUX_RUST_MENU_A9B_H

// C ABI of rust/src/luxmenu/mainmenu/ffi.rs: the main menu lives in Rust, cLuxMainMenu holds its handle.

#include <cstddef>
#include <cstdint>

struct OhplA9bState
{
	size_t mpSet;
	size_t mpFirstLabel;
	size_t mpCamera;
	size_t mpWorld;
	size_t mpLogo;
	float mfTopAlpha;
	float mfFadeAlpha;
	float mvLogoSize[2];
	int mlWindow;
	int mlTopVisible;
};

extern "C"
{
	uint64_t ohpl_a9b_new();
	void ohpl_a9b_free(uint64_t alMe);
	void ohpl_a9b_load_fonts(uint64_t alMe);
	void ohpl_a9b_create_gui(uint64_t alMe);
	void ohpl_a9b_reset(uint64_t alMe);
	void ohpl_a9b_on_quit(uint64_t alMe);
	void ohpl_a9b_enter(uint64_t alMe);
	void ohpl_a9b_leave(uint64_t alMe);
	void ohpl_a9b_update(uint64_t alMe, float afTimeStep);
	void ohpl_a9b_draw(uint64_t alMe);
	void ohpl_a9b_set_window(uint64_t alMe, int alWindow);
	void ohpl_a9b_exit_pressed(uint64_t alMe);
	void ohpl_a9b_focus(uint64_t alMe, int alGot);
	void ohpl_a9b_device_changed(uint64_t alMe);
	size_t ohpl_a9b_set(uint64_t alMe);
	void ohpl_a9b_state(uint64_t alMe, OhplA9bState *apOut);
	size_t ohpl_a9b_dump_window(uint64_t alMe, int alWindow, int alBegin);
	void ohpl_a9b_dump_background(uint64_t alMe);
	void ohpl_a9b_dump_end(uint64_t alMe);
}

#endif
