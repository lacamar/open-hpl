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


extern "C" {

void ohpl_gui_build(void* list, OhplGuiBatches* out);

}
}

#endif
