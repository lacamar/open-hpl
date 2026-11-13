// H8 differential harness. OHPL_IMPL=cpp and =rust must print the same text.
//   H8_SCENARIO=list H8_SEED=n h8_gen   cRenderList::AddObject + Compile over stub renderables
//   H8_SCENARIO=clip H8_SEED=n h8_gen   iRenderer::GetClipRectFromObject padding (C++ formula copied from Renderer.cpp)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include <vector>

#define private public
#define protected public
#include "graphics/Graphics.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/MaterialType_BasicSolid.h"
#include "graphics/MaterialType_BasicTranslucent.h"
#include "graphics/MaterialType_Decal.h"
#include "graphics/MaterialType_Water.h"
#include "graphics/Renderable.h"
#include "graphics/RenderList.h"
#include "graphics/Renderer.h"
#include "graphics/RustRenderH8.h"
#include "math/Frustum.h"
#include "math/BoundingVolume.h"
#include "math/Math.h"
#include "system/String.h"
#include "impl/RustBackends.h"
#undef private
#undef protected

using namespace hpl;

int hplMain(const std::string&) { return 0; }

static unsigned long long gRng;
static unsigned Rnd(unsigned n)
{
	gRng = gRng * 6364136223846793005ull + 1442695040888963407ull;
	return (unsigned)((gRng >> 33) % n);
}
static float RndF() { return (float)((int)Rnd(2001) - 1000) / 64.0f; }
static uint32_t Bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

struct Stub : public iRenderable
{
	Stub(const tString &n) : iRenderable(n) {}
	cMaterial *mpMat;
	cBoundingVolume mBV;
	cMatrixf mMtx, mMtxVp;
	eRenderableType mType;
	bool mbOq, mbVpOk;
	int mlMatrixCount, mlFrameUpd, mlVpUpd;
	void *mpVtx;

	cMaterial *GetMaterial() { return mpMat; }
	iVertexBuffer *GetVertexBuffer() { return (iVertexBuffer*)mpVtx; }
	cMatrixf *GetModelMatrix(cFrustum *apFrustum) { return apFrustum ? &mMtxVp : &mMtx; }
	eRenderableType GetRenderType() { return mType; }
	void UpdateGraphicsForFrame(float) { ++mlFrameUpd; }
	bool UpdateGraphicsForViewport(cFrustum*, float) { ++mlVpUpd; return mbVpOk; }
	bool UsesOcclusionQuery() { return mbOq; }
	int GetMatrixUpdateCount() { return mlMatrixCount; }
	tString GetEntityType() { return "Stub"; }
	cBoundingVolume *GetBoundingVolume() { return &mBV; }
};

static cMatrixf RndMatrix()
{
	cMatrixf m = cMatrixf::Identity;
	for(int i = 0; i < 16; ++i) m.v[i] = RndF() / 8.0f;
	return m;
}

static void PrintV(const cVector3f &v) { printf(" %08x %08x %08x", Bits(v.x), Bits(v.y), Bits(v.z)); }

static void AddRec(cRenderList &list, Stub *o, int frame, cFrustum &frustum)
{
	cMaterial *m = o->mpMat;
	iMaterialType *t = m ? m->GetType() : NULL;
	printf("A %d %d %d %d %d %d %d %08x", (int)o->mType, m != NULL, t != NULL, t && t->IsTranslucent(), t && t->IsDecal(),
		   m && m->mvTextures[eMaterialTexture_Illumination] != NULL, (int)o->mbOq, Bits(o->mfIlluminationAmount));
	cBoundingVolume *bv = o->GetBoundingVolume();
	PrintV(bv->GetMin()); PrintV(bv->GetMax()); PrintV(bv->GetWorldCenter()); PrintV(frustum.GetOrigin());
	for(int i = 0; i < 16; ++i) printf(" %08x", Bits(frustum.GetViewMatrix().v[i]));
	const int mfc = m ? m->mlRenderFrameCount : 0, ofc = o->mlRenderFrameCount;
	printf(" %d %d %d |", frame, mfc, ofc);
	const size_t before[7] = { list.mvOcclusionQueryObjects.size(), list.mvLights.size(), list.mvFogAreas.size(), list.mvSolidObjects.size(),
							   list.mvTransObjects.size(), list.mvDecalObjects.size(), list.mvIllumObjects.size() };
	const int fu = o->mlFrameUpd, vu = o->mlVpUpd;
	list.AddObject(o);
	const unsigned gates = (m && m->mlRenderFrameCount != mfc ? 1u : 0u) | (o->mlFrameUpd != fu ? 2u : 0u) | (o->mlVpUpd != vu ? 4u : 0u);
	const bool bSkip = o->mlVpUpd != vu && !o->mbVpOk;
	const size_t after[7] = { list.mvOcclusionQueryObjects.size(), list.mvLights.size(), list.mvFogAreas.size(), list.mvSolidObjects.size(),
							  list.mvTransObjects.size(), list.mvDecalObjects.size(), list.mvIllumObjects.size() };
	unsigned lists = 0;
	for(int i = 0; i < 7; ++i) if(after[i] != before[i]) lists |= 1u << i;
	if(bSkip) printf(" %u skip\n", gates);
	else printf(" %u %08x %u\n", gates, Bits(o->mfViewSpaceZ), lists);
}

static int ListScenario(int seed)
{
	gRng = (unsigned long long)seed * 7919 + 17;
	cMaterialType_SolidDiffuse solid(NULL, NULL);
	cMaterialType_Translucent trans(NULL, NULL);
	cMaterialType_Water water(NULL, NULL);
	cMaterialType_Decal decal(NULL, NULL);
	iMaterialType *types[4] = { &solid, &trans, &water, &decal };

	std::vector<cMaterial*> mats;
	for(int i = 0; i < 12; ++i)
	{
		cMaterial *m = new cMaterial("m", _W(""), NULL, NULL, types[Rnd(4)]);
		m->mAlphaMode = (eMaterialAlphaMode)Rnd(2);
		m->mlDecalSortOrder = (int)Rnd(5);
		m->mbLargeTransperantSurface = Rnd(2) != 0;
		for(int t = 0; t < eMaterialTexture_LastEnum; ++t) m->mvTextures[t] = Rnd(2) ? (iTexture*)(uintptr_t)(0x1000 + 16 * Rnd(6)) : NULL;
		for(int r = 0; r < eMaterialRenderMode_LastEnum; ++r)
		{
			for(int u = 0; u < kMaxTextureUnits; ++u) m->mvTextureInUnit[r][u] = (iTexture*)(uintptr_t)(0x2000 + 16 * Rnd(4));
			for(int s = 0; s < 2; ++s) m->mvPrograms[s][r] = (iGpuProgram*)(uintptr_t)(0x3000 + 16 * Rnd(4));
		}
		mats.push_back(m);
	}

	cFrustum frustum;
	cMatrixf view = RndMatrix();
	cVector3f origin(RndF(), RndF(), RndF());
	frustum.SetupPerspectiveProj(cMatrixf::Identity, view, 100.0f, 0.1f, 1.0f, 1.0f, origin);

	const int n = 20 + Rnd(40);
	std::vector<Stub*> objs;
	static const eRenderableType kTypes[] = { eRenderableType_ParticleEmitter, eRenderableType_Light, eRenderableType_Billboard,
											 eRenderableType_Beam, eRenderableType_GuiSet, eRenderableType_Rope, eRenderableType_FogArea,
											 eRenderableType_Dummy };
	for(int i = 0; i < n; ++i)
	{
		Stub *o = new Stub("o");
		o->mpMat = Rnd(8) == 0 ? NULL : mats[Rnd((unsigned)mats.size())];
		o->mType = kTypes[Rnd(8)];
		o->mbOq = Rnd(4) == 0;
		o->mbVpOk = Rnd(8) != 0;
		o->mlMatrixCount = i;
		o->mlFrameUpd = o->mlVpUpd = 0;
		o->mpVtx = (void*)(uintptr_t)(0x4000 + 16 * Rnd(5));
		o->mMtx = RndMatrix();
		o->mMtxVp = RndMatrix();
		o->mfIlluminationAmount = Rnd(3) == 0 ? 0.0f : RndF() / 16.0f;
		cVector3f vMin(RndF(), RndF(), RndF());
		cVector3f vSize(Rnd(10) + 0.5f, Rnd(10) + 0.5f, Rnd(10) + 0.5f);
		o->mBV.SetLocalMinMax(vMin, vMin + vSize);
		o->mBV.SetPosition(cVector3f(RndF(), RndF(), RndF()));
		o->mlRenderFrameCount = Rnd(2) ? -1 : 1;
		objs.push_back(o);
	}
	for(size_t i = 0; i < mats.size(); ++i) mats[i]->mlRenderFrameCount = Rnd(2) ? -1 : 1;

	cRenderList list;
	for(int frame = 0; frame < 3; ++frame)
	{
		iRenderer::mlRenderFrameCount = 1 + frame;
		list.Clear();
		list.Setup(0.016f * (frame + 1), &frustum);
		for(int pass = 0; pass < 2; ++pass)
			for(size_t i = 0; i < objs.size(); ++i) if(pass == 0 || Rnd(3) == 0) AddRec(list, objs[i], 1 + frame, frustum);
		list.Compile(eRenderListCompileFlag_Z | eRenderListCompileFlag_Diffuse | eRenderListCompileFlag_Decal | eRenderListCompileFlag_Illumination | eRenderListCompileFlag_Translucent);

		printf("frame %d lights %d fogs %d\n", frame, list.GetLightNum(), list.GetFogAreaNum());
		auto idOf = [&](iRenderable *p) { for(size_t i = 0; i < objs.size(); ++i) if(objs[i] == p) return (int)i; return -1; };
		auto dump = [&](const char *name, const tRenderableVec &v) {
			printf("%s", name);
			for(size_t i = 0; i < v.size(); ++i) printf(" %d", idOf(v[i]));
			printf("\n");
		};
		dump("oq", list.mvOcclusionQueryObjects);
		dump("solid", list.mvSolidObjects);
		dump("trans", list.mvTransObjects);
		dump("decal", list.mvDecalObjects);
		dump("illum", list.mvIllumObjects);
		for(int t = 0; t < eRenderListType_LastEnum; ++t) { char nm[16]; snprintf(nm, sizeof(nm), "sorted%d", t); dump(nm, list.mvSortedArrays[t]); }
		for(size_t i = 0; i < objs.size(); ++i)
			printf("o%d z %08x fc %d upd %d %d mtx %d place %d\n", (int)i, Bits(objs[i]->mfViewSpaceZ), objs[i]->mlRenderFrameCount,
				   objs[i]->mlFrameUpd, objs[i]->mlVpUpd, objs[i]->mpModelMatrix == &objs[i]->mMtx ? 0 : objs[i]->mpModelMatrix == &objs[i]->mMtxVp ? 1 : 2,
				   objs[i]->mlLargePlaneSurfacePlacement);
		for(size_t i = 0; i < mats.size(); ++i) printf("m%d fc %d\n", (int)i, mats[i]->mlRenderFrameCount);
	}
	fflush(stdout);
	_exit(0);
}

static int ClipScenario(int seed)
{
	gRng = (unsigned long long)seed * 7919 + 17;
	for(int i = 0; i < 2000; ++i)
	{
		cRect2l clipRect((int)Rnd(1500) - 200, (int)Rnd(900) - 100, (int)Rnd(1700), (int)Rnd(1000));
		cVector2l screen((int)Rnd(1921), (int)Rnd(1081));
		float tanHalf = (float)Rnd(4000) / 2000.0f;
		cRect2l c = clipRect;
		int lXInc = (int)((float)c.w * tanHalf);
		int lYInc = (int)((float)c.h * tanHalf);
		c.x = cMath::Max(c.x - lXInc, 0);
		c.y = cMath::Max(c.y - lYInc, 0);
		c.w = cMath::Min(c.w + lXInc * 2, screen.x - c.x);
		c.h = cMath::Min(c.h + lYInc * 2, screen.y - c.y);
		OhplH8Rect r = { clipRect.x, clipRect.y, clipRect.w, clipRect.h };
		ohpl_h8_clip_pad(&r, screen.x, screen.y, tanHalf);
		printf("%d %d %d %d %d %d %08x | %d %d %d %d\n", clipRect.x, clipRect.y, clipRect.w, clipRect.h, screen.x, screen.y, Bits(tanHalf), c.x, c.y, c.w, c.h);
		if(r.x != c.x || r.y != c.y || r.w != c.w || r.h != c.h) { printf("MISMATCH\n"); return 1; }
	}
	return 0;
}

int main()
{
	fprintf(stderr, "rs %d\n", (int)OhplBackendRustRequested());
	const char *sc = getenv("H8_SCENARIO");
	int seed = getenv("H8_SEED") ? atoi(getenv("H8_SEED")) : 0;
	if(sc && !strcmp(sc, "clip")) return ClipScenario(seed);
	return ListScenario(seed);
}
