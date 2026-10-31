#include "impl/LowLevelGraphicsVulkan.h"

#include "graphics/Bitmap.h"
#include "math/Math.h"
#include "system/EngineDiagnostics.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/String.h"

#include <vk_mem_alloc.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// FORMATS
	//////////////////////////////////////////////////////////////////////////

	enum eConvert { eConvert_None, eConvert_RGBtoRGBA, eConvert_FloatToHalf, eConvert_Float3to4 };

	struct cVkFormat
	{
		VkFormat mFormat = VK_FORMAT_UNDEFINED;
		VkComponentMapping mSwizzle = {};
		eConvert mConvert = eConvert_None;
		int mlSrcComponents = 0; // for conversions
	};

	static cVkFormat ToVkFormat(ePixelFormat aFormat, bool abSRGB, VkFormat aDepth24)
	{
		const VkComponentSwizzle R = VK_COMPONENT_SWIZZLE_R, G = VK_COMPONENT_SWIZZLE_G, ONE = VK_COMPONENT_SWIZZLE_ONE, ZERO = VK_COMPONENT_SWIZZLE_ZERO;
		cVkFormat f;
		switch(aFormat)
		{
		case ePixelFormat_Alpha: f.mFormat = VK_FORMAT_R8_UNORM; f.mSwizzle = {ZERO, ZERO, ZERO, R}; break;
		case ePixelFormat_Luminance: f.mFormat = VK_FORMAT_R8_UNORM; f.mSwizzle = {R, R, R, ONE}; break;
		case ePixelFormat_LuminanceAlpha: f.mFormat = VK_FORMAT_R8G8_UNORM; f.mSwizzle = {R, R, R, G}; break;
		case ePixelFormat_RGB: f.mFormat = abSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM; f.mConvert = eConvert_RGBtoRGBA; break;
		case ePixelFormat_RGBA: f.mFormat = abSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM; break;
		case ePixelFormat_BGR: f.mFormat = abSRGB ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM; f.mConvert = eConvert_RGBtoRGBA; break;
		case ePixelFormat_BGRA: f.mFormat = abSRGB ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM; break;
		case ePixelFormat_DXT1: f.mFormat = abSRGB ? VK_FORMAT_BC1_RGBA_SRGB_BLOCK : VK_FORMAT_BC1_RGBA_UNORM_BLOCK; break;
		case ePixelFormat_DXT2:
		case ePixelFormat_DXT3: f.mFormat = abSRGB ? VK_FORMAT_BC2_SRGB_BLOCK : VK_FORMAT_BC2_UNORM_BLOCK; break;
		case ePixelFormat_DXT4:
		case ePixelFormat_DXT5: f.mFormat = abSRGB ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK; break;
		case ePixelFormat_RGTC1: f.mFormat = VK_FORMAT_BC4_UNORM_BLOCK; break;
		case ePixelFormat_RGTC2: f.mFormat = VK_FORMAT_BC5_UNORM_BLOCK; f.mSwizzle = {R, R, R, G}; break; // SOMA uploads these as LATC2
		case ePixelFormat_BC6H: f.mFormat = VK_FORMAT_BC6H_UFLOAT_BLOCK; break;
		case ePixelFormat_BC7: f.mFormat = abSRGB ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC7_UNORM_BLOCK; break;
		case ePixelFormat_Depth16: f.mFormat = VK_FORMAT_D16_UNORM; break;
		case ePixelFormat_Depth24: f.mFormat = aDepth24; break;
		case ePixelFormat_Depth32: f.mFormat = VK_FORMAT_D32_SFLOAT; break;
		case ePixelFormat_Alpha16: f.mFormat = VK_FORMAT_R16_SFLOAT; f.mSwizzle = {ZERO, ZERO, ZERO, R}; f.mConvert = eConvert_FloatToHalf; f.mlSrcComponents = 1; break;
		case ePixelFormat_Luminance16: f.mFormat = VK_FORMAT_R16_SFLOAT; f.mSwizzle = {R, R, R, ONE}; f.mConvert = eConvert_FloatToHalf; f.mlSrcComponents = 1; break;
		case ePixelFormat_LuminanceAlpha16: f.mFormat = VK_FORMAT_R16G16_SFLOAT; f.mSwizzle = {R, R, R, G}; f.mConvert = eConvert_FloatToHalf; f.mlSrcComponents = 2; break;
		case ePixelFormat_RGB16: f.mFormat = VK_FORMAT_R16G16B16A16_SFLOAT; f.mConvert = eConvert_FloatToHalf; f.mlSrcComponents = 3; break;
		case ePixelFormat_RGBA16: f.mFormat = VK_FORMAT_R16G16B16A16_SFLOAT; f.mConvert = eConvert_FloatToHalf; f.mlSrcComponents = 4; break;
		case ePixelFormat_Alpha32: f.mFormat = VK_FORMAT_R32_SFLOAT; f.mSwizzle = {ZERO, ZERO, ZERO, R}; break;
		case ePixelFormat_Luminance32: f.mFormat = VK_FORMAT_R32_SFLOAT; f.mSwizzle = {R, R, R, ONE}; break;
		case ePixelFormat_LuminanceAlpha32: f.mFormat = VK_FORMAT_R32G32_SFLOAT; f.mSwizzle = {R, R, R, G}; break;
		case ePixelFormat_RGB32: f.mFormat = VK_FORMAT_R32G32B32A32_SFLOAT; f.mConvert = eConvert_Float3to4; break;
		case ePixelFormat_RGBA32: f.mFormat = VK_FORMAT_R32G32B32A32_SFLOAT; break;
		default: break;
		}
		return f;
	}

	static uint16_t FloatToHalf(float f)
	{
		uint32_t x;
		memcpy(&x, &f, 4);
		uint32_t s = (x >> 16) & 0x8000;
		int e = (int)((x >> 23) & 0xFF) - 127 + 15;
		uint32_t m = x & 0x7FFFFF;
		if(((x >> 23) & 0xFF) == 0xFF) return (uint16_t)(s | 0x7C00 | (m ? 0x200 : 0));
		if(e >= 31) return (uint16_t)(s | 0x7C00);
		if(e <= 0)
		{
			if(e < -10) return (uint16_t)s;
			m |= 0x800000;
			return (uint16_t)(s | (m >> (14 - e)));
		}
		return (uint16_t)(s | (e << 10) | (m >> 13));
	}

	// Returns data in the layout the Vulkan format expects (may point into avScratch)
	static const void* ConvertPixels(const cVkFormat& aFmt, const void *apData, size_t alPixels, std::vector<unsigned char>& avScratch, size_t& alSize)
	{
		switch(aFmt.mConvert)
		{
		case eConvert_RGBtoRGBA:
		{
			avScratch.resize(alPixels * 4);
			const unsigned char *s = (const unsigned char*)apData;
			for(size_t i=0; i<alPixels; ++i) { avScratch[i*4] = s[i*3]; avScratch[i*4+1] = s[i*3+1]; avScratch[i*4+2] = s[i*3+2]; avScratch[i*4+3] = 255; }
			alSize = avScratch.size();
			return avScratch.data();
		}
		case eConvert_FloatToHalf:
		{
			int lDst = aFmt.mlSrcComponents == 3 ? 4 : aFmt.mlSrcComponents;
			avScratch.resize(alPixels * lDst * 2);
			uint16_t *d = (uint16_t*)avScratch.data();
			const float *s = (const float*)apData;
			for(size_t i=0; i<alPixels; ++i)
				for(int c=0; c<lDst; ++c) d[i*lDst + c] = c < aFmt.mlSrcComponents ? FloatToHalf(s[i*aFmt.mlSrcComponents + c]) : FloatToHalf(1.0f);
			alSize = avScratch.size();
			return avScratch.data();
		}
		case eConvert_Float3to4:
		{
			avScratch.resize(alPixels * 16);
			float *d = (float*)avScratch.data();
			const float *s = (const float*)apData;
			for(size_t i=0; i<alPixels; ++i) { d[i*4] = s[i*3]; d[i*4+1] = s[i*3+1]; d[i*4+2] = s[i*3+2]; d[i*4+3] = 1; }
			alSize = avScratch.size();
			return avScratch.data();
		}
		default:
			return apData;
		}
	}

	static VkSamplerAddressMode ToVkWrap(eTextureWrap aMode)
	{
		switch(aMode)
		{
		case eTextureWrap_Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case eTextureWrap_ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		default: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// TEXTURE
	//////////////////////////////////////////////////////////////////////////

	cVulkanTexture::cVulkanTexture(const tString &asName, eTextureType aType, eTextureUsage aUsage, cLowLevelGraphicsVulkan* apGfx)
		: iTexture(asName, _W(""), aType, aUsage, apGfx), mpGfx(apGfx)
	{
		mbUseMipMaps = aType != eTextureType_Rect;
		if(aUsage == eTextureUsage_RenderTarget) mbUseMipMaps = false;
	}

	cVulkanTexture::~cVulkanTexture()
	{
		mpGfx->TextureDestroyed(this);
		for(cVulkanImage& img : mvImages) mpGfx->DestroyImage(img);
	}

	cVulkanImage* cVulkanTexture::GetImage()
	{
		if(!mbContainsData || mvImages.empty()) return NULL;
		size_t lFrame = mvImages.size() > 1 ? std::min((size_t)mfTimeCount, mvImages.size()-1) : 0;
		return mvImages[lFrame].mImage ? &mvImages[lFrame] : NULL;
	}

	int cVulkanTexture::GetCurrentLowlevelHandle()
	{
		cVulkanImage *pImg = GetImage();
		return pImg ? (int)((uintptr_t)pImg->mImage & 0x7FFFFFFF) : 0;
	}

	bool cVulkanTexture::CreateImage(int alIdx, const cVector3l& avSize, ePixelFormat aFormat, int alLevels)
	{
		if((int)mvImages.size() <= alIdx) mvImages.resize(alIdx + 1);
		cVulkanImage& img = mvImages[alIdx];
		mpGfx->DestroyImage(img);

		cVkFormat fmt = ToVkFormat(aFormat, mbsRGB, mpGfx->mDepth24Format);
		if(fmt.mFormat == VK_FORMAT_UNDEFINED) { Error("Vulkan: unsupported pixel format %d for '%s'\n", (int)aFormat, msName.c_str()); return false; }
		bool bDepth = PixelFormatIsDepth(aFormat);
		VkImageUsageFlags lUsage = VK_IMAGE_USAGE_SAMPLED_BIT;
		if(mUsage == eTextureUsage_RenderTarget || bDepth)
			lUsage |= bDepth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		else if(!PixelFormatIsCompressed(aFormat))
			lUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // engine may render into any texture through a frame buffer

		VkImageType type = VK_IMAGE_TYPE_2D;
		VkImageViewType view = VK_IMAGE_VIEW_TYPE_2D;
		int lLayers = 1;
		switch(mType)
		{
		case eTextureType_1D: type = VK_IMAGE_TYPE_1D; view = VK_IMAGE_VIEW_TYPE_1D; break;
		case eTextureType_3D: type = VK_IMAGE_TYPE_3D; view = VK_IMAGE_VIEW_TYPE_3D; break;
		case eTextureType_CubeMap: view = VK_IMAGE_VIEW_TYPE_CUBE; lLayers = 6; break;
		default: break;
		}
		VkComponentMapping swizzle = fmt.mSwizzle;
		mSwizzle = swizzle;
		if(mbRedFromAlpha && mType == eTextureType_2D) swizzle.r = VK_COMPONENT_SWIZZLE_A;
		mpGfx->CreateImage(img, type, view, fmt.mFormat, avSize, alLevels, lLayers, lUsage, &swizzle);
		return img.mImage != VK_NULL_HANDLE;
	}

	void cVulkanTexture::Upload(int alIdx, int alLevel, int alLayer, const cVector3l& avOffset, const cVector3l& avSize, ePixelFormat aFormat,
								const void *apData, int alDataSize)
	{
		if(apData == NULL || alIdx >= (int)mvImages.size()) return;
		cVkFormat fmt = ToVkFormat(aFormat, mbsRGB, mpGfx->mDepth24Format);
		std::vector<unsigned char> vScratch;
		size_t lSize = alDataSize;
		size_t lPixels = (size_t)std::max(1, avSize.x) * std::max(1, avSize.y) * std::max(1, avSize.z);
		const void *pData = ConvertPixels(fmt, apData, lPixels, vScratch, lSize);
		mpGfx->UploadToImage(mvImages[alIdx], alLevel, alLayer, avOffset, avSize, pData, lSize);
	}

	static int FullMipCount(const cVector3l& v)
	{
		int lMax = std::max(v.x, std::max(v.y, v.z)), n = 1;
		while(lMax > 1) { lMax >>= 1; ++n; }
		return n;
	}

	// 2x2 box filter on 8-bit data, like gluScaleImage at exact halvings
	static void HalveImage(std::vector<unsigned char>& avData, cVector3l& avSize, int alBpp)
	{
		cVector3l vNew(std::max(1, avSize.x/2), std::max(1, avSize.y/2), 1);
		std::vector<unsigned char> vOut((size_t)vNew.x * vNew.y * alBpp);
		for(int y=0; y<vNew.y; ++y)
			for(int x=0; x<vNew.x; ++x)
				for(int c=0; c<alBpp; ++c)
				{
					int lSum = 0;
					for(int dy=0; dy<2; ++dy)
						for(int dx=0; dx<2; ++dx)
						{
							int sx = std::min(avSize.x-1, x*2+dx), sy = std::min(avSize.y-1, y*2+dy);
							lSum += avData[((size_t)sy*avSize.x + sx)*alBpp + c];
						}
					vOut[((size_t)y*vNew.x + x)*alBpp + c] = (unsigned char)((lSum + 2) / 4);
				}
		avData.swap(vOut);
		avSize = vNew;
	}

	bool cVulkanTexture::CreateFromBitmapToIndex(cBitmap* apBmp, int alIdx)
	{
		int lFaces = mType == eTextureType_CubeMap ? 6 : 1;
		if(lFaces == 6 && apBmp->GetNumOfImages() < 6)
		{
			Error("Bitmap has to few images (%d) to create cubmap texture '%s'\n", apBmp->GetNumOfImages(), msName.c_str());
			return false;
		}
		ePixelFormat format = apBmp->GetPixelFormat();
		int lNumMips = apBmp->GetNumOfMipMaps();
		mvSize = apBmp->GetSize();
		mPixelFormat = format;
		mbIsCompressed = PixelFormatIsCompressed(format);
		if(mbIsCompressed && lNumMips <= 1) mbUseMipMaps = false;
		if(mType == eTextureType_Rect) mbUseMipMaps = false;

		int lStart = 0;
		bool bCpuResize = false;
		if(mlSizeDownScaleLevel > 0 && (!mbIsCompressed || lNumMips > 1) && !PixelFormatIsDepth(format))
		{
			if(lNumMips > 1)
			{
				lStart = std::min(lNumMips - 1, (int)mlSizeDownScaleLevel);
				mvSize.x = std::max(1, mvSize.x >> lStart);
				mvSize.y = std::max(1, mvSize.y >> lStart);
				mvSize.z = std::max(1, mvSize.z >> lStart);
			}
			else if((mType == eTextureType_2D || mType == eTextureType_CubeMap) && mvSize.x > mvMinDownScaleSize.x && mvSize.y > mvMinDownScaleSize.y &&
					!PixelFormatIsFloatingPoint(format))
				bCpuResize = true;
		}

		std::vector<std::vector<unsigned char>> vResized(lFaces);
		cVector3l vBaseSize = mvSize;
		if(bCpuResize)
		{
			int lBpp = GetBytesPerPixel(format);
			for(int f=0; f<lFaces; ++f)
			{
				cBitmapData *pData = apBmp->GetData(f, 0);
				vResized[f].assign(pData->mpData, pData->mpData + pData->mlSize);
				cVector3l vSize = mvSize;
				for(int i=0; i<(int)mlSizeDownScaleLevel; ++i)
				{
					if(vSize.x/2 < mvMinDownScaleSize.x || vSize.y/2 < mvMinDownScaleSize.y) break;
					HalveImage(vResized[f], vSize, lBpp);
				}
				vBaseSize = vSize;
			}
			mvSize = vBaseSize;
		}

		bool bProvidedMips = mbUseMipMaps && lNumMips > 1 && !bCpuResize;
		int lLevels = !mbUseMipMaps ? 1 : bProvidedMips ? lNumMips - lStart : FullMipCount(mvSize);
		if(!CreateImage(alIdx, mvSize, format, lLevels)) return false;

		mlMemorySize = 0;
		for(int f=0; f<lFaces; ++f)
		{
			cVector3l vSize = mvSize;
			int lUploadLevels = bProvidedMips ? lLevels : 1;
			for(int l=0; l<lUploadLevels; ++l)
			{
				cBitmapData *pData = apBmp->GetData(f, l + lStart);
				const void *pSrc = bCpuResize ? (const void*)vResized[f].data() : (const void*)pData->mpData;
				int lSize = bCpuResize ? (int)vResized[f].size() : pData->mlSize;
				Upload(alIdx, l, f, cVector3l(0,0,0), vSize, format, pSrc, lSize);
				mlMemorySize += lSize;
				vSize.x = std::max(1, vSize.x >> 1);
				vSize.y = std::max(1, vSize.y >> 1);
				vSize.z = std::max(1, vSize.z >> 1);
			}
		}
		if(mbUseMipMaps && !bProvidedMips) mpGfx->GenerateMips(mvImages[alIdx]);

		mbContainsData = true;
		mbMipFilter = mbUseMipMaps && mType != eTextureType_Rect;
		mMinFilter = mMagFilter = VK_FILTER_LINEAR;
		mMipMode = mFilter == eTextureFilter_Bilinear ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
		if(mType == eTextureType_Rect) mWrapS = mWrapT = mWrapR = eTextureWrap_ClampToEdge;
		return true;
	}

	bool cVulkanTexture::CreateFromBitmap(cBitmap* apBmp){ return CreateFromBitmapToIndex(apBmp, 0); }

	bool cVulkanTexture::CreateAnimFromBitmapVec(std::vector<cBitmap*> *avBitmaps)
	{
		for(size_t i=0; i<avBitmaps->size(); ++i)
			if(!CreateFromBitmapToIndex((*avBitmaps)[i], (int)i)) return false;
		return true;
	}

	bool cVulkanTexture::CreateCubeFromBitmapVec(std::vector<cBitmap*> *avBitmaps)
	{
		if(mUsage == eTextureUsage_RenderTarget || mType != eTextureType_CubeMap) return false;
		if(avBitmaps->size() < 6) { Error("Only %d bitmaps supplied for creation of cube map, 6 needed.", (int)avBitmaps->size()); return false; }
		cBitmap *pFirst = (*avBitmaps)[0];
		ePixelFormat format = pFirst->GetPixelFormat();
		mvSize = pFirst->GetSize();
		mPixelFormat = format;
		mbIsCompressed = PixelFormatIsCompressed(format);
		int lNumMips = pFirst->GetNumOfMipMaps();
		if(mbIsCompressed && lNumMips <= 1) mbUseMipMaps = false;
		bool bProvidedMips = mbUseMipMaps && lNumMips > 1;
		int lLevels = !mbUseMipMaps ? 1 : bProvidedMips ? lNumMips : FullMipCount(mvSize);
		if(!CreateImage(0, mvSize, format, lLevels)) return false;
		for(int f=0; f<6; ++f)
		{
			cBitmap *pBmp = (*avBitmaps)[f];
			cVector3l vSize = mvSize;
			for(int l=0; l<(bProvidedMips ? lLevels : 1); ++l)
			{
				cBitmapData *pData = pBmp->GetData(0, l);
				Upload(0, l, f, cVector3l(0,0,0), vSize, format, pData->mpData, pData->mlSize);
				vSize.x = std::max(1, vSize.x >> 1);
				vSize.y = std::max(1, vSize.y >> 1);
			}
		}
		if(mbUseMipMaps && !bProvidedMips) mpGfx->GenerateMips(mvImages[0]);
		mbContainsData = true;
		mbMipFilter = mbUseMipMaps;
		mMipMode = mFilter == eTextureFilter_Bilinear ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
		return true;
	}

	bool cVulkanTexture::CreateFromRawData(const cVector3l &avSize, ePixelFormat aPixelFormat, unsigned char *apData)
	{
		mvSize = cVector3l(std::max(1, avSize.x), std::max(1, avSize.y), std::max(1, avSize.z));
		mPixelFormat = aPixelFormat;
		mbIsCompressed = PixelFormatIsCompressed(aPixelFormat);
		if(mType == eTextureType_Rect || (mbIsCompressed && apData)) mbUseMipMaps = false;
		int lLevels = mbUseMipMaps ? FullMipCount(mvSize) : 1;
		if(!CreateImage(0, mvSize, aPixelFormat, lLevels)) return false;
		int lLayers = mType == eTextureType_CubeMap ? 6 : 1;
		if(apData)
		{
			int lDataSize = mvSize.x * mvSize.y * mvSize.z * GetBytesPerPixel(aPixelFormat);
			for(int f=0; f<lLayers; ++f) Upload(0, 0, f, cVector3l(0,0,0), mvSize, aPixelFormat, apData, lDataSize);
			mlMemorySize = lDataSize * lLayers;
			if(mbUseMipMaps) mpGfx->GenerateMips(mvImages[0]);
		}
		mbContainsData = true;
		mbMipFilter = mbUseMipMaps && mType != eTextureType_Rect;
		mMinFilter = mMagFilter = VK_FILTER_LINEAR;
		mMipMode = mFilter == eTextureFilter_Bilinear ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
		if(mType == eTextureType_Rect) mWrapS = mWrapT = mWrapR = eTextureWrap_ClampToEdge;
		return true;
	}

	void cVulkanTexture::SetRawData(int alLevel, const cVector3l& avOffset, const cVector3l& avSize, ePixelFormat aPixelFormat, void *apData)
	{
		if(!mbContainsData || apData == NULL) return;
		int lDataSize = std::max(1, avSize.x) * std::max(1, avSize.y) * std::max(1, avSize.z) * GetBytesPerPixel(aPixelFormat);
		Upload(0, alLevel, 0, avOffset, avSize, aPixelFormat, apData, lDataSize);
	}

	void cVulkanTexture::Update(float afTimeStep)
	{
		if(mvImages.size() <= 1) return;
		float fMax = (float)mvImages.size();
		mfTimeCount += afTimeStep * (1.0f/mfFrameTime) * mfTimeDir;
		if(mfTimeDir > 0)
		{
			if(mfTimeCount >= fMax)
			{
				if(mAnimMode == eTextureAnimMode_Loop) mfTimeCount = 0;
				else { mfTimeCount = fMax - 1.0f; mfTimeDir = -1.0f; }
			}
		}
		else if(mfTimeCount < 0) { mfTimeCount = 1; mfTimeDir = 1.0f; }
	}

	bool cVulkanTexture::HasAnimation(){ return mvImages.size() > 1; }

	void cVulkanTexture::NextFrame()
	{
		mfTimeCount += mfTimeDir;
		float fMax = (float)mvImages.size();
		if(mfTimeDir > 0)
		{
			if(mfTimeCount >= fMax)
			{
				if(mAnimMode == eTextureAnimMode_Loop) mfTimeCount = 0;
				else { mfTimeCount = fMax - 1.0f; mfTimeDir = -1.0f; }
			}
		}
		else if(mfTimeCount < 0) { mfTimeCount = 1; mfTimeDir = 1.0f; }
	}

	void cVulkanTexture::PrevFrame()
	{
		mfTimeCount -= mfTimeDir;
		float fMax = (float)mvImages.size();
		if(mfTimeDir < 0)
		{
			if(mfTimeCount >= fMax)
			{
				if(mAnimMode == eTextureAnimMode_Loop) mfTimeCount = 0;
				else { mfTimeCount = fMax - 1.0f; mfTimeDir = -1.0f; }
			}
		}
		else if(mfTimeCount < 0) { mfTimeCount = 1; mfTimeDir = 1.0f; }
	}

	float cVulkanTexture::GetT(){ return cMath::Modulus(mfTimeCount, 1.0f); }
	float cVulkanTexture::GetTimeCount(){ return mfTimeCount; }
	void cVulkanTexture::SetTimeCount(float afX){ mfTimeCount = afX; }

	bool cVulkanTexture::GetRawPixelsRGBAFloat(std::vector<float> &avOut)
	{
		cVulkanImage *pImg = GetImage();
		if(pImg == NULL || mbIsCompressed) return false;
		VulkanReadPixelsFloat(mpGfx, *pImg, 0, 0, cVector2l(0,0), cVector2l(pImg->mvSize.x, pImg->mvSize.y), avOut);
		return true;
	}

	void cVulkanTexture::SetFilter(eTextureFilter aFilter)
	{
		if(mFilter == aFilter) return;
		mFilter = aFilter;
		if(!mbContainsData) return;
		if(mbUseMipMaps && mType != eTextureType_Rect)
		{
			mbMipFilter = true;
			mMagFilter = mMinFilter = aFilter == eTextureFilter_Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
			mMipMode = aFilter == eTextureFilter_Trilinear ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
		}
		else
		{
			mbMipFilter = false;
			mMagFilter = mMinFilter = aFilter == eTextureFilter_Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
		}
	}

	void cVulkanTexture::SetAnisotropyDegree(float afX)
	{
		if(!mbContainsData || afX < 1.0f || afX > mpGfx->mfMaxAnisotropy) return;
		mfAnisotropyDegree = afX;
	}

	void cVulkanTexture::SetWrapS(eTextureWrap aMode){ if(mType != eTextureType_Rect) mWrapS = aMode; }
	void cVulkanTexture::SetWrapT(eTextureWrap aMode){ if(mType != eTextureType_Rect) mWrapT = aMode; }
	void cVulkanTexture::SetWrapR(eTextureWrap aMode){ if(mType != eTextureType_Rect) mWrapR = aMode; }
	void cVulkanTexture::SetWrapSTR(eTextureWrap aMode){ if(mType != eTextureType_Rect) mWrapS = mWrapT = mWrapR = aMode; }
	void cVulkanTexture::SetCompareMode(eTextureCompareMode aMode){ mCompareMode = aMode; }
	void cVulkanTexture::SetCompareFunc(eTextureCompareFunc aFunc){ mCompareFunc = aFunc; }

	void cVulkanTexture::SetRedFromAlpha(bool abX)
	{
		if(!mbContainsData || mType != eTextureType_2D || mbRedFromAlpha == abX) return;
		mbRedFromAlpha = abX;
		UpdateView();
	}

	void cVulkanTexture::UpdateView()
	{
		VkComponentMapping swizzle = mSwizzle;
		if(mbRedFromAlpha) swizzle.r = mSwizzle.a == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_A : mSwizzle.a;
		for(cVulkanImage& img : mvImages)
		{
			if(img.mImage == VK_NULL_HANDLE) continue;
			VkImageView old = img.mView;
			VkDevice dev = mpGfx->mDevice;
			mpGfx->DeferDestroy([dev, old]{ vkDestroyImageView(dev, old, NULL); });
			VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
			vci.image = img.mImage;
			vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			vci.format = img.mFormat;
			vci.components = swizzle;
			vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, (uint32_t)img.mlLevels, 0, 1 };
			vkCreateImageView(dev, &vci, NULL, &img.mView);
		}
	}

	void cVulkanTexture::AutoGenerateMipmaps()
	{
		if(!mbUseMipMaps) return;
		for(cVulkanImage& img : mvImages) mpGfx->GenerateMips(img);
	}

	VkImageView cVulkanTexture::GetAttachView(int alLevel, int alLayer)
	{
		cVulkanImage *pImg = GetImage();
		if(pImg == NULL) return VK_NULL_HANDLE;
		int lKey = (alLevel << 16) | alLayer;
		auto it = pImg->mmapAttachViews.find(lKey);
		if(it != pImg->mmapAttachViews.end()) return it->second;
		VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		vci.image = pImg->mImage;
		vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vci.format = pImg->mFormat;
		vci.subresourceRange = { pImg->mAspect, (uint32_t)alLevel, 1, (uint32_t)alLayer, 1 };
		VkImageView view;
		vkCreateImageView(mpGfx->mDevice, &vci, NULL, &view);
		pImg->mmapAttachViews[lKey] = view;
		return view;
	}

	VkSampler cVulkanTexture::GetSampler()
	{
		VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
		sci.magFilter = mMagFilter;
		sci.minFilter = mMinFilter;
		sci.mipmapMode = mMipMode;
		sci.addressModeU = ToVkWrap(mWrapS);
		sci.addressModeV = ToVkWrap(mWrapT);
		sci.addressModeW = ToVkWrap(mWrapR);
		sci.maxLod = mbMipFilter ? VK_LOD_CLAMP_NONE : 0.0f;
		sci.anisotropyEnable = mfAnisotropyDegree > 1.0f && mbMipFilter;
		sci.maxAnisotropy = mfAnisotropyDegree;
		sci.compareEnable = mCompareMode != eTextureCompareMode_None && IsDepth();
		sci.compareOp = mCompareFunc == eTextureCompareFunc_GreaterOrEqual ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
		sci.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		return mpGfx->GetSampler(sci);
	}

	//////////////////////////////////////////////////////////////////////////
	// FRAME BUFFER
	//////////////////////////////////////////////////////////////////////////

	cVulkanDepthStencilBuffer::cVulkanDepthStencilBuffer(cLowLevelGraphicsVulkan* apGfx, const cVector2l& avSize, int alDepthBits, int alStencilBits)
		: iDepthStencilBuffer(avSize, alDepthBits, alStencilBits), mpGfx(apGfx)
	{
		VkFormat fmt = alStencilBits > 0 ? apGfx->mDepthStencilFormat : alDepthBits == 16 ? VK_FORMAT_D16_UNORM : alDepthBits == 32 ? VK_FORMAT_D32_SFLOAT : apGfx->mDepth24Format;
		if(alDepthBits == 0 && alStencilBits > 0) fmt = apGfx->mDepthStencilFormat;
		apGfx->CreateImage(mImage, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, fmt, cVector3l(avSize.x, avSize.y, 1), 1, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
		VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		vci.image = mImage.mImage;
		vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vci.format = fmt;
		vci.subresourceRange = { mImage.mAspect, 0, 1, 0, 1 };
		vkDestroyImageView(apGfx->mDevice, mImage.mView, NULL);
		vkCreateImageView(apGfx->mDevice, &vci, NULL, &mImage.mView);
	}

	cVulkanDepthStencilBuffer::~cVulkanDepthStencilBuffer(){ mpGfx->DestroyImage(mImage); }

	cVulkanFrameBuffer::cVulkanFrameBuffer(const tString& asName, cLowLevelGraphicsVulkan* apGfx) : iFrameBuffer(asName, apGfx), mpGfx(apGfx) {}

	void cVulkanFrameBuffer::SetColor(int alIdx, iTexture *apTexture, int alLevel, int alLayer)
	{
		mpGfx->FrameBufferChanged(this);
		mpColorBuffer[alIdx] = apTexture;
		mvLevel[alIdx] = alLevel;
		mvLayer[alIdx] = alLayer;
		if(alLevel > 0) mbExplicitMipLevel = true;
		if(apTexture) SetFirstSize(cVector2l(std::max(1, apTexture->GetWidth() >> alLevel), std::max(1, apTexture->GetHeight() >> alLevel)));
	}

	void cVulkanFrameBuffer::SetTexture2D(int alColorIdx, iTexture *apTexture, int alMipmapLevel)
	{
		if(apTexture && apTexture->GetType() != eTextureType_2D && apTexture->GetType() != eTextureType_Rect) return;
		SetColor(alColorIdx, apTexture, alMipmapLevel, 0);
	}

	void cVulkanFrameBuffer::SetTexture3D(int alColorIdx, iTexture *apTexture, int alZ, int alMipmapLevel)
	{
		if(apTexture && apTexture->GetType() != eTextureType_3D) return;
		SetColor(alColorIdx, apTexture, alMipmapLevel, alZ);
	}

	void cVulkanFrameBuffer::SetTextureCubeMap(int alColorIdx, iTexture *apTexture, int alFace, int alMipmapLevel)
	{
		if(apTexture && apTexture->GetType() != eTextureType_CubeMap) return;
		SetColor(alColorIdx, apTexture, alMipmapLevel, alFace);
	}

	void cVulkanFrameBuffer::SetDepthTexture2D(iTexture *apTexture, int alMipmapLevel)
	{
		mpGfx->FrameBufferChanged(this);
		mpDepthBuffer = apTexture;
		mvLevel[kMaxDrawColorBuffers] = alMipmapLevel;
		mvLayer[kMaxDrawColorBuffers] = 0;
		if(apTexture) SetFirstSize(apTexture->GetSizeInt2D());
	}

	void cVulkanFrameBuffer::SetDepthTextureCubeMap(iTexture *apTexture, int alFace, int alMipmapLevel)
	{
		mpGfx->FrameBufferChanged(this);
		mpDepthBuffer = apTexture;
		mvLevel[kMaxDrawColorBuffers] = alMipmapLevel;
		mvLayer[kMaxDrawColorBuffers] = alFace;
		if(apTexture) SetFirstSize(apTexture->GetSizeInt2D());
	}

	void cVulkanFrameBuffer::SetDepthStencilBuffer(iDepthStencilBuffer* apBuffer)
	{
		mpGfx->FrameBufferChanged(this);
		if(apBuffer == NULL) { mpDepthBuffer = mpStencilBuffer = NULL; return; }
		if(apBuffer->GetDepthBits() > 0) mpDepthBuffer = apBuffer;
		if(apBuffer->GetStencilBits() > 0) mpStencilBuffer = apBuffer;
		SetFirstSize(apBuffer->GetSize());
	}

	bool cVulkanFrameBuffer::CompileAndValidate()
	{
		mpGfx->FrameBufferChanged(this);
		return true;
	}

	void cVulkanFrameBuffer::PostBindUpdate()
	{
		if(mbExplicitMipLevel) return;
		for(int i=0; i<kMaxDrawColorBuffers; ++i)
		{
			iFrameBufferAttachment *pAtt = mpColorBuffer[i];
			if(pAtt && pAtt->GetFrameBufferAttachmentType() == eFrameBufferAttachment_Texture)
			{
				iTexture *pTex = static_cast<iTexture*>(pAtt);
				if(pTex->UsesMipMaps()) pTex->AutoGenerateMipmaps();
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// VERTEX BUFFER
	//////////////////////////////////////////////////////////////////////////

	static int ElementLocation(eVertexBufferElement aType)
	{
		switch(aType)
		{
		case eVertexBufferElement_Position: return eGlslVkAttrib_Vertex;
		case eVertexBufferElement_Normal: return eGlslVkAttrib_Normal;
		case eVertexBufferElement_Color0: return eGlslVkAttrib_Color;
		case eVertexBufferElement_Color1: return eGlslVkAttrib_SecondaryColor;
		case eVertexBufferElement_Texture1Tangent: return eGlslVkAttrib_MultiTexCoord0 + 1;
		case eVertexBufferElement_Texture0: return eGlslVkAttrib_MultiTexCoord0;
		case eVertexBufferElement_Texture1: return eGlslVkAttrib_MultiTexCoord0 + 1;
		case eVertexBufferElement_Texture2: return eGlslVkAttrib_MultiTexCoord0 + 2;
		case eVertexBufferElement_Texture3: return eGlslVkAttrib_MultiTexCoord0 + 3;
		case eVertexBufferElement_Texture4: return eGlslVkAttrib_MultiTexCoord0 + 4;
		default: return -1;
		}
	}

	static VkFormat ElementFormat(eVertexBufferElementFormat aFormat, int alNum, bool abColor)
	{
		static const VkFormat vFloat[] = { VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT };
		static const VkFormat vUnorm[] = { VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
		static const VkFormat vUscaled[] = { VK_FORMAT_R8_USCALED, VK_FORMAT_R8G8_USCALED, VK_FORMAT_R8G8B8_USCALED, VK_FORMAT_R8G8B8A8_USCALED };
		int i = cMath::Clamp(alNum, 1, 4) - 1;
		if(aFormat == eVertexBufferElementFormat_Byte) return abColor ? vUnorm[i] : vUscaled[i];
		return vFloat[i];
	}

	cVulkanVertexBuffer::cVulkanVertexBuffer(cLowLevelGraphicsVulkan* apGfx, eVertexBufferDrawType aDrawType, eVertexBufferUsageType aUsageType,
											 int alReserveVtxSize, int alReserveIdxSize)
		: iVertexBufferOpenGL(apGfx, eVertexBufferType_Hardware, aDrawType, aUsageType, alReserveVtxSize, alReserveIdxSize), mpGfx(apGfx) {}

	cVulkanVertexBuffer::~cVulkanVertexBuffer()
	{
		if(mBuffer)
		{
			VmaAllocator alloc = mpGfx->mpAllocator;
			VkBuffer buf = mBuffer;
			VmaAllocation mem = mpAlloc;
			mpGfx->DeferDestroy([alloc, buf, mem]{ vmaDestroyBuffer(alloc, buf, mem); });
		}
	}

	void cVulkanVertexBuffer::CompileSpecific(){ mbDirty = true; }
	void cVulkanVertexBuffer::UpdateData(tVertexElementFlag aTypes, bool abIndices){ mbDirty = true; }
	void cVulkanVertexBuffer::Bind(){}
	void cVulkanVertexBuffer::UnBind(){}

	iVertexBufferOpenGL* cVulkanVertexBuffer::CreateDataCopy(tVertexElementFlag aFlags, eVertexBufferDrawType aDrawType, eVertexBufferUsageType aUsageType,
															 int alReserveVtxSize, int alReserveIdxSize)
	{
		return hplNew(cVulkanVertexBuffer, (mpGfx, aDrawType, aUsageType, alReserveVtxSize, alReserveIdxSize));
	}

	void cVulkanVertexBuffer::Upload()
	{
		size_t lSize = 0;
		for(cVtxBufferGLElementArray *pEl : mvElementArrays)
		{
			lSize = (lSize + 15) & ~(size_t)15;
			mvElementOffset[pEl->mType] = lSize;
			lSize += pEl->Size() * GetVertexFormatByteSize(pEl->mFormat);
		}
		lSize = (lSize + 15) & ~(size_t)15;
		mlIndexOffset = lSize;
		lSize += mvIndexArray.size() * sizeof(unsigned int);

		// A buffer the GPU may still read gets replaced instead of overwritten
		if(mBuffer && (lSize > mlBufferSize || mlLastUseSerial > mpGfx->mlCompletedSerial))
		{
			VmaAllocator alloc = mpGfx->mpAllocator;
			VkBuffer buf = mBuffer;
			VmaAllocation mem = mpAlloc;
			mpGfx->DeferDestroy([alloc, buf, mem]{ vmaDestroyBuffer(alloc, buf, mem); });
			mBuffer = VK_NULL_HANDLE;
		}
		if(mBuffer == VK_NULL_HANDLE)
		{
			mlBufferSize = lSize;
			mpGfx->CreateBuffer(lSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, true, mBuffer, mpAlloc, &mpMapped);
		}
		unsigned char *pDst = (unsigned char*)mpMapped;
		for(cVtxBufferGLElementArray *pEl : mvElementArrays)
			memcpy(pDst + mvElementOffset[pEl->mType], pEl->GetArrayPtr(), pEl->Size() * GetVertexFormatByteSize(pEl->mFormat));
		if(!mvIndexArray.empty()) memcpy(pDst + mlIndexOffset, mvIndexArray.data(), mvIndexArray.size() * sizeof(unsigned int));
		mbDirty = false;
	}

	void cVulkanVertexBuffer::DrawRange(const unsigned int *apIndices, int alCount, eVertexBufferDrawType aDrawType)
	{
		if(mbDirty) Upload();
		if(mBuffer == VK_NULL_HANDLE || alCount <= 0) return;
		mlLastUseSerial = mpGfx->mlSubmitSerial;
		cEngineDiagnostics::CountDrawCall();

		cLowLevelGraphicsVulkan::cAttribStream vStreams[eGlslVkAttrib_LastEnum] = {};
		for(cVtxBufferGLElementArray *pEl : mvElementArrays)
		{
			int lLoc = ElementLocation(pEl->mType);
			if(lLoc < 0) continue;
			bool bColor = pEl->mType == eVertexBufferElement_Color0 || pEl->mType == eVertexBufferElement_Color1;
			vStreams[lLoc] = { mBuffer, mvElementOffset[pEl->mType], (uint32_t)(pEl->mlElementNum * GetVertexFormatByteSize(pEl->mFormat)),
							   ElementFormat(pEl->mFormat, pEl->mlElementNum, bColor) };
		}

		// Primitives Vulkan lacks become triangle/line lists
		cLowLevelGraphicsVulkan::ePrim prim = cLowLevelGraphicsVulkan::ePrim_Triangles;
		std::vector<unsigned int> vConverted;
		const unsigned int *pIdx = apIndices;
		int lCount = alCount;
		switch(aDrawType)
		{
		case eVertexBufferDrawType_TriStrip: prim = cLowLevelGraphicsVulkan::ePrim_TriStrip; break;
		case eVertexBufferDrawType_TriFan: prim = cLowLevelGraphicsVulkan::ePrim_TriFan; break;
		case eVertexBufferDrawType_Line: prim = cLowLevelGraphicsVulkan::ePrim_Lines; break;
		case eVertexBufferDrawType_LineStrip: prim = cLowLevelGraphicsVulkan::ePrim_LineStrip; break;
		case eVertexBufferDrawType_LineLoop:
			prim = cLowLevelGraphicsVulkan::ePrim_LineStrip;
			vConverted.assign(apIndices, apIndices + alCount);
			vConverted.push_back(apIndices[0]);
			break;
		case eVertexBufferDrawType_Quad:
			for(int q=0; q+3<alCount; q+=4)
				for(int k : {0, 1, 2, 0, 2, 3}) vConverted.push_back(apIndices[q+k]);
			break;
		case eVertexBufferDrawType_QuadStrip:
			for(int q=0; q+3<alCount; q+=2)
				for(int k : {0, 1, 3, 0, 3, 2}) vConverted.push_back(apIndices[q+k]);
			break;
		default: break;
		}
		if(!vConverted.empty() || aDrawType == eVertexBufferDrawType_Quad || aDrawType == eVertexBufferDrawType_QuadStrip)
		{
			pIdx = vConverted.data();
			lCount = (int)vConverted.size();
		}
		if(lCount == 0) return;

		VkBuffer ib = mBuffer;
		VkDeviceSize ibOffset = mlIndexOffset;
		if(pIdx != mvIndexArray.data())
			memcpy(mpGfx->AllocTransient(lCount * 4, ib, ibOffset, 4), pIdx, lCount * 4);
		mpGfx->Draw(vStreams, prim, lCount, ib, ibOffset);
	}

	void cVulkanVertexBuffer::Draw(eVertexBufferDrawType aDrawType)
	{
		eVertexBufferDrawType drawType = aDrawType == eVertexBufferDrawType_LastEnum ? mDrawType : aDrawType;
		int lSize = mlElementNum < 0 ? GetIndexNum() : mlElementNum;
		DrawRange(mvIndexArray.data(), lSize, drawType);
	}

	void cVulkanVertexBuffer::DrawIndices(unsigned int *apIndices, int alCount, eVertexBufferDrawType aDrawType)
	{
		DrawRange(apIndices, alCount, aDrawType == eVertexBufferDrawType_LastEnum ? mDrawType : aDrawType);
	}

	//////////////////////////////////////////////////////////////////////////
	// SHADERS
	//////////////////////////////////////////////////////////////////////////

	cVulkanShader::cVulkanShader(const tString& asName, eGpuShaderType aType) : iGpuShader(asName, _W(""), aType, eGpuProgramFormat_GLSL) {}

	bool cVulkanShader::CreateFromFile(const tWString& asFile, const tString& asEntry, bool abPrintInfoIfFail)
	{
		unsigned int lSize = cPlatform::GetFileSize(asFile);
		if(lSize == 0) { Error("Could not open file %ls\n", asFile.c_str()); return false; }
		SetFullPath(asFile);
		tString sData(lSize, '\0');
		cPlatform::CopyFileToBuffer(asFile, &sData[0], lSize);
		return CreateFromString(sData.c_str(), asEntry, abPrintInfoIfFail);
	}

	bool cVulkanShader::CreateFromString(const char *apStringData, const tString& asEntry, bool abPrintInfoIfFail)
	{
		// Compiled at link time: the rewrite needs both stages
		msSource = apStringData;
		return true;
	}

	cVulkanProgram::cVulkanProgram(const tString& asName, cLowLevelGraphicsVulkan* apGfx) : iGpuProgram(asName, eGpuProgramFormat_GLSL), mpGfx(apGfx)
	{
		for(int& o : mvBuiltinOffset) o = -1;
	}

	cVulkanProgram::~cVulkanProgram()
	{
		if(mpGfx->mpProgram == this) mpGfx->mpProgram = NULL;
		VkDevice dev = mpGfx->mDevice;
		auto destroy = mpGfx->vkDestroyShaderEXT_;
		VkShaderEXT v0 = mvShaders[0], v1 = mvShaders[1];
		if(v0 || v1) mpGfx->DeferDestroy([dev, destroy, v0, v1]{ if(v0) destroy(dev, v0, NULL); if(v1) destroy(dev, v1, NULL); });
	}

	bool cVulkanProgram::Link()
	{
		cVulkanShader *pVtx = static_cast<cVulkanShader*>(mpShader[eGpuShaderType_Vertex]);
		cVulkanShader *pFrag = static_cast<cVulkanShader*>(mpShader[eGpuShaderType_Fragment]);
		if(pVtx == NULL || pFrag == NULL) { Error("Vulkan program '%s' needs both stages\n", msName.c_str()); return false; }
		if(!LinkFromSource(pVtx->msSource, pFrag->msSource)) return false;
		for(iGpuShader *pShader : {(iGpuShader*)pVtx, (iGpuShader*)pFrag})
			for(int i=0; i<pShader->GetSamplerUnitNum(); ++i)
				SetSamplerToUnit(pShader->GetSamplerUnit(i)->msName, pShader->GetSamplerUnit(i)->mlUnit);
		return true;
	}

	bool cVulkanProgram::LinkFromSource(const tString& asVertex, const tString& asFragment)
	{
		unsigned long lStart = cPlatform::GetApplicationTime();
		cGlslVkProgram prog;
		std::vector<unsigned int> vSpirv[2];
		std::string sLog;
		bool bOk = GlslToVulkan(asVertex, asFragment, prog);
		if(!bOk) sLog = prog.msError;
		for(int s=0; s<2 && bOk; ++s)
			bOk = CompileGlslToSpirv(s == 0 ? prog.msVertex : prog.msFragment, s == 0, vSpirv[s], sLog);
		if(bOk) bOk = mpGfx->CreateShaders(vSpirv[0], vSpirv[1], mvShaders);
		cEngineDiagnostics::AddLinkTime((double)(cPlatform::GetApplicationTime() - lStart));
		cEngineDiagnostics::ReportShader(msName, "link", bOk, sLog);
		if(!bOk)
		{
			Error("Failed to build Vulkan program '%s': %s\n", msName.c_str(), sLog.c_str());
			if(getenv("OPENHPL_VK_SHADER_LOG")) Log("Vertex:\n%s\nFragment:\n%s\n", prog.msVertex.c_str(), prog.msFragment.c_str());
			return false;
		}

		mvSamplers = prog.mvSamplers;
		mvSamplerUnit.assign(mvSamplers.size(), -1);
		mlAttribMask = prog.mlAttribMask;
		mlFragOutputs = prog.mlFragOutputs;

		// std140 offsets come from glslang reflection of the shared block
		mvUniforms.clear();
		size_t lBlockSize = 16;
		{
			glslang::TShader *pShaders[2];
			glslang::TProgram program;
			EShMessages msgs = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
			const std::string* vSrc[2] = { &prog.msVertex, &prog.msFragment };
			for(int s=0; s<2; ++s)
			{
				pShaders[s] = new glslang::TShader(s == 0 ? EShLangVertex : EShLangFragment);
				const char *pSrc = vSrc[s]->c_str();
				pShaders[s]->setStrings(&pSrc, 1);
				pShaders[s]->setEnvInput(glslang::EShSourceGlsl, s == 0 ? EShLangVertex : EShLangFragment, glslang::EShClientVulkan, 100);
				pShaders[s]->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
				pShaders[s]->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);
				pShaders[s]->parse(GetDefaultResources(), 450, false, msgs);
				program.addShader(pShaders[s]);
			}
			if(program.link(msgs) && program.buildReflection(EShReflectionAllBlockVariables))
			{
				for(int b=0; b<program.getNumUniformBlocks(); ++b)
					if(program.getUniformBlock(b).name == "hpl_Uniforms") lBlockSize = std::max<size_t>(lBlockSize, program.getUniformBlock(b).size);
				for(int i=0; i<program.getNumUniformVariables(); ++i)
				{
					const glslang::TObjectReflection& u = program.getUniform(i);
					if(u.offset < 0) continue;
					cVulkanUniform vu;
					vu.msName = u.name;
					size_t lBracket = vu.msName.find('[');
					if(lBracket != tString::npos) vu.msName.resize(lBracket);
					vu.mlOffset = u.offset;
					vu.mlArrayStride = u.arrayStride > 0 ? u.arrayStride : 16;
					vu.mlCount = std::max(1, u.size);
					// GL type enums: FLOAT_MAT3 0x8B5B, FLOAT_MAT4 0x8B5C, INT* 0x1404/0x8B53-55, UNSIGNED_INT 0x1405, BOOL* 0x8B56-59
					int lType = u.glDefineType;
					vu.mlMatrixColumns = lType == 0x8B5B ? 3 : lType == 0x8B5C ? 4 : 0;
					vu.mbInt = lType == 0x1404 || lType == 0x1405 || (lType >= 0x8B53 && lType <= 0x8B59);
					mvUniforms.push_back(vu);
				}
			}
			for(int s=0; s<2; ++s) delete pShaders[s];
		}
		mvUniformData.assign(lBlockSize, 0);
		static const char *vBuiltins[] = { "hpl_AlphaTest", "hpl_ModelViewProjectionMatrix", "hpl_ModelViewMatrix", "hpl_ProjectionMatrix", "hpl_NormalMatrix", "hpl_ClipPlane" };
		for(int i=0; i<6; ++i)
		{
			int lIdx = FindUniform(vBuiltins[i]);
			mvBuiltinOffset[i] = lIdx >= 0 ? mvUniforms[lIdx].mlOffset : -1;
		}
		for(const cGlslVkUniformInit& init : prog.mvInits)
		{
			int lId = GetVariableId(init.msName);
			if(lId >= 0 && !init.mvValues.empty()) Write(lId, init.mvValues.data(), (int)std::min<size_t>(init.mvValues.size(), 4), 1);
		}
		return true;
	}

	void cVulkanProgram::Bind(){ mpGfx->mpProgram = this; }
	void cVulkanProgram::UnBind(){ if(mpGfx->mpProgram == this) mpGfx->mpProgram = NULL; }

	bool cVulkanProgram::SetSamplerToUnit(const tString& asSamplerName, int alUnit)
	{
		for(size_t i=0; i<mvSamplers.size(); ++i)
			if(mvSamplers[i].msName == asSamplerName) { mvSamplerUnit[i] = alUnit; return true; }
		return false;
	}

	int cVulkanProgram::FindUniform(const tString& asName)
	{
		for(size_t i=0; i<mvUniforms.size(); ++i) if(mvUniforms[i].msName == asName) return (int)i;
		return -1;
	}

	int cVulkanProgram::GetVariableId(const tString& asName)
	{
		for(size_t i=0; i<mvParameters.size(); ++i) if(mvParameters[i].msName == asName) return (int)i;
		int lUniform = FindUniform(asName);
		if(lUniform < 0) return -1;
		mvParameters.push_back({asName, lUniform});
		return (int)mvParameters.size() - 1;
	}

	bool cVulkanProgram::GetVariableAsId(const tString& asName, int alId)
	{
		if(alId < 0) return false;
		for(size_t i=0; i<mvParameters.size(); ++i)
			if(mvParameters[i].msName == asName) return (int)i == alId;
		int lUniform = FindUniform(asName);
		if(lUniform < 0) return false;
		if(alId < (int)mvParameters.size())
		{
			if(mvParameters[alId].mlUniform >= 0) return mvParameters[alId].mlUniform == lUniform;
		}
		else
			mvParameters.resize(alId + 1);
		mvParameters[alId] = {asName, lUniform};
		return true;
	}

	bool cVulkanProgram::Write(int alVarId, const float *apData, int alComponents, int alCount, bool abInt)
	{
		if(alVarId < 0 || alVarId >= (int)mvParameters.size() || mvParameters[alVarId].mlUniform < 0) return false;
		const cVulkanUniform& u = mvUniforms[mvParameters[alVarId].mlUniform];
		int lCount = std::min(alCount, u.mlCount);
		for(int e=0; e<lCount; ++e)
		{
			unsigned char *pDst = mvUniformData.data() + u.mlOffset + e * u.mlArrayStride;
			if(pDst + alComponents * 4 > mvUniformData.data() + mvUniformData.size()) return false;
			for(int c=0; c<alComponents; ++c)
			{
				if(u.mbInt) { int32_t v = (int32_t)apData[e*alComponents + c]; memcpy(pDst + c*4, &v, 4); }
				else memcpy(pDst + c*4, &apData[e*alComponents + c], 4);
			}
		}
		return true;
	}

	bool cVulkanProgram::SetInt(int alVarId, int alX)
	{
		if(alVarId < 0 || alVarId >= (int)mvParameters.size() || mvParameters[alVarId].mlUniform < 0) return false;
		const cVulkanUniform& u = mvUniforms[mvParameters[alVarId].mlUniform];
		if(u.mbInt) { memcpy(mvUniformData.data() + u.mlOffset, &alX, 4); return true; }
		float f = (float)alX;
		return Write(alVarId, &f, 1, 1);
	}

	bool cVulkanProgram::SetFloat(int alVarId, float afX){ return Write(alVarId, &afX, 1, 1); }
	bool cVulkanProgram::SetVec2f(int alVarId, float afX, float afY){ float v[2] = {afX, afY}; return Write(alVarId, v, 2, 1); }
	bool cVulkanProgram::SetVec3f(int alVarId, float afX, float afY, float afZ){ float v[3] = {afX, afY, afZ}; return Write(alVarId, v, 3, 1); }
	bool cVulkanProgram::SetVec4f(int alVarId, float afX, float afY, float afZ, float afW){ float v[4] = {afX, afY, afZ, afW}; return Write(alVarId, v, 4, 1); }
	bool cVulkanProgram::SetFloatArray(int alVarId, const float *apX, int alCount){ return Write(alVarId, apX, 1, alCount); }

	bool cVulkanProgram::SetMatrixf(int alVarId, const cMatrixf& aMtx)
	{
		if(alVarId < 0 || alVarId >= (int)mvParameters.size() || mvParameters[alVarId].mlUniform < 0) return false;
		const cVulkanUniform& u = mvUniforms[mvParameters[alVarId].mlUniform];
		int lCols = u.mlMatrixColumns ? u.mlMatrixColumns : 4;
		if(u.mlOffset + lCols * 16 > (int)mvUniformData.size()) return false;
		float *pDst = (float*)(mvUniformData.data() + u.mlOffset);
		for(int c=0; c<lCols; ++c)
			for(int r=0; r<4; ++r)
				pDst[c*4 + r] = r < lCols ? aMtx.m[r][c] : 0.0f;
		return true;
	}

	tString cVulkanProgram::DumpUniforms()
	{
		tString sOut;
		for(const cVulkanUniform& u : mvUniforms)
		{
			const float *p = (const float*)(mvUniformData.data() + u.mlOffset);
			int lN = u.mlMatrixColumns ? u.mlMatrixColumns * 4 : 4;
			sOut += u.msName;
			for(int j=0; j<lN && u.mlOffset + (j+1)*4 <= (int)mvUniformData.size(); ++j) sOut += " " + cString::ToString(p[j]);
			sOut += "\n";
		}
		return sOut;
	}

	//////////////////////////////////////////////////////////////////////////
	// OCCLUSION QUERY
	//////////////////////////////////////////////////////////////////////////

	cVulkanOcclusionQuery::cVulkanOcclusionQuery(cLowLevelGraphicsVulkan* apGfx) : mpGfx(apGfx)
	{
		mlIndex = UINT32_MAX;
		if(!apGfx->mvFreeQueries.empty()) { mlIndex = apGfx->mvFreeQueries.back(); apGfx->mvFreeQueries.pop_back(); }
	}

	cVulkanOcclusionQuery::~cVulkanOcclusionQuery()
	{
		if(mlIndex == UINT32_MAX) return;
		if(mbPending) FetchResults();
		mpGfx->mvFreeQueries.push_back(mlIndex);
	}

	void cVulkanOcclusionQuery::Begin()
	{
		if(mlIndex == UINT32_MAX) return;
		if(mbPending) { while(!FetchResults()) {} }
		vkResetQueryPool(mpGfx->mDevice, mpGfx->mQueryPool, mlIndex, 1);
		mpGfx->BeginQuery(mlIndex);
	}

	void cVulkanOcclusionQuery::End()
	{
		if(mlIndex == UINT32_MAX) return;
		mpGfx->EndQuery(mlIndex);
		mlSubmitSerial = mpGfx->mlSubmitSerial;
		mbPending = true;
	}

	bool cVulkanOcclusionQuery::FetchResults()
	{
		if(!mbPending) return true;
		// GL drivers answer queries from the same frame; flush so the renderer's culling sees results
		if(mlSubmitSerial >= mpGfx->mlSubmitSerial) mpGfx->SubmitAndWait();
		uint64_t vResult[2] = {0, 0};
		VkResult res = vkGetQueryPoolResults(mpGfx->mDevice, mpGfx->mQueryPool, mlIndex, 1, sizeof(vResult), vResult, sizeof(vResult),
											 VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		if(res == VK_NOT_READY || vResult[1] == 0) return false;
		mlSampleCount = (unsigned int)vResult[0];
		mbPending = false;
		return true;
	}
}
