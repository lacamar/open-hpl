/*
 * RecordingLowLevelGraphics: iLowLevelGraphics that does no GPU work and records every call.
 *
 * USAGE (differential harness for a renderer/material/post-effect/gui chunk)
 *   cRecordingConfig cfg;                      // seed, screen size, caps; same cfg for both runs
 *   cRecordingLowLevelGraphics llg(cfg);
 *   RunOriginalCppModule(&llg);                // module under test, handed &llg
 *   std::string a = llg.Trace().Text();
 *   cRecordingLowLevelGraphics llg2(cfg);
 *   RunRustBackedModule(&llg2);                // same inputs, Rust-backed path
 *   assert(a == llg2.Trace().Text());          // or diff the two (Trace().WriteFile(path))
 *
 * Trace format, one call per line: `Fn(arg, ...)` or `Fn(arg, ...) -> result` for queries.
 *   floats  f:<hex bits>        ints decimal (u/l/ul suffix for unsigned/long)
 *   objects <kind>#<n>          n = creation order per kind (tex, vb, prog, shader, fb, dsb, oq, font);
 *                               resource methods are `tex#0.SetFilter(2)`; never raw pointers
 *   blobs   tag{n=<size>,h=<fnv1a64>}  uploads (bitmap, raw data, shader source, vertex arrays)
 * Deterministic: queries (GetCaps, occlusion sample counts, program variable ids, clip planes,
 * current frame buffer) come from cRecordingConfig and a seeded table, never from the host.
 * Resources created by the recorder are owned by the caller (the module under test deletes them);
 * delete them before the recorder, or the trace stays valid but their destroy lines are lost.
 * Compare modules only on identical call sequences: caller-side state (iRenderFunctions caches)
 * is part of what is being compared.
 */
#ifndef HPL_RECORDING_LOWLEVELGRAPHICS_H
#define HPL_RECORDING_LOWLEVELGRAPHICS_H

#include <map>
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

#include "TraceWriter.h"

namespace hpl {

	class cRecordingConfig
	{
	public:
		cRecordingConfig() : mlSeed(1), mvScreenSize(1280, 720), mlOcclusionTableSize(64), mbFrameBufferCompileOk(true)
		{
			mCaps[eGraphicCaps_MaxTextureImageUnits] = 16;
			mCaps[eGraphicCaps_MaxTextureCoordUnits] = 8;
			mCaps[eGraphicCaps_MaxUserClipPlanes] = 6;
			mCaps[eGraphicCaps_MaxAnisotropicFiltering] = 16;
			mCaps[eGraphicCaps_MaxDrawBuffers] = kMaxDrawColorBuffers;
			mCaps[eGraphicCaps_MaxColorRenderTargets] = kMaxDrawColorBuffers;
		}

		unsigned long long mlSeed;
		cVector2l mvScreenSize;
		int mlOcclusionTableSize;
		bool mbFrameBufferCompileOk;
		std::map<int, int> mCaps;
		int mlDefaultCap = 1;
	};

	class cRecordingLowLevelGraphics : public iLowLevelGraphics
	{
	public:
		explicit cRecordingLowLevelGraphics(const cRecordingConfig& aCfg = cRecordingConfig());
		~cRecordingLowLevelGraphics();

		cTraceWriter& Trace() { return mTrace; }
		const cRecordingConfig& Config() const { return mCfg; }
		unsigned int OcclusionSample(int alFetchIndex) const;

		bool Init(int alWidth, int alHeight, int alDisplay, int alBpp, int abFullscreen, int alMultisampling,
				  eGpuProgramFormat aGpuProgramFormat, const tString& asWindowCaption, const cVector2l& avWindowPos);
		eGpuProgramFormat GetGpuProgramFormat();
		int GetCaps(eGraphicCaps aType);
		tString GetRendererDescription();
		void* GetWindowHandle();
		bool SetColorOutput(const tString& asMode, float afPaperWhite, float afPeakNits);
		tString GetColorOutput();
		float GetHdrHeadroom();
		bool GetScreenFloatStats(float& afMax, float& afMean, float& afFracOver1);
		void ShowCursor(bool abX);
		void SetWindowGrab(bool abX);
		void SetRelativeMouse(bool abX);
		void SetWindowCaption(const tString& asName);
		bool GetWindowMouseFocus();
		bool GetWindowInputFocus();
		bool GetWindowIsVisible();
		int GetMultisampling();
		cVector2f GetScreenSizeFloat();
		const cVector2l& GetScreenSizeInt();
		bool CheckAndUpdateScreenSize();
		bool ForceWindowSize(int alWidth, int alHeight);
		bool GetFullscreenModeActive();
		void SetVsyncActive(bool abX, bool abAdaptive = false);
		void SetMultisamplingActive(bool abX);
		void SetGammaCorrection(float afX);
		float GetGammaCorrection();

		iFontData* CreateFontData(const tString& asName);
		iGpuProgram* CreateGpuProgram(const tString& asName);
		iGpuShader* CreateGpuShader(const tString& asName, eGpuShaderType aType);
		iTexture* CreateTexture(const tString& asName, eTextureType aType, eTextureUsage aUsage);
		iVertexBuffer* CreateVertexBuffer(eVertexBufferType aType, eVertexBufferDrawType aDrawType,
										  eVertexBufferUsageType aUsageType, int alReserveVtxSize = 0, int alReserveIdxSize = 0);
		iFrameBuffer* CreateFrameBuffer(const tString& asName);
		iDepthStencilBuffer* CreateDepthStencilBuffer(const cVector2l& avSize, int alDepthBits, int alStencilBits);
		iOcclusionQuery* CreateOcclusionQuery();

		void ClearFrameBuffer(tClearFrameBufferFlag aFlags);
		void SetClearColor(const cColor& aCol);
		void SetClearDepth(float afDepth);
		void SetClearStencil(int alVal);
		void FlushRendering();
		void WaitAndFinishRendering();
		void SwapBuffers();
		void CopyFrameBufferToTexure(iTexture* apTex, const cVector2l& avPos, const cVector2l& avSize,
									 const cVector2l& avTexOffset = 0);
		cBitmap* CopyFrameBufferToBitmap(const cVector2l& avScreenPos = 0, const cVector2l& avScreenSize = -1);
		void SetCurrentFrameBuffer(iFrameBuffer* apFrameBuffer, const cVector2l& avPos = 0, const cVector2l& avSize = -1);
		iFrameBuffer* GetCurrentFrameBuffer();
		void SetFrameBufferDrawTargets(int* apTargets, int alNumOfTargets);

		void SetColorWriteActive(bool abR, bool abG, bool abB, bool abA);
		void SetDepthWriteActive(bool abX);
		void SetCullActive(bool abX);
		void SetCullMode(eCullMode aMode);
		void SetDepthTestActive(bool abX);
		void SetDepthTestFunc(eDepthTestFunc aFunc);
		void SetAlphaTestActive(bool abX);
		void SetAlphaTestFunc(eAlphaTestFunc aFunc, float afRef);
		void SetStencilActive(bool abX);
		void SetStencilWriteMask(unsigned int alMask);
		void SetStencil(eStencilFunc aFunc, int alRef, unsigned int aMask, eStencilOp aFailOp, eStencilOp aZFailOp,
						eStencilOp aZPassOp);
		void SetStencilTwoSide(eStencilFunc aFrontFunc, eStencilFunc aBackFunc, int alRef, unsigned int aMask,
							   eStencilOp aFrontFailOp, eStencilOp aFrontZFailOp, eStencilOp aFrontZPassOp,
							   eStencilOp aBackFailOp, eStencilOp aBackZFailOp, eStencilOp aBackZPassOp);
		void SetScissorActive(bool abX);
		void SetScissorRect(const cVector2l& avPos, const cVector2l& avSize);
		void SetClipPlane(int alIdx, const cPlanef& aPlane);
		cPlanef GetClipPlane(int alIdx);
		void SetClipPlaneActive(int alIdx, bool abX);
		void SetColor(const cColor& aColor);
		void SetBlendActive(bool abX);
		void SetBlendFunc(eBlendFunc aSrcFactor, eBlendFunc aDestFactor);
		void SetBlendFuncSeparate(eBlendFunc aSrcFactorColor, eBlendFunc aDestFactorColor, eBlendFunc aSrcFactorAlpha,
								  eBlendFunc aDestFactorAlpha);
		void SetPolygonOffsetActive(bool abX);
		void SetDepthClampActive(bool abX);
		void SetPolygonOffset(float afBias, float afSlopeScaleBias);

		void PushMatrix(eMatrix aMtxType);
		void PopMatrix(eMatrix aMtxType);
		void SetIdentityMatrix(eMatrix aMtxType);
		void SetMatrix(eMatrix aMtxType, const cMatrixf& a_mtxA);
		void SetOrthoProjection(const cVector2f& avSize, float afMin, float afMax);
		void SetOrthoProjection(const cVector3f& avMin, const cVector3f& avMax);

		void SetTexture(unsigned int alUnit, iTexture* apTex);
		void SetActiveTextureUnit(unsigned int alUnit);
		void SetTextureEnv(eTextureParam aParam, int alVal);
		void SetTextureConstantColor(const cColor& aColor);

		void DrawTriangle(tVertexVec& avVtx);
		void DrawQuad(const cVector3f& avPos, const cVector2f& avSize, const cColor& aColor = cColor(1, 1));
		void DrawQuad(const cVector3f& avPos, const cVector2f& avSize, const cVector2f& avMinTexCoord,
					  const cVector2f& avMaxTexCoord, const cColor& aColor = cColor(1, 1));
		void DrawQuad(const cVector3f& avPos, const cVector2f& avSize, const cVector2f& avMinTexCoord0,
					  const cVector2f& avMaxTexCoord0, const cVector2f& avMinTexCoord1, const cVector2f& avMaxTexCoord1,
					  const cColor& aColor = cColor(1, 1));
		void DrawQuad(const tVertexVec& avVtx);
		void DrawQuad(const tVertexVec& avVtx, const cColor aCol);
		void DrawQuad(const tVertexVec& avVtx, const float afZ);
		void DrawQuad(const tVertexVec& avVtx, const float afZ, const cColor& aCol);
		void DrawQuadMultiTex(const tVertexVec& avVtx, const tVector3fVec& avExtraUvs);
		void DrawLine(const cVector3f& avBegin, const cVector3f& avEnd, cColor aCol);
		void DrawLine(const cVector3f& avBegin, const cColor& aBeginCol, const cVector3f& avEnd, const cColor& aEndCol);
		void DrawBoxMinMax(const cVector3f& avMin, const cVector3f& avMax, cColor aCol);
		void DrawSphere(const cVector3f& avPos, float afRadius, cColor aCol);
		void DrawSphere(const cVector3f& avPos, float afRadius, cColor aColX, cColor aColY, cColor aColZ);
		void DrawLineQuad(const cRect2f& aRect, float afZ, cColor aCol);
		void DrawLineQuad(const cVector3f& avPos, const cVector2f& avSize, cColor aCol);

		void AddVertexToBatch(const cVertex* apVtx);
		void AddVertexToBatch(const cVertex* apVtx, const cVector3f* avTransform);
		void AddVertexToBatch(const cVertex* apVtx, const cMatrixf* aMtx);
		void AddVertexToBatch_Size2D(const cVertex* apVtx, const cVector3f* avTransform, const cColor* apCol,
									 const float& mfW, const float& mfH);
		void AddVertexToBatch_Raw(const cVector3f& avPos, const cColor& aColor, const cVector3f& avTex);
		void AddIndexToBatch(int alIndex);
		void AddTexCoordToBatch(unsigned int alUnit, const cVector3f* apCoord);
		void SetBatchTextureUnitActive(unsigned int alUnit, bool abActive);
		void FlushTriBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear = true);
		void FlushQuadBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear = true);
		void ClearBatch();

	private:
		template<class... A>
		void Rec(const char* asFn, const A&... a) { mTrace.Call(asFn, a...); }
		template<class R, class... A>
		void RecR(const char* asFn, const R& aRet, const A&... a) { mTrace.CallR(asFn, aRet, a...); }

		cRecordingConfig mCfg;
		cTraceWriter mTrace;
		std::vector<unsigned int> mOcclusionTable;
		cVector2l mvScreenSize;
		iFrameBuffer* mpCurrentFrameBuffer;
		cPlanef mClipPlanes[8];
		float mfGamma;
		int mlMultisampling;
		bool mbFullscreen;
		eGpuProgramFormat mProgramFormat;
		tString msColorOutput;
		int mlOcclusionFetches;
		friend class cRecordingOcclusionQuery;
	};

	class cRecordingTexture : public iTexture
	{
	public:
		cRecordingTexture(cRecordingLowLevelGraphics* apLlg, const tString& asName, eTextureType aType,
						  eTextureUsage aUsage);
		~cRecordingTexture();
		bool CreateFromBitmap(cBitmap* pBmp);
		bool CreateAnimFromBitmapVec(std::vector<cBitmap*>* avBitmaps);
		bool CreateCubeFromBitmapVec(std::vector<cBitmap*>* avBitmaps);
		bool CreateFromRawData(const cVector3l& avSize, ePixelFormat aPixelFormat, unsigned char* apData);
		void SetRawData(int alLevel, const cVector3l& avOffset, const cVector3l& avSize, ePixelFormat aPixelFormat,
						void* apData);
		void Update(float afTimeStep);
		void SetFilter(eTextureFilter aFilter);
		void SetAnisotropyDegree(float afX);
		void SetWrapS(eTextureWrap aMode);
		void SetWrapT(eTextureWrap aMode);
		void SetWrapR(eTextureWrap aMode);
		void SetWrapSTR(eTextureWrap aMode);
		void SetCompareMode(eTextureCompareMode aMode);
		void SetCompareFunc(eTextureCompareFunc aFunc);
		void SetRedFromAlpha(bool abX);
		void AutoGenerateMipmaps();
		bool HasAnimation();
		void NextFrame();
		void PrevFrame();
		float GetT();
		float GetTimeCount();
		void SetTimeCount(float afX);
		int GetCurrentLowlevelHandle();
		bool GetRawPixelsRGBAFloat(std::vector<float>& avOut);

	private:
		template<class... A>
		void Rec(const char* asFn, const A&... a) { mpTrace->Call(msLabel + "." + asFn, a...); }
		void RecBitmap(const char* asFn, cBitmap* apBmp);

		cTraceWriter* mpTrace;
		std::string msLabel;
		float mfTimeCount;
		int mlFrames;
		int mlHandle;
	};

	class cRecordingVertexBuffer : public iVertexBuffer
	{
	public:
		cRecordingVertexBuffer(cRecordingLowLevelGraphics* apLlg, eVertexBufferType aType,
							   eVertexBufferDrawType aDrawType, eVertexBufferUsageType aUsageType, int alReserveVtxSize,
							   int alReserveIdxSize);
		~cRecordingVertexBuffer();
		void CreateElementArray(eVertexBufferElement aType, eVertexBufferElementFormat aFormat, int alElementNum,
								int alProgramVarIndex = 0);
		void AddVertexVec3f(eVertexBufferElement aElement, const cVector3f& avVtx);
		void AddVertexVec4f(eVertexBufferElement aElement, const cVector3f& avVtx, float afW);
		void AddVertexColor(eVertexBufferElement aElement, const cColor& aColor);
		void AddIndex(unsigned int alIndex);
		bool Compile(tVertexCompileFlag aFlags);
		void UpdateData(tVertexElementFlag aTypes, bool abIndices);
		void CreateShadowDouble(bool abUpdateData);
		void Transform(const cMatrixf& mtxTransform);
		void Draw(eVertexBufferDrawType aDrawType = eVertexBufferDrawType_LastEnum);
		void DrawIndices(unsigned int* apIndices, int alCount, eVertexBufferDrawType aDrawType = eVertexBufferDrawType_LastEnum);
		void Bind();
		void UnBind();
		iVertexBuffer* CreateCopy(eVertexBufferType aType, eVertexBufferUsageType aUsageType, tVertexElementFlag alVtxToCopy);
		cBoundingVolume CreateBoundingVolume();
		int GetElementNum(eVertexBufferElement aElement);
		eVertexBufferElementFormat GetElementFormat(eVertexBufferElement aElement);
		int GetElementProgramVarIndex(eVertexBufferElement aElement);
		float* GetFloatArray(eVertexBufferElement aElement);
		int* GetIntArray(eVertexBufferElement aElement);
		unsigned char* GetByteArray(eVertexBufferElement aElement);
		unsigned int* GetIndices();
		int GetVertexNum();
		int GetIndexNum();
		void ResizeArray(eVertexBufferElement aElement, int alSize);
		void ResizeIndices(int alSize);

		using iVertexBuffer::GetElementNum;

	private:
		struct cArray
		{
			eVertexBufferElementFormat mFormat = eVertexBufferElementFormat_Float;
			int mlElementNum = 0;
			int mlProgramVar = 0;
			std::vector<float> mvFloat;
			std::vector<int> mvInt;
			std::vector<unsigned char> mvByte;
		};

		template<class... A>
		void Rec(const char* asFn, const A&... a) { mpTrace->Call(msLabel + "." + asFn, a...); }
		size_t ArraySize(const cArray& aArr) const;

		cRecordingLowLevelGraphics* mpLlg;
		cTraceWriter* mpTrace;
		std::string msLabel;
		std::map<int, cArray> mArrays;
		std::vector<unsigned int> mvIndices;
	};

	class cRecordingGpuShader : public iGpuShader
	{
	public:
		cRecordingGpuShader(cRecordingLowLevelGraphics* apLlg, const tString& asName, eGpuShaderType aType);
		~cRecordingGpuShader();
		bool Reload() { return false; }
		void Unload() {}
		void Destroy() {}
		bool SamplerNeedsTextureUnitSetup();
		bool CreateFromFile(const tWString& asFile, const tString& asEntry = "main", bool abPrintInfoIfFail = true);
		bool CreateFromString(const char* apStringData, const tString& asEntry = "main", bool abPrintInfoIfFail = true);

	private:
		cTraceWriter* mpTrace;
		std::string msLabel;
	};

	class cRecordingGpuProgram : public iGpuProgram
	{
	public:
		cRecordingGpuProgram(cRecordingLowLevelGraphics* apLlg, const tString& asName, eGpuProgramFormat aFormat);
		~cRecordingGpuProgram();
		bool Link();
		tString DumpUniforms();
		void Bind();
		void UnBind();
		bool CanAccessAPIMatrix();
		bool SetSamplerToUnit(const tString& asSamplerName, int alUnit);
		int GetVariableId(const tString& asName);
		bool GetVariableAsId(const tString& asName, int alId);
		bool SetInt(int alVarId, int alX);
		bool SetFloat(int alVarId, float afX);
		bool SetVec2f(int alVarId, float afX, float afY);
		bool SetVec3f(int alVarId, float afX, float afY, float afZ);
		bool SetVec4f(int alVarId, float afX, float afY, float afZ, float afW);
		bool SetFloatArray(int alVarId, const float* apX, int alCount);
		bool SetMatrixf(int alVarId, const cMatrixf& mMtx);
		bool SetMatrixf(int alVarId, eGpuShaderMatrix mType, eGpuShaderMatrixOp mOp);

		using iGpuProgram::SetVec2f;
		using iGpuProgram::SetVec3f;

	private:
		template<class... A>
		void Rec(const char* asFn, const A&... a) { mpTrace->Call(msLabel + "." + asFn, a...); }

		cTraceWriter* mpTrace;
		std::string msLabel;
		std::map<tString, int> mVariableIds;
	};

	class cRecordingDepthStencilBuffer : public iDepthStencilBuffer
	{
	public:
		cRecordingDepthStencilBuffer(cRecordingLowLevelGraphics* apLlg, const cVector2l& avSize, int alDepthBits,
									 int alStencilBits);
		~cRecordingDepthStencilBuffer();

	private:
		cTraceWriter* mpTrace;
		std::string msLabel;
	};

	class cRecordingFrameBuffer : public iFrameBuffer
	{
	public:
		cRecordingFrameBuffer(cRecordingLowLevelGraphics* apLlg, const tString& asName);
		~cRecordingFrameBuffer();
		void SetTexture2D(int alColorIdx, iTexture* apTexture, int alMipmapLevel = 0);
		void SetTexture3D(int alColorIdx, iTexture* apTexture, int alZ, int alMipmapLevel = 0);
		void SetTextureCubeMap(int alColorIdx, iTexture* apTexture, int alFace, int alMipmapLevel = 0);
		void SetDepthTexture2D(iTexture* apTexture, int alMipmapLevel = 0);
		void SetDepthTextureCubeMap(iTexture* apTexture, int alFace, int alMipmapLevel = 0);
		void SetDepthStencilBuffer(iDepthStencilBuffer* apBuffer);
		bool CompileAndValidate();
		void PostBindUpdate();

	private:
		template<class... A>
		void Rec(const char* asFn, const A&... a) { mpTrace->Call(msLabel + "." + asFn, a...); }

		cRecordingLowLevelGraphics* mpLlg;
		cTraceWriter* mpTrace;
		std::string msLabel;
	};

	class cRecordingOcclusionQuery : public iOcclusionQuery
	{
	public:
		explicit cRecordingOcclusionQuery(cRecordingLowLevelGraphics* apLlg);
		~cRecordingOcclusionQuery();
		void Begin();
		void End();
		bool FetchResults();
		unsigned int GetSampleCount();

	private:
		cRecordingLowLevelGraphics* mpLlg;
		cTraceWriter* mpTrace;
		std::string msLabel;
		unsigned int mlSamples;
	};

	class cRecordingFontData : public iFontData
	{
	public:
		cRecordingFontData(cRecordingLowLevelGraphics* apLlg, const tString& asName);
		~cRecordingFontData();
		bool CreateFromFontFile(const tWString& asFileName, int alSize, unsigned short alFirstChar, unsigned short alLastChar);
		bool CreateFromBitmapFile(const tWString& asFileName);

	private:
		cTraceWriter* mpTrace;
		std::string msLabel;
	};

}

#endif
