// Drives cLowLevelGraphicsSDL through a fixed script against fake GL and SDL entry points and
// writes the call trace. Run with OHPL_IMPL=cpp and OHPL_IMPL=rust and diff the files.
#define private public
#include "impl/LowLevelGraphicsSDL.h"
#undef private
#include "impl/OcclusionQueryOGL.h"
#include "impl/RustBackends.h"
#include "graphics/Bitmap.h"
#include "graphics/FrameBuffer.h"
#include "graphics/Texture.h"
#include "fakegl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hpl { tString GlewCapsLine(); }
extern std::string gFfi;
using namespace hpl;

static void Mark(const char *s) { gTrace += "== "; gTrace += s; gTrace += "\n"; }

int hplMain(const tString&)
{
	const char *sOut = getenv("L2_TRACE_OUT");
	if(!sOut) return 2;
	cLowLevelGraphicsSDL *g = new cLowLevelGraphicsSDL();

	Mark("init");
	g->Init(640, 480, 0, 32, 1, 4, eGpuProgramFormat_GLSL, "cap", cVector2l(0, 0));
	{
		char vCaps[1024];
		if(g->mpRs) ohpl_glctx_caps_line(g->mpRs, vCaps, sizeof(vCaps));
		else snprintf(vCaps, sizeof(vCaps), "%s", GlewCapsLine().c_str());
		gTrace += std::string("CAPS ") + vCaps + "\n";
	}
	AddRange(g->mpVertexArray, sizeof(float) * g->mlBatchStride * g->mlBatchArraySize, "vtx");
	AddRange(g->mpTexCoordArray, sizeof(float*) * kMaxTextureUnits, "tcs");
	AddRange(g->mpIndexArray, sizeof(unsigned) * g->mlBatchArraySize, "idx");
	for(int i = 0; i < kMaxTextureUnits; ++i)
	{
		static char sLabel[kMaxTextureUnits][8];
		snprintf(sLabel[i], sizeof(sLabel[i]), "tc%d", i);
		AddRange(g->mpTexCoordArray[i], sizeof(float) * 3 * g->mlBatchArraySize, sLabel[i]);
	}

	Mark("caps");
	for(int i = 0; i < eGraphicCaps_LastEnum; ++i)
		gTrace += "cap" + std::to_string(i) + "=" + std::to_string(g->GetCaps((eGraphicCaps)i)) + "\n";
	gTrace += std::string("desc=") + g->GetRendererDescription() + "\n";

	Mark("clear");
	g->SetClearColor(cColor(0.1f, 0.2f, 0.3f, 0.4f));
	g->SetClearDepth(0.5f);
	g->SetClearStencil(3);
	g->ClearFrameBuffer(7);
	g->ClearFrameBuffer(eClearFrameBufferFlag_Color);
	g->ClearFrameBuffer(eClearFrameBufferFlag_Depth | eClearFrameBufferFlag_Stencil);

	Mark("state");
	g->SetColorWriteActive(true, false, true, true);
	g->SetColorWriteActive(true, false, true, true);
	g->SetDepthWriteActive(false);
	g->SetDepthWriteActive(false);
	g->SetDepthTestActive(false);
	g->SetDepthTestActive(true);
	g->SetDepthTestFunc(eDepthTestFunc_Less);
	g->SetDepthTestFunc(eDepthTestFunc_Less);
	g->SetAlphaTestActive(true);
	g->SetAlphaTestFunc(eAlphaTestFunc_Greater, 0.25f);
	g->SetAlphaTestFunc(eAlphaTestFunc_Greater, 0.25f);
	g->SetAlphaTestFunc(eAlphaTestFunc_Greater, 0.5f);
	g->SetStencilActive(true);
	g->SetStencilWriteMask(0xff);
	g->SetStencil(eStencilFunc_Always, 1, 0xf, eStencilOp_Keep, eStencilOp_Zero, eStencilOp_Replace);
	g->SetStencilActive(true);
	g->SetStencilTwoSide(eStencilFunc_Less, eStencilFunc_Greater, 2, 0xe, eStencilOp_Keep, eStencilOp_IncrementWrap,
						 eStencilOp_Invert, eStencilOp_DecrementWrap, eStencilOp_Zero, eStencilOp_Increment);
	g->SetStencilActive(true);
	g->SetStencilActive(false);
	g->SetCullActive(false);
	g->SetCullActive(true);
	g->SetCullMode(eCullMode_Clockwise);
	g->SetCullMode(eCullMode_CounterClockwise);
	g->SetScissorActive(true);
	g->SetScissorActive(true);
	g->SetScissorRect(cVector2l(10, 20), cVector2l(30, 40));
	g->SetClipPlane(1, cPlanef(1.0f, 2.0f, 3.0f, 4.0f));
	gTrace += "plane=" + A(g->GetClipPlane(1).a) + "\n";
	g->SetClipPlaneActive(1, true);
	g->SetClipPlaneActive(1, false);
	g->SetBlendActive(true);
	g->SetBlendActive(true);
	g->SetBlendFunc(eBlendFunc_SrcAlpha, eBlendFunc_OneMinusSrcAlpha);
	g->SetBlendFuncSeparate(eBlendFunc_One, eBlendFunc_Zero, eBlendFunc_SrcAlpha, eBlendFunc_OneMinusSrcAlpha);
	g->SetDepthClampActive(true);
	g->SetDepthClampActive(false);
	g->SetPolygonOffsetActive(true);
	g->SetPolygonOffset(1.5f, 2.5f);
	g->SetPolygonOffsetActive(false);

	Mark("matrix");
	cMatrixf mtx(cMatrixf::Identity);
	for(int i = 0; i < 16; ++i) mtx.v[i] = 1.0f + 0.5f * i;
	g->PushMatrix(eMatrix_ModelView);
	g->SetMatrix(eMatrix_Projection, mtx);
	g->SetIdentityMatrix(eMatrix_Texture);
	g->PopMatrix(eMatrix_ModelView);
	g->SetOrthoProjection(cVector2f(800, 600), -1, 1);
	g->SetOrthoProjection(cVector3f(-1, -2, -3), cVector3f(4, 5, 6));

	Mark("texture");
	iTexture *tex = g->CreateTexture("t", eTextureType_2D, eTextureUsage_Normal);
	{
		cBitmap bmp;
		bmp.CreateData(cVector3l(4, 4, 1), ePixelFormat_RGBA, 0, 0);
		tex->CreateFromBitmap(&bmp);
	}
	gFfi += "@gen " + std::to_string(gGen) + "\n";
	Mark("settex");
	g->SetTexture(0, tex);
	g->SetTexture(0, tex);
	g->SetTexture(1, tex);
	g->SetTexture(0, NULL);
	g->SetTexture(9, tex);
	g->SetActiveTextureUnit(2);
	g->SetActiveTextureUnit(2);
	g->SetActiveTextureUnit(3);
	g->SetTextureEnv(eTextureParam_ColorFunc, eTextureFunc_Modulate);
	g->SetTextureEnv(eTextureParam_AlphaSource1, eTextureSource_Texture);
	g->SetTextureEnv(eTextureParam_ColorOp2, eTextureOp_OneMinusAlpha);
	g->SetTextureEnv(eTextureParam_ColorScale, 2);
	g->SetTextureConstantColor(cColor(0.5f, 0.25f, 0.75f, 1.0f));
	g->SetColor(cColor(1, 0.5f, 0.25f, 0.125f));

	Mark("framebuffer");
	iFrameBuffer *fb = g->CreateFrameBuffer("fb");
	gFfi += "@gen " + std::to_string(gGen) + "\n";
	g->SetCurrentFrameBuffer(fb, cVector2l(1, 2), cVector2l(100, 50));
	int vTargets[3] = {0, 1, 2};
	g->SetFrameBufferDrawTargets(vTargets, 3);
	g->SetFrameBufferDrawTargets(vTargets, 0);
	g->SetScissorRect(cVector2l(1, 2), cVector2l(3, 4));
	g->CopyFrameBufferToTexure(tex, cVector2l(2, 3), cVector2l(8, 8), cVector2l(1, 1));
	g->CopyFrameBufferToTexure(tex, cVector2l(0, 0), cVector2l(0, 0), cVector2l(0, 0));
	cBitmap *pBmp = g->CopyFrameBufferToBitmap(cVector2l(1, 1), cVector2l(4, 4));
	delete pBmp;
	g->SetCurrentFrameBuffer(NULL);
	g->SetCurrentFrameBuffer(NULL, cVector2l(5, 6), cVector2l(70, 60));
	g->SetScissorRect(cVector2l(1, 2), cVector2l(3, 4));
	g->WaitAndFinishRendering();
	g->FlushRendering();

	Mark("immediate");
	{
		tVertexVec vVtx;
		for(int i = 0; i < 4; ++i)
			vVtx.push_back(cVertex(cVector3f(i, i + 1, i + 2), cVector3f(0.25f * i, 0.5f, 0.75f), cColor(0.1f * i, 0.2f, 0.3f, 0.4f)));
		tVertexVec vTri(vVtx.begin(), vVtx.begin() + 3);
		g->DrawTriangle(vTri);
		g->DrawQuad(cVector3f(1, 2, 3), cVector2f(4, 5), cColor(1, 0.5f));
		g->DrawQuad(cVector3f(1, 2, 3), cVector2f(4, 5), cVector2f(0.1f, 0.2f), cVector2f(0.9f, 0.8f), cColor(1, 0.5f));
		g->DrawQuad(cVector3f(1, 2, 3), cVector2f(4, 5), cVector2f(0, 0), cVector2f(1, 1), cVector2f(0.2f, 0.2f), cVector2f(0.7f, 0.7f), cColor(1, 0.5f));
		g->DrawQuad(vVtx);
		g->DrawQuad(vVtx, cColor(0.3f, 0.6f));
		g->DrawQuad(vVtx, 2.5f);
		g->DrawQuad(vVtx, 2.5f, cColor(0.3f, 0.6f));
		tVector3fVec vExtra;
		for(int i = 0; i < 4; ++i) vExtra.push_back(cVector3f(0.1f * i, 0.2f, 0.3f));
		g->DrawQuadMultiTex(vVtx, vExtra);
		g->DrawLine(cVector3f(0, 0, 0), cVector3f(1, 1, 1), cColor(1, 0, 0, 1));
		g->DrawLine(cVector3f(0, 0, 0), cColor(1, 0, 0, 1), cVector3f(1, 1, 1), cColor(0, 1, 0, 1));
		g->DrawBoxMinMax(cVector3f(-1, -1, -1), cVector3f(1, 1, 1), cColor(0, 0, 1, 1));
		g->DrawSphere(cVector3f(1, 2, 3), 2.0f, cColor(1, 1, 0, 1));
		g->DrawSphere(cVector3f(1, 2, 3), 2.0f, cColor(1, 0, 0, 1), cColor(0, 1, 0, 1), cColor(0, 0, 1, 1));
		g->DrawLineQuad(cRect2f(1, 2, 3, 4), 0.5f, cColor(1, 1, 1, 1));
		g->DrawLineQuad(cVector3f(1, 2, 3), cVector2f(4, 5), cColor(1, 1, 1, 1));
	}

	Mark("batch");
	{
		for(int i = 0; i < 4; ++i)
		{
			cVertex v(cVector3f(i, 2.0f * i, 0.5f), cVector3f(0.25f * i, 0.5f, 0.75f), cColor(0.1f * i, 0.2f, 0.3f, 0.4f));
			g->AddVertexToBatch(&v);
		}
		for(int i : {0, 1, 2, 0, 2, 3}) g->AddIndexToBatch(i);
		cVector3f vTc(0.5f, 0.25f, 0.125f);
		for(int i = 0; i < 4; ++i) g->AddTexCoordToBatch(1, &vTc);
		g->SetBatchTextureUnitActive(0, true);
		g->SetBatchTextureUnitActive(1, true);
		g->SetBatchTextureUnitActive(1, false);
		g->FlushTriBatch(eVtxBatchFlag_Position | eVtxBatchFlag_Color0 | eVtxBatchFlag_Texture0, true);
		for(int i = 0; i < 4; ++i)
		{
			cVertex v(cVector3f(i, 2.0f * i, 0.5f), cVector3f(0.25f * i, 0.5f, 0.75f), cColor(0.1f * i, 0.2f, 0.3f, 0.4f));
			g->AddVertexToBatch(&v);
		}
		for(int i : {0, 1, 2, 3}) g->AddIndexToBatch(i);
		g->FlushQuadBatch(eVtxBatchFlag_Position | eVtxBatchFlag_Normal | eVtxBatchFlag_Texture1 | eVtxBatchFlag_Texture2, false);
		g->ClearBatch();
	}

	Mark("query");
	{
		iOcclusionQuery *q = g->CreateOcclusionQuery();
		q->Begin();
		q->End();
		bool b1 = q->FetchResults();
		bool b2 = q->FetchResults();
		gTrace += "fetch=" + std::to_string(b1) + std::to_string(b2) + " samples=" + std::to_string(q->GetSampleCount()) + "\n";
		delete q;
	}

	Mark("window");
	g->ShowCursor(true);
	g->SetWindowGrab(true);
	g->SetRelativeMouse(true);
	g->SetWindowCaption("renamed");
	gTrace += "focus=" + std::to_string(g->GetWindowMouseFocus()) + std::to_string(g->GetWindowInputFocus()) + std::to_string(g->GetWindowIsVisible()) + "\n";
	g->SetVsyncActive(true, false);
	g->SetVsyncActive(true, true);
	g->SetVsyncActive(false, false);
	g->SetMultisamplingActive(true);
	g->SetMultisamplingActive(false);
	g->SetGammaCorrection(2.2f);
	gTrace += "gamma=" + A(g->GetGammaCorrection()) + "\n";
	g->SwapBuffers();
	g->SwapBuffers();
	g->ForceWindowSize(800, 600);
	gTrace += "resized=" + std::to_string(g->CheckAndUpdateScreenSize()) + std::to_string(g->CheckAndUpdateScreenSize()) +
			  " size=" + std::to_string(g->GetScreenSizeInt().x) + "x" + std::to_string(g->GetScreenSizeInt().y) + "\n";
	g->SwapBuffers();
	g->SetGammaCorrection(1.0f);
	g->SwapBuffers();

	Mark("teardown");
	delete tex;
	delete fb;
	delete g;

	FILE *f = fopen(sOut, "w");
	if(!f) return 1;
	fwrite(gTrace.data(), 1, gTrace.size(), f);
	fclose(f);
	if(const char *sFfi = getenv("L2_FFI_OUT"))
	{
		FILE *ff = fopen(sFfi, "w");
		if(!ff) return 1;
		fwrite(gFfi.data(), 1, gFfi.size(), ff);
		fclose(ff);
	}
	return 0;
}
