// L3: drives cSDLTexture, the vertex buffers, cFrameBufferGL and the GLSL classes through a script
// against fake GL entry points and writes the call trace (L3_TRACE_OUT) and, on the Rust path, the
// log of shell-to-Rust calls (L3_FFI_OUT). Run with OHPL_IMPL=cpp and OHPL_IMPL=rust and diff.
#include "impl/LowLevelGraphicsSDL.h"
#include "impl/RustBackends.h"
#include "graphics/Bitmap.h"
#include "graphics/FrameBuffer.h"
#include "graphics/Texture.h"
#include "graphics/VertexBuffer.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#define protected public
#include "impl/VertexBufferOpenGL.h"
#undef protected
#include "fakegl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

extern std::string gFfi;
extern bool gFfiOn;
using namespace hpl;

static void Ffi(const std::string &s) { if(gFfiOn) gFfi += s + "\n"; }

static int gRanges = 0;

static void Reg(const void *apPtr, size_t alBytes, const std::string &asLabel)
{
	AddRange(apPtr, alBytes, asLabel.c_str());
	Ffi("@range " + asLabel + " " + std::to_string(alBytes));
	++gRanges;
}

static void ClearReg()
{
	if(gRanges == 0) return;
	ClearRanges();
	Ffi("@clear");
	gRanges = 0;
}

static std::map<int, iTexture*> gTex;
static std::map<int, iFrameBuffer*> gFb;
static std::map<int, iDepthStencilBuffer*> gDsb;
static std::map<int, iVertexBuffer*> gVb;
static std::map<int, iGpuShader*> gSh;
static std::map<int, iGpuProgram*> gProg;
static std::map<std::pair<int,int>, int> gVar;
static std::map<int, std::vector<float> > gArr;
static int gBmpN = 0;

static cBitmap* MakeBitmap(const cVector3l &avSize, ePixelFormat aFormat, int alImages, int alMips)
{
	cBitmap *pBmp = new cBitmap();
	pBmp->SetUpData(alImages, alMips);
	std::string sBase = "b" + std::to_string(gBmpN++);
	for(int i=0; i<alImages; ++i)
	{
		for(int m=0; m<alMips; ++m)
		{
			pBmp->CreateData(avSize, aFormat, i, m);
			cBitmapData *pData = pBmp->GetData(i, m);
			Reg(pData->mpData, pData->mlSize, sBase + "i" + std::to_string(i) + "m" + std::to_string(m));
		}
	}
	return pBmp;
}

static std::string Shader(int alSeed, int alLines)
{
	std::string s;
	for(int i=0; i<alLines; ++i) s += "void f" + std::to_string(alSeed + i) + "() {}\r\n";
	if(alSeed & 1) s += "tail";
	return s;
}

static void RegVb(int alIdx, iVertexBuffer *apVb)
{
	ClearReg();
	iVertexBufferOpenGL *pVb = dynamic_cast<iVertexBufferOpenGL*>(apVb);
	if(pVb == NULL) return;
	for(size_t i=0; i<pVb->mvElementArrays.size(); ++i)
	{
		cVtxBufferGLElementArray *pEl = pVb->mvElementArrays[i];
		if(pEl->Size() == 0) continue;
		Reg(pEl->GetArrayPtr(), pEl->Size() * GetVertexFormatByteSize(pEl->mFormat), "v" + std::to_string(alIdx) + "e" + std::to_string((int)pEl->mType));
	}
	if(pVb->mvIndexArray.size() > 0) Reg(&pVb->mvIndexArray[0], pVb->mvIndexArray.size() * 4, "v" + std::to_string(alIdx) + "i");
}

int hplMain(const tString&)
{
	const char *sOut = getenv("L3_TRACE_OUT");
	const char *sScript = getenv("L3_SCRIPT");
	if(!sOut || !sScript) return 2;
	cLowLevelGraphicsSDL *g = new cLowLevelGraphicsSDL();
	g->Init(640, 480, 0, 32, 1, 4, eGpuProgramFormat_GLSL, "l3", cVector2l(0, 0));
	for(int i = 0; i < eGraphicCaps_LastEnum; ++i) g->GetCaps((eGraphicCaps)i);
	gTrace.clear();
	gGen = gShaders = gPrograms = 0;
	gGlErrors = gFailCompile = gFailLink = 0;
	gFbStatus = 0x8CD5;
	ClearRanges();
	gFfiOn = true;

	std::ifstream in(sScript);
	std::string sLine;
	while(std::getline(in, sLine))
	{
		if(sLine.empty() || sLine[0] == '#') continue;
		std::istringstream ss(sLine);
		std::string op;
		ss >> op;
		auto I = [&]() { long v = 0; ss >> v; return (int)v; };
		auto F = [&]() { float v = 0; ss >> v; return v; };
		auto S = [&]() { std::string v; ss >> v; return v; };
		gTrace += "== " + sLine + "\n";
		ClearReg();

		if(op == "fail")
		{
			std::string k = S();
			int n = I();
			if(k == "errors") gGlErrors = n;
			else if(k == "compile") gFailCompile = n;
			else if(k == "link") gFailLink = n;
			else if(k == "fbstatus") gFbStatus = n;
			Ffi("@fail " + k + " " + std::to_string(n));
		}
		else if(op == "tex_new")
		{
			int i = I(), type = I(), usage = I(), mips = I(), srgb = I(), down = I();
			iTexture *t = g->CreateTexture("t" + std::to_string(i), (eTextureType)type, (eTextureUsage)usage);
			t->SetUseMipMaps(mips != 0);
			t->SetsRGB(srgb != 0);
			t->SetSizeDownScaleLevel(down);
			gTex[i] = t;
		}
		else if(op == "tex_bmp")
		{
			int i = I(), fmt = I(), w = I(), h = I(), d = I(), images = I(), mips = I();
			cBitmap *pBmp = MakeBitmap(cVector3l(w, h, d), (ePixelFormat)fmt, images, mips);
			bool b = gTex[i]->CreateFromBitmap(pBmp);
			gTrace += "ret=" + std::to_string(b) + "\n";
			delete pBmp;
			ClearReg();
		}
		else if(op == "tex_anim")
		{
			int i = I(), n = I(), fmt = I(), w = I(), h = I(), d = I(), mips = I();
			std::vector<cBitmap*> v;
			for(int k=0; k<n; ++k) v.push_back(MakeBitmap(cVector3l(w, h, d), (ePixelFormat)fmt, 1, mips));
			bool b = gTex[i]->CreateAnimFromBitmapVec(&v);
			gTrace += "ret=" + std::to_string(b) + "\n";
			for(size_t k=0; k<v.size(); ++k) delete v[k];
			ClearReg();
		}
		else if(op == "tex_cube")
		{
			int i = I(), n = I(), fmt = I(), w = I(), h = I(), mips = I();
			std::vector<cBitmap*> v;
			for(int k=0; k<n; ++k) v.push_back(MakeBitmap(cVector3l(w, h, 1), (ePixelFormat)fmt, 1, mips));
			bool b = gTex[i]->CreateCubeFromBitmapVec(&v);
			gTrace += "ret=" + std::to_string(b) + "\n";
			for(size_t k=0; k<v.size(); ++k) delete v[k];
			ClearReg();
		}
		else if(op == "tex_raw" || op == "tex_setraw")
		{
			int i = I();
			int level = 0, ox = 0, oy = 0, oz = 0;
			if(op == "tex_setraw") { level = I(); ox = I(); oy = I(); oz = I(); }
			int fmt = I(), w = I(), h = I(), d = I();
			std::vector<unsigned char> data((size_t)w * h * d * 16 + 16);
			Reg(&data[0], data.size(), "raw" + std::to_string(gBmpN++));
			if(op == "tex_raw")
			{
				bool b = gTex[i]->CreateFromRawData(cVector3l(w, h, d), (ePixelFormat)fmt, &data[0]);
				gTrace += "ret=" + std::to_string(b) + "\n";
			}
			else gTex[i]->SetRawData(level, cVector3l(ox, oy, oz), cVector3l(w, h, d), (ePixelFormat)fmt, &data[0]);
			ClearReg();
		}
		else if(op == "tex_filter") { int i = I(); gTex[i]->SetFilter((eTextureFilter)I()); }
		else if(op == "tex_aniso") { int i = I(); gTex[i]->SetAnisotropyDegree(F()); }
		else if(op == "tex_wrap")
		{
			int i = I(); std::string w = S(); eTextureWrap m = (eTextureWrap)I();
			if(w == "s") gTex[i]->SetWrapS(m); else if(w == "t") gTex[i]->SetWrapT(m);
			else if(w == "r") gTex[i]->SetWrapR(m); else gTex[i]->SetWrapSTR(m);
			gTrace += "wrap=" + std::to_string(gTex[i]->GetWrapS()) + std::to_string(gTex[i]->GetWrapT()) + std::to_string(gTex[i]->GetWrapR()) + "\n";
		}
		else if(op == "tex_cmpmode") { int i = I(); gTex[i]->SetCompareMode((eTextureCompareMode)I()); }
		else if(op == "tex_cmpfunc") { int i = I(); gTex[i]->SetCompareFunc((eTextureCompareFunc)I()); }
		else if(op == "tex_red") { int i = I(); gTex[i]->SetRedFromAlpha(I() != 0); }
		else if(op == "tex_mips") { gTex[I()]->AutoGenerateMipmaps(); }
		else if(op == "tex_pixels")
		{
			int i = I();
			std::vector<float> v;
			bool b = gTex[i]->GetRawPixelsRGBAFloat(v);
			gTrace += "pixels=" + std::to_string(b) + "," + std::to_string(v.size()) + "\n";
		}
		else if(op == "tex_time") { int i = I(); gTex[i]->SetFrameTime(F()); gTex[i]->SetAnimMode((eTextureAnimMode)I()); }
		else if(op == "tex_update") { int i = I(); gTex[i]->Update(F()); }
		else if(op == "tex_next") { gTex[I()]->NextFrame(); }
		else if(op == "tex_prev") { gTex[I()]->PrevFrame(); }
		else if(op == "tex_settime") { int i = I(); gTex[i]->SetTimeCount(F()); }
		else if(op == "tex_state")
		{
			int i = I();
			iTexture *t = gTex[i];
			gTrace += "state=" + std::to_string(t->GetCurrentLowlevelHandle()) + "," + std::to_string(t->HasAnimation()) + "," +
					  A(t->GetTimeCount()) + "," + A(t->GetT()) + "," + std::to_string(t->GetMemorySize()) + "," +
					  std::to_string(t->UsesMipMaps()) + "," + std::to_string(t->IsCompressed()) + "," +
					  std::to_string(t->GetSize().x) + "x" + std::to_string(t->GetSize().y) + "x" + std::to_string(t->GetSize().z) + "," +
					  std::to_string(t->GetPixelFormat()) + "," + std::to_string(t->GetFilter()) + "\n";
		}
		else if(op == "tex_free") { int i = I(); delete gTex[i]; gTex.erase(i); }

		else if(op == "dsb_new") { int i = I(), w = I(), h = I(), d = I(), s = I(); gDsb[i] = g->CreateDepthStencilBuffer(cVector2l(w, h), d, s); }
		else if(op == "dsb_free") { int i = I(); delete gDsb[i]; gDsb.erase(i); }
		else if(op == "fb_new") { int i = I(); gFb[i] = g->CreateFrameBuffer("fb" + std::to_string(i)); }
		else if(op == "fb_tex")
		{
			int i = I(), kind = I(), idx = I(), t = I(), mip = I(), extra = I();
			iTexture *pTex = t >= 0 ? gTex[t] : NULL;
			iFrameBuffer *f = gFb[i];
			switch(kind)
			{
			case 0: f->SetTexture2D(idx, pTex, mip); break;
			case 1: f->SetTexture3D(idx, pTex, extra, mip); break;
			case 2: f->SetTextureCubeMap(idx, pTex, extra, mip); break;
			case 3: f->SetDepthTexture2D(pTex, mip); break;
			default: f->SetDepthTextureCubeMap(pTex, extra, mip); break;
			}
			gTrace += "fbsize=" + std::to_string(f->GetSize().x) + "x" + std::to_string(f->GetSize().y) + "\n";
		}
		else if(op == "fb_dsb") { int i = I(), d = I(); gFb[i]->SetDepthStencilBuffer(d >= 0 ? gDsb[d] : NULL); gTrace += "fbsize=" + std::to_string(gFb[i]->GetSize().x) + "\n"; }
		else if(op == "fb_compile") { int i = I(); gTrace += "ret=" + std::to_string(gFb[i]->CompileAndValidate()) + "\n"; }
		else if(op == "fb_post") { gFb[I()]->PostBindUpdate(); }
		else if(op == "fb_free") { int i = I(); delete gFb[i]; gFb.erase(i); }

		else if(op == "vb_new")
		{
			int i = I(), type = I(), draw = I(), usage = I(), rv = I(), ri = I();
			gVb[i] = g->CreateVertexBuffer((eVertexBufferType)type, (eVertexBufferDrawType)draw, (eVertexBufferUsageType)usage, rv, ri);
		}
		else if(op == "vb_elem")
		{
			int i = I(), e = I(), f = I(), n = I(), var = I();
			gVb[i]->CreateElementArray((eVertexBufferElement)e, (eVertexBufferElementFormat)f, n, var);
		}
		else if(op == "vb_num") { int i = I(); gVb[i]->SetElementNum(I()); }
		else if(op == "vb_fill")
		{
			int i = I(), nv = I(), ni = I();
			iVertexBuffer *v = gVb[i];
			for(int e=0; e<eVertexBufferElement_LastEnum; ++e)
			{
				eVertexBufferElement elem = (eVertexBufferElement)e;
				int n = v->GetElementNum(elem);
				if(n == 0) continue;
				eVertexBufferElementFormat f = v->GetElementFormat(elem);
				for(int k=0; k<nv; ++k)
				{
					cVector3f vec(0.25f * k, 0.5f + e, 0.125f * (k + e));
					if(elem == eVertexBufferElement_Color0 || elem == eVertexBufferElement_Color1)
						v->AddVertexColor(elem, cColor(0.1f * k, 0.2f, 0.3f, 0.4f));
					else if(f == eVertexBufferElementFormat_Float && n == 4) v->AddVertexVec4f(elem, vec, 1.0f);
					else if(f == eVertexBufferElementFormat_Float) v->AddVertexVec3f(elem, vec);
				}
				if(f != eVertexBufferElementFormat_Float && elem != eVertexBufferElement_Color0 && elem != eVertexBufferElement_Color1)
					v->ResizeArray(elem, nv * n);
				else if(elem == eVertexBufferElement_Color0 || elem == eVertexBufferElement_Color1)
					if(f != eVertexBufferElementFormat_Float) v->ResizeArray(elem, nv * n);
			}
			for(int k=0; k<ni; ++k) v->AddIndex(k % (nv > 0 ? nv : 1));
			gTrace += "vtx=" + std::to_string(v->GetVertexNum()) + " idx=" + std::to_string(v->GetIndexNum()) + "\n";
		}
		else if(op == "vb_compile") { int i = I(); RegVb(i, gVb[i]); bool b = gVb[i]->Compile(I()); gTrace += "ret=" + std::to_string(b) + "\n"; RegVb(i, gVb[i]); }
		else if(op == "vb_update") { int i = I(); unsigned t = (unsigned)I(); int idx = I(); RegVb(i, gVb[i]); gVb[i]->UpdateData(t, idx != 0); }
		else if(op == "vb_bind") { int i = I(); RegVb(i, gVb[i]); gVb[i]->Bind(); }
		else if(op == "vb_unbind") { gVb[I()]->UnBind(); }
		else if(op == "vb_draw") { int i = I(); RegVb(i, gVb[i]); gVb[i]->Draw((eVertexBufferDrawType)I()); }
		else if(op == "vb_drawidx")
		{
			int i = I(), n = I(), dt = I();
			static unsigned vIdx[64];
			for(int k=0; k<64; ++k) vIdx[k] = k;
			Reg(vIdx, sizeof(vIdx), "di");
			gVb[i]->DrawIndices(vIdx, n, (eVertexBufferDrawType)dt);
		}
		else if(op == "vb_transform")
		{
			int i = I(); RegVb(i, gVb[i]);
			cMatrixf m(cMatrixf::Identity);
			for(int k=0; k<16; ++k) m.v[k] = 1.0f + 0.25f * k;
			m.v[12] = m.v[13] = m.v[14] = 0; m.v[15] = 1;
			gVb[i]->Transform(m);
		}
		else if(op == "vb_shadow") { int i = I(); RegVb(i, gVb[i]); gVb[i]->CreateShadowDouble(I() != 0); gTrace += "vtx=" + std::to_string(gVb[i]->GetVertexNum()) + "\n"; }
		else if(op == "vb_copy")
		{
			int i = I(), j = I(), type = I(), usage = I(); unsigned fl = (unsigned)I();
			gVb[j] = gVb[i]->CreateCopy((eVertexBufferType)type, (eVertexBufferUsageType)usage, fl);
			gTrace += "vtx=" + std::to_string(gVb[j]->GetVertexNum()) + " idx=" + std::to_string(gVb[j]->GetIndexNum()) + "\n";
		}
		else if(op == "vb_bv") { int i = I(); RegVb(i, gVb[i]); cBoundingVolume bv = gVb[i]->CreateBoundingVolume(); gTrace += "bv=" + A(bv.GetRadius()) + "\n"; }
		else if(op == "vb_free") { int i = I(); delete gVb[i]; gVb.erase(i); }

		else if(op == "sh_new") { int i = I(), t = I(); gSh[i] = g->CreateGpuShader("sh" + std::to_string(i), (eGpuShaderType)t); }
		else if(op == "sh_src")
		{
			int i = I(), seed = I(), lines = I(), quiet = I();
			std::string src = Shader(seed, lines);
			bool b = gSh[i]->CreateFromString(src.c_str(), "main", quiet == 0);
			gTrace += "ret=" + std::to_string(b) + "\n";
		}
		else if(op == "sh_debug") { iGpuShader::SetLogDebugInformation(I() != 0); }
		else if(op == "sh_sampler") { int i = I(); std::string n = S(); gSh[i]->AddSamplerUnit(n, I()); }
		else if(op == "sh_free") { int i = I(); delete gSh[i]; gSh.erase(i); }
		else if(op == "prog_new") { int i = I(); gProg[i] = g->CreateGpuProgram("p" + std::to_string(i)); }
		else if(op == "prog_shader") { int i = I(), t = I(), s = I(); gProg[i]->SetShader((eGpuShaderType)t, s >= 0 ? gSh[s] : NULL); }
		else if(op == "prog_link") { int i = I(); gTrace += "ret=" + std::to_string(gProg[i]->Link()) + "\n"; }
		else if(op == "prog_bind") { gProg[I()]->Bind(); }
		else if(op == "prog_unbind") { gProg[I()]->UnBind(); }
		else if(op == "prog_var") { int i = I(), slot = I(); std::string n = S(); int id = gProg[i]->GetVariableId(n); gVar[std::make_pair(i, slot)] = id; gTrace += "id=" + std::to_string(id) + "\n"; }
		else if(op == "prog_asid") { int i = I(), slot = I(); std::string n = S(); bool b = gProg[i]->GetVariableAsId(n, gVar.count(std::make_pair(i, slot)) ? gVar[std::make_pair(i, slot)] : slot); gTrace += "ret=" + std::to_string(b) + "\n"; }
		else if(op == "prog_set")
		{
			int i = I(), slot = I(); std::string k = S();
			int id = gVar.count(std::make_pair(i, slot)) ? gVar[std::make_pair(i, slot)] : -1;
			float a = F(), b = F(), c = F(), d = F();
			iGpuProgram *p = gProg[i];
			bool r = false;
			if(k == "i") r = p->SetInt(id, (int)a);
			else if(k == "f") r = p->SetFloat(id, a);
			else if(k == "v2") r = p->SetVec2f(id, a, b);
			else if(k == "v3") r = p->SetVec3f(id, a, b, c);
			else if(k == "v4") r = p->SetVec4f(id, a, b, c, d);
			else if(k == "arr") { float v[8] = {a, b, c, d, 5, 6, 7, 8}; r = p->SetFloatArray(id, v, 4); }
			else if(k == "mtx") { cMatrixf m(cMatrixf::Identity); m.v[1] = a; r = p->SetMatrixf(id, m); }
			gTrace += "ret=" + std::to_string(r) + "\n";
		}
		else if(op == "prog_sampler") { int i = I(); std::string n = S(); gTrace += "ret=" + std::to_string(gProg[i]->SetSamplerToUnit(n, I())) + "\n"; }
		else if(op == "prog_dump") { gTrace += "dump=" + gProg[I()]->DumpUniforms(); }
		else if(op == "prog_free") { int i = I(); delete gProg[i]; gProg.erase(i); }
		else { fprintf(stderr, "unknown op %s\n", op.c_str()); return 3; }
	}

	gTrace += "== teardown\n";
	for(auto &p : gProg) delete p.second;
	for(auto &p : gSh) delete p.second;
	for(auto &p : gVb) delete p.second;
	for(auto &p : gFb) delete p.second;
	for(auto &p : gDsb) delete p.second;
	for(auto &p : gTex) delete p.second;
	gFfiOn = false;

	FILE *f = fopen(sOut, "w");
	if(!f) return 1;
	fwrite(gTrace.data(), 1, gTrace.size(), f);
	fclose(f);
	if(const char *sFfi = getenv("L3_FFI_OUT"))
	{
		FILE *ff = fopen(sFfi, "w");
		if(!ff) return 1;
		fwrite(gFfi.data(), 1, gFfi.size(), ff);
		fclose(ff);
	}
	return 0;
}
