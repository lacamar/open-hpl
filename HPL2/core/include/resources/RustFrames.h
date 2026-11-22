#ifndef HPL_RUST_FRAMES_H
#define HPL_RUST_FRAMES_H

// C ABI of rust/src/resources/h3_frames.rs (image frames: cFrameTexture, cFrameBitmap, cFrameSubImage state).

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"
#include "resources/RustResourcesH3.h"

struct OhplFrSub
{
	hpl::cRect2l rect;
	hpl::cVector2l src;
	float tex[4][3];
	int updateCount;
};
static_assert(sizeof(OhplFrSub) == 76, "OhplFrSub layout");

struct OhplFrBmpInfo
{
	hpl::cVector3l size;
	int format;
	int bytesPerPixel;
	unsigned char *data;
	size_t len;
	bool compressed;
	OhplH3Str name;
};
static_assert(sizeof(OhplFrBmpInfo) == 64, "OhplFrBmpInfo layout");

struct OhplFrHost
{
	void *ctx;
	void (*log)(void*, int, const uint8_t*, size_t);
	void* (*texNew)(void*);
	void (*texDelete)(void*, void*);
	void (*texUpload)(void*, void*, void*);
	void (*texSize)(void*, void*, hpl::cVector3l*);
	void* (*bmpNew)(void*, int, int);
	void (*bmpDelete)(void*, void*);
	void (*bmpInfo)(void*, void*, OhplFrBmpInfo*);
	void (*bmpClear)(void*, void*);
	void* (*subNew)(void*, OhplH3Str, OhplH3WStr, void*, bool);
	void* (*subRs)(void*, void*);
};

extern "C" {
void ohpl_fr_sub_free(void *sub, unsigned users);
void* ohpl_fr_sub_texture(const void *sub);
void ohpl_fr_sub_flush(void *sub);
void* ohpl_fr_ftex_custom_image(void *frameTexture, const hpl::cVector2l *pos, const hpl::cVector2l *size);
void* ohpl_fr_ftex_texture(const void *frameTexture);
}

#endif
