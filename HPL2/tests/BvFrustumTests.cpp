// cBoundingVolume / cFrustum differential: random op sequences, results printed as float bits.
// The recorded C++ output is rust/tests/golden/BV/dump.txt, replayed by math::tests_bv.
// OHPL_BV_DUMP=<file> (default stdout), OHPL_BV_SEEDS=<n> (default 24).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "math/Math.h"
#include "math/BoundingVolume.h"
#include "math/Frustum.h"
#include "scene/RenderableContainer.h"

using namespace hpl;

namespace {

FILE* gOut = NULL;
uint32_t gRng = 1;

uint32_t Rnd() { gRng = gRng * 1664525u + 1013904223u; return gRng >> 8; }
int RndI(int n) { return (int)(Rnd() % (uint32_t)n); }
float RndF(float lo, float hi) { return lo + (hi - lo) * (float)(Rnd() & 0xFFFF) / 65535.0f; }
uint32_t Bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

void PutF(float f) { fprintf(gOut, " %08x", Bits(f)); }
void PutV(const cVector3f& v) { PutF(v.x); PutF(v.y); PutF(v.z); }
void PutM(const cMatrixf& m) { for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) PutF(m.m[r][c]); }
void PutP(const cPlanef& p) { PutF(p.a); PutF(p.b); PutF(p.c); PutF(p.d); }
void End() { fputc('\n', gOut); }

cVector3f RndV3(float lo, float hi) { float x = RndF(lo, hi); float y = RndF(lo, hi); float z = RndF(lo, hi); return cVector3f(x, y, z); }
cVector3f RndVec(float r) { return RndV3(-r, r); }
cMatrixf RndMatrix(bool scale)
{
	cVector3f rot = RndV3(-3, 3); int order = RndI(6);
	cMatrixf m = cMath::MatrixRotate(rot, (eEulerRotationOrder)order);
	if (scale) m = cMath::MatrixMul(m, cMath::MatrixScale(RndV3(0.5f, 2)));
	m.SetTranslation(RndVec(100));
	return m;
}

void PutBvFields(cBoundingVolume& b)
{
	PutM(b.m_mtxTransform);
	PutV(b.mvLocalMax); PutV(b.mvLocalMin); PutV(b.mvMax); PutV(b.mvMin); PutV(b.mvPivot);
	PutV(b.mvWorldMax); PutV(b.mvWorldMin); PutV(b.mvPosition); PutV(b.mvSize); PutF(b.mfRadius);
}

void RunBv(int seed)
{
	cBoundingVolume b;
	float arr[2][40];
	for (int step = 0; step < 40; ++step)
	{
		int op = step == 0 ? 0 : RndI(11);
		fprintf(gOut, "b %d %d %d", seed, step, op);
		switch (op)
		{
		case 0: { cVector3f lo = RndVec(20); cVector3f hi = lo + RndV3(0, 30); b.SetLocalMinMax(lo, hi); PutV(b.GetMax()); PutV(b.GetMin()); } break;
		case 1: b.SetSize(RndV3(0.1f, 20)); break;
		case 2: b.SetPosition(RndVec(100)); break;
		case 3: b.SetTransform(RndMatrix(RndI(2) == 0)); break;
		case 4: PutV(b.GetMax()); PutV(b.GetMin()); break;
		case 5: PutV(b.GetWorldCenter()); PutF(b.GetRadius()); break;
		case 6: PutV(b.GetSize()); PutV(b.GetLocalCenter()); PutV(b.GetLocalMax()); PutV(b.GetLocalMin()); break;
		case 7: PutV(b.GetPosition()); PutM(b.GetTransform()); break;
		case 8: case 9:
		{
			int stride = 3 + RndI(2);
			int arrays = 1 + RndI(2);
			for (int a = 0; a < arrays; ++a)
			{
				int n = 1 + RndI(8);
				for (int i = 0; i < n * stride; ++i) arr[a][i] = RndF(-30, 30);
				b.AddArrayPoints(arr[a], n);
			}
			if (op == 8) { cBoundingVolume c = b; b.CreateFromPoints(stride); c.CreateFromPoints(stride); PutBvFields(c); }
			else b.CreateFromPoints(stride);
		} break;
		case 10: { cBoundingVolume c(b); c.SetPosition(RndVec(10)); PutV(c.GetMax()); PutF(c.GetRadius()); PutV(b.GetMax()); } break;
		}
		PutBvFields(b);
		End();
	}
}

struct Node : iRenderableContainerNode
{
	Node(const cVector3f& c, float r, const cVector3f& mn, const cVector3f& mx)
	{
		mS.mvCenter = c; mS.mfRadius = r; mS.mvMin = mn; mS.mvMax = mx;
	}
};

struct FBuf { alignas(16) unsigned char b[sizeof(cFrustum)]; cFrustum* f; };

void MakeFrustum(FBuf& fb)
{
	memset(fb.b, 0, sizeof(fb.b));
	fb.f = new (fb.b) cFrustum();
	cFrustum& f = *fb.f;
	cMatrixf mtxCam = RndMatrix(false);
	cMatrixf mtxView = cMath::MatrixInverse(mtxCam);
	cVector3f origin = mtxCam.GetTranslation();
	float nearP = RndF(0.05f, 0.5f), farP = RndF(20, 200);
	if (RndI(4) != 0)
	{
		float fov = RndF(0.5f, 2.0f), aspect = RndF(0.7f, 2.0f);
		bool inf = RndI(5) == 0;
		bool oblique = RndI(4) == 0;
		bool custom = RndI(4) == 0;
		cMatrixf proj = cMath::MatrixPerspectiveProjection(nearP, farP, fov, aspect, inf);
		cMatrixf customProj = cMath::MatrixPerspectiveProjection(nearP, farP * 0.5f, fov, aspect, false);
		f.SetupPerspectiveProj(proj, mtxView, farP, nearP, fov, aspect, origin, inf, custom ? &customProj : NULL, oblique);
		f.SetInvertsCullMode(RndI(2) == 0);
	}
	else
	{
		float vx = RndF(5, 40); float vy = RndF(5, 40); cVector2f vs(vx, vy);
		cMatrixf proj = cMath::MatrixOrthographicProjection(nearP, farP, vs);
		f.SetupOrthoProj(proj, mtxView, farP, nearP, vs, origin, RndI(5) == 0);
	}
}

void RunFrustum(int seed)
{
	FBuf fb[3];
	for (int i = 0; i < 3; ++i)
	{
		MakeFrustum(fb[i]);
		cFrustum& f = *fb[i].f;
		fprintf(gOut, "f %d %d", seed, i);
		PutM(f.GetProjectionMatrix()); PutM(f.GetViewMatrix());
		PutF(f.GetFarPlane()); PutF(f.GetNearPlane()); fprintf(gOut, " %d %d %d", (int)f.GetInfFarPlane(), (int)f.GetInvertsCullMode(), (int)f.GetProjectionType());
		if (f.GetProjectionType() == eProjectionType_Perspective) { PutF(f.GetFOV()); PutF(f.GetAspect()); }
		else { PutF(f.GetOrthoViewSize().x); PutF(f.GetOrthoViewSize().y); }
		for (int p = 0; p < 6; ++p) PutP(f.GetPlane((eFrustumPlane)p));
		for (int v = 0; v < 8; ++v) PutV(f.GetVertex(v));
		cSpheref s = f.GetBoundingSphere(); PutV(s.center); PutF(s.r);
		cBoundingVolume& bv = const_cast<cBoundingVolume&>(f.GetBoundingVolume());
		PutV(bv.GetMin()); PutV(bv.GetMax()); PutF(bv.GetRadius());
		PutV(f.GetOrigin()); PutV(f.GetForward());
		PutV(f.GetOriginBV()->GetMin()); PutV(f.GetOriginBV()->GetMax()); PutF(f.GetOriginBV()->GetRadius());
		End();
	}
	for (int q = 0; q < 40; ++q)
	{
		int i = RndI(3);
		cFrustum& f = *fb[i].f;
		cSpheref s = f.GetBoundingSphere();
		int op = RndI(15);
		cVector3f c;
		int mode = RndI(3);
		if (mode == 0) c = RndVec(80);
		else
		{
			int nv = (op >= 4 && op <= 7) ? 4 : 8;
			float w[8], sum = 0;
			for (int k = 0; k < nv; ++k) { w[k] = RndF(0.01f, 1); sum += w[k]; }
			c = cVector3f(0, 0, 0);
			for (int k = 0; k < nv; ++k) c = c + f.GetVertex(k) * (w[k] / sum);
			c = c + RndVec(mode == 1 ? 0.3f : 3.0f);
		}
		float r = RndF(0.05f, 8);
		cVector3f mn = c, mx = c + RndV3(0.1f, 12);
		fprintf(gOut, "q %d %d %d %d", seed, q, i, op);
		switch (op)
		{
		case 0: fprintf(gOut, " %d", (int)f.CollidePoint(c)); break;
		case 1: { cBoundingVolume b; b.SetLocalMinMax(mn - c, mx - c); b.SetPosition(c); if (RndI(2)) b.SetTransform(RndMatrix(false)); fprintf(gOut, " %d", (int)f.CollideBoundingVolume(&b)); } break;
		case 2: { Node n(c, r, mn, mx); fprintf(gOut, " %d", (int)f.CollideNode(&n)); } break;
		case 3: { int j = (i + 1 + RndI(2)) % 3; fprintf(gOut, " %d", (int)f.CollideFrustum(fb[j].f)); } break;
		case 4: fprintf(gOut, " %d", (int)f.CheckSphereNearPlaneIntersection(c, r)); break;
		case 5: fprintf(gOut, " %d", (int)f.CheckAABBNearPlaneIntersection(mn, mx)); break;
		case 6: { cBoundingVolume b; b.SetLocalMinMax(mn - c, mx - c); b.SetPosition(c); fprintf(gOut, " %d", (int)f.CheckBVNearPlaneIntersection(&b)); } break;
		case 7: { int j = (i + 1 + RndI(2)) % 3; fprintf(gOut, " %d", (int)f.CheckFrustumNearPlaneIntersection(fb[j].f)); } break;
		case 8: { cVector3f a = c, b2 = c + RndVec(60), out = RndVec(1); bool h = f.CheckLineIntersection(a, b2, out); fprintf(gOut, " %d", (int)h); PutV(out); } break;
		case 9: fprintf(gOut, " %d", (int)f.CollideFustrumSphere(c, r)); break;
		case 10: { int mp[3] = {6, 4, 2}; fprintf(gOut, " %d", (int)f.CollideSphere(c, r, mp[RndI(3)])); } break;
		case 11: { int mp[3] = {6, 4, 2}; fprintf(gOut, " %d", (int)f.CollideAABB(mn, mx, mp[RndI(3)])); } break;
		case 12: { cVector3f a = s.center, b2 = s.center + RndVec(150), out(0, 0, 0); bool h = f.CheckLineIntersection(a, b2, out); fprintf(gOut, " %d", (int)h); PutV(out); } break;
		case 13: { cVector3f a = s.center + RndVec(30), b2 = s.center + RndVec(30), out(0, 0, 0); bool h = f.CheckLineIntersection(a, b2, out); fprintf(gOut, " %d", (int)h); PutV(out); } break;
		case 14: fprintf(gOut, " %d", (int)f.CollidePoint(s.center + RndVec(s.r * 0.5f))); break;
		}
		End();
	}
	for (int i = 0; i < 3; ++i) fb[i].f->~cFrustum();
}

}

int hplMain(const hpl::tString&)
{
	const char* path = getenv("OHPL_BV_DUMP");
	gOut = path ? fopen(path, "w") : stdout;
	int seeds = getenv("OHPL_BV_SEEDS") ? atoi(getenv("OHPL_BV_SEEDS")) : 24;
	for (int s = 0; s < seeds; ++s)
	{
		gRng = (uint32_t)s * 7919u + 1u;
		RunBv(s);
		RunFrustum(s);
	}
	if (path) fclose(gOut);
	return 0;
}
