#ifndef HPL_RUST_GUI_SKIN_H
#define HPL_RUST_GUI_SKIN_H

// C ABI of rust/src/gui/ffi_skin.rs.

#include <cstddef>
#include <cstdint>
#include <string>
#include "graphics/GraphicsTypes.h"
#include "gui/GuiTypes.h"

namespace hpl {

struct OhplGuiAnimState {
	float currentFrame;
	int activeImage;
	bool forward;
};

struct OhplGuiAnimInput {
	size_t animationCount;
	size_t bufferLen;
	int currentAnimation;
	int type;
	float frameLength;
	float timeStep;
	int paused;
};

struct OhplSkinAttribute {
	const char* type;
	float value[3];
};

struct OhplSkinFont {
	const char* type;
	const char* file;
	float size[2];
	float color[4];
};

struct OhplSkinGfx {
	const char* type;
	const char* file;
	const char* materialWarn;
	const char* animWarn;
	float offset[2];
	float activeSize[2];
	float color[4];
	float frameTime;
	unsigned int material;
	int animMode;
	int animated;
	int hasActiveSize;
};

struct OhplSkinView {
	const OhplSkinAttribute* attributes;
	size_t attributeCount;
	const OhplSkinFont* fonts;
	size_t fontCount;
	const OhplSkinGfx* gfx;
	size_t gfxCount;
};

static_assert(sizeof(OhplGuiAnimState) == 12 && sizeof(OhplGuiAnimInput) == 40, "anim ABI");
static_assert(sizeof(OhplSkinAttribute) == 24 && sizeof(OhplSkinFont) == 40 && sizeof(OhplSkinGfx) == 88 && sizeof(OhplSkinView) == 48, "skin ABI");
static_assert(eBlendFunc_Zero == 0 && eBlendFunc_One == 1 && eBlendFunc_DestColor == 4 && eBlendFunc_SrcAlpha == 6 &&
			  eBlendFunc_OneMinusSrcAlpha == 7, "blend values");
static_assert(eGuiMaterial_LastEnum == 6, "material values");

enum { kOhplSkinFont = 0, kOhplSkinAttribute = 1, kOhplSkinGfx = 2 };

extern "C" {

uint32_t ohpl_gui_skin_from_name(int kind, const char* name, size_t len);
void ohpl_gui_skin_name(int kind, uint32_t index, void (*sink)(void*, const char*, size_t), void* ctx);
uint32_t ohpl_gui_material_slot(uint32_t type);
bool ohpl_gui_material_blend(uint32_t type, int* src, int* dst);
int ohpl_gui_mouse_to_mouse(int button);
int ohpl_gui_mouse_to_gui(int button);

int64_t ohpl_gui_gfx_update(OhplGuiAnimState* state, const OhplGuiAnimInput* input);
void ohpl_gui_gfx_restart(OhplGuiAnimState* state);
float ohpl_gui_gfx_animation_time(float time, bool hasLength, float length);
void ohpl_gui_gfx_texture_uv(float* tex, float sizeX, float sizeY, bool rect, float startX, float startY, float endX,
							 float endY, float* imageSize);
void ohpl_gui_gfx_flip_uv(float* tex, bool flip, int source, const float* image);
bool ohpl_gui_gfx_flush_changed(int* last, int count);

void* ohpl_gui_skin_parse(const char* data, size_t len, bool readOk, int* status);
const OhplSkinView* ohpl_gui_skin_view(const void* handle);
void ohpl_gui_skin_free(void* handle);

}

}

#endif
