#ifndef HPL_LOWLEVELGRAPHICS_VULKAN_H
#define HPL_LOWLEVELGRAPHICS_VULKAN_H

#include "graphics/LowLevelGraphics.h"
#include "graphics/Texture.h"
#include "graphics/FrameBuffer.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#include "graphics/OcclusionQuery.h"
#include "impl/VertexBufferOpenGL.h"
#include "impl/GlslToVulkan.h"

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>

#include <functional>
#include <map>
#include <vector>

struct VmaAllocator_T;
struct VmaAllocation_T;

namespace hpl {

	class cLowLevelGraphicsVulkan;
	class cVulkanProgram;

	// GL keeps the bottom row first; so do we: viewports are not flipped and present flips once.
	struct cVulkanImage
	{
		VkImage mImage = VK_NULL_HANDLE;
		VmaAllocation_T* mpAlloc = nullptr;
		VkImageView mView = VK_NULL_HANDLE;
		VkFormat mFormat = VK_FORMAT_UNDEFINED;
		VkImageAspectFlags mAspect = VK_IMAGE_ASPECT_COLOR_BIT;
		cVector3l mvSize = cVector3l(0);
		int mlLevels = 1;
		int mlLayers = 1;
		std::map<int, VkImageView> mmapAttachViews; // (level<<8 | layer)
	};

	size_t VulkanFormatSize(VkFormat aFormat);
	bool VulkanReadPixelsFloat(cLowLevelGraphicsVulkan *apGfx, cVulkanImage& aImg, int alLevel, int alLayer, const cVector2l& avPos,
							   const cVector2l& avSize, std::vector<float>& avOut);

	//////////////////////////////////////////////////////////////////////////

	class cVulkanTexture : public iTexture
	{
	public:
		cVulkanTexture(const tString &asName, eTextureType aType, eTextureUsage aUsage, cLowLevelGraphicsVulkan* apGfx);
		~cVulkanTexture();

		bool CreateFromBitmap(cBitmap* pBmp);
		bool CreateAnimFromBitmapVec(std::vector<cBitmap*> *avBitmaps);
		bool CreateCubeFromBitmapVec(std::vector<cBitmap*> *avBitmaps);
		bool CreateFromRawData(const cVector3l &avSize,ePixelFormat aPixelFormat, unsigned char *apData);
		void SetRawData(int alLevel, const cVector3l& avOffset, const cVector3l& avSize, ePixelFormat aPixelFormat, void *apData);

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
		bool GetRawPixelsRGBAFloat(std::vector<float> &avOut);

		cVulkanImage* GetImage();
		VkImageView GetAttachView(int alLevel, int alLayer);
		VkSampler GetSampler(bool abShadow=false);
		bool IsDepth(){ return PixelFormatIsDepth(mPixelFormat); }

	private:
		bool CreateFromBitmapToIndex(cBitmap* apBmp, int alIdx);
		bool CreateImage(int alIdx, const cVector3l& avSize, ePixelFormat aFormat, int alLevels);
		void Upload(int alIdx, int alLevel, int alLayer, const cVector3l& avOffset, const cVector3l& avSize, ePixelFormat aFormat, const void *apData, int alDataSize);
		void UpdateView();

		cLowLevelGraphicsVulkan* mpGfx;
		std::vector<cVulkanImage> mvImages;
		bool mbContainsData = false;
		bool mbRedFromAlpha = false;
		VkComponentMapping mSwizzle = {};
		bool mbMipFilter = true;
		VkFilter mMinFilter = VK_FILTER_LINEAR, mMagFilter = VK_FILTER_LINEAR;
		VkSamplerMipmapMode mMipMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		float mfTimeCount = 0;
		float mfTimeDir = 1;
	};

	//////////////////////////////////////////////////////////////////////////

	class cVulkanDepthStencilBuffer : public iDepthStencilBuffer
	{
	public:
		cVulkanDepthStencilBuffer(cLowLevelGraphicsVulkan* apGfx, const cVector2l& avSize, int alDepthBits, int alStencilBits);
		~cVulkanDepthStencilBuffer();
		cVulkanImage mImage;
	private:
		cLowLevelGraphicsVulkan* mpGfx;
	};

	class cVulkanFrameBuffer : public iFrameBuffer
	{
	public:
		cVulkanFrameBuffer(const tString& asName, cLowLevelGraphicsVulkan* apGfx);

		void SetTexture2D(int alColorIdx, iTexture *apTexture, int alMipmapLevel=0);
		void SetTexture3D(int alColorIdx, iTexture *apTexture, int alZ, int alMipmapLevel=0);
		void SetTextureCubeMap(int alColorIdx, iTexture *apTexture, int alFace, int alMipmapLevel=0);
		void SetDepthTexture2D(iTexture *apTexture, int alMipmapLevel=0);
		void SetDepthTextureCubeMap(iTexture *apTexture, int alFace, int alMipmapLevel=0);
		void SetDepthStencilBuffer(iDepthStencilBuffer* apBuffer);
		bool CompileAndValidate();
		void PostBindUpdate();

		int mvLevel[kMaxDrawColorBuffers + 1] = {0};
		int mvLayer[kMaxDrawColorBuffers + 1] = {0};
		bool mbExplicitMipLevel = false;
	private:
		void SetColor(int alIdx, iTexture *apTexture, int alLevel, int alLayer);
		void SetFirstSize(const cVector2l& avSize){ if(mvSize.x < 0) mvSize = avSize; }
		cLowLevelGraphicsVulkan* mpGfx;
	};

	//////////////////////////////////////////////////////////////////////////

	class cVulkanVertexBuffer : public iVertexBufferOpenGL
	{
	public:
		cVulkanVertexBuffer(cLowLevelGraphicsVulkan* apGfx, eVertexBufferDrawType aDrawType, eVertexBufferUsageType aUsageType,
							int alReserveVtxSize, int alReserveIdxSize);
		~cVulkanVertexBuffer();

		void UpdateData(tVertexElementFlag aTypes, bool abIndices);
		void Draw(eVertexBufferDrawType aDrawType = eVertexBufferDrawType_LastEnum);
		void DrawIndices(unsigned int *apIndices, int alCount, eVertexBufferDrawType aDrawType = eVertexBufferDrawType_LastEnum);
		void Bind();
		void UnBind();

	private:
		void CompileSpecific();
		iVertexBufferOpenGL* CreateDataCopy(tVertexElementFlag aFlags, eVertexBufferDrawType aDrawType, eVertexBufferUsageType aUsageType,
											int alReserveVtxSize, int alReserveIdxSize);
		void Upload();
		void DrawRange(const unsigned int *apIndices, int alCount, eVertexBufferDrawType aDrawType);

		cLowLevelGraphicsVulkan* mpGfx;
		VkBuffer mBuffer = VK_NULL_HANDLE;
		VmaAllocation_T* mpAlloc = nullptr;
		size_t mlBufferSize = 0;
		size_t mvElementOffset[eVertexBufferElement_LastEnum] = {0};
		size_t mlIndexOffset = 0;
		bool mbDirty = true;
		void *mpMapped = nullptr;
		uint64_t mlLastUseSerial = 0;
	};

	//////////////////////////////////////////////////////////////////////////

	class cVulkanShader : public iGpuShader
	{
	public:
		cVulkanShader(const tString& asName, eGpuShaderType aType);
		bool Reload(){ return false; }
		void Unload(){}
		void Destroy(){}
		bool SamplerNeedsTextureUnitSetup(){ return true; }
		bool CreateFromFile(const tWString& asFile, const tString& asEntry="main", bool abPrintInfoIfFail=true);
		bool CreateFromString(const char *apStringData, const tString& asEntry="main", bool abPrintInfoIfFail=true);
		tString msSource;
	};

	struct cVulkanUniform
	{
		tString msName;
		int mlOffset;
		int mlArrayStride;
		int mlMatrixColumns; // 0 = not a matrix, 3 or 4
		int mlCount;
		bool mbInt;
		bool mbBool;
		int mlComponents;
	};

	class cVulkanProgram : public iGpuProgram
	{
	public:
		cVulkanProgram(const tString& asName, cLowLevelGraphicsVulkan* apGfx);
		~cVulkanProgram();

		bool Link();
		void Bind();
		void UnBind();
		bool CanAccessAPIMatrix(){ return true; }
		bool SetSamplerToUnit(const tString& asSamplerName, int alUnit);
		int GetVariableId(const tString& asName);
		bool GetVariableAsId(const tString& asName, int alId);
		bool SetInt(int alVarId, int alX);
		bool SetFloat(int alVarId, float afX);
		bool SetVec2f(int alVarId, float afX,float afY);
		bool SetVec3f(int alVarId, float afX,float afY,float afZ);
		bool SetVec4f(int alVarId, float afX,float afY,float afZ, float afW);
		bool SetFloatArray(int alVarId, const float *apX, int alCount);
		bool SetMatrixf(int alVarId, const cMatrixf& mMtx);
		bool SetMatrixf(int alVarId, eGpuShaderMatrix mType, eGpuShaderMatrixOp mOp){ return false; }
		tString DumpUniforms();

		bool LinkFromSource(const tString& asVertex, const tString& asFragment);

		VkShaderEXT mvShaders[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
		std::vector<unsigned char> mvUniformData;
		std::vector<cGlslVkSampler> mvSamplers;
		std::vector<int> mvSamplerUnit;
		unsigned int mlAttribMask = 0;
		int mlFragOutputs = 1;
		int mvBuiltinOffset[6]; // alpha test, MVP, MV, P, normal matrix, clip planes (FF only)
		bool mbUniformsDirty = true;

	private:
		struct cParam { tString msName; int mlUniform = -1; int mlElement = 0; };
		int FindUniform(const tString& asName, int* apElement=nullptr);
		bool Write(int alVarId, const float *apData, int alComponents, int alCount, bool abInt=false);

		cLowLevelGraphicsVulkan* mpGfx;
		std::vector<cVulkanUniform> mvUniforms;
		std::vector<cParam> mvParameters;
	};

	//////////////////////////////////////////////////////////////////////////

	class cVulkanOcclusionQuery : public iOcclusionQuery
	{
	public:
		cVulkanOcclusionQuery(cLowLevelGraphicsVulkan* apGfx);
		~cVulkanOcclusionQuery();
		void Begin();
		void End();
		bool FetchResults();
		unsigned int GetSampleCount(){ return mlSampleCount; }

		uint32_t mlIndex;
		uint64_t mlSubmitSerial = 0;
		bool mbPending = false;
	private:
		cLowLevelGraphicsVulkan* mpGfx;
		unsigned int mlSampleCount = 0;
	};

	//////////////////////////////////////////////////////////////////////////

	class cLowLevelGraphicsVulkan : public iLowLevelGraphics
	{
	public:
		cLowLevelGraphicsVulkan();
		~cLowLevelGraphicsVulkan();

		bool Init(	int alWidth, int alHeight, int alDisplay, int alBpp, int abFullscreen, int alMultisampling,
					eGpuProgramFormat aGpuProgramFormat,const tString& asWindowCaption, const cVector2l &avWindowPos);

		eGpuProgramFormat GetGpuProgramFormat(){ return eGpuProgramFormat_GLSL; }
		int GetCaps(eGraphicCaps aType);
		tString GetRendererDescription();
		void* GetWindowHandle(){ return mpWindow; }

		void ShowCursor(bool abX);
		void SetWindowGrab(bool abX);
		void SetRelativeMouse(bool abX);
		void SetWindowCaption(const tString& asName);
		bool GetWindowMouseFocus();
		bool GetWindowInputFocus();
		bool GetWindowIsVisible();
		bool GetFullscreenModeActive(){ return mbFullscreen; }
		int GetMultisampling(){ return 0; }
		cVector2f GetScreenSizeFloat(){ return cVector2f((float)mvScreenSize.x, (float)mvScreenSize.y); }
		const cVector2l& GetScreenSizeInt(){ return mvScreenSize; }
		bool CheckAndUpdateScreenSize();
		bool ForceWindowSize(int alWidth, int alHeight);
		void SetVsyncActive(bool abX, bool abAdaptive = false);
		void SetMultisamplingActive(bool abX){}
		void SetGammaCorrection(float afX){ mfGammaCorrection = afX; }
		float GetGammaCorrection(){ return mfGammaCorrection; }

		iFontData* CreateFontData(const tString &asName);
		iGpuProgram* CreateGpuProgram(const tString& asName);
		iGpuShader* CreateGpuShader(const tString& asName, eGpuShaderType aType);
		iTexture* CreateTexture(const tString &asName,eTextureType aType, eTextureUsage aUsage);
		iVertexBuffer* CreateVertexBuffer(eVertexBufferType aType, eVertexBufferDrawType aDrawType, eVertexBufferUsageType aUsageType,
										  int alReserveVtxSize=0,int alReserveIdxSize=0);
		iFrameBuffer* CreateFrameBuffer(const tString& asName);
		iDepthStencilBuffer* CreateDepthStencilBuffer(const cVector2l& avSize, int alDepthBits, int alStencilBits);
		iOcclusionQuery* CreateOcclusionQuery();

		void ClearFrameBuffer(tClearFrameBufferFlag aFlags);
		void SetClearColor(const cColor& aCol){ mClearColor = aCol; }
		void SetClearDepth(float afDepth){ mfClearDepth = afDepth; }
		void SetClearStencil(int alVal){ mlClearStencil = alVal; }

		void FlushRendering(){}
		void WaitAndFinishRendering();
		void SwapBuffers();

		void CopyFrameBufferToTexure(iTexture* apTex, const cVector2l &avPos, const cVector2l &avSize, const cVector2l &avTexOffset=0);
		cBitmap* CopyFrameBufferToBitmap(const cVector2l &avScreenPos=0, const cVector2l &avScreenSize=-1);

		void SetCurrentFrameBuffer(iFrameBuffer* apFrameBuffer, const cVector2l &avPos = 0, const cVector2l& avSize = -1);
		iFrameBuffer* GetCurrentFrameBuffer(){ return mpFrameBuffer; }
		void SetFrameBufferDrawTargets(int *apTargets, int alNumOfTargets);

		void SetColorWriteActive(bool abR,bool abG,bool abB,bool abA);
		void SetDepthWriteActive(bool abX){ mState.mbDepthWrite = abX; mbStateDirty = true; }
		void SetCullActive(bool abX){ mState.mbCull = abX; mbStateDirty = true; }
		void SetCullMode(eCullMode aMode){ mState.mCullMode = aMode; mbStateDirty = true; }
		void SetDepthTestActive(bool abX){ mState.mbDepthTest = abX; mbStateDirty = true; }
		void SetDepthTestFunc(eDepthTestFunc aFunc){ mState.mDepthFunc = aFunc; mbStateDirty = true; }
		void SetAlphaTestActive(bool abX){ mbAlphaTest = abX; }
		void SetAlphaTestFunc(eAlphaTestFunc aFunc,float afRef){ mAlphaFunc = aFunc; mfAlphaRef = afRef; }
		void SetStencilActive(bool abX){ mState.mbStencil = abX; mbStateDirty = true; }
		void SetStencilWriteMask(unsigned int alMask){ mState.mlStencilWriteMask = alMask; mbStateDirty = true; }
		void SetStencil(eStencilFunc aFunc,int alRef, unsigned int aMask, eStencilOp aFailOp,eStencilOp aZFailOp,eStencilOp aZPassOp);
		void SetStencilTwoSide(eStencilFunc aFrontFunc,eStencilFunc aBackFunc, int alRef, unsigned int aMask,
								eStencilOp aFrontFailOp,eStencilOp aFrontZFailOp,eStencilOp aFrontZPassOp,
								eStencilOp aBackFailOp,eStencilOp aBackZFailOp,eStencilOp aBackZPassOp);
		void SetScissorActive(bool abX){ mState.mbScissor = abX; mbStateDirty = true; }
		void SetScissorRect(const cVector2l& avPos, const cVector2l& avSize);
		void SetClipPlane(int alIdx, const cPlanef& aPlane);
		cPlanef GetClipPlane(int alIdx){ return mvClipPlanes[alIdx]; }
		void SetClipPlaneActive(int alIdx, bool abX){ mvClipPlaneActive[alIdx] = abX; }
		void SetColor(const cColor &aColor){ mCurrentColor = aColor; }
		void SetBlendActive(bool abX){ mState.mbBlend = abX; mbStateDirty = true; }
		void SetBlendFunc(eBlendFunc aSrcFactor, eBlendFunc aDestFactor){ SetBlendFuncSeparate(aSrcFactor, aDestFactor, aSrcFactor, aDestFactor); }
		void SetBlendFuncSeparate(eBlendFunc aSrcFactorColor, eBlendFunc aDestFactorColor, eBlendFunc aSrcFactorAlpha, eBlendFunc aDestFactorAlpha);
		void SetPolygonOffsetActive(bool abX){ mState.mbDepthBias = abX; mbStateDirty = true; }
		void SetDepthClampActive(bool abX){ mState.mbDepthClamp = abX; mbStateDirty = true; }
		void SetPolygonOffset(float afBias, float afSlopeScaleBias){ mState.mfDepthBias = afBias; mState.mfDepthSlope = afSlopeScaleBias; mbStateDirty = true; }

		void PushMatrix(eMatrix aMtxType);
		void PopMatrix(eMatrix aMtxType);
		void SetIdentityMatrix(eMatrix aMtxType){ mvMatrix[aMtxType] = cMatrixf::Identity; }
		void SetMatrix(eMatrix aMtxType, const cMatrixf& a_mtxA){ mvMatrix[aMtxType] = a_mtxA; }
		void SetOrthoProjection(const cVector2f& avSize, float afMin, float afMax);
		void SetOrthoProjection(const cVector3f& avMin, const cVector3f& avMax);

		void SetTexture(unsigned int alUnit,iTexture* apTex){ mvTextures[alUnit] = apTex; }
		void SetActiveTextureUnit(unsigned int alUnit){}
		void SetTextureEnv(eTextureParam aParam, int alVal){}
		void SetTextureConstantColor(const cColor &aColor){}

		void DrawTriangle(tVertexVec& avVtx);
		void DrawQuad(const cVector3f &avPos,const cVector2f &avSize, const cColor& aColor=cColor(1,1));
		void DrawQuad(const cVector3f &avPos,const cVector2f &avSize, const cVector2f &avMinTexCoord,const cVector2f &avMaxTexCoord,
					  const cColor& aColor=cColor(1,1));
		void DrawQuad(const cVector3f &avPos,const cVector2f &avSize, const cVector2f &avMinTexCoord0,const cVector2f &avMaxTexCoord0,
					  const cVector2f &avMinTexCoord1,const cVector2f &avMaxTexCoord1, const cColor& aColor=cColor(1,1));
		void DrawQuad(const tVertexVec &avVtx);
		void DrawQuad(const tVertexVec &avVtx, const cColor aCol);
		void DrawQuad(const tVertexVec &avVtx,const float afZ);
		void DrawQuad(const tVertexVec &avVtx,const float afZ,const cColor &aCol);
		void DrawQuadMultiTex(const tVertexVec &avVtx,const tVector3fVec &avExtraUvs);
		void DrawLine(const cVector3f& avBegin, const cVector3f& avEnd, cColor aCol);
		void DrawLine(const cVector3f& avBegin, const cColor& aBeginCol, const cVector3f& avEnd, const cColor& aEndCol);
		void DrawBoxMinMax(const cVector3f& avMin, const cVector3f& avMax, cColor aCol);
		void DrawSphere(const cVector3f& avPos, float afRadius, cColor aCol);
		void DrawSphere(const cVector3f& avPos, float afRadius, cColor aColX, cColor aColY, cColor aColZ);
		void DrawLineQuad(const cRect2f& aRect, float afZ, cColor aCol);
		void DrawLineQuad(const cVector3f &avPos,const cVector2f &avSize, cColor aCol);

		void AddVertexToBatch(const cVertex *apVtx);
		void AddVertexToBatch(const cVertex *apVtx, const cVector3f* avTransform);
		void AddVertexToBatch(const cVertex *apVtx, const cMatrixf* aMtx);
		void AddVertexToBatch_Size2D(const cVertex *apVtx, const cVector3f* avTransform, const cColor* apCol,const float& mfW, const float& mfH);
		void AddVertexToBatch_Raw(const cVector3f& avPos, const cColor &aColor, const cVector3f& avTex);
		void AddIndexToBatch(int alIndex);
		void AddTexCoordToBatch(unsigned int alUnit,const cVector3f *apCoord);
		void SetBatchTextureUnitActive(unsigned int alUnit,bool abActive){ mbBatchUnitOwnCoords[alUnit] = abActive; }
		void FlushTriBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear=true);
		void FlushQuadBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear=true);
		void ClearBatch();

		/////////////////////////////////////
		// Backend internals used by the resource classes

		struct cAttribStream { VkBuffer mBuffer; VkDeviceSize mOffset; uint32_t mStride; VkFormat mFormat; };
		enum ePrim { ePrim_Triangles, ePrim_TriStrip, ePrim_TriFan, ePrim_Lines, ePrim_LineStrip, ePrim_Points };

		// Draws with the bound program (or the fixed-function emulation). Streams are indexed by eGlslVkAttrib.
		void Draw(const cAttribStream *apStreams, ePrim aPrim, uint32_t alCount, VkBuffer aIndexBuffer, VkDeviceSize alIndexOffset);
		void* AllocTransient(size_t alSize, VkBuffer& aBuffer, VkDeviceSize& aOffset, size_t alAlign=16);

		VkCommandBuffer Cmd();
		void EndRendering();
		void SubmitAndWait();
		void DeferDestroy(std::function<void()> aFunc);
		void CreateImage(cVulkanImage& aImg, VkImageType aType, VkImageViewType aViewType, VkFormat aFormat, const cVector3l& avSize,
						 int alLevels, int alLayers, VkImageUsageFlags aUsage, const VkComponentMapping* apSwizzle=nullptr);
		void DestroyImage(cVulkanImage& aImg);
		void CreateBuffer(VkDeviceSize alSize, VkBufferUsageFlags aUsage, bool abHostVisible, VkBuffer& aBuffer, VmaAllocation_T*& apAlloc, void** apMapped=nullptr);
		VkSampler GetSampler(const VkSamplerCreateInfo& aInfo);
		void UploadToImage(cVulkanImage& aImg, int alLevel, int alLayer, const cVector3l& avOffset, const cVector3l& avSize, const void *apData, size_t alSize);
		void GenerateMips(cVulkanImage& aImg);
		void ReadImage(cVulkanImage& aImg, int alLevel, int alLayer, const cVector2l& avPos, const cVector2l& avSize, std::vector<unsigned char>& avOut, size_t alPixelSize);
		bool CreateShaders(const std::vector<unsigned int>& avVert, const std::vector<unsigned int>& avFrag, VkShaderEXT *apOut);
		void FrameBufferChanged(iFrameBuffer* apFrameBuffer){ if(apFrameBuffer == mpFrameBuffer) EndRendering(); }
		void TextureDestroyed(iTexture* apTex);
		void BeginQuery(uint32_t alIndex);
		void EndQuery(uint32_t alIndex);

		cVulkanProgram* mpProgram = nullptr;
		VkDevice mDevice = VK_NULL_HANDLE;
		VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
		VmaAllocator_T* mpAllocator = nullptr;
		VkFormat mDepthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
		VkFormat mDepth24Format = VK_FORMAT_X8_D24_UNORM_PACK32;
		VkQueryPool mQueryPool = VK_NULL_HANDLE;
		std::vector<uint32_t> mvFreeQueries;
		uint64_t mlSubmitSerial = 1; // serial of the command buffer being recorded
		uint64_t mlCompletedSerial = 0;
		uint32_t mlActiveQuery = UINT32_MAX;
		float mfMaxAnisotropy = 1;

		PFN_vkCreateShadersEXT vkCreateShadersEXT_ = nullptr;
		PFN_vkDestroyShaderEXT vkDestroyShaderEXT_ = nullptr;

	private:
		struct cRenderState
		{
			bool mbDepthWrite = true, mbCull = true, mbDepthTest = true, mbStencil = false, mbScissor = false, mbBlend = false;
			bool mbDepthBias = false, mbDepthClamp = false;
			eCullMode mCullMode = eCullMode_CounterClockwise;
			eDepthTestFunc mDepthFunc = eDepthTestFunc_LessOrEqual;
			VkStencilOpState mStencilFront{}, mStencilBack{};
			unsigned int mlStencilWriteMask = 0xFFFFFFFF;
			VkColorBlendEquationEXT mBlend{};
			VkColorComponentFlags mColorMask = 0xF;
			cVector2l mvScissorPos = 0, mvScissorSize = 0;
			float mfDepthBias = 0, mfDepthSlope = 0;
		};

		struct cFrame
		{
			VkCommandBuffer mCmd = VK_NULL_HANDLE;
			VkFence mFence = VK_NULL_HANDLE;
			VkSemaphore mAcquired = VK_NULL_HANDLE;
			uint64_t mlSerial = 0;
			std::vector<std::function<void()>> mvDestroy;
		};

		struct cRingBuffer
		{
			VkBuffer mBuffer = VK_NULL_HANDLE;
			VmaAllocation_T* mpAlloc = nullptr;
			unsigned char *mpMapped = nullptr;
			size_t mlSize = 0, mlUsed = 0;
		};

		bool CreateDevice();
		bool CreateSwapchain();
		void DestroySwapchain();
		void CreateScreenTargets();
		void BeginCommandBuffer();
		void Submit(bool abPresent);
		void WaitFrame(cFrame& aFrame);
		void BeginRendering();
		void ApplyState(int alColorTargets);
		void Present();
		cVulkanProgram* GetFixedFunctionProgram();
		cVulkanImage* GetDummyImage(const std::string& asSamplerType);
		void ImmediateFlush();

		void ImBegin(int alMode);
		void ImEnd();
		void ImVertex(float x, float y, float z);
		void ImTexCoord(int alUnit, float s, float t, float r=0);
		void SetUpBatchStreams(tVtxBatchFlag aFlags, cAttribStream *apStreams);
		void FlushBatch(tVtxBatchFlag aTypeFlags, bool abQuads, bool abAutoClear);

		// shared with LowLevelGraphicsImmediate.inl
		float* mpVertexArray;
		unsigned int mlVertexCount = 0;
		unsigned int* mpIndexArray;
		unsigned int mlIndexCount = 0;
		unsigned int mlBatchStride = 13;
		float *mpTexCoordArray[kMaxTextureUnits];
		unsigned int mlTexCoordArrayCount[kMaxTextureUnits];
		bool mbBatchUnitOwnCoords[kMaxTextureUnits];
		unsigned int mlBatchArraySize = 20000;

		SDL_Window *mpWindow = nullptr;
		cVector2l mvScreenSize;
		bool mbFullscreen = false;
		bool mbGrab = false;
		bool mbHeadless = false;
		bool mbVsync = true;
		float mfGammaCorrection = 1.0f;

		VkInstance mInstance = VK_NULL_HANDLE;
		VkSurfaceKHR mSurface = VK_NULL_HANDLE;
		VkQueue mQueue = VK_NULL_HANDLE;
		uint32_t mlQueueFamily = 0;
		VkCommandPool mCommandPool = VK_NULL_HANDLE;
		VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
		VkFormat mSwapchainFormat = VK_FORMAT_UNDEFINED;
		VkColorSpaceKHR mSwapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		VkExtent2D mSwapchainExtent{};
		std::vector<VkImage> mvSwapchainImages;
		std::vector<VkImageView> mvSwapchainViews;
		std::vector<VkSemaphore> mvRenderDone;
		bool mbSwapchainDirty = false;
		uint32_t mlPresentImage = 0;
		cFrame mvFrames[2];
		int mlFrame = 0;
		cRingBuffer mvRing[2];
		std::vector<std::function<void()>> mvDestroyNow;
		VkDescriptorSetLayout mSetLayout = VK_NULL_HANDLE;
		VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
		std::map<uint64_t, VkSampler> mmapSamplers;
		std::map<std::string, cVulkanImage> mmapDummyImages;
		std::map<unsigned int, cVulkanProgram*> mmapFixedFunction;
		cVulkanProgram *mpPresentProgram = nullptr;

		cVulkanImage mScreenColor, mScreenDepth;

		// HDR / wide colour output
		int mlOutputMode = 0; // 0 SDR, 1 HDR10 PQ, 2 scRGB linear, 3 Display-P3 SDR
		float mfPaperWhite = 203, mfPeakNits = 1000;

		bool mbRendering = false;
		VkImageView mvRenderColor[kMaxDrawColorBuffers];
		VkImage mvRenderColorImage[kMaxDrawColorBuffers];
		int mlRenderColorCount = 0;
		VkImageView mRenderDepth = VK_NULL_HANDLE;
		bool mbRenderStencil = false;
		bool mbStateDirty = true;
		int mvDrawTargets[kMaxDrawColorBuffers];
		int mlDrawTargetCount = -1;

		cRenderState mState;
		iFrameBuffer* mpFrameBuffer = nullptr;
		cVector2l mvFrameBufferPos = 0, mvFrameBufferSize = 0, mvFrameBufferTotalSize = 0;
		cColor mClearColor = cColor(0,0);
		float mfClearDepth = 1;
		int mlClearStencil = 0;
		cColor mCurrentColor = cColor(1,1);
		bool mbAlphaTest = false;
		eAlphaTestFunc mAlphaFunc = eAlphaTestFunc_GreaterOrEqual;
		float mfAlphaRef = 0.6f;
		cPlanef mvClipPlanes[kMaxClipPlanes];
		cPlanef mvClipPlanesEye[kMaxClipPlanes];
		bool mvClipPlaneActive[kMaxClipPlanes] = {false};
		cMatrixf mvMatrix[eMatrix_LastEnum];
		std::vector<cMatrixf> mvMatrixStack[eMatrix_LastEnum];
		iTexture* mvTextures[kMaxTextureUnits] = {nullptr};

		struct cImVertex { float mvPos[3]; float mvColor[4]; float mvTex[4][4]; };
		std::vector<cImVertex> mvImVertices;
		cImVertex mImCurrent;
		int mlImMode = 0;
	};

}

#endif
