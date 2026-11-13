// S4 golden generator. The save/load lambdas and the debug/nav string expressions are the code of
// SomaAgent.cpp (SomaAgentSavePath/LoadPath/SomaNavPath/SomaAgentDebug) over a stub pathfinder;
// strings go through the real cString::ToString and cVector3f::ToString of libHPL2.
//   gen <seeds>  -> text on stdout
// P line: moving g0 g1 g2 exact result in_map end_cb ntracks {node min max anim loop}* idx active paused loop atnode wait freq trackcb
//         | blob-hex ; floats are bit patterns, strings hex ("-" empty)
// L line: cut idx active paused loop atnode wait freq | parsed (P layout, after "cut")
// N line: n x y z ... | text-hex ; D line: debug fields | text-hex
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "math/Vector3.h"
#include "system/String.h"

using namespace hpl;
typedef std::string tString;

struct cAgentTrackNode
{
	tString msNode;
	float mfMinWait, mfMaxWait;
	tString msAnim;
	bool mbLoopAnim;
};

struct PF
{
	bool mbMoving = false, mbExact = false, mbCallbackInMap = false;
	bool mbTrackActive = false, mbTrackPaused = false, mbTrackLoop = false, mbAtTrackNode = false;
	float mvGoal[3] = {};
	int mlTrackIdx = -1;
	float mfTrackWait = 0, mfTrackFreq = 1;
	tString msResultCallback, msEndOfPathCallback, msTrackCallback;
	std::vector<cAgentTrackNode> mvTrack;
	// load results
	bool mMoved = false, mExact = false, mInMap = false;
	float mGoal[3] = {};
	tString mResult;
	void MoveTo(const cVector3f &g, bool e, const tString &r, bool m)
	{
		mMoved = true, mGoal[0] = g.x, mGoal[1] = g.y, mGoal[2] = g.z, mExact = e, mResult = r, mInMap = m;
	}
	void Stop() { mMoved = false; }
};

static uint32_t gs;
static uint32_t Rnd() { return gs = gs * 1664525u + 1013904223u; }
static float RndF() { return (int)(Rnd() >> 8) / 4096.0f - 2000.0f; }
static unsigned Fb(float f)
{
	unsigned u;
	memcpy(&u, &f, 4);
	return u;
}
static std::string Hex(const std::string &s)
{
	if (s.empty())
		return "-";
	std::string o;
	char b[3];
	for (unsigned char c : s)
		snprintf(b, 3, "%02x", c), o += b;
	return o;
}
static std::string RndStr()
{
	std::string s;
	int n = Rnd() % 6 == 0 ? 0 : Rnd() % 12;
	for (int i = 0; i < n; ++i)
		s += (char)(Rnd() & 0xff);
	return s;
}

// ---- verbatim from SomaAgentSavePath ----
static std::string Save(PF *pPF)
{
	std::string s;
	auto pod = [&](const auto &x) { s.append((const char *)&x, sizeof(x)); };
	auto str = [&](const tString &x) { pod((uint32_t)x.size()); s += x; };
	pod(pPF->mbMoving), pod(pPF->mvGoal), pod(pPF->mbExact), str(pPF->msResultCallback), pod(pPF->mbCallbackInMap), str(pPF->msEndOfPathCallback);
	pod((uint32_t)pPF->mvTrack.size());
	for (const cAgentTrackNode &n : pPF->mvTrack)
		str(n.msNode), pod(n.mfMinWait), pod(n.mfMaxWait), str(n.msAnim), pod(n.mbLoopAnim);
	pod(pPF->mlTrackIdx), pod(pPF->mbTrackActive), pod(pPF->mbTrackPaused), pod(pPF->mbTrackLoop), pod(pPF->mbAtTrackNode);
	pod(pPF->mfTrackWait), pod(pPF->mfTrackFreq), str(pPF->msTrackCallback);
	return s;
}

// ---- verbatim from SomaAgentLoadPath ----
static void Load(PF *pPF, const std::string &asData)
{
	size_t p = 0;
	auto pod = [&](auto &x) {
		if (p + sizeof(x) <= asData.size())
			memcpy(&x, asData.data() + p, sizeof(x));
		p += sizeof(x);
	};
	auto str = [&](tString &x) {
		uint32_t n = 0;
		pod(n);
		x = p + n <= asData.size() ? asData.substr(p, n) : "";
		p += n;
	};
	bool bMoving = false, bExact = false, bInMap = false;
	cVector3f vGoal = 0;
	tString sResult;
	pod(bMoving), pod(vGoal), pod(bExact), str(sResult), pod(bInMap), str(pPF->msEndOfPathCallback);
	uint32_t n = 0;
	pod(n);
	pPF->mvTrack.assign(std::min<uint32_t>(n, 4096), cAgentTrackNode{});
	for (cAgentTrackNode &t : pPF->mvTrack)
		str(t.msNode), pod(t.mfMinWait), pod(t.mfMaxWait), str(t.msAnim), pod(t.mbLoopAnim);
	pod(pPF->mlTrackIdx), pod(pPF->mbTrackActive), pod(pPF->mbTrackPaused), pod(pPF->mbTrackLoop), pod(pPF->mbAtTrackNode);
	pod(pPF->mfTrackWait), pod(pPF->mfTrackFreq), str(pPF->msTrackCallback);
	if (bMoving)
		pPF->MoveTo(vGoal, bExact, sResult, bInMap);
	else
		pPF->Stop();
}

static void PrintState(const PF &p, bool withMove)
{
	if (withMove)
		printf(" %d %08x %08x %08x %d %s %d %s", (int)p.mMoved, Fb(p.mGoal[0]), Fb(p.mGoal[1]), Fb(p.mGoal[2]), (int)p.mExact, Hex(p.mResult).c_str(), (int)p.mInMap,
			   Hex(p.msEndOfPathCallback).c_str());
	else
		printf(" %d %08x %08x %08x %d %s %d %s", (int)p.mbMoving, Fb(p.mvGoal[0]), Fb(p.mvGoal[1]), Fb(p.mvGoal[2]), (int)p.mbExact, Hex(p.msResultCallback).c_str(),
			   (int)p.mbCallbackInMap, Hex(p.msEndOfPathCallback).c_str());
	printf(" %zu", p.mvTrack.size());
	for (const cAgentTrackNode &n : p.mvTrack)
		printf(" %s %08x %08x %s %d", Hex(n.msNode).c_str(), Fb(n.mfMinWait), Fb(n.mfMaxWait), Hex(n.msAnim).c_str(), (int)n.mbLoopAnim);
	printf(" %d %d %d %d %d %08x %08x %s", p.mlTrackIdx, (int)p.mbTrackActive, (int)p.mbTrackPaused, (int)p.mbTrackLoop, (int)p.mbAtTrackNode, Fb(p.mfTrackWait), Fb(p.mfTrackFreq),
		   Hex(p.msTrackCallback).c_str());
}

static std::string Flt(float f) { return cString::ToString(f); }

int main(int argc, char **argv)
{
	int seeds = argc > 1 ? atoi(argv[1]) : 12;
	for (int sd = 1; sd <= seeds; ++sd)
	{
		gs = sd * 7919u;
		for (int k = 0; k < 6; ++k)
		{
			PF p;
			p.mbMoving = Rnd() & 1, p.mbExact = Rnd() & 1, p.mbCallbackInMap = Rnd() & 1;
			p.mbTrackActive = Rnd() & 1, p.mbTrackPaused = Rnd() & 1, p.mbTrackLoop = Rnd() & 1, p.mbAtTrackNode = Rnd() & 1;
			for (float &g : p.mvGoal)
				g = RndF();
			p.mlTrackIdx = (int)(Rnd() % 9) - 2;
			p.mfTrackWait = RndF(), p.mfTrackFreq = RndF();
			p.msResultCallback = RndStr(), p.msEndOfPathCallback = RndStr(), p.msTrackCallback = RndStr();
			int nt = Rnd() % 5;
			for (int i = 0; i < nt; ++i)
				p.mvTrack.push_back(cAgentTrackNode{RndStr(), RndF(), RndF(), RndStr(), (bool)(Rnd() & 1)});
			std::string blob = Save(&p);
			printf("P");
			PrintState(p, false);
			printf(" | %s\n", Hex(blob).c_str());
			for (int m = 0; m < 6; ++m)
			{
				std::string d = blob;
				if (m == 1)
					d = d.substr(0, Rnd() % (d.size() + 1));
				if (m == 2 && d.size() > 6)
					d[Rnd() % 6 + 15 % d.size()] ^= 0xff;
				if (m == 3 && d.size() > 24)
					d.resize(d.size() - 1 - Rnd() % 20);
				if (m == 4 && d.size() > 24)
				{
					uint32_t huge = 0xffffff00u + Rnd() % 200;
					memcpy(&d[14 + 0 * 4], &huge, 4);
				}
				PF cur;
				cur.mlTrackIdx = (int)(Rnd() % 5) - 1, cur.mbTrackActive = Rnd() & 1, cur.mbTrackPaused = Rnd() & 1, cur.mbTrackLoop = Rnd() & 1;
				cur.mbAtTrackNode = Rnd() & 1, cur.mfTrackWait = RndF(), cur.mfTrackFreq = RndF();
				PF o = cur;
				Load(&o, d);
				printf("L %s %d %d %d %d %d %08x %08x |", Hex(d).c_str(), cur.mlTrackIdx, (int)cur.mbTrackActive, (int)cur.mbTrackPaused, (int)cur.mbTrackLoop,
					   (int)cur.mbAtTrackNode, Fb(cur.mfTrackWait), Fb(cur.mfTrackFreq));
				PrintState(o, true);
				printf("\n");
			}
		}
		// nav path text, verbatim expression
		{
			int n = Rnd() % 5;
			std::vector<cVector3f> lst;
			for (int i = 0; i < n; ++i)
				lst.push_back(cVector3f(RndF(), RndF(), RndF()));
			tString s;
			for (auto it = lst.rbegin(); it != lst.rend(); ++it)
				s += cString::ToString(it->x) + " " + cString::ToString(it->y) + " " + cString::ToString(it->z) + "\n";
			printf("N %d", n);
			for (auto &v : lst)
				printf(" %08x %08x %08x", Fb(v.x), Fb(v.y), Fb(v.z));
			printf(" | %s\n", Hex(s).c_str());
		}
		// debug text: the three sections of SomaAgentDebug
		for (int k = 0; k < 4; ++k)
		{
			bool hs = Rnd() & 1, hp = Rnd() & 1, hc = Rnd() & 1;
			tString cur = RndStr(), prev = RndStr(), next = RndStr(), anim = RndStr();
			int timers = Rnd() % 9;
			bool pm = Rnd() & 1, pa = Rnd() & 1, pp = Rnd() & 1, pn = Rnd() & 1, cm = Rnd() & 1, ct = Rnd() & 1, cw = Rnd() & 1, co = Rnd() & 1;
			int pidx = Rnd() % 30, plen = Rnd() % 30, tidx = (int)(Rnd() % 6) - 1, tlen = Rnd() % 6, ss = (int)(Rnd() % 4) - 1, ie = Rnd() % 5, wh = Rnd() % 8;
			float pg[3] = {RndF(), RndF(), RndF()}, cg[3] = {RndF(), RndF(), RndF()}, pw = RndF(), fw = RndF(), st = RndF(), iw = RndF(), ro = RndF();
			tString s;
			if (hs)
				s += "state=" + cur + " prev=" + prev + " next=" + next + " timers=" + cString::ToString(timers);
			if (hp)
				s += " pf_moving=" + cString::ToString(pm) + " path=" + cString::ToString(pidx) + "/" + cString::ToString(plen) + " goal=" + cVector3f(pg[0], pg[1], pg[2]).ToString() +
					 " track=" + cString::ToString(tidx) + "/" + cString::ToString(tlen) + " track_active=" + cString::ToString(pa) + " paused=" + cString::ToString(pp) +
					 " at_node=" + cString::ToString(pn) + " wait=" + cString::ToString(pw);
			if (hc)
				s += " cm_moving=" + cString::ToString(cm) + " cm_goal=" + cVector3f(cg[0], cg[1], cg[2]).ToString() + " speed_state=" + cString::ToString(ss) +
					 " fwd=" + cString::ToString(fw) + " stuck=" + cString::ToString(st) + " turning=" + cString::ToString(ct) + " anim=" + anim +
					 " idle_extra=" + cString::ToString(ie) + "/" + cString::ToString(iw) + " wall=" + cString::ToString(cw) + "/" + cString::ToString(wh) +
					 " roll=" + cString::ToString(ro) + " doa=" + cString::ToString(co);
			printf("D %d %d %d %s %s %s %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", (int)hs, (int)hp, (int)hc, Hex(cur).c_str(), Hex(prev).c_str(), Hex(next).c_str(),
				   Hex(anim).c_str(), timers, (int)pm, (int)pa, (int)pp, (int)pn, (int)cm, (int)ct, (int)cw, (int)co, pidx, plen, tidx, tlen, ss, ie, wh);
			for (float f : {pg[0], pg[1], pg[2], cg[0], cg[1], cg[2], pw, fw, st, iw, ro})
				printf(" %08x", Fb(f));
			printf(" | %s\n", Hex(s).c_str());
		}
	}
	return 0;
}
