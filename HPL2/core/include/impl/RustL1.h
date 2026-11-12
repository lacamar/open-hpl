#ifndef HPL_RUST_L1_H
#define HPL_RUST_L1_H

// C ABI of rust/src/backend/sdl (stage L1). Used when OHPL_IMPL=rust.

#include <cstddef>
#include <cstdint>

#include "SDL2/SDL_events.h"
#include "input/RustInput.h"

struct OhplL1Timer;
struct OhplL1Mutex;
struct OhplL1Thread;
struct OhplL1Log;
struct OhplL1Pad;

struct OhplL1Date {
	int seconds, minutes, hours, month_day, month, year, week_day, year_day;
};
struct OhplL1PadInfo {
	int32_t instance;
	size_t buttons;
	size_t axes;
};
static_assert(sizeof(OhplL1Date) == 32 && sizeof(OhplL1PadInfo) == 24, "L1 layouts");
enum { kOhplL1PollQuit = 1, kOhplL1PollPlugged = 2, kOhplL1PollRemoved = 4 };

extern "C" {

OhplL1Timer* ohpl_l1_timer_new();
void ohpl_l1_timer_free(OhplL1Timer*);
void ohpl_l1_timer_start(OhplL1Timer*);
void ohpl_l1_timer_stop(OhplL1Timer*);
double ohpl_l1_timer_micros(OhplL1Timer*);

OhplL1Mutex* ohpl_l1_mutex_new();
void ohpl_l1_mutex_free(OhplL1Mutex*);
bool ohpl_l1_mutex_lock(const OhplL1Mutex*);
bool ohpl_l1_mutex_unlock(const OhplL1Mutex*);

OhplL1Thread* ohpl_l1_thread_new();
void ohpl_l1_thread_free(OhplL1Thread*);
void ohpl_l1_thread_start(OhplL1Thread*, int (*fn)(void*), const char* name, void* data);
void ohpl_l1_thread_stop(OhplL1Thread*);
void ohpl_l1_thread_set_priority(int prio);

uint32_t ohpl_l1_ticks();
void ohpl_l1_delay(uint32_t ms);
void ohpl_l1_clipboard_set(const char* text);
bool ohpl_l1_clipboard_get(void (*sink)(void*, const unsigned char*, size_t), void* ctx);
void ohpl_l1_display_resolution(int display, int* w, int* h);
void ohpl_l1_video_modes(void (*cb)(void*, int, int, int, int), void* ctx);
bool ohpl_l1_display_name(int display, void (*sink)(void*, const unsigned char*, size_t), void* ctx);
void ohpl_l1_message_box(unsigned kind, const char* caption, const char* message);

uint64_t ohpl_l1_file_size(const char* path);
bool ohpl_l1_file_exists(const char* path);
bool ohpl_l1_folder_exists(const char* path);
void ohpl_l1_remove_file(const char* path);
bool ohpl_l1_create_folder(const char* path);
bool ohpl_l1_full_path(const char* path, void (*sink)(void*, const unsigned char*, size_t), void* ctx);
OhplL1Date ohpl_l1_file_date(const char* path, bool created);
OhplL1Date ohpl_l1_date_now();
bool ohpl_l1_working_dir(void (*sink)(void*, const unsigned char*, size_t), void* ctx);
void ohpl_l1_find_files(const char* dir, const uint32_t* mask, size_t mask_len, void (*sink)(void*, const uint32_t*, size_t), void* ctx);
void ohpl_l1_find_folders(const char* dir, bool add_hidden, bool add_up, void (*sink)(void*, const uint32_t*, size_t), void* ctx);

int ohpl_l1_main_args(int argc, const char* const* argv, void (*sink)(void*, const unsigned char*, size_t), void* ctx, bool* cwd);
bool ohpl_l1_protect_steam();
void ohpl_l1_sdl_hints();
bool ohpl_l1_sdl_init(bool video);
void ohpl_l1_sdl_error(void (*sink)(void*, const unsigned char*, size_t), void* ctx);
void ohpl_l1_sdl_shutdown();

OhplL1Log* ohpl_l1_log_open(const char* path);
void ohpl_l1_log_write(OhplL1Log*, const char* msg);
void ohpl_l1_log_close(OhplL1Log*);

uint32_t ohpl_l1_poll(bool (*filter)(void*, const SDL_Event*), void (*push)(void*, const SDL_Event*), void* ctx);
bool ohpl_l1_filter_keeps(bool (*filter)(void*, const SDL_Event*), void* ctx, const SDL_Event* ev);
void ohpl_l1_convert_events(const SDL_Event* ev, size_t n, OhplInputEvent* out);
void ohpl_l1_input_init();
int ohpl_l1_joystick_count();
int ohpl_l1_mod_state();
void ohpl_l1_mouse_flush();
void ohpl_l1_mouse_relative(int* x, int* y);
OhplL1Pad* ohpl_l1_pad_open(int index, OhplL1PadInfo* info, void (*sink)(void*, const unsigned char*, size_t), void* ctx);
void ohpl_l1_pad_free(OhplL1Pad*);
void ohpl_l1_pad_rumble(OhplL1Pad*, float value, int millisec);

}

#endif
