// H16 sound harness. Links libHPL2.a; OHPL_IMPL=cpp|rust picks the cSoundHandler path.
//   gen sound <seed-count> [ops]   random play/stop/fade/pause/priority/callback script, trace on stdout
// Trace: "op ..." lines (the script), "g ..." lines (global tables the C++ settings handler computed,
// replay input), indented lines (channel calls and callbacks), "st ..." handler state after each op.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "sound/SoundHandler.h"
#include "sound/LowLevelSound.h"
#include "sound/SoundChannel.h"
#include "sound/SoundData.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "ai/AINodeContainer.h"
#include "ai/AStar.h"
#include "math/Math.h"
#include "system/Platform.h"

using namespace hpl;

static std::string gEv;

static unsigned Fb(float f) { unsigned u; memcpy(&u, &f, 4); return u; }
static unsigned long long Db(double f) { unsigned long long u; memcpy(&u, &f, 8); return u; }

static void Ev(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void Ev(const char* fmt, ...)
{
	char b[256];
	va_list a;
	va_start(a, fmt);
	vsnprintf(b, sizeof b, fmt, a);
	va_end(a);
	gEv += "  ";
	gEv += b;
	gEv += "\n";
}

class cFakeData : public iSoundData
{
public:
	cFakeData() : iSoundData("fake", L"fake", false) {}
	bool CreateFromFile(const tWString&) { return true; }
	iSoundChannel* CreateChannel(int) { return NULL; }
	bool IsStereo() { return false; }
};
static cFakeData gData;

static int gChId = 0;

class cFakeChannel : public iSoundChannel
{
public:
	void MarkStopUsed() { mbStopUsed = true; }
	int mId;
	bool mPlaying;
	cFakeChannel() : iSoundChannel(&gData, NULL)
	{
		mId = gChId++;
		mPlaying = false;
		mlPriority = 0;
	}
	~cFakeChannel() { Ev("c%d delete", mId); }
	void Play() { Ev("c%d play", mId); mPlaying = true; mbPaused = false; }
	void Stop() { Ev("c%d stop", mId); mPlaying = false; mbStopUsed = true; }
	void SetPaused(bool x) { Ev("c%d paused %d", mId, x); mbPaused = x; }
	void SetSpeed(float x) { Ev("c%d speed %08x", mId, Fb(x)); mfSpeed = x; }
	void SetVolume(float x) { Ev("c%d volume %08x", mId, Fb(x)); mfVolume = x; }
	void SetLooping(bool x) { Ev("c%d looping %d", mId, x); mbLooping = x; }
	void SetPan(float) {}
	void Set3D(bool x) { Ev("c%d 3d %d", mId, x); mb3D = x; }
	void SetPriority(int x) { Ev("c%d prio %d", mId, x); mlPriority = x; }
	int GetPriority() { return mlPriority; }
	void SetPositionIsRelative(bool x) { Ev("c%d relative %d", mId, x); mbPositionRelative = x; }
	void SetPosition(const cVector3f& p) { Ev("c%d pos %08x %08x %08x", mId, Fb(p.x), Fb(p.y), Fb(p.z)); mvPosition = p; }
	void SetVelocity(const cVector3f&) {}
	void SetMinDistance(float x) { Ev("c%d min %08x", mId, Fb(x)); mfMinDistance = x; }
	void SetMaxDistance(float x) { Ev("c%d max %08x", mId, Fb(x)); mfMaxDistance = x; }
	bool IsPlaying() { return mPlaying; }
	bool IsBufferUnderrun() { return false; }
	double GetElapsedTime() { return 0; }
	double GetTotalTime() { return 0; }
	void SetElapsedTime(double) {}
	void SetAffectedByEnv(bool x) { Ev("c%d env %d", mId, x); mbAffectedByEnv = x; }
	void SetFiltering(bool, int) {}
	void SetFilterGain(float) {}
	void SetFilterGainHF(float) {}
	void SetFilterGainLF(float) {}
	void SetEnvSendGain(float) {}
};

class cFakeLow : public iLowLevelSound
{
public:
	cFakeLow() { m_mtxListener = cMatrixf::Identity; }
	void GetSupportedFormats(tStringList&) {}
	iSoundData* LoadSoundData(const tString&, const tWString&, const tString&, bool, bool) { return NULL; }
	void UpdateSound(float) {}
	void SetListenerAttributes(const cVector3f&, const cVector3f&, const cVector3f&, const cVector3f&) {}
	void SetListenerPosition(const cVector3f&) {}
	void SetListenerAttenuation(bool) {}
	void SetSetRolloffFactor(float) {}
	void SetVolume(float) {}
	void Init(int, bool, int, int, bool, bool, int, int, int, int, bool) {}
	void SetEnvVolume(float) {}
	iSoundEnvironment* LoadSoundEnvironment(const tString&) { return NULL; }
	void SetSoundEnvironment(iSoundEnvironment*) {}
	void FadeSoundEnvironment(iSoundEnvironment*, iSoundEnvironment*, float) {}
	iSoundDeviceIdentifier* GetCurrentSoundDevice() { return NULL; }
	void SetListener(const cVector3f& p, const cMatrixf& m) { mvListenerPosition = p; m_mtxListener = m; }
};

class cTestHandler : public cSoundHandler
{
public:
	cTestHandler(iLowLevelSound* l) : cSoundHandler(l, NULL) {}
	iSoundChannel* CreateChannel(const tString& asName, int alPriority, bool abStream, bool* apNotEnoughChannels)
	{
		if(apNotEnoughChannels) *apNotEnoughChannels = false;
		if(asName == "missing") return NULL;
		if(asName == "full") { if(apNotEnoughChannels) *apNotEnoughChannels = true; return NULL; }
		cFakeChannel* c = new cFakeChannel();
		Ev("c%d create %s prio %d stream %d", c->mId, asName.c_str(), alPriority, abStream);
		return c;
	}
};

class cCb : public iSoundEntryCallback
{
public:
	cTestHandler* mH;
	int mMode;
	int mId;
	void OnPriorityRelease()
	{
		Ev("cb %d mode %d", mId, mMode);
		if(mMode == 1) mH->Play3D("cbplay", false, 0.5f, cVector3f(1, 0, 0), 1, 10);
		if(mMode == 2) mH->StopAll(eSoundEntryType_All);
		if(mMode == 3) mH->FadeOutAll(eSoundEntryType_All, 0.5f, false);
		if(mMode == 4) mH->Stop("snd_a");
	}
};

static unsigned long long gRng;
static unsigned Rnd(unsigned n)
{
	gRng ^= gRng << 13; gRng ^= gRng >> 7; gRng ^= gRng << 17;
	return (unsigned)((gRng >> 11) % n);
}
static float RndF(float lo, float hi) { return lo + (hi - lo) * (Rnd(1001) / 1000.0f); }

static const char* kNames[] = { "snd_a", "Snd_A", "snd_b", "SND_C", "missing", "full", "snd_d", "stream_e" };

struct Ref { cSoundEntry* p; int id; cFakeChannel* ch; };

static void Dump(cTestHandler& h, std::vector<Ref>& refs)
{
	tSoundEntryList* l = h.GetEntryList();
	char b[512];
	std::string s = "st";
	snprintf(b, sizeof b, " n=%zu", l->size());
	s += b;
	for(cSoundEntry* e : *l)
	{
		cFakeChannel* c = static_cast<cFakeChannel*>(e->GetChannel());
		snprintf(b, sizeof b, " [%d %s t%u c%d p%d%d%d first%d vm%08x v%08x sp%08x pr%d]", e->GetId(), e->GetName().c_str(),
				 (unsigned)e->GetType(), c->mId, c->mPlaying, c->GetPaused(), c->GetLooping(), e->IsFirstTime(),
				 Fb(e->GetVolumeMul()), Fb(c->GetVolume()), Fb(c->GetSpeed()), c->GetPriority());
		s += b;
	}
	printf("%s\n", s.c_str());
}

static void LogCb(eLogOutputType t, const char* m)
{
	std::string s = m;
	while(!s.empty() && s.back() == '\n') s.pop_back();
	Ev("log %d %s", (int)t, s.c_str());
}

static int RunSound(int seeds, int ops)
{
	SetLogMessageCallback(LogCb);
	for(int seed = 1; seed <= seeds; ++seed)
	{
		gRng = 0x9E3779B97F4A7C15ull * (unsigned long long)seed + 12345;
		gChId = 0;
		printf("seed %d\n", seed);
		cFakeLow low;
		cTestHandler* h = new cTestHandler(&low);
		std::vector<Ref> refs;
		std::vector<cCb*> cbs;
		float prevV[32], prevS[32];
		for(int i = 0; i < 32; ++i) { prevV[i] = 1; prevS[i] = 1; }

		for(int n = 0; n < ops; ++n)
		{
			gEv.clear();
			char line[400];
			std::string extra;
			unsigned k = Rnd(100);
			if(k < 24)
			{
				const char* name = kNames[Rnd(8)];
				bool loop = Rnd(3) == 0;
				float vol = RndF(0.1f, 1.0f);
				cVector3f pos(RndF(-12, 12), RndF(-3, 3), RndF(-12, 12));
				float mn = RndF(0.5f, 4), mx = mn + RndF(1, 12);
				unsigned type = 1u << Rnd(4);
				bool rel = Rnd(4) == 0, is3d = Rnd(4) != 0, stream = Rnd(5) == 0;
				int prio = (int)Rnd(5) - 2;
				int cb = Rnd(3) == 0 ? 1 + (int)Rnd(4) : 0;
				snprintf(line, sizeof line, "op play %s %d %08x %08x %08x %08x %08x %08x %u %d %d %d %d %d", name, loop, Fb(vol),
						 Fb(pos.x), Fb(pos.y), Fb(pos.z), Fb(mn), Fb(mx), type, rel, is3d, prio, stream, cb);
				bool notEnough = false;
				cSoundEntry* e = h->Play(name, loop, vol, pos, mn, mx, (eSoundEntryType)type, rel, is3d, prio, stream, &notEnough);
				if(e)
				{
					if(cb)
					{
						cCb* c = new cCb();
						c->mH = h; c->mMode = cb; c->mId = e->GetId();
						cbs.push_back(c);
						e->SetCallBack(c);
					}
					refs.push_back({ e, e->GetId(), static_cast<cFakeChannel*>(e->GetChannel()) });
				}
				Ev("play -> %d notenough %d", e ? e->GetId() : -1, notEnough);
			}
			else if(k < 50)
			{
				float dt = RndF(0.005f, 0.3f);
				snprintf(line, sizeof line, "op tick %08x", Fb(dt));
				h->Update(dt);
				std::string g;
				for(int i = 0; i < 32; ++i)
				{
					float v = h->GetGlobalVolume((eSoundEntryType)(1u << i)), sp = h->GetGlobalSpeed((eSoundEntryType)(1u << i));
					if(Fb(v) != Fb(prevV[i]) || Fb(sp) != Fb(prevS[i]))
					{
						char b[64];
						snprintf(b, sizeof b, "g %d %08x %08x\n", i, Fb(v), Fb(sp));
						g += b;
						prevV[i] = v; prevS[i] = sp;
					}
				}
				extra = g;
			}
			else if(k < 56)
			{
				const char* name = kNames[Rnd(8)];
				snprintf(line, sizeof line, "op stop %s", name);
				Ev("stop -> %d", h->Stop(name));
			}
			else if(k < 59)
			{
				unsigned m = 1 + Rnd(15);
				switch(Rnd(4))
				{
				case 0: snprintf(line, sizeof line, "op stopall %u", m); h->StopAll(m); break;
				case 1: snprintf(line, sizeof line, "op pauseall %u", m); h->PauseAll(m); break;
				case 2: snprintf(line, sizeof line, "op resumeall %u", m); h->ResumeAll(m); break;
				default:
				{
					float sp = RndF(0.2f, 3); bool dis = Rnd(2);
					snprintf(line, sizeof line, "op fadeoutall %u %08x %d", m, Fb(sp), dis);
					h->FadeOutAll(m, sp, dis);
				}
				}
			}
			else if(k < 76 && !refs.empty())
			{
				unsigned r = Rnd((unsigned)refs.size());
				Ref& ref = refs[r];
				bool valid = h->IsValid(ref.p, ref.id);
				float a = RndF(0, 1.5f), b = RndF(0.1f, 3);
				switch(Rnd(9))
				{
				case 0: snprintf(line, sizeof line, "op fadevol %u %08x %08x", r, Fb(a), Fb(b)); if(valid) ref.p->FadeVolumeMulTo(a, b); break;
				case 1: snprintf(line, sizeof line, "op fadespeed %u %08x %08x", r, Fb(a), Fb(b)); if(valid) ref.p->FadeSpeedMulTo(a, b); break;
				case 2: snprintf(line, sizeof line, "op fadeout %u %08x", r, Fb(b)); if(valid) ref.p->FadeOut(b); break;
				case 3: snprintf(line, sizeof line, "op fadein %u %08x %08x", r, Fb(a), Fb(b)); if(valid) ref.p->FadeIn(a, b); break;
				case 4: snprintf(line, sizeof line, "op setvolmul %u %08x", r, Fb(a)); if(valid) ref.p->SetVolumeMul(a); break;
				case 5: snprintf(line, sizeof line, "op setdefault %u %08x %08x", r, Fb(a), Fb(b)); if(valid) { ref.p->SetDefaultVolume(a); ref.p->SetDefaultSpeed(b); ref.p->SetSpeedMul(a); } break;
				case 6: snprintf(line, sizeof line, "op stopentry %u", r); if(valid) ref.p->Stop(); break;
				case 7: snprintf(line, sizeof line, "op stopdisabled %u %d", r, (int)(a > 0.7f)); if(valid) ref.p->SetStopDisabled(a > 0.7f); break;
				default: snprintf(line, sizeof line, "op pause %u %d", r, (int)(a > 0.7f)); if(valid) ref.p->SetPaused(a > 0.7f); break;
				}
				Ev("valid %d stopdisabled %d", valid, valid ? ref.p->GetStopDisabled() : -1);
			}
			else if(k < 82 && !refs.empty())
			{
				unsigned r = Rnd((unsigned)refs.size());
				Ref& ref = refs[r];
				if(h->IsValid(ref.p, ref.id))
				{
					if(Rnd(2)) { snprintf(line, sizeof line, "op steal %u", r); ref.ch->mPlaying = false; }
					else { snprintf(line, sizeof line, "op end %u", r); ref.ch->mPlaying = false; ref.ch->MarkStopUsed(); }
				}
				else snprintf(line, sizeof line, "op nop");
			}
			else if(k < 88)
			{
				unsigned m = 1 + Rnd(15);
				float v = RndF(0, 1.2f), sp = RndF(0.05f, 2);
				int id = Rnd(3) == 0 ? (int)Rnd(3) : -1;
				bool destroy = Rnd(2);
				int which = (int)Rnd(4);
				snprintf(line, sizeof line, "op global %d %u %08x %08x %d %d", which, m, Fb(v), Fb(sp), id, destroy);
				int r = -2;
				if(which == 0) r = h->SetGlobalVolume(v, m, id);
				else if(which == 1) r = h->SetGlobalSpeed(v, m, id);
				else if(which == 2) r = h->FadeGlobalVolume(v, sp, m, id, destroy);
				else r = h->FadeGlobalSpeed(v, sp, m, id, destroy);
				Ev("global -> %d", r);
			}
			else if(k < 91)
			{
				bool s = Rnd(2);
				snprintf(line, sizeof line, "op silent %d", s);
				h->SetSilent(s);
			}
			else if(k < 96)
			{
				cVector3f p(RndF(-8, 8), RndF(-2, 2), RndF(-8, 8));
				float a = RndF(0, 6.28f);
				cMatrixf m = cMatrixf::Identity;
				m.m[0][0] = cosf(a); m.m[0][2] = sinf(a); m.m[2][0] = -sinf(a); m.m[2][2] = cosf(a);
				m.m[0][3] = p.x; m.m[1][3] = p.y; m.m[2][3] = p.z;
				low.SetListener(p, m);
				std::string s = "op listener";
				char b[64];
				snprintf(b, sizeof b, " %08x %08x %08x", Fb(p.x), Fb(p.y), Fb(p.z)); s += b;
				for(int i = 0; i < 16; ++i) { snprintf(b, sizeof b, " %08x", Fb(m.v[i])); s += b; }
				snprintf(line, sizeof line, "%s", s.c_str());
			}
			else
			{
				const char* name = kNames[Rnd(8)];
				unsigned t = 1u << Rnd(5);
				snprintf(line, sizeof line, "op query %s %u", name, t);
				Ev("isplaying %d", h->IsPlaying(name));
				Ev("global %08x %08x", Fb(h->GetGlobalVolume((eSoundEntryType)t)), Fb(h->GetGlobalSpeed((eSoundEntryType)t)));
			}
			printf("%s\n%s%s", line, extra.c_str(), gEv.c_str());
			Dump(*h, refs);
		}
		gEv.clear();
		delete h;
		printf("%s", gEv.c_str());
		printf("end\n");
		for(cCb* c : cbs) delete c;
	}
	return 0;
}

class cRejectCb : public iAStarCallback
{
public:
	int mMod;
	bool CanAddNode(cAINode* p, cAINode* c) { return (p->GetIdx() * 7 + c->GetIdx()) % mMod != 0; }
};

// Walls: the plane x = 2.5 blocks rays with |z| below 2.5, and x = 5.5 blocks rays at odd z cells.
static bool WallCast(unsigned flags, const cVector3f* a, const cVector3f* b)
{
	if((a->x - 2.5f) * (b->x - 2.5f) < 0 && a->z < 2.5f && b->z < 2.5f) return true;
	if((a->x - 5.5f) * (b->x - 5.5f) < 0 && ((int)floorf(a->z) & 1)) return true;
	return false;
}

static int RunAi(int seeds, const char* file)
{
	cAINodeContainer::mpTestCast = WallCast;
	for(int seed = 1; seed <= seeds; ++seed)
	{
		gRng = 0xD1B54A32D192ED03ull * (unsigned long long)seed + 777;
		printf("aiseed %d\n", seed);
		cAINodeContainer c("c", "node", NULL, cVector3f(0.6f, 1.5f, 0.6f));
		int w = 4 + (int)Rnd(8), hgt = 3 + (int)Rnd(8);
		bool jitter = Rnd(2);
		std::vector<cVector3f> pos;
		std::vector<std::string> names;
		std::vector<int> ids;
		for(int z = 0; z < hgt; ++z)
			for(int x = 0; x < w; ++x)
			{
				if(Rnd(100) < 12) continue;
				cVector3f p((float)x, 0.0f, (float)z);
				if(jitter) { p.x += RndF(-0.3f, 0.3f); p.z += RndF(-0.3f, 0.3f); }
				p.y = Rnd(4) == 0 ? RndF(0, 0.05f) : 0.0f;
				int n = (int)pos.size();
				pos.push_back(p);
				names.push_back("n" + std::to_string(Rnd(100) < 10 && n > 0 ? (int)Rnd(n) : n));
				ids.push_back(Rnd(100) < 10 && n > 0 ? (int)Rnd(n) + 100 : n + 100);
			}
		printf("nodes %zu\n", pos.size());
		std::string xml = "<AINodes>\n";
		for(size_t i = 0; i < pos.size(); ++i)
		{
			xml += "<Node Name=\"" + names[i] + "\" ID=\"" + std::to_string(ids[i]) + "\">\n";
			for(size_t j = 0; j < pos.size(); ++j)
			{
				if(i == j) continue;
				float d = cMath::Vector3Dist(pos[i], pos[j]);
				if(d > 1.6f || Rnd(100) < 25) continue;
				char b[96];
				snprintf(b, sizeof b, "<Edge Node=\"%s\" Distance=\"%.9g\"/>\n", names[j].c_str(), d);
				xml += b;
			}
			xml += "</Node>\n";
		}
		xml += "</AINodes>\n";
		FILE* f = fopen(file, "w");
		fputs(xml.c_str(), f);
		fclose(f);
		for(size_t i = 0; i < pos.size(); ++i) c.AddNode(names[i], ids[i], pos[i], NULL);
		printf("ranks");
		for(size_t i = 0; i < pos.size(); ++i)
		{
			int rank = 0;
			for(size_t j = 0; j < pos.size(); ++j) rank += (char*)c.GetNode((int)j) < (char*)c.GetNode((int)i);
			printf(" %d", rank);
		}
		printf("\n");
		c.LoadFromFile(cString::To16Char(file));
		for(int i = 0; i < c.GetNodeNum(); ++i)
		{
			cAINode* n = c.GetNode(i);
			printf("node %d %s %d %08x %08x %08x\n", i, n->GetName().c_str(), n->GetID(), Fb(n->GetPosition().x), Fb(n->GetPosition().y), Fb(n->GetPosition().z));
			for(int e = 0; e < n->GetEdgeNum(); ++e)
				printf("edge %d %d %08x %08x\n", i, n->GetEdge(e)->mpNode->GetIdx(), Fb(n->GetEdge(e)->mfDistance), Fb(n->GetEdge(e)->mfSqrDistance));
		}
		for(int q = 0; q < 40; ++q)
		{
			int k = (int)Rnd(100);
			if(k < 20)
			{
				std::string nm = "n" + std::to_string((int)Rnd((unsigned)pos.size() + 3));
				cAINode* n = c.GetNodeFromName(nm);
				printf("name %s -> %d\n", nm.c_str(), n ? n->GetIdx() : -1);
			}
			else if(k < 40)
			{
				int id = 100 + (int)Rnd((unsigned)pos.size() + 3);
				cAINode* n = c.GetNodeFromID(id);
				printf("id %d -> %d\n", id, n ? n->GetIdx() : -1);
			}
			else if(k < 55)
			{
				cVector3f p(RndF(-1, (float)w), 0, RndF(-1, (float)hgt));
				float r = RndF(0.2f, 3);
				printf("iter %08x %08x %08x:", Fb(p.x), Fb(p.y), Fb(p.z));
				cAINodeIterator it = c.GetNodeIterator(p, r);
				// the query radius goes in the second line so replay can parse it
				while(it.HasNext()) printf(" %d", it.Next()->GetIdx());
				printf(" r %08x\n", Fb(r));
			}
			else
			{
				cVector3f a(RndF(-1, (float)w), 0, RndF(-1, (float)hgt)), g(RndF(-1, (float)w), 0, RndF(-1, (float)hgt));
				int maxIt = Rnd(3) == 0 ? (int)Rnd(20) : -1;
				int mod = Rnd(2) ? 0 : 2 + (int)Rnd(4);
				cAStarHandler as(&c);
				as.SetMaxIterations(maxIt);
				cRejectCb cb;
				cb.mMod = mod;
				if(mod) as.SetCallback(&cb);
				tAINodeList path;
				bool ok = as.GetPath(a, g, &path);
				printf("astar %08x %08x %08x %08x %08x %08x %d %d: %d", Fb(a.x), Fb(a.y), Fb(a.z), Fb(g.x), Fb(g.y), Fb(g.z), maxIt, mod, ok);
				for(cAINode* n : path) printf(" %d", n->GetIdx());
				printf("\n");
			}
		}
	}
	return 0;
}

int hplMain(const std::string&) { return 0; }

int main(int argc, char** argv)
{
	if(argc >= 3 && !strcmp(argv[1], "sound")) return RunSound(atoi(argv[2]), argc > 3 ? atoi(argv[3]) : 80);
	if(argc >= 4 && !strcmp(argv[1], "ai")) return RunAi(atoi(argv[2]), argv[3]);
	fprintf(stderr, "usage: gen sound <seeds> [ops]\n");
	return 2;
}
