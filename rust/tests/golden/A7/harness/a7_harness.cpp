// min==max in the random-interval areas: libc rand() is shared with parallel cargo tests.
// a7_harness cpp|rust: seeded op scripts through the std-container originals of cLuxMap (cpp) or the
// Rust-owned map arena and area blocks (rust). Same driver, two backends; traces must be byte-equal.
#include "RustLuxMapA7.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <list>
#include <map>
#include <string>
#include <vector>

struct Rng
{
	uint64_t s;
	uint32_t next() { s = s * 6364136223846793005ULL + 1442695040888963407ULL; return (uint32_t)(s >> 33); }
	float f(float scale) { return (float)(next() % 2001) / 1000.0f * scale; }
	bool b() { return next() & 1; }
	int n(int k) { return (int)(next() % k); }
};

static std::string Bits(float f) { uint32_t u; memcpy(&u, &f, 4); char b[16]; snprintf(b, sizeof b, "%08x", u); return b; }
static std::string Lower(const std::string &s) { std::string r = s; for(char &c : r) c = (char)tolower((unsigned char)c); return r; }

static std::vector<std::string> gTrace;
static void T(const std::string &s) { gTrace.push_back(s); }

struct Obj
{
	int tag;
	std::string name;
	int id;
	bool doomed;
	Obj(int t, const std::string &n, int i = 0) : tag(t), name(n), id(i), doomed(false) {}
};

static void Drop(void *p)
{
	Obj *o = static_cast<Obj*>(p);
	T("drop " + std::to_string(o->tag) + " " + o->name);
}

enum { kEnt, kDis, kLamp, kStart, kPos, kUse };
static const char *kNames[] = { "Door_A", "door_a", "Lamp_1", "LAMP_1", "x", "Y", "Door_B", "pos_1", "Pos_1", "start" };

struct TimerRec { std::string name, func; float count; bool destroy; };

class IMap
{
public:
	virtual ~IMap() {}
	virtual void AddEntity(Obj *e, int kindFlags) = 0;
	virtual Obj *FindName(const std::string &n) = 0;
	virtual Obj *FindId(int id) = 0;
	virtual bool Exists(Obj *e) = 0;
	virtual Obj *Latest() = 0;
	virtual void ResetLatest() = 0;
	virtual int FreeId() = 0;
	virtual void Doom(Obj *e) = 0;
	virtual void UpdateDoomed(Rng &r) = 0;
	virtual std::vector<Obj*> Walk(int which) = 0;
	virtual void DestroyAll() = 0;
	virtual void Teardown() = 0;
	virtual void TimerAdd(const std::string &n, float t, const std::string &f) = 0;
	virtual void TimerRemove(const std::string &n) = 0;
	virtual bool TimerGet(const std::string &n, float *c) = 0;
	virtual void TimersUpdate(float step, Rng &r) = 0;
	virtual std::vector<TimerRec> Timers() = 0;
	virtual void TimersSet(const std::vector<TimerRec> &v) = 0;
	virtual void UseAdd(Obj *o, const std::string &n, const std::string &i, const std::string &e) = 0;
	virtual bool UseRemoveName(const std::string &n) = 0;
	virtual void UseRemovePtr(Obj *o, const std::string &n) = 0;
	virtual Obj *UseFind(const std::string &i, const std::string &e) = 0;
	virtual void DissolveAdd(Obj *o, float t) = 0;
	virtual void DissolveUpdate(float step) = 0;
	virtual void LampAdd(Obj *c, const void *light) = 0;
	virtual Obj *LampFind(const void *light) = 0;
	virtual void LampUpdate(float step) = 0;
	virtual void StartAdd(Obj *o, bool pos) = 0;
	virtual Obj *StartFind(const std::string &n) = 0;
	virtual Obj *PosFind(const std::string &n) = 0;
	virtual Obj *StartFirst() = 0;
	virtual std::vector<Obj*> Starts() = 0;
	virtual bool Commentary(bool show) = 0;
};

static void Script(IMap &m, Rng &r, const std::string &func, const std::string &name, bool updating);

// ---------------------------------------------------------------- the std-container originals
class RefMap : public IMap
{
	std::list<Obj*> ents, enemies, stickies, doomed;
	std::multimap<std::string, Obj*> byName;
	std::multimap<int, Obj*> byId;
	Obj *latest = NULL;
	bool deleting = false, updating = false, commentary = false;
	std::list<TimerRec*> timers;
	std::list<std::pair<Obj*, std::vector<std::string>>> uses;
	struct Dis { Obj *o; float speed, alpha; };
	std::list<Dis> dis;
	std::list<std::pair<Obj*, const void*>> lamps;
	std::multimap<std::string, Obj*> starts, poss;
	std::vector<Obj*> startOrder;
	std::map<Obj*, int> flags;

	static void Remove(std::list<Obj*> &l, Obj *o) { for(auto it = l.begin(); it != l.end(); ++it) if(*it == o) { l.erase(it); return; } }
	static void RemoveMap(std::multimap<std::string, Obj*> &m, Obj *o) { for(auto it = m.begin(); it != m.end(); ++it) if(it->second == o) { m.erase(it); return; } }
	static void RemoveMapI(std::multimap<int, Obj*> &m, Obj *o) { for(auto it = m.begin(); it != m.end(); ++it) if(it->second == o) { m.erase(it); return; } }

public:
	void AddEntity(Obj *e, int kf) override
	{
		byName.insert({ Lower(e->name), e });
		byId.insert({ e->id, e });
		ents.push_back(e);
		latest = e;
		flags[e] = kf;
		bool enemy = kf & 1, sticky = kf & 2;
		if(enemy) enemies.push_back(e);
		else if(sticky) stickies.push_back(e);
	}
	Obj *FindName(const std::string &n) override { auto it = byName.find(Lower(n)); return it == byName.end() ? NULL : it->second; }
	Obj *FindId(int id) override { auto it = byId.find(id); return it == byId.end() ? NULL : it->second; }
	bool Exists(Obj *e) override { for(Obj *o : ents) if(o == e) return true; return false; }
	Obj *Latest() override { return latest; }
	void ResetLatest() override { latest = NULL; }
	int FreeId() override
	{
		std::vector<int> ids;
		for(auto &p : byId) ids.push_back(p.first);
		return ohpl_ga5_free_entity_id(ids.data(), ids.size());
	}
	void Doom(Obj *e) override { if(e->doomed) return; e->doomed = true; doomed.push_back(e); }
	void UpdateDoomed(Rng &r) override
	{
		for(auto it = doomed.begin(); it != doomed.end(); ++it)
		{
			Obj *e = *it;
			T("doomed " + e->name);
			if(r.n(4) == 0)
			{
				std::vector<Obj*> all(ents.begin(), ents.end());
				if(!all.empty()) Doom(all[r.n((int)all.size())]);
			}
			Remove(ents, e);
			RemoveMap(byName, e);
			RemoveMapI(byId, e);
			int kf = flags[e];
			if(kf & 1) Remove(enemies, e);
			else if(kf & 2) Remove(stickies, e);
			Drop(e);
		}
		doomed.clear();
	}
	std::vector<Obj*> Walk(int which) override
	{
		const std::list<Obj*> &l = which == 0 ? ents : which == 1 ? enemies : stickies;
		return std::vector<Obj*>(l.begin(), l.end());
	}
	void DestroyAll() override
	{
		for(TimerRec *t : timers) delete t;
		timers.clear();
		for(auto &p : lamps) Drop(p.first);
		lamps.clear();
		deleting = true;
		for(Obj *e : ents) Drop(e);
		ents.clear();
		deleting = false;
		byId.clear(); byName.clear(); doomed.clear(); latest = NULL; enemies.clear(); stickies.clear();
		commentary = false;
	}
	void Teardown() override
	{
		for(TimerRec *t : timers) delete t;
		timers.clear();
		for(auto &p : lamps) Drop(p.first);
		lamps.clear();
		deleting = true;
		for(Obj *e : ents) Drop(e);
		deleting = false;
		for(auto &p : starts) Drop(p.second);
		for(auto &p : poss) Drop(p.second);
		for(auto &u : uses) Drop(u.first);
		for(auto &d : dis) Drop(d.o);
	}
	void TimerAdd(const std::string &n, float t, const std::string &f) override
	{
		TimerRec *r = new TimerRec{ n, f, ohpl_ga5_timer_count(t), false };
		timers.push_back(r);
	}
	void TimerRemove(const std::string &n) override
	{
		for(auto it = timers.begin(); it != timers.end(); )
		{
			TimerRec *t = *it;
			if(t->name == n)
			{
				if(ohpl_ga5_timer_remove_marks(updating)) { t->destroy = true; ++it; }
				else { it = timers.erase(it); delete t; }
			}
			else ++it;
		}
	}
	bool TimerGet(const std::string &n, float *c) override
	{
		for(TimerRec *t : timers) if(t->name == n) { *c = t->count; return true; }
		return false;
	}
	void TimersUpdate(float step, Rng &r) override
	{
		updating = true;
		for(TimerRec *t : timers) t->count = ohpl_ga5_timer_step(t->count, step);
		for(auto it = timers.begin(); it != timers.end(); )
		{
			TimerRec *t = *it;
			if(ohpl_ga5_timer_due(t->count, t->destroy))
			{
				Script(*this, r, t->func, t->name, true);
				it = timers.erase(it);
				delete t;
			}
			else ++it;
		}
		for(auto it = timers.begin(); it != timers.end(); )
		{
			if((*it)->destroy) { delete *it; it = timers.erase(it); }
			else ++it;
		}
		updating = false;
	}
	std::vector<TimerRec> Timers() override { std::vector<TimerRec> v; for(TimerRec *t : timers) v.push_back(*t); return v; }
	void TimersSet(const std::vector<TimerRec> &v) override
	{
		for(TimerRec *t : timers) delete t;
		timers.clear();
		for(const TimerRec &t : v) timers.push_back(new TimerRec(t));
	}
	void UseAdd(Obj *o, const std::string &n, const std::string &i, const std::string &e) override { uses.push_back({ o, { n, i, e } }); }
	bool UseRemoveName(const std::string &n) override
	{
		for(auto it = uses.begin(); it != uses.end(); ++it)
			if(it->second[0] == n) { Drop(it->first); uses.erase(it); return true; }
		return false;
	}
	void UseRemovePtr(Obj *o, const std::string &n) override
	{
		for(auto it = uses.begin(); it != uses.end(); ++it)
		{
			if(it->first != o) continue;
			if(ohpl_ga5_callback_remove_ok(n.data(), n.size(), it->second[0].data(), it->second[0].size()) == false) break;
			Drop(o);
			uses.erase(it);
			break;
		}
	}
	Obj *UseFind(const std::string &i, const std::string &e) override
	{
		for(auto &u : uses)
			if(ohpl_ga5_use_item_matches(u.second[1].data(), u.second[1].size(), u.second[2].data(), u.second[2].size(), i.data(), i.size(), e.data(), e.size())) return u.first;
		return NULL;
	}
	void DissolveAdd(Obj *o, float t) override { dis.push_back({ o, ohpl_ga5_dissolve_speed(t), 1.0f }); }
	void DissolveUpdate(float step) override
	{
		for(auto it = dis.begin(); it != dis.end(); )
		{
			bool rm = ohpl_ga5_dissolve_step(&it->alpha, it->speed, step);
			T("cover " + it->o->name + " " + Bits(it->alpha));
			if(rm) { Drop(it->o); it = dis.erase(it); }
			else ++it;
		}
	}
	void LampAdd(Obj *c, const void *l) override { lamps.push_back({ c, l }); }
	Obj *LampFind(const void *l) override { for(auto &p : lamps) if(p.second == l) return p.first; return NULL; }
	void LampUpdate(float step) override { for(auto &p : lamps) T("lamp " + p.first->name + " " + Bits(step)); }
	void StartAdd(Obj *o, bool pos) override
	{
		if(pos) poss.insert({ o->name, o });
		else { starts.insert({ o->name, o }); startOrder.push_back(o); }
	}
	Obj *StartFind(const std::string &n) override { auto it = starts.find(n); return it == starts.end() ? NULL : it->second; }
	Obj *PosFind(const std::string &n) override { auto it = poss.find(n); return it == poss.end() ? NULL : it->second; }
	Obj *StartFirst() override { return starts.empty() ? NULL : starts.begin()->second; }
	std::vector<Obj*> Starts() override { return startOrder; }
	bool Commentary(bool show) override
	{
		int n = ohpl_ga5_commentary_sync(commentary, show);
		if(n < 0) return false;
		commentary = n != 0;
		return true;
	}
};

// ---------------------------------------------------------------- the Rust-owned arena
static void SinkS(void *ctx, const unsigned char *p, size_t n) { static_cast<std::string*>(ctx)->assign((const char*)p, n); }
static void DissolveCb(void *, void *o, float a, bool) { T("cover " + static_cast<Obj*>(o)->name + " " + Bits(a)); }
static void LampCb(void *, void *c) {}
static float gLampStep;
static void LampCb2(void *, void *c) { T("lamp " + static_cast<Obj*>(c)->name + " " + Bits(gLampStep)); }

static void *Typed(Obj *e) { return reinterpret_cast<char*>(e) + 8; }
static Obj *FromTyped(void *p) { return p ? reinterpret_cast<Obj*>(reinterpret_cast<char*>(p) - 8) : NULL; }

class RustMap : public IMap
{
	OhplA7Ctx *c;
	std::map<Obj*, int> flags;
public:
	RustMap()
	{
		c = ohpl_a7_new();
		for(unsigned k = 0; k < 6; ++k) ohpl_a7_set_drop(c, k, Drop);
	}
	~RustMap() { ohpl_a7_free(c); }
	void AddEntity(Obj *e, int kf) override
	{
		flags[e] = kf;
		bool enemy = kf & 1, sticky = kf & 2;
		ohpl_a7_ent_add(c, e, Typed(e), OHPL_A7_S(e->name), e->id, enemy, enemy ? false : sticky, sticky);
	}
	Obj *FindName(const std::string &n) override { return static_cast<Obj*>(ohpl_a7_ent_find_name(c, OHPL_A7_S(n))); }
	Obj *FindId(int id) override { return static_cast<Obj*>(ohpl_a7_ent_find_id(c, id)); }
	bool Exists(Obj *e) override { return ohpl_a7_ent_exists(c, e); }
	Obj *Latest() override { return static_cast<Obj*>(ohpl_a7_ent_latest(c)); }
	void ResetLatest() override { ohpl_a7_ent_reset_latest(c); }
	int FreeId() override { return ohpl_a7_ent_free_id(c); }
	void Doom(Obj *e) override { if(e->doomed) return; e->doomed = true; ohpl_a7_doom_push(c, e); }
	void UpdateDoomed(Rng &r) override
	{
		uint64_t cur = ohpl_a7_cur_begin(c, kOhplA7Doomed);
		while(Obj *e = static_cast<Obj*>(ohpl_a7_cur_get(c, kOhplA7Doomed, cur)))
		{
			T("doomed " + e->name);
			if(r.n(4) == 0)
			{
				std::vector<Obj*> all = Walk(0);
				if(!all.empty()) Doom(all[r.n((int)all.size())]);
			}
			int kf = flags[e];
			bool enemy = kf & 1, sticky = kf & 2;
			ohpl_a7_ent_unlink(c, e, Typed(e), enemy, enemy ? false : sticky, sticky);
			Drop(e);
			cur = ohpl_a7_cur_next(c, kOhplA7Doomed, cur);
		}
		ohpl_a7_doom_clear(c);
	}
	std::vector<Obj*> Walk(int which) override
	{
		static const unsigned kW[] = { kOhplA7Entities, kOhplA7Enemies, kOhplA7StickyList };
		std::vector<Obj*> v;
		for(uint64_t cur = ohpl_a7_cur_begin(c, kW[which]); void *p = ohpl_a7_cur_get(c, kW[which], cur); cur = ohpl_a7_cur_next(c, kW[which], cur))
			v.push_back(which == 0 ? static_cast<Obj*>(p) : FromTyped(p));
		return v;
	}
	void DestroyAll() override { ohpl_a7_destroy_all(c); }
	void Teardown() override { ohpl_a7_teardown(c); }
	void TimerAdd(const std::string &n, float t, const std::string &f) override { ohpl_a7_timer_add(c, OHPL_A7_S(n), t, OHPL_A7_S(f)); }
	void TimerRemove(const std::string &n) override { ohpl_a7_timer_remove(c, OHPL_A7_S(n)); }
	bool TimerGet(const std::string &n, float *o) override { return ohpl_a7_timer_get(c, OHPL_A7_S(n), o); }
	void TimersUpdate(float step, Rng &r) override
	{
		ohpl_a7_timers_begin(c, step);
		size_t cursor = 0;
		std::string n, f;
		while(ohpl_a7_timer_poll(c, &cursor, SinkS, &n, &f))
		{
			Script(*this, r, f, n, true);
			ohpl_a7_timer_finish(c, cursor);
		}
		ohpl_a7_timers_end(c);
	}
	std::vector<TimerRec> Timers() override
	{
		std::vector<TimerRec> v;
		for(size_t i = 0; i < ohpl_a7_timer_len(c); ++i)
		{
			TimerRec t; bool d;
			ohpl_a7_timer_at(c, i, SinkS, &t.name, &t.func, &t.count, &d);
			t.destroy = d;
			v.push_back(t);
		}
		return v;
	}
	void TimersSet(const std::vector<TimerRec> &v) override
	{
		ohpl_a7_timers_clear(c);
		for(const TimerRec &t : v) ohpl_a7_timer_push(c, OHPL_A7_S(t.name), OHPL_A7_S(t.func), t.count, t.destroy);
	}
	void UseAdd(Obj *o, const std::string &n, const std::string &i, const std::string &e) override { ohpl_a7_use_add(c, o, OHPL_A7_S(n), OHPL_A7_S(i), OHPL_A7_S(e)); }
	bool UseRemoveName(const std::string &n) override { return ohpl_a7_use_remove_name(c, OHPL_A7_S(n)); }
	void UseRemovePtr(Obj *o, const std::string &n) override { ohpl_a7_use_remove_ptr(c, o, OHPL_A7_S(n)); }
	Obj *UseFind(const std::string &i, const std::string &e) override { return static_cast<Obj*>(ohpl_a7_use_find(c, OHPL_A7_S(i), OHPL_A7_S(e))); }
	void DissolveAdd(Obj *o, float t) override { ohpl_a7_dissolve_add(c, o, t); }
	void DissolveUpdate(float step) override { ohpl_a7_dissolve_update(c, step, DissolveCb, NULL); }
	void LampAdd(Obj *o, const void *l) override { ohpl_a7_lamp_add(c, o, l); }
	Obj *LampFind(const void *l) override { return static_cast<Obj*>(ohpl_a7_lamp_find(c, l)); }
	void LampUpdate(float step) override { gLampStep = step; ohpl_a7_lamp_each(c, LampCb2, NULL); }
	void StartAdd(Obj *o, bool pos) override
	{
		if(pos) ohpl_a7_pos_add(c, o, OHPL_A7_S(o->name));
		else ohpl_a7_start_add(c, o, OHPL_A7_S(o->name));
	}
	Obj *StartFind(const std::string &n) override { return static_cast<Obj*>(ohpl_a7_start_find(c, OHPL_A7_S(n))); }
	Obj *PosFind(const std::string &n) override { return static_cast<Obj*>(ohpl_a7_pos_find(c, OHPL_A7_S(n))); }
	Obj *StartFirst() override { return static_cast<Obj*>(ohpl_a7_start_first(c)); }
	std::vector<Obj*> Starts() override
	{
		std::vector<Obj*> v;
		for(size_t i = 0; i < ohpl_a7_start_num(c); ++i) v.push_back(static_cast<Obj*>(ohpl_a7_start_at(c, i)));
		return v;
	}
	bool Commentary(bool show) override
	{
		int n = ohpl_a7_commentary_sync(c, show);
		return n >= 0;
	}
};

static std::string N(Obj *o) { return o ? o->name + "#" + std::to_string(o->id) : "-"; }
static std::string Fl(float f) { return Bits(f); }

static std::vector<Obj*> gPool;
static int gSerial;

// Stand-in for RunScript: seeded effects on the map, drawn in call order.
static void Script(IMap &m, Rng &r, const std::string &func, const std::string &name, bool updating)
{
	T("script " + func + "(" + name + ")");
	switch(r.n(6))
	{
	case 0: { std::string cn = "cb" + std::to_string(gSerial++); float ct = r.f(0.05f); std::string cf = "fn" + std::to_string(r.n(3)); m.TimerAdd(cn, ct, cf); } break;
	case 1: m.TimerRemove(kNames[r.n(10)]); break;
	case 2: m.TimerRemove(name); break;
	case 3: { float c; bool ok = m.TimerGet(kNames[r.n(10)], &c); T(std::string("get ") + (ok ? Fl(c) : "none")); } break;
	default: break;
	}
}

static void Run(IMap &m, uint64_t seed)
{
	Rng r = { seed };
	std::vector<Obj*> live, start;
	int steps = 400;
	for(int n = 0; n < steps; ++n)
	{
		int k = r.n(22);
		std::string head = "s" + std::to_string(n) + " k" + std::to_string(k) + " ";
		switch(k)
		{
		case 0: case 1: case 2:
		{
			std::string en = kNames[r.n(10)];
			bool fresh = r.b();
			Obj *e = new Obj(kEnt, en, fresh ? m.FreeId() : r.n(6));
			int kf = r.n(5) == 0 ? 1 : r.n(5) == 0 ? 2 : 0;
			gPool.push_back(e);
			m.AddEntity(e, kf);
			live.push_back(e);
			T(head + "add " + N(e) + " kf" + std::to_string(kf) + " latest " + N(m.Latest()));
		}
		break;
		case 3: case 4:
		{
			bool low = r.b();
			std::string fn = kNames[r.n(10)];
			Obj *byName = m.FindName(low ? Lower(fn) : fn);
			T(head + "find " + N(byName) + " " + N(m.FindId(r.n(8))));
		}
			break;
		case 5:
			if(!live.empty()) { Obj *e = live[r.n((int)live.size())]; T(head + "exists " + std::to_string(m.Exists(e)) + " " + N(e)); }
			break;
		case 6: T(head + "freeid " + std::to_string(m.FreeId())); break;
		case 7:
			if(r.n(4) == 0) { m.ResetLatest(); T(head + "reset " + N(m.Latest())); }
			break;
		case 8: case 9:
			if(!live.empty()) { Obj *e = live[r.n((int)live.size())]; m.Doom(e); T(head + "doom " + N(e)); }
			break;
		case 10:
			m.UpdateDoomed(r);
			{
				std::vector<Obj*> now;
				for(Obj *o : m.Walk(0)) now.push_back(o);
				live = now;
			}
			T(head + "after doomed live=" + std::to_string(live.size()));
			break;
		case 11:
		{
			std::string s = head + "walk";
			for(int w = 0; w < 3; ++w) { s += " |"; for(Obj *o : m.Walk(w)) s += " " + N(o); }
			T(s);
		}
		break;
		case 12: case 13:
		{
			std::string tn = kNames[r.n(10)];
			float tt = r.f(0.2f);
			if(r.n(4) == 0) tt -= 0.1f;
			std::string tf = "fn" + std::to_string(r.n(3));
			m.TimerAdd(tn, tt, tf);
			T(head + "timer+");
		}
			break;
		case 14:
			m.TimerRemove(kNames[r.n(10)]);
			{ std::string s = head + "timer-"; for(auto &t : m.Timers()) s += " " + t.name + ":" + Fl(t.count) + (t.destroy ? "!" : ""); T(s); }
			break;
		case 15:
		{
			float step = r.f(0.05f);
			m.TimersUpdate(step, r);
			std::string s = head + "tick";
			for(auto &t : m.Timers()) s += " " + t.name + ":" + t.func + ":" + Fl(t.count) + (t.destroy ? "!" : "");
			T(s);
		}
		break;
		case 16:
		{
			Obj *o = new Obj(kUse, "use" + std::to_string(gSerial++));
			gPool.push_back(o);
			std::string i = kNames[r.n(4)], e = kNames[r.n(4)];
			m.UseAdd(o, o->name, i, e);
			T(head + "use+ " + o->name + " " + i + " " + e + " find " + N(m.UseFind(i, e)));
		}
		break;
		case 17:
		{
			if(r.b()) T(head + "use-name " + std::to_string(m.UseRemoveName("use" + std::to_string(r.n(gSerial + 1)))));
			else
			{
				std::string ui = kNames[r.n(4)];
				std::string ue = kNames[r.n(4)];
				Obj *o = m.UseFind(ui, ue);
				if(o) { m.UseRemovePtr(o, r.b() ? o->name : "other"); T(head + "use-ptr " + o->name + " again " + N(m.UseFind(kNames[0], kNames[0]))); }
			}
		}
		break;
		case 18:
		{
			Obj *o = new Obj(kDis, "dis" + std::to_string(gSerial++));
			gPool.push_back(o);
			m.DissolveAdd(o, r.f(0.3f));
			if(r.n(3) == 0) { for(int i = 0; i < 3; ++i) m.DissolveUpdate(r.f(0.2f)); }
			T(head + "dissolve");
		}
		break;
		case 19:
		{
			static char lights[8];
			Obj *o = new Obj(kLamp, "lamp" + std::to_string(gSerial++));
			gPool.push_back(o);
			const void *l = &lights[r.n(8)];
			if(m.LampFind(l) == NULL) m.LampAdd(o, l);
			T(head + "lamp find " + N(m.LampFind(l)));
			gLampStep = 0;
			m.LampUpdate(r.f(0.05f));
		}
		break;
		case 20:
		{
			bool pos = r.b();
			Obj *o = new Obj(pos ? kPos : kStart, kNames[r.n(10)]);
			gPool.push_back(o);
			m.StartAdd(o, pos);
			std::string sf = kNames[r.n(10)];
			std::string pf = kNames[r.n(10)];
			std::string s = head + (pos ? "pos+ " : "start+ ") + N(o) + " first " + N(m.StartFirst()) + " find " + N(m.StartFind(sf)) + " " + N(m.PosFind(pf)) + " order";
			for(Obj *x : m.Starts()) s += " " + x->name;
			T(s);
		}
		break;
		default:
		{
			bool show = r.b();
			T(head + "commentary " + std::to_string(m.Commentary(show)));
			if(r.n(40) == 0)
			{
				std::vector<TimerRec> saved = m.Timers();
				for(auto &t : saved) t.count += 1.0f;
				m.TimersSet(saved);
				T(head + "timers-set " + std::to_string(m.Timers().size()));
			}
			if(r.n(60) == 0) { m.DestroyAll(); live.clear(); T(head + "destroy-all"); }
		}
		break;
		}
	}
	m.Teardown();
	T("teardown");
}

// ---------------------------------------------------------------- areas
// cpp: the plain structs the C++ shells held before, driven through the same ohpl_ga5_* the shells call.
// rust: the Rust-owned blocks.
static std::string V(const OhplGa5Vec3 &v) { return Bits(v.x) + "," + Bits(v.y) + "," + Bits(v.z); }

static void Areas(bool rust, uint64_t seed)
{
	Rng r = { seed };
	OhplA7Ctx *ctx = ohpl_a7_new();
	for(int round = 0; round < 40; ++round)
	{
		int kind = round % 9;
		OhplA7Area *a = rust ? ohpl_a7_area_new(ctx, kind) : NULL;
		static OhplA7Insanity ins; static OhplA7Flashback fb; static OhplA7Slime sl; static OhplA7Examine ex; static OhplA7Liquid lq;
		static OhplA7Ladder ld; static OhplA7Sticky st; static OhplA7Script sc; static OhplA7Base base;
		memset(&ins, 0, sizeof ins); memset(&fb, 0, sizeof fb); memset(&sl, 0, sizeof sl); memset(&ex, 0, sizeof ex);
		memset(&lq, 0, sizeof lq); memset(&ld, 0, sizeof ld); memset(&st, 0, sizeof st); memset(&sc, 0, sizeof sc); memset(&base, 0, sizeof base);
		OhplA7Base *b = rust ? ohpl_a7_area_base(a) : &base;
		void *pod = rust ? ohpl_a7_area_pod(a) : NULL;
		std::string head = "area " + std::to_string(round) + " kind " + std::to_string(kind);
		const int tbl[4] = { 10, 11, 12, 13 };
		float engine = 1.0f / 60.0f;
		switch(kind)
		{
		case kOhplA7Insanity:
		{
			OhplA7Insanity &s = rust ? *static_cast<OhplA7Insanity*>(pod) : ins;
			s.checkTimeMin = 0.25f; s.checkTimeMax = 0.25f; s.checkCollisionCount = r.f(0.3f);
			for(int i = 0; i < 60; ++i)
			{
				float step = r.f(0.05f);
				bool go = rust ? ohpl_a7_area_tick(a, step, engine) : ohpl_ga5_tick_random(&s.checkCollisionCount, step, s.checkTimeMin, s.checkTimeMax);
				T(head + " tick " + std::to_string(go) + " " + Bits(s.checkCollisionCount));
			}
		}
		break;
		case kOhplA7Slime:
		{
			OhplA7Slime &s = rust ? *static_cast<OhplA7Slime*>(pod) : sl;
			s.minCheckAttackTime = 0.5f; s.maxCheckAttackTime = 0.5f; s.checkCollisionCount = r.f(0.5f);
			for(int i = 0; i < 60; ++i)
			{
				float step = r.f(0.08f);
				bool go = rust ? ohpl_a7_area_tick(a, step, engine) : ohpl_ga5_tick_random(&s.checkCollisionCount, step, s.minCheckAttackTime, s.maxCheckAttackTime);
				T(head + " tick " + std::to_string(go) + " " + Bits(s.checkCollisionCount));
			}
		}
		break;
		case kOhplA7Flashback:
		{
			OhplA7Flashback &s = rust ? *static_cast<OhplA7Flashback*>(pod) : fb;
			s.checkCollisionCount = r.f(0.2f);
			for(int i = 0; i < 60; ++i)
			{
				float step = r.b() ? engine : r.f(0.05f) / 100.0f;
				bool go;
				if(rust) go = ohpl_a7_area_tick(a, step, engine);
				else go = ohpl_ga5_step_too_small(step, engine) ? false : ohpl_ga5_tick_fixed(&s.checkCollisionCount, step, 0.1f);
				T(head + " tick " + std::to_string(go) + " " + Bits(s.checkCollisionCount));
			}
		}
		break;
		case kOhplA7Examine:
		{
			OhplA7Examine &s = rust ? *static_cast<OhplA7Examine*>(pod) : ex;
			s.insaneLimit = 30.0f;
			for(int i = 0; i < 80; ++i)
			{
				if(r.n(3) == 0)
				{
					OhplGa5ExamineIn in = { r.b(), r.b(), r.b(), r.b(), r.b() };
					OhplGa5ExamineOut out;
					float sanity = r.f(50.0f);
					if(rust) ohpl_a7_area_examine(a, &in, sanity, &out);
					else { ohpl_ga5_examine_interact(&in, sanity, s.insaneLimit, s.playSoundCount, &out); s.playSoundCount = 0.5f; }
					T(head + " interact " + std::to_string(out.showText) + std::to_string(out.insaneText) + std::to_string(out.playSound) + std::to_string(out.insaneSound) + " " + Bits(s.playSoundCount));
				}
				else
				{
					float step = r.f(0.1f);
					if(rust) ohpl_a7_area_tick(a, step, engine); else ohpl_ga5_examine_tick(&s.playSoundCount, step);
					T(head + " tick " + Bits(s.playSoundCount));
				}
			}
		}
		break;
		case kOhplA7Liquid:
		{
			OhplA7Liquid &s = rust ? *static_cast<OhplA7Liquid*>(pod) : lq;
			for(int i = 0; i < 30; ++i)
			{
				float py = r.f(10.0f), sy = r.f(4.0f);
				if(rust) ohpl_a7_area_liquid_setup(a, py, sy); else ohpl_ga5_liquid_surface_plane(py, sy, &s.surfacePlane);
				float step = r.b() ? engine : r.f(0.05f) / 100.0f;
				bool go;
				if(rust) go = ohpl_a7_area_tick(a, step, engine);
				else { go = !ohpl_ga5_step_too_small(step, engine); if(go) s.timeCount += step; }
				T(head + " liquid " + std::to_string(go) + " " + Bits(s.surfacePlane.d) + " " + Bits(s.timeCount));
			}
		}
		break;
		case kOhplA7Ladder:
		{
			OhplA7Ladder &s = rust ? *static_cast<OhplA7Ladder*>(pod) : ld;
			for(int i = 0; i < 20; ++i)
			{
				OhplGa5Mtx m;
				for(int x = 0; x < 4; ++x) for(int y = 0; y < 4; ++y) m.m[x][y] = (x == y) ? 1.0f : r.f(0.5f) - 0.5f;
				float py = r.f(10.0f), sy = r.f(4.0f);
				if(rust) ohpl_a7_area_ladder_setup(a, &m, py, sy);
				else { OhplGa5LadderSetup l; ohpl_ga5_ladder_setup(&m, py, sy, &l); s.forward = l.forward; s.maxY = l.maxY; s.minY = l.minY; }
				OhplGa5Vec3 ap = { r.f(5.0f), r.f(5.0f), r.f(5.0f) }, cs = { 0.5f, 1.8f, 0.5f }, out;
				float cy = r.f(8.0f);
				if(rust) ohpl_a7_area_ladder_start(a, &ap, &cs, cy, &out);
				else ohpl_ga5_ladder_start_pos(&ap, &s.forward, &cs, cy, s.maxY, s.minY, &out);
				T(head + " ladder " + V(s.forward) + " " + Bits(s.maxY) + " " + Bits(s.minY) + " start " + V(out));
			}
		}
		break;
		case kOhplA7Sticky:
		{
			OhplA7Sticky &s = rust ? *static_cast<OhplA7Sticky*>(pod) : st;
			s.poseTime = 0.2f; s.canDetach = r.b(); s.moveBody = r.b(); s.rotateBody = r.b(); s.setMtxTime = 1.0f;
			for(int i = 0; i < 30; ++i)
			{
				OhplGa5Mtx m;
				for(int x = 0; x < 4; ++x) for(int y = 0; y < 4; ++y) m.m[x][y] = (x == y) ? 1.0f : r.f(0.5f) - 0.5f;
				if(r.n(3) == 0)
				{
					bool grav = r.b();
					float mass = r.f(5.0f);
					int saved = r.n(3) - 1;
					if(rust) ohpl_a7_area_sticky_attach(a, &m, grav, mass, saved);
					else { s.attachedStart = m; s.setMtxTime = 0; s.attachedBodyGravity = grav; s.attachedBodyMass = mass; s.attachedEntityFullGameSaved = saved >= 0 ? saved != 0 : false; }
					T(head + " attach " + std::to_string(s.attachedBodyGravity) + " " + Bits(s.attachedBodyMass) + " " + std::to_string(s.attachedEntityFullGameSaved));
				}
				OhplGa5Vec3 ap = { r.f(3.0f), r.f(3.0f), r.f(3.0f) }, mc = { r.f(1.0f), r.f(1.0f), r.f(1.0f) }, at = { r.f(3.0f), r.f(3.0f), r.f(3.0f) };
				float step = r.f(0.1f);
				OhplGa5StickyOut out;
				memset(&out, 0, sizeof out);
				bool ran;
				if(rust) ran = ohpl_a7_area_sticky_step(a, &m, &ap, &mc, &at, step, &out);
				else
				{
					ran = !(s.setMtxTime >= 1);
					if(ran)
					{
						OhplGa5StickyIn in;
						in.start = s.attachedStart; in.areaWorld = m; in.areaPos = ap; in.massCentre = mc; in.attachedPos = at;
						in.poseTime = s.poseTime; in.step = step; in.setTime = s.setMtxTime; in.moveBody = s.moveBody; in.rotateBody = s.rotateBody;
						ohpl_ga5_sticky_attach(&in, &out);
						s.setMtxTime = out.setTime;
					}
				}
				bool cd = rust ? ohpl_a7_area_sticky_can_detach(a) : (s.canDetach && s.setMtxTime >= 1);
				T(head + " step " + std::to_string(ran) + " " + std::to_string(out.mode) + " " + Bits(out.setTime) + " " + std::to_string(out.done) + " " + Bits(out.mtx.m[0][0]) + " " + V(out.pos) + " detach " + std::to_string(cd));
			}
		}
		break;
		default:
		{
			OhplA7Script &s = rust ? *static_cast<OhplA7Script*>(pod) : sc;
			s.itemInteraction = r.b();
			for(int i = 0; i < 20; ++i)
			{
				bool cb = r.b();
				int custom = r.n(4) + 10;
				bool ci = rust ? ohpl_a7_area_can_interact(a, cb) : (kind == kOhplA7Script ? ohpl_ga5_script_can_interact(s.itemInteraction, cb) : true);
				int cross = rust ? ohpl_a7_area_crosshair(a, cb, custom, &tbl) : (kind == kOhplA7Script ? ohpl_ga5_script_crosshair(cb, custom, tbl[0], tbl[1]) : tbl[0]);
				bool oi = kind == kOhplA7Script ? (rust ? ohpl_a7_area_on_interact(a, cb) : cb) : false;
				T(head + " iface " + std::to_string(ci) + " " + std::to_string(cross) + " " + std::to_string(oi));
			}
		}
		break;
		}
		OhplGa5Vec3 bp = { r.f(3.0f), r.f(3.0f), r.f(3.0f) }, pp = { r.f(3.0f), r.f(3.0f), r.f(3.0f) }, mv;
		if(rust) { ohpl_a7_area_attach(a, &bp, &pp); ohpl_a7_area_follow(a, &pp, &bp, &mv); }
		else { ohpl_ga5_attach_offset(&bp, &pp, &b->relOffset); ohpl_ga5_follow_offset(&b->relOffset, &pp, &bp, &mv); }
		T(head + " follow " + V(b->relOffset) + " " + V(mv));
		if(rust) ohpl_a7_area_free(ctx, a);
	}
	ohpl_a7_teardown(ctx);
	ohpl_a7_free(ctx);
}

// cLuxMapHandler's map list: push_back, STLFindAndDelete, STLDeleteAll.
static void Maps(bool rust, uint64_t seed)
{
	Rng r = { seed };
	std::list<Obj*> ref;
	OhplA7Ctx *c = rust ? ohpl_a7_new() : NULL;
	if(rust) ohpl_a7_set_drop(c, kOhplA7Entity, Drop);
	std::vector<Obj*> pool;
	for(int i = 0; i < 60; ++i)
	{
		int k = r.n(5);
		if(k < 2)
		{
			Obj *o = new Obj(kEnt, "map" + std::to_string(i));
			pool.push_back(o);
			if(rust) ohpl_a7_list_add(c, o); else ref.push_back(o);
			T("load " + o->name);
		}
		else if(k < 4 && !pool.empty())
		{
			Obj *o = pool[r.n((int)pool.size())];
			if(rust) ohpl_a7_list_destroy(c, o);
			else { for(auto it = ref.begin(); it != ref.end(); ++it) if(*it == o) { ref.erase(it); break; } Drop(o); }
			T("destroy " + o->name);
		}
		else
		{
			if(rust) ohpl_a7_destroy_entities(c);
			else { for(Obj *o : ref) Drop(o); ref.clear(); }
			T("reset");
		}
	}
	if(rust) { ohpl_a7_destroy_entities(c); ohpl_a7_free(c); }
	else for(Obj *o : ref) Drop(o);
	for(Obj *o : pool) delete o;
}

int main(int argc, char **argv)
{
	bool rust = argc > 1 && strcmp(argv[1], "rust") == 0;
	for(uint64_t seed = 1; seed <= 24; ++seed)
	{
		gTrace.clear();
		gSerial = 0;
		T("seed " + std::to_string(seed));
		if(rust) { RustMap m; Run(m, seed); }
		else { RefMap m; Run(m, seed); }
		for(const std::string &s : gTrace) puts(s.c_str());
		for(Obj *o : gPool) delete o;
		gPool.clear();
	}
	for(uint64_t seed = 1; seed <= 6; ++seed)
	{
		gTrace.clear();
		T("area seed " + std::to_string(seed));
		Areas(rust, seed);
		for(const std::string &s : gTrace) puts(s.c_str());
	}
	for(uint64_t seed = 1; seed <= 4; ++seed)
	{
		gTrace.clear();
		T("maps seed " + std::to_string(seed));
		Maps(rust, seed);
		for(const std::string &s : gTrace) puts(s.c_str());
	}
	return 0;
}
