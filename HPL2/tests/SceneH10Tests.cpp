// H10 differential scenes: iEntity3D / cNode3D hierarchies and cCamera.
// Random op sequences, results printed as float bits. Run per OHPL_IMPL and compare the dumps;
// OHPL_IMPL=cpp output is recorded under rust/tests/golden/H10 and replayed by scene::tests_h10.
// OHPL_H10_DUMP=<file> (default stdout), OHPL_H10_SEEDS=<n> (default 40).
// Lines: "o ..." op, "= ..." result, "e ..." entity event, "h ..." state hash, "~ ..." not replayed.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "scene/Camera.h"
#include "scene/Entity3D.h"
#include "scene/Node3D.h"
#include "math/Math.h"
#include "math/BoundingVolume.h"

using namespace hpl;

namespace {

FILE* gOut = NULL;
uint32_t gRng = 1;

uint32_t Rnd() { gRng = gRng * 1664525u + 1013904223u; return gRng >> 8; }
int RndI(int n) { return (int)(Rnd() % (uint32_t)n); }
float RndF(float lo, float hi) { return lo + (hi - lo) * (float)(Rnd() & 0xFFFF) / 65535.0f; }

uint32_t Bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

struct Hash
{
	uint64_t h = 0xcbf29ce484222325ULL;
	void U32(uint32_t v) { for (int i = 0; i < 4; ++i) h = (h ^ ((v >> (8 * i)) & 255)) * 0x100000001b3ULL; }
	void F(float v) { U32(Bits(v)); }
	void M(const cMatrixf& m) { for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) F(m.m[r][c]); }
	void V(const cVector3f& v) { F(v.x); F(v.y); F(v.z); }
};

void PutM(const cMatrixf& m)
{
	for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) fprintf(gOut, " %08x", Bits(m.m[r][c]));
}
void PutV(const cVector3f& v) { fprintf(gOut, " %08x %08x %08x", Bits(v.x), Bits(v.y), Bits(v.z)); }

cMatrixf RndMatrix()
{
	cMatrixf m = cMath::MatrixRotate(cVector3f(RndF(-3, 3), RndF(-3, 3), RndF(-3, 3)), (eEulerRotationOrder)RndI(6));
	if (RndI(3) == 0) m = cMath::MatrixMul(m, cMath::MatrixScale(cVector3f(RndF(0.5f, 2), RndF(0.5f, 2), RndF(0.5f, 2))));
	m.SetTranslation(cVector3f(RndF(-100, 100), RndF(-100, 100), RndF(-100, 100)));
	return m;
}
cVector3f RndVec() { return cVector3f(RndF(-50, 50), RndF(-50, 50), RndF(-50, 50)); }

void OpM(const char* name, int id, const cMatrixf& m, int extra = -1)
{
	fprintf(gOut, "o %s %d", name, id);
	if (extra >= 0) fprintf(gOut, " %d", extra);
	PutM(m);
	fprintf(gOut, "\n");
}
void OpV(const char* name, int id, const cVector3f& v, int extra = -1)
{
	fprintf(gOut, "o %s %d", name, id);
	PutV(v);
	if (extra >= 0) fprintf(gOut, " %d", extra);
	fprintf(gOut, "\n");
}

class cH10Ent : public iEntity3D
{
public:
	cH10Ent(int alId) : iEntity3D("h10ent"), mlId(alId), mbRead(false) {}
	tString GetEntityType() { return "H10"; }
	cBoundingVolume& Bv() { return mBoundingVolume; }
	void SetApply(bool abX) { mbApplyTransformToBV = abX; }
	void Mix(::Hash& h)
	{
		h.M(m_mtxLocalTransform);
		h.M(m_mtxWorldTransform);
		h.U32(mbTransformUpdated); h.U32(mlCount); h.U32(mbUpdateBoundingVolume);
	}
	int mlId;
	bool mbRead;
protected:
	void OnTransformUpdated()
	{
		fprintf(gOut, "e u %d\n", mlId);
		if (mbRead) { cMatrixf& m = GetWorldMatrix(); fprintf(gOut, "= r"); PutM(m); fprintf(gOut, "\n"); }
	}
};

class cH10Cb : public iEntityCallback
{
public:
	void OnTransformUpdate(iEntity3D* apEntity) { fprintf(gOut, "e c %d\n", static_cast<cH10Ent*>(apEntity)->mlId); }
};

void HashNode(Hash& h, cNode3D* n)
{
	h.M(n->GetLocalMatrix());
	h.M(n->GetWorldMatrix());
}

void RunEntityScene(int seed)
{
	gRng = (uint32_t)seed * 2654435761u + 12345u;
	const int N = 3 + RndI(6), E = 3 + RndI(6), ops = 300;
	fprintf(gOut, "scene %d %d %d\n", seed, N, E);

	std::vector<cNode3D*> nodes;
	std::vector<cH10Ent*> ents;
	for (int i = 0; i < N; ++i) nodes.push_back(new cNode3D("n", false));
	for (int i = 0; i < E; ++i) ents.push_back(new cH10Ent(i));
	cH10Cb cb;

	std::vector<int> nParent(N, -1), nEParent(N, -1), eNode(E, -1), eParent(E, -1);
	std::vector<bool> eCb(E, false);

	for (int k = 0; k < ops; ++k)
	{
		int r = RndI(100);
		int n = RndI(N), e = RndI(E);
		if (r < 8)
		{
			int c = 1 + RndI(N - 1), p = RndI(c);
			if (nParent[c] != -1 || nEParent[c] != -1) continue;
			fprintf(gOut, "o nsp %d %d\n", c, p);
			nodes[c]->SetParent(nodes[p]);
			nParent[c] = p;
		}
		else if (r < 16)
		{
			if (eNode[e] != -1 || n > e) continue;
			fprintf(gOut, "o nae %d %d\n", n, e);
			fprintf(gOut, "= b %d\n", nodes[n]->AddEntity(ents[e]) ? 1 : 0);
			eNode[e] = n;
		}
		else if (r < 22)
		{
			int c = 1 + RndI(E - 1), p = RndI(c);
			fprintf(gOut, "o eac %d %d\n", p, c);
			ents[p]->AddChild(ents[c]);
			eParent[c] = p;
		}
		else if (r < 27)
		{
			if (nParent[n] != -1 || n <= e) continue;
			fprintf(gOut, "o enc %d %d\n", e, n);
			ents[e]->AddNodeChild(nodes[n]);
			nEParent[n] = e;
		}
		else if (r < 30)
		{
			if (eNode[e] == -1) continue;
			fprintf(gOut, "o nre %d %d\n", eNode[e], e);
			fprintf(gOut, "= b %d\n", nodes[eNode[e]]->RemoveEntity(ents[e]) ? 1 : 0);
			eNode[e] = -1;
		}
		else if (r < 32)
		{
			if (eParent[e] == -1) continue;
			fprintf(gOut, "o erc %d %d\n", eParent[e], e);
			ents[eParent[e]]->RemoveChild(ents[e]);
			eParent[e] = -1;
		}
		else if (r < 34)
		{
			if (nEParent[n] == -1) continue;
			fprintf(gOut, "o ern %d %d\n", nEParent[n], n);
			ents[nEParent[n]]->RemoveNodeChild(nodes[n]);
			nEParent[n] = -1;
		}
		else if (r < 36)
		{
			if (nParent[n] == -1) continue;
			fprintf(gOut, "o nrc %d %d\n", nParent[n], n);
			nodes[nParent[n]]->RemoveChild(nodes[n]);
		}
		else if (r < 40) { cVector3f v = RndVec(); OpV("ep", e, v); ents[e]->SetPosition(v); }
		else if (r < 44) { cMatrixf m = RndMatrix(); OpM("em", e, m); ents[e]->SetMatrix(m); }
		else if (r < 47) { cVector3f v = RndVec(); OpV("ewp", e, v); ents[e]->SetWorldPosition(v); }
		else if (r < 50) { cMatrixf m = RndMatrix(); OpM("ewm", e, m); ents[e]->SetWorldMatrix(m); }
		else if (r < 53) { cVector3f v = RndVec(); OpV("np", n, v); nodes[n]->SetPosition(v); }
		else if (r < 57) { cMatrixf m = RndMatrix(); int f = RndI(2); OpM("nm", n, m, f); nodes[n]->SetMatrix(m, f != 0); }
		else if (r < 60) { cVector3f v = RndVec(); OpV("nwp", n, v); nodes[n]->SetWorldPosition(v); }
		else if (r < 63) { cMatrixf m = RndMatrix(); OpM("nwm", n, m); nodes[n]->SetWorldMatrix(m); }
		else if (r < 66)
		{
			cVector3f v(RndF(-3, 3), RndF(-3, 3), RndF(-3, 3));
			int o = RndI(6);
			fprintf(gOut, "o nar %d", n); PutV(v); fprintf(gOut, " %d\n", o);
			nodes[n]->AddRotation(v, (eEulerRotationOrder)o);
		}
		else if (r < 68)
		{
			cQuaternion q;
			q.FromRotationMatrix(RndMatrix());
			fprintf(gOut, "o naq %d %08x %08x %08x %08x\n", n, Bits(q.v.x), Bits(q.v.y), Bits(q.v.z), Bits(q.w));
			nodes[n]->AddRotation(q);
		}
		else if (r < 70) { cVector3f v = RndVec(); OpV("nas", n, v); nodes[n]->AddScale(v); }
		else if (r < 72) { cVector3f v = RndVec(); OpV("nat", n, v); nodes[n]->AddTranslation(v); }
		else if (r < 75) { int f = RndI(2); fprintf(gOut, "o num %d %d\n", n, f); nodes[n]->UpdateMatrix(f != 0); }
		else if (r < 77)
		{
			cMatrixf m = RndMatrix(); int on = RndI(2), post = RndI(2);
			fprintf(gOut, "o npt %d %d %d", n, post, on); PutM(m); fprintf(gOut, "\n");
			if (post) { nodes[n]->SetPostTransform(m); nodes[n]->SetUsePostTransform(on != 0); }
			else { nodes[n]->SetPreTransform(m); nodes[n]->SetUsePreTransform(on != 0); }
		}
		else if (r < 80)
		{
			int post = RndI(2), f = RndI(2);
			fprintf(gOut, "o nap %d %d %d\n", n, post, f);
			if (post) nodes[n]->ApplyPostAnimTransform(f != 0); else nodes[n]->ApplyPreAnimTransform(f != 0);
		}
		else if (r < 83)
		{
			if (eCb[e]) { fprintf(gOut, "o ecr %d\n", e); ents[e]->RemoveCallback(&cb); eCb[e] = false; }
			else { fprintf(gOut, "o eca %d\n", e); ents[e]->AddCallback(&cb); eCb[e] = true; }
		}
		else if (r < 86)
		{
			cVector3f a(RndF(-5, 0), RndF(-5, 0), RndF(-5, 0)), b(RndF(0, 5), RndF(0, 5), RndF(0, 5));
			fprintf(gOut, "o ebv %d", e); PutV(a); PutV(b); fprintf(gOut, "\n");
			ents[e]->Bv().SetLocalMinMax(a, b);
		}
		else if (r < 88) { int f = RndI(2); fprintf(gOut, "o eap %d %d\n", e, f); ents[e]->SetApply(f != 0); }
		else if (r < 90) { int f = RndI(2); fprintf(gOut, "o ere %d %d\n", e, f); ents[e]->mbRead = f != 0; }
		else if (r < 92) { int f = RndI(2); fprintf(gOut, "o etu %d %d\n", e, f); ents[e]->SetTransformUpdated(f != 0); }
		else if (r < 94) { fprintf(gOut, "o qew %d\n= w", e); PutM(ents[e]->GetWorldMatrix()); fprintf(gOut, "\n"); }
		else if (r < 95) { fprintf(gOut, "o qewp %d\n= p", e); PutV(ents[e]->GetWorldPosition()); fprintf(gOut, "\n"); }
		else if (r < 96) { fprintf(gOut, "o qel %d\n= l", e); PutM(ents[e]->GetLocalMatrix()); fprintf(gOut, "\n"); }
		else if (r < 97) { fprintf(gOut, "o qnw %d\n= w", n); PutM(nodes[n]->GetWorldMatrix()); fprintf(gOut, "\n"); }
		else if (r < 98) { fprintf(gOut, "o qnwp %d\n= p", n); PutV(nodes[n]->GetWorldPosition()); fprintf(gOut, "\n"); }
		else if (r < 99)
		{
			fprintf(gOut, "o qbv %d\n", e);
			cBoundingVolume* bv = ents[e]->GetBoundingVolume();
			fprintf(gOut, "= bvt"); PutM(bv->GetTransform()); fprintf(gOut, "\n");
			fprintf(gOut, "~ bv"); PutV(bv->GetMin()); PutV(bv->GetMax()); PutV(bv->GetWorldCenter());
			fprintf(gOut, " %08x\n", Bits(bv->GetRadius()));
		}
		else
		{
			fprintf(gOut, "o qs %d\n= s %d %d\n", e, ents[e]->GetTransformUpdated() ? 1 : 0, ents[e]->GetTransformUpdateCount());
		}
	}

	fprintf(gOut, "o end\n");
	Hash h;
	for (int i = 0; i < N; ++i)
	{
		Hash o;
		HashNode(o, nodes[i]);
		fprintf(gOut, "h n %d %016llx\n", i, (unsigned long long)o.h);
		HashNode(h, nodes[i]);
	}
	for (int i = 0; i < E; ++i)
	{
		ents[i]->GetBoundingVolume();
		Hash o;
		ents[i]->Mix(o);
		fprintf(gOut, "h e %d %016llx %d %d\n", i, (unsigned long long)o.h, ents[i]->GetTransformUpdated() ? 1 : 0, ents[i]->GetTransformUpdateCount());
		ents[i]->Mix(h);
	}
	fprintf(gOut, "h %016llx\n", (unsigned long long)h.h);

	for (int i = E - 1; i >= 0; --i) delete ents[i];
	for (int i = N - 1; i >= 0; --i) delete nodes[i];
}

void CamHash(cCamera& c, const char* tag)
{
	Hash h;
	h.V(c.GetPosition()); h.F(c.GetFOV()); h.F(c.GetAspect()); h.F(c.GetFarClipPlane()); h.F(c.GetNearClipPlane());
	h.F(c.GetOrthoViewSize().x); h.F(c.GetOrthoViewSize().y); h.U32(c.GetProjectionType());
	h.F(c.GetPitch()); h.F(c.GetYaw()); h.F(c.GetRoll());
	h.F(c.GetPitchMinLimit()); h.F(c.GetPitchMaxLimit()); h.F(c.GetYawMinLimit()); h.F(c.GetYawMaxLimit());
	h.U32(c.GetRotateMode()); h.U32(c.GetMoveMode()); h.U32(c.GetInifintiveFarPlane());
	h.M(c.GetPrevView()); h.M(c.GetPrevProjection()); h.M(c.GetRotationMatrix());
	h.V(c.GetAttachmentNode()->GetLocalPosition());
	fprintf(gOut, "%s %016llx\n", tag, (unsigned long long)h.h);
}

void RunCameraScene(int seed)
{
	gRng = (uint32_t)seed * 2246822519u + 777u;
	fprintf(gOut, "cscene %d\n", seed);
	cCamera cam;
	cam.SetPrevView(cMatrixf::Identity);
	cam.SetPrevProjection(cMatrixf::Identity);
	for (int k = 0; k < 400; ++k)
	{
		int r = RndI(100);
		float a = RndF(-3, 3);
		if (r < 4) { cVector3f v = RndVec(); OpV("cpos", 0, v); cam.SetPosition(v); }
		else if (r < 8) { fprintf(gOut, "o cpitch %08x\n", Bits(a)); cam.SetPitch(a); }
		else if (r < 12) { fprintf(gOut, "o cyaw %08x\n", Bits(a)); cam.SetYaw(a); }
		else if (r < 15) { fprintf(gOut, "o croll %08x\n", Bits(a)); cam.SetRoll(a); }
		else if (r < 19) { fprintf(gOut, "o capitch %08x\n", Bits(a)); cam.AddPitch(a); }
		else if (r < 23) { fprintf(gOut, "o cayaw %08x\n", Bits(a)); cam.AddYaw(a); }
		else if (r < 26) { fprintf(gOut, "o caroll %08x\n", Bits(a)); cam.AddRoll(a); }
		else if (r < 30) { fprintf(gOut, "o cmove 0 %08x\n", Bits(a * 10)); cam.MoveForward(a * 10); }
		else if (r < 34) { fprintf(gOut, "o cmove 1 %08x\n", Bits(a * 10)); cam.MoveRight(a * 10); }
		else if (r < 38) { fprintf(gOut, "o cmove 2 %08x\n", Bits(a * 10)); cam.MoveUp(a * 10); }
		else if (r < 41) { float v = RndI(4) == 0 ? cam.GetFOV() : RndF(0.5f, 2.5f); fprintf(gOut, "o cfov %08x\n", Bits(v)); cam.SetFOV(v); }
		else if (r < 44) { float v = RndI(4) == 0 ? cam.GetAspect() : RndF(0.5f, 2.5f); fprintf(gOut, "o caspect %08x\n", Bits(v)); cam.SetAspect(v); }
		else if (r < 46) { float v = RndF(100, 2000); fprintf(gOut, "o cfar %08x\n", Bits(v)); cam.SetFarClipPlane(v); }
		else if (r < 48) { float v = RndF(0.01f, 1); fprintf(gOut, "o cnear %08x\n", Bits(v)); cam.SetNearClipPlane(v); }
		else if (r < 51) { int t = RndI(2); fprintf(gOut, "o cproj %d\n", t); cam.SetProjectionType((eProjectionType)t); }
		else if (r < 54) { cVector2f v(RndF(1, 20), RndF(1, 20)); fprintf(gOut, "o cortho %08x %08x\n", Bits(v.x), Bits(v.y)); cam.SetOrthoViewSize(v); }
		else if (r < 57) { int f = RndI(2); fprintf(gOut, "o cinf %d\n", f); cam.SetInifintiveFarPlane(f != 0); }
		else if (r < 60) { int m = RndI(2); fprintf(gOut, "o crot %d\n", m); cam.SetRotateMode((eCameraRotateMode)m); }
		else if (r < 62) { int m = RndI(2); fprintf(gOut, "o cmm %d\n", m); cam.SetMoveMode((eCameraMoveMode)m); }
		else if (r < 63) { fprintf(gOut, "o creset\n"); cam.ResetRotation(); }
		else if (r < 66) { cVector3f v = RndVec(); fprintf(gOut, "o crow 2"); PutV(v); fprintf(gOut, "\n"); cam.SetForward(v); }
		else if (r < 68) { cVector3f v = RndVec(); fprintf(gOut, "o crow 0"); PutV(v); fprintf(gOut, "\n"); cam.SetRight(v); }
		else if (r < 70) { cVector3f v = RndVec(); fprintf(gOut, "o crow 1"); PutV(v); fprintf(gOut, "\n"); cam.SetUp(v); }
		else if (r < 72) { cMatrixf m = RndMatrix(); fprintf(gOut, "o crm"); PutM(m); fprintf(gOut, "\n"); cam.SetRotationMatrix(m); }
		else if (r < 74)
		{
			float lo = RndF(-1.5f, 0), hi = RndF(0, 1.5f);
			if (RndI(3) == 0) lo = hi = 0;
			fprintf(gOut, "o clim %08x %08x\n", Bits(lo), Bits(hi));
			cam.SetPitchLimits(lo, hi);
			cam.SetYawLimits(lo * 2, hi * 2);
		}
		else if (r < 78) { fprintf(gOut, "o cqv\n= v"); PutM(cam.GetViewMatrix()); fprintf(gOut, "\n"); }
		else if (r < 82) { fprintf(gOut, "o cqp\n= p"); PutM(cam.GetProjectionMatrix()); fprintf(gOut, "\n"); }
		else if (r < 85) { fprintf(gOut, "o cqm\n= m"); PutM(cam.GetMoveMatrix()); fprintf(gOut, "\n"); }
		else if (r < 88)
		{
			fprintf(gOut, "o cqd\n= d"); PutV(cam.GetForward()); PutV(cam.GetRight()); PutV(cam.GetUp()); fprintf(gOut, "\n");
		}
		else if (r < 94)
		{
			fprintf(gOut, "o cqf\n");
			cFrustum* f = cam.GetFrustum();
			fprintf(gOut, "~ f %08x %08x %d", Bits(f->GetFarPlane()), Bits(f->GetNearPlane()), f->GetInfFarPlane() ? 1 : 0);
			for (int i = 0; i < 6; ++i)
			{
				const cPlanef& p = f->GetPlane((eFrustumPlane)i);
				fprintf(gOut, " %08x %08x %08x %08x", Bits(p.a), Bits(p.b), Bits(p.c), Bits(p.d));
			}
			fprintf(gOut, "\n");
			fprintf(gOut, "= fi %d\n", cam.GetInifintiveFarPlane() ? 1 : 0);
		}
		else if (r < 97)
		{
			cVector2f s(RndF(0, 1), RndF(0, 1));
			fprintf(gOut, "o cqu %08x %08x\n", Bits(s.x), Bits(s.y));
			cVector3f p(0, 0, 0), d(0, 0, 0);
			cam.UnProject(&p, &d, s, cVector2f(1, 1));
			fprintf(gOut, "~ u"); PutV(p); PutV(d); fprintf(gOut, "\n");
		}
		else
		{
			fprintf(gOut, "o cqt\n= t %d\n", cam.GetAttachmentNode()->GetLocalPosition().x == cam.GetPosition().x ? 1 : 0);
		}
		CamHash(cam, "h");
	}
	fprintf(gOut, "o cend\n");
}

} // namespace

int hplMain(const hpl::tString&)
{
	const char* path = std::getenv("OHPL_H10_DUMP");
	gOut = path ? fopen(path, "w") : stdout;
	const char* n = std::getenv("OHPL_H10_SEEDS");
	int seeds = n ? atoi(n) : 40;
	for (int s = 1; s <= seeds; ++s) RunEntityScene(s);
	for (int s = 1; s <= seeds / 2; ++s) RunCameraScene(s);
	if (path) fclose(gOut);
	return 0;
}
