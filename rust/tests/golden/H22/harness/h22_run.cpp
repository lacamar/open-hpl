// H22 frame-loop differential harness: the original C++ cEngine::Run (extracted from Engine.cpp by
// extract_run.py, run over scripted mocks) against the Rust loop (ohpl_h22_engine_run) over the same
// mock host. Every host operation is logged with its arguments and result.
//   usage: H22_SEED=<n> H22_MODE=cpp|rust h22_run
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "engine/EngineTypes.h"
#include "engine/RustEngine.h"
#include "engine/RustEngineH22.h"
#include "scene/Scene.h"
#include "system/SystemTypes.h"

using namespace hpl;

enum { OP_WINDOW_MOUSE = 100, OP_WINDOW_VISIBLE = 101 };

static const char *kOpNames[] = {
	"LOG", "LOG_MEDIUM", "BROADCAST", "RUN_MESSAGE", "CONTAINER_IS_DEFAULT", "LT_RESET", "WANT_UPDATE", "END_UPDATE_LOOP", "STEP_SIZE",
	"GAME_IS_DONE", "WAIT_OUT_OF_FOCUS", "WINDOW_INPUT_FOCUS", "SLEEP", "INPUT_UPDATE", "PAUSED", "HAS_HEADLESS", "HC_LOGIC_STEP",
	"HC_UPDATE", "QUIT_POSTED", "RESET_QUIT", "WINDOW_FOCUS_BITS", "TAKE_PLUGGED", "TAKE_REMOVED", "UPDATE_LOG_ACTIVE",
	"CLEAR_UPDATE_LOG", "RENDER_ONCE", "LIMIT_FPS", "SWAP", "TIMING", "FT_START", "FT_STOP", "FT_SEC", "SCENE_RENDER", "DEVHUD_RENDER",
	"FLUSH", "DIAG_END_FRAME", "DIAG_ADD_TIMING", "FPS_ADD", "APP_TIME", "STEADY_NS"
};

struct Mock
{
	uint64_t s = 1;
	long budget = 300;
	long calls = 0;
	bool done = false, paused = false, quit = false, renderOnce = false, limitFps = true, headless = false, waitFocus = false, updateLog = false;
	int fbits = 7;
	uint64_t appTime = 1000;
	int64_t steadyNs = 5000000000LL;
	std::string trace;

	uint64_t rnd() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 0x2545F4914F6CDD1DULL; }
	bool pm(int n) { return (int)((rnd() >> 11) % 1000) < n; }

	void init(unsigned seed)
	{
		s = 0x9E3779B97F4A7C15ULL * (seed + 1);
		for(int i = 0; i < 4; ++i) rnd();
		budget = 120 + (long)(rnd() % 500) + (seed % 7 == 2 ? 3000 : 0);
		headless = seed % 2 == 0;
		waitFocus = seed % 3 == 0;
		updateLog = seed % 4 == 1;
		limitFps = seed % 5 != 0;
	}

	void log(const char *name, double a0, double a1, double a2, double a3, double r)
	{
		char b[240];
		int n = snprintf(b, sizeof(b), "%s", name);
		double a[4] = { a0, a1, a2, a3 };
		for(int i = 0; i < 4; ++i)
			if(a[i] != 0) n += snprintf(b + n, sizeof(b) - n, " %.17g", a[i]);
		if(r != 0) n += snprintf(b + n, sizeof(b) - n, " -> %.17g", r);
		trace += b;
		trace += "\n";
	}

	double op(int o, double a0 = 0, double a1 = 0, double a2 = 0, double a3 = 0)
	{
		switch(o)
		{
		case OHPL_H22_OP_HAS_HEADLESS: return headless;
		case OHPL_H22_OP_WAIT_OUT_OF_FOCUS: return waitFocus;
		case OHPL_H22_OP_RENDER_ONCE: return renderOnce;
		case OHPL_H22_OP_LIMIT_FPS: return limitFps;
		case OHPL_H22_OP_WINDOW_FOCUS_BITS:
		{
			bool bI = op(OHPL_H22_OP_WINDOW_INPUT_FOCUS) != 0;
			bool bM = op(OP_WINDOW_MOUSE) != 0;
			bool bV = op(OP_WINDOW_VISIBLE) != 0;
			return (bI ? 1 : 0) | (bM ? 2 : 0) | (bV ? 4 : 0);
		}
		}
		++calls;
		double r = 0;
		switch(o)
		{
		case OHPL_H22_OP_GAME_IS_DONE: if(calls >= budget) done = true; r = done; break;
		case OHPL_H22_OP_WANT_UPDATE: r = pm(450); break;
		case OHPL_H22_OP_STEP_SIZE: r = (double)(1.0f / 60.0f); break;
		case OHPL_H22_OP_PAUSED: r = paused; break;
		case OHPL_H22_OP_WINDOW_INPUT_FOCUS: r = pm(500); break;
		case OP_WINDOW_MOUSE: r = pm(500); break;
		case OP_WINDOW_VISIBLE: r = pm(700); break;
		case OHPL_H22_OP_QUIT_POSTED: r = quit; break;
		case OHPL_H22_OP_RESET_QUIT: quit = false; break;
		case OHPL_H22_OP_TAKE_PLUGGED: r = pm(60); break;
		case OHPL_H22_OP_TAKE_REMOVED: r = pm(60); break;
		case OHPL_H22_OP_UPDATE_LOG_ACTIVE: r = updateLog; break;
		case OHPL_H22_OP_CONTAINER_IS_DEFAULT: r = pm(600); break;
		case OHPL_H22_OP_FT_SEC: r = 0.001 * (1 + (int)(rnd() % 30)); break;
		case OHPL_H22_OP_APP_TIME: appTime += 1 + rnd() % 25; r = (double)appTime; break;
		case OHPL_H22_OP_STEADY_NS: steadyNs += 100000 + (int64_t)(rnd() % 3000000); r = (double)steadyNs; break;
		case OHPL_H22_OP_RUN_MESSAGE:
		{
			uint64_t v = rnd();
			if(v % 1000 < 20) paused = !paused;
			else if(v % 1000 < 40) renderOnce = !renderOnce;
			else if(v % 1000 < 60) limitFps = !limitFps;
			else if(v % 1000 < 75) quit = true;
			break;
		}
		}
		const char *name = o == OP_WINDOW_MOUSE ? "WINDOW_MOUSE_FOCUS" : o == OP_WINDOW_VISIBLE ? "WINDOW_VISIBLE" : kOpNames[o];
		log(name, a0, a1, a2, a3, r);
		return r;
	}
} gMock;

struct MockClock
{
	typedef int64_t rep;
	typedef std::nano period;
	typedef std::chrono::nanoseconds duration;
	typedef std::chrono::time_point<MockClock> time_point;
	static time_point now() { return time_point(duration((int64_t)gMock.op(OHPL_H22_OP_STEADY_NS))); }
};

namespace mock {

struct MockPlat
{
	static unsigned long GetApplicationTime() { return (unsigned long)gMock.op(OHPL_H22_OP_APP_TIME); }
	static void Sleep(unsigned int ms) { gMock.op(OHPL_H22_OP_SLEEP, ms); }
};

struct MockDiag
{
	static void EndFrame() { gMock.op(OHPL_H22_OP_DIAG_END_FRAME); }
	static void AddFrameTiming(double l, int s, double r, double w) { gMock.op(OHPL_H22_OP_DIAG_ADD_TIMING, l, s, r, w); }
};

static void MockLog(const char *fmt, ...)
{
	if(!strcmp(fmt, "--------------------------------------------------------\n\n")) gMock.op(OHPL_H22_OP_LOG, 0);
	else if(!strcmp(fmt, "Game Running\n")) gMock.op(OHPL_H22_OP_LOG, 1);
	else if(!strcmp(fmt, "--------------------------------------------------------\n")) gMock.op(OHPL_H22_OP_LOG, 2);
	else if(!strcmp(fmt, "Statistics\n")) gMock.op(OHPL_H22_OP_LOG, 3);
	else if(!strcmp(fmt, "User Exit\n")) gMock.op(OHPL_H22_OP_LOG, 4);
	else if(!strcmp(fmt, " Medium framerate: %f\n"))
	{
		va_list ap;
		va_start(ap, fmt);
		double d = va_arg(ap, double);
		va_end(ap);
		gMock.op(OHPL_H22_OP_LOG_MEDIUM, d);
	}
	else gMock.trace += std::string("UNKNOWN LOG ") + fmt;
}

static int TimingTag(const char *n)
{
	static const char *v[6] = { "WaitAndFinishRendering", "SwapBuffers", "OnDraw", "RenderAll", "PostRender", "FlushRender" };
	for(int i = 0; i < 6; ++i) if(!strcmp(n, v[i])) return i;
	return -1;
}
#define START_TIMING(x) gMock.op(OHPL_H22_OP_TIMING, TimingTag(#x), 1);
#define STOP_TIMING(x) gMock.op(OHPL_H22_OP_TIMING, TimingTag(#x), 0);

static bool GetUpdateLogActive() { return gMock.op(OHPL_H22_OP_UPDATE_LOG_ACTIVE) != 0; }
static void ClearUpdateLogFile() { gMock.op(OHPL_H22_OP_CLEAR_UPDATE_LOG); }

struct MockUpd
{
	void RunMessage(eUpdateableMessage m, float x = 0) { gMock.op(OHPL_H22_OP_RUN_MESSAGE, (double)m, (double)x); }
	void BroadcastMessageToAll(eUpdateableMessage m) { gMock.op(OHPL_H22_OP_BROADCAST, (double)m); }
	tString GetCurrentContainerName() { return gMock.op(OHPL_H22_OP_CONTAINER_IS_DEFAULT) != 0 ? "Default" : "Other"; }
};
struct MockTimer
{
	void Reset() { gMock.op(OHPL_H22_OP_LT_RESET); }
	bool WantUpdate() { return gMock.op(OHPL_H22_OP_WANT_UPDATE) != 0; }
	void EndUpdateLoop() { gMock.op(OHPL_H22_OP_END_UPDATE_LOOP); }
};
struct MockFrameTimer
{
	void Start() { gMock.op(OHPL_H22_OP_FT_START); }
	void Stop() { gMock.op(OHPL_H22_OP_FT_STOP); }
	double GetTimeInSec() { return gMock.op(OHPL_H22_OP_FT_SEC); }
};
struct MockInput
{
	void Update(float dt) { gMock.op(OHPL_H22_OP_INPUT_UPDATE, (double)dt); }
	bool isQuitMessagePosted() { return gMock.op(OHPL_H22_OP_QUIT_POSTED) != 0; }
	void resetQuitMessagePosted() { gMock.op(OHPL_H22_OP_RESET_QUIT); }
};
struct MockHc
{
	void LogicStep() { gMock.op(OHPL_H22_OP_HC_LOGIC_STEP); }
	void Update() { gMock.op(OHPL_H22_OP_HC_UPDATE); }
};
struct MockLow
{
	void SwapBuffers() { gMock.op(OHPL_H22_OP_SWAP); }
	void FlushRendering() { gMock.op(OHPL_H22_OP_FLUSH); }
	bool GetWindowInputFocus() { return gMock.op(OHPL_H22_OP_WINDOW_INPUT_FOCUS) != 0; }
	bool GetWindowMouseFocus() { return gMock.op(OP_WINDOW_MOUSE) != 0; }
	bool GetWindowIsVisible() { return gMock.op(OP_WINDOW_VISIBLE) != 0; }
};
struct MockGraphics
{
	MockLow low;
	MockLow *GetLowLevel() { return &low; }
};
struct MockScene
{
	void Render(float ft, int flags) { gMock.op(OHPL_H22_OP_SCENE_RENDER, (double)ft); }
};
struct MockDevHud
{
	void Render(bool) { gMock.op(OHPL_H22_OP_DEVHUD_RENDER); }
};
struct MockFps
{
	void AddFrame() { gMock.op(OHPL_H22_OP_FPS_ADD); }
};

template<int ID> struct Silent { operator bool() const { return gMock.op(ID) != 0; } };
struct Take
{
	int id;
	operator bool() const { return gMock.op(id) != 0; }
	Take &operator=(bool) { return *this; }
};

struct MockEngine
{
	MockEngine()
	{
		static MockHc hc;
		mpHeadlessControl = gMock.headless ? &hc : NULL;
		ohpl_en_loop_init(&mLoop);
		mfFrameTime = 0;
		mbApplicationHasInputFocus = mbApplicationHasMouseFocus = mbApplicationIsVisible = false;
	}

	void Run();
	void UpdateFrameTimer();
	void CheckAndBroadcastFocusChange();
	void CheckIfAppInFocusElseWait();
	void CheckAndBroadcastDeviceChange();

	bool GetGameIsDone() { return gMock.op(OHPL_H22_OP_GAME_IS_DONE) != 0; }
	bool GetPaused() { return gMock.op(OHPL_H22_OP_PAUSED) != 0; }
	float GetStepSize() { return (float)gMock.op(OHPL_H22_OP_STEP_SIZE); }

	MockUpd uUpdater, *mpUpdater = &uUpdater;
	MockTimer uTimer, *mpLogicTimer = &uTimer;
	MockFrameTimer uFt, *mpFrameTimer = &uFt;
	MockInput uInput, *mpInput = &uInput;
	MockHc *mpHeadlessControl;
	MockGraphics uGfx, *mpGraphics = &uGfx;
	MockScene uScene, *mpScene = &uScene;
	MockDevHud uHud, *mpDevHud = &uHud;
	MockFps uFps, *mpFPSCounter = &uFps;

	OhplEnLoop mLoop;
	float mfFrameTime;
	Silent<OHPL_H22_OP_WAIT_OUT_OF_FOCUS> mbWaitIfAppOutOfFocus;
	Silent<OHPL_H22_OP_RENDER_ONCE> mbRenderOnce;
	Silent<OHPL_H22_OP_LIMIT_FPS> mbLimitFPS;
	bool mbDevHudActive = false;
	bool mbApplicationHasInputFocus, mbApplicationHasMouseFocus, mbApplicationIsVisible;
	static Take mbDevicePlugged, mbDeviceRemoved;
};
Take MockEngine::mbDevicePlugged = { OHPL_H22_OP_TAKE_PLUGGED };
Take MockEngine::mbDeviceRemoved = { OHPL_H22_OP_TAKE_REMOVED };

#include "run_body.inc"

} // namespace mock

static double Call(void *, int op, double a0, double a1, double a2, double a3)
{
	if(op == OHPL_H22_OP_STEP) return 0;
	return gMock.op(op, a0, a1, a2, a3);
}

static void PrintState(const OhplEnLoop &l, float ft, unsigned bits)
{
	char b[400];
	snprintf(b, sizeof(b), "loop gameTime=%.17g numOfTimes=%.17g logicMs=%.17g swapMs=%.17g steps=%d clearCheck=%d isUpdated=%d bufferSwap=%d swappedOnce=%d\nframeTime=%.9g focusBits=%u\n",
			 l.gameTime, l.numOfTimes, l.logicMs, l.swapMs, l.steps, l.clearCheck, l.isUpdated, l.bufferSwap, l.swappedOnce, ft, bits);
	fputs(b, stdout);
}

int hplMain(const tString&)
{
	const char *pSeed = getenv("H22_SEED");
	const char *pMode = getenv("H22_MODE");
	gMock.init(pSeed ? atoi(pSeed) : 1);
	if(pMode && !strcmp(pMode, "cpp"))
	{
		mock::MockEngine e;
		e.Run();
		unsigned bits = (e.mbApplicationHasInputFocus ? 1 : 0) | (e.mbApplicationHasMouseFocus ? 2 : 0) | (e.mbApplicationIsVisible ? 4 : 0);
		fputs(gMock.trace.c_str(), stdout);
		PrintState(e.mLoop, e.mfFrameTime, bits);
	}
	else
	{
		OhplEnLoop loop;
		ohpl_en_loop_init(&loop);
		float ft = 0;
		uint32_t bits = 0;
		ohpl_h22_engine_run(Call, NULL, &loop, &ft, &bits);
		fputs(gMock.trace.c_str(), stdout);
		PrintState(loop, ft, bits);
	}
	return 0;
}
