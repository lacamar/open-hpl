#ifndef LUX_RUST_MENU_A9_H
#define LUX_RUST_MENU_A9_H

// Main menu windows in Rust (rust/src/luxmenu/win/). Rust owns the window objects; OhplA9Host holds
// the host ops (LuxMenuHost.cpp) that wrap the C++ widgets and game singletons. Op numbers mirror
// rust/src/luxmenu/win/host.rs.

#include <cstddef>
#include <cstdint>

typedef void *OhplA9W;
typedef void (*OhplA9Sink)(void *apCtx, const uint32_t *apText, size_t alLen);

// Host ops handed to Rust once (same order as rust/src/luxmenu/win/host.rs `table!`).
struct OhplA9Host
{
	size_t (*ohpl_a9h_lux)(int alOp, size_t alArg);
	void (*ohpl_a9h_lux_str)(int alOp, OhplA9Sink apSink, void *apCtx);
	size_t (*ohpl_a9h_lux_with)(int alOp, const uint32_t *apText, size_t alLen);
	void (*ohpl_a9h_lux_with_str)(int alOp, const uint32_t *apText, size_t alLen, OhplA9Sink apSink, void *apCtx);
	void (*ohpl_a9h_plat_list)(int alKind, const uint32_t *apDir, size_t alDirLen, const uint32_t *apMask, size_t alMaskLen, OhplA9Sink apSink, void *apCtx);
	void (*ohpl_a9h_plat_date)(int alKind, const uint32_t *apPath, size_t alLen, int32_t *apOut);
	size_t (*ohpl_a9h_plat_folder)(int alOp, const uint32_t *apPath, size_t alLen);
	size_t (*ohpl_a9h_cfg)(int alOp, const uint8_t *apSection, size_t alSecLen, const uint8_t *apKey, size_t alKeyLen, float *apOut);
	void (*ohpl_a9h_tr)(const uint8_t *apCat, size_t alCatLen, const uint8_t *apKey, size_t alKeyLen, OhplA9Sink apSink, void *apCtx);
	OhplA9W (*ohpl_a9h_story_new)(const uint32_t *apPath, size_t alLen);
	void (*ohpl_a9h_story_str)(OhplA9W apStory, int alOp, OhplA9Sink apSink, void *apCtx);
	void (*ohpl_a9h_story_activate)(OhplA9W apStory);
	void (*ohpl_a9h_story_free)(OhplA9W apStory);
};

extern "C"
{
	void ohpl_a9_set_host(const OhplA9Host *apHost);
}

#endif
