/*
 * Tests for the GPU trace recorder (HPL2/tests/trace): every iLowLevelGraphics and resource
 * virtual is driven, the trace must be byte-identical across runs and carry no pointers, and the
 * real iRenderFunctions code runs against the recorder without a window.
 */

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "graphics/RenderFunctions.h"
#include "math/Frustum.h"
#include "trace/RecordingLowLevelGraphics.h"

using namespace hpl;

static int gFailures = 0;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++gFailures; \
		} \
	} while (0)

static cMatrixf TestMatrix(float s)
{
	cMatrixf m(cMatrixf::Identity);
	for(int i = 0; i < 16; ++i) m.v[i] = s + 0.25f * i;
	return m;
}

static void Exercise(iLowLevelGraphics* g, std::vector<const void*>& aAddrs)
{
	g->Init(640, 480, 0, 32, 0, 4, eGpuProgramFormat_GLSL, "cap", cVector2l(1, 2));
	g->GetGpuProgramFormat();
	CHECK(g->GetCaps(eGraphicCaps_MaxTextureImageUnits) == 16);
	g->GetRendererDescription();
	CHECK(g->GetWindowHandle() == NULL);
	g->SetColorOutput("hdr10", 203.0f, 1000.0f);
	g->GetColorOutput();
	g->GetHdrHeadroom();
	float a, b, c;
	g->GetScreenFloatStats(a, b, c);
	g->ShowCursor(true);
	g->SetWindowGrab(true);
	g->SetRelativeMouse(true);
	g->SetWindowCaption("x");
	g->GetWindowMouseFocus();
	g->GetWindowInputFocus();
	g->GetWindowIsVisible();
	g->GetMultisampling();
	CHECK(g->GetScreenSizeFloat().x == 640.0f);
	CHECK(g->GetScreenSizeInt().y == 480);
	g->CheckAndUpdateScreenSize();
	g->ForceWindowSize(800, 600);
	g->GetFullscreenModeActive();
	g->SetVsyncActive(true, true);
	g->SetMultisamplingActive(true);
	g->SetGammaCorrection(2.2f);
	CHECK(g->GetGammaCorrection() == 2.2f);

	iFontData* font = g->CreateFontData("font");
	font->CreateFromFontFile(_W("f.ttf"), 12, 32, 127);
	font->CreateFromBitmapFile(_W("f.fnt"));
	iGpuProgram* prog = g->CreateGpuProgram("prog");
	iGpuShader* vs = g->CreateGpuShader("vs", eGpuShaderType_Vertex);
	iGpuShader* fs = g->CreateGpuShader("fs", eGpuShaderType_Fragment);
	CHECK(vs->SamplerNeedsTextureUnitSetup() == false);
	vs->CreateFromFile(_W("a.vert"), "main", true);
	fs->CreateFromString("void main(){}", "main", true);
	prog->SetShader(eGpuShaderType_Vertex, vs);
	prog->SetShader(eGpuShaderType_Fragment, fs);
	prog->SetAutoDestroyShaders(false);
	prog->Link();
	prog->DumpUniforms();
	prog->Bind();
	CHECK(prog->CanAccessAPIMatrix());
	prog->SetSamplerToUnit("s0", 0);
	int id = prog->GetVariableId("u_color");
	CHECK(prog->GetVariableId("u_color") == id);
	CHECK(prog->GetVariableId("u_other") != id);
	prog->GetVariableAsId("u_fixed", 9);
	prog->SetInt(id, 1);
	prog->SetFloat(id, 0.5f);
	prog->SetVec2f(id, 1.f, 2.f);
	prog->SetVec3f(id, 1.f, 2.f, 3.f);
	prog->SetVec4f(id, 1.f, 2.f, 3.f, 4.f);
	float fa[3] = {1.f, 2.f, 3.f};
	prog->SetFloatArray(id, fa, 3);
	prog->SetMatrixf(id, TestMatrix(1));
	prog->SetMatrixf(id, eGpuShaderMatrix_ViewProjection, eGpuShaderMatrixOp_Identity);
	prog->UnBind();

	iTexture* tex = g->CreateTexture("tex", eTextureType_2D, eTextureUsage_Normal);
	cBitmap bmp;
	bmp.CreateData(cVector3l(2, 2, 1), ePixelFormat_RGBA, 0, 0);
	bmp.Clear(cColor(0.25f, 0.5f, 0.75f, 1), 0, 0);
	tex->CreateFromBitmap(&bmp);
	std::vector<cBitmap*> bmps(1, &bmp);
	tex->CreateAnimFromBitmapVec(&bmps);
	tex->CreateCubeFromBitmapVec(&bmps);
	unsigned char raw[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	tex->CreateFromRawData(cVector3l(2, 2, 1), ePixelFormat_RGBA, raw);
	tex->SetRawData(0, cVector3l(0, 0, 0), cVector3l(2, 2, 1), ePixelFormat_RGBA, raw);
	tex->Update(0.016f);
	tex->SetFilter(eTextureFilter_Trilinear);
	tex->SetAnisotropyDegree(4.f);
	tex->SetWrapS(eTextureWrap_Clamp);
	tex->SetWrapT(eTextureWrap_Clamp);
	tex->SetWrapR(eTextureWrap_Clamp);
	tex->SetWrapSTR(eTextureWrap_Repeat);
	tex->SetCompareMode(eTextureCompareMode_RToTexture);
	tex->SetCompareFunc(eTextureCompareFunc_LessOrEqual);
	tex->SetRedFromAlpha(true);
	tex->AutoGenerateMipmaps();
	tex->HasAnimation();
	tex->NextFrame();
	tex->PrevFrame();
	tex->GetT();
	tex->SetTimeCount(1.f);
	tex->GetTimeCount();
	tex->GetCurrentLowlevelHandle();
	std::vector<float> px;
	tex->GetRawPixelsRGBAFloat(px);

	iVertexBuffer* vb = g->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri, eVertexBufferUsageType_Static, 4, 6);
	vb->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 4);
	vb->CreateElementArray(eVertexBufferElement_Color0, eVertexBufferElementFormat_Float, 4);
	vb->CreateElementArray(eVertexBufferElement_Normal, eVertexBufferElementFormat_Float, 3);
	for(int i = 0; i < 4; ++i)
	{
		vb->AddVertexVec4f(eVertexBufferElement_Position, cVector3f((float)i, (float)(i * 2), 0), 1.f);
		vb->AddVertexColor(eVertexBufferElement_Color0, cColor(1, 0.5f, 0.25f, 1));
		vb->AddVertexVec3f(eVertexBufferElement_Normal, cVector3f(0, 0, 1));
	}
	for(unsigned i : {0u, 1u, 2u, 0u, 2u, 3u}) vb->AddIndex(i);
	CHECK(vb->GetVertexNum() == 4);
	CHECK(vb->GetIndexNum() == 6);
	vb->Compile(0);
	vb->UpdateData(eVertexElementFlag_Position, true);
	vb->CreateShadowDouble(true);
	vb->Transform(TestMatrix(2));
	vb->Bind();
	vb->Draw();
	unsigned int idx[3] = {0, 1, 2};
	vb->DrawIndices(idx, 3);
	vb->UnBind();
	iVertexBuffer* vb2 = vb->CreateCopy(eVertexBufferType_Software, eVertexBufferUsageType_Dynamic, eVertexElementFlag_Position);
	CHECK(vb2->GetVertexNum() == 4);
	vb2->CreateBoundingVolume();
	CHECK(vb->GetElementNum(eVertexBufferElement_Position) == 4);
	vb->GetElementFormat(eVertexBufferElement_Position);
	vb->GetElementProgramVarIndex(eVertexBufferElement_Position);
	CHECK(vb->GetFloatArray(eVertexBufferElement_Position) != NULL);
	vb->GetIntArray(eVertexBufferElement_Position);
	vb->GetByteArray(eVertexBufferElement_Position);
	CHECK(vb->GetIndices() != NULL);
	vb->ResizeArray(eVertexBufferElement_Position, 32);
	vb->ResizeIndices(12);

	iDepthStencilBuffer* dsb = g->CreateDepthStencilBuffer(cVector2l(64, 64), 24, 8);
	iFrameBuffer* fb = g->CreateFrameBuffer("fb");
	fb->SetTexture2D(0, tex);
	fb->SetTexture3D(1, tex, 0);
	fb->SetTextureCubeMap(2, tex, 1);
	fb->SetDepthTexture2D(tex);
	fb->SetDepthTextureCubeMap(tex, 2);
	fb->SetDepthStencilBuffer(dsb);
	CHECK(fb->CompileAndValidate());
	fb->PostBindUpdate();
	iOcclusionQuery* oq = g->CreateOcclusionQuery();
	oq->Begin();
	oq->End();
	oq->FetchResults();
	oq->GetSampleCount();

	g->ClearFrameBuffer(eClearFrameBufferFlag_Color | eClearFrameBufferFlag_Depth);
	g->SetClearColor(cColor(0.1f, 0.2f, 0.3f, 0.4f));
	g->SetClearDepth(1.f);
	g->SetClearStencil(0);
	g->FlushRendering();
	g->WaitAndFinishRendering();
	g->SwapBuffers();
	g->CopyFrameBufferToTexure(tex, cVector2l(0, 0), cVector2l(64, 64), cVector2l(1, 1));
	cBitmap* shot = g->CopyFrameBufferToBitmap();
	CHECK(shot != NULL && shot->GetSize().x == 800);
	delete shot;
	g->SetCurrentFrameBuffer(fb, cVector2l(1, 1), cVector2l(32, 32));
	CHECK(g->GetCurrentFrameBuffer() == fb);
	int targets[2] = {0, 1};
	g->SetFrameBufferDrawTargets(targets, 2);

	g->SetColorWriteActive(true, false, true, false);
	g->SetDepthWriteActive(true);
	g->SetCullActive(true);
	g->SetCullMode(eCullMode_Clockwise);
	g->SetDepthTestActive(true);
	g->SetDepthTestFunc(eDepthTestFunc_Less);
	g->SetAlphaTestActive(true);
	g->SetAlphaTestFunc(eAlphaTestFunc_Greater, 0.5f);
	g->SetStencilActive(true);
	g->SetStencilWriteMask(0xff);
	g->SetStencil(eStencilFunc_Always, 1, 0xff, eStencilOp_Keep, eStencilOp_Keep, eStencilOp_Replace);
	g->SetStencilTwoSide(eStencilFunc_Always, eStencilFunc_Never, 1, 0xff, eStencilOp_Keep, eStencilOp_Keep, eStencilOp_Replace,
						 eStencilOp_Keep, eStencilOp_Zero, eStencilOp_Invert);
	g->SetScissorActive(true);
	g->SetScissorRect(cVector2l(1, 2), cVector2l(3, 4));
	g->SetClipPlane(1, cPlanef(0, 1, 0, 2));
	CHECK(g->GetClipPlane(1).d == 2.f);
	g->SetClipPlaneActive(1, true);
	g->SetColor(cColor(1, 0, 0, 1));
	g->SetBlendActive(true);
	g->SetBlendFunc(eBlendFunc_SrcAlpha, eBlendFunc_OneMinusSrcAlpha);
	g->SetBlendFuncSeparate(eBlendFunc_One, eBlendFunc_Zero, eBlendFunc_SrcAlpha, eBlendFunc_DestAlpha);
	g->SetPolygonOffsetActive(true);
	g->SetDepthClampActive(true);
	g->SetPolygonOffset(1.f, 2.f);

	g->PushMatrix(eMatrix_ModelView);
	g->PopMatrix(eMatrix_ModelView);
	g->SetIdentityMatrix(eMatrix_Projection);
	g->SetMatrix(eMatrix_ModelView, TestMatrix(3));
	g->SetOrthoProjection(cVector2f(800, 600), -1, 1);
	g->SetOrthoProjection(cVector3f(0, 0, -1), cVector3f(800, 600, 1));

	g->SetTexture(0, tex);
	g->SetActiveTextureUnit(1);
	g->SetTextureEnv(eTextureParam_ColorFunc, 1);
	g->SetTextureConstantColor(cColor(0.5f, 0.5f, 0.5f, 0.5f));

	tVertexVec quad(4, cVertex(cVector3f(1, 2, 3), cVector3f(0.5f, 0.5f, 0), cColor(1, 1)));
	g->DrawTriangle(quad);
	g->DrawQuad(cVector3f(0, 0, 0), cVector2f(8, 8));
	g->DrawQuad(cVector3f(0, 0, 0), cVector2f(8, 8), cVector2f(0, 0), cVector2f(1, 1));
	g->DrawQuad(cVector3f(0, 0, 0), cVector2f(8, 8), cVector2f(0, 0), cVector2f(1, 1), cVector2f(0, 0), cVector2f(1, 1));
	g->DrawQuad(quad);
	g->DrawQuad(quad, cColor(1, 0, 0, 1));
	g->DrawQuad(quad, 0.5f);
	g->DrawQuad(quad, 0.5f, cColor(0, 1, 0, 1));
	g->DrawQuadMultiTex(quad, tVector3fVec(2, cVector3f(0.1f, 0.2f, 0.3f)));
	g->DrawLine(cVector3f(0, 0, 0), cVector3f(1, 1, 1), cColor(1, 1));
	g->DrawLine(cVector3f(0, 0, 0), cColor(1, 0, 0, 1), cVector3f(1, 1, 1), cColor(0, 0, 1, 1));
	g->DrawBoxMinMax(cVector3f(0, 0, 0), cVector3f(1, 1, 1), cColor(1, 1));
	g->DrawSphere(cVector3f(0, 0, 0), 2.f, cColor(1, 1));
	g->DrawSphere(cVector3f(0, 0, 0), 2.f, cColor(1, 0, 0, 1), cColor(0, 1, 0, 1), cColor(0, 0, 1, 1));
	g->DrawLineQuad(cRect2f(1, 2, 3, 4), 0.f, cColor(1, 1));
	g->DrawLineQuad(cVector3f(0, 0, 0), cVector2f(4, 4), cColor(1, 1));

	cVertex v(cVector3f(1, 2, 3), cColor(1, 1));
	cVector3f tr(4, 5, 6);
	cMatrixf mtx = TestMatrix(4);
	cColor col(0.5f, 0.5f, 0.5f, 1);
	float w = 3.f, h = 4.f;
	g->AddVertexToBatch(&v);
	g->AddVertexToBatch(&v, &tr);
	g->AddVertexToBatch(&v, &mtx);
	g->AddVertexToBatch_Size2D(&v, &tr, &col, w, h);
	g->AddVertexToBatch_Raw(cVector3f(1, 2, 3), col, cVector3f(0, 1, 0));
	g->AddIndexToBatch(3);
	g->AddTexCoordToBatch(1, &tr);
	g->SetBatchTextureUnitActive(1, true);
	g->FlushTriBatch(eVtxBatchFlag_Position, true);
	g->FlushQuadBatch(eVtxBatchFlag_Position | eVtxBatchFlag_Color0, false);
	g->ClearBatch();

	g->SetCurrentFrameBuffer(NULL);

	aAddrs = {font, prog, vs, fs, tex, vb, vb2, dsb, fb, oq};
	delete oq;
	delete fb;
	delete dsb;
	delete vb2;
	delete vb;
	delete tex;
	delete prog;
	delete vs;
	delete fs;
	delete font;
}

class cTestRenderFunctions : public iRenderFunctions
{
public:
	explicit cTestRenderFunctions(iLowLevelGraphics* apLlg)
	{
		mpGraphics = NULL;
		SetupRenderFunctions(apLlg);
	}
};

static void RunRenderFunctions(iLowLevelGraphics* g, std::vector<const void*>& aAddrs)
{
	cTestRenderFunctions rf(g);
	cRenderTarget rt;
	cFrustum frustum;
	frustum.SetupPerspectiveProj(TestMatrix(0.5f), TestMatrix(1.5f), 100.f, 0.1f, 1.2f, 1.7f, cVector3f(1, 2, 3));
	rf.InitAndResetRenderFunctions(&frustum, &rt, false);

	iGpuProgram* prog = g->CreateGpuProgram("rf_prog");
	iTexture* tex = g->CreateTexture("rf_tex", eTextureType_2D, eTextureUsage_RenderTarget);
	iVertexBuffer* vb = g->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri, eVertexBufferUsageType_Static);
	vb->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 3);
	for(int i = 0; i < 3; ++i) vb->AddVertexVec3f(eVertexBufferElement_Position, cVector3f((float)i, 1, 2));
	for(unsigned i : {0u, 1u, 2u}) vb->AddIndex(i);
	iFrameBuffer* fb = g->CreateFrameBuffer("rf_fb");
	fb->SetTexture2D(0, tex);

	cMatrixf proj = TestMatrix(1);
	rf.SetProjectionMatrix(&proj);
	rf.SetFlatProjection(cVector2f(800, 600), -10, 10);
	rf.SetDepthTest(false);
	rf.SetDepthTest(false);
	rf.SetDepthWrite(false);
	rf.SetDepthTestFunc(eDepthTestFunc_Greater);
	rf.SetCullActive(false);
	rf.SetCullMode(eCullMode_Clockwise);
	rf.SetInvertCullMode(true);
	rf.SetCullMode(eCullMode_CounterClockwise);
	rf.SetStencilActive(true);
	rf.SetStencilWriteMask(0x0f);
	rf.SetStencil(eStencilFunc_Equal, 1, 0xff, eStencilOp_Keep, eStencilOp_Keep, eStencilOp_Increment);
	rf.SetScissorRect(cVector2l(1, 1), cVector2l(10, 10), true);
	rf.SetChannelMode(eMaterialChannelMode_RGB);
	rf.SetAlphaMode(eMaterialAlphaMode_Trans);
	rf.SetAlphaLimit(0.25f);
	rf.SetBlendMode(eMaterialBlendMode_Add);
	rf.SetProgram(prog);
	rf.SetTexture(0, tex);
	rf.SetTexture(0, tex);
	rf.SetVertexBuffer(vb);
	cMatrixf mdl = TestMatrix(5);
	rf.SetMatrix(&mdl);
	rf.DrawCurrent();
	rf.SetFrameBuffer(fb);
	rf.ClearFrameBuffer(eClearFrameBufferFlag_Color, false);
	rf.DrawQuad(cVector3f(0, 0, 0), cVector2f(1, 1));
	rf.DrawQuad(cVector3f(0, 0, 0), cVector2f(1, 1), cVector2f(0, 0), cVector2f(1, 1), true, cColor(1, 0, 0, 1));
	rf.DrawWireFrame(vb, cColor(1, 1, 1, 1));
	rf.CopyFrameBufferToTexure(tex, cVector2l(0, 0), cVector2l(64, 64), cVector2l(0, 0), false);
	rf.SetFrameBuffer(NULL);
	rf.ExitAndCleanUpRenderFunctions();

	aAddrs.push_back(prog); aAddrs.push_back(tex); aAddrs.push_back(vb); aAddrs.push_back(fb);
	delete fb;
	delete vb;
	delete tex;
	delete prog;
}

static std::string Run(const cRecordingConfig& aCfg, std::vector<const void*>& aAddrs)
{
	cRecordingLowLevelGraphics llg(aCfg);
	Exercise(&llg, aAddrs);
	RunRenderFunctions(&llg, aAddrs);
	return llg.Trace().Text();
}

static bool HasPointer(const std::string& asTrace, const std::vector<const void*>& aAddrs)
{
	for(const void* p : aAddrs)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%llx", (unsigned long long)(uintptr_t)p);
		if(asTrace.find(buf) != std::string::npos) return true;
	}
	return asTrace.find("0x") != std::string::npos;
}

static size_t Count(const std::string& s, const char* sub)
{
	size_t n = 0, pos = 0;
	while((pos = s.find(sub, pos)) != std::string::npos) { ++n; pos += strlen(sub); }
	return n;
}

int hplMain(const tString&)
{
	cRecordingConfig cfg;
	std::vector<const void*> addrsA, addrsB;
	std::string a = Run(cfg, addrsA);
	std::string b = Run(cfg, addrsB);

	if(const char* dump = getenv("TRACE_RECORDER_DUMP"))
		if(FILE* f = fopen(dump, "wb")) { fwrite(a.data(), 1, a.size(), f); fclose(f); }

	CHECK(a.size() > 4000);
	CHECK(a == b);
	CHECK(!HasPointer(a, addrsA));
	CHECK(!HasPointer(a, addrsB));
	CHECK(a.find("tex#0.SetFilter(") != std::string::npos);
	CHECK(a.find("SetCurrentFrameBuffer(fb#") != std::string::npos);
	CHECK(a.find("oq#0.FetchResults(") != std::string::npos);
	CHECK(Count(a, "DrawQuad(") == 9);
	CHECK(a.find("DrawQuad(v3{f:00000000,f:00000000,f:00000000}, v2{f:41000000,f:41000000}, c{") != std::string::npos);
	CHECK(a.find("prog#1.~") != std::string::npos);
	if(a != b)
	{
		size_t i = 0;
		while(i < a.size() && i < b.size() && a[i] == b[i]) ++i;
		fprintf(stderr, "first diff at %zu: '%.60s' vs '%.60s'\n", i, a.c_str() + i, b.c_str() + i);
	}

	cRecordingConfig cfg2 = cfg;
	cfg2.mlSeed = 777;
	std::vector<const void*> addrsC;
	CHECK(Run(cfg2, addrsC) != a);

	if(gFailures == 0) printf("TraceRecorderTests: OK (%zu bytes of trace)\n", a.size());
	return gFailures == 0 ? 0 : 1;
}
