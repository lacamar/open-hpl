#ifndef HPL_RUST_GUI_H
#define HPL_RUST_GUI_H

// C ABI of rust/src/gui/ffi.rs.

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"
#include "math/RustMath.h"
#include "graphics/GraphicsTypes.h"
#include "gui/GuiTypes.h"

namespace hpl {

static_assert(sizeof(cVertex) == 64 && offsetof(cVertex, tex) == 12 && offsetof(cVertex, col) == 48, "cVertex layout");
static_assert(eGuiMaterial_LastEnum == 6, "OHPL_GUI_MATERIAL_NONE");
static_assert(eFontAlign_Left == 0 && eFontAlign_Right == 1 && eFontAlign_Center == 2, "font align values");
static_assert(sizeof(wchar_t) == 4, "wide text crosses the ABI as u32");

enum { kOhplGuiBeginMaterial = 1, kOhplGuiSetClip = 2, kOhplGuiEndClip = 4, kOhplGuiEndMaterial = 8 };

struct OhplGuiGfxInfo {
	cVector3f offset;
	cVector2f image_size;
	const void* const* material;
	const void* const* texture0;
	const cVertex* vtx;
};

struct OhplGuiHost {
	void* ctx;
	void (*gfx_info)(void*, void*, OhplGuiGfxInfo*);
	void (*gfx_flush)(void*, void*);
	void* (*material)(void*, int);
};

struct OhplGuiFrame {
	const void* clip;
	cRect2f clip_rect;
	cVector3f draw_offset;
};

struct OhplGuiGlyphTable {
	const void* const* glyphs;
	size_t count;
	unsigned short first;
	unsigned short last;
	size_t off_gfx;
	size_t off_offset;
	size_t off_size;
	size_t off_advance;
};

struct OhplGuiVertex {
	cVector3f pos;
	cColor col;
	cVector3f tex;
};

struct OhplGuiBatch {
	const void* material;
	const void* texture;
	const void* clip;
	size_t first;
	size_t count;
	uint32_t flags;
};

struct OhplGuiBatches {
	const OhplGuiBatch* batches;
	size_t batch_count;
	const OhplGuiVertex* vertices;
	size_t vertex_count;
};

struct OhplGuiTooltip {
	cVector3f pos;
	cVector2f size;
	cVector2f background_size;
};

extern "C" {

void* ohpl_gui_list_new();
void ohpl_gui_list_free(void* list);
void ohpl_gui_list_clear(void* list);

void ohpl_gui_draw_gfx(void* list, const OhplGuiHost* host, const OhplGuiFrame* frame, void* gfx,
					   const cVector3f* pos, const cVector2f* size, const cColor* color,
					   int material, float angle, const cVector3f* pivot);
void ohpl_gui_draw_text(void* list, const OhplGuiHost* host, const OhplGuiFrame* frame, const OhplGuiGlyphTable* table,
						const wchar_t* text, size_t len, const cVector2f* size, const cVector3f* pos,
						const cColor* color, int material, int align);
void ohpl_gui_build(void* list, OhplGuiBatches* out);

void ohpl_gui_clip_child_rect(const cRect2f* parent, const cVector3f* pos, const cVector2f* size, cRect2f* out);
void ohpl_gui_scissor(const cRect2f* rect, const cVector2l* target, const cVector2f* virtualSize,
					  const cVector2f* virtualOffset, bool flipY, cVector2l* outPos, cVector2l* outSize);
void ohpl_gui_tooltip(const cVector2f* mouse, float mouseZ, const cVector2f* pointerSize, const cVector2f* virtualSize,
					  float textLength, float fontHeight, int rows, OhplGuiTooltip* out);
void ohpl_gui_position_inside_bounds(const cVector3f* pos, const cVector2f* size, const cVector2f* setSize, cVector3f* out);
float ohpl_gui_next_popup_z(float z);
void ohpl_gui_virtual_size(const cVector2f* size, float globalScale, bool ignoreGlobalScale, cVector2f* out);

}
}

#endif
