// H12 pass 2 differential: particle system lifecycle, emitter shell, billboard, beam, lens flare
// driven through the real C++ classes over a RecordingLowLevelGraphics, plus a grid of decision probes.
// Run per OHPL_IMPL and compare the dumps; the "dec" lines (original C++ expressions) are recorded
// under rust/tests/golden/H12/dec.txt and replayed by scene::tests_h12b. scripts/h12b-diff.sh drives it.
// Env: OHPL_H12B_DIRS (':'-separated roots searched for *.ps), OHPL_H12B_DUMP (default stdout),
// OHPL_H12B_EVERY (every n-th .ps file, default 1), OHPL_H12B_FRAMES (default 40), OHPL_H12B_MODE (dec|scene|all).
// The probes also run the Rust function under test and exit 1 on a mismatch against the C++ expression.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <functional>
#include <deque>
#include <queue>
#include <stack>
#include <unordered_map>
#include <array>
#include <set>
#include <iomanip>
#include <mutex>
#include <atomic>
#include <thread>

// The scene shells keep their material and flag arrays private; the harness plays with them.
#define private public
#define protected public
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "impl/tinyXML/tinyxml.h"
#include "math/Frustum.h"
#include "math/Math.h"
#include "resources/MaterialManager.h"
#include "resources/Resources.h"
#include "resources/XmlDocument.h"
#include "scene/BillBoard.h"
#include "scene/Beam.h"
#include "scene/LensFlare.h"
#include "scene/ParticleEmitter.h"
#include "scene/ParticleEmitter_UserData.h"
#include "scene/ParticleSystem.h"
#include "scene/RustSceneH12.h"
#include "trace/RecordingLowLevelGraphics.h"
#undef private
#undef protected

using namespace hpl;

namespace {

FILE *gOut = NULL;
int gMismatch = 0;
uint32_t Bits(float v) { uint32_t u; memcpy(&u, &v, 4); return u; }

struct Line
{
	std::string s;
	void F(float v) { char b[16]; snprintf(b, sizeof b, " %08x", Bits(v)); s += b; }
	void I(long v) { s += " " + std::to_string(v); }
	void V3(const cVector3f &v) { F(v.x); F(v.y); F(v.z); }
	void C(const cColor &c) { F(c.r); F(c.g); F(c.b); F(c.a); }
	void S(const std::string &t) { s += " " + t; }
};

uint64_t Fnv(const std::string &t)
{
	uint64_t h = 0xcbf29ce484222325ULL;
	for(unsigned char c : t) h = (h ^ c) * 0x100000001b3ULL;
	return h;
}

// One probe: the C++ expression's result goes to the dump, the Rust function must agree.
void Dec(const std::string &asKey, const std::string &asOrig, const std::string &asRust)
{
	fprintf(gOut, "dec %s ->%s\n", asKey.c_str(), asOrig.c_str());
	if(asOrig != asRust)
	{
		fprintf(stderr, "MISMATCH dec %s: cpp%s rust%s\n", asKey.c_str(), asOrig.c_str(), asRust.c_str());
		++gMismatch;
	}
}

void Decisions()
{
	const int lSleeps[] = {300, 11, 10, 9, 1, 0};
	for(int lSleep : lSleeps) for(int act = 0; act < 2; ++act) for(int dy = 0; dy < 2; ++dy) for(int same = 0; same < 2; ++same)
	for(int ud = 0; ud < 2; ++ud) for(int dc : {-1, 7}) for(int mc : {7, 8})
	{
		Line k; k.I(lSleep); k.I(act); k.I(dy); k.I(same); k.I(ud); k.I(dc); k.I(mc);
		// iParticleEmitter::UpdateLogic
		OhplH12PeCtl o; o.sleep_count = lSleep; o.direction_update_count = dc; o.update_gfx = o.update_bv = false;
		uint32_t r = 0;
		if(act)
		{
			bool bRet = false;
			if(dy == 0 && !same)
			{
				if(o.sleep_count <= 0) bRet = true; else o.sleep_count--;
			}
			else if(o.sleep_count < 10) o.sleep_count = 10;
			if(!bRet)
			{
				o.update_gfx = true; o.update_bv = true; r = 1;
				if(ud && o.direction_update_count != mc) { o.direction_update_count = mc; r |= 2; }
			}
		}
		OhplH12PeCtl n; n.sleep_count = lSleep; n.direction_update_count = dc; n.update_gfx = n.update_bv = false;
		uint32_t rn = ohpl_h12b_pe_update_gate(&n, act, dy, same, ud, mc);
		Line a, b;
		a.I(r); a.I(o.sleep_count); a.I(o.direction_update_count); a.I(o.update_gfx); a.I(o.update_bv);
		b.I(rn); b.I(n.sleep_count); b.I(n.direction_update_count); b.I(n.update_gfx); b.I(n.update_bv);
		Dec("pe_update" + k.s, a.s, b.s);
	}
	for(int ub = 0; ub < 2; ++ub) for(int ax = 0; ax < 2; ++ax) for(int ac : {-1, 5}) for(int mc : {5, 6})
	{
		Line k; k.I(ub); k.I(ax); k.I(ac); k.I(mc);
		// iParticleEmitter::GetBoundingVolume
		OhplH12PeCtl o; o.update_bv = ub; o.axis_update_count = ac;
		uint32_t r = 0;
		if(o.update_bv)
		{
			r = 1;
			if(ax && o.axis_update_count != mc) { o.axis_update_count = mc; r |= 2; }
			o.update_bv = false;
		}
		OhplH12PeCtl n; n.update_bv = ub; n.axis_update_count = ac;
		uint32_t rn = ohpl_h12b_pe_bv_gate(&n, ax, mc);
		Line a, b; a.I(r); a.I(o.axis_update_count); a.I(o.update_bv); b.I(rn); b.I(n.axis_update_count); b.I(n.update_bv);
		Dec("pe_bv" + k.s, a.s, b.s);

		// UpdateGraphicsForViewport
		OhplH12PeCtl o2; o2.axis_update_count = ac;
		bool bo = false;
		if(ax && o2.axis_update_count != mc) { o2.axis_update_count = mc; bo = true; }
		OhplH12PeCtl n2; n2.axis_update_count = ac;
		bool bn = ohpl_h12b_pe_axis_gate(&n2, ax, mc);
		Line a2, b2; a2.I(bo); a2.I(o2.axis_update_count); b2.I(bn); b2.I(n2.axis_update_count);
		Dec("pe_axis" + k.s, a2.s, b2.s);
	}
	for(int f = 0; f < 2; ++f) for(int ax = 0; ax < 2; ++ax)
	{
		Line k; k.I(f); k.I(ax);
		const int o = !f ? 0 : (ax ? 1 : 2);
		Line a, b; a.I(o); b.I(ohpl_h12b_pe_model_kind(f, ax));
		Dec("pe_model" + k.s, a.s, b.s);
	}

	for(int root = 0; root < 2; ++root) for(int ex : {0, 2}) for(int mask = 0; mask < 16; ++mask) for(int nc = 0; nc <= 4; ++nc)
	{
		if(mask >> nc) continue;
		Line k; k.I(root); k.I(ex); k.I(mask); k.I(nc);
		// cParticleSystemData::LoadFromElement
		std::string ol; size_t lTotal = ex; bool bRet = false;
		if(root)
		{
			for(int i = 0; i < nc; ++i)
			{
				if(!((mask >> i) & 1)) { ol += '0'; continue; }
				ol += '1'; ++lTotal;
			}
			bRet = lTotal != 0;
		}
		bool vIs[5] = {}, vLoad[5] = {};
		for(int i = 0; i < nc; ++i) vIs[i] = (mask >> i) & 1;
		bool bn = ohpl_h12b_psdata_plan(root, vIs, nc, ex, vLoad);
		std::string rl;
		for(int i = 0; i < nc; ++i) rl += vLoad[i] ? '1' : '0';
		if(!root) { ol.assign(nc, '0'); }
		Dec("psdata" + k.s, " " + ol + " " + std::to_string(bRet), " " + rl + " " + std::to_string(bn));
	}
	for(int n : {0, 1, 3}) Dec("psdata_create " + std::to_string(n), n == 0 ? " 0" : " 1", ohpl_h12b_psdata_can_create(n) ? " 1" : " 0");

	const float fCols[] = {-1.0f, 0.0f, 0.5f};
	for(float r : fCols) for(float g : fCols) for(float b : fCols) for(int vis = 0; vis < 2; ++vis)
	{
		cColor c(r, g, b, 1);
		Line k; k.C(c); k.I(vis);
		bool o = true;
		if(c.r <= 0 && c.g <= 0 && c.b <= 0) o = false; else o = vis;
		Dec("visible" + k.s, " " + std::to_string(o), " " + std::to_string(ohpl_h12b_is_visible(&c, vis)));
	}

	const cVector3f vAxes[] = {cVector3f(0,1,0), cVector3f(0,-1,0), cVector3f(1,0,0), cVector3f(0.6f,0.8f,0), cVector3f(0,0.99999994f,0), cVector3f(0,1,1e-30f)};
	const float fSizes[][2] = {{1,2},{3,1},{2,2},{0.5f,0.25f}};
	for(int ty = 0; ty < 2; ++ty) for(const cVector3f &ax : vAxes) for(const auto &sz : fSizes)
	{
		Line k; k.I(ty); k.V3(ax); k.F(sz[0]); k.F(sz[1]);
		bool bo = false; float fo = 0;
		if(ty && ax != cVector3f(0,1,0))
		{
			float fMax = sz[0];
			if(fMax < sz[1]) fMax = sz[1];
			fMax *= kSqrt2f;
			fo = fMax; bo = true;
		}
		float fr = 0;
		bool bn = ohpl_h12b_bb_axis_size(ty, &ax, sz[0], sz[1], &fr);
		Line a, b; a.I(bo); if(bo) a.F(fo); b.I(bn); if(bn) b.F(fr);
		Dec("bb_axis" + k.s, a.s, b.s);
	}

	const float fNaN = std::nanf("");
	const float fRects[][4] = {{-0.5f,-0.5f,0.5f,0.5f},{-2,-2,2,2},{0,0,3,0.5f},{0.5f,0.5f,0.5f,0.5f},{-1,-1,1,1},{fNaN,-1,1,1},{-3,0.25f,-2,0.5f},{0.5f,0.5f,0.25f,0.75f}};
	for(int s : {0, 1, 50, 100}) for(int m : {1, 64, 100}) for(int clip = 0; clip < 2; ++clip) for(const auto &rc : fRects)
	{
		Line k; k.I(s); k.I(m); k.I(clip); k.F(rc[0]); k.F(rc[1]); k.F(rc[2]); k.F(rc[3]);
		// cBillboard::RetrieveOcculsionQuery
		float fAlpha = (float)s / (float)m;
		cVector3f vMin(rc[0], rc[1], 0), vMax(rc[2], rc[3], 0);
		if(clip)
		{
			cVector3f vTotalSize = vMax - vMin;
			if(vMin.x < -1) vMin.x = -1;
			if(vMin.y < -1) vMin.y = -1;
			if(vMax.x > 1) vMax.x = 1;
			if(vMax.y > 1) vMax.y = 1;
			cVector3f vInsideSize = vMax - vMin;
			float fInsideArea = vInsideSize.x * vInsideSize.y;
			float fTotalArea = vTotalSize.x * vTotalSize.y;
			if(fTotalArea > 0) fAlpha *= fInsideArea / fTotalArea;
			else fAlpha = 0;
		}
		else fAlpha = 0;
		cVector3f nMin(rc[0], rc[1], 0), nMax(rc[2], rc[3], 0);
		Line a, b; a.F(fAlpha); b.F(ohpl_h12b_bb_halo_alpha(s, m, clip, &nMin, &nMax));
		Dec("bb_halo_alpha" + k.s, a.s, b.s);
	}
	for(int su = 0; su < 2; ++su) for(int bc : {-1, 4, 5}) for(int tc : {4, 5})
	{
		Line k; k.I(su); k.I(bc); k.I(tc);
		bool bs = su; int lc = bc; uint32_t o = 0;
		if(bs) { bs = false; o |= 1; }
		if(lc != tc) { lc = tc; o |= 2; }
		bool bs2 = su; int lc2 = bc;
		uint32_t n = ohpl_h12b_bb_halo_gates(&bs2, &lc2, tc);
		Line a, b; a.I(o); a.I(bs); a.I(lc); b.I(n); b.I(bs2); b.I(lc2);
		Dec("bb_halo_gates" + k.s, a.s, b.s);
	}
	for(int s : {-1, 4}) for(int e : {-1, 4, 5}) for(int t : {4, -1, 5})
	{
		Line k; k.I(s); k.I(e); k.I(t);
		bool o = !(s == t && e == t);
		Dec("beam_geom" + k.s, " " + std::to_string(o), " " + std::to_string(ohpl_h12b_beam_needs_geometry(s, e, t)));
	}
	for(int av : {0, 1, 2, 5, 100}) for(int mx : {0, 1, 4, 8, -3})
	{
		Line k; k.I(av); k.I(mx);
		// cRopeEntity::UpdateGraphicsForViewport gather
		int lCount = 0;
		for(int i = 0; i < av; ++i)
		{
			if(lCount >= mx) break;
			++lCount;
		}
		Dec("rope_points" + k.s, " " + std::to_string(lCount), " " + std::to_string(ohpl_h12b_rope_points(av, mx)));
		Dec("rope_elems" + k.s, " " + std::to_string((lCount - 1) * 6), " " + std::to_string(ohpl_h12b_rope_elements(lCount)));
	}
	for(int act = 0; act < 2; ++act) for(int cnt : {-3, 0, 1, 16}) for(int ex : {-1, 0, 4, 64})
	{
		Line k; k.I(act); k.I(cnt); k.I(ex);
		int o = 0;
		if(act && cnt > 0)
		{
			bool bHas = ex >= 0;
			if(bHas && ex != cnt * 4) o = kOhplH12IrisRecreate;
			else if(!bHas) o = kOhplH12IrisCreate;
			else o = kOhplH12IrisReuse;
		}
		Dec("iris" + k.s, " " + std::to_string(o), " " + std::to_string(ohpl_h12b_iris_plan(act, cnt, ex)));
	}
	for(int m = 0; m < 8; ++m) for(float fade : {1.0f, 0.0f, -1.0f, 0.5f})
	{
		bool v[3] = {(m & 1) != 0, (m & 2) != 0, (m & 4) != 0};
		Line k; k.I(m); k.F(fade);
		int first = 1;
		if(!v[1]) { first = 1; for(int i = 0; i < 3; ++i) if(v[i]) { first = i; break; } } // IsFlareActive(Halo) else first active, else Halo
		bool any = false;
		if(fade > 0) for(int i = 0; i < 3; ++i) if(v[i]) { any = true; break; }
		Dec("flare" + k.s, " " + std::to_string(first) + " " + std::to_string(any),
			" " + std::to_string(ohpl_h12b_flare_first_active(v)) + " " + std::to_string(ohpl_h12b_flare_any_active(fade, v)));
	}
	for(int gui = 0; gui < 2; ++gui) for(int u3 = 0; u3 < 2; ++u3)
	{
		Line k; k.I(gui); k.I(u3);
		int g, rel, range;
		if(gui) { g = 1; rel = 1; range = 0; }
		else if(u3) { g = 0; rel = 0; range = 1; }
		else { g = 0; rel = 1; range = 0; }
		uint32_t r = ohpl_h12b_snd_play_route(gui, u3);
		Dec("snd_route" + k.s, " " + std::to_string(g) + " " + std::to_string(rel) + " " + std::to_string(range),
			" " + std::to_string(r & 1) + " " + std::to_string((r >> 1) & 1) + " " + std::to_string((r >> 2) & 1));
	}
}

// ---------------------------------------------------------------- scenes

struct Rig
{
	cRecordingConfig cfg;
	cRecordingLowLevelGraphics llg;
	cGraphics gfx;
	cResources res;
	cFrustum frustum;
	Rig() : llg(cfg), gfx(&llg, NULL), res(NULL, &llg)
	{
		// The managers are set up by cResources::Init, which the rig does not run.
		res.mpLowLevelSystem = NULL;
		res.mpImageManager = NULL; res.mpGpuShaderManager = NULL; res.mpParticleManager = NULL; res.mpSoundManager = NULL;
		res.mpFontManager = NULL; res.mpScriptManager = NULL; res.mpTextureManager = NULL; res.mpSoundEntityManager = NULL;
		res.mpAnimationManager = NULL; res.mpVideoManager = NULL; res.mpEntFileManager = NULL; res.mpMeshManager = NULL;
		res.mpMeshLoaderHandler = NULL; res.mpBitmapLoaderHandler = NULL; res.mpWorldLoaderHandler = NULL; res.mpVideoLoaderHandler = NULL;
		res.mpMaterialManager = new cMaterialManager(&gfx, &res);
		cMatrixf mtxView = cMatrixf::Identity, mtxProj = cMatrixf::Identity;
		frustum.SetupPerspectiveProj(mtxProj, mtxView, 100.0f, 0.1f, 1.0f, 1.3f, cVector3f(0, 0, 0));
	}
	~Rig() {}
	void Trace(const char *asTag)
	{
		const std::string t = llg.Trace().Text();
		fprintf(gOut, "trace %s %zu %016llx\n", asTag, t.size(), (unsigned long long)Fnv(t));
	}
};

void Vb(Line &l, iVertexBuffer *apVb, eVertexBufferElement aElem, int alMaxFloats)
{
	const int n = std::min(apVb->GetElementNum(aElem), alMaxFloats);
	const float *p = apVb->GetFloatArray(aElem);
	for(int i = 0; i < n; ++i) l.F(p[i]);
}

void Emit(const Line &l) { fprintf(gOut, "%s\n", l.s.c_str()); }

void CollectPs(const std::string &dir, std::vector<std::string> &out)
{
	DIR *d = opendir(dir.c_str());
	if(!d) return;
	while(dirent *e = readdir(d))
	{
		if(e->d_name[0] == '.') continue;
		std::string p = dir + "/" + e->d_name;
		struct stat st;
		if(stat(p.c_str(), &st) != 0) continue;
		if(S_ISDIR(st.st_mode)) CollectPs(p, out);
		else if(p.size() > 3 && strcasecmp(p.c_str() + p.size() - 3, ".ps") == 0) out.push_back(p);
	}
	closedir(d);
}

void EmitterLine(Line &l, iParticleEmitter *pe)
{
	l.I(pe->GetParticleNum()); l.I(pe->IsDead()); l.I(pe->IsDying()); l.I(pe->mCtl.sleep_count);
	l.I(pe->mCtl.update_bv); l.I(pe->IsVisible());
}

void PsScene(const std::string &asPath, int alSeed, int alFrames)
{
	TiXmlDocument doc;
	if(!doc.LoadFile(asPath.c_str())) return;
	TiXmlElement *pRoot = doc.RootElement();
	if(!pRoot) return;

	Rig rig;
	cXmlElement el(std::string(pRoot->Value()) == "ParticleSystem" ? "ParticleSystem" : "Other", NULL);
	for(TiXmlElement *e = pRoot->FirstChildElement(); e; e = e->NextSiblingElement())
	{
		cXmlElement *c = el.CreateChildElement(e->Value());
		for(TiXmlAttribute *a = e->FirstAttribute(); a; a = a->Next()) c->SetAttribute(a->Name(), a->Value());
	}

	srand(alSeed);
	cParticleSystemData data("data", &rig.res, &rig.gfx);
	const bool bLoad = data.LoadFromElement(&el);
	Line h; h.S("ps"); h.S(asPath.substr(asPath.rfind('/') + 1)); h.I(bLoad); h.I(data.mvEmitterData.size());
	Emit(h);
	if(!bLoad) { rig.Trace("ps"); return; }
	// Collision needs a physics world.
	for(size_t i = 0; i < data.mvEmitterData.size(); ++i) static_cast<cParticleEmitterData_UserData*>(data.mvEmitterData[i])->mbCollides = false;

	cParticleSystem *ps = data.Create("ps", cVector3f(1, 1, 1));
	if(!ps) { fprintf(gOut, "ps null\n"); return; }
	ps->SetPosition(cVector3f(1, 2, 3));
	for(int f = 0; f < alFrames; ++f)
	{
		if(f == 5) ps->SetVisible(false);
		if(f == 8) ps->SetVisible(true);
		if(f == 10) { ps->SetCulledByDistance(true); ps->SetCulledByFog(true); }
		if(f % 7 == 3) iRenderer::IncRenderFrameCount();
		if(f == alFrames - 6) ps->Kill();
		if(f == alFrames - 2) ps->KillInstantly();
		ps->UpdateLogic(1.0f / 30.0f);
		Line l; l.S("f"); l.I(f); l.I(ps->IsDead()); l.I(ps->IsDying()); l.I(ps->mbFirstUpdate); l.I(ps->mbIsVisible);
		for(int i = 0; i < ps->GetEmitterNum(); ++i)
		{
			iParticleEmitter *pe = ps->GetEmitter(i);
			EmitterLine(l, pe);
			cBoundingVolume *bv = pe->GetBoundingVolume();
			l.V3(bv->GetMin()); l.V3(bv->GetMax());
			l.I(pe->UpdateGraphicsForViewport(&rig.frustum, 0.033f));
			cMatrixf *m = pe->GetModelMatrix(&rig.frustum);
			l.F(m->m[0][0]); l.F(m->m[3][1]);
			l.I(pe->GetModelMatrix(NULL) == &pe->GetWorldMatrix());
		}
		Emit(l);
	}
	hplDelete(ps);
	rig.Trace("ps");
}

void SynthPs(const char *asName, std::vector<std::pair<const char*, const char*>> avAttrs, int alFrames)
{
	Rig rig;
	cXmlElement el("ParticleSystem", NULL);
	cXmlElement *c = el.CreateChildElement("ParticleEmitter");
	for(auto &a : avAttrs) c->SetAttribute(a.first, a.second);
	cXmlElement *x = el.CreateChildElement("Junk");
	x->SetAttribute("a", "b");
	srand(77);
	cParticleSystemData data("d", &rig.res, &rig.gfx);
	const bool bLoad = data.LoadFromElement(&el);
	Line h; h.S("synth"); h.S(asName); h.I(bLoad); h.I(data.mvEmitterData.size()); Emit(h);
	cParticleSystem *ps = data.Create("ps", cVector3f(1, 1, 1));
	if(!ps) return;
	for(int f = 0; f < alFrames; ++f)
	{
		ps->UpdateLogic(1.0f / 30.0f);
		Line l; l.S("f"); l.I(f); l.I(ps->IsDead()); l.I(ps->IsDying()); l.I(ps->mbFirstUpdate);
		ps->GetEmitter(0)->GetBoundingVolume();
		EmitterLine(l, ps->GetEmitter(0));
		Emit(l);
	}
	hplDelete(ps);
	rig.Trace("synth");
}

void BillboardScene()
{
	for(int ty : {eBillboardType_Point, eBillboardType_Axis, eBillboardType_FixedAxis})
	{
		Rig rig;
		cBillboard *bb = new cBillboard("bb", cVector2f(2, 3), (eBillboardType)ty, &rig.res, &rig.gfx);
		Line l; l.S("bb"); l.I(ty); l.I(bb->IsVisible());
		bb->SetColor(cColor(0, 0, 0, 1)); l.I(bb->IsVisible());
		bb->SetColor(cColor(0.5f, 0.25f, 0.125f, 0.75f)); l.I(bb->IsVisible());
		bb->SetBrightness(2); bb->SetBrightness(2);
		bb->SetHaloAlpha(0.5f);
		bb->SetSize(cVector2f(4, 1));
		bb->SetAxis(cVector3f(1, 1, 0));
		l.V3(bb->GetBoundingVolume()->GetMax());
		bb->SetAxis(cVector3f(0, 5, 0));
		l.V3(bb->GetBoundingVolume()->GetMax());
		bb->SetIsHalo(true); bb->SetIsHalo(true); bb->SetIsHalo(false);
		bb->SetHaloSourceSize(cVector3f(1, 2, 3));
		l.I(bb->UsesOcclusionQuery());
		bb->SetForwardOffset(0.5f);
		bb->SetPosition(cVector3f(1, 2, 3));
		cMatrixf *m = bb->GetModelMatrix(&rig.frustum);
		for(int i = 0; i < 4; ++i) for(int j = 0; j < 4; ++j) l.F(m->m[i][j]);
		Vb(l, bb->mpVtxBuffer, eVertexBufferElement_Color0, 16);
		Vb(l, bb->mpVtxBuffer, eVertexBufferElement_Position, 16);
		Emit(l);
		delete bb;
		rig.Trace("bb");
	}
}

void BeamScene()
{
	Rig rig;
	cBeam *b = new cBeam("beam", &rig.res, &rig.gfx);
	Line l; l.S("beam"); l.I(b->IsVisible());
	b->SetSize(cVector2f(0.5f, 2)); b->SetTileHeight(false); b->SetTileHeight(true);
	b->SetMultiplyAlphaWithColor(true);
	b->SetColor(cColor(1, 0.5f, 0.25f, 0.5f));
	b->GetEnd()->SetColor(cColor(0.25f, 0.5f, 1, 1));
	b->SetPosition(cVector3f(0, 0, 0));
	b->GetEnd()->SetPosition(cVector3f(3, 4, 5));
	for(int f = 0; f < 3; ++f)
	{
		b->UpdateGraphicsForFrame(0.03f);
		Vb(l, b->mpVtxBuffer, eVertexBufferElement_Position, 16);
		Vb(l, b->mpVtxBuffer, eVertexBufferElement_Texture0, 12);
		Vb(l, b->mpVtxBuffer, eVertexBufferElement_Color0, 16);
		l.V3(b->GetBoundingVolume()->GetMax());
		if(f == 1) b->GetEnd()->SetPosition(cVector3f(-1, 2, 0));
	}
	cMatrixf *m = b->GetModelMatrix(&rig.frustum);
	for(int i = 0; i < 4; ++i) for(int j = 0; j < 4; ++j) l.F(m->m[i][j]);
	b->SetColor(cColor(0, 0, 0, 1)); l.I(b->IsVisible());
	Emit(l);
	delete b;
	rig.Trace("beam");
}

void FlareScene()
{
	Rig rig;
	srand(5);
	cLensFlare *f = new cLensFlare("flare", &rig.res, &rig.gfx);
	Line l; l.S("flare"); l.I(f->mlMultiIrisSeed); l.I(f->IsVisible()); l.I(f->GetFirstActiveType()); l.I(f->IsAnyTypeActive());
	cMaterial *pFake = reinterpret_cast<cMaterial*>(0x1000);
	for(int mask = 0; mask < 8; ++mask)
	{
		for(int i = 0; i < 3; ++i) { f->mpMaterial[i] = NULL; f->mbActive[i] = false; }
		for(int i = 0; i < 3; ++i) if((mask >> i) & 1) { f->mpMaterial[i] = pFake; f->mbActive[i] = true; }
		for(float fade : {1.0f, 0.0f})
		{
			f->mfFade = fade;
			l.I(f->GetFirstActiveType()); l.I(f->IsAnyTypeActive()); l.I(f->IsVisible());
			iVertexBuffer *vb = f->PrepareMultiIrisVertexBuffer(&rig.frustum, 0.75f);
			l.I(vb != NULL);
			if(vb) { Vb(l, vb, eVertexBufferElement_Position, 24); Vb(l, vb, eVertexBufferElement_Color0, 24); }
		}
		f->mlMultiIrisCount = mask % 2 ? 4 : 16;
		f->mfFade = 1;
		f->UpdateLogic(0.25f);
		l.F(f->mfFade);
	}
	for(int i = 0; i < 3; ++i) f->mpMaterial[i] = NULL;
	Emit(l);
	delete f;
	rig.Trace("flare");
}

}

int hplMain(const hpl::tString&)
{
	const char *dump = getenv("OHPL_H12B_DUMP");
	gOut = dump ? fopen(dump, "w") : stdout;
	if(!gOut) return 2;
	const std::string sMode = getenv("OHPL_H12B_MODE") ? getenv("OHPL_H12B_MODE") : "all";
	int lEvery = getenv("OHPL_H12B_EVERY") ? atoi(getenv("OHPL_H12B_EVERY")) : 1;
	if(lEvery < 1) lEvery = 1;
	const int lFrames = getenv("OHPL_H12B_FRAMES") ? atoi(getenv("OHPL_H12B_FRAMES")) : 40;

	if(sMode == "dec" || sMode == "all") Decisions();
	if(sMode == "scene" || sMode == "all")
	{
		BillboardScene();
		BeamScene();
		FlareScene();
		SynthPs("basic", {{"MaxParticleNum","30"},{"Respawn","true"},{"ParticlesPerSecond","40"},{"MinLifeSpan","0.5"},{"MaxLifeSpan","1"},
			{"MinStartVel","-1 0 -1"},{"MaxStartVel","1 2 1"},{"MinStartSize","0.2 0.2"},{"MaxStartSize","0.4 0.4"}}, lFrames);
		SynthPs("warm", {{"MaxParticleNum","30"},{"Respawn","true"},{"ParticlesPerSecond","40"},{"MinLifeSpan","0.5"},{"MaxLifeSpan","1"},
			{"WarmUpTime","2"},{"WarmUpStepsPerSec","10"}}, lFrames);
		SynthPs("warm_zero_steps", {{"MaxParticleNum","10"},{"Respawn","true"},{"ParticlesPerSecond","10"},{"MinLifeSpan","1"},{"MaxLifeSpan","1"},
			{"WarmUpTime","1"},{"WarmUpStepsPerSec","0"}}, lFrames);
		SynthPs("axis_dir", {{"MaxParticleNum","10"},{"Respawn","true"},{"ParticlesPerSecond","10"},{"MinLifeSpan","1"},{"MaxLifeSpan","1"},
			{"DrawType","Axis"},{"UsesDirection","true"},{"CoordSystem","Local"}}, lFrames);
		SynthPs("oneshot", {{"MaxParticleNum","8"},{"Respawn","false"},{"ParticlesPerSecond","20"},{"MinLifeSpan","0.2"},{"MaxLifeSpan","0.4"}}, lFrames);

		std::vector<std::string> files;
		std::string roots = getenv("OHPL_H12B_DIRS") ? getenv("OHPL_H12B_DIRS") : "";
		for(size_t s = 0; s < roots.size();)
		{
			size_t e = roots.find(':', s);
			if(e == std::string::npos) e = roots.size();
			if(e > s) CollectPs(roots.substr(s, e - s), files);
			s = e + 1;
		}
		std::sort(files.begin(), files.end());
		int lDone = 0;
		for(size_t i = 0; i < files.size(); i += lEvery)
		{
			PsScene(files[i], 1000 + (int)i, lFrames);
			++lDone;
		}
		fprintf(stderr, "ps files %zu, run %d\n", files.size(), lDone);
	}
	if(dump) fclose(gOut);
	fprintf(stderr, "mismatches %d\n", gMismatch);
	return gMismatch ? 1 : 0;
}
