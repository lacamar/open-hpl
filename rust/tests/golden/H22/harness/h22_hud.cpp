// H22 DevHud draw-sequence differential harness: the original cDevHud::Render/RenderBugReport
// (extracted from DevHud.cpp by extract_hud.py) over a recording GUI set/font mock, against
// ohpl_h22_hud_render over the same mock. First trace line is the case, then every draw operation.
//   usage: H22_SEED=<n> H22_MODE=cpp|rust h22_hud
#include <cstdarg>
#include <cstdint>
#include <cwchar>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "engine/RustEngine.h"
#include "engine/RustEngineH22.h"
#include "graphics/Bitmap.h"
#include "graphics/Color.h"
#include "math/Math.h"
#include "math/MathTypes.h"
#include "system/EngineDiagnostics.h"
#include "system/String.h"
#include "system/SystemTypes.h"

using namespace hpl;

static std::string gTrace;

static void T(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void T(const char *fmt, ...)
{
	char b[2048];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(b, sizeof(b), fmt, ap);
	va_end(ap);
	gTrace += b;
	gTrace += "\n";
}

static std::string HexW(const wchar_t *p, size_t n)
{
	std::string s;
	char b[16];
	for(size_t i = 0; i < n; ++i) { snprintf(b, sizeof(b), "%x.", (unsigned)p[i]); s += b; }
	return s.empty() ? "-" : s;
}

struct Scenario
{
	int sx, sy;
	bool hud, open, capture, initOk;
	unsigned long saved, now;
	tWString text;
	tString last;
	float fps, avg;
	tStringVec sys;
	uint64_t s;
	uint64_t rnd() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 0x2545F4914F6CDD1DULL; }
} gSc;

static float FontLen(float size, const wchar_t *p, size_t n)
{
	int w = 0;
	for(size_t i = 0; i < n; ++i) w += 3 + (int)(p[i] % 5);
	return size * (float)w;
}

namespace mock {

struct MockPlat
{
	static unsigned long GetApplicationTime() { T("APP_TIME -> %lu", gSc.now); return gSc.now; }
};

struct MockFont
{
	float GetLength(const cVector2f &v, const wchar_t *apText)
	{
		size_t n = wcslen(apText);
		float r = FontLen(v.x, apText, n);
		T("FONT_LENGTH %.9g %s -> %.9g", v.x, HexW(apText, n).c_str(), r);
		return r;
	}
};

struct MockSet
{
	void SetVirtualSize(const cVector2f &v, float, float, const cVector2f &, bool) { T("SET_VIRTUAL %.9g %.9g", v.x, v.y); }
	void DrawGfx(void *, const cVector3f &p, const cVector2f &s) { T("DRAW_GFX %.9g %.9g %.9g %.9g %.9g", p.x, p.y, p.z, s.x, s.y); }
	void DrawFont(const tWString &t, MockFont *, const cVector3f &p, const cVector2f &s, const cColor &c)
	{
		T("DRAW_FONT %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g", HexW(t.data(), t.size()).c_str(), p.x, p.y, p.z, s.x, c.r, c.g, c.b, c.a);
	}
	void Render(void *) { T("RENDER_SET"); }
};

struct MockLow
{
	cVector2l mvScreen;
	const cVector2l &GetScreenSizeInt() { return mvScreen; }
	cBitmap *CopyFrameBufferToBitmap() { T("CAPTURE"); return NULL; }
	float GetGammaCorrection() { return 1; }
	void SetCurrentFrameBuffer(void *) { T("SET_FB_NULL"); }
};

struct MockGraphics { MockLow low; MockLow *GetLowLevel() { return &low; } };

struct MockEngine
{
	MockGraphics gfx;
	MockGraphics *GetGraphics() { return &gfx; }
	float GetFPS() { return gSc.fps; }
	float GetAvgFrameTimeInMS() { return gSc.avg; }
};

struct MockHud
{
	bool Init() { T("INIT -> %d", gSc.initOk ? 1 : 0); return gSc.initOk; }
	void Render(bool abHud);
	void RenderBugReport(const cVector2l &avScreen);

	MockEngine *mpEngine;
	MockSet *mpSet;
	MockFont *mpFont;
	void *mpBackground;
	tStringVec mvSystem;
	bool mbBugOpen, mbBugCapture;
	unsigned long mlSavedTime;
	tWString msBugText;
	cBitmap *mpBugShot;
	tString msLastSaved;
};

#include "hud_body.inc"

} // namespace mock

static uint64_t HostTime(void *) { T("APP_TIME -> %lu", gSc.now); return gSc.now; }
static bool HostInit(void *) { T("INIT -> %d", gSc.initOk ? 1 : 0); return gSc.initOk; }
static void HostCapture(void *) { T("CAPTURE"); }
static void HostScreen(void *, int *x, int *y) { *x = gSc.sx; *y = gSc.sy; }
static void HostVirtual(void *, float w, float h) { T("SET_VIRTUAL %.9g %.9g", w, h); }
static float HostFps(void *) { return gSc.fps; }
static float HostAvg(void *) { return gSc.avg; }
static size_t HostSysN(void *) { return gSc.sys.size(); }
static void HostSys(void *, size_t i, OhplH22Sink sink, void *ctx) { sink(ctx, (const uint8_t*)gSc.sys[i].data(), gSc.sys[i].size()); }
static float HostLen(void *, float size, const uint32_t *p, size_t n)
{
	tWString w((const wchar_t*)p, n);
	float r = FontLen(size, w.data(), n);
	T("FONT_LENGTH %.9g %s -> %.9g", size, HexW(w.data(), n).c_str(), r);
	return r;
}
static void HostGfx(void *, float x, float y, float z, float w, float h) { T("DRAW_GFX %.9g %.9g %.9g %.9g %.9g", x, y, z, w, h); }
static void HostFont(void *, const uint32_t *p, size_t n, float x, float y, float z, float size, float r, float g, float b, float a)
{
	tWString w((const wchar_t*)p, n);
	T("DRAW_FONT %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g", HexW(w.data(), n).c_str(), x, y, z, size, r, g, b, a);
}
static void HostFb(void *) { T("SET_FB_NULL"); }
static void HostRender(void *) { T("RENDER_SET"); }

static std::string RandText(Scenario &sc, int n)
{
	static const char *pieces[] = { "a", "bb", "ccc", " ", " ", "  ", "\n", "word", "longerword", "\xc3\xa9", "\xe6\x97\xa5", "x_y", "-" };
	std::string s;
	for(int i = 0; i < n; ++i) s += pieces[sc.rnd() % (sizeof(pieces) / sizeof(*pieces))];
	return s;
}

int hplMain(const tString&)
{
	const char *pSeed = getenv("H22_SEED");
	const char *pMode = getenv("H22_MODE");
	unsigned seed = pSeed ? atoi(pSeed) : 1;
	gSc.s = 0x9E3779B97F4A7C15ULL * (seed + 1);
	for(int i = 0; i < 4; ++i) gSc.rnd();
	static const int vW[] = { 640, 800, 1280, 1366, 1920, 2560, 3840, 1024, 4096 };
	static const int vH[] = { 360, 480, 720, 768, 1080, 1440, 2160, 600, 100 };
	int k = (int)(gSc.rnd() % 9);
	gSc.sx = vW[k] + (int)(gSc.rnd() % 3) * 7;
	gSc.sy = vH[(k + (int)(gSc.rnd() % 3)) % 9] + (int)(gSc.rnd() % 3) * 3;
	gSc.hud = gSc.rnd() % 3 != 0;
	gSc.open = gSc.rnd() % 3 == 0;
	gSc.capture = gSc.rnd() % 4 == 0;
	gSc.initOk = gSc.rnd() % 20 != 0;
	gSc.now = 100000 + gSc.rnd() % 100000;
	switch(gSc.rnd() % 4)
	{
	case 0: gSc.saved = 0; break;
	case 1: gSc.saved = gSc.now - gSc.rnd() % 3500; break;
	case 2: gSc.saved = gSc.now - 4000 - gSc.rnd() % 5000; break;
	default: gSc.saved = gSc.now - 3999; break;
	}
	gSc.text = cString::UTF8ToWChar(RandText(gSc, (int)(gSc.rnd() % 60)));
	gSc.last = "/home/u/.local/share/open-hpl/bugs/" + RandText(gSc, (int)(gSc.rnd() % 6));
	gSc.fps = (float)(gSc.rnd() % 24000) / 100.0f;
	gSc.avg = (float)(gSc.rnd() % 90000) / 1000.0f;
	for(int i = (int)(gSc.rnd() % 5); i > 0; --i) gSc.sys.push_back(RandText(gSc, (int)(gSc.rnd() % 12) + 1));

	std::string sSys;
	for(const tString &s : gSc.sys) sSys += HexW(cString::UTF8ToWChar(s).data(), cString::UTF8ToWChar(s).size()) + ",";
	T("case sx=%d sy=%d hud=%d open=%d capture=%d init=%d saved=%lu now=%lu fps=%.9g avg=%.9g text=%s last=%s sys=%s", gSc.sx, gSc.sy, gSc.hud, gSc.open,
	  gSc.capture, gSc.initOk, gSc.saved, gSc.now, gSc.fps, gSc.avg, HexW(gSc.text.data(), gSc.text.size()).c_str(),
	  HexW(cString::UTF8ToWChar(gSc.last).data(), cString::UTF8ToWChar(gSc.last).size()).c_str(), sSys.c_str());

	bool bCaptureAfter;
	if(pMode && !strcmp(pMode, "cpp"))
	{
		mock::MockEngine eng;
		eng.gfx.low.mvScreen = cVector2l(gSc.sx, gSc.sy);
		mock::MockSet set;
		mock::MockFont font;
		mock::MockHud hud;
		hud.mpEngine = &eng;
		hud.mpSet = &set;
		hud.mpFont = &font;
		hud.mpBackground = NULL;
		hud.mvSystem = gSc.sys;
		hud.mbBugOpen = gSc.open;
		hud.mbBugCapture = gSc.capture;
		hud.mlSavedTime = gSc.saved;
		hud.msBugText = gSc.text;
		hud.mpBugShot = NULL;
		hud.msLastSaved = gSc.last;
		hud.Render(gSc.hud);
		bCaptureAfter = hud.mbBugCapture;
	}
	else
	{
		OhplH22HudHost h = { NULL, HostTime, HostInit, HostCapture, HostScreen, HostVirtual, HostFps, HostAvg, HostSysN, HostSys, HostLen, HostGfx,
							 HostFont, HostFb, HostRender };
		bool bTaken = ohpl_h22_hud_render(&h, gSc.hud, gSc.open, gSc.capture, gSc.saved, (const uint32_t*)gSc.text.data(), gSc.text.size(),
										  (const uint8_t*)gSc.last.data(), gSc.last.size());
		bCaptureAfter = gSc.capture && !bTaken;
	}
	T("capture_after=%d", bCaptureAfter ? 1 : 0);
	fputs(gTrace.c_str(), stdout);
	return 0;
}
