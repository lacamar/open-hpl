#include "SomaS5Dump.h"
#include "SomaLuxScriptable.h"
#include "RustSomaLuxS5.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *Dump()
{
	static FILE *spFile = NULL;
	static bool sbInit = false;
	if (sbInit == false)
	{
		sbInit = true;
		const char *sPath = getenv("OHPL_S5_DUMP");
		if (sPath && *sPath)
			spFile = fopen(sPath, "a");
	}
	return spFile;
}

unsigned OhplS5Bits(float afX)
{
	unsigned l;
	memcpy(&l, &afX, sizeof(l));
	return l;
}

bool OhplS5DumpEnabled() { return Dump() != NULL; }

void OhplS5Log(const char *fmt, ...)
{
	FILE *f = Dump();
	if (f == NULL)
		return;
	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fflush(f);
}

namespace
{
unsigned long gSeed = 12345;
unsigned Rnd()
{
	gSeed = (gSeed * 1103515245ul + 12345ul) & 0x7fffffff;
	return (gSeed >> 8) & 0xffff;
}
float RndF() { return Rnd() / 65536.0f; }
}

namespace
{
enum { kAdd, kRemove, kExists, kLeft, kPause, kPassed, kTVal, kFade, kFSet, kFPause, kFVal, kClear };

struct TsOps
{
	cSomaLuxScriptable s;
	float Op(unsigned op, uint64_t id, float f0, float f1, int i0, unsigned flags, const char *func = "")
	{
		return ohpl_s5_ts_op(&s, op, id, f0, f1, i0, flags, (const unsigned char *)func, strlen(func));
	}
	void Timer_Add(uint64_t id, float t, const char *f, bool c, bool r) { Op(kAdd, id, t, 0, 0, (c ? 1 : 0) | (r ? 2 : 0), f); }
	void Timer_Remove(uint64_t id) { Op(kRemove, id, 0, 0, 0, 0); }
	bool Timer_Exists(uint64_t id) { return Op(kExists, id, 0, 0, 0, 0) != 0; }
	float Timer_GetTimeLeft(uint64_t id) { return Op(kLeft, id, 0, 0, 0, 0); }
	void Timer_SetPaused(uint64_t id, bool b) { Op(kPause, id, 0, 0, 0, b); }
	bool Timer_TimeHasPassed(uint64_t id, float l) { return Op(kPassed, id, l, 0, 0, 0) != 0; }
	float Timer_GetValue(uint64_t id, float a, float b, int e, bool abs) { return Op(kTVal, id, a, b, e, abs); }
	void Fader_FadeTo(uint64_t id, float g, float t, bool r, bool k) { Op(kFade, id, g, t, 0, (r ? 1 : 0) | (k ? 2 : 0)); }
	void Fader_Set(uint64_t id, float x, bool k) { Op(kFSet, id, x, 0, 0, k); }
	void Fader_SetPaused(uint64_t id, bool b) { Op(kFPause, id, 0, 0, 0, b); }
	float Fader_GetValue(uint64_t id, float a, float b, int e, bool abs) { return Op(kFVal, id, a, b, e, abs); }
	void ClearAll() { Op(kClear, 0, 0, 0, 0, 0); }
	void UpdateTimers(float dt) { s.UpdateTimers(dt); }
	bool TimersIdle() { return s.TimersIdle(); }
	tString DescribeTimers()
	{
		tString sOut;
		ohpl_s5_ts_describe(&s, +[](void *apCtx, const unsigned char *apData, size_t alLen) { ((tString *)apCtx)->assign((const char *)apData, alLen); }, &sOut);
		return sOut;
	}
};
}

void OhplS5SelfTest()
{
	TsOps s;
	const uint64_t vIds[] = {1, 2, 3, SomaHash64("a"), SomaHash64("bb")};
	const char *vFuncs[] = {"", "F", "G"};
	for (int i = 0; i < 600; ++i)
	{
		uint64_t id = vIds[Rnd() % 5];
		switch (Rnd() % 16)
		{
		case 0:
		case 1:
		{
			float t = RndF() * 2;
			const char *f = vFuncs[Rnd() % 3];
			bool c = Rnd() & 1, r = Rnd() & 1;
			s.Timer_Add(id, t, f, c, r);
			OhplS5Log("st add %llu %.9g %s %d %d", (unsigned long long)id, t, f, c, r);
			break;
		}
		case 2: s.Timer_Remove(id); OhplS5Log("st rem %llu", (unsigned long long)id); break;
		case 3: OhplS5Log("st exists %llu %d", (unsigned long long)id, s.Timer_Exists(id)); break;
		case 4: OhplS5Log("st left %llu %.9g", (unsigned long long)id, s.Timer_GetTimeLeft(id)); break;
		case 5:
		{
			bool b = Rnd() & 1;
			s.Timer_SetPaused(id, b);
			OhplS5Log("st pause %llu %d", (unsigned long long)id, b);
			break;
		}
		case 6:
		{
			float l = RndF() * 2;
			OhplS5Log("st passed %llu %.9g %d", (unsigned long long)id, l, s.Timer_TimeHasPassed(id, l));
			break;
		}
		case 7:
		{
			int e = Rnd() % 31;
			bool a = Rnd() & 1;
			OhplS5Log("st tval %llu %d %d %.9g", (unsigned long long)id, e, a, s.Timer_GetValue(id, -1, 3, e, a));
			break;
		}
		case 8:
		{
			float g = RndF() * 4 - 1, t = Rnd() % 4 == 0 ? 0 : RndF() * 2;
			bool r = Rnd() & 1, k = Rnd() & 1;
			s.Fader_FadeTo(id, g, t, r, k);
			OhplS5Log("st fade %llu %.9g %.9g %d %d", (unsigned long long)id, g, t, r, k);
			break;
		}
		case 9:
		{
			float x = RndF() * 2 - 1;
			bool k = Rnd() & 1;
			s.Fader_Set(id, x, k);
			OhplS5Log("st fset %llu %.9g %d", (unsigned long long)id, x, k);
			break;
		}
		case 10:
		{
			bool b = Rnd() & 1;
			s.Fader_SetPaused(id, b);
			OhplS5Log("st fpause %llu %d", (unsigned long long)id, b);
			break;
		}
		case 11:
		{
			int e = Rnd() % 31;
			bool a = Rnd() & 1;
			OhplS5Log("st fval %llu %d %d %.9g", (unsigned long long)id, e, a, s.Fader_GetValue(id, 0, 1, e, a));
			break;
		}
		case 12:
		case 13:
		case 14:
		{
			float dt = RndF() * 0.5f;
			s.UpdateTimers(dt);
			OhplS5Log("st upd %.9g idle=%d", dt, s.TimersIdle());
			break;
		}
		default:
			if (Rnd() % 8 == 0)
			{
				s.ClearAll();
				OhplS5Log("st clear");
			}
			break;
		}
		OhplS5Log("st state %s", s.DescribeTimers().c_str());
	}
}
