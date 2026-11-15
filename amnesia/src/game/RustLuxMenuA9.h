#ifndef LUX_RUST_MENU_A9_H
#define LUX_RUST_MENU_A9_H

// Main menu windows in Rust (rust/src/luxmenu/win/). Rust owns the window objects; OhplA9Host holds
// the host ops (LuxMenuHost.cpp) that wrap the C++ widgets and game singletons. Op numbers mirror
// rust/src/luxmenu/win/host.rs.

#include <cstddef>
#include <cstdint>

typedef void *OhplA9W;
typedef void (*OhplA9Sink)(void *apCtx, const uint32_t *apText, size_t alLen);

struct OhplA9Str
{
	const uint32_t *mpText;
	size_t mlLen;
};

static_assert(sizeof(OhplA9Str) == 2 * sizeof(size_t), "OhplA9Str layout");

// Host ops handed to Rust once (same order as rust/src/luxmenu/win/host.rs `table!`).
struct OhplA9Host
{
	OhplA9W (*ohpl_a9h_new)(OhplA9W apSet, int alKind, OhplA9W apParent, const float *apPos, const float *apSize, int alFlags, const uint32_t *apText, size_t alLen);
	OhplA9W (*ohpl_a9h_popup)(OhplA9W apSet, const OhplA9Str *apStr, size_t alObj, int alKillOnEscape, bool abCopyFocus);
	size_t (*ohpl_a9h_set)(OhplA9W apSet, int alOp, OhplA9W apWidget, size_t alArg);
	size_t (*ohpl_a9h_w)(OhplA9W apWidget, int alOp, size_t alArg);
	void (*ohpl_a9h_w_set_f)(OhplA9W apWidget, int alOp, const float *apVal);
	void (*ohpl_a9h_w_get_f)(OhplA9W apWidget, int alOp, float *apOut);
	size_t (*ohpl_a9h_w_str)(OhplA9W apWidget, int alOp, const uint32_t *apText, size_t alLen, size_t alArg);
	void (*ohpl_a9h_w_get_str)(OhplA9W apWidget, int alOp, int alIdx, OhplA9Sink apSink, void *apCtx);
	void (*ohpl_a9h_w_cb)(OhplA9W apWidget, int alMsg, size_t alObj);
	void (*ohpl_a9h_w_nav)(OhplA9W apWidget, int alDir, OhplA9W apTarget);
	float (*ohpl_a9h_text_width)(OhplA9W apWidget, const uint32_t *apText, size_t alLen);
	void (*ohpl_a9h_image_file)(OhplA9W apWidget, const uint32_t *apText, size_t alLen);
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
	// C++ -> Rust
	bool ohpl_a9_cb(size_t alObj, OhplA9W apWidget, int alVal);
	void ohpl_a9_set_host(const OhplA9Host *apHost);
}

#endif
