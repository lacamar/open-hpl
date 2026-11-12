// H5 differential harness: drives cFrameBitmap (atlas) and cProgramComboManager (settings) with a
// seeded script and prints every observable. Run with OHPL_IMPL=cpp and =rust and diff stdout.
// usage: H5_SCENARIO=atlas|combo H5_SEED=<n> h5_trace   ("-o" lines are the script ops; stderr = GPU trace)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#define private public
#define protected public
#include "graphics/FrameBitmap.h"
#include "graphics/ProgramComboManager.h"
#include "graphics/FrameSubImage.h"
#include "resources/Resources.h"
#undef private
#undef protected
#include "graphics/Bitmap.h"
#include "graphics/FrameSubImage.h"
#include "graphics/FrameTexture.h"
#include "resources/GpuShaderManager.h"
#include "system/PreprocessParser.h"
#include "trace/RecordingLowLevelGraphics.h"

using namespace hpl;

static unsigned long long gRng;
static unsigned Rnd(unsigned n)
{
	gRng = gRng * 6364136223846793005ull + 1442695040888963407ull;
	return (unsigned)((gRng >> 33) % n);
}

static unsigned long long Fnv(const unsigned char *p, size_t n)
{
	unsigned long long h = 1469598103934665603ull;
	for(size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; }
	return h;
}

// Source pixels: byte k of add number n (the Rust replay regenerates them with the same formula).
static cBitmap* MakeSource(int n, int w, int h)
{
	cBitmap *b = new cBitmap();
	b->CreateData(cVector3l(w, h, 1), ePixelFormat_RGBA, 0, 0);
	cBitmapData *d = b->GetData(0, 0);
	for(int k = 0; k < d->mlSize; ++k) d->mpData[k] = (unsigned char)(n * 131 + k * 7 + (k >> 8) * 13);
	return b;
}

static int AtlasScenario(int seed)
{
	gRng = (unsigned long long)seed * 7919 + 17;
	cRecordingLowLevelGraphics llg;
	int sizes[] = { 64, 128, 256 };
	int aw = sizes[Rnd(3)], ah = sizes[Rnd(3)];
	printf("frame %d %d\n", aw, ah);
	iTexture *tex = llg.CreateTexture("atlas", eTextureType_2D, eTextureUsage_Normal);
	cFrameTexture *ft = new cFrameTexture(tex, 3, NULL, false);
	cBitmap *bmp = new cBitmap();
	bmp->CreateData(cVector3l(aw, ah, 1), ePixelFormat_RGBA, 0, 0);
	cFrameBitmap *fb = new cFrameBitmap(bmp, ft, 3);

	std::vector<cFrameSubImage*> subs;
	std::map<cFrameSubImage*, int> ids;
	int adds = 0;
	int ops = 30 + Rnd(30);
	for(int o = 0; o < ops; ++o)
	{
		// mbIsUpdated is uninitialised until the first add (bd quirk), so the script starts with one.
		unsigned r = o == 0 ? 1 : Rnd(10);
		if(r < 5)
		{
			int w = 1 + Rnd(r == 0 ? 70 : 30), h = 1 + Rnd(r == 0 ? 70 : 30);
			cBitmap *src = MakeSource(adds, w, h);
			bool found = false;
			cFrameSubImage *s = fb->AddBitmap(src, _W("p"), NULL, &found);
			printf("-o add %d %d %d\n", adds, w, h);
			printf("found=%d", found ? 1 : 0);
			if(s)
			{
				int id = (int)subs.size();
				subs.push_back(s);
				ids[s] = id;
				printf(" sub=%d rect=%d,%d,%d,%d", id, s->mRect.x, s->mRect.y, s->mRect.w, s->mRect.h);
			}
			printf(" full=%d upd=%d adds=%d pic=%d texpic=%d\n", fb->IsFull(), fb->IsUpdated(),
				   fb->GetAdditionsSinceReorganization(), fb->mlPicCount, ft->GetPicCount());
			delete src;
			++adds;
		}
		else if(r < 7 && !subs.empty())
		{
			size_t i = Rnd((unsigned)subs.size());
			if(subs[i])
			{
				printf("-o del %d\n", (int)i);
				delete subs[i];
				subs[i] = NULL;
			}
		}
		else if(r < 8)
		{
			bool l = Rnd(2) != 0;
			printf("-o lock %d\n", l);
			fb->SetLocked(l);
			printf("locked=%d\n", fb->IsLocked());
		}
		else if(r < 9)
		{
			printf("-o flush\n");
			fb->UpdateBeforeDraw();
			printf("flushed upd=%d adds=%d pic=%d texpic=%d hash=%016llx\n", fb->IsUpdated(),
				   fb->GetAdditionsSinceReorganization(), fb->mlPicCount, ft->GetPicCount(),
				   Fnv(bmp->GetData(0, 0)->mpData, bmp->GetData(0, 0)->mlSize));
			for(cFBitmapImageListIt it = fb->mlstImages.begin(); it != fb->mlstImages.end(); ++it)
			{
				cFBitmapImage *im = *it;
				printf("  img sub=%d rect=%d,%d,%d,%d\n", im->mpSubImage ? ids[im->mpSubImage] : -1,
					   im->mRect.x, im->mRect.y, im->mRect.w, im->mRect.h);
			}
		}
		else if(!subs.empty())
		{
			size_t i = Rnd((unsigned)subs.size());
			if(subs[i])
			{
				printf("-o subflush %d\n", (int)i);
				subs[i]->Flush();
				const tVertexVec &v = subs[i]->GetVertexVec();
				printf("sub %d upd=%d", (int)i, subs[i]->GetUpdateCount());
				for(int k = 0; k < 4; ++k)
				{
					unsigned a, b;
					memcpy(&a, &v[k].tex.x, 4);
					memcpy(&b, &v[k].tex.y, 4);
					printf(" %08x,%08x", a, b);
				}
				printf("\n");
			}
		}
	}
	printf("-o flush\n");
	fb->UpdateBeforeDraw();
	printf("final upd=%d hash=%016llx\n", fb->IsUpdated(), Fnv(bmp->GetData(0, 0)->mpData, bmp->GetData(0, 0)->mlSize));
	for(size_t i = 0; i < subs.size(); ++i) delete subs[i];
	delete fb;
	delete ft;
	fputs(llg.Trace().Text().c_str(), stderr);
	return 0;
}

static cRecordingLowLevelGraphics *gLlg = NULL;

extern "C" iGpuShader* __wrap__ZN3hpl17cGpuShaderManager12CreateShaderERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS_14eGpuShaderTypeEPNS_19cParserVarContainerE(
	void*, const tString &asName, eGpuShaderType aType, cParserVarContainer *apVars)
{
	printf("create file=%s type=%d vars=", asName.c_str(), (int)aType);
	tParseVarMap *m = apVars->GetMapPtr();
	for(tParseVarMapIt it = m->begin(); it != m->end(); ++it)
		printf("%s=%s;", it->first.c_str(), it->second.c_str());
	printf("\n");
	iGpuShader *s = gLlg->CreateGpuShader(asName, aType);
	return s;
}

static int ComboScenario(int seed)
{
	gRng = (unsigned long long)seed * 104729 + 5;
	cRecordingLowLevelGraphics llg;
	gLlg = &llg;
	cResources *fakeRes = (cResources*)malloc(sizeof(cResources));
	memset(fakeRes, 0, sizeof(cResources));
	cGpuShaderManager *shaderMgr = new cGpuShaderManager(NULL, &llg, NULL, NULL);
	fakeRes->mpGpuShaderManager = shaderMgr;
	int nModes = 1 + Rnd(3);
	char name[32];
	snprintf(name, sizeof(name), "mat%u", Rnd(100));
	cProgramComboManager *m = new cProgramComboManager(name, NULL, fakeRes, nModes);
	printf("mgr %s %d\n", name, nModes);
	if(Rnd(3) == 0)
	{
		snprintf(name, sizeof(name), "renamed%u", Rnd(100));
		m->SetName(name);
		printf("-o rename %s\n", name);
	}
	std::vector<int> nFeat(nModes);
	for(int mode = 0; mode < nModes; ++mode)
	{
		char mn[32], vs[32], fs[32];
		snprintf(mn, sizeof(mn), "mode%d_%u", mode, Rnd(50));
		snprintf(vs, sizeof(vs), "vs_%u.vert", Rnd(50));
		snprintf(fs, sizeof(fs), "fs_%u.frag", Rnd(50));
		int nf = Rnd(7);
		nFeat[mode] = nf;
		std::vector<cProgramComboFeature> feats;
		printf("-o setup %d %s %s %s", mode, mn, vs, fs);
		for(int i = 0; i < nf; ++i)
		{
			char v[32];
			snprintf(v, sizeof(v), "Feat%u", Rnd(30));
			unsigned mask = 1 + Rnd(3);
			feats.push_back(cProgramComboFeature(v, mask));
			printf(" f:%u:%s", mask, v);
		}
		cParserVarContainer defs;
		int nd = Rnd(4);
		for(int i = 0; i < nd; ++i)
		{
			char k[32], v[32];
			snprintf(k, sizeof(k), "Def%u", Rnd(10));
			snprintf(v, sizeof(v), "%u", Rnd(1000));
			defs.Add(k, v);
		}
		for(tParseVarMapIt it = defs.GetMapPtr()->begin(); it != defs.GetMapPtr()->end(); ++it)
			printf(" d:%s:%s", it->first.c_str(), it->second.c_str());
		printf("\n");
		m->SetupGenerateProgramData(mode, mn, vs, fs, feats.empty() ? NULL : &feats[0], nf, defs);
		printf("combos=%d\n", m->GetGenerateCombinationNum(mode));
	}
	int ops = 20 + Rnd(20);
	for(int o = 0; o < ops; ++o)
	{
		int mode = Rnd(nModes);
		int flags = Rnd(1u << (nFeat[mode] > 0 ? nFeat[mode] : 1));
		if(Rnd(2) == 0)
		{
			printf("-o name %d %d\n", mode, flags);
			printf("%s\n", m->GenerateProgramName(mode, flags).c_str());
		}
		else
		{
			unsigned type = Rnd(2) ? kPC_VertexBit : kPC_FragmentBit;
			const cProgramComboSettings &s = m->mvSettings[mode];
			printf("-o shader %d %d %u\n", mode, flags, type);
			iGpuShader *sh = m->GetShaderForCombo(mode, flags, type == kPC_VertexBit ? s.msVtxShader : s.msFragShader, type);
			printf("shader id=%u\n", sh ? sh->GetUserId() : 0u);
		}
	}
	printf("-o end\n");
	delete m;
	delete shaderMgr;
	gLlg = NULL;
	return 0;
}

int hplMain(const tString&)
{
	const char *sKind = getenv("H5_SCENARIO"), *sSeed = getenv("H5_SEED");
	if(!sKind || !sSeed) return 2;
	int seed = atoi(sSeed);
	if(!strcmp(sKind, "atlas")) return AtlasScenario(seed);
	if(!strcmp(sKind, "combo")) return ComboScenario(seed);
	return 2;
}
