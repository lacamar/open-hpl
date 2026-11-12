// H6 differential harness: seeded scripts against cSkeleton/cBone, cAnimation/cAnimationTrack and
// cMesh. Prints "<op>\t<result>" per line; OHPL_IMPL=cpp and =rust must print the same text.
// usage: H6_SCENARIO=skel|anim|mesh H6_SEED=<n> h6_script
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "graphics/Animation.h"
#include "graphics/AnimationTrack.h"
#include "graphics/Bone.h"
#include "graphics/Mesh.h"
#include "graphics/Skeleton.h"
#include "graphics/SubMesh.h"

using namespace hpl;

int hplMain(const std::string&) { return 0; }

static unsigned long long gRng;
static unsigned Rnd(unsigned n)
{
	gRng = gRng * 6364136223846793005ull + 1442695040888963407ull;
	return (unsigned)((gRng >> 33) % n);
}

static unsigned Bits(float f) { unsigned u; memcpy(&u, &f, 4); return u; }
static float RndF() { return ((int)Rnd(2001) - 1000) / 64.0f; }

static std::string Hex(float f) { char b[12]; snprintf(b, sizeof b, "%08x", Bits(f)); return b; }

static std::string Mtx(const cMatrixf& m)
{
	std::string s;
	for(int i = 0; i < 16; ++i) s += (i ? "," : "") + Hex(m.v[i]);
	return s;
}

static cMatrixf RndMtx()
{
	cMatrixf m;
	for(int i = 0; i < 16; ++i) m.v[i] = RndF();
	if(Rnd(2)) { m.v[12] = m.v[13] = m.v[14] = 0; m.v[15] = 1; }
	return m;
}

static const char* kNames[] = { "root", "hip", "spine", "arm_l", "arm_r", "hip", "head", "leg" };
static const char* kSids[] = { "", "s1", "s2", "s1", "s3", "", "s4", "s2" };

static void Line(const std::string& op, const std::string& res) { printf("%s\t%s\n", op.c_str(), res.empty() ? "." : res.c_str()); }

static int Skel()
{
	cSkeleton sk;
	std::vector<cBone*> bones;
	bones.push_back(sk.GetRootBone());
	auto IdOf = [&](cBone* b) { if(!b) return -1; for(size_t i = 0; i < bones.size(); ++i) if(bones[i] == b) return (int)i; return -2; };
	char op[256];
	int n = 40 + Rnd(80);
	for(int i = 0; i < n; ++i)
	{
		int k = Rnd(bones.size());
		switch(Rnd(16))
		{
		case 0: case 1: case 2:
		{
			int nm = Rnd(8), sd = Rnd(8);
			snprintf(op, sizeof op, "child %d %s %s", k, kNames[nm], kSids[sd][0] ? kSids[sd] : "-");
			bones.push_back(bones[k]->CreateChildBone(kNames[nm], kSids[sd]));
			Line(op, "id " + std::to_string(bones.size() - 1));
			// a fresh bone's matrices are uninitialised in C++
			cMatrixf m = RndMtx();
			bones.back()->SetTransform(m);
			snprintf(op, sizeof op, "tf %d %s", (int)bones.size() - 1, Mtx(m).c_str());
			Line(op, "");
			break;
		}
		case 3: case 4: { cMatrixf m = RndMtx(); bones[k]->SetTransform(m); snprintf(op, sizeof op, "tf %d %s", k, Mtx(m).c_str()); Line(op, ""); break; }
		case 5: { cMatrixf m = RndMtx(); bones[k]->SetTransformUnscaled(m); snprintf(op, sizeof op, "utf %d %s", k, Mtx(m).c_str()); Line(op, ""); break; }
		case 6: { cMatrixf m = RndMtx(); bones[k]->SetInvBindTransform(m); snprintf(op, sizeof op, "ibind %d %s", k, Mtx(m).c_str()); Line(op, ""); break; }
		case 7: snprintf(op, sizeof op, "local %d", k); Line(op, Mtx(bones[k]->GetLocalTransform())); break;
		case 8: snprintf(op, sizeof op, "world %d", k); Line(op, Mtx(bones[k]->GetWorldTransform())); break;
		case 9: snprintf(op, sizeof op, "inv %d", k); Line(op, Mtx(bones[k]->GetInvWorldTransform())); break;
		case 10: snprintf(op, sizeof op, "ibindq %d", k); Line(op, Mtx(bones[k]->GetInvBindTransform())); break;
		case 11:
			// unscaled world of bones that never got an unscaled local is uninitialised in C++
			if(k == 0) { snprintf(op, sizeof op, "uworld %d", k); Line(op, Mtx(bones[k]->GetWorldTransformUnscaled())); }
			break;
		case 12:
		{
			int nm = Rnd(8), sd = Rnd(8);
			snprintf(op, sizeof op, "idx %s", kNames[nm]);
			Line(op, std::to_string(sk.GetBoneIndexByName(kNames[nm])) + " " + std::to_string(IdOf(sk.GetBoneByName(kNames[nm]))));
			snprintf(op, sizeof op, "sidx %s", kSids[sd][0] ? kSids[sd] : "-");
			Line(op, std::to_string(sk.GetBoneIndexBySid(kSids[sd])) + " " + std::to_string(IdOf(sk.GetBoneBySid(kSids[sd]))));
			break;
		}
		case 13:
			if(sk.GetBoneNum() > 0)
			{
				int idx = Rnd(sk.GetBoneNum());
				snprintf(op, sizeof op, "byidx %d", idx);
				Line(op, std::to_string(IdOf(sk.GetBoneByIndex(idx))) + " of " + std::to_string(sk.GetBoneNum()));
			}
			break;
		case 14: snprintf(op, sizeof op, "num"); Line(op, std::to_string(sk.GetBoneNum())); break;
		case 15:
			if(Rnd(3) == 0 && bones.size() > 1) { snprintf(op, sizeof op, "detach %d", k); bones[k]->Detach(); Line(op, "parent " + std::to_string(IdOf(bones[k]->GetParent()))); }
			break;
		}
	}
	snprintf(op, sizeof op, "uworld 0");
	Line(op, Mtx(bones[0]->GetWorldTransformUnscaled()));
	return 0;
}

static int Anim()
{
	cAnimation an("a", L"", "f");
	float len = 0.5f + Rnd(40) / 4.0f;
	an.SetLength(len);
	char op[256];
	snprintf(op, sizeof op, "length %s", Hex(len).c_str());
	Line(op, "");
	std::vector<cAnimationTrack*> tr;
	std::vector<std::string> names;
	auto IdxOf = [](cAnimationTrack* t, cKeyFrame* f) { if(!f) return -1; for(int i = 0; i < t->GetKeyFrameNum(); ++i) if(t->GetKeyFrame(i) == f) return i; return -2; };
	auto Kf = [](const cKeyFrame& f) {
		return Hex(f.time) + " " + Hex(f.trans.x) + "," + Hex(f.trans.y) + "," + Hex(f.trans.z) + " " +
			Hex(f.rotation.v.x) + "," + Hex(f.rotation.v.y) + "," + Hex(f.rotation.v.z) + "," + Hex(f.rotation.w);
	};
	tr.push_back(an.CreateTrack("t0", eAnimTransformFlag_Rotate)); names.push_back("t0");
	Line("mk t0", "0");
	int n = 60 + Rnd(120);
	for(int i = 0; i < n; ++i)
	{
		int t = Rnd(tr.size());
		switch(Rnd(12))
		{
		case 0:
			if(tr.size() < 4) { std::string nm = "t" + std::to_string(Rnd(4)); tr.push_back(an.CreateTrack(nm, eAnimTransformFlag_Translate)); names.push_back(nm); snprintf(op, sizeof op, "mk %s", nm.c_str()); Line(op, std::to_string(tr.size() - 1)); }
			break;
		case 1: case 2: case 3: case 4:
		{
			float time = Rnd(8) == 0 ? Rnd(60) / 4.0f - 2.0f : len * Rnd(1001) / 1000.0f;
			cKeyFrame* f = tr[t]->CreateKeyFrame(time);
			snprintf(op, sizeof op, "create %d %s", t, Hex(time).c_str());
			Line(op, std::to_string(IdxOf(tr[t], f)) + " " + Kf(*f));
			// writes through the returned address must stick
			f->trans = cVector3f(RndF(), RndF(), RndF());
			cVector3f ax(RndF(), RndF(), RndF() + 0.5f);
			f->rotation = cQuaternion(RndF() / 16.0f, ax);
			if(Rnd(4) == 0) f->rotation.w = -f->rotation.w;
			snprintf(op, sizeof op, "set %d %d %s", t, IdxOf(tr[t], f), Kf(*f).c_str());
			Line(op, "");
			break;
		}
		case 5: case 6: case 7:
		{
			float time = Rnd(10) == 0 ? (Rnd(2) ? -1.0f : len + 3.0f) : len * Rnd(1001) / 1000.0f;
			snprintf(op, sizeof op, "interp %d %s", t, Hex(time).c_str());
			Line(op, Kf(tr[t]->GetInterpolatedKeyFrame(time)));
			break;
		}
		case 8: case 9:
		{
			float time = len * Rnd(1001) / 1000.0f;
			cKeyFrame *a = (cKeyFrame*)1, *b = (cKeyFrame*)1;
			float w = tr[t]->GetKeyFramesAtTime(time, &a, &b);
			snprintf(op, sizeof op, "at %d %s", t, Hex(time).c_str());
			Line(op, Hex(w) + " " + std::to_string(IdxOf(tr[t], a)) + " " + std::to_string(IdxOf(tr[t], b)));
			break;
		}
		case 10:
			if(Rnd(6) == 0) { tr[t]->ClearKeyFrames(); snprintf(op, sizeof op, "clear %d", t); Line(op, ""); }
			break;
		case 11:
		{
			snprintf(op, sizeof op, "cnt %d", t);
			Line(op, std::to_string(tr[t]->GetKeyFrameNum()));
			std::string nm = "t" + std::to_string(Rnd(5));
			snprintf(op, sizeof op, "byname %s", nm.c_str());
			cAnimationTrack* f = an.GetTrackByName(nm);
			int idx = -1;
			for(size_t j = 0; j < tr.size(); ++j) if(tr[j] == f) { idx = (int)j; break; }
			Line(op, std::to_string(idx) + " of " + std::to_string(an.GetTrackNum()));
			break;
		}
		}
	}
	return 0;
}

static int MeshScenario()
{
	cMesh mesh("m", L"", NULL, NULL);
	char op[256];
	std::vector<cSubMesh*> subs;
	std::vector<cAnimation*> anims;
	static const char* sn[] = { "a", "b", "c", "a", "d" };
	auto Sub = [&](cSubMesh* s) { if(!s) return -1; for(size_t i = 0; i < subs.size(); ++i) if(subs[i] == s) return (int)i; return -2; };
	int n = 30 + Rnd(60);
	for(int i = 0; i < n; ++i)
	{
		const char* nm = sn[Rnd(5)];
		switch(Rnd(8))
		{
		case 0: case 1: subs.push_back(mesh.CreateSubMesh(nm)); snprintf(op, sizeof op, "sub %s", nm); Line(op, std::to_string(subs.size() - 1)); break;
		case 2: case 3:
			snprintf(op, sizeof op, "subq %s", nm);
			Line(op, std::to_string(mesh.GetSubMeshIndex(nm)) + " " + std::to_string(Sub(mesh.GetSubMeshName(nm))) + " " + std::to_string(mesh.GetSubMeshNum()));
			break;
		case 4: case 5:
		{
			cAnimation* a = new cAnimation(nm, L"", "f");
			anims.push_back(a);
			mesh.AddAnimation(a);
			snprintf(op, sizeof op, "anim %s", nm);
			Line(op, std::to_string(mesh.GetAnimationNum()));
			break;
		}
		case 6:
		{
			snprintf(op, sizeof op, "animq %s", nm);
			int idx = mesh.GetAnimationIndex(nm);
			cAnimation* f = mesh.GetAnimationFromName(nm);
			int fi = -1;
			for(size_t j = 0; j < anims.size(); ++j) if(anims[j] == f) { fi = (int)j; break; }
			Line(op, std::to_string(idx) + " " + std::to_string(fi));
			break;
		}
		case 7:
			if(Rnd(8) == 0) { mesh.ClearAnimations(false); anims.clear(); snprintf(op, sizeof op, "clr"); Line(op, std::to_string(mesh.GetAnimationNum())); }
			break;
		}
	}
	return 0;
}

int main()
{
	const char* sc = getenv("H6_SCENARIO");
	const char* sd = getenv("H6_SEED");
	if(!sc || !sd) return 2;
	gRng = (unsigned long long)atoi(sd) * 7919 + 17;
	if(!strcmp(sc, "skel")) return Skel();
	if(!strcmp(sc, "anim")) return Anim();
	return MeshScenario();
}
