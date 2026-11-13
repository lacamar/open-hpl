// a5_harness cpp|rust: seeded steps through the pre-A5 shell composition (cpp) or ohpl_a5_* (rust).
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
static bool gRust;

static void ButtonSet(OhplA5Button &s, bool nw, bool fx)
{
	int c;
	if(ohpl_ga4b_button_set(&s.on, nw, &c) == false) return;
	gLog.push_back(OpLine(kOhplA5Op_ConnState, c, 0, 0));
	gLog.push_back(OpLine(kOhplA5Op_SetEffects, s.on, fx, 0));
}

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
		if(k == 0)
		{
			if(gRust) ohpl_a5_button_interact(&st, &gSink); else ButtonSet(st, !st.on, true);
		}
		else if(k == 1)
		{
			if(gRust) ohpl_a5_button_set_on(&st, a, false, &gSink); else ButtonSet(st, a, false);
		}
		else if(k == 2)
		{
			if(gRust) ohpl_a5_button_conn(&st, c, &gSink);
			else
			{
				int sw = ohpl_ga4b_button_conn_state(c);
				if(sw == 1) ButtonSet(st, true, true);
				else if(sw == 0) ButtonSet(st, false, true);
			}
		}
		else { st.can_on = a; st.can_off = !a; }
		bool ci = gRust ? ohpl_a5_button_can_interact(&st) : ohpl_ga4b_button_can_interact(st.on, st.can_on, st.can_off);
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
		if(gRust)
		{
			OhplA5PhotoHost host = { &p, &PBounds, &PLight, &PIllum };
			ohpl_a5_photocell_update(&st, dt, &host, &gSink);
		}
		else
		{
			if(ohpl_ga4b_photo_tick(&st.check_count, dt))
			{
				st.light_level = ohpl_ga4b_photo_level(p.lv, 6);
				int c = ohpl_ga4b_photo_switch(st.light_level, &st.lit, st.on_limit, st.off_limit);
				if(c != 0) gLog.push_back(OpLine(kOhplA5Op_ConnState, c, 0, 0));
			}
			float goal = ohpl_ga4b_photo_goal(st.light_level, st.on_limit, st.off_limit);
			if(p.has && p.illum != goal)
				gLog.push_back(OpLine(kOhplA5Op_PhotoIllum, 0, 0, ohpl_ga4b_photo_illum_step(p.illum, goal, dt)));
		}
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
			if(gRust) ohpl_a5_door_update(&st, dt); else st.locked_count = ohpl_ga4b_door_tick(st.locked_count, dt);
		}
		gLog.clear();
		if(gRust) ohpl_a5_door_interact(&st, &gSink);
		else
		{
			OhplGa4bDoorAct act;
			ohpl_ga4b_door_interact(st.locked, st.locked_count, &act);
			if(act.locked_feedback) { gLog.push_back(OpLine(kOhplA5Op_DoorLockedFeedback, 0, 0, 0)); st.locked_count = act.count; }
			if(act.change_map) gLog.push_back(OpLine(kOhplA5Op_DoorChangeMap, 0, 0, 0));
		}
		Emit("d " + Bits(st.locked_count), Join(gLog));
	}
}

struct OilCtx { bool hard; float lamp; float total; };
static bool OHard(void *c) { return ((OilCtx *)c)->hard; }
static float OLamp(void *c) { return ((OilCtx *)c)->lamp; }
static float OTotal(void *c) { return ((OilCtx *)c)->total; }

static void CalcOil(OhplA5Oil &s, OilCtx &h)
{
	if(s.calculated) return;
	if(h.hard) { s.amount = 10.0f; s.calculated = true; return; }
	s.amount = ohpl_ga4b_oil_amount(h.total);
	s.calculated = true;
}

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
			bool res;
			if(gRust) res = ohpl_a5_oil_interact(&st, &host, &gSink);
			else
			{
				CalcOil(st, h);
				OhplGa4bOilFill fill;
				ohpl_ga4b_oil_fill(st.amount, h.lamp, &fill);
				if(fill.result == kOhplGa4bOil_Empty)
				{
					gLog.push_back(OpLine(kOhplA5Op_OilSound, kOhplA5Snd_Empty, 0, 0));
					gLog.push_back(OpLine(kOhplA5Op_OilMessage, kOhplA5Msg_Empty, 0, 0));
					res = false;
				}
				else if(fill.result == kOhplGa4bOil_Full)
				{
					gLog.push_back(OpLine(kOhplA5Op_OilMessage, kOhplA5Msg_LanternFull, 0, 0));
					res = false;
				}
				else
				{
					st.amount = fill.barrel;
					gLog.push_back(OpLine(kOhplA5Op_OilSetLamp, 0, 0, fill.lamp));
					gLog.push_back(OpLine(kOhplA5Op_OilSound, kOhplA5Snd_Fill, 0, 0));
					int msg = fill.tier == 0 ? kOhplA5Msg_RefillFull : fill.tier == 1 ? kOhplA5Msg_Refill025 : fill.tier == 2 ? kOhplA5Msg_Refill050 : fill.tier == 3 ? kOhplA5Msg_Refill075 : kOhplA5Msg_Refill100;
					gLog.push_back(OpLine(kOhplA5Op_OilMessage, msg, 0, 0));
					res = true;
				}
			}
			if(r.b()) h.lamp = r.f(50.0f);
			int tier;
			if(gRust) tier = ohpl_a5_oil_focus_tier(&st, &host);
			else { CalcOil(st, h); tier = ohpl_ga4b_oil_focus_tier(st.amount); }
			Emit("o " + B(res) + " " + Bits(st.amount) + " " + std::to_string(tier), Join(gLog));
		}
	}
}

int main(int argc, char **argv)
{
	gRust = argc > 1 && !strcmp(argv[1], "rust");
	for(uint64_t seed = 1; seed <= 8; ++seed) { Button(seed); Photocell(seed); Door(seed); Oil(seed); }
	return 0;
}
