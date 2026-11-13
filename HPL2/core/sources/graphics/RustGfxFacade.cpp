#include "graphics/RustGraphicsH5b.h"

#include <cstring>
#include <string>
#include <vector>

#include "graphics/Bitmap.h"
#include "graphics/FontData.h"
#include "graphics/FrameBuffer.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/OcclusionQuery.h"
#include "graphics/Texture.h"
#include "graphics/VertexBuffer.h"
#include "math/BoundingVolume.h"

namespace hpl {

	namespace {
		const uint64_t kNone = 0xFFFFFFFFull;

		struct cLevel { const unsigned char *mpData; size_t mlLen; };
		struct cBmpIn { int mvSize[3]; int mlFormat; unsigned mlImages; unsigned mlMips; const cLevel *mpLevels; };

		class cRead
		{
		public:
			cRead(const uint64_t *apA, size_t alN) : mpA(apA), mlN(alN), mlI(0) {}
			uint64_t U() { return mlI < mlN ? mpA[mlI++] : 0; }
			int I() { return (int)(int64_t)U(); }
			unsigned Un() { return (unsigned)U(); }
			float F() { uint32_t u = (uint32_t)U(); float f; memcpy(&f, &u, 4); return f; }
			bool B() { return (U() & 1) != 0; }
			template<class T> T* P() { return reinterpret_cast<T*>((uintptr_t)U()); }
			std::string S() { const char *p = P<const char>(); size_t n = (size_t)U(); return std::string(p, n); }
			tWString W()
			{
				std::string s = S();
				tWString w;
				for(size_t i = 0; i < s.size();)
				{
					unsigned char c = s[i];
					unsigned cp; int len;
					if(c < 0x80) { cp = c; len = 1; }
					else if(c < 0xE0) { cp = c & 0x1F; len = 2; }
					else if(c < 0xF0) { cp = c & 0x0F; len = 3; }
					else { cp = c & 0x07; len = 4; }
					for(int k = 1; k < len && i + k < s.size(); ++k) cp = (cp << 6) | (s[i + k] & 0x3F);
					w.push_back((wchar_t)cp);
					i += len;
				}
				return w;
			}
			cVector2f V2() { float x = F(); float y = F(); return cVector2f(x, y); }
			cVector3f V3() { float x = F(); float y = F(); float z = F(); return cVector3f(x, y, z); }
			cColor C() { float r = F(); float g = F(); float b = F(); float a = F(); return cColor(r, g, b, a); }
			cVector2l V2i() { int x = I(); int y = I(); return cVector2l(x, y); }
			cVector3l V3i() { int x = I(); int y = I(); int z = I(); return cVector3l(x, y, z); }
			cMatrixf M() { cMatrixf m; memcpy(m.v, P<const float>(), sizeof(float) * 16); return m; }
			cVertex Vtx(const float *p)
			{
				cVertex v;
				v.pos = cVector3f(p[0], p[1], p[2]);
				v.tex = cVector3f(p[3], p[4], p[5]);
				v.tan = cVector3f(p[6], p[7], p[8]);
				v.norm = cVector3f(p[9], p[10], p[11]);
				v.col = cColor(p[12], p[13], p[14], p[15]);
				return v;
			}
			cVertex V1() { return Vtx(P<const float>()); }
			tVertexVec Vs()
			{
				const float *p = P<const float>(); size_t n = (size_t)U();
				tVertexVec v;
				for(size_t i = 0; i < n; ++i) v.push_back(Vtx(p + i * 16));
				return v;
			}
		private:
			const uint64_t *mpA; size_t mlN, mlI;
		};

		uint64_t FBits(float afX) { uint32_t u; memcpy(&u, &afX, 4); return u; }
	}

	struct cRustGfxFacade::cImpl
	{
		iLowLevelGraphics *mpLlg;
		std::vector<iFontData*> mFont;
		std::vector<iGpuProgram*> mProg;
		std::vector<iGpuShader*> mShader;
		std::vector<iTexture*> mTex;
		std::vector<iVertexBuffer*> mVb;
		std::vector<iFrameBuffer*> mFb;
		std::vector<iDepthStencilBuffer*> mDsb;
		std::vector<iOcclusionQuery*> mOq;
		std::string mStr;
		std::vector<unsigned char> mBytes;
		std::vector<float> mFloats;

		template<class T> static T* Get(std::vector<T*> &avV, uint64_t alId) { return alId < avV.size() ? avV[alId] : NULL; }
		template<class T> static uint64_t Find(std::vector<T*> &avV, T *apObj)
		{
			if(apObj == NULL) return kNone;
			for(size_t i = 0; i < avV.size(); ++i) if(avV[i] == apObj) return i;
			return kNone;
		}
		template<class T> static uint64_t Add(std::vector<T*> &avV, T *apObj) { avV.push_back(apObj); return avV.size() - 1; }

		iTexture* Tex(cRead &r) { return Get(mTex, r.U()); }
		iTexture* TexO(cRead &r) { uint64_t id = r.U(); return id == kNone ? NULL : Get(mTex, id); }
		iVertexBuffer* Vb(cRead &r) { return Get(mVb, r.U()); }
		iGpuProgram* Prog(cRead &r) { return Get(mProg, r.U()); }
		iFrameBuffer* Fb(cRead &r) { return Get(mFb, r.U()); }

		void Bitmaps(cRead &r, std::vector<cBitmap*> &avOut)
		{
			const cBmpIn *p = r.P<const cBmpIn>(); unsigned n = r.Un();
			for(unsigned b = 0; b < n; ++b)
			{
				const cBmpIn &in = p[b];
				cBitmap *bmp = new cBitmap();
				cVector3l size(in.mvSize[0], in.mvSize[1], in.mvSize[2]);
				bmp->SetUpData((int)in.mlImages, (int)in.mlMips);
				for(unsigned i = 0; i < in.mlImages; ++i)
					for(unsigned m = 0; m < in.mlMips; ++m)
					{
						bmp->CreateData(size, (ePixelFormat)in.mlFormat, (int)i, (int)m);
						cBitmapData *d = bmp->GetData((int)i, (int)m);
						const cLevel &lv = in.mpLevels[i * in.mlMips + m];
						memcpy(d->mpData, lv.mpData, lv.mlLen < (size_t)d->mlSize ? lv.mlLen : (size_t)d->mlSize);
					}
				avOut.push_back(bmp);
			}
		}

		uint64_t Str(const std::string &asS, uint64_t *apOut) { mStr = asS; apOut[0] = mStr.size(); return (uint64_t)(uintptr_t)mStr.data(); }

		uint64_t Dispatch(uint32_t alOp, cRead &r, uint64_t *out);
	};

	cRustGfxFacade::cRustGfxFacade(iLowLevelGraphics *apLlg) : mpImpl(new cImpl()) { mpImpl->mpLlg = apLlg; }
	cRustGfxFacade::~cRustGfxFacade() { delete mpImpl; }

	uint32_t cRustGfxFacade::WrapTexture(iTexture *apTex) { return (uint32_t)cImpl::Add(mpImpl->mTex, apTex); }
	uint32_t cRustGfxFacade::WrapVertexBuffer(iVertexBuffer *apVb) { return (uint32_t)cImpl::Add(mpImpl->mVb, apVb); }
	uint32_t cRustGfxFacade::WrapProgram(iGpuProgram *apProg) { return (uint32_t)cImpl::Add(mpImpl->mProg, apProg); }
	uint32_t cRustGfxFacade::WrapFrameBuffer(iFrameBuffer *apFb) { return (uint32_t)cImpl::Add(mpImpl->mFb, apFb); }

	uint64_t cRustGfxFacade::Call(void *apSelf, uint32_t alOp, const uint64_t *apArgs, size_t alCount, uint64_t *apOut)
	{
		cRustGfxFacade *pSelf = static_cast<cRustGfxFacade*>(apSelf);
		cRead r(apArgs, alCount);
		return pSelf->mpImpl->Dispatch(alOp, r, apOut);
	}

	uint64_t cRustGfxFacade::cImpl::Dispatch(uint32_t alOp, cRead &r, uint64_t *out)
	{
		iLowLevelGraphics *g = mpLlg;
		switch(alOp)
		{
		case kOhplH5b_INIT:
		{
			cVector2l vSize = r.V2i(); int d = r.I(), bpp = r.I(), fs = r.I(), ms = r.I(), fmt = r.I();
			std::string cap = r.S(); cVector2l pos = r.V2i();
			return g->Init(vSize.x, vSize.y, d, bpp, fs, ms, (eGpuProgramFormat)fmt, cap, pos);
		}
		case kOhplH5b_GPU_PROGRAM_FORMAT: return (uint64_t)(int64_t)g->GetGpuProgramFormat();
		case kOhplH5b_CAPS: return (uint64_t)(int64_t)g->GetCaps((eGraphicCaps)r.I());
		case kOhplH5b_RENDERER_DESCRIPTION: return Str(g->GetRendererDescription(), out);
		case kOhplH5b_WINDOW_HANDLE: return (uint64_t)(uintptr_t)g->GetWindowHandle();
		case kOhplH5b_SET_COLOR_OUTPUT: { std::string m = r.S(); float a = r.F(), b = r.F(); return g->SetColorOutput(m, a, b); }
		case kOhplH5b_COLOR_OUTPUT: return Str(g->GetColorOutput(), out);
		case kOhplH5b_HDR_HEADROOM: return FBits(g->GetHdrHeadroom());
		case kOhplH5b_SCREEN_FLOAT_STATS:
		{
			float a = 0, b = 0, c = 0; bool ok = g->GetScreenFloatStats(a, b, c);
			out[0] = FBits(a); out[1] = FBits(b); out[2] = FBits(c);
			return ok;
		}
		case kOhplH5b_SHOW_CURSOR: g->ShowCursor(r.B()); return 0;
		case kOhplH5b_SET_WINDOW_GRAB: g->SetWindowGrab(r.B()); return 0;
		case kOhplH5b_SET_RELATIVE_MOUSE: g->SetRelativeMouse(r.B()); return 0;
		case kOhplH5b_SET_WINDOW_CAPTION: g->SetWindowCaption(r.S()); return 0;
		case kOhplH5b_WINDOW_MOUSE_FOCUS: return g->GetWindowMouseFocus();
		case kOhplH5b_WINDOW_INPUT_FOCUS: return g->GetWindowInputFocus();
		case kOhplH5b_WINDOW_IS_VISIBLE: return g->GetWindowIsVisible();
		case kOhplH5b_MULTISAMPLING: return (uint64_t)(int64_t)g->GetMultisampling();
		case kOhplH5b_SCREEN_SIZE_FLOAT: { cVector2f v = g->GetScreenSizeFloat(); out[0] = FBits(v.x); out[1] = FBits(v.y); return 0; }
		case kOhplH5b_SCREEN_SIZE_INT: { cVector2l v = g->GetScreenSizeInt(); out[0] = (uint64_t)(int64_t)v.x; out[1] = (uint64_t)(int64_t)v.y; return 0; }
		case kOhplH5b_CHECK_UPDATE_SCREEN_SIZE: return g->CheckAndUpdateScreenSize();
		case kOhplH5b_FORCE_WINDOW_SIZE: { int w = r.I(); int h = r.I(); return g->ForceWindowSize(w, h); }
		case kOhplH5b_FULLSCREEN_ACTIVE: return g->GetFullscreenModeActive();
		case kOhplH5b_SET_VSYNC: { bool a = r.B(); bool b = r.B(); g->SetVsyncActive(a, b); return 0; }
		case kOhplH5b_SET_MULTISAMPLING_ACTIVE: g->SetMultisamplingActive(r.B()); return 0;
		case kOhplH5b_SET_GAMMA: g->SetGammaCorrection(r.F()); return 0;
		case kOhplH5b_GAMMA: return FBits(g->GetGammaCorrection());

		case kOhplH5b_CREATE_FONT: return Add(mFont, g->CreateFontData(r.S()));
		case kOhplH5b_CREATE_PROGRAM: return Add(mProg, g->CreateGpuProgram(r.S()));
		case kOhplH5b_CREATE_SHADER: { std::string n = r.S(); return Add(mShader, g->CreateGpuShader(n, (eGpuShaderType)r.I())); }
		case kOhplH5b_CREATE_TEXTURE: { std::string n = r.S(); int t = r.I(); int u = r.I(); return Add(mTex, g->CreateTexture(n, (eTextureType)t, (eTextureUsage)u)); }
		case kOhplH5b_CREATE_VERTEX_BUFFER:
		{
			int t = r.I(), d = r.I(), u = r.I(), v = r.I(), i = r.I();
			return Add(mVb, g->CreateVertexBuffer((eVertexBufferType)t, (eVertexBufferDrawType)d, (eVertexBufferUsageType)u, v, i));
		}
		case kOhplH5b_CREATE_FRAME_BUFFER: return Add(mFb, g->CreateFrameBuffer(r.S()));
		case kOhplH5b_CREATE_DEPTH_STENCIL: { cVector2l s = r.V2i(); int d = r.I(); int st = r.I(); return Add(mDsb, g->CreateDepthStencilBuffer(s, d, st)); }
		case kOhplH5b_CREATE_OCCLUSION_QUERY: return Add(mOq, g->CreateOcclusionQuery());
		case kOhplH5b_DESTROY_FONT: { uint64_t i = r.U(); delete Get(mFont, i); mFont[i] = NULL; return 0; }
		case kOhplH5b_DESTROY_PROGRAM: { uint64_t i = r.U(); delete Get(mProg, i); mProg[i] = NULL; return 0; }
		case kOhplH5b_DESTROY_SHADER: { uint64_t i = r.U(); delete Get(mShader, i); mShader[i] = NULL; return 0; }
		case kOhplH5b_DESTROY_TEXTURE: { uint64_t i = r.U(); delete Get(mTex, i); mTex[i] = NULL; return 0; }
		case kOhplH5b_DESTROY_VERTEX_BUFFER: { uint64_t i = r.U(); delete Get(mVb, i); mVb[i] = NULL; return 0; }
		case kOhplH5b_DESTROY_FRAME_BUFFER: { uint64_t i = r.U(); delete Get(mFb, i); mFb[i] = NULL; return 0; }
		case kOhplH5b_DESTROY_DEPTH_STENCIL: { uint64_t i = r.U(); delete Get(mDsb, i); mDsb[i] = NULL; return 0; }
		case kOhplH5b_DESTROY_OCCLUSION_QUERY: { uint64_t i = r.U(); delete Get(mOq, i); mOq[i] = NULL; return 0; }

		case kOhplH5b_CLEAR_FRAME_BUFFER: g->ClearFrameBuffer(r.Un()); return 0;
		case kOhplH5b_SET_CLEAR_COLOR: g->SetClearColor(r.C()); return 0;
		case kOhplH5b_SET_CLEAR_DEPTH: g->SetClearDepth(r.F()); return 0;
		case kOhplH5b_SET_CLEAR_STENCIL: g->SetClearStencil(r.I()); return 0;
		case kOhplH5b_FLUSH_RENDERING: g->FlushRendering(); return 0;
		case kOhplH5b_WAIT_AND_FINISH_RENDERING: g->WaitAndFinishRendering(); return 0;
		case kOhplH5b_SWAP_BUFFERS: g->SwapBuffers(); return 0;
		case kOhplH5b_COPY_FRAME_BUFFER_TO_TEXTURE:
		{
			iTexture *t = Tex(r); cVector2l p = r.V2i(); cVector2l s = r.V2i(); cVector2l o = r.V2i();
			g->CopyFrameBufferToTexure(t, p, s, o);
			return 0;
		}
		case kOhplH5b_COPY_FRAME_BUFFER_TO_BITMAP:
		{
			cVector2l p = r.V2i(); cVector2l s = r.V2i();
			cBitmap *b = g->CopyFrameBufferToBitmap(p, s);
			cBitmapData *d = b->GetData(0, 0);
			mBytes.assign(d->mpData, d->mpData + d->mlSize);
			out[0] = (uint64_t)(int64_t)b->GetSize().x; out[1] = (uint64_t)(int64_t)b->GetSize().y; out[2] = (uint64_t)(int64_t)b->GetSize().z;
			out[3] = (uint64_t)(int64_t)b->GetPixelFormat(); out[4] = mBytes.size();
			delete b;
			return (uint64_t)(uintptr_t)mBytes.data();
		}
		case kOhplH5b_SET_CURRENT_FRAME_BUFFER:
		{
			uint64_t id = r.U(); cVector2l p = r.V2i(); cVector2l s = r.V2i();
			g->SetCurrentFrameBuffer(id == kNone ? NULL : Get(mFb, id), p, s);
			return 0;
		}
		case kOhplH5b_CURRENT_FRAME_BUFFER: return Find(mFb, g->GetCurrentFrameBuffer());
		case kOhplH5b_SET_FRAME_BUFFER_DRAW_TARGETS: { int *t = r.P<int>(); (void)r.U(); int n = r.I(); g->SetFrameBufferDrawTargets(t, n); return 0; }

		case kOhplH5b_SET_COLOR_WRITE_ACTIVE: { bool a = r.B(), b = r.B(), c = r.B(), d = r.B(); g->SetColorWriteActive(a, b, c, d); return 0; }
		case kOhplH5b_SET_DEPTH_WRITE_ACTIVE: g->SetDepthWriteActive(r.B()); return 0;
		case kOhplH5b_SET_CULL_ACTIVE: g->SetCullActive(r.B()); return 0;
		case kOhplH5b_SET_CULL_MODE: g->SetCullMode((eCullMode)r.I()); return 0;
		case kOhplH5b_SET_DEPTH_TEST_ACTIVE: g->SetDepthTestActive(r.B()); return 0;
		case kOhplH5b_SET_DEPTH_TEST_FUNC: g->SetDepthTestFunc((eDepthTestFunc)r.I()); return 0;
		case kOhplH5b_SET_ALPHA_TEST_ACTIVE: g->SetAlphaTestActive(r.B()); return 0;
		case kOhplH5b_SET_ALPHA_TEST_FUNC: { int f = r.I(); float x = r.F(); g->SetAlphaTestFunc((eAlphaTestFunc)f, x); return 0; }
		case kOhplH5b_SET_STENCIL_ACTIVE: g->SetStencilActive(r.B()); return 0;
		case kOhplH5b_SET_STENCIL_WRITE_MASK: g->SetStencilWriteMask(r.Un()); return 0;
		case kOhplH5b_SET_STENCIL:
		{
			int f = r.I(), ref = r.I(); unsigned m = r.Un(); int a = r.I(), b = r.I(), c = r.I();
			g->SetStencil((eStencilFunc)f, ref, m, (eStencilOp)a, (eStencilOp)b, (eStencilOp)c);
			return 0;
		}
		case kOhplH5b_SET_STENCIL_TWO_SIDE:
		{
			int f = r.I(), b = r.I(), ref = r.I(); unsigned m = r.Un(); const int *o = r.P<const int>(); (void)r.U();
			g->SetStencilTwoSide((eStencilFunc)f, (eStencilFunc)b, ref, m, (eStencilOp)o[0], (eStencilOp)o[1], (eStencilOp)o[2],
								 (eStencilOp)o[3], (eStencilOp)o[4], (eStencilOp)o[5]);
			return 0;
		}
		case kOhplH5b_SET_SCISSOR_ACTIVE: g->SetScissorActive(r.B()); return 0;
		case kOhplH5b_SET_SCISSOR_RECT: { cVector2l p = r.V2i(); cVector2l s = r.V2i(); g->SetScissorRect(p, s); return 0; }
		case kOhplH5b_SET_CLIP_PLANE: { int i = r.I(); cColor c = r.C(); g->SetClipPlane(i, cPlanef(c.r, c.g, c.b, c.a)); return 0; }
		case kOhplH5b_CLIP_PLANE: { cPlanef p = g->GetClipPlane(r.I()); out[0] = FBits(p.a); out[1] = FBits(p.b); out[2] = FBits(p.c); out[3] = FBits(p.d); return 0; }
		case kOhplH5b_SET_CLIP_PLANE_ACTIVE: { int i = r.I(); bool b = r.B(); g->SetClipPlaneActive(i, b); return 0; }
		case kOhplH5b_SET_COLOR: g->SetColor(r.C()); return 0;
		case kOhplH5b_SET_BLEND_ACTIVE: g->SetBlendActive(r.B()); return 0;
		case kOhplH5b_SET_BLEND_FUNC: { int a = r.I(), b = r.I(); g->SetBlendFunc((eBlendFunc)a, (eBlendFunc)b); return 0; }
		case kOhplH5b_SET_BLEND_FUNC_SEPARATE:
		{
			int a = r.I(), b = r.I(), c = r.I(), d = r.I();
			g->SetBlendFuncSeparate((eBlendFunc)a, (eBlendFunc)b, (eBlendFunc)c, (eBlendFunc)d);
			return 0;
		}
		case kOhplH5b_SET_POLYGON_OFFSET_ACTIVE: g->SetPolygonOffsetActive(r.B()); return 0;
		case kOhplH5b_SET_DEPTH_CLAMP_ACTIVE: g->SetDepthClampActive(r.B()); return 0;
		case kOhplH5b_SET_POLYGON_OFFSET: { float a = r.F(), b = r.F(); g->SetPolygonOffset(a, b); return 0; }

		case kOhplH5b_PUSH_MATRIX: g->PushMatrix((eMatrix)r.I()); return 0;
		case kOhplH5b_POP_MATRIX: g->PopMatrix((eMatrix)r.I()); return 0;
		case kOhplH5b_SET_IDENTITY_MATRIX: g->SetIdentityMatrix((eMatrix)r.I()); return 0;
		case kOhplH5b_SET_MATRIX: { int k = r.I(); cMatrixf m = r.M(); g->SetMatrix((eMatrix)k, m); return 0; }
		case kOhplH5b_SET_ORTHO_SIZE: { cVector2f s = r.V2(); float a = r.F(), b = r.F(); g->SetOrthoProjection(s, a, b); return 0; }
		case kOhplH5b_SET_ORTHO_BOX: { cVector3f a = r.V3(); cVector3f b = r.V3(); g->SetOrthoProjection(a, b); return 0; }
		case kOhplH5b_SET_TEXTURE: { unsigned u = r.Un(); g->SetTexture(u, TexO(r)); return 0; }
		case kOhplH5b_SET_ACTIVE_TEXTURE_UNIT: g->SetActiveTextureUnit(r.Un()); return 0;
		case kOhplH5b_SET_TEXTURE_ENV: { int p = r.I(), v = r.I(); g->SetTextureEnv((eTextureParam)p, v); return 0; }
		case kOhplH5b_SET_TEXTURE_CONSTANT_COLOR: g->SetTextureConstantColor(r.C()); return 0;

		case kOhplH5b_TEX_CREATE_FROM_BITMAP:
		{
			iTexture *t = Tex(r); std::vector<cBitmap*> v; Bitmaps(r, v);
			bool ok = t->CreateFromBitmap(v[0]);
			for(size_t i = 0; i < v.size(); ++i) delete v[i];
			return ok;
		}
		case kOhplH5b_TEX_CREATE_ANIM:
		{
			iTexture *t = Tex(r); std::vector<cBitmap*> v; Bitmaps(r, v);
			bool ok = t->CreateAnimFromBitmapVec(&v);
			for(size_t i = 0; i < v.size(); ++i) delete v[i];
			return ok;
		}
		case kOhplH5b_TEX_CREATE_CUBE:
		{
			iTexture *t = Tex(r); std::vector<cBitmap*> v; Bitmaps(r, v);
			bool ok = t->CreateCubeFromBitmapVec(&v);
			for(size_t i = 0; i < v.size(); ++i) delete v[i];
			return ok;
		}
		case kOhplH5b_TEX_CREATE_RAW:
		{
			iTexture *t = Tex(r); cVector3l s = r.V3i(); int f = r.I(); bool has = r.B(); unsigned char *p = r.P<unsigned char>(); (void)r.U();
			return t->CreateFromRawData(s, (ePixelFormat)f, has ? p : NULL);
		}
		case kOhplH5b_TEX_SET_RAW:
		{
			iTexture *t = Tex(r); int lv = r.I(); cVector3l o = r.V3i(); cVector3l s = r.V3i(); int f = r.I(); bool has = r.B(); void *p = r.P<void>(); (void)r.U();
			t->SetRawData(lv, o, s, (ePixelFormat)f, has ? p : NULL);
			return 0;
		}
		case kOhplH5b_TEX_UPDATE: { iTexture *t = Tex(r); t->Update(r.F()); return 0; }
		case kOhplH5b_TEX_SET_FILTER: { iTexture *t = Tex(r); t->SetFilter((eTextureFilter)r.I()); return 0; }
		case kOhplH5b_TEX_SET_ANISOTROPY: { iTexture *t = Tex(r); t->SetAnisotropyDegree(r.F()); return 0; }
		case kOhplH5b_TEX_SET_WRAP_S: { iTexture *t = Tex(r); t->SetWrapS((eTextureWrap)r.I()); return 0; }
		case kOhplH5b_TEX_SET_WRAP_T: { iTexture *t = Tex(r); t->SetWrapT((eTextureWrap)r.I()); return 0; }
		case kOhplH5b_TEX_SET_WRAP_R: { iTexture *t = Tex(r); t->SetWrapR((eTextureWrap)r.I()); return 0; }
		case kOhplH5b_TEX_SET_WRAP_STR: { iTexture *t = Tex(r); t->SetWrapSTR((eTextureWrap)r.I()); return 0; }
		case kOhplH5b_TEX_SET_COMPARE_MODE: { iTexture *t = Tex(r); t->SetCompareMode((eTextureCompareMode)r.I()); return 0; }
		case kOhplH5b_TEX_SET_COMPARE_FUNC: { iTexture *t = Tex(r); t->SetCompareFunc((eTextureCompareFunc)r.I()); return 0; }
		case kOhplH5b_TEX_SET_RED_FROM_ALPHA: { iTexture *t = Tex(r); t->SetRedFromAlpha(r.B()); return 0; }
		case kOhplH5b_TEX_AUTO_MIPMAPS: Tex(r)->AutoGenerateMipmaps(); return 0;
		case kOhplH5b_TEX_HAS_ANIMATION: return Tex(r)->HasAnimation();
		case kOhplH5b_TEX_NEXT_FRAME: Tex(r)->NextFrame(); return 0;
		case kOhplH5b_TEX_PREV_FRAME: Tex(r)->PrevFrame(); return 0;
		case kOhplH5b_TEX_TIME_COUNT: return FBits(Tex(r)->GetTimeCount());
		case kOhplH5b_TEX_SET_TIME_COUNT: { iTexture *t = Tex(r); t->SetTimeCount(r.F()); return 0; }
		case kOhplH5b_TEX_HANDLE: return (uint64_t)(int64_t)Tex(r)->GetCurrentLowlevelHandle();
		case kOhplH5b_TEX_SIZE:
		{
			cVector3l s = Tex(r)->GetSize();
			out[0] = (uint64_t)(int64_t)s.x; out[1] = (uint64_t)(int64_t)s.y; out[2] = (uint64_t)(int64_t)s.z;
			return 0;
		}
		case kOhplH5b_TEX_PIXELS_RGBA_FLOAT:
		{
			mFloats.clear();
			if(!Tex(r)->GetRawPixelsRGBAFloat(mFloats)) return 0;
			out[0] = mFloats.size();
			return (uint64_t)(uintptr_t)(mFloats.empty() ? (const float*)&mFloats : mFloats.data());
		}

		case kOhplH5b_VB_CREATE_ELEMENT_ARRAY: { iVertexBuffer *v = Vb(r); int k = r.I(), f = r.I(), n = r.I(), p = r.I(); v->CreateElementArray((eVertexBufferElement)k, (eVertexBufferElementFormat)f, n, p); return 0; }
		case kOhplH5b_VB_ADD_VEC3: { iVertexBuffer *v = Vb(r); int e = r.I(); cVector3f p = r.V3(); v->AddVertexVec3f((eVertexBufferElement)e, p); return 0; }
		case kOhplH5b_VB_ADD_VEC4: { iVertexBuffer *v = Vb(r); int e = r.I(); cVector3f p = r.V3(); float w = r.F(); v->AddVertexVec4f((eVertexBufferElement)e, p, w); return 0; }
		case kOhplH5b_VB_ADD_COLOR: { iVertexBuffer *v = Vb(r); int e = r.I(); cColor c = r.C(); v->AddVertexColor((eVertexBufferElement)e, c); return 0; }
		case kOhplH5b_VB_ADD_INDEX: { iVertexBuffer *v = Vb(r); v->AddIndex(r.Un()); return 0; }
		case kOhplH5b_VB_COMPILE: { iVertexBuffer *v = Vb(r); return v->Compile(r.Un()); }
		case kOhplH5b_VB_UPDATE: { iVertexBuffer *v = Vb(r); unsigned t = r.Un(); bool i = r.B(); v->UpdateData(t, i); return 0; }
		case kOhplH5b_VB_CREATE_SHADOW_DOUBLE: { iVertexBuffer *v = Vb(r); v->CreateShadowDouble(r.B()); return 0; }
		case kOhplH5b_VB_TRANSFORM: { iVertexBuffer *v = Vb(r); cMatrixf m = r.M(); v->Transform(m); return 0; }
		case kOhplH5b_VB_DRAW: { iVertexBuffer *v = Vb(r); v->Draw((eVertexBufferDrawType)r.I()); return 0; }
		case kOhplH5b_VB_DRAW_INDICES:
		{
			iVertexBuffer *v = Vb(r); unsigned *p = r.P<unsigned>(); (void)r.U(); int n = r.I(); int d = r.I();
			v->DrawIndices(p, n, (eVertexBufferDrawType)d);
			return 0;
		}
		case kOhplH5b_VB_BIND: Vb(r)->Bind(); return 0;
		case kOhplH5b_VB_UNBIND: Vb(r)->UnBind(); return 0;
		case kOhplH5b_VB_CREATE_COPY:
		{
			iVertexBuffer *v = Vb(r); int t = r.I(), u = r.I(); unsigned f = r.Un();
			return Add(mVb, v->CreateCopy((eVertexBufferType)t, (eVertexBufferUsageType)u, f));
		}
		case kOhplH5b_VB_BOUNDING_VOLUME:
		{
			cBoundingVolume bv = Vb(r)->CreateBoundingVolume();
			const cVector3f &a = bv.GetLocalMin(); const cVector3f &b = bv.GetLocalMax();
			out[0] = FBits(a.x); out[1] = FBits(a.y); out[2] = FBits(a.z); out[3] = FBits(b.x); out[4] = FBits(b.y); out[5] = FBits(b.z);
			return 0;
		}
		case kOhplH5b_VB_ELEMENT_NUM: { iVertexBuffer *v = Vb(r); return (uint64_t)(int64_t)v->GetElementNum((eVertexBufferElement)r.I()); }
		case kOhplH5b_VB_ELEMENT_FORMAT: { iVertexBuffer *v = Vb(r); return (uint64_t)(int64_t)v->GetElementFormat((eVertexBufferElement)r.I()); }
		case kOhplH5b_VB_ELEMENT_VAR: { iVertexBuffer *v = Vb(r); return (uint64_t)(int64_t)v->GetElementProgramVarIndex((eVertexBufferElement)r.I()); }
		case kOhplH5b_VB_VERTEX_NUM: return (uint64_t)(int64_t)Vb(r)->GetVertexNum();
		case kOhplH5b_VB_INDEX_NUM: return (uint64_t)(int64_t)Vb(r)->GetIndexNum();
		case kOhplH5b_VB_RESIZE_ARRAY: { iVertexBuffer *v = Vb(r); int e = r.I(); int s = r.I(); v->ResizeArray((eVertexBufferElement)e, s); return 0; }
		case kOhplH5b_VB_RESIZE_INDICES: { iVertexBuffer *v = Vb(r); v->ResizeIndices(r.I()); return 0; }

		case kOhplH5b_SHADER_FROM_FILE: { iGpuShader *s = Get(mShader, r.U()); tWString f = r.W(); std::string e = r.S(); bool p = r.B(); return s->CreateFromFile(f, e, p); }
		case kOhplH5b_SHADER_FROM_STRING: { iGpuShader *s = Get(mShader, r.U()); std::string src = r.S(); std::string e = r.S(); bool p = r.B(); return s->CreateFromString(src.c_str(), e, p); }
		case kOhplH5b_PROG_SET_SHADER: { iGpuProgram *p = Prog(r); int k = r.I(); uint64_t id = r.U(); p->SetShader((eGpuShaderType)k, id == kNone ? NULL : Get(mShader, id)); return 0; }
		case kOhplH5b_PROG_LINK: return Prog(r)->Link();
		case kOhplH5b_PROG_DUMP_UNIFORMS: return Str(Prog(r)->DumpUniforms(), out);
		case kOhplH5b_PROG_BIND: Prog(r)->Bind(); return 0;
		case kOhplH5b_PROG_UNBIND: Prog(r)->UnBind(); return 0;
		case kOhplH5b_PROG_SET_SAMPLER_TO_UNIT: { iGpuProgram *p = Prog(r); std::string n = r.S(); return p->SetSamplerToUnit(n, r.I()); }
		case kOhplH5b_PROG_VARIABLE_ID: { iGpuProgram *p = Prog(r); return (uint64_t)(int64_t)p->GetVariableId(r.S()); }
		case kOhplH5b_PROG_VARIABLE_AS_ID: { iGpuProgram *p = Prog(r); std::string n = r.S(); return p->GetVariableAsId(n, r.I()); }
		case kOhplH5b_PROG_SET_INT: { iGpuProgram *p = Prog(r); int v = r.I(); return p->SetInt(v, r.I()); }
		case kOhplH5b_PROG_SET_FLOAT: { iGpuProgram *p = Prog(r); int v = r.I(); return p->SetFloat(v, r.F()); }
		case kOhplH5b_PROG_SET_VEC2: { iGpuProgram *p = Prog(r); int v = r.I(); cVector2f a = r.V2(); return p->SetVec2f(v, a.x, a.y); }
		case kOhplH5b_PROG_SET_VEC3: { iGpuProgram *p = Prog(r); int v = r.I(); cVector3f a = r.V3(); return p->SetVec3f(v, a.x, a.y, a.z); }
		case kOhplH5b_PROG_SET_VEC4: { iGpuProgram *p = Prog(r); int v = r.I(); cColor a = r.C(); return p->SetVec4f(v, a.r, a.g, a.b, a.a); }
		case kOhplH5b_PROG_SET_FLOAT_ARRAY: { iGpuProgram *p = Prog(r); int v = r.I(); const float *x = r.P<const float>(); int n = (int)r.U(); return p->SetFloatArray(v, x, n); }
		case kOhplH5b_PROG_SET_MATRIX: { iGpuProgram *p = Prog(r); int v = r.I(); cMatrixf m = r.M(); return p->SetMatrixf(v, m); }
		case kOhplH5b_PROG_SET_MATRIX_API: { iGpuProgram *p = Prog(r); int v = r.I(); int k = r.I(); int o = r.I(); return p->SetMatrixf(v, (eGpuShaderMatrix)k, (eGpuShaderMatrixOp)o); }

		case kOhplH5b_FB_SET_TEXTURE_2D: { iFrameBuffer *f = Fb(r); int i = r.I(); iTexture *t = TexO(r); f->SetTexture2D(i, t, r.I()); return 0; }
		case kOhplH5b_FB_SET_TEXTURE_3D: { iFrameBuffer *f = Fb(r); int i = r.I(); iTexture *t = TexO(r); int z = r.I(); f->SetTexture3D(i, t, z, r.I()); return 0; }
		case kOhplH5b_FB_SET_TEXTURE_CUBE: { iFrameBuffer *f = Fb(r); int i = r.I(); iTexture *t = TexO(r); int z = r.I(); f->SetTextureCubeMap(i, t, z, r.I()); return 0; }
		case kOhplH5b_FB_SET_DEPTH_TEXTURE_2D: { iFrameBuffer *f = Fb(r); iTexture *t = TexO(r); f->SetDepthTexture2D(t, r.I()); return 0; }
		case kOhplH5b_FB_SET_DEPTH_TEXTURE_CUBE: { iFrameBuffer *f = Fb(r); iTexture *t = TexO(r); int z = r.I(); f->SetDepthTextureCubeMap(t, z, r.I()); return 0; }
		case kOhplH5b_FB_SET_DEPTH_STENCIL: { iFrameBuffer *f = Fb(r); uint64_t id = r.U(); f->SetDepthStencilBuffer(id == kNone ? NULL : Get(mDsb, id)); return 0; }
		case kOhplH5b_FB_COMPILE_AND_VALIDATE: return Fb(r)->CompileAndValidate();
		case kOhplH5b_FB_POST_BIND_UPDATE: Fb(r)->PostBindUpdate(); return 0;

		case kOhplH5b_OQ_BEGIN: Get(mOq, r.U())->Begin(); return 0;
		case kOhplH5b_OQ_END: Get(mOq, r.U())->End(); return 0;
		case kOhplH5b_OQ_FETCH_RESULTS: return Get(mOq, r.U())->FetchResults();
		case kOhplH5b_OQ_SAMPLE_COUNT: return Get(mOq, r.U())->GetSampleCount();
		case kOhplH5b_FONT_FROM_FONT_FILE:
		{
			iFontData *f = Get(mFont, r.U()); tWString n = r.W(); int s = r.I(); unsigned a = r.Un(); unsigned b = r.Un();
			return f->CreateFromFontFile(n, s, (unsigned short)a, (unsigned short)b);
		}
		case kOhplH5b_FONT_FROM_BITMAP_FILE: { iFontData *f = Get(mFont, r.U()); return f->CreateFromBitmapFile(r.W()); }

		case kOhplH5b_DRAW_TRIANGLE: { tVertexVec v = r.Vs(); g->DrawTriangle(v); return 0; }
		case kOhplH5b_DRAW_QUAD_POS: { cVector3f p = r.V3(); cVector2f s = r.V2(); g->DrawQuad(p, s, r.C()); return 0; }
		case kOhplH5b_DRAW_QUAD_UV: { cVector3f p = r.V3(); cVector2f s = r.V2(); cVector2f a = r.V2(); cVector2f b = r.V2(); g->DrawQuad(p, s, a, b, r.C()); return 0; }
		case kOhplH5b_DRAW_QUAD_UV2:
		{
			cVector3f p = r.V3(); cVector2f s = r.V2(); cVector2f a = r.V2(); cVector2f b = r.V2(); cVector2f c = r.V2(); cVector2f d = r.V2();
			g->DrawQuad(p, s, a, b, c, d, r.C());
			return 0;
		}
		case kOhplH5b_DRAW_QUAD_VTX: { tVertexVec v = r.Vs(); g->DrawQuad(v); return 0; }
		case kOhplH5b_DRAW_QUAD_VTX_COL: { tVertexVec v = r.Vs(); g->DrawQuad(v, r.C()); return 0; }
		case kOhplH5b_DRAW_QUAD_VTX_Z: { tVertexVec v = r.Vs(); g->DrawQuad(v, r.F()); return 0; }
		case kOhplH5b_DRAW_QUAD_VTX_Z_COL: { tVertexVec v = r.Vs(); float z = r.F(); cColor c = r.C(); g->DrawQuad(v, z, c); return 0; }
		case kOhplH5b_DRAW_QUAD_MULTITEX:
		{
			tVertexVec v = r.Vs(); const float *p = r.P<const float>(); size_t n = (size_t)r.U();
			tVector3fVec e;
			for(size_t i = 0; i < n; ++i) e.push_back(cVector3f(p[i * 3], p[i * 3 + 1], p[i * 3 + 2]));
			g->DrawQuadMultiTex(v, e);
			return 0;
		}
		case kOhplH5b_DRAW_LINE: { cVector3f a = r.V3(); cVector3f b = r.V3(); g->DrawLine(a, b, r.C()); return 0; }
		case kOhplH5b_DRAW_LINE_COLS: { cVector3f a = r.V3(); cColor ac = r.C(); cVector3f b = r.V3(); cColor bc = r.C(); g->DrawLine(a, ac, b, bc); return 0; }
		case kOhplH5b_DRAW_BOX_MIN_MAX: { cVector3f a = r.V3(); cVector3f b = r.V3(); g->DrawBoxMinMax(a, b, r.C()); return 0; }
		case kOhplH5b_DRAW_SPHERE: { cVector3f p = r.V3(); float rad = r.F(); g->DrawSphere(p, rad, r.C()); return 0; }
		case kOhplH5b_DRAW_SPHERE_AXES: { cVector3f p = r.V3(); float rad = r.F(); cColor a = r.C(); cColor b = r.C(); cColor c = r.C(); g->DrawSphere(p, rad, a, b, c); return 0; }
		case kOhplH5b_DRAW_LINE_QUAD_RECT: { cColor q = r.C(); float z = r.F(); cColor c = r.C(); g->DrawLineQuad(cRect2f(q.r, q.g, q.b, q.a), z, c); return 0; }
		case kOhplH5b_DRAW_LINE_QUAD_POS: { cVector3f p = r.V3(); cVector2f s = r.V2(); g->DrawLineQuad(p, s, r.C()); return 0; }

		case kOhplH5b_ADD_VERTEX_TO_BATCH: { cVertex v = r.V1(); g->AddVertexToBatch(&v); return 0; }
		case kOhplH5b_ADD_VERTEX_TO_BATCH_TRANSLATED: { cVertex v = r.V1(); cVector3f t = r.V3(); g->AddVertexToBatch(&v, &t); return 0; }
		case kOhplH5b_ADD_VERTEX_TO_BATCH_MATRIX: { cVertex v = r.V1(); cMatrixf m = r.M(); g->AddVertexToBatch(&v, &m); return 0; }
		case kOhplH5b_ADD_VERTEX_TO_BATCH_SIZE2D:
		{
			cVertex v = r.V1(); cVector3f t = r.V3(); bool has = r.B(); cColor c = r.C(); float w = r.F(), h = r.F();
			g->AddVertexToBatch_Size2D(&v, &t, has ? &c : NULL, w, h);
			return 0;
		}
		case kOhplH5b_ADD_VERTEX_TO_BATCH_RAW: { cVector3f p = r.V3(); cColor c = r.C(); cVector3f t = r.V3(); g->AddVertexToBatch_Raw(p, c, t); return 0; }
		case kOhplH5b_ADD_INDEX_TO_BATCH: g->AddIndexToBatch(r.I()); return 0;
		case kOhplH5b_ADD_TEXCOORD_TO_BATCH: { unsigned u = r.Un(); cVector3f c = r.V3(); g->AddTexCoordToBatch(u, &c); return 0; }
		case kOhplH5b_SET_BATCH_TEXTURE_UNIT_ACTIVE: { unsigned u = r.Un(); g->SetBatchTextureUnitActive(u, r.B()); return 0; }
		case kOhplH5b_FLUSH_TRI_BATCH: { unsigned f = r.Un(); g->FlushTriBatch(f, r.B()); return 0; }
		case kOhplH5b_FLUSH_QUAD_BATCH: { unsigned f = r.Un(); g->FlushQuadBatch(f, r.B()); return 0; }
		case kOhplH5b_CLEAR_BATCH: g->ClearBatch(); return 0;
		}
		return 0;
	}

}
