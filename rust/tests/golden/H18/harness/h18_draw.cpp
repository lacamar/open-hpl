// H18 draw trace harness: cGuiSet::DrawGfx / clip regions / Render over a RecordingLowLevelGraphics.
// Prints the recorded low-level trace; run with OHPL_IMPL=cpp and =rust and diff stdout.
// usage: H18_SEED=<n> h18_draw
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>

#define private public
#define protected public
#include "gui/Gui.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"
#include "graphics/Graphics.h"
#undef private
#undef protected
#include "graphics/Bitmap.h"
#include "impl/RustBackends.h"
#include "trace/RecordingLowLevelGraphics.h"

using namespace hpl;

static unsigned long long gRng;
static unsigned Rnd(unsigned n)
{
	gRng = gRng * 6364136223846793005ull + 1442695040888963407ull;
	return (unsigned)((gRng >> 33) % n);
}

static float Frac(unsigned n) { return (float)Rnd(n); }

int hplMain(const tString&)
{
	const char *s = getenv("H18_SEED");
	int seed = s ? atoi(s) : 1;
	gRng = (unsigned long long)seed * 7919 + 17;

	if(getenv("H18_PAD")) { for(int p = 0; p < atoi(getenv("H18_PAD")); ++p) (void)malloc(24 + p * 8); }
	cRecordingLowLevelGraphics llg;
	cGraphics gfx(&llg, NULL);
	cGui gui;
	gui.Init(NULL, &gfx, NULL, NULL, NULL);
	cGuiSet *set = new cGuiSet("h18", &gui, NULL, NULL, &gfx, NULL, NULL);

	std::vector<cGuiGfxElement*> elems;
	for(int i = 0; i < 4; ++i)
	{
		iTexture *tex = llg.CreateTexture("t", eTextureType_2D, eTextureUsage_Normal);
		cBitmap *bmp = new cBitmap();
		bmp->CreateData(cVector3l(16 << Rnd(3), 16 << Rnd(3), 1), ePixelFormat_RGBA, 0, 0);
		cBitmapData *d = bmp->GetData(0, 0);
		for(int k = 0; k < d->mlSize; ++k) d->mpData[k] = (unsigned char)(i * 131 + k * 7);
		tex->CreateFromBitmap(bmp);
		elems.push_back(gui.CreateGfxTexture(tex, false, (eGuiMaterial)Rnd(6), cColor(1, 1)));
	}
	elems.push_back(gui.CreateGfxFilledRect(cColor(0.5f, 0.25f, 0.75f, 1), eGuiMaterial_Alpha));

	for(int frame = 0; frame < 2; ++frame)
	{
		set->SetCurrentClipRegion(&set->mBaseClipRegion);
		int draws = 30 + (int)Rnd(20);
		for(int i = 0; i < draws; ++i)
		{
			if(Rnd(5) == 0)
			{
				cGuiClipRegion *parent = Rnd(2) ? &set->mBaseClipRegion : set->mpCurrentClipRegion;
				set->SetCurrentClipRegion(parent->CreateChild(cVector3f(Frac(300), Frac(300), 0), cVector2f(Frac(200) + 10, Frac(200) + 10)));
			}
			if(Rnd(7) == 0) set->SetDrawOffset(cVector3f(Frac(20), Frac(20), 0));
			cGuiGfxElement *g = elems[Rnd((unsigned)elems.size())];
			eGuiMaterial mat = Rnd(3) == 0 ? (eGuiMaterial)Rnd(6) : eGuiMaterial_LastEnum;
			bool pivot = Rnd(4) == 0;
			set->DrawGfx(g, cVector3f(Frac(500), Frac(400), (float)((i * 7) % 50) * 0.5f - 5.0f + (float)Rnd(2) * 100.0f * (float)(i % 2)), cVector2f(Frac(120) + 4, Frac(120) + 4),
						 cColor((float)Rnd(100) / 100.0f, 1, (float)Rnd(100) / 100.0f, (float)Rnd(100) / 100.0f),
						 mat, Rnd(3) == 0 ? Frac(360) * 0.017453f : 0.0f, pivot, cVector3f(Frac(60), Frac(60), 0));
		}
		set->Render(NULL);
		set->ClearRenderObjects();
	}
	fputs(llg.Trace().Text().c_str(), stdout);
	fflush(stdout);
	_exit(0);
}
