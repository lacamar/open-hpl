// H13 differential: cWorld entity registry, .map static-combine planner and .map parse traces.
//  [reg]  seeded create/destroy/lookup/iteration sequences: std::list/std::map oracle (the original
//         cWorld semantics) vs the Rust registry; the oracle output is the golden.
//  [walk] OhplH13Walk cursors (erase/append while walking, destroy order): std::list vs Rust cursors.
//  [walk] OhplH13Walk cursors (erase/append while walking, destroy order): std::list vs Rust cursors.
//  [plan] CombineAndCreateMeshesAndPhysics run-splitting: original loops vs ohpl_h13_plan_*.
//  [map]  every *.map under OHPL_H13_DIRS parsed through HplMapParse with a recording builder
//         (tinyxml document, C ABI marshalling); one hash per map.
// Output goes to OHPL_H13_DUMP (default stdout), summary and diffs to stderr; exit 1 on a diff.
// Env: OHPL_H13_DIRS (':'-separated roots), OHPL_H13_REG_SEEDS (default 6), OHPL_H13_REG_OPS (400).

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <list>
#include <map>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "impl/XmlDocumentTiny.h"
#include "resources/HplMapParse.h"
#include "resources/XmlDocument.h"
#include "scene/RustSceneH13.h"
#include "system/String.h"

using namespace hpl;

namespace {

struct Rng
{
	uint64_t s;
	uint32_t Next()
	{
		s = s * 6364136223846793005ULL + 1442695040888963407ULL;
		return (uint32_t)(s >> 33);
	}
	uint32_t Below(uint32_t n) { return Next() % n; }
};

struct Ent
{
	tString msName;
	int mlID;
	const tString& GetName() { return msName; }
	int GetUniqueID() { return mlID; }
};

const int kEnts = 24;
const int kKinds = 17;

const char *kNames[] = { "", "a", "A", "door_01", "Door_01", "\xe9z", "z", "light\xff", "door_01\x01" };

int EntIndex(Ent *pool, void *p)
{
	return p ? (int)((Ent*)p - pool) : -1;
}

std::string RunReg(bool abRust, uint64_t alSeed, int alOps)
{
	Ent pool[kEnts];
	Rng rng = { alSeed };
	for(int i=0; i<kEnts; ++i)
	{
		pool[i].msName = kNames[rng.Below(sizeof(kNames)/sizeof(kNames[0]))];
		pool[i].mlID = (int)rng.Below(9) - 1;
	}

	std::string out;
	char buf[256];
	for(int i=0; i<kEnts; ++i)
	{
		snprintf(buf, sizeof(buf), "ent %d %s %d\n", i, pool[i].msName.c_str(), pool[i].mlID);
		out += buf;
	}

	std::list<Ent*> lists[kKinds];
	std::map<tString, Ent*> areas;
	OhplH13Reg *pReg = abRust ? ohpl_h13_reg_new() : NULL;
	// Name lookups do not consult entity state beyond name/id, so the same pool serves every kind.

	auto Dump = [&](int k)
	{
		std::string s;
		if(abRust)
		{
			size_t n = ohpl_h13_reg_len(pReg, k);
			snprintf(buf, sizeof(buf), "s %d %zu:", k, n);
			s += buf;
			for(size_t i=0; i<n; ++i)
			{
				snprintf(buf, sizeof(buf), " %d", EntIndex(pool, ohpl_h13_reg_get(pReg, k, i)));
				s += buf;
			}
		}
		else
		{
			snprintf(buf, sizeof(buf), "s %d %zu:", k, lists[k].size());
			s += buf;
			for(Ent *p : lists[k])
			{
				snprintf(buf, sizeof(buf), " %d", EntIndex(pool, p));
				s += buf;
			}
		}
		out += s + "\n";
	};

	for(int op=0; op<alOps; ++op)
	{
		static const int kPick[] = { 0, 1, 2, 10, 14 };
		int k = kPick[rng.Below(5)];
		int e = (int)rng.Below(kEnts);
		Ent *pE = &pool[e];
		uint32_t lOp = rng.Below(100);
		int lRes = 0;
		const char *sOp = "";
		char sExtra[32] = "";
		if(lOp < 34)
		{
			sOp = "A";
			if(abRust) ohpl_h13_reg_add(pReg, k, pE);
			else lists[k].push_back(pE);
		}
		else if(lOp < 48)
		{
			sOp = "R";
			if(abRust) lRes = ohpl_h13_reg_remove(pReg, k, pE);
			else
			{
				auto it = std::find(lists[k].begin(), lists[k].end(), pE);
				lRes = it != lists[k].end();
				if(lRes) lists[k].erase(it);
			}
		}
		else if(lOp < 52)
		{
			sOp = "D";
			if(abRust) ohpl_h13_reg_make_dynamic(pReg, pE);
			else
			{
				auto it = std::find(lists[kOhplH13StaticMesh].begin(), lists[kOhplH13StaticMesh].end(), pE);
				if(it != lists[kOhplH13StaticMesh].end()) lists[kOhplH13StaticMesh].erase(it);
				lists[kOhplH13DynMesh].push_back(pE);
			}
			k = kOhplH13StaticMesh;
		}
		else if(lOp < 57)
		{
			sOp = "S";
			int e2 = (int)rng.Below(kEnts), e3 = (int)rng.Below(kEnts);
			snprintf(sExtra, sizeof(sExtra), " %d %d", e2, e3);
			size_t vSet[3] = { (size_t)pE, (size_t)&pool[e2], (size_t)&pool[e3] };
			if(abRust) ohpl_h13_reg_remove_set(pReg, k, vSet, 3);
			else lists[k].remove_if([&](Ent *p) { return (size_t)p == vSet[0] || (size_t)p == vSet[1] || (size_t)p == vSet[2]; });
		}
		else if(lOp < 59)
		{
			sOp = "C";
			if(abRust) ohpl_h13_reg_clear(pReg, k);
			else lists[k].clear();
		}
		else if(lOp < 72)
		{
			sOp = "N";
			bool bLast = rng.Below(3) == 0;
			const tString &sKey = pE->msName;
			Ent *pFound = NULL;
			if(abRust)
			{
				OhplName key{sKey.data(), sKey.size()};
				pFound = (Ent*)ohpl_h13_reg_find_name(pReg, k, key, bLast, OhplH13NameOf<Ent>);
			}
			else
			{
				for(Ent *p : lists[k])
					if(p->msName == sKey) { pFound = p; if(!bLast) break; }
			}
			lRes = EntIndex(pool, pFound);
			if(bLast) sOp = "NL";
		}
		else if(lOp < 82)
		{
			sOp = "I";
			Ent *pFound = NULL;
			if(abRust) pFound = (Ent*)ohpl_h13_reg_find_id(pReg, k, pE->mlID, OhplH13IdOf<Ent>);
			else
			{
				for(Ent *p : lists[k])
					if(p->mlID == pE->mlID) { pFound = p; break; }
			}
			lRes = EntIndex(pool, pFound);
		}
		else if(lOp < 88)
		{
			sOp = "X";
			if(abRust) lRes = ohpl_h13_reg_index(pReg, k, pE);
			else
			{
				lRes = -1;
				int i = 0;
				for(Ent *p : lists[k]) { if(p == pE) { lRes = i; break; } ++i; }
			}
		}
		else if(lOp < 92)
		{
			sOp = "F";
			Ent *pFound = NULL;
			if(abRust) pFound = (Ent*)ohpl_h13_reg_first_by_name(pReg, k, OhplH13NameOf<Ent>);
			else
			{
				for(Ent *p : lists[k])
					if(pFound == NULL || p->msName < pFound->msName) pFound = p;
			}
			lRes = EntIndex(pool, pFound);
		}
		else if(lOp < 96)
		{
			sOp = "T";
			if(abRust) lRes = ohpl_h13_area_insert(pReg, OhplName{pE->msName.data(), pE->msName.size()}, pE);
			else lRes = areas.insert(std::map<tString, Ent*>::value_type(pE->msName, pE)).second;
		}
		else if(lOp < 99)
		{
			sOp = "G";
			Ent *pFound = NULL;
			if(abRust) pFound = (Ent*)ohpl_h13_area_get(pReg, OhplName{pE->msName.data(), pE->msName.size()});
			else
			{
				auto it = areas.find(pE->msName);
				if(it != areas.end()) pFound = it->second;
			}
			lRes = EntIndex(pool, pFound);
		}
		else
		{
			sOp = "Z";
			if(abRust) ohpl_h13_area_clear(pReg);
			else areas.clear();
		}
		snprintf(buf, sizeof(buf), "%s %d %d%s = %d\n", sOp, k, e, sExtra, lRes);
		out += buf;
		if(op % 7 == 6 || lOp < 59) Dump(k);
	}
	for(int k=0; k<kKinds; ++k) Dump(k);
	if(pReg) ohpl_h13_reg_free(pReg);
	return out;
}

std::vector<int> gDrops;
Ent *gPool = NULL;

void RecordDrop(void *apItem)
{
	gDrops.push_back(EntIndex(gPool, apItem));
}

// Cursor walks with erase/append and destroy order: OhplH13Walk over std::list (oracle) vs Rust cursors.
std::string RunWalk(bool abRust, uint64_t alSeed, int alOps)
{
	Ent pool[kEnts];
	gPool = pool;
	gDrops.clear();
	Rng rng = { alSeed };
	for(int i=0; i<kEnts; ++i) pool[i].mlID = (int)rng.Below(5);

	std::list<Ent*> list;
	OhplH13Reg *pReg = abRust ? ohpl_h13_reg_new() : NULL;
	if(pReg) ohpl_h13_reg_set_foreign(pReg, 9, NULL, NULL, RecordDrop);
	std::string out;
	char buf[64];
	for(int op=0; op<alOps; ++op)
	{
		const uint32_t lOp = rng.Below(10);
		const int e = (int)rng.Below(kEnts);
		if(lOp < 5)
		{
			OhplH13Push(pReg, 9, list, &pool[e]);
			continue;
		}
		if(lOp < 8)
		{
			bool bAppended = false;
			out += "w:";
			for(OhplH13Walk<std::list<Ent*> > it(pReg, 9, list); it.Has();)
			{
				Ent *pE = it.Get();
				snprintf(buf, sizeof(buf), " %d", EntIndex(pool, pE));
				out += buf;
				if(pE->mlID == 0) { out += "x"; it.Erase(); continue; }
				if(pE->mlID == 1 && !bAppended) { bAppended = true; OhplH13Push(pReg, 9, list, &pool[e]); }
				it.Advance();
			}
			out += "\n";
			continue;
		}
		if(lOp < 9)
		{
			if(pReg) ohpl_h13_reg_destroy(pReg, 9, &pool[e], false);
			else
			{
				for(std::list<Ent*>::iterator it = list.begin(); it != list.end(); ++it)
					if(*it == &pool[e]) { list.erase(it); break; }
				RecordDrop(&pool[e]);
			}
			continue;
		}
		if(pReg) ohpl_h13_reg_destroy_all(pReg, 9);
		else
		{
			for(Ent *p : list) RecordDrop(p);
			list.clear();
		}
		out += "all\n";
	}
	out += "drops:";
	for(int d : gDrops) { snprintf(buf, sizeof(buf), " %d", d); out += buf; }
	out += "\nsize ";
	snprintf(buf, sizeof(buf), "%zu\n", OhplH13Size(pReg, 9, list));
	out += buf;
	if(pReg) ohpl_h13_reg_free(pReg);
	gPool = NULL;
	return out;
}

typedef std::vector<std::pair<int,int> > tPlan;

tPlan OracleMeshPlan(const std::vector<OhplH13MeshItem> &v)
{
	tPlan plan;
	int lFirst = 0;
	for(size_t i=0; i<v.size(); ++i)
	{
		if(i < v.size()-1)
		{
			const OhplH13MeshItem &next = v[i+1];
			if(next.mMaterial != v[i].mMaterial || next.mbShadow != v[i].mbShadow || next.mbCombine == false)
			{
				plan.push_back(std::make_pair(lFirst, (int)i));
				lFirst = (int)i+1;
			}
		}
		else plan.push_back(std::make_pair(lFirst, (int)i));
	}
	return plan;
}

tPlan OracleBodyPlan(const std::vector<OhplH13BodyItem> &v, size_t alMeshCount)
{
	tPlan plan;
	const int lIndexCountLimit = 50000;
	int lIndexCount = 0;
	int lFirst = 0;
	for(size_t i=0; i<v.size(); ++i)
	{
		const OhplH13BodyItem &o = v[i];
		if(o.mbCollides) lIndexCount += o.mlIndexNum;
		if(i < alMeshCount-1)
		{
			const OhplH13BodyItem &next = v[i+1];
			if(lIndexCount > lIndexCountLimit || next.mbCharCollider != o.mbCharCollider ||
				next.mPhysMaterial != o.mPhysMaterial || next.mbShadow != o.mbShadow)
			{
				plan.push_back(std::make_pair(lFirst, (int)i));
				lFirst = (int)i+1;
				lIndexCount = 0;
			}
		}
		else plan.push_back(std::make_pair(lFirst, (int)i));
	}
	return plan;
}

std::string PlanText(const tPlan &p)
{
	std::string s;
	char buf[32];
	for(size_t i=0; i<p.size(); ++i)
	{
		snprintf(buf, sizeof(buf), " %d-%d", p[i].first, p[i].second);
		s += buf;
	}
	return s;
}

tPlan RustPlan(const int32_t *apOut, size_t alRuns)
{
	tPlan p;
	for(size_t i=0; i<alRuns; ++i) p.push_back(std::make_pair(apOut[2*i], apOut[2*i+1]));
	return p;
}

int RunPlans(FILE *f)
{
	int lDiffs = 0, lCases = 0;
	Rng rng = { 0x13572468ULL };
	char buf[128];
	for(int c=0; c<400; ++c)
	{
		size_t n = c < 4 ? (size_t)c : rng.Below(60);
		uint32_t lMats = 1 + rng.Below(4);
		uint32_t lCombineOdds = 1 + rng.Below(8);
		std::vector<OhplH13MeshItem> vm(n);
		std::vector<OhplH13BodyItem> vb(n);
		for(size_t i=0; i<n; ++i)
		{
			vm[i].mMaterial = (size_t)(0x1000 + 0x40 * rng.Below(lMats));
			vm[i].mbShadow = rng.Below(5) == 0;
			vm[i].mbCombine = rng.Below(lCombineOdds) != 0;
			vb[i].mPhysMaterial = (size_t)(0x2000 + 0x40 * rng.Below(lMats));
			vb[i].mbShadow = rng.Below(6) == 0;
			vb[i].mbCharCollider = rng.Below(8) == 0;
			vb[i].mbCollides = rng.Below(4) != 0;
			vb[i].mlIndexNum = (int32_t)rng.Below(c % 5 == 0 ? 2000000000u : 30000u);
			if(c % 11 == 0) vb[i].mlIndexNum = INT32_MAX - (int32_t)rng.Below(4);
		}
		std::vector<int32_t> out(2 * n + 2);
		tPlan om = OracleMeshPlan(vm);
		tPlan rm = RustPlan(out.data(), ohpl_h13_plan_meshes(vm.data(), n, out.data(), n));
		tPlan ob = n ? OracleBodyPlan(vb, n) : tPlan();
		tPlan rb = RustPlan(out.data(), ohpl_h13_plan_bodies(vb.data(), n, n, out.data(), n));
		lCases += 2;
		if(om != rm) { ++lDiffs; fprintf(stderr, "plan mesh diff case %d\n", c); }
		if(ob != rb) { ++lDiffs; fprintf(stderr, "plan body diff case %d\n", c); }

		snprintf(buf, sizeof(buf), "plan %d %zu\n", c, n);
		fputs(buf, f);
		for(size_t i=0; i<n; ++i)
		{
			snprintf(buf, sizeof(buf), "m %zu %d %d b %zu %d %d %d %d\n", vm[i].mMaterial, vm[i].mbShadow, vm[i].mbCombine,
					vb[i].mPhysMaterial, vb[i].mbShadow, vb[i].mbCharCollider, vb[i].mbCollides, vb[i].mlIndexNum);
			fputs(buf, f);
		}
		fprintf(f, "pm%s\npb%s\n", PlanText(om).c_str(), PlanText(ob).c_str());
	}
	fprintf(stderr, "plan: cases %d diffs %d\n", lCases, lDiffs);
	return lDiffs;
}

///////////////////////////////////////////////////////////////////////////
// [map]

struct Trace
{
	std::string text;
	int events = 0;
	uint64_t hash = 14695981039346656037ULL;

	void Line(const std::string &s)
	{
		++events;
		for(unsigned char c : s) { hash ^= c; hash *= 1099511628211ULL; }
		hash ^= '\n';
		hash *= 1099511628211ULL;
	}
};

std::string B(float v) { uint32_t u; memcpy(&u, &v, 4); char b[16]; snprintf(b, sizeof(b), "%08x", u); return b; }
std::string V3(const OhplF3 &v) { return B(v.x) + " " + B(v.y) + " " + B(v.z); }
std::string V4(const OhplF4 &v) { return B(v.r) + " " + B(v.g) + " " + B(v.b) + " " + B(v.a); }
std::string S(OhplStr s) { return std::string(s.p, s.n); }
std::string N(int v) { return std::to_string(v); }

Trace* T(void *p) { return (Trace*)p; }

std::string Attr(OhplXmlNode node, const char *name)
{
	if(!node) return "-";
	const char *v = ((cXmlElement*)node)->GetAttribute(name);
	return v ? v : "-";
}

void Node(void *c, int kind, OhplXmlNode n) noexcept
{
	T(c)->Line("node " + N(kind) + " " + Attr(n, "Name") + " " + Attr(n, "ID"));
}

OhplMapBuilder MakeBuilder(Trace *t)
{
	OhplMapBuilder b;
	memset(&b, 0, sizeof(b));
	b.ctx = t;
	b.log = [](void *c, OhplStr s) noexcept { T(c)->Line("log " + S(s)); };
	b.warning = [](void *c, OhplStr s) noexcept { T(c)->Line("warn " + S(s)); };
	b.error = [](void *c, OhplStr s) noexcept { T(c)->Line("err " + S(s)); };
	b.phase = [](void *c, int p, bool begin) noexcept { T(c)->Line("phase " + N(p) + " " + N(begin)); };
	b.set_fog = [](void *c, bool a, OhplF4 col, float fe, float st, float en, bool cull) noexcept
	{ T(c)->Line("fog " + N(a) + " " + V4(col) + " " + B(fe) + " " + B(st) + " " + B(en) + " " + N(cull)); };
	b.set_skybox = [](void *c, bool a, OhplF4 col, OhplStr tex) noexcept { T(c)->Line("sky " + N(a) + " " + V4(col) + " " + S(tex)); };
	b.static_begin = [](void *c) noexcept { T(c)->Line("sbegin"); };
	b.static_object = [](void *c, OhplStr name, OhplStr file, OhplF3 pos, OhplF3 rot, OhplF3 scale, bool coll, bool sh, int id) noexcept
	{ T(c)->Line("obj " + S(name) + " " + S(file) + " " + V3(pos) + " " + V3(rot) + " " + V3(scale) + " " + N(coll) + " " + N(sh) + " " + N(id)); };
	b.static_plane = [](void *c, OhplStr name, OhplStr mat, bool sh, bool coll, int id, OhplF3 pos, OhplF3 scale, OhplF3 rot,
						OhplF3 start, OhplF3 end, const OhplF2 *uv) noexcept
	{
		std::string s = "plane " + S(name) + " " + S(mat) + " " + N(sh) + " " + N(coll) + " " + N(id) + " " + V3(pos) + " " + V3(scale) +
						" " + V3(rot) + " " + V3(start) + " " + V3(end);
		for(int i=0; i<4; ++i) s += " " + B(uv[i].x) + " " + B(uv[i].y);
		T(c)->Line(s);
	};
	b.static_decal = [](void *c, OhplStr name, OhplF4 col, int id, OhplStr mat, OhplXmlNode mesh) noexcept
	{ T(c)->Line("decal " + S(name) + " " + V4(col) + " " + N(id) + " " + S(mat) + " " + N(mesh != NULL)); };
	b.static_loading_done = [](void *c) noexcept { T(c)->Line("sdone"); };
	b.static_combo = [](void *c, int g, const int *ids, size_t n) noexcept
	{
		std::string s = "combo " + N(g);
		for(size_t i=0; i<n; ++i) s += " " + N(ids[i]);
		T(c)->Line(s);
	};
	b.static_compile = [](void *c) noexcept { T(c)->Line("scompile"); };
	b.entities_begin = [](void *c) noexcept { T(c)->Line("ebegin"); };
	b.entities_end = [](void *c) noexcept { T(c)->Line("eend"); };
	b.entity = [](void *c, OhplStr name, int id, bool act, OhplF3 pos, OhplF3 rot, OhplF3 scale, OhplStr file, OhplXmlNode vars) noexcept
	{ T(c)->Line("ent " + S(name) + " " + N(id) + " " + N(act) + " " + V3(pos) + " " + V3(rot) + " " + V3(scale) + " " + S(file) + " " + N(vars != NULL)); };
	b.area = [](void *c, OhplStr name, int id, bool act, OhplF3 pos, OhplF3 rot, OhplF3 scale, OhplStr type, OhplXmlNode vars) noexcept
	{ T(c)->Line("area " + S(name) + " " + N(id) + " " + N(act) + " " + V3(pos) + " " + V3(rot) + " " + V3(scale) + " " + S(type) + " " + N(vars != NULL)); };
	b.fog_area = [](void *c, OhplXmlNode n) noexcept { Node(c, 0, n); };
	b.particle_system = [](void *c, OhplXmlNode n) noexcept { Node(c, 1, n); };
	b.sound = [](void *c, OhplXmlNode n) noexcept { Node(c, 2, n); };
	b.billboard = [](void *c, OhplXmlNode n) noexcept { Node(c, 3, n); };
	b.light = [](void *c, OhplXmlNode n) noexcept { Node(c, 4, n); };
	return b;
}

void FindMaps(const std::string &dir, std::vector<std::string> &out)
{
	DIR *d = opendir(dir.c_str());
	if(!d) return;
	while(dirent *e = readdir(d))
	{
		std::string name = e->d_name;
		if(name == "." || name == "..") continue;
		std::string path = dir + "/" + name;
		struct stat st;
		if(stat(path.c_str(), &st) != 0) continue;
		if(S_ISDIR(st.st_mode)) FindMaps(path, out);
		else if(name.size() > 4 && name.compare(name.size()-4, 4, ".map") == 0) out.push_back(path);
	}
	closedir(d);
}

int RunMaps(FILE *f)
{
	const char *sDirs = getenv("OHPL_H13_DIRS");
	if(!sDirs) return 0;
	std::string dirs = sDirs;
	int lMaps = 0, lBad = 0;
	size_t pos = 0;
	while(pos <= dirs.size())
	{
		size_t end = dirs.find(':', pos);
		if(end == std::string::npos) end = dirs.size();
		std::string root = dirs.substr(pos, end - pos);
		pos = end + 1;
		if(root.empty()) continue;
		size_t slash = root.find_last_of('/');
		std::string tag = slash == std::string::npos ? root : root.substr(slash + 1);

		std::vector<std::string> files;
		FindMaps(root, files);
		std::sort(files.begin(), files.end());
		for(const std::string &path : files)
		{
			cXmlDocumentTiny doc("map");
			if(!doc.CreateFromFile(cString::To16Char(path))) { ++lBad; fprintf(f, "map %s/%s unreadable\n", tag.c_str(), path.c_str() + root.size() + 1); continue; }
			Trace t;
			OhplMapBuilder b = MakeBuilder(&t);
			OhplMapOptions opts;
			opts.flags = 0;
			opts.logCache = false;
			opts.file.p = "x.map";
			opts.file.n = 5;
			int lRes = HplMapParse(&doc, &b, &opts, false);
			++lMaps;
			fprintf(f, "map %s/%s res=%d events=%d hash=%016llx\n", tag.c_str(), path.c_str() + root.size() + 1, lRes, t.events, (unsigned long long)t.hash);
		}
	}
	fprintf(stderr, "map: parsed %d, unreadable %d\n", lMaps, lBad);
	return 0;
}

}

int hplMain(const hpl::tString&)
{
	const char *sDump = getenv("OHPL_H13_DUMP");
	FILE *f = sDump ? fopen(sDump, "w") : stdout;
	if(!f) return 2;

	int lDiffs = 0;
	const int lSeeds = getenv("OHPL_H13_REG_SEEDS") ? atoi(getenv("OHPL_H13_REG_SEEDS")) : 6;
	const int lOps = getenv("OHPL_H13_REG_OPS") ? atoi(getenv("OHPL_H13_REG_OPS")) : 400;
	long lLines = 0;
	for(int s=0; s<lSeeds; ++s)
	{
		std::string sOracle = RunReg(false, 1000 + s, lOps);
		std::string sRust = RunReg(true, 1000 + s, lOps);
		fprintf(f, "reg %d\n%s", s, sOracle.c_str());
		lLines += std::count(sOracle.begin(), sOracle.end(), '\n');
		if(sOracle != sRust)
		{
			++lDiffs;
			size_t i = 0;
			while(i < sOracle.size() && i < sRust.size() && sOracle[i] == sRust[i]) ++i;
			fprintf(stderr, "reg seed %d differs at byte %zu: oracle '%.60s' rust '%.60s'\n", s, i, sOracle.c_str() + i, sRust.c_str() + i);
		}
	}
	fprintf(stderr, "reg: seeds %d, lines %ld, diffs %d\n", lSeeds, lLines, lDiffs);

	{
		int lWalkDiffs = 0;
		for(int s=0; s<lSeeds; ++s)
		{
			std::string sOracle = RunWalk(false, 2000 + s, lOps);
			std::string sRust = RunWalk(true, 2000 + s, lOps);
			fprintf(f, "walk %d\n%s", s, sOracle.c_str());
			if(sOracle != sRust)
			{
				++lWalkDiffs;
				size_t i = 0;
				while(i < sOracle.size() && i < sRust.size() && sOracle[i] == sRust[i]) ++i;
				fprintf(stderr, "walk seed %d differs at byte %zu: oracle '%.60s' rust '%.60s'\n", s, i, sOracle.c_str() + i, sRust.c_str() + i);
			}
		}
		fprintf(stderr, "walk: seeds %d, diffs %d\n", lSeeds, lWalkDiffs);
		lDiffs += lWalkDiffs;
	}

	lDiffs += RunPlans(f);
	RunMaps(f);
	if(sDump) fclose(f);
	return lDiffs ? 1 : 0;
}
