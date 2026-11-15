// a5_harness: seeded steps through ohpl_a5_*; the output is compared with ../small_props.txt.
#include "RustLuxPropA5.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct Rng
{
	uint64_t s;
	uint32_t next() { s = s * 6364136223846793005ULL + 1442695040888963407ULL; return (uint32_t)(s >> 33); }
	float f(float scale) { return (float)(next() % 2001) / 1000.0f * scale; }
	bool b() { return next() & 1; }
};

static std::string Bits(float f) { uint32_t u; memcpy(&u, &f, 4); char b[16]; snprintf(b, sizeof b, "%08x", u); return b; }
static std::string B(bool b) { return b ? "true" : "false"; }
static std::string OpLine(int code, int i, int j, float f)
{
	return "op " + std::to_string(code) + " " + std::to_string(i) + " " + std::to_string(j) + " " + Bits(f);
}
static std::string Join(const std::vector<std::string> &v)
{
	std::string s;
	for(size_t i = 0; i < v.size(); ++i) s += (i ? ";" : "") + v[i];
	return s;
}
static void Emit(const std::string &head, const std::string &tail)
{
	std::string l = head + " " + tail;
	while(!l.empty() && l.back() == ' ') l.pop_back();
	puts(l.c_str());
}

static std::vector<std::string> gLog;
static void Push(void *, const OhplGa4Op *op) { gLog.push_back(OpLine(op->code, op->i, op->j, op->f[0])); }
static OhplGa4Sink gSink = { NULL, &Push };

static void Button(uint64_t seed)
{
	Rng r = { seed };
	OhplA5Button st;
	ohpl_a5_button_init(&st);
	for(int n = 0; n < 200; ++n)
	{
		int k = r.next() % 4;
		bool a = r.b();
		int c = (int)(r.next() % 3) - 1;
		gLog.clear();
		if(k == 0) ohpl_a5_button_interact(&st, &gSink);
		else if(k == 1) ohpl_a5_button_set_on(&st, a, false, &gSink);
		else if(k == 2) ohpl_a5_button_conn(&st, c, &gSink);
		else { st.can_on = a; st.can_off = !a; }
		bool ci = ohpl_a5_button_can_interact(&st);
		if(ci != ohpl_ga4b_button_can_interact(st.on, st.can_on, st.can_off)) puts("MISMATCH can_interact");
		Emit("b " + std::to_string(k) + " " + B(st.on), Join(gLog));
	}
}

struct PhotoCtx { float lv[6]; int asked; bool has; float illum; };
static void PBounds(void *, float *size, float *center) { size[0] = 1; size[1] = 2; size[2] = 3; center[0] = 10; center[1] = 20; center[2] = 30; }
static float PLight(void *c, const float *) { PhotoCtx *p = (PhotoCtx *)c; return p->lv[(p->asked++) % 6]; }
static bool PIllum(void *c, float *out) { PhotoCtx *p = (PhotoCtx *)c; if(!p->has) return false; *out = p->illum; return true; }

static void Photocell(uint64_t seed)
{
	Rng r = { seed };
	OhplA5Photocell st = {};
	st.on_limit = r.f(0.5f);
	st.off_limit = r.f(0.25f);
	for(int n = 0; n < 200; ++n)
	{
		float dt = r.f(0.02f);
		PhotoCtx p;
		for(int i = 0; i < 6; ++i) p.lv[i] = r.f(0.5f);
		p.has = r.b();
		p.illum = p.has ? r.f(0.5f) : 0;
		p.asked = 0;
		gLog.clear();
		OhplA5PhotoHost host = { &p, &PBounds, &PLight, &PIllum };
		ohpl_a5_photocell_update(&st, dt, &host, &gSink);
		Emit("p " + Bits(st.light_level) + " " + B(st.lit), Join(gLog));
	}
}

static void Door(uint64_t seed)
{
	Rng r = { seed };
	OhplA5Door st = {};
	for(int n = 0; n < 200; ++n)
	{
		st.locked = r.b();
		if(r.b())
		{
			float dt = r.f(0.5f);
			ohpl_a5_door_update(&st, dt);
		}
		gLog.clear();
		ohpl_a5_door_interact(&st, &gSink);
		Emit("d " + Bits(st.locked_count), Join(gLog));
	}
}

struct OilCtx { bool hard; float lamp; float total; };
static bool OHard(void *c) { return ((OilCtx *)c)->hard; }
static float OLamp(void *c) { return ((OilCtx *)c)->lamp; }
static float OTotal(void *c) { return ((OilCtx *)c)->total; }

static void Oil(uint64_t seed)
{
	Rng r = { seed };
	for(int o = 0; o < 60; ++o)
	{
		OilCtx h;
		h.hard = r.next() % 4 == 0;
		h.lamp = r.f(50.0f);
		h.total = r.f(100.0f);
		OhplA5OilHost host = { &h, &OHard, &OLamp, &OTotal };
		OhplA5Oil st = {};
		for(int n = 0; n < 5; ++n)
		{
			gLog.clear();
			bool res = ohpl_a5_oil_interact(&st, &host, &gSink);
			if(r.b()) h.lamp = r.f(50.0f);
			int tier = ohpl_a5_oil_focus_tier(&st, &host);
			Emit("o " + B(res) + " " + Bits(st.amount) + " " + std::to_string(tier), Join(gLog));
		}
	}
}

int main()
{
	for(uint64_t seed = 1; seed <= 8; ++seed) { Button(seed); Photocell(seed); Door(seed); Oil(seed); }
	return 0;
}
