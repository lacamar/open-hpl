// H11 differential harness: cMeshEntity/cSubMeshEntity/cAnimationState driven over real game meshes and animations,
// iLight (point/spot/box) and cFogArea driven by random op scripts. OHPL_IMPL=cpp and =rust must print the same text.
//   h11_script mesh <seed> <steps> <file.msh> [extra.anm ...]
//   h11_script light <seed> <steps>
//   h11_script fog <seed> <steps>
// Lines: "o <op> args" op, "= <v>" result bits, "e <ev>" callback event, "h <hash>" state hash (replayed by cargo test),
//        "~ ..." output only the C++ host can produce (bone matrices, skinned vertices); compared live, not replayed.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#define private public
#define protected public

#include "resources/ResourceManager.h"
#include "graphics/Animation.h"
#include "graphics/AnimationTrack.h"
#include "graphics/Bone.h"
#include "graphics/BoneState.h"
#include "graphics/Mesh.h"
#include "graphics/Skeleton.h"
#include "graphics/SubMesh.h"
#include "graphics/VertexBuffer.h"
#include "impl/VertexBufferOGL_VBO.h"
#include "impl/MeshLoaderMSH.h"
#include "impl/MeshLoaderAssimp.h"
#include "impl/MeshLoaderCollada.h"
#include "impl/LowLevelGraphicsSDL.h"
#include "math/BoundingVolume.h"
#include "math/Frustum.h"
#include "math/Math.h"
#include "resources/AnimationManager.h"
#include "resources/MaterialManager.h"
#include "resources/MeshManager.h"
#include "resources/Resources.h"
#include "scene/AnimationState.h"
#include "scene/DummyRenderable.h"
#include "scene/FogArea.h"
#include "scene/LightBox.h"
#include "scene/LightPoint.h"
#include "scene/LightSpot.h"
#include "scene/MeshEntity.h"
#include "scene/Node3D.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"

using namespace hpl;

namespace hpl {
	cMaterial* cMaterialManager::CreateMaterial(const tString&) { return NULL; }
}

class HVb : public cVertexBufferOGL_VBO
{
public:
	HVb(iLowLevelGraphics* l, eVertexBufferDrawType d, eVertexBufferUsageType u, int v, int i) : cVertexBufferOGL_VBO(l, d, u, v, i) {}
	void UpdateData(tVertexElementFlag, bool) {}
	void Bind() {}
	void UnBind() {}
	void Draw(eVertexBufferDrawType) {}
	void DrawIndices(unsigned int*, int, eVertexBufferDrawType) {}
private:
	void CompileSpecific() {}
};

class HLow : public cLowLevelGraphicsSDL
{
public:
	iVertexBuffer* CreateVertexBuffer(eVertexBufferType, eVertexBufferDrawType d, eVertexBufferUsageType u, int v, int i)
	{
		return new HVb(this, d, u, v, i);
	}
};

struct HMsh : cMeshLoaderMSH
{
	HMsh(iLowLevelGraphics* l) : cMeshLoaderMSH(l)
	{
		mpMaterialManager = (cMaterialManager*)calloc(1, 65536);
		mpMeshManager = (cMeshManager*)calloc(1, 65536);
		mpAnimationManager = (cAnimationManager*)calloc(1, 65536);
	}
};

struct HAss : cMeshLoaderAssimp
{
	HAss(iLowLevelGraphics* l) : cMeshLoaderAssimp(l)
	{
		mpMaterialManager = (cMaterialManager*)calloc(1, 65536);
		mpMeshManager = (cMeshManager*)calloc(1, 65536);
		mpAnimationManager = (cAnimationManager*)calloc(1, 65536);
	}
};

struct HCol : cMeshLoaderCollada
{
	HCol(iLowLevelGraphics* l, cMeshLoaderMSH* m) : cMeshLoaderCollada(l, m, false)
	{
		mpMaterialManager = (cMaterialManager*)calloc(1, 65536);
		mpMeshManager = (cMeshManager*)calloc(1, 65536);
		mpAnimationManager = (cAnimationManager*)calloc(1, 65536);
	}
};

int hplMain(const std::string&) { return 0; }

static unsigned long long gRng;
static unsigned Rnd(unsigned n)
{
	gRng = gRng * 6364136223846793005ull + 1442695040888963407ull;
	return (unsigned)((gRng >> 33) % n);
}
static float RndF(float lo, float hi) { return lo + (hi - lo) * (float)Rnd(65536) / 65535.0f; }
static unsigned Bits(float f) { unsigned u; memcpy(&u, &f, 4); return u; }

struct Hash
{
	unsigned long long h = 0xcbf29ce484222325ULL;
	void U(unsigned v) { for(int i = 0; i < 4; ++i) h = (h ^ ((v >> (8 * i)) & 255)) * 0x100000001b3ULL; }
	void F(float v) { U(Bits(v)); }
	void M(const cMatrixf& m) { for(int i = 0; i < 16; ++i) F(m.v[i]); }
	void V(const cVector3f& v) { F(v.x); F(v.y); F(v.z); }
	void C(const cColor& c) { F(c.r); F(c.g); F(c.b); F(c.a); }
};

static void Out(const char* tag, unsigned long long h) { printf("%s %016llx\n", tag, h); }

struct Callback : cMeshEntityCallback
{
	int mlAfter = 0;
	void AfterAnimationUpdate(cMeshEntity*, float dt) { printf("e after %08x\n", Bits(dt)); ++mlAfter; }
	bool OnAnimationEvent(cMeshEntity*, cAnimationEvent* e)
	{
		printf("e event %08x %d %s\n", Bits(e->mfTime), (int)e->mType, e->msValue.empty() ? "." : e->msValue.c_str());
		return e->msValue.empty() ? e->msName != "pass" : true;
	}
};

static void HashAnimStates(Hash& h, cMeshEntity* e)
{
	h.U(e->GetAnimationStateNum());
	for(int i = 0; i < e->GetAnimationStateNum(); ++i)
	{
		cAnimationState* s = e->GetAnimationState(i);
		h.F(s->GetLength()); h.F(s->GetWeight()); h.F(s->GetSpeed()); h.F(s->GetBaseSpeed()); h.F(s->GetTimePosition());
		h.F(s->GetPreviousTimePosition()); h.F(s->GetSpecialEventTime()); h.F(s->GetFadeStep());
		h.U(s->IsActive() | s->IsLooping() << 1 | s->IsPaused() << 2 | s->IsLayer() << 3 | s->IsOver() << 4);
	}
	h.U(e->GetNormalizeAnimationWeights());
	h.U(e->GetSkeletonPhysicsActive()); h.F(e->GetSkeletonPhysicsWeight());
}

static void HashNodes(Hash& h, cMeshEntity* e)
{
	for(int i = 0; i < e->GetBoneStateNum(); ++i) { h.M(e->GetBoneState(i)->GetLocalMatrix()); h.M(e->GetBoneState(i)->GetWorldMatrix()); }
	for(int i = 0; i < e->GetNodeStateNum(); ++i) { h.M(e->GetNodeState(i)->GetLocalMatrix()); h.M(e->GetNodeState(i)->GetWorldMatrix()); }
	for(int i = 0; i < e->GetSubMeshEntityNum(); ++i)
	{
		cSubMeshEntity* s = e->GetSubMeshEntity(i);
		h.M(s->GetWorldMatrix());
		cMatrixf* mm = s->GetModelMatrix(NULL);
		h.U(mm != NULL);
		cBoundingVolume* bv = s->GetBoundingVolume();
		h.V(bv->GetMin()); h.V(bv->GetMax());
	}
}

static void HashGraphics(Hash& bones, Hash& skin, cMeshEntity* e)
{
	for(size_t i = 0; i < e->mvBoneMatrices.size(); ++i) bones.M(e->mvBoneMatrices[i]);
#if __has_include("scene/RustSceneH11.h")
	bones.U(e->mCtl.bone_matrices_version);
#else
	bones.U(e->mlBoneMatricesVersion);
#endif
	for(int i = 0; i < e->GetSubMeshEntityNum(); ++i)
	{
		cSubMeshEntity* s = e->GetSubMeshEntity(i);
		iVertexBuffer* vb = s->GetVertexBuffer();
		const float* p = vb->GetFloatArray(eVertexBufferElement_Position);
		const float* n = vb->GetFloatArray(eVertexBufferElement_Normal);
		int pn = vb->GetElementNum(eVertexBufferElement_Position);
		int nn = vb->GetElementNum(eVertexBufferElement_Normal);
		for(int v = 0; v < vb->GetVertexNum(); ++v)
		{
			for(int k = 0; k < 3; ++k) { if(p) skin.F(p[v * pn + k]); if(n) skin.F(n[v * nn + k]); }
		}
	}
}

// Name lookups, sub-mesh fan-out setters and skeleton-physics flag paths; "~" lines, compared live (cpp vs rust).
static void Extras(cMeshEntity* e, int nAnim)
{
	Hash h;
	std::vector<std::string> names;
	for(int i = 0; i < nAnim; ++i) names.push_back(e->GetAnimationState(i)->GetName());
	names.push_back("no-such-anim");
	names.push_back("");
	for(size_t i = 0; i < names.size(); ++i)
	{
		const std::string& n = names[i];
		h.U(e->GetAnimationStateIndex(n)); h.U(e->GetAnimationStateFromName(n) != NULL);
		h.U(e->GetBoneStateIndex(n)); h.U(e->GetBoneStateFromName(n) != NULL);
		h.U(e->GetNodeStateIndex(n)); h.U(e->GetNodeStateFromName(n) != NULL);
		h.U(e->GetSubMeshEntityName(n) != NULL);
		h.U(e->AnimationIsOver(n));
		e->PlayName(n, i & 1, i & 2);
		HashAnimStates(h, e);
		e->PlayFadeToName(n, i & 1, 0.25f * (float)i);
		HashAnimStates(h, e);
	}
	for(int i = 0; i < e->GetBoneStateNum(); ++i)
	{
		const std::string n = e->GetBoneState(i)->GetName();
		h.U(e->GetBoneStateIndex(n));
		h.U(e->GetBoneStateFromName(n) == e->GetBoneState(i));
	}
	for(int i = 0; i < e->GetNodeStateNum(); ++i)
	{
		const std::string n = e->GetNodeState(i)->GetName();
		h.U(e->GetNodeStateIndex(n));
		h.U(e->GetNodeStateFromName(n) == e->GetNodeState(i));
	}
	for(int i = 0; i < e->GetSubMeshEntityNum(); ++i)
		h.U(e->GetSubMeshEntityName(e->GetMesh()->GetSubMesh(i)->GetName()) != NULL);
	Out("~ names", h.h);

	Hash g;
	for(int round = 0; round < 3; ++round)
	{
		e->SetVisible(round != 1); e->SetVisible(round != 1);
		e->SetStatic(round == 2);
		e->SetIlluminationAmount(0.25f * (float)round); e->SetCoverageAmount(round == 1 ? 0.0f : 0.5f);
		e->SetLiquidAmount(round - 1.0f);
		if(round == 2) e->UseAutomaticLiquidAmount();
		e->SetRenderFlagBit(1 << round, round != 1);
		e->SetColorMul(cColor(0.1f * (float)round, 0.5f, 0.75f, 1));
		e->SetIlluminationColor(cColor(0.9f, 0.1f * (float)round, 0.3f, 1));
		g.U(e->IsVisible()); g.U(e->IsStatic()); g.F(e->GetIlluminationAmount()); g.F(e->GetCoverageAmount()); g.F(e->GetLiquidAmount());
		for(int i = 0; i < e->GetSubMeshEntityNum(); ++i)
		{
			cSubMeshEntity* x = e->GetSubMeshEntity(i);
			g.U(x->GetVisibleVar()); g.U(x->IsStatic()); g.F(x->GetIlluminationAmount()); g.F(x->GetCoverageAmount()); g.F(x->GetLiquidAmount());
			g.U(x->GetRenderFlags()); g.C(x->GetColorMul()); g.C(x->GetIlluminationColor());
		}
	}
	Out("~ setters", g.h);

	Hash k;
	e->SetSkeletonCollidersActive(true); k.U(e->GetSkeletonCollidersActive());
	e->SetSkeletonPhysicsActive(true); k.U(e->GetSkeletonPhysicsActive());
	e->FadeSkeletonPhysicsWeight(0.5f); k.F(e->GetSkeletonPhysicsWeight());
	e->UpdateLogic(0.2f);
	e->UpdateLogic(0.4f);
	e->AlignBodiesToSkeleton(false);
	k.U(e->CheckColliderShapeCollision(NULL, NULL, cMatrixf::Identity, NULL, NULL));
	e->SetSkeletonCollidersActive(false); k.U(e->GetSkeletonCollidersActive());
	e->SetSkeletonPhysicsActive(false); k.U(e->GetSkeletonPhysicsActive());
	k.F(e->GetSkeletonPhysicsWeight());
	e->FadeSkeletonPhysicsWeight(0.5f);
	HashAnimStates(k, e);
	Out("~ physflags", k.h);
}

static int Mesh(int seed, int steps, const char* file, int argc, char** argv)
{
	SetLogMessageCallback([](eLogOutputType, const char* m) { if(getenv("H11_LOG")) fprintf(stderr, "log: %s", m); });
	HLow low;
	HMsh msh(&low);
	HAss ass(&low);
	HCol col(&low, &msh);
	cMesh* mesh = msh.LoadMesh(cString::To16Char(file), 0);
	if(!mesh) { printf("FAIL load\n"); return 1; }
	cMeshEntity* e = new cMeshEntity("ent", mesh, (cMaterialManager*)calloc(1, 65536), (cMeshManager*)calloc(1, 65536), (cAnimationManager*)calloc(1, 65536));
	Callback cb;
	e->SetCallback(&cb);
	for(int a = 0; a < argc; ++a)
	{
		std::string path = argv[a];
		bool bFbx = path.size() > 4 && path.compare(path.size() - 4, 4, ".fbx") == 0;
		bool bDae = path.size() > 9 && path.compare(path.size() - 9, 9, ".dae_anim") == 0;
		cAnimation* an = bFbx ? ass.LoadAnimation(cString::To16Char(path)) : bDae ? col.LoadAnimation(cString::To16Char(path)) : msh.LoadAnimation(cString::To16Char(path));
		if(an) e->AddAnimation(an, an->GetName(), 1.0f);
		else fprintf(stderr, "anm load failed: %s\n", argv[a]);
	}
	gRng = (unsigned long long)seed * 7919 + 17;

	printf("m skel=%d bones=%d nodes=%d subs=%d anims=%d meshnodes=%d\n", e->GetMesh()->GetSkeleton() != NULL, e->GetBoneStateNum(), e->GetNodeStateNum(), e->GetSubMeshEntityNum(), e->GetAnimationStateNum(), e->GetMesh()->GetNodeNum());
	int nAnim = e->GetAnimationStateNum();
	for(int i = 0; i < nAnim; ++i)
	{
		int ev = 1 + Rnd(4);
		for(int k = 0; k < ev; ++k)
		{
			cAnimationEvent* x = e->GetAnimationState(i)->CreateEvent();
			x->mfTime = RndF(0, e->GetAnimationState(i)->GetLength());
			x->mType = Rnd(3) ? eAnimationEventType_PlaySound : eAnimationEventType_LastEnum;
			x->msValue = Rnd(4) ? "snd" : "";
			x->msName = Rnd(2) ? "pass" : "";
		}
		cAnimationState* sx = e->GetAnimationState(i);
		printf("s %d %08x %08x\n", i, Bits(sx->GetLength()), Bits(sx->GetBaseSpeed()));
		for(int k = 0; k < sx->GetEventNum(); ++k)
		{
			cAnimationEvent* x = sx->GetEvent(k);
			printf("x %d %08x %d %s\n", i, Bits(x->mfTime), (int)x->mType, x->msValue.empty() ? "." : x->msValue.c_str());
		}
	}

	for(int step = 0; step < steps; ++step)
	{
		int op = Rnd(10) < 3 ? 0 : (int)Rnd(30);
		int ai = nAnim ? (int)Rnd(nAnim) : 0;
		cAnimationState* st = nAnim ? e->GetAnimationState(ai) : NULL;
		float f = RndF(-1, 3);
		bool b = Rnd(2);
		char buf[160];
		bool upd = false;
		if(op < 8)
		{
			static const float dts[] = { 0.0f, 1.0f / 60, 1.0f / 30, 0.05f, 0.2f, 0.5f };
			float dt = dts[Rnd(6)];
			snprintf(buf, sizeof buf, "update %08x", Bits(dt));
			printf("o %s\n", buf);
			e->UpdateLogic(dt);
			upd = true;
		}
		else if(op == 8 && nAnim) { bool sp = Rnd(2); printf("o play %d %d %d\n", ai, b, (int)sp); e->Play(ai, b, sp); }
		else if(op == 9 && nAnim) { printf("o playfade %d %d %08x\n", ai, b, Bits(f)); e->PlayFadeTo(ai, b, f); }
		else if(op == 10) { printf("o stop\n"); e->Stop(); }
		else if(op == 11 && nAnim) { printf("o active %d %d\n", ai, b); st->SetActive(b); }
		else if(op == 12 && nAnim) { printf("o weight %d %08x\n", ai, Bits(f)); st->SetWeight(f); }
		else if(op == 13 && nAnim) { printf("o time %d %08x\n", ai, Bits(f * st->GetLength())); st->SetTimePosition(f * st->GetLength()); }
		else if(op == 14 && nAnim) { printf("o loop %d %d\n", ai, b); st->SetLoop(b); }
		else if(op == 15 && nAnim) { printf("o pause %d %d\n", ai, b); st->SetPaused(b); }
		else if(op == 16 && nAnim) { printf("o fadespeed %d %08x %08x\n", ai, Bits(f), Bits(f + 1)); st->FadeSpeed(f, f + 1); }
		else if(op == 17 && nAnim) { printf("o layer %d %d\n", ai, b); st->SetLayer(b); }
		else if(op == 18 && nAnim) { bool in = Rnd(2); printf("o fade%s %d %08x\n", in ? "in" : "out", ai, Bits(f)); if(in) st->FadeIn(f); else st->FadeOut(f); }
		else if(op == 19) { printf("o normalize %d\n", b); e->SetNormalizeAnimationWeights(b); }
		else if(op == 20)
		{
			printf("o skelphys %d\n", b);
			if(e->GetMesh()->GetSkeleton()) e->SetSkeletonPhysicsActive(b);
		}
		else if(op == 21)
		{
			printf("o physfade %08x\n", Bits(f));
			if(e->GetMesh()->GetSkeleton()) e->FadeSkeletonPhysicsWeight(f);
		}
		else if(op == 22) { printf("o physweight %08x\n", Bits(f)); e->SetSkeletonPhysicsWeight(f); }
		else if(op == 23)
		{
			int n = e->GetMesh()->GetSkeleton() ? e->GetMesh()->GetSkeleton()->GetBoneNum() : 0;
			std::vector<float> v;
			for(int i = 0; i < n * 7; ++i) v.push_back(RndF(-1, 1));
			printf("o restpose %d\n", n);
			bool ok = e->SetBoneRestPoseFromMapValues(v);
			printf("= %d\n", ok);
			for(int k = 0; ok && k < 4 && n > 0; ++k)
			{
				int bi = Rnd(n);
				cBone* bone = e->GetMesh()->GetSkeleton()->GetBoneByIndex(bi);
				printf("r");
				for(int q = 0; q < 7; ++q) printf(" %08x", Bits(v[bi * 7 + q]));
				for(int q = 0; q < 16; ++q) printf(" %08x", Bits(bone->GetLocalTransform().v[q]));
				printf(" %08x", Bits(bone->GetLocalUnitScale()));
				printf(" ->");
				for(int q = 0; q < 16; ++q) printf(" %08x", Bits(e->mvBoneRestPose[bi].v[q]));
				printf("\n");
			}
		}
		else if(op == 24)
		{
			cMatrixf m = cMath::MatrixRotate(cVector3f(RndF(-3, 3), RndF(-3, 3), RndF(-3, 3)), eEulerRotationOrder_XYZ);
			m.SetTranslation(cVector3f(RndF(-10, 10), RndF(-10, 10), RndF(-10, 10)));
			printf("o setmatrix\n");
			e->SetMatrix(m);
		}
		else if(op == 25) { printf("o static %d\n", b); e->SetStatic(b); }
		else if(op == 26 || op == 27)
		{
			printf("o graphics\n");
			for(int i = 0; i < e->GetSubMeshEntityNum(); ++i) e->GetSubMeshEntity(i)->UpdateGraphicsForFrame(0.016f);
			Hash bones, skin;
			HashGraphics(bones, skin, e);
			Out("~ bones", bones.h);
			Out("~ skin", skin.h);
		}
		else if(op == 28 && e->GetSubMeshEntityNum() > 0)
		{
			printf("o bounds\n");
			cBoundingVolume* bv = e->GetBoundingVolume();
			Hash h; h.V(bv->GetMin()); h.V(bv->GetMax());
			Out("~ bv", h.h);
		}
		else if(op == 29)
		{
			printf("o calcxf\n");
			if(e->GetMesh()->GetSkeleton() && e->GetBoneStateNum() > 0 && e->GetBoneStateRoot()->GetChildIterator().HasNext())
			{
				cVector3f p, a;
				cMatrixf sw = static_cast<cBoneState*>(e->GetBoneStateRoot()->GetChildIterator().Next())->GetWorldMatrix();
				cMatrixf iw = e->GetMesh()->GetSkeleton()->GetRootBone()->GetChildIterator().Next()->GetInvWorldTransform();
				cMatrixf m = e->CalculateTransformFromSkeleton(&p, &a);
				printf("c");
				for(int q = 0; q < 16; ++q) printf(" %08x", Bits(sw.v[q]));
				for(int q = 0; q < 16; ++q) printf(" %08x", Bits(iw.v[q]));
				printf(" ->");
				for(int q = 0; q < 16; ++q) printf(" %08x", Bits(m.v[q]));
				printf(" %08x %08x %08x %08x\n", Bits(p.x), Bits(p.y), Bits(p.z), Bits(a.y));
			}
		}
		else { printf("o nop\n"); }
		(void)upd;
		Hash h;
		HashAnimStates(h, e);
		Hash nodes;
		HashNodes(nodes, e);
		Out("h", h.h);
		Out("~ n", nodes.h);
	}
	Extras(e, nAnim);
	return 0;
}

struct HRes { char pad[1]; };

static iLight* MkLight(int kind, cResources* r)
{
	if(kind == 0) return new cLightPoint("pl", r);
	if(kind == 1) return new cLightSpot("sl", r);
	return new cLightBox("bl", r);
}

static void HashLight(Hash& h, iLight* l, int kind)
{
	h.C(l->GetDiffuseColor()); h.F(l->GetRadius()); h.F(l->GetBrightness());
	h.U(l->IsLit() | l->IsVisible() << 1 | l->IsFading() << 2 | l->GetFlickerActive() << 3);
	h.C(l->GetDestColor()); h.F(l->GetDestRadius());
	cBoundingVolume* bv = l->GetBoundingVolume();
	h.V(bv->GetMin()); h.V(bv->GetMax());
	h.M(*l->GetModelMatrix(NULL));
	if(kind == 1)
	{
		cLightSpot* s = static_cast<cLightSpot*>(l);
		h.M(s->GetViewMatrix()); h.M(s->GetProjectionMatrix()); h.M(s->GetViewProjMatrix());
		h.F(s->GetTanHalfFOV()); h.F(s->GetCosHalfFOV());
		cFrustum* fr = s->GetFrustum();
		h.V(fr->GetOrigin());
		for(int i = 0; i < 6; ++i) { cPlanef p = fr->GetPlane((eFrustumPlane)i); h.F(p.a); h.F(p.b); h.F(p.c); h.F(p.d); }
	}
	if(kind == 2)
	{
		cLightBox* b = static_cast<cLightBox*>(l);
		for(int i = 0; i < 9; ++i) h.V(b->GetIrradianceBands()[i]);
	}
}

static int Light(int seed, int steps)
{
	gRng = (unsigned long long)seed * 104729 + 5;
	srand(seed);
	iLight::SetHpl3Visibility(true);
	bool gHpl3 = true;
	cResources* res = (cResources*)calloc(1, 1 << 20);
	int kind = seed % 3;
	iLight* l = MkLight(kind, res);
	std::vector<cDummyRenderable*> objs;
	for(int i = 0; i < 6; ++i)
	{
		objs.push_back(new cDummyRenderable("d"));
		objs.back()->SetStatic(i % 2);
	}
	printf("light kind=%d\n", kind);
	for(int step = 0; step < steps; ++step)
	{
		int op = Rnd(25);
		float f = RndF(0, 5);
		bool b = Rnd(2);
		if(op < 5)
		{
			static const float dts[] = { 0.0f, 1.0f / 60, 0.05f, 0.3f };
			float dt = dts[Rnd(4)];
			printf("o update %08x\n", Bits(dt));
			l->UpdateLogic(dt);
		}
		else if(op == 5) { cColor c(RndF(0, 1), RndF(0, 1), RndF(0, 1), RndF(0, 1)); if(!Rnd(4)) c = cColor(0, 0); printf("o diffuse\n"); l->SetDiffuseColor(c); }
		else if(op == 6) { printf("o brightness %08x\n", Bits(f)); l->SetBrightness(Rnd(5) ? f : 0); }
		else if(op == 7) { printf("o radius %08x\n", Bits(f)); l->SetRadius(f * 3); }
		else if(op == 8) { printf("o fadeto %08x\n", Bits(f)); l->FadeTo(cColor(RndF(0, 1), RndF(0, 1), RndF(0, 1), 1), f * 2, RndF(0, 2)); }
		else if(op == 9) { printf("o fadebright %08x\n", Bits(f)); l->FadeBrightnessTo(f, RndF(0, 2), Rnd(2) ? std::function<float(float)>() : std::function<float(float)>([](float t) { return t * t; })); }
		else if(op == 10) { printf("o stopfade\n"); l->StopFading(); }
		else if(op == 11)
		{
			printf("o flicker\n");
			l->SetFlicker(cColor(RndF(0, 1), RndF(0, 1), RndF(0, 1), 1), RndF(0, 3), RndF(0.1f, 1), RndF(1, 2), "", "", RndF(0.1f, 1), RndF(1, 2), "", "",
						  b, RndF(0.05f, 0.5f), RndF(0.5f, 1), RndF(0.05f, 0.5f), RndF(0.5f, 1));
		}
		else if(op == 12) { printf("o flickeractive %d\n", b); l->SetFlickerActive(b); }
		else if(op == 13)
		{
			cMatrixf m = cMath::MatrixRotate(cVector3f(RndF(-3, 3), RndF(-3, 3), RndF(-3, 3)), eEulerRotationOrder_XYZ);
			m.SetTranslation(cVector3f(RndF(-10, 10), RndF(-10, 10), RndF(-10, 10)));
			printf("o setmatrix\n");
			l->SetMatrix(m);
		}
		else if(op == 14) { printf("o hpl3 %d\n", b); iLight::SetHpl3Visibility(b); gHpl3 = b; }
		else if(op == 15) { printf("o visible %d\n", b); l->SetVisible(b); }
		else if(op == 16)
		{
			printf("o mask %d\n", b);
			l->SetMaskBox(b, cVector3f(RndF(-2, 2), RndF(-2, 2), RndF(-2, 2)), cVector3f(RndF(0.5f, 6), RndF(0.5f, 6), RndF(0.5f, 6)));
		}
		else if(op == 17 && kind == 1)
		{
			cLightSpot* s = static_cast<cLightSpot*>(l);
			int w = Rnd(4);
			printf("o spot %d %08x\n", w, Bits(f));
			if(w == 0) s->SetFOV(RndF(0.2f, 2.5f)); else if(w == 1) s->SetAspect(RndF(0.5f, 2)); else if(w == 2) s->SetNearClipPlane(RndF(0.01f, 1)); else cLightSpot::SetShadowNearClip(Rnd(2) ? 0.0f : f * 0.1f);
		}
		else if(op == 18 && kind == 2)
		{
			cLightBox* x = static_cast<cLightBox*>(l);
			printf("o box %08x\n", Bits(f));
			if(Rnd(2)) x->SetSize(cVector3f(f, f + 1, f + 2));
			else
			{
				std::vector<cVector3f> bands;
				for(int i = 0; i < 9; ++i) bands.push_back(cVector3f(RndF(0, 1), RndF(0, 1), RndF(0, 1)));
				char nm[8]; snprintf(nm, sizeof nm, "S%d", (int)Rnd(3));
				if(Rnd(2)) x->AddIrradianceSet(nm, bands); else x->FadeIrradianceSet(nm, RndF(0, 2));
			}
		}
		else if(op == 19)
		{
			cBoundingVolume bv;
			bv.SetLocalMinMax(cVector3f(RndF(-8, 0), RndF(-8, 0), RndF(-8, 0)), cVector3f(RndF(0, 8), RndF(0, 8), RndF(0, 8)));
			printf("o collide\n");
			bool r = l->CollidesWithBV(&bv);
			Hash h; h.U(r);
			Out("~ collide", h.h);
		}
		else if(op == 20)
		{
			int k = Rnd(6);
			printf("o shadowadd %d %d\n", k, objs[k]->GetTransformUpdateCount());
			l->AddShadowCaster(objs[k]);
		}
		else if(op == 21)
		{
			int j = Rnd(6);
			objs[j]->SetMatrix(cMatrixf::Identity);
			printf("o objmove %d\n", j);
		}
		else if(op == 23)
		{
			unsigned mask = Rnd(64);
			int what = Rnd(5);
			printf("o shadowcheck %d %u", what, mask);
			for(size_t i = 0; i < objs.size(); ++i) printf(" %d %d", objs[i]->GetTransformUpdateCount(), objs[i]->IsStatic());
			printf("\n");
			tRenderableVec v;
			for(size_t i = 0; i < objs.size(); ++i) if(mask >> i & 1) v.push_back(objs[i]);
			if(what == 0) { printf("="); for(size_t i = 0; i < objs.size(); ++i) printf(" %d", l->ShadowCasterIsValid(objs[i])); printf("\n"); }
			else if(what == 1 || what == 2) printf("= %d\n", l->ShadowCastersAreUnchanged(v));
			else if(what == 3) l->SetShadowCasterCacheFromVec(v);
			else l->ClearShadowCasterCache();
		}
		else if(op == 22) { printf("o intersect\n"); Hash h; h.U(l->CheckObjectIntersection(objs[Rnd(6)])); Out("~ isect", h.h); }
		else { printf("o nop\n"); }
		{
			const cColor& d = l->GetDiffuseColor();
			printf("= lit %d %08x %08x %08x %08x %08x %08x %d %d -> %d %d\n", kind, Bits(d.r), Bits(d.g), Bits(d.b), Bits(d.a), Bits(l->GetBrightness()), Bits(l->GetRadius()),
				   gHpl3, (int)l->GetVisibleVar(), l->IsLit(), l->IsVisible());
		}
		Hash h;
		HashLight(h, l, kind);
		Out("~ l", h.h);
	}
	return 0;
}

static int Fog(int seed, int steps)
{
	gRng = (unsigned long long)seed * 15485863 + 3;
	cResources* res = (cResources*)calloc(1, 1 << 20);
	cFogArea* fa = new cFogArea("fog", res);
	for(int step = 0; step < steps; ++step)
	{
		int op = Rnd(6);
		if(op == 0) { cVector3f s(RndF(0.1f, 20), RndF(0.1f, 20), RndF(0.1f, 20)); printf("o size\n"); fa->SetSize(s); }
		else if(op == 1)
		{
			cMatrixf m = cMath::MatrixRotate(cVector3f(RndF(-3, 3), RndF(-3, 3), RndF(-3, 3)), eEulerRotationOrder_XYZ);
			m.SetTranslation(cVector3f(RndF(-10, 10), RndF(-10, 10), RndF(-10, 10)));
			printf("o setmatrix\n");
			fa->SetMatrix(m);
		}
		else if(op == 2) { printf("o props\n"); fa->SetStart(RndF(0, 5)); fa->SetEnd(RndF(5, 50)); fa->SetFalloffExp(RndF(0.5f, 3)); fa->SetColor(cColor(RndF(0, 1), RndF(0, 1), RndF(0, 1), 1)); }
		else printf("o nop\n");
		printf("= fog %08x %08x %08x -> %08x %08x %08x %08x %08x %08x\n", Bits(fa->GetSize().x), Bits(fa->GetSize().y), Bits(fa->GetSize().z),
			   Bits(fa->GetBoundingVolume()->GetLocalMax().x), Bits(fa->GetBoundingVolume()->GetLocalMax().y), Bits(fa->GetBoundingVolume()->GetLocalMax().z),
			   Bits(fa->GetBoundingVolume()->GetLocalMin().x), Bits(fa->GetBoundingVolume()->GetLocalMin().y), Bits(fa->GetBoundingVolume()->GetLocalMin().z));
		Hash h;
		h.V(fa->GetSize()); h.F(fa->GetStart()); h.F(fa->GetEnd()); h.F(fa->GetFalloffExp()); h.C(fa->GetColor());
		h.M(*fa->GetModelMatrix(NULL));
		cBoundingVolume* bv = fa->GetBoundingVolume();
		h.V(bv->GetMin()); h.V(bv->GetMax());
		Out("~ f", h.h);
	}
	return 0;
}

int main(int argc, char** argv)
{
	if(argc < 4) { fprintf(stderr, "usage: h11_script mesh|light|fog <seed> <steps> [file.msh [anm...]]\n"); return 2; }
	std::string m = argv[1];
	int seed = atoi(argv[2]), steps = atoi(argv[3]);
	if(m == "mesh" && argc >= 5) return Mesh(seed, steps, argv[4], argc - 5, argv + 5);
	if(m == "light") return Light(seed, steps);
	if(m == "fog") return Fog(seed, steps);
	return 2;
}
