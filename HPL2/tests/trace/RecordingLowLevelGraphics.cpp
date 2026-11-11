#include "RecordingLowLevelGraphics.h"

#include "math/Math.h"
#include "system/String.h"

namespace hpl {

	namespace {
		unsigned long long SplitMix64(unsigned long long& alState)
		{
			unsigned long long z = (alState += 0x9e3779b97f4a7c15ull);
			z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
			z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
			return z ^ (z >> 31);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// LOW LEVEL
	//////////////////////////////////////////////////////////////////////////

	cRecordingLowLevelGraphics::cRecordingLowLevelGraphics(const cRecordingConfig& aCfg)
		: mCfg(aCfg), mvScreenSize(aCfg.mvScreenSize), mpCurrentFrameBuffer(NULL), mfGamma(1.0f), mlMultisampling(0),
		  mbFullscreen(false), mProgramFormat(eGpuProgramFormat_LastEnum), msColorOutput("sdr"), mlOcclusionFetches(0)
	{
		unsigned long long state = mCfg.mlSeed;
		int n = mCfg.mlOcclusionTableSize > 0 ? mCfg.mlOcclusionTableSize : 1;
		for(int i = 0; i < n; ++i)
		{
			unsigned long long r = SplitMix64(state);
			mOcclusionTable.push_back((r & 3) == 0 ? 0u : (unsigned int)((r >> 8) % 1000u) + 1u);
		}
	}

	cRecordingLowLevelGraphics::~cRecordingLowLevelGraphics() {}

	unsigned int cRecordingLowLevelGraphics::OcclusionSample(int alFetchIndex) const
	{
		return mOcclusionTable[alFetchIndex % mOcclusionTable.size()];
	}

	bool cRecordingLowLevelGraphics::Init(int alWidth, int alHeight, int alDisplay, int alBpp, int abFullscreen,
										  int alMultisampling, eGpuProgramFormat aGpuProgramFormat,
										  const tString& asWindowCaption, const cVector2l& avWindowPos)
	{
		Rec("Init", alWidth, alHeight, alDisplay, alBpp, abFullscreen, alMultisampling, aGpuProgramFormat, asWindowCaption, avWindowPos);
		mvScreenSize = cVector2l(alWidth, alHeight);
		mlMultisampling = alMultisampling;
		mbFullscreen = abFullscreen != 0;
		mProgramFormat = aGpuProgramFormat;
		return true;
	}

	eGpuProgramFormat cRecordingLowLevelGraphics::GetGpuProgramFormat()
	{
		RecR("GetGpuProgramFormat", mProgramFormat);
		return mProgramFormat;
	}

	int cRecordingLowLevelGraphics::GetCaps(eGraphicCaps aType)
	{
		auto it = mCfg.mCaps.find((int)aType);
		int v = it != mCfg.mCaps.end() ? it->second : mCfg.mlDefaultCap;
		RecR("GetCaps", v, aType);
		return v;
	}

	tString cRecordingLowLevelGraphics::GetRendererDescription()
	{
		RecR("GetRendererDescription", tString("recording"));
		return "recording";
	}

	void* cRecordingLowLevelGraphics::GetWindowHandle()
	{
		Rec("GetWindowHandle");
		return NULL;
	}

	bool cRecordingLowLevelGraphics::SetColorOutput(const tString& asMode, float afPaperWhite, float afPeakNits)
	{
		RecR("SetColorOutput", true, asMode, afPaperWhite, afPeakNits);
		msColorOutput = asMode;
		return true;
	}

	tString cRecordingLowLevelGraphics::GetColorOutput()
	{
		RecR("GetColorOutput", msColorOutput);
		return msColorOutput;
	}

	float cRecordingLowLevelGraphics::GetHdrHeadroom()
	{
		RecR("GetHdrHeadroom", 1.0f);
		return 1.0f;
	}

	bool cRecordingLowLevelGraphics::GetScreenFloatStats(float& afMax, float& afMean, float& afFracOver1)
	{
		afMax = 1.0f;
		afMean = 0.5f;
		afFracOver1 = 0.0f;
		RecR("GetScreenFloatStats", true, afMax, afMean, afFracOver1);
		return true;
	}

	void cRecordingLowLevelGraphics::ShowCursor(bool abX) { Rec("ShowCursor", abX); }
	void cRecordingLowLevelGraphics::SetWindowGrab(bool abX) { Rec("SetWindowGrab", abX); }
	void cRecordingLowLevelGraphics::SetRelativeMouse(bool abX) { Rec("SetRelativeMouse", abX); }
	void cRecordingLowLevelGraphics::SetWindowCaption(const tString& asName) { Rec("SetWindowCaption", asName); }
	bool cRecordingLowLevelGraphics::GetWindowMouseFocus() { RecR("GetWindowMouseFocus", true); return true; }
	bool cRecordingLowLevelGraphics::GetWindowInputFocus() { RecR("GetWindowInputFocus", true); return true; }
	bool cRecordingLowLevelGraphics::GetWindowIsVisible() { RecR("GetWindowIsVisible", true); return true; }
	int cRecordingLowLevelGraphics::GetMultisampling() { RecR("GetMultisampling", mlMultisampling); return mlMultisampling; }

	cVector2f cRecordingLowLevelGraphics::GetScreenSizeFloat()
	{
		cVector2f v((float)mvScreenSize.x, (float)mvScreenSize.y);
		RecR("GetScreenSizeFloat", v);
		return v;
	}

	const cVector2l& cRecordingLowLevelGraphics::GetScreenSizeInt()
	{
		RecR("GetScreenSizeInt", mvScreenSize);
		return mvScreenSize;
	}

	bool cRecordingLowLevelGraphics::CheckAndUpdateScreenSize() { RecR("CheckAndUpdateScreenSize", false); return false; }

	bool cRecordingLowLevelGraphics::ForceWindowSize(int alWidth, int alHeight)
	{
		Rec("ForceWindowSize", alWidth, alHeight);
		mvScreenSize = cVector2l(alWidth, alHeight);
		return true;
	}

	bool cRecordingLowLevelGraphics::GetFullscreenModeActive() { RecR("GetFullscreenModeActive", mbFullscreen); return mbFullscreen; }
	void cRecordingLowLevelGraphics::SetVsyncActive(bool abX, bool abAdaptive) { Rec("SetVsyncActive", abX, abAdaptive); }
	void cRecordingLowLevelGraphics::SetMultisamplingActive(bool abX) { Rec("SetMultisamplingActive", abX); }
	void cRecordingLowLevelGraphics::SetGammaCorrection(float afX) { Rec("SetGammaCorrection", afX); mfGamma = afX; }
	float cRecordingLowLevelGraphics::GetGammaCorrection() { RecR("GetGammaCorrection", mfGamma); return mfGamma; }

	iFontData* cRecordingLowLevelGraphics::CreateFontData(const tString& asName)
	{
		iFontData* p = new cRecordingFontData(this, asName);
		RecR("CreateFontData", p, asName);
		return p;
	}

	iGpuProgram* cRecordingLowLevelGraphics::CreateGpuProgram(const tString& asName)
	{
		iGpuProgram* p = new cRecordingGpuProgram(this, asName, mProgramFormat);
		RecR("CreateGpuProgram", p, asName);
		return p;
	}

	iGpuShader* cRecordingLowLevelGraphics::CreateGpuShader(const tString& asName, eGpuShaderType aType)
	{
		iGpuShader* p = new cRecordingGpuShader(this, asName, aType);
		RecR("CreateGpuShader", p, asName, aType);
		return p;
	}

	iTexture* cRecordingLowLevelGraphics::CreateTexture(const tString& asName, eTextureType aType, eTextureUsage aUsage)
	{
		iTexture* p = new cRecordingTexture(this, asName, aType, aUsage);
		RecR("CreateTexture", p, asName, aType, aUsage);
		return p;
	}

	iVertexBuffer* cRecordingLowLevelGraphics::CreateVertexBuffer(eVertexBufferType aType, eVertexBufferDrawType aDrawType,
																  eVertexBufferUsageType aUsageType, int alReserveVtxSize,
																  int alReserveIdxSize)
	{
		iVertexBuffer* p = new cRecordingVertexBuffer(this, aType, aDrawType, aUsageType, alReserveVtxSize, alReserveIdxSize);
		RecR("CreateVertexBuffer", p, aType, aDrawType, aUsageType, alReserveVtxSize, alReserveIdxSize);
		return p;
	}

	iFrameBuffer* cRecordingLowLevelGraphics::CreateFrameBuffer(const tString& asName)
	{
		iFrameBuffer* p = new cRecordingFrameBuffer(this, asName);
		RecR("CreateFrameBuffer", p, asName);
		return p;
	}

	iDepthStencilBuffer* cRecordingLowLevelGraphics::CreateDepthStencilBuffer(const cVector2l& avSize, int alDepthBits,
																			  int alStencilBits)
	{
		iDepthStencilBuffer* p = new cRecordingDepthStencilBuffer(this, avSize, alDepthBits, alStencilBits);
		RecR("CreateDepthStencilBuffer", p, avSize, alDepthBits, alStencilBits);
		return p;
	}

	iOcclusionQuery* cRecordingLowLevelGraphics::CreateOcclusionQuery()
	{
		iOcclusionQuery* p = new cRecordingOcclusionQuery(this);
		RecR("CreateOcclusionQuery", p);
		return p;
	}

	void cRecordingLowLevelGraphics::ClearFrameBuffer(tClearFrameBufferFlag aFlags) { Rec("ClearFrameBuffer", aFlags); }
	void cRecordingLowLevelGraphics::SetClearColor(const cColor& aCol) { Rec("SetClearColor", aCol); }
	void cRecordingLowLevelGraphics::SetClearDepth(float afDepth) { Rec("SetClearDepth", afDepth); }
	void cRecordingLowLevelGraphics::SetClearStencil(int alVal) { Rec("SetClearStencil", alVal); }
	void cRecordingLowLevelGraphics::FlushRendering() { Rec("FlushRendering"); }
	void cRecordingLowLevelGraphics::WaitAndFinishRendering() { Rec("WaitAndFinishRendering"); }
	void cRecordingLowLevelGraphics::SwapBuffers() { Rec("SwapBuffers"); }

	void cRecordingLowLevelGraphics::CopyFrameBufferToTexure(iTexture* apTex, const cVector2l& avPos, const cVector2l& avSize,
															 const cVector2l& avTexOffset)
	{
		Rec("CopyFrameBufferToTexure", apTex, avPos, avSize, avTexOffset);
	}

	cBitmap* cRecordingLowLevelGraphics::CopyFrameBufferToBitmap(const cVector2l& avScreenPos, const cVector2l& avScreenSize)
	{
		Rec("CopyFrameBufferToBitmap", avScreenPos, avScreenSize);
		cVector2l vSize = avScreenSize.x < 0 ? mvScreenSize : avScreenSize;
		cBitmap* pBmp = new cBitmap();
		pBmp->CreateData(cVector3l(vSize.x, vSize.y, 1), ePixelFormat_RGBA, 0, 0);
		pBmp->Clear(cColor(0, 0, 0, 1), 0, 0);
		return pBmp;
	}

	void cRecordingLowLevelGraphics::SetCurrentFrameBuffer(iFrameBuffer* apFrameBuffer, const cVector2l& avPos,
														   const cVector2l& avSize)
	{
		Rec("SetCurrentFrameBuffer", apFrameBuffer, avPos, avSize);
		mpCurrentFrameBuffer = apFrameBuffer;
	}

	iFrameBuffer* cRecordingLowLevelGraphics::GetCurrentFrameBuffer()
	{
		RecR("GetCurrentFrameBuffer", mpCurrentFrameBuffer);
		return mpCurrentFrameBuffer;
	}

	void cRecordingLowLevelGraphics::SetFrameBufferDrawTargets(int* apTargets, int alNumOfTargets)
	{
		std::vector<int> v(apTargets, apTargets + (alNumOfTargets > 0 ? alNumOfTargets : 0));
		Rec("SetFrameBufferDrawTargets", v, alNumOfTargets);
	}

	void cRecordingLowLevelGraphics::SetColorWriteActive(bool abR, bool abG, bool abB, bool abA) { Rec("SetColorWriteActive", abR, abG, abB, abA); }
	void cRecordingLowLevelGraphics::SetDepthWriteActive(bool abX) { Rec("SetDepthWriteActive", abX); }
	void cRecordingLowLevelGraphics::SetCullActive(bool abX) { Rec("SetCullActive", abX); }
	void cRecordingLowLevelGraphics::SetCullMode(eCullMode aMode) { Rec("SetCullMode", aMode); }
	void cRecordingLowLevelGraphics::SetDepthTestActive(bool abX) { Rec("SetDepthTestActive", abX); }
	void cRecordingLowLevelGraphics::SetDepthTestFunc(eDepthTestFunc aFunc) { Rec("SetDepthTestFunc", aFunc); }
	void cRecordingLowLevelGraphics::SetAlphaTestActive(bool abX) { Rec("SetAlphaTestActive", abX); }
	void cRecordingLowLevelGraphics::SetAlphaTestFunc(eAlphaTestFunc aFunc, float afRef) { Rec("SetAlphaTestFunc", aFunc, afRef); }
	void cRecordingLowLevelGraphics::SetStencilActive(bool abX) { Rec("SetStencilActive", abX); }
	void cRecordingLowLevelGraphics::SetStencilWriteMask(unsigned int alMask) { Rec("SetStencilWriteMask", alMask); }

	void cRecordingLowLevelGraphics::SetStencil(eStencilFunc aFunc, int alRef, unsigned int aMask, eStencilOp aFailOp,
												eStencilOp aZFailOp, eStencilOp aZPassOp)
	{
		Rec("SetStencil", aFunc, alRef, aMask, aFailOp, aZFailOp, aZPassOp);
	}

	void cRecordingLowLevelGraphics::SetStencilTwoSide(eStencilFunc aFrontFunc, eStencilFunc aBackFunc, int alRef,
													   unsigned int aMask, eStencilOp aFrontFailOp, eStencilOp aFrontZFailOp,
													   eStencilOp aFrontZPassOp, eStencilOp aBackFailOp,
													   eStencilOp aBackZFailOp, eStencilOp aBackZPassOp)
	{
		Rec("SetStencilTwoSide", aFrontFunc, aBackFunc, alRef, aMask, aFrontFailOp, aFrontZFailOp, aFrontZPassOp, aBackFailOp,
			aBackZFailOp, aBackZPassOp);
	}

	void cRecordingLowLevelGraphics::SetScissorActive(bool abX) { Rec("SetScissorActive", abX); }
	void cRecordingLowLevelGraphics::SetScissorRect(const cVector2l& avPos, const cVector2l& avSize) { Rec("SetScissorRect", avPos, avSize); }

	void cRecordingLowLevelGraphics::SetClipPlane(int alIdx, const cPlanef& aPlane)
	{
		Rec("SetClipPlane", alIdx, aPlane);
		if(alIdx >= 0 && alIdx < 8) mClipPlanes[alIdx] = aPlane;
	}

	cPlanef cRecordingLowLevelGraphics::GetClipPlane(int alIdx)
	{
		cPlanef p = (alIdx >= 0 && alIdx < 8) ? mClipPlanes[alIdx] : cPlanef();
		RecR("GetClipPlane", p, alIdx);
		return p;
	}

	void cRecordingLowLevelGraphics::SetClipPlaneActive(int alIdx, bool abX) { Rec("SetClipPlaneActive", alIdx, abX); }
	void cRecordingLowLevelGraphics::SetColor(const cColor& aColor) { Rec("SetColor", aColor); }
	void cRecordingLowLevelGraphics::SetBlendActive(bool abX) { Rec("SetBlendActive", abX); }
	void cRecordingLowLevelGraphics::SetBlendFunc(eBlendFunc aSrcFactor, eBlendFunc aDestFactor) { Rec("SetBlendFunc", aSrcFactor, aDestFactor); }

	void cRecordingLowLevelGraphics::SetBlendFuncSeparate(eBlendFunc aSrcFactorColor, eBlendFunc aDestFactorColor,
														  eBlendFunc aSrcFactorAlpha, eBlendFunc aDestFactorAlpha)
	{
		Rec("SetBlendFuncSeparate", aSrcFactorColor, aDestFactorColor, aSrcFactorAlpha, aDestFactorAlpha);
	}

	void cRecordingLowLevelGraphics::SetPolygonOffsetActive(bool abX) { Rec("SetPolygonOffsetActive", abX); }
	void cRecordingLowLevelGraphics::SetDepthClampActive(bool abX) { Rec("SetDepthClampActive", abX); }
	void cRecordingLowLevelGraphics::SetPolygonOffset(float afBias, float afSlopeScaleBias) { Rec("SetPolygonOffset", afBias, afSlopeScaleBias); }

	void cRecordingLowLevelGraphics::PushMatrix(eMatrix aMtxType) { Rec("PushMatrix", aMtxType); }
	void cRecordingLowLevelGraphics::PopMatrix(eMatrix aMtxType) { Rec("PopMatrix", aMtxType); }
	void cRecordingLowLevelGraphics::SetIdentityMatrix(eMatrix aMtxType) { Rec("SetIdentityMatrix", aMtxType); }
	void cRecordingLowLevelGraphics::SetMatrix(eMatrix aMtxType, const cMatrixf& a_mtxA) { Rec("SetMatrix", aMtxType, a_mtxA); }
	void cRecordingLowLevelGraphics::SetOrthoProjection(const cVector2f& avSize, float afMin, float afMax) { Rec("SetOrthoProjection", avSize, afMin, afMax); }
	void cRecordingLowLevelGraphics::SetOrthoProjection(const cVector3f& avMin, const cVector3f& avMax) { Rec("SetOrthoProjection", avMin, avMax); }

	void cRecordingLowLevelGraphics::SetTexture(unsigned int alUnit, iTexture* apTex) { Rec("SetTexture", alUnit, apTex); }
	void cRecordingLowLevelGraphics::SetActiveTextureUnit(unsigned int alUnit) { Rec("SetActiveTextureUnit", alUnit); }
	void cRecordingLowLevelGraphics::SetTextureEnv(eTextureParam aParam, int alVal) { Rec("SetTextureEnv", aParam, alVal); }
	void cRecordingLowLevelGraphics::SetTextureConstantColor(const cColor& aColor) { Rec("SetTextureConstantColor", aColor); }

	void cRecordingLowLevelGraphics::DrawTriangle(tVertexVec& avVtx) { Rec("DrawTriangle", avVtx); }

	void cRecordingLowLevelGraphics::DrawQuad(const cVector3f& avPos, const cVector2f& avSize, const cColor& aColor)
	{
		Rec("DrawQuad", avPos, avSize, aColor);
	}

	void cRecordingLowLevelGraphics::DrawQuad(const cVector3f& avPos, const cVector2f& avSize, const cVector2f& avMinTexCoord,
											  const cVector2f& avMaxTexCoord, const cColor& aColor)
	{
		Rec("DrawQuad", avPos, avSize, avMinTexCoord, avMaxTexCoord, aColor);
	}

	void cRecordingLowLevelGraphics::DrawQuad(const cVector3f& avPos, const cVector2f& avSize, const cVector2f& avMinTexCoord0,
											  const cVector2f& avMaxTexCoord0, const cVector2f& avMinTexCoord1,
											  const cVector2f& avMaxTexCoord1, const cColor& aColor)
	{
		Rec("DrawQuad", avPos, avSize, avMinTexCoord0, avMaxTexCoord0, avMinTexCoord1, avMaxTexCoord1, aColor);
	}

	void cRecordingLowLevelGraphics::DrawQuad(const tVertexVec& avVtx) { Rec("DrawQuad", avVtx); }
	void cRecordingLowLevelGraphics::DrawQuad(const tVertexVec& avVtx, const cColor aCol) { Rec("DrawQuad", avVtx, aCol); }
	void cRecordingLowLevelGraphics::DrawQuad(const tVertexVec& avVtx, const float afZ) { Rec("DrawQuad", avVtx, afZ); }
	void cRecordingLowLevelGraphics::DrawQuad(const tVertexVec& avVtx, const float afZ, const cColor& aCol) { Rec("DrawQuad", avVtx, afZ, aCol); }
	void cRecordingLowLevelGraphics::DrawQuadMultiTex(const tVertexVec& avVtx, const tVector3fVec& avExtraUvs) { Rec("DrawQuadMultiTex", avVtx, avExtraUvs); }
	void cRecordingLowLevelGraphics::DrawLine(const cVector3f& avBegin, const cVector3f& avEnd, cColor aCol) { Rec("DrawLine", avBegin, avEnd, aCol); }

	void cRecordingLowLevelGraphics::DrawLine(const cVector3f& avBegin, const cColor& aBeginCol, const cVector3f& avEnd,
											  const cColor& aEndCol)
	{
		Rec("DrawLine", avBegin, aBeginCol, avEnd, aEndCol);
	}

	void cRecordingLowLevelGraphics::DrawBoxMinMax(const cVector3f& avMin, const cVector3f& avMax, cColor aCol) { Rec("DrawBoxMinMax", avMin, avMax, aCol); }
	void cRecordingLowLevelGraphics::DrawSphere(const cVector3f& avPos, float afRadius, cColor aCol) { Rec("DrawSphere", avPos, afRadius, aCol); }

	void cRecordingLowLevelGraphics::DrawSphere(const cVector3f& avPos, float afRadius, cColor aColX, cColor aColY, cColor aColZ)
	{
		Rec("DrawSphere", avPos, afRadius, aColX, aColY, aColZ);
	}

	void cRecordingLowLevelGraphics::DrawLineQuad(const cRect2f& aRect, float afZ, cColor aCol) { Rec("DrawLineQuad", aRect, afZ, aCol); }
	void cRecordingLowLevelGraphics::DrawLineQuad(const cVector3f& avPos, const cVector2f& avSize, cColor aCol) { Rec("DrawLineQuad", avPos, avSize, aCol); }

	void cRecordingLowLevelGraphics::AddVertexToBatch(const cVertex* apVtx) { Rec("AddVertexToBatch", *apVtx); }
	void cRecordingLowLevelGraphics::AddVertexToBatch(const cVertex* apVtx, const cVector3f* avTransform) { Rec("AddVertexToBatch", *apVtx, *avTransform); }
	void cRecordingLowLevelGraphics::AddVertexToBatch(const cVertex* apVtx, const cMatrixf* aMtx) { Rec("AddVertexToBatch", *apVtx, *aMtx); }

	void cRecordingLowLevelGraphics::AddVertexToBatch_Size2D(const cVertex* apVtx, const cVector3f* avTransform,
															 const cColor* apCol, const float& mfW, const float& mfH)
	{
		Rec("AddVertexToBatch_Size2D", *apVtx, *avTransform, apCol ? *apCol : cColor(0, 0), mfW, mfH);
	}

	void cRecordingLowLevelGraphics::AddVertexToBatch_Raw(const cVector3f& avPos, const cColor& aColor, const cVector3f& avTex)
	{
		Rec("AddVertexToBatch_Raw", avPos, aColor, avTex);
	}

	void cRecordingLowLevelGraphics::AddIndexToBatch(int alIndex) { Rec("AddIndexToBatch", alIndex); }
	void cRecordingLowLevelGraphics::AddTexCoordToBatch(unsigned int alUnit, const cVector3f* apCoord) { Rec("AddTexCoordToBatch", alUnit, *apCoord); }
	void cRecordingLowLevelGraphics::SetBatchTextureUnitActive(unsigned int alUnit, bool abActive) { Rec("SetBatchTextureUnitActive", alUnit, abActive); }
	void cRecordingLowLevelGraphics::FlushTriBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear) { Rec("FlushTriBatch", aTypeFlags, abAutoClear); }
	void cRecordingLowLevelGraphics::FlushQuadBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear) { Rec("FlushQuadBatch", aTypeFlags, abAutoClear); }
	void cRecordingLowLevelGraphics::ClearBatch() { Rec("ClearBatch"); }

	//////////////////////////////////////////////////////////////////////////
	// TEXTURE
	//////////////////////////////////////////////////////////////////////////

	cRecordingTexture::cRecordingTexture(cRecordingLowLevelGraphics* apLlg, const tString& asName, eTextureType aType,
										 eTextureUsage aUsage)
		: iTexture(asName, _W(""), aType, aUsage, apLlg), mpTrace(&apLlg->Trace()), mfTimeCount(0), mlFrames(1), mlHandle(0)
	{
		msLabel = mpTrace->NewLabel(static_cast<iTexture*>(this), "tex");
		mlHandle = atoi(msLabel.c_str() + 4) + 1;
	}

	cRecordingTexture::~cRecordingTexture()
	{
		Rec("~");
		mpTrace->Forget(static_cast<iTexture*>(this));
	}

	void cRecordingTexture::RecBitmap(const char* asFn, cBitmap* apBmp)
	{
		std::string s;
		cTraceWriter w;
		w.Put(apBmp->GetSize());
		w.Put(apBmp->GetPixelFormat());
		w.Put(apBmp->GetNumOfImages());
		w.Put(apBmp->GetNumOfMipMaps());
		for(int i = 0; i < apBmp->GetNumOfImages(); ++i)
			for(int m = 0; m < apBmp->GetNumOfMipMaps(); ++m)
			{
				cBitmapData* d = apBmp->GetData(i, m);
				if(d && d->mpData) w.PutHash("img", d->mpData, d->mlSize);
			}
		Rec(asFn, w.Text());
	}

	bool cRecordingTexture::CreateFromBitmap(cBitmap* pBmp)
	{
		RecBitmap("CreateFromBitmap", pBmp);
		mvSize = pBmp->GetSize();
		mPixelFormat = pBmp->GetPixelFormat();
		return true;
	}

	bool cRecordingTexture::CreateAnimFromBitmapVec(std::vector<cBitmap*>* avBitmaps)
	{
		Rec("CreateAnimFromBitmapVec", (int)avBitmaps->size());
		for(cBitmap* b : *avBitmaps) RecBitmap("  frame", b);
		if(!avBitmaps->empty()) { mvSize = (*avBitmaps)[0]->GetSize(); mPixelFormat = (*avBitmaps)[0]->GetPixelFormat(); }
		mlFrames = (int)avBitmaps->size();
		return true;
	}

	bool cRecordingTexture::CreateCubeFromBitmapVec(std::vector<cBitmap*>* avBitmaps)
	{
		Rec("CreateCubeFromBitmapVec", (int)avBitmaps->size());
		for(cBitmap* b : *avBitmaps) RecBitmap("  face", b);
		if(!avBitmaps->empty()) { mvSize = (*avBitmaps)[0]->GetSize(); mPixelFormat = (*avBitmaps)[0]->GetPixelFormat(); }
		return true;
	}

	bool cRecordingTexture::CreateFromRawData(const cVector3l& avSize, ePixelFormat aPixelFormat, unsigned char* apData)
	{
		cTraceWriter w;
		if(apData)
			w.PutHash("raw", apData, (size_t)avSize.x * avSize.y * avSize.z * GetBytesPerPixel(aPixelFormat));
		else
			w.Put("null");
		Rec("CreateFromRawData", avSize, aPixelFormat, w.Text());
		mvSize = avSize;
		mPixelFormat = aPixelFormat;
		return true;
	}

	void cRecordingTexture::SetRawData(int alLevel, const cVector3l& avOffset, const cVector3l& avSize,
									   ePixelFormat aPixelFormat, void* apData)
	{
		cTraceWriter w;
		if(apData)
			w.PutHash("raw", apData, (size_t)avSize.x * avSize.y * avSize.z * GetBytesPerPixel(aPixelFormat));
		else
			w.Put("null");
		Rec("SetRawData", alLevel, avOffset, avSize, aPixelFormat, w.Text());
	}

	void cRecordingTexture::Update(float afTimeStep) { Rec("Update", afTimeStep); }
	void cRecordingTexture::SetFilter(eTextureFilter aFilter) { Rec("SetFilter", aFilter); mFilter = aFilter; }
	void cRecordingTexture::SetAnisotropyDegree(float afX) { Rec("SetAnisotropyDegree", afX); mfAnisotropyDegree = afX; }
	void cRecordingTexture::SetWrapS(eTextureWrap aMode) { Rec("SetWrapS", aMode); mWrapS = aMode; }
	void cRecordingTexture::SetWrapT(eTextureWrap aMode) { Rec("SetWrapT", aMode); mWrapT = aMode; }
	void cRecordingTexture::SetWrapR(eTextureWrap aMode) { Rec("SetWrapR", aMode); mWrapR = aMode; }
	void cRecordingTexture::SetWrapSTR(eTextureWrap aMode) { Rec("SetWrapSTR", aMode); mWrapS = mWrapT = mWrapR = aMode; }
	void cRecordingTexture::SetCompareMode(eTextureCompareMode aMode) { Rec("SetCompareMode", aMode); mCompareMode = aMode; }
	void cRecordingTexture::SetCompareFunc(eTextureCompareFunc aFunc) { Rec("SetCompareFunc", aFunc); mCompareFunc = aFunc; }
	void cRecordingTexture::SetRedFromAlpha(bool abX) { Rec("SetRedFromAlpha", abX); }
	void cRecordingTexture::AutoGenerateMipmaps() { Rec("AutoGenerateMipmaps"); }
	bool cRecordingTexture::HasAnimation() { return mlFrames > 1; }
	void cRecordingTexture::NextFrame() { Rec("NextFrame"); }
	void cRecordingTexture::PrevFrame() { Rec("PrevFrame"); }
	float cRecordingTexture::GetT() { return 0; }
	float cRecordingTexture::GetTimeCount() { return mfTimeCount; }
	void cRecordingTexture::SetTimeCount(float afX) { Rec("SetTimeCount", afX); mfTimeCount = afX; }
	int cRecordingTexture::GetCurrentLowlevelHandle() { return mlHandle; }

	bool cRecordingTexture::GetRawPixelsRGBAFloat(std::vector<float>& avOut)
	{
		Rec("GetRawPixelsRGBAFloat");
		avOut.assign((size_t)mvSize.x * mvSize.y * 4, 0.0f);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// VERTEX BUFFER
	//////////////////////////////////////////////////////////////////////////

	cRecordingVertexBuffer::cRecordingVertexBuffer(cRecordingLowLevelGraphics* apLlg, eVertexBufferType aType,
												   eVertexBufferDrawType aDrawType, eVertexBufferUsageType aUsageType,
												   int alReserveVtxSize, int alReserveIdxSize)
		: iVertexBuffer(apLlg, aType, aDrawType, aUsageType, alReserveVtxSize, alReserveIdxSize), mpLlg(apLlg),
		  mpTrace(&apLlg->Trace())
	{
		mbTangents = false;
		msLabel = mpTrace->NewLabel(static_cast<iVertexBuffer*>(this), "vb");
	}

	cRecordingVertexBuffer::~cRecordingVertexBuffer()
	{
		Rec("~");
		mpTrace->Forget(static_cast<iVertexBuffer*>(this));
	}

	size_t cRecordingVertexBuffer::ArraySize(const cArray& aArr) const
	{
		size_t n = aArr.mFormat == eVertexBufferElementFormat_Float ? aArr.mvFloat.size()
				 : aArr.mFormat == eVertexBufferElementFormat_Int ? aArr.mvInt.size()
				 : aArr.mvByte.size();
		return aArr.mlElementNum > 0 ? n / aArr.mlElementNum : 0;
	}

	void cRecordingVertexBuffer::CreateElementArray(eVertexBufferElement aType, eVertexBufferElementFormat aFormat,
													int alElementNum, int alProgramVarIndex)
	{
		Rec("CreateElementArray", aType, aFormat, alElementNum, alProgramVarIndex);
		cArray& a = mArrays[(int)aType];
		a.mFormat = aFormat;
		a.mlElementNum = alElementNum;
		a.mlProgramVar = alProgramVarIndex;
		mVertexFlags |= GetVertexElementFlagFromEnum(aType);
	}

	void cRecordingVertexBuffer::AddVertexVec3f(eVertexBufferElement aElement, const cVector3f& avVtx)
	{
		Rec("AddVertexVec3f", aElement, avVtx);
		cArray& a = mArrays[(int)aElement];
		a.mvFloat.push_back(avVtx.x);
		a.mvFloat.push_back(avVtx.y);
		a.mvFloat.push_back(avVtx.z);
		for(int i = 3; i < a.mlElementNum; ++i) a.mvFloat.push_back(0);
	}

	void cRecordingVertexBuffer::AddVertexVec4f(eVertexBufferElement aElement, const cVector3f& avVtx, float afW)
	{
		Rec("AddVertexVec4f", aElement, avVtx, afW);
		cArray& a = mArrays[(int)aElement];
		a.mvFloat.push_back(avVtx.x);
		a.mvFloat.push_back(avVtx.y);
		a.mvFloat.push_back(avVtx.z);
		a.mvFloat.push_back(afW);
	}

	void cRecordingVertexBuffer::AddVertexColor(eVertexBufferElement aElement, const cColor& aColor)
	{
		Rec("AddVertexColor", aElement, aColor);
		cArray& a = mArrays[(int)aElement];
		for(int i = 0; i < 4; ++i)
		{
			if(a.mFormat == eVertexBufferElementFormat_Byte)
				a.mvByte.push_back((unsigned char)(aColor.v[i] * 255.0f + 0.5f));
			else
				a.mvFloat.push_back(aColor.v[i]);
		}
	}

	void cRecordingVertexBuffer::AddIndex(unsigned int alIndex)
	{
		Rec("AddIndex", alIndex);
		mvIndices.push_back(alIndex);
	}

	bool cRecordingVertexBuffer::Compile(tVertexCompileFlag aFlags)
	{
		cTraceWriter w;
		for(auto& kv : mArrays)
		{
			w.Put(kv.first);
			w.PutHash("f", kv.second.mvFloat.data(), kv.second.mvFloat.size() * sizeof(float));
			w.PutHash("i", kv.second.mvInt.data(), kv.second.mvInt.size() * sizeof(int));
			w.PutHash("b", kv.second.mvByte.data(), kv.second.mvByte.size());
		}
		w.PutHash("idx", mvIndices.data(), mvIndices.size() * sizeof(unsigned int));
		Rec("Compile", aFlags, w.Text());
		return true;
	}

	void cRecordingVertexBuffer::UpdateData(tVertexElementFlag aTypes, bool abIndices)
	{
		cTraceWriter w;
		for(auto& kv : mArrays)
			if(GetVertexElementFlagFromEnum((eVertexBufferElement)kv.first) & aTypes)
			{
				w.Put(kv.first);
				w.PutHash("f", kv.second.mvFloat.data(), kv.second.mvFloat.size() * sizeof(float));
				w.PutHash("b", kv.second.mvByte.data(), kv.second.mvByte.size());
			}
		if(abIndices) w.PutHash("idx", mvIndices.data(), mvIndices.size() * sizeof(unsigned int));
		Rec("UpdateData", aTypes, abIndices, w.Text());
	}

	void cRecordingVertexBuffer::CreateShadowDouble(bool abUpdateData) { Rec("CreateShadowDouble", abUpdateData); }

	void cRecordingVertexBuffer::Transform(const cMatrixf& mtxTransform)
	{
		Rec("Transform", mtxTransform);
		auto it = mArrays.find((int)eVertexBufferElement_Position);
		if(it == mArrays.end() || it->second.mlElementNum < 3) return;
		cArray& a = it->second;
		for(size_t i = 0; i + a.mlElementNum <= a.mvFloat.size(); i += a.mlElementNum)
		{
			cVector3f v = cMath::MatrixMul(mtxTransform, cVector3f(a.mvFloat[i], a.mvFloat[i + 1], a.mvFloat[i + 2]));
			a.mvFloat[i] = v.x; a.mvFloat[i + 1] = v.y; a.mvFloat[i + 2] = v.z;
		}
	}

	void cRecordingVertexBuffer::Draw(eVertexBufferDrawType aDrawType) { Rec("Draw", aDrawType); }

	void cRecordingVertexBuffer::DrawIndices(unsigned int* apIndices, int alCount, eVertexBufferDrawType aDrawType)
	{
		cTraceWriter w;
		w.PutHash("idx", apIndices, (size_t)(alCount > 0 ? alCount : 0) * sizeof(unsigned int));
		Rec("DrawIndices", w.Text(), alCount, aDrawType);
	}

	void cRecordingVertexBuffer::Bind() { Rec("Bind"); }
	void cRecordingVertexBuffer::UnBind() { Rec("UnBind"); }

	iVertexBuffer* cRecordingVertexBuffer::CreateCopy(eVertexBufferType aType, eVertexBufferUsageType aUsageType,
													  tVertexElementFlag alVtxToCopy)
	{
		cRecordingVertexBuffer* p = new cRecordingVertexBuffer(mpLlg, aType, mDrawType, aUsageType, mlReservedVtxSize, mlReservedIdxSize);
		for(auto& kv : mArrays)
			if(GetVertexElementFlagFromEnum((eVertexBufferElement)kv.first) & alVtxToCopy)
			{
				p->mArrays[kv.first] = kv.second;
				p->mVertexFlags |= GetVertexElementFlagFromEnum((eVertexBufferElement)kv.first);
			}
		p->mvIndices = mvIndices;
		Rec("CreateCopy", aType, aUsageType, alVtxToCopy, static_cast<iVertexBuffer*>(p));
		return p;
	}

	cBoundingVolume cRecordingVertexBuffer::CreateBoundingVolume()
	{
		Rec("CreateBoundingVolume");
		cBoundingVolume bv;
		auto it = mArrays.find((int)eVertexBufferElement_Position);
		if(it == mArrays.end() || it->second.mlElementNum < 3 || it->second.mvFloat.empty()) return bv;
		const cArray& a = it->second;
		cVector3f mn(a.mvFloat[0], a.mvFloat[1], a.mvFloat[2]), mx = mn;
		for(size_t i = 0; i + a.mlElementNum <= a.mvFloat.size(); i += a.mlElementNum)
			for(int c = 0; c < 3; ++c)
			{
				if(a.mvFloat[i + c] < mn.v[c]) mn.v[c] = a.mvFloat[i + c];
				if(a.mvFloat[i + c] > mx.v[c]) mx.v[c] = a.mvFloat[i + c];
			}
		bv.SetLocalMinMax(mn, mx);
		return bv;
	}

	int cRecordingVertexBuffer::GetElementNum(eVertexBufferElement aElement)
	{
		auto it = mArrays.find((int)aElement);
		return it == mArrays.end() ? 0 : it->second.mlElementNum;
	}

	eVertexBufferElementFormat cRecordingVertexBuffer::GetElementFormat(eVertexBufferElement aElement)
	{
		auto it = mArrays.find((int)aElement);
		return it == mArrays.end() ? eVertexBufferElementFormat_Float : it->second.mFormat;
	}

	int cRecordingVertexBuffer::GetElementProgramVarIndex(eVertexBufferElement aElement)
	{
		auto it = mArrays.find((int)aElement);
		return it == mArrays.end() ? 0 : it->second.mlProgramVar;
	}

	float* cRecordingVertexBuffer::GetFloatArray(eVertexBufferElement aElement)
	{
		auto it = mArrays.find((int)aElement);
		return it == mArrays.end() ? NULL : it->second.mvFloat.data();
	}

	int* cRecordingVertexBuffer::GetIntArray(eVertexBufferElement aElement)
	{
		auto it = mArrays.find((int)aElement);
		return it == mArrays.end() ? NULL : it->second.mvInt.data();
	}

	unsigned char* cRecordingVertexBuffer::GetByteArray(eVertexBufferElement aElement)
	{
		auto it = mArrays.find((int)aElement);
		return it == mArrays.end() ? NULL : it->second.mvByte.data();
	}

	unsigned int* cRecordingVertexBuffer::GetIndices() { return mvIndices.data(); }

	int cRecordingVertexBuffer::GetVertexNum()
	{
		auto it = mArrays.find((int)eVertexBufferElement_Position);
		if(it != mArrays.end()) return (int)ArraySize(it->second);
		return mArrays.empty() ? 0 : (int)ArraySize(mArrays.begin()->second);
	}

	int cRecordingVertexBuffer::GetIndexNum() { return (int)mvIndices.size(); }

	void cRecordingVertexBuffer::ResizeArray(eVertexBufferElement aElement, int alSize)
	{
		Rec("ResizeArray", aElement, alSize);
		cArray& a = mArrays[(int)aElement];
		size_t n = alSize > 0 ? (size_t)alSize : 0;
		a.mvFloat.resize(a.mFormat == eVertexBufferElementFormat_Float ? n : 0);
		a.mvInt.resize(a.mFormat == eVertexBufferElementFormat_Int ? n : 0);
		a.mvByte.resize(a.mFormat == eVertexBufferElementFormat_Byte ? n : 0);
	}

	void cRecordingVertexBuffer::ResizeIndices(int alSize)
	{
		Rec("ResizeIndices", alSize);
		mvIndices.resize(alSize > 0 ? (size_t)alSize : 0);
	}

	//////////////////////////////////////////////////////////////////////////
	// SHADER / PROGRAM
	//////////////////////////////////////////////////////////////////////////

	cRecordingGpuShader::cRecordingGpuShader(cRecordingLowLevelGraphics* apLlg, const tString& asName, eGpuShaderType aType)
		: iGpuShader(asName, _W(""), aType, apLlg->GetGpuProgramFormat()), mpTrace(&apLlg->Trace())
	{
		mlUserId = 0;
		msLabel = mpTrace->NewLabel(static_cast<iGpuShader*>(this), "shader");
	}

	cRecordingGpuShader::~cRecordingGpuShader()
	{
		mpTrace->Call(msLabel + ".~");
		mpTrace->Forget(static_cast<iGpuShader*>(this));
	}

	bool cRecordingGpuShader::SamplerNeedsTextureUnitSetup() { return false; }

	bool cRecordingGpuShader::CreateFromFile(const tWString& asFile, const tString& asEntry, bool abPrintInfoIfFail)
	{
		mpTrace->CallR(msLabel + ".CreateFromFile", true, asFile, asEntry, abPrintInfoIfFail);
		return true;
	}

	bool cRecordingGpuShader::CreateFromString(const char* apStringData, const tString& asEntry, bool abPrintInfoIfFail)
	{
		mpTrace->CallR(msLabel + ".CreateFromString", true, apStringData, asEntry, abPrintInfoIfFail);
		return true;
	}

	cRecordingGpuProgram::cRecordingGpuProgram(cRecordingLowLevelGraphics* apLlg, const tString& asName,
											   eGpuProgramFormat aFormat)
		: iGpuProgram(asName, aFormat), mpTrace(&apLlg->Trace())
	{
		msLabel = mpTrace->NewLabel(static_cast<iGpuProgram*>(this), "prog");
	}

	cRecordingGpuProgram::~cRecordingGpuProgram()
	{
		Rec("~");
		mpTrace->Forget(static_cast<iGpuProgram*>(this));
	}

	bool cRecordingGpuProgram::Link()
	{
		Rec("Link", mpShader[0], mpShader[1]);
		return true;
	}

	tString cRecordingGpuProgram::DumpUniforms() { Rec("DumpUniforms"); return ""; }
	void cRecordingGpuProgram::Bind() { Rec("Bind"); }
	void cRecordingGpuProgram::UnBind() { Rec("UnBind"); }
	bool cRecordingGpuProgram::CanAccessAPIMatrix() { return true; }

	bool cRecordingGpuProgram::SetSamplerToUnit(const tString& asSamplerName, int alUnit)
	{
		Rec("SetSamplerToUnit", asSamplerName, alUnit);
		return true;
	}

	int cRecordingGpuProgram::GetVariableId(const tString& asName)
	{
		auto it = mVariableIds.find(asName);
		int id;
		if(it == mVariableIds.end())
		{
			id = (int)mVariableIds.size();
			mVariableIds[asName] = id;
		}
		else
			id = it->second;
		mpTrace->CallR(msLabel + ".GetVariableId", id, asName);
		return id;
	}

	bool cRecordingGpuProgram::GetVariableAsId(const tString& asName, int alId)
	{
		Rec("GetVariableAsId", asName, alId);
		mVariableIds[asName] = alId;
		return true;
	}

	bool cRecordingGpuProgram::SetInt(int alVarId, int alX) { Rec("SetInt", alVarId, alX); return true; }
	bool cRecordingGpuProgram::SetFloat(int alVarId, float afX) { Rec("SetFloat", alVarId, afX); return true; }
	bool cRecordingGpuProgram::SetVec2f(int alVarId, float afX, float afY) { Rec("SetVec2f", alVarId, afX, afY); return true; }
	bool cRecordingGpuProgram::SetVec3f(int alVarId, float afX, float afY, float afZ) { Rec("SetVec3f", alVarId, afX, afY, afZ); return true; }
	bool cRecordingGpuProgram::SetVec4f(int alVarId, float afX, float afY, float afZ, float afW) { Rec("SetVec4f", alVarId, afX, afY, afZ, afW); return true; }

	bool cRecordingGpuProgram::SetFloatArray(int alVarId, const float* apX, int alCount)
	{
		cTraceWriter w;
		for(int i = 0; i < alCount; ++i) w.Put(apX[i]);
		Rec("SetFloatArray", alVarId, alCount, w.Text());
		return true;
	}

	bool cRecordingGpuProgram::SetMatrixf(int alVarId, const cMatrixf& mMtx) { Rec("SetMatrixf", alVarId, mMtx); return true; }

	bool cRecordingGpuProgram::SetMatrixf(int alVarId, eGpuShaderMatrix mType, eGpuShaderMatrixOp mOp)
	{
		Rec("SetMatrixfApi", alVarId, mType, mOp);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// FRAME BUFFER / DEPTH STENCIL / QUERY / FONT
	//////////////////////////////////////////////////////////////////////////

	cRecordingDepthStencilBuffer::cRecordingDepthStencilBuffer(cRecordingLowLevelGraphics* apLlg, const cVector2l& avSize,
															   int alDepthBits, int alStencilBits)
		: iDepthStencilBuffer(avSize, alDepthBits, alStencilBits), mpTrace(&apLlg->Trace())
	{
		msLabel = mpTrace->NewLabel(static_cast<iDepthStencilBuffer*>(this), "dsb");
	}

	cRecordingDepthStencilBuffer::~cRecordingDepthStencilBuffer()
	{
		mpTrace->Call(msLabel + ".~");
		mpTrace->Forget(static_cast<iDepthStencilBuffer*>(this));
	}

	cRecordingFrameBuffer::cRecordingFrameBuffer(cRecordingLowLevelGraphics* apLlg, const tString& asName)
		: iFrameBuffer(asName, apLlg), mpLlg(apLlg), mpTrace(&apLlg->Trace())
	{
		msLabel = mpTrace->NewLabel(static_cast<iFrameBuffer*>(this), "fb");
	}

	cRecordingFrameBuffer::~cRecordingFrameBuffer()
	{
		Rec("~");
		mpTrace->Forget(static_cast<iFrameBuffer*>(this));
	}

	void cRecordingFrameBuffer::SetTexture2D(int alColorIdx, iTexture* apTexture, int alMipmapLevel)
	{
		Rec("SetTexture2D", alColorIdx, apTexture, alMipmapLevel);
		mpColorBuffer[alColorIdx] = apTexture;
		if(apTexture) mvSize = cVector2l(apTexture->GetWidth(), apTexture->GetHeight());
	}

	void cRecordingFrameBuffer::SetTexture3D(int alColorIdx, iTexture* apTexture, int alZ, int alMipmapLevel)
	{
		Rec("SetTexture3D", alColorIdx, apTexture, alZ, alMipmapLevel);
		mpColorBuffer[alColorIdx] = apTexture;
		if(apTexture) mvSize = cVector2l(apTexture->GetWidth(), apTexture->GetHeight());
	}

	void cRecordingFrameBuffer::SetTextureCubeMap(int alColorIdx, iTexture* apTexture, int alFace, int alMipmapLevel)
	{
		Rec("SetTextureCubeMap", alColorIdx, apTexture, alFace, alMipmapLevel);
		mpColorBuffer[alColorIdx] = apTexture;
		if(apTexture) mvSize = cVector2l(apTexture->GetWidth(), apTexture->GetHeight());
	}

	void cRecordingFrameBuffer::SetDepthTexture2D(iTexture* apTexture, int alMipmapLevel)
	{
		Rec("SetDepthTexture2D", apTexture, alMipmapLevel);
		mpDepthBuffer = apTexture;
	}

	void cRecordingFrameBuffer::SetDepthTextureCubeMap(iTexture* apTexture, int alFace, int alMipmapLevel)
	{
		Rec("SetDepthTextureCubeMap", apTexture, alFace, alMipmapLevel);
		mpDepthBuffer = apTexture;
	}

	void cRecordingFrameBuffer::SetDepthStencilBuffer(iDepthStencilBuffer* apBuffer)
	{
		Rec("SetDepthStencilBuffer", apBuffer);
		mpDepthBuffer = apBuffer;
		mpStencilBuffer = apBuffer;
	}

	bool cRecordingFrameBuffer::CompileAndValidate()
	{
		bool b = mpLlg->Config().mbFrameBufferCompileOk;
		mpTrace->CallR(msLabel + ".CompileAndValidate", b);
		return b;
	}

	void cRecordingFrameBuffer::PostBindUpdate() { Rec("PostBindUpdate"); }

	cRecordingOcclusionQuery::cRecordingOcclusionQuery(cRecordingLowLevelGraphics* apLlg)
		: mpLlg(apLlg), mpTrace(&apLlg->Trace()), mlSamples(0)
	{
		msLabel = mpTrace->NewLabel(static_cast<iOcclusionQuery*>(this), "oq");
	}

	cRecordingOcclusionQuery::~cRecordingOcclusionQuery()
	{
		mpTrace->Call(msLabel + ".~");
		mpTrace->Forget(static_cast<iOcclusionQuery*>(this));
	}

	void cRecordingOcclusionQuery::Begin() { mpTrace->Call(msLabel + ".Begin"); }
	void cRecordingOcclusionQuery::End() { mpTrace->Call(msLabel + ".End"); }

	bool cRecordingOcclusionQuery::FetchResults()
	{
		mlSamples = mpLlg->OcclusionSample(mpLlg->mlOcclusionFetches++);
		mpTrace->CallR(msLabel + ".FetchResults", true, mlSamples);
		return true;
	}

	unsigned int cRecordingOcclusionQuery::GetSampleCount()
	{
		mpTrace->CallR(msLabel + ".GetSampleCount", mlSamples);
		return mlSamples;
	}

	cRecordingFontData::cRecordingFontData(cRecordingLowLevelGraphics* apLlg, const tString& asName)
		: iFontData(asName, apLlg), mpTrace(&apLlg->Trace())
	{
		msLabel = mpTrace->NewLabel(static_cast<iFontData*>(this), "font");
	}

	cRecordingFontData::~cRecordingFontData()
	{
		mpTrace->Call(msLabel + ".~");
		mpTrace->Forget(static_cast<iFontData*>(this));
	}

	bool cRecordingFontData::CreateFromFontFile(const tWString& asFileName, int alSize, unsigned short alFirstChar,
												unsigned short alLastChar)
	{
		mpTrace->CallR(msLabel + ".CreateFromFontFile", true, asFileName, alSize, (int)alFirstChar, (int)alLastChar);
		return true;
	}

	bool cRecordingFontData::CreateFromBitmapFile(const tWString& asFileName)
	{
		mpTrace->CallR(msLabel + ".CreateFromBitmapFile", true, asFileName);
		return true;
	}

}
