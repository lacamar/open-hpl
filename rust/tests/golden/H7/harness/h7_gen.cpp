// H7 differential harness. OHPL_IMPL=cpp and =rust must print the same text.
//   h7_gen script   (H7_SCENARIO=mat|pe|reg H7_SEED=n)  cMaterial state, iPostEffect base state, registries
//   h7_gen census   (.mat paths on stdin, relative to the cwd)  per-.mat outputs; unique inputs on stderr-free "U" lines
//   h7_gen pass     (H7_SEED=n)  post-effect passes over the recording low-level graphics: output + GPU trace
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <unistd.h>
#include <map>
#include <set>
#include <string>
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
#include "graphics/PostEffect.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/PostEffect_Bloom.h"
#include "graphics/PostEffect_ColorConvTex.h"
#include "graphics/PostEffect_ImageTrail.h"
#include "graphics/PostEffect_RadialBlur.h"
#include "graphics/FrameBuffer.h"
#include "graphics/RustMaterial.h"
#include "graphics/RustMaterialH7.h"
#include "graphics/RustMaterialShim.h"
#include "system/String.h"
#include "math/Frustum.h"
#include "graphics/GPUShader.h"
#include "graphics/GPUProgram.h"
#include "impl/LowLevelResourcesSDL.h"
#include "resources/GpuShaderManager.h"
#include "resources/Resources.h"
#include "resources/XmlDocument.h"
#include "trace/RecordingLowLevelGraphics.h"
#include "resources/RustResMgr.h"
#undef private
#undef protected

using namespace hpl;

int hplMain(const std::string&) { return 0; }

static cRecordingLowLevelGraphics* gLlg = NULL;

namespace hpl {
	iGpuShader* cGpuShaderManager::CreateShader(const tString& asName, eGpuShaderType aType, cParserVarContainer*)
	{
		return gLlg ? gLlg->CreateGpuShader(asName, aType) : NULL;
	}
}

static unsigned long long gRng;
static unsigned Rnd(unsigned n)
{
	gRng = gRng * 6364136223846793005ull + 1442695040888963407ull;
	return (unsigned)((gRng >> 33) % n);
}

static uint64_t Fnv(const void* p, size_t n, uint64_t h = 1469598103934665603ull)
{
	const unsigned char* c = (const unsigned char*)p;
	for(size_t i = 0; i < n; ++i) { h ^= c[i]; h *= 1099511628211ull; }
	return h;
}

static void PrintMatrix(const cMatrixf& m)
{
	for(int i = 0; i < 16; ++i) { uint32_t u; memcpy(&u, &m.v[i], 4); printf(" %08x", u); }
}

static uint32_t FloatBits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float RndF() { return (float)((int)Rnd(2001) - 1000) / 64.0f; }

struct TypeSet
{
	std::vector<iMaterialType*> types;
	std::vector<std::string> names;
	TypeSet()
	{
		Add(new cMaterialType_SolidDiffuse(NULL, NULL), "soliddiffuse");
		Add(new cMaterialType_Translucent(NULL, NULL), "translucent");
		Add(new cMaterialType_Water(NULL, NULL), "water");
		Add(new cMaterialType_Decal(NULL, NULL), "decal");
		Add(new cMaterialType_TerrainBlend(NULL, NULL), "terrainblend");
		Add(new cMaterialType_TerrainDecal(NULL, NULL), "terraindecal");
		Add(new cMaterialType_Undergrowth(NULL, NULL), "undergrowth");
		Add(new cMaterialType_ProjectedUV(NULL, NULL), "projecteduv");
		Add(new cMaterialType_VertexBlend(NULL, NULL), "vertexblend");
		Add(new cMaterialType_SolidDiffuse(NULL, NULL), "terrain");
	}
	void Add(iMaterialType* t, const char* n) { types.push_back(t); names.push_back(n); }
	int Find(const std::string& n) { for(size_t i = 0; i < names.size(); ++i) if(names[i] == n) return (int)i; return -1; }
};

static void PrintType(iMaterialType* t)
{
	printf("tr %d decal %d spec", t->IsTranslucent(), t->IsDecal());
	for(int m = 0; m < eMaterialRenderMode_LastEnum; ++m) printf(" %d", t->HasTypeSpecifics((eMaterialRenderMode)m));
	printf(" tex");
	for(int i = 0; i < t->GetUsedTextureNum(); ++i) printf(" %d", (int)t->GetUsedTexture(i)->mType);
	printf(" vars");
	for(int i = 0; i < t->GetUserVariableNum(); ++i)
	{
		cMaterialUserVariable* v = t->GetUserVariable(i);
		printf(" %s=%s", v->msName.c_str(), v->msValue.c_str());
	}
	printf("\n");
}

static void PrintMat(cMaterial& m)
{
	printf("blend %d alpha %d depth %d hasuv %d anims %d uv", (int)m.GetBlendMode(), (int)m.GetAlphaMode(), (int)m.GetDepthTest(),
		   (int)m.HasUvAnimation(), m.GetUvAnimationNum());
	PrintMatrix(m.GetUvMatrix());
	printf("\n");
}

static int MatScenario(int seed)
{
	gRng = (unsigned long long)seed * 7919 + 17;
	TypeSet ts;
	for(size_t i = 0; i < ts.types.size(); ++i) { printf("type %s ", ts.names[i].c_str()); PrintType(ts.types[i]); }
	int t0 = Rnd((unsigned)ts.types.size());
	cMaterial m("mat", _W(""), NULL, NULL, ts.types[t0]);
	fprintf(stderr, "rs %d\n", m.mpRs != NULL);
	printf("new %s ", ts.names[t0].c_str()); PrintMat(m);
	int ops = 30 + Rnd(40);
	for(int o = 0; o < ops; ++o)
	{
		unsigned r = Rnd(10);
		switch(r)
		{
		case 0: { int v = Rnd(8); printf("-o blend %d\n", v); m.SetBlendMode((eMaterialBlendMode)v); break; }
		case 1: { int v = Rnd(2); printf("-o alpha %d\n", v); m.SetAlphaMode((eMaterialAlphaMode)v); break; }
		case 2: { int v = Rnd(2); printf("-o depth %d\n", v); m.SetDepthTest(v != 0); break; }
		case 3: case 4:
		{
			int k = Rnd(3), a = Rnd(3); float sp = RndF(), am = RndF();
			printf("-o anim %d %d %08x %08x\n", k, a, FloatBits(sp), FloatBits(am));
			m.AddUvAnimation((eMaterialUvAnimation)k, sp, am, (eMaterialAnimationAxis)a);
			break;
		}
		case 5: printf("-o clear\n"); m.ClearUvAnimations(); break;
		case 6: case 7: case 8:
		{
			float dt = (float)Rnd(200) / 1000.0f;
			printf("-o update %08x\n", FloatBits(dt)); m.UpdateBeforeRendering(dt);
			break;
		}
		case 9:
		{
			int t = Rnd((unsigned)ts.types.size());
			printf("-o settype %s\n", ts.names[t].c_str()); m.SetType(ts.types[t]);
			break;
		}
		}
		PrintMat(m);
	}
	return 0;
}

struct StubParams : public iPostEffectParams
{
	StubParams(const char* n) : iPostEffectParams(n), mlX(0) {}
	kPostEffectParamsClassInit(StubParams)
	int mlX;
};

struct StubType : public iPostEffectType
{
	StubType(const char* n, cGraphics* g) : iPostEffectType(n, g, NULL) {}
	iPostEffect* CreatePostEffect(iPostEffectParams*) { return NULL; }
};

struct StubEffect : public iPostEffect
{
	StubEffect(cGraphics* g, iPostEffectType* t) : iPostEffect(g, NULL, t), mParams("") {}
	void OnSetActive(bool b) { printf("  onsetactive %d\n", b); }
	void OnSetParams() { printf("  onsetparams %d\n", mParams.mlX); }
	iPostEffectParams* GetTypeSpecificParams() { return &mParams; }
	iTexture* RenderEffect(iTexture*, iFrameBuffer*) { return NULL; }
	StubParams mParams;
};

static void PrintPe(StubEffect& e)
{
	printf("active %d disabled %d p %d\n", e.IsActive(), e.IsDisabled(), e.mParams.mlX);
}

static int PeScenario(int seed)
{
	gRng = (unsigned long long)seed * 7919 + 17;
	cRecordingLowLevelGraphics llg;
	gLlg = &llg;
	cGraphics gfx(&llg, NULL);
	const char* names[] = { "Bloom", "ImageTrail", "RadialBlur", "ColorConvTex", "Other" };
	StubType* types[5];
	for(int i = 0; i < 5; ++i) types[i] = new StubType(names[i], &gfx);
	int ti = Rnd(5);
	bool noType = Rnd(8) == 0;
	StubEffect e(&gfx, noType ? NULL : types[ti]);
	fprintf(stderr, "rs %d\n", e.mpRs != NULL);
	printf("new %s ", noType ? "(none)" : names[ti]); PrintPe(e);
	int ops = 30 + Rnd(30);
	for(int o = 0; o < ops; ++o)
	{
		switch(Rnd(6))
		{
		case 0: { int v = Rnd(2); printf("-o active %d\n", v); e.SetActive(v != 0); break; }
		case 1: { int v = Rnd(2); printf("-o disabled %d\n", v); e.SetDisabled(v != 0); break; }
		case 2: case 3:
		{
			int n = Rnd(5); StubParams p(names[n]); p.mlX = (int)Rnd(1000);
			printf("-o set %s %d\n", names[n], p.mlX); e.SetParams(&p);
			break;
		}
		default:
		{
			int n = Rnd(5); StubParams p(names[n]); p.mlX = -1;
			e.GetParams(&p); printf("-o get %s -> %d\n", names[n], p.mlX);
			break;
		}
		}
		PrintPe(e);
	}
	return 0;
}

static int RegScenario()
{
	TypeSet ts;
	printf("count %zu\n", (size_t)ohpl_h7_type_count());
	for(size_t i = 0; i < ts.types.size(); ++i) { printf("%s ", ts.names[i].c_str()); PrintType(ts.types[i]); }
	return 0;
}

// ---- census ----

static const int kKinds = 8;
static int KindOf(const std::string& type)
{
	static const char* n[] = { "soliddiffuse", "projecteduv", "vertexblend", "translucent", "water", "decal", "terrainblend", "undergrowth" };
	if(type == "terrain") return 0;
	if(type == "terraindecal") return 5;
	for(int i = 0; i < kKinds; ++i) if(type == n[i]) return i;
	return -1;
}

static const size_t kVarSize[kKinds] = { 172, 24, 88, 72, 60, 0, 136, 44 };

static const void* VarsPod(int kind, cMaterial* m)
{
	iMaterialVars* v = m->GetVars();
	switch(kind)
	{
	case 0: return (const OhplSolidDiffuseVars*)static_cast<cMaterialType_SolidDiffuse_Vars*>(v);
	case 1: return (const OhplProjectedUvVars*)static_cast<cMaterialType_ProjectedUV_Vars*>(v);
	case 2: return (const OhplVertexBlendVars*)static_cast<cMaterialType_VertexBlend_Vars*>(v);
	case 3: return (const OhplTranslucentVars*)static_cast<cMaterialType_Translucent_Vars*>(v);
	case 4: return (const OhplWaterVars*)static_cast<cMaterialType_Water_Vars*>(v);
	case 6: return (const OhplTerrainBlendVars*)static_cast<cMaterialType_TerrainBlend_Vars*>(v);
	case 7: return (const OhplUndergrowthVars*)static_cast<cMaterialType_Undergrowth_Vars*>(v);
	}
	return NULL;
}

struct VarsDump : public cResourceVarsObject
{
	void Print(std::string& out)
	{
		for(tResourceVarMapIt it = m_mapVars.begin(); it != m_mapVars.end(); ++it) out += "  " + it->first + "=" + it->second + "\n";
	}
};

static const char* GetTextureNameForCensus(int slot)
{
	static std::string names[64];
	std::string& s = names[slot];
	s.clear();
	ohpl_rm_mat_texture_string(slot, [](void* c, const uint8_t* p, size_t n){ static_cast<std::string*>(c)->assign((const char*)p, n); }, &s);
	return s.c_str();
}

static std::string gOut;
static void P(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void P(const char* fmt, ...)
{
	char buf[512];
	va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
	gOut += buf;
}

static int Census(TypeSet& ts, std::set<std::string>& unique)
{
	char line[4096];
	cLowLevelResourcesSDL res(NULL);
	bool printedGlobals = false;
	while(fgets(line, sizeof line, stdin))
	{
		size_t n = strlen(line);
		while(n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
		if(!n) continue;
		gOut.clear();
		iXmlDocument* doc = res.CreateXmlDocument();
		if(!doc->CreateFromFile(cString::UTF8ToWChar(line)))
		{
			printf("%s unreadable\n", line);
			delete doc;
			continue;
		}
		cXmlElement* main = doc->GetFirstElement("Main");
		tString sType = main ? cString::ToLowerCase(main->GetAttributeString("Type")) : "";
		int ti = ts.Find(sType);
		if(!main || sType == "" || ti < 0)
		{
			printf("%s skip %s\n", line, sType.c_str());
			delete doc;
			continue;
		}
		int kind = KindOf(sType);
		iMaterialType* type = ts.types[ti];
		bool bDepth = main->GetAttributeBool("DepthTest", true);
		tString sBlend = main->GetAttributeString("BlendMode", "Add");
		cMaterial mat("m", _W(""), NULL, NULL, type);
		mat.SetDepthTest(bDepth);
		bool bWarn = false;
		if(type->IsTranslucent()) mat.SetBlendMode((eMaterialBlendMode)ohpl_rm_mat_blend_mode((const uint8_t*)sBlend.data(), sBlend.size(), &bWarn));

		uint64_t texMask = 0;
		cXmlElement* texRoot = doc->GetFirstElement("TextureUnits");
		for(int i = 0; texRoot && i < type->GetUsedTextureNum(); ++i)
		{
			int slot = (int)type->GetUsedTexture(i)->mType;
			cXmlElement* c = texRoot->GetFirstElement(GetTextureNameForCensus(slot));
			if(!c) c = texRoot->GetFirstElement((tString(GetTextureNameForCensus(slot)) + "Side").c_str());
			if(c && c->GetAttributeString("File", "") != "") texMask |= 1ull << slot;
		}
		cXmlElement* uvRoot = doc->GetFirstElement("UvAnimations");
		if(uvRoot)
		{
			cXmlNodeListIterator it = uvRoot->GetChildIterator();
			while(it.HasNext())
			{
				cXmlElement* a = it.Next()->ToElement();
				tString t = a->GetAttributeString("Type"), ax = a->GetAttributeString("Axis");
				bool e1 = false, e2 = false;
				int at = ohpl_rm_mat_uv_anim_type((const uint8_t*)t.data(), t.size(), &e1);
				int aa = ohpl_rm_mat_anim_axis((const uint8_t*)ax.data(), ax.size(), &e2);
				mat.AddUvAnimation((eMaterialUvAnimation)at, a->GetAttributeFloat("Speed", 0), a->GetAttributeFloat("Amplitude", 0), (eMaterialAnimationAxis)aa);
			}
		}
		VarsDump vars;
		cXmlElement* varRoot = doc->GetFirstElement("SpecificVariables");
		if(varRoot) vars.LoadVariables(varRoot);
		type->LoadVariables(&mat, &vars);
		for(int i = 0; i < 3; ++i) mat.UpdateBeforeRendering(0.37f);

		P("%s %s tex %llx\n", line, sType.c_str(), (unsigned long long)texMask);
		P("mat blend %d alpha %d depth %d dof %d refl %d %08x large %d uv", (int)mat.GetBlendMode(), (int)mat.GetAlphaMode(), (int)mat.GetDepthTest(),
		  mat.GetAffectedByDepthOfField(), mat.GetWorldReflectionOcclusionTest(), FloatBits(mat.GetMaxReflectionDistance()),
		  mat.GetLargeTransperantSurface());
		for(int i = 0; i < 16; ++i) { uint32_t u; memcpy(&u, &mat.GetUvMatrix().v[i], 4); P(" %08x", u); }
		P("\n");

		OhplMatView view;
		memset(&view, 0, sizeof view);
		view.texMask = texMask;
		memcpy(view.uvMatrix, cMatrixf::Identity.v, sizeof view.uvMatrix);
		view.blendMode = mat.GetBlendMode();
		view.hasUvAnim = mat.HasUvAnimation();
		const void* pod = VarsPod(kind, &mat);
		OhplGlobals g = rustmat::MakeGlobals();
		if(!printedGlobals) { printedGlobals = true; printf("G"); for(size_t i = 0; i < sizeof g; ++i) printf("%02x", ((unsigned char*)&g)[i]); printf("\n"); }
		for(int sk = 0; sk < 2; ++sk)
			for(int m = 0; m < eMaterialRenderMode_LastEnum; ++m)
			{
				OhplProgSel s;
				ohpl_mat_program(kind, &view, pod, &g, m, sk, &s);
				P("prog %d %d: %d %d %x\n", sk, m, s.manager, s.mode, s.flags);
			}
		for(int m = 0; m < eMaterialRenderMode_LastEnum; ++m)
			for(int u = 0; u < kMaxTextureUnits; ++u)
			{
				OhplTexSel s;
				ohpl_mat_texture_unit(kind, &view, pod, &g, m, u, &s);
				P("tu %d %d: %d %d\n", m, u, s.kind, s.slot);
			}
		OhplMatCompileOut c;
		ohpl_mat_compile(kind, &view, pod, &g, &c);
		P("compile %x %x %x %x %d %d %d %d %d %d %d\n", c.specSet, c.specVal, c.objSet, c.objVal, c.alphaMode, c.dissolveFilter, c.blendMode,
		  c.hasRefraction, c.refractionEdge, c.worldReflection, c.translucentIllum);
		VarsDump out;
		rustmat::VarValues(kind, pod, &mat, &out);
		out.Print(gOut);

		printf("M %08x %016llx\n", (unsigned)Fnv(line, strlen(line)), (unsigned long long)Fnv(gOut.data(), gOut.size()));
		if(getenv("H7_FULL")) fputs(gOut.c_str(), stdout);

		// unique outputs-relevant input: kind, tex mask, blend, uv flag, vars bytes -> hash of the Rust-side outputs
		static const size_t kProgLen = 0;
		(void)kProgLen;
		std::string key;
		char hdr[128];
		snprintf(hdr, sizeof hdr, "U %d %llx %d %d ", kind, (unsigned long long)texMask, (int)mat.GetBlendMode(), (int)mat.HasUvAnimation());
		key = hdr;
		// the vars bytes with struct padding (uninitialised in the C++ object) zeroed: reload into a 0x00 and a 0xff buffer, keep equal bytes
		unsigned char b0[256], b1[256];
		memset(b0, 0, sizeof b0); memset(b1, 0xff, sizeof b1);
		if(pod)
		{
			OhplVarsVt vt = rustmat::MakeVarsVt(&vars);
			OhplLoadOut lo;
			ohpl_mat_default_vars(kind, b0); ohpl_mat_load_vars(kind, b0, &vt, &lo);
			ohpl_mat_default_vars(kind, b1); ohpl_mat_load_vars(kind, b1, &vt, &lo);
		}
		for(size_t i = 0; pod && i < kVarSize[kind]; ++i) { char b[4]; snprintf(b, sizeof b, "%02x", b0[i] == b1[i] ? b0[i] : 0); key += b; }
		if(!pod) key += "-";
		// outputs text without the per-material header lines: programs, texture units, compile
		size_t from = gOut.find("prog 0 0:");
		size_t to = gOut.find("compile");
		to = gOut.find('\n', to);
		std::string outs = gOut.substr(from, to - from + 1);
		char hb[40];
		snprintf(hb, sizeof hb, " %016llx", (unsigned long long)Fnv(outs.data(), outs.size()));
		unique.insert(key + hb);
		delete doc;
	}
	return 0;
}

// ---- passes ----

static int Pass(int seed)
{
	gRng = (unsigned long long)seed * 7919 + 17;
	cRecordingLowLevelGraphics llg;
	gLlg = &llg;
	cResources* resources = (cResources*)calloc(1, 65536);
	cGraphics gfx(&llg, NULL);
	gfx.mpResources = resources;
	cPostEffectComposite comp(&gfx);
	cPostEffectType_Bloom bloomT(&gfx, resources);
	cPostEffectType_ImageTrail trailT(&gfx, resources);
	cPostEffectType_RadialBlur radialT(&gfx, resources);
	cPostEffectType_ColorConvTex colorT(&gfx, resources);
	iPostEffectType* types[4] = { &bloomT, &trailT, &radialT, &colorT };
	cPostEffectParams_Bloom pb; cPostEffectParams_ImageTrail pt; cPostEffectParams_RadialBlur pr; cPostEffectParams_ColorConvTex pc;
	iPostEffectParams* params[4] = { &pb, &pt, &pr, &pc };
	std::vector<iPostEffect*> effects;
	int num = 1 + Rnd(4);
	for(int i = 0; i < num; ++i)
	{
		int t = Rnd(4);
		iPostEffect* e = types[t]->CreatePostEffect(params[t]);
		e->SetActive(Rnd(5) != 0);
		if(Rnd(8) == 0) e->SetDisabled(true);
		comp.AddPostEffect(e, (int)Rnd(10));
		effects.push_back(e);
		printf("effect %d type %d active %d\n", i, t, (int)e->IsActive());
	}
	iFrameBuffer* target = gfx.GetTempFrameBuffer(cVector2l(1280, 720), ePixelFormat_RGBA, 7);
	iTexture* input = target->GetColorBuffer(0)->ToTexture();
	cRenderTarget rt;
	rt.mpFrameBuffer = target;
	rt.mvPos = cVector2l(0, 0);
	rt.mvSize = cVector2l(1280, 720);
	cFrustum frustum;
	for(int f = 0; f < 3; ++f)
	{
		if(Rnd(3) == 0 && !effects.empty()) { effects[Rnd((unsigned)effects.size())]->SetActive(Rnd(2) != 0); }
		comp.Render(0.016f * (f + 1), &frustum, input, &rt);
		printf("frame %d has active %d\n", f, (int)comp.HasActiveEffects());
	}
	fputs(llg.Trace().Text().c_str(), stdout);
	fflush(stdout);
	_exit(0); // destroying cGraphics without an engine crashes
}

int main(int argc, char** argv)
{
	const char* mode = argc > 1 ? argv[1] : "script";
	if(!strcmp(mode, "script"))
	{
		const char* sc = getenv("H7_SCENARIO");
		int seed = getenv("H7_SEED") ? atoi(getenv("H7_SEED")) : 0;
		if(sc && !strcmp(sc, "pe")) return PeScenario(seed);
		if(sc && !strcmp(sc, "reg")) return RegScenario();
		return MatScenario(seed);
	}
	if(!strcmp(mode, "census"))
	{
		TypeSet ts;
		std::set<std::string> unique;
		int rc = Census(ts, unique);
		for(std::set<std::string>::iterator it = unique.begin(); it != unique.end(); ++it) printf("%s\n", it->c_str());
		return rc;
	}
	if(!strcmp(mode, "pass")) return Pass(getenv("H7_SEED") ? atoi(getenv("H7_SEED")) : 0);
	return 1;
}
