#include "impl/LowLevelGraphicsVulkan.h"

#include "impl/SDLFontData.h"
#include "graphics/Bitmap.h"
#include "math/Math.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/String.h"

#include <SDL2/SDL_vulkan.h>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace hpl {

	static PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT_;
	static PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT_;
	static PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR_;
	static PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT_;
	static PFN_vkCmdSetRasterizationSamplesEXT vkCmdSetRasterizationSamplesEXT_;
	static PFN_vkCmdSetSampleMaskEXT vkCmdSetSampleMaskEXT_;
	static PFN_vkCmdSetAlphaToCoverageEnableEXT vkCmdSetAlphaToCoverageEnableEXT_;
	static PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT_;
	static PFN_vkCmdSetColorBlendEquationEXT vkCmdSetColorBlendEquationEXT_;
	static PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT_;
	static PFN_vkCmdSetDepthClampEnableEXT vkCmdSetDepthClampEnableEXT_;
	static PFN_vkSetHdrMetadataEXT vkSetHdrMetadataEXT_;

	static const int kSamplerBindings = 24;
	static const size_t kRingSize = 16 << 20;

	static bool VkCheck(VkResult aRes, const char *apWhat)
	{
		if(aRes == VK_SUCCESS) return true;
		Error("Vulkan: %s failed (%d)\n", apWhat, (int)aRes);
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// ENUM CONVERSION
	//////////////////////////////////////////////////////////////////////////

	static VkCompareOp ToVkCompare(int aFunc) // eDepthTestFunc / eStencilFunc / eAlphaTestFunc share order
	{
		static const VkCompareOp v[] = { VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER,
										 VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_ALWAYS };
		return aFunc >= 0 && aFunc < 8 ? v[aFunc] : VK_COMPARE_OP_ALWAYS;
	}

	static VkStencilOp ToVkStencilOp(eStencilOp aOp)
	{
		switch(aOp)
		{
		case eStencilOp_Keep: return VK_STENCIL_OP_KEEP;
		case eStencilOp_Zero: return VK_STENCIL_OP_ZERO;
		case eStencilOp_Replace: return VK_STENCIL_OP_REPLACE;
		case eStencilOp_Increment: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
		case eStencilOp_Decrement: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		case eStencilOp_Invert: return VK_STENCIL_OP_INVERT;
		case eStencilOp_IncrementWrap: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
		case eStencilOp_DecrementWrap: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
		default: return VK_STENCIL_OP_KEEP;
		}
	}

	static VkBlendFactor ToVkBlend(eBlendFunc aFunc)
	{
		switch(aFunc)
		{
		case eBlendFunc_Zero: return VK_BLEND_FACTOR_ZERO;
		case eBlendFunc_One: return VK_BLEND_FACTOR_ONE;
		case eBlendFunc_SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
		case eBlendFunc_OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case eBlendFunc_DestColor: return VK_BLEND_FACTOR_DST_COLOR;
		case eBlendFunc_OneMinusDestColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case eBlendFunc_SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
		case eBlendFunc_OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case eBlendFunc_DestAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
		case eBlendFunc_OneMinusDestAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case eBlendFunc_SrcAlphaSaturate: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		default: return VK_BLEND_FACTOR_ONE;
		}
	}

	static void WriteMatrix(unsigned char *apDst, const cMatrixf& aMtx, int alColumns)
	{
		float *pDst = (float*)apDst;
		for(int c=0; c<alColumns; ++c)
			for(int r=0; r<4; ++r)
				pDst[c*4 + r] = r < alColumns ? aMtx.m[r][c] : 0.0f;
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	cLowLevelGraphicsVulkan::cLowLevelGraphicsVulkan()
	{
		mpVertexArray = (float*)hplMalloc(sizeof(float) * mlBatchStride * mlBatchArraySize);
		mpIndexArray = (unsigned int*)hplMalloc(sizeof(unsigned int) * mlBatchArraySize);
		for(int i=0; i<kMaxTextureUnits; ++i)
		{
			mpTexCoordArray[i] = (float*)hplMalloc(sizeof(float) * 3 * mlBatchArraySize);
			mlTexCoordArrayCount[i] = 0;
			mbBatchUnitOwnCoords[i] = false;
		}
		for(int i=0; i<eMatrix_LastEnum; ++i) mvMatrix[i] = cMatrixf::Identity;
		mState.mBlend = { VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD };
		VkStencilOpState stencil = { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xFF, 0xFF, 0 };
		mState.mStencilFront = mState.mStencilBack = stencil;
		memset(&mImCurrent, 0, sizeof(mImCurrent));
		for(int i=0; i<4; ++i) mImCurrent.mvTex[i][3] = 1;
	}

	cLowLevelGraphicsVulkan::~cLowLevelGraphicsVulkan()
	{
		if(mDevice)
		{
			vkDeviceWaitIdle(mDevice);
			for(cFrame& f : mvFrames) { for(auto& d : f.mvDestroy) d(); f.mvDestroy.clear(); }
			for(auto& d : mvDestroyNow) d();
			for(auto& it : mmapFixedFunction) hplDelete(it.second);
			if(mpPresentProgram) hplDelete(mpPresentProgram);
			for(cFrame& f : mvFrames) { for(auto& d : f.mvDestroy) d(); f.mvDestroy.clear(); }
			for(auto& it : mmapDummyImages) DestroyImage(it.second);
			DestroyImage(mScreenColor);
			DestroyImage(mScreenDepth);
			for(auto& it : mmapSamplers) vkDestroySampler(mDevice, it.second, NULL);
			for(cRingBuffer& r : mvRing) vmaDestroyBuffer(mpAllocator, r.mBuffer, r.mpAlloc);
			DestroySwapchain();
			for(cFrame& f : mvFrames) { vkDestroyFence(mDevice, f.mFence, NULL); vkDestroySemaphore(mDevice, f.mAcquired, NULL); }
			vkDestroyQueryPool(mDevice, mQueryPool, NULL);
			vkDestroyCommandPool(mDevice, mCommandPool, NULL);
			vkDestroyPipelineLayout(mDevice, mPipelineLayout, NULL);
			vkDestroyDescriptorSetLayout(mDevice, mSetLayout, NULL);
			vmaDestroyAllocator(mpAllocator);
			vkDestroyDevice(mDevice, NULL);
		}
		if(mSurface) vkDestroySurfaceKHR(mInstance, mSurface, NULL);
		if(mInstance) vkDestroyInstance(mInstance, NULL);
		if(mpWindow) SDL_DestroyWindow(mpWindow);
		hplFree(mpVertexArray);
		hplFree(mpIndexArray);
		for(int i=0; i<kMaxTextureUnits; ++i) hplFree(mpTexCoordArray[i]);
	}

	//////////////////////////////////////////////////////////////////////////
	// SETUP
	//////////////////////////////////////////////////////////////////////////

	bool cLowLevelGraphicsVulkan::Init(int alWidth, int alHeight, int alDisplay, int alBpp, int abFullscreen, int alMultisampling,
										eGpuProgramFormat aGpuProgramFormat, const tString& asWindowCaption, const cVector2l &avWindowPos)
	{
		mvScreenSize = cVector2l(alWidth, alHeight);
		mbFullscreen = abFullscreen;
		mbHeadless = getenv("OPENHPL_HEADLESS_SOCKET") != NULL;

		if(const char *pHdr = getenv("OPENHPL_HDR"))
		{
			tString s = cString::ToLowerCase(pHdr);
			mlOutputMode = s == "hdr10" || s == "pq" || s == "1" ? 1 : s == "scrgb" ? 2 : s == "p3" || s == "wcg" ? 3 : 0;
		}
		if(const char *p = getenv("OPENHPL_HDR_PAPER_WHITE")) mfPaperWhite = (float)atof(p);
		if(const char *p = getenv("OPENHPL_HDR_PEAK")) mfPeakNits = (float)atof(p);

		unsigned int lFlags = SDL_WINDOW_VULKAN;
		if(alWidth == 0 && alHeight == 0) { mvScreenSize = cVector2l(800,600); lFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP; }
		else if(abFullscreen)
		{
			const char *pDriver = SDL_GetCurrentVideoDriver();
			lFlags |= pDriver && cString::ToLowerCase(pDriver) == "wayland" ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
		}
		if(mbHeadless) lFlags |= SDL_WINDOW_HIDDEN;

		Log(" Setting video mode: %d x %d (Vulkan)\n", mvScreenSize.x, mvScreenSize.y);
		mpWindow = SDL_CreateWindow(asWindowCaption.c_str(), SDL_WINDOWPOS_CENTERED_DISPLAY(alDisplay), SDL_WINDOWPOS_CENTERED_DISPLAY(alDisplay),
									mvScreenSize.x, mvScreenSize.y, lFlags);
		if(mpWindow == NULL) { FatalError("Unable to create Vulkan window! %s\n", SDL_GetError()); return false; }
		SDL_GetWindowSize(mpWindow, &mvScreenSize.x, &mvScreenSize.y);

		if(!CreateDevice()) { FatalError("Unable to initialize Vulkan\n"); return false; }
		BeginCommandBuffer();
		CreateScreenTargets();
		if(!mbHeadless && !CreateSwapchain()) { FatalError("Unable to create Vulkan swapchain\n"); return false; }

		if(mbGrab) SetWindowGrab(true);
		ShowCursor(false);
		SetCurrentFrameBuffer(NULL);
		return true;
	}

	bool cLowLevelGraphicsVulkan::CreateDevice()
	{
		uint32_t lExtCount = 0;
		std::vector<const char*> vExt;
		if(SDL_Vulkan_GetInstanceExtensions(mpWindow, &lExtCount, NULL))
		{
			vExt.resize(lExtCount);
			SDL_Vulkan_GetInstanceExtensions(mpWindow, &lExtCount, vExt.data());
		}
		uint32_t lAvail = 0;
		vkEnumerateInstanceExtensionProperties(NULL, &lAvail, NULL);
		std::vector<VkExtensionProperties> vAvail(lAvail);
		vkEnumerateInstanceExtensionProperties(NULL, &lAvail, vAvail.data());
		for(auto& e : vAvail)
			if(!strcmp(e.extensionName, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME)) vExt.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);

		std::vector<const char*> vLayers;
		if(getenv("OPENHPL_VK_VALIDATION")) vLayers.push_back("VK_LAYER_KHRONOS_validation");

		VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
		app.pApplicationName = "open-hpl";
		app.pEngineName = "HPL2";
		app.apiVersion = VK_API_VERSION_1_3;
		VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
		ici.pApplicationInfo = &app;
		ici.enabledExtensionCount = (uint32_t)vExt.size();
		ici.ppEnabledExtensionNames = vExt.data();
		ici.enabledLayerCount = (uint32_t)vLayers.size();
		ici.ppEnabledLayerNames = vLayers.data();
		if(!VkCheck(vkCreateInstance(&ici, NULL, &mInstance), "vkCreateInstance")) return false;

		if(!mbHeadless && !SDL_Vulkan_CreateSurface(mpWindow, mInstance, &mSurface))
		{
			Error("SDL_Vulkan_CreateSurface: %s\n", SDL_GetError());
			return false;
		}

		uint32_t lDevCount = 0;
		vkEnumeratePhysicalDevices(mInstance, &lDevCount, NULL);
		std::vector<VkPhysicalDevice> vDevs(lDevCount);
		vkEnumeratePhysicalDevices(mInstance, &lDevCount, vDevs.data());
		for(VkPhysicalDevice d : vDevs)
		{
			VkPhysicalDeviceProperties p;
			vkGetPhysicalDeviceProperties(d, &p);
			if(p.apiVersion < VK_API_VERSION_1_3) continue;
			if(mPhysicalDevice == VK_NULL_HANDLE || p.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU) mPhysicalDevice = d;
			if(p.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU) break;
		}
		if(mPhysicalDevice == VK_NULL_HANDLE) { Error("No Vulkan 1.3 device\n"); return false; }

		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);
		mfMaxAnisotropy = props.limits.maxSamplerAnisotropy;
		Log("  Vulkan device: %s\n", props.deviceName);

		uint32_t lQueueCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &lQueueCount, NULL);
		std::vector<VkQueueFamilyProperties> vQueues(lQueueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &lQueueCount, vQueues.data());
		for(uint32_t i=0; i<lQueueCount; ++i)
		{
			VkBool32 bPresent = VK_TRUE;
			if(mSurface) vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, i, mSurface, &bPresent);
			if((vQueues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && bPresent) { mlQueueFamily = i; break; }
		}

		uint32_t lDevExtCount = 0;
		vkEnumerateDeviceExtensionProperties(mPhysicalDevice, NULL, &lDevExtCount, NULL);
		std::vector<VkExtensionProperties> vDevExt(lDevExtCount);
		vkEnumerateDeviceExtensionProperties(mPhysicalDevice, NULL, &lDevExtCount, vDevExt.data());
		auto HasExt = [&](const char *apName){ for(auto& e : vDevExt) if(!strcmp(e.extensionName, apName)) return true; return false; };

		std::vector<const char*> vDeviceExt = {
			VK_EXT_SHADER_OBJECT_EXTENSION_NAME, VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
			VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME };
		for(const char *pExt : vDeviceExt)
			if(!HasExt(pExt)) { Error("Vulkan device lacks %s\n", pExt); return false; }
		if(mSurface) vDeviceExt.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		bool bHdrMetadata = mSurface && HasExt(VK_EXT_HDR_METADATA_EXTENSION_NAME);
		if(bHdrMetadata) vDeviceExt.push_back(VK_EXT_HDR_METADATA_EXTENSION_NAME);

		VkPhysicalDeviceExtendedDynamicState3FeaturesEXT eds3{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT};
		eds3.extendedDynamicState3PolygonMode = VK_TRUE;
		eds3.extendedDynamicState3RasterizationSamples = VK_TRUE;
		eds3.extendedDynamicState3SampleMask = VK_TRUE;
		eds3.extendedDynamicState3AlphaToCoverageEnable = VK_TRUE;
		eds3.extendedDynamicState3ColorBlendEnable = VK_TRUE;
		eds3.extendedDynamicState3ColorBlendEquation = VK_TRUE;
		eds3.extendedDynamicState3ColorWriteMask = VK_TRUE;
		eds3.extendedDynamicState3DepthClampEnable = VK_TRUE;
		VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vtxInput{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT};
		vtxInput.pNext = &eds3;
		vtxInput.vertexInputDynamicState = VK_TRUE;
		VkPhysicalDeviceShaderObjectFeaturesEXT shaderObject{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT};
		shaderObject.pNext = &vtxInput;
		shaderObject.shaderObject = VK_TRUE;
		VkPhysicalDeviceVulkan13Features v13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
		v13.pNext = &shaderObject;
		v13.dynamicRendering = VK_TRUE;
		v13.synchronization2 = VK_TRUE;
		v13.shaderDemoteToHelperInvocation = VK_TRUE;
		VkPhysicalDeviceVulkan12Features v12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
		v12.pNext = &v13;
		v12.hostQueryReset = VK_TRUE;
		VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
		features.pNext = &v12;
		features.features.samplerAnisotropy = VK_TRUE;
		features.features.textureCompressionBC = VK_TRUE;
		features.features.depthClamp = VK_TRUE;
		features.features.shaderClipDistance = VK_TRUE;
		features.features.independentBlend = VK_TRUE;
		features.features.fillModeNonSolid = VK_TRUE;
		features.features.occlusionQueryPrecise = VK_TRUE;

		float fPriority = 1.0f;
		VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
		qci.queueFamilyIndex = mlQueueFamily;
		qci.queueCount = 1;
		qci.pQueuePriorities = &fPriority;
		VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
		dci.pNext = &features;
		dci.queueCreateInfoCount = 1;
		dci.pQueueCreateInfos = &qci;
		dci.enabledExtensionCount = (uint32_t)vDeviceExt.size();
		dci.ppEnabledExtensionNames = vDeviceExt.data();
		if(!VkCheck(vkCreateDevice(mPhysicalDevice, &dci, NULL, &mDevice), "vkCreateDevice")) return false;
		vkGetDeviceQueue(mDevice, mlQueueFamily, 0, &mQueue);

		#define LOAD(name) name##_ = (PFN_##name)vkGetDeviceProcAddr(mDevice, #name)
		LOAD(vkCmdSetVertexInputEXT); LOAD(vkCmdBindShadersEXT); LOAD(vkCmdPushDescriptorSetKHR);
		LOAD(vkCmdSetPolygonModeEXT); LOAD(vkCmdSetRasterizationSamplesEXT); LOAD(vkCmdSetSampleMaskEXT);
		LOAD(vkCmdSetAlphaToCoverageEnableEXT); LOAD(vkCmdSetColorBlendEnableEXT); LOAD(vkCmdSetColorBlendEquationEXT);
		LOAD(vkCmdSetColorWriteMaskEXT); LOAD(vkCmdSetDepthClampEnableEXT);
		LOAD(vkCreateShadersEXT); LOAD(vkDestroyShaderEXT);
		if(bHdrMetadata) LOAD(vkSetHdrMetadataEXT);
		#undef LOAD

		VmaAllocatorCreateInfo aci{};
		aci.physicalDevice = mPhysicalDevice;
		aci.device = mDevice;
		aci.instance = mInstance;
		aci.vulkanApiVersion = VK_API_VERSION_1_3;
		if(!VkCheck(vmaCreateAllocator(&aci, &mpAllocator), "vmaCreateAllocator")) return false;

		VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pci.queueFamilyIndex = mlQueueFamily;
		vkCreateCommandPool(mDevice, &pci, NULL, &mCommandPool);
		for(cFrame& f : mvFrames)
		{
			VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
			cai.commandPool = mCommandPool;
			cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			cai.commandBufferCount = 1;
			vkAllocateCommandBuffers(mDevice, &cai, &f.mCmd);
			VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			vkCreateFence(mDevice, &fci, NULL, &f.mFence);
			VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
			vkCreateSemaphore(mDevice, &sci, NULL, &f.mAcquired);
		}
		for(cRingBuffer& r : mvRing)
		{
			r.mlSize = kRingSize;
			void *pMapped;
			CreateBuffer(r.mlSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
						 VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, r.mBuffer, r.mpAlloc, &pMapped);
			r.mpMapped = (unsigned char*)pMapped;
		}

		std::vector<VkDescriptorSetLayoutBinding> vBindings(1 + kSamplerBindings);
		for(int i=0; i<(int)vBindings.size(); ++i)
		{
			vBindings[i].binding = i;
			vBindings[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			vBindings[i].descriptorCount = 1;
			vBindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
		lci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		lci.bindingCount = (uint32_t)vBindings.size();
		lci.pBindings = vBindings.data();
		vkCreateDescriptorSetLayout(mDevice, &lci, NULL, &mSetLayout);
		VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		plci.setLayoutCount = 1;
		plci.pSetLayouts = &mSetLayout;
		vkCreatePipelineLayout(mDevice, &plci, NULL, &mPipelineLayout);

		VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		qpci.queryType = VK_QUERY_TYPE_OCCLUSION;
		qpci.queryCount = 4096;
		vkCreateQueryPool(mDevice, &qpci, NULL, &mQueryPool);
		vkResetQueryPool(mDevice, mQueryPool, 0, qpci.queryCount);
		for(uint32_t i=qpci.queryCount; i>0; --i) mvFreeQueries.push_back(i-1);

		VkFormatProperties fp;
		vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, VK_FORMAT_D24_UNORM_S8_UINT, &fp);
		mDepthStencilFormat = (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_D32_SFLOAT_S8_UINT;
		vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, VK_FORMAT_X8_D24_UNORM_PACK32, &fp);
		VkFormatFeatureFlags lNeed = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
		mDepth24Format = (fp.optimalTilingFeatures & lNeed) == lNeed ? VK_FORMAT_X8_D24_UNORM_PACK32 : VK_FORMAT_D32_SFLOAT;
		return true;
	}

	void cLowLevelGraphicsVulkan::CreateScreenTargets()
	{
		DestroyImage(mScreenColor);
		DestroyImage(mScreenDepth);
		VkFormat colorFormat = mlOutputMode != 0 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
		VkImageUsageFlags lUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		CreateImage(mScreenColor, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, colorFormat, cVector3l(mvScreenSize.x, mvScreenSize.y, 1), 1, 1, lUsage);
		CreateImage(mScreenDepth, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, mDepthStencilFormat, cVector3l(mvScreenSize.x, mvScreenSize.y, 1), 1, 1,
					VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	}

	bool cLowLevelGraphicsVulkan::CreateSwapchain()
	{
		VkSurfaceCapabilitiesKHR caps;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &caps);
		uint32_t lCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &lCount, NULL);
		std::vector<VkSurfaceFormatKHR> vFormats(lCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &lCount, vFormats.data());

		auto Find = [&](VkColorSpaceKHR aSpace, std::initializer_list<VkFormat> avFormats) -> bool {
			for(VkFormat fmt : avFormats)
				for(auto& f : vFormats)
					if(f.colorSpace == aSpace && f.format == fmt) { mSwapchainFormat = fmt; mSwapchainColorSpace = aSpace; return true; }
			return false;
		};
		bool bFound = false;
		if(mlOutputMode == 1) bFound = Find(VK_COLOR_SPACE_HDR10_ST2084_EXT, {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_FORMAT_R16G16B16A16_SFLOAT});
		else if(mlOutputMode == 2) bFound = Find(VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, {VK_FORMAT_R16G16B16A16_SFLOAT});
		else if(mlOutputMode == 3) bFound = Find(VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT, {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM});
		if(mlOutputMode != 0 && !bFound)
		{
			Warning("Vulkan: output mode %d not supported by the display, using SDR\n", mlOutputMode);
			mlOutputMode = 0;
		}
		if(mlOutputMode == 0 && !Find(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_A2B10G10R10_UNORM_PACK32}))
		{
			mSwapchainFormat = vFormats[0].format;
			mSwapchainColorSpace = vFormats[0].colorSpace;
		}

		mSwapchainExtent = caps.currentExtent;
		if(mSwapchainExtent.width == 0xFFFFFFFF) mSwapchainExtent = { (uint32_t)mvScreenSize.x, (uint32_t)mvScreenSize.y };

		uint32_t lModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &lModeCount, NULL);
		std::vector<VkPresentModeKHR> vModes(lModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &lModeCount, vModes.data());
		VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
		if(!mbVsync)
			for(VkPresentModeKHR m : vModes)
				if(m == VK_PRESENT_MODE_MAILBOX_KHR || (m == VK_PRESENT_MODE_IMMEDIATE_KHR && mode != VK_PRESENT_MODE_MAILBOX_KHR)) mode = m;

		VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
		sci.surface = mSurface;
		sci.minImageCount = std::max(caps.minImageCount, 3u);
		if(caps.maxImageCount) sci.minImageCount = std::min(sci.minImageCount, caps.maxImageCount);
		sci.imageFormat = mSwapchainFormat;
		sci.imageColorSpace = mSwapchainColorSpace;
		sci.imageExtent = mSwapchainExtent;
		sci.imageArrayLayers = 1;
		sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		sci.preTransform = caps.currentTransform;
		sci.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
		sci.presentMode = mode;
		sci.clipped = VK_TRUE;
		sci.oldSwapchain = mSwapchain;
		VkSwapchainKHR newSwapchain;
		if(!VkCheck(vkCreateSwapchainKHR(mDevice, &sci, NULL, &newSwapchain), "vkCreateSwapchainKHR")) return false;
		DestroySwapchain();
		mSwapchain = newSwapchain;

		vkGetSwapchainImagesKHR(mDevice, mSwapchain, &lCount, NULL);
		mvSwapchainImages.resize(lCount);
		vkGetSwapchainImagesKHR(mDevice, mSwapchain, &lCount, mvSwapchainImages.data());
		for(VkImage img : mvSwapchainImages)
		{
			VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
			vci.image = img;
			vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			vci.format = mSwapchainFormat;
			vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			VkImageView view;
			vkCreateImageView(mDevice, &vci, NULL, &view);
			mvSwapchainViews.push_back(view);
			VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
			VkSemaphore sem;
			vkCreateSemaphore(mDevice, &semInfo, NULL, &sem);
			mvRenderDone.push_back(sem);
		}

		if(vkSetHdrMetadataEXT_ && (mlOutputMode == 1 || mlOutputMode == 2))
		{
			VkHdrMetadataEXT md{VK_STRUCTURE_TYPE_HDR_METADATA_EXT};
			md.displayPrimaryRed = {0.708f, 0.292f};
			md.displayPrimaryGreen = {0.170f, 0.797f};
			md.displayPrimaryBlue = {0.131f, 0.046f};
			md.whitePoint = {0.3127f, 0.3290f};
			md.maxLuminance = mfPeakNits;
			md.minLuminance = 0.0f;
			md.maxContentLightLevel = mfPeakNits;
			md.maxFrameAverageLightLevel = mfPaperWhite;
			vkSetHdrMetadataEXT_(mDevice, 1, &mSwapchain, &md);
		}
		Log("  Vulkan swapchain: %ux%u format %d colorspace %d (output mode %d)\n", mSwapchainExtent.width, mSwapchainExtent.height,
			(int)mSwapchainFormat, (int)mSwapchainColorSpace, mlOutputMode);
		mbSwapchainDirty = false;
		return true;
	}

	void cLowLevelGraphicsVulkan::DestroySwapchain()
	{
		if(mSwapchain == VK_NULL_HANDLE) return;
		vkDeviceWaitIdle(mDevice);
		for(VkImageView v : mvSwapchainViews) vkDestroyImageView(mDevice, v, NULL);
		for(VkSemaphore s : mvRenderDone) vkDestroySemaphore(mDevice, s, NULL);
		mvSwapchainViews.clear();
		mvRenderDone.clear();
		mvSwapchainImages.clear();
		vkDestroySwapchainKHR(mDevice, mSwapchain, NULL);
		mSwapchain = VK_NULL_HANDLE;
	}

	tString cLowLevelGraphicsVulkan::GetRendererDescription()
	{
		VkPhysicalDeviceProperties p;
		vkGetPhysicalDeviceProperties(mPhysicalDevice, &p);
		static const char *vModes[] = {"SDR", "HDR10", "scRGB", "P3"};
		return tString(p.deviceName) + " | Vulkan " + cString::ToString((int)VK_API_VERSION_MAJOR(p.apiVersion)) + "." +
			   cString::ToString((int)VK_API_VERSION_MINOR(p.apiVersion)) + " " + vModes[mlOutputMode];
	}

	int cLowLevelGraphicsVulkan::GetCaps(eGraphicCaps aType)
	{
		switch(aType)
		{
		case eGraphicCaps_MaxTextureImageUnits: return kMaxTextureUnits;
		case eGraphicCaps_MaxTextureCoordUnits: return 8;
		case eGraphicCaps_MaxUserClipPlanes: return kMaxClipPlanes;
		case eGraphicCaps_MaxAnisotropicFiltering: return (int)mfMaxAnisotropy;
		case eGraphicCaps_Multisampling: return 0;
		case eGraphicCaps_MaxDrawBuffers:
		case eGraphicCaps_MaxColorRenderTargets: return kMaxDrawColorBuffers;
		case eGraphicCaps_ShaderModel_3:
		case eGraphicCaps_ShaderModel_4: return mbForceShaderModel3And4Off ? 0 : 1;
		case eGraphicCaps_OGL_ATIFragmentShader: return 0;
		default: return 1;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// WINDOW
	//////////////////////////////////////////////////////////////////////////

	void cLowLevelGraphicsVulkan::ShowCursor(bool abX){ SDL_ShowCursor(abX ? SDL_ENABLE : SDL_DISABLE); }

	void cLowLevelGraphicsVulkan::SetWindowGrab(bool abX)
	{
		mbGrab = abX;
		if(mbHeadless || mpWindow == NULL) return;
		SDL_SetWindowGrab(mpWindow, abX ? SDL_TRUE : SDL_FALSE);
	}

	void cLowLevelGraphicsVulkan::SetRelativeMouse(bool abX)
	{
		if(mbHeadless) return;
		SDL_SetRelativeMouseMode(abX ? SDL_TRUE : SDL_FALSE);
	}

	void cLowLevelGraphicsVulkan::SetWindowCaption(const tString& asName){ SDL_SetWindowTitle(mpWindow, asName.c_str()); }
	bool cLowLevelGraphicsVulkan::GetWindowMouseFocus(){ return (SDL_GetWindowFlags(mpWindow) & SDL_WINDOW_MOUSE_FOCUS) != 0; }
	bool cLowLevelGraphicsVulkan::GetWindowInputFocus(){ return (SDL_GetWindowFlags(mpWindow) & SDL_WINDOW_INPUT_FOCUS) != 0; }
	bool cLowLevelGraphicsVulkan::GetWindowIsVisible(){ return (SDL_GetWindowFlags(mpWindow) & SDL_WINDOW_SHOWN) != 0; }

	bool cLowLevelGraphicsVulkan::ForceWindowSize(int alWidth, int alHeight)
	{
		if(mpWindow == NULL) return false;
		SDL_SetWindowFullscreen(mpWindow, 0);
		SDL_SetWindowSize(mpWindow, alWidth, alHeight);
		if(mbHeadless) CheckAndUpdateScreenSize();
		return true;
	}

	bool cLowLevelGraphicsVulkan::CheckAndUpdateScreenSize()
	{
		if(mpWindow == NULL) return false;
		int lW = 0, lH = 0;
		SDL_Vulkan_GetDrawableSize(mpWindow, &lW, &lH);
		if(lW <= 0 || lH <= 0 || (lW == mvScreenSize.x && lH == mvScreenSize.y)) return false;
		SubmitAndWait();
		mvScreenSize = cVector2l(lW, lH);
		CreateScreenTargets();
		mbSwapchainDirty = true;
		if(mpFrameBuffer == NULL) SetCurrentFrameBuffer(NULL);
		return true;
	}

	void cLowLevelGraphicsVulkan::SetVsyncActive(bool abX, bool abAdaptive)
	{
		if(mbVsync == abX) return;
		mbVsync = abX;
		mbSwapchainDirty = true;
	}

	//////////////////////////////////////////////////////////////////////////
	// DATA CREATION
	//////////////////////////////////////////////////////////////////////////

	iFontData* cLowLevelGraphicsVulkan::CreateFontData(const tString &asName){ return hplNew(cSDLFontData, (asName, this)); }
	iGpuProgram* cLowLevelGraphicsVulkan::CreateGpuProgram(const tString& asName){ return hplNew(cVulkanProgram, (asName, this)); }
	iGpuShader* cLowLevelGraphicsVulkan::CreateGpuShader(const tString& asName, eGpuShaderType aType){ return hplNew(cVulkanShader, (asName, aType)); }
	iTexture* cLowLevelGraphicsVulkan::CreateTexture(const tString &asName, eTextureType aType, eTextureUsage aUsage){ return hplNew(cVulkanTexture, (asName, aType, aUsage, this)); }
	iVertexBuffer* cLowLevelGraphicsVulkan::CreateVertexBuffer(eVertexBufferType aType, eVertexBufferDrawType aDrawType, eVertexBufferUsageType aUsageType,
															   int alReserveVtxSize, int alReserveIdxSize)
	{
		return hplNew(cVulkanVertexBuffer, (this, aDrawType, aUsageType, alReserveVtxSize, alReserveIdxSize));
	}
	iFrameBuffer* cLowLevelGraphicsVulkan::CreateFrameBuffer(const tString& asName){ return hplNew(cVulkanFrameBuffer, (asName, this)); }
	iDepthStencilBuffer* cLowLevelGraphicsVulkan::CreateDepthStencilBuffer(const cVector2l& avSize, int alDepthBits, int alStencilBits)
	{
		return hplNew(cVulkanDepthStencilBuffer, (this, avSize, alDepthBits, alStencilBits));
	}
	iOcclusionQuery* cLowLevelGraphicsVulkan::CreateOcclusionQuery(){ return hplNew(cVulkanOcclusionQuery, (this)); }

	//////////////////////////////////////////////////////////////////////////
	// RESOURCES
	//////////////////////////////////////////////////////////////////////////

	void cLowLevelGraphicsVulkan::CreateBuffer(VkDeviceSize alSize, VkBufferUsageFlags aUsage, bool abHostVisible, VkBuffer& aBuffer, VmaAllocation_T*& apAlloc, void** apMapped)
	{
		VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		bci.size = std::max<VkDeviceSize>(alSize, 16);
		bci.usage = aUsage;
		VmaAllocationCreateInfo aci{};
		aci.usage = VMA_MEMORY_USAGE_AUTO;
		if(abHostVisible) aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		VmaAllocationInfo info;
		VkCheck(vmaCreateBuffer(mpAllocator, &bci, &aci, &aBuffer, &apAlloc, &info), "vmaCreateBuffer");
		if(apMapped) *apMapped = info.pMappedData;
	}

	void cLowLevelGraphicsVulkan::CreateImage(cVulkanImage& aImg, VkImageType aType, VkImageViewType aViewType, VkFormat aFormat, const cVector3l& avSize,
											  int alLevels, int alLayers, VkImageUsageFlags aUsage, const VkComponentMapping* apSwizzle)
	{
		bool bDepth = aFormat == VK_FORMAT_D16_UNORM || aFormat == VK_FORMAT_D32_SFLOAT || aFormat == VK_FORMAT_X8_D24_UNORM_PACK32 ||
					  aFormat == VK_FORMAT_D24_UNORM_S8_UINT || aFormat == VK_FORMAT_D32_SFLOAT_S8_UINT;
		bool bStencil = aFormat == VK_FORMAT_D24_UNORM_S8_UINT || aFormat == VK_FORMAT_D32_SFLOAT_S8_UINT;
		aImg.mFormat = aFormat;
		aImg.mvSize = avSize;
		aImg.mlLevels = alLevels;
		aImg.mlLayers = alLayers;
		aImg.mAspect = bDepth ? (VK_IMAGE_ASPECT_DEPTH_BIT | (bStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)) : VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		ici.flags = aViewType == VK_IMAGE_VIEW_TYPE_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
		if(aType == VK_IMAGE_TYPE_3D) ici.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
		ici.imageType = aType;
		ici.format = aFormat;
		ici.extent = { (uint32_t)std::max(1, avSize.x), (uint32_t)std::max(1, avSize.y), (uint32_t)std::max(1, avSize.z) };
		ici.mipLevels = alLevels;
		ici.arrayLayers = alLayers;
		ici.samples = VK_SAMPLE_COUNT_1_BIT;
		ici.tiling = VK_IMAGE_TILING_OPTIMAL;
		ici.usage = aUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		VmaAllocationCreateInfo aci{};
		aci.usage = VMA_MEMORY_USAGE_AUTO;
		if(!VkCheck(vmaCreateImage(mpAllocator, &ici, &aci, &aImg.mImage, &aImg.mpAlloc, NULL), "vmaCreateImage")) return;

		VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		vci.image = aImg.mImage;
		vci.viewType = aViewType;
		vci.format = aFormat;
		if(apSwizzle) vci.components = *apSwizzle;
		vci.subresourceRange = { (VkImageAspectFlags)(bDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), 0, (uint32_t)alLevels, 0, (uint32_t)alLayers };
		vkCreateImageView(mDevice, &vci, NULL, &aImg.mView);

		// All images live in GENERAL; render passes end with a full barrier.
		VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
		b.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
		b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.image = aImg.mImage;
		b.subresourceRange = { aImg.mAspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dep.imageMemoryBarrierCount = 1;
		dep.pImageMemoryBarriers = &b;
		EndRendering();
		vkCmdPipelineBarrier2(Cmd(), &dep);

		// Fresh GL textures read as zero; keep render targets from showing garbage
		if(!bDepth && (aUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
		{
			VkClearColorValue zero{};
			VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
			vkCmdClearColorImage(Cmd(), aImg.mImage, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
			VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
			mb.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			mb.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
			mb.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			mb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
			VkDependencyInfo dep2{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
			dep2.memoryBarrierCount = 1;
			dep2.pMemoryBarriers = &mb;
			vkCmdPipelineBarrier2(Cmd(), &dep2);
		}
	}

	void cLowLevelGraphicsVulkan::DestroyImage(cVulkanImage& aImg)
	{
		if(aImg.mImage == VK_NULL_HANDLE) return;
		VkDevice dev = mDevice;
		VmaAllocator alloc = mpAllocator;
		VkImage img = aImg.mImage;
		VmaAllocation mem = aImg.mpAlloc;
		std::vector<VkImageView> vViews = { aImg.mView };
		for(auto& it : aImg.mmapAttachViews) vViews.push_back(it.second);
		for(int i=0; i<mlRenderColorCount; ++i) if(mvRenderColorImage[i] == img) EndRendering();
		DeferDestroy([dev, alloc, img, mem, vViews]{
			for(VkImageView v : vViews) vkDestroyImageView(dev, v, NULL);
			vmaDestroyImage(alloc, img, mem);
		});
		aImg = cVulkanImage();
	}

	VkSampler cLowLevelGraphicsVulkan::GetSampler(const VkSamplerCreateInfo& aInfo)
	{
		uint64_t key = (uint64_t)aInfo.magFilter | (uint64_t)aInfo.minFilter << 2 | (uint64_t)aInfo.mipmapMode << 4 |
					   (uint64_t)aInfo.addressModeU << 6 | (uint64_t)aInfo.addressModeV << 9 | (uint64_t)aInfo.addressModeW << 12 |
					   (uint64_t)aInfo.compareEnable << 15 | (uint64_t)aInfo.compareOp << 16 | (uint64_t)(aInfo.maxAnisotropy * 4) << 20 |
					   (uint64_t)(aInfo.anisotropyEnable) << 28 | (uint64_t)(aInfo.maxLod * 4) << 29;
		auto it = mmapSamplers.find(key);
		if(it != mmapSamplers.end()) return it->second;
		VkSampler s;
		vkCreateSampler(mDevice, &aInfo, NULL, &s);
		mmapSamplers[key] = s;
		return s;
	}

	void cLowLevelGraphicsVulkan::DeferDestroy(std::function<void()> aFunc)
	{
		mvFrames[mlFrame].mvDestroy.push_back(aFunc);
	}

	void cLowLevelGraphicsVulkan::TextureDestroyed(iTexture* apTex)
	{
		for(int i=0; i<kMaxTextureUnits; ++i) if(mvTextures[i] == apTex) mvTextures[i] = NULL;
	}

	void* cLowLevelGraphicsVulkan::AllocTransient(size_t alSize, VkBuffer& aBuffer, VkDeviceSize& aOffset, size_t alAlign)
	{
		cRingBuffer& r = mvRing[mlFrame];
		size_t lOffset = (r.mlUsed + alAlign - 1) & ~(alAlign - 1);
		if(lOffset + alSize > r.mlSize)
		{
			VkBuffer oldBuffer = r.mBuffer;
			VmaAllocation oldAlloc = r.mpAlloc;
			VmaAllocator alloc = mpAllocator;
			DeferDestroy([alloc, oldBuffer, oldAlloc]{ vmaDestroyBuffer(alloc, oldBuffer, oldAlloc); });
			r.mlSize = std::max(r.mlSize * 2, alSize * 2);
			void *pMapped;
			CreateBuffer(r.mlSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
						 VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, r.mBuffer, r.mpAlloc, &pMapped);
			r.mpMapped = (unsigned char*)pMapped;
			lOffset = 0;
		}
		r.mlUsed = lOffset + alSize;
		aBuffer = r.mBuffer;
		aOffset = lOffset;
		return r.mpMapped + lOffset;
	}

	static void FullBarrier(VkCommandBuffer aCmd)
	{
		VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
		mb.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		mb.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		mb.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		mb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dep.memoryBarrierCount = 1;
		dep.pMemoryBarriers = &mb;
		vkCmdPipelineBarrier2(aCmd, &dep);
	}

	void cLowLevelGraphicsVulkan::UploadToImage(cVulkanImage& aImg, int alLevel, int alLayer, const cVector3l& avOffset, const cVector3l& avSize,
												const void *apData, size_t alSize)
	{
		// Large uploads (level loads) get a private staging buffer so the ring doesn't balloon
		VkBuffer buffer;
		VkDeviceSize lOffset = 0;
		if(alSize > (4u << 20))
		{
			VmaAllocation alloc;
			void *pMapped;
			CreateBuffer(alSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, buffer, alloc, &pMapped);
			memcpy(pMapped, apData, alSize);
			VmaAllocator allocator = mpAllocator;
			DeferDestroy([allocator, buffer, alloc]{ vmaDestroyBuffer(allocator, buffer, alloc); });
		}
		else
			memcpy(AllocTransient(alSize, buffer, lOffset, 16), apData, alSize);

		EndRendering();
		VkBufferImageCopy region{};
		region.bufferOffset = lOffset;
		region.imageSubresource = { (VkImageAspectFlags)(aImg.mAspect & VK_IMAGE_ASPECT_STENCIL_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT : aImg.mAspect), (uint32_t)alLevel, (uint32_t)alLayer, 1 };
		region.imageOffset = { avOffset.x, avOffset.y, avOffset.z };
		region.imageExtent = { (uint32_t)std::max(1, avSize.x), (uint32_t)std::max(1, avSize.y), (uint32_t)std::max(1, avSize.z) };
		FullBarrier(Cmd());
		vkCmdCopyBufferToImage(Cmd(), buffer, aImg.mImage, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
		FullBarrier(Cmd());
	}

	void cLowLevelGraphicsVulkan::GenerateMips(cVulkanImage& aImg)
	{
		if(aImg.mlLevels <= 1) return;
		EndRendering();
		VkCommandBuffer cmd = Cmd();
		FullBarrier(cmd);
		for(int i=1; i<aImg.mlLevels; ++i)
		{
			VkImageBlit blit{};
			blit.srcSubresource = { aImg.mAspect, (uint32_t)(i-1), 0, (uint32_t)aImg.mlLayers };
			blit.srcOffsets[1] = { std::max(1, aImg.mvSize.x >> (i-1)), std::max(1, aImg.mvSize.y >> (i-1)), std::max(1, aImg.mvSize.z >> (i-1)) };
			blit.dstSubresource = { aImg.mAspect, (uint32_t)i, 0, (uint32_t)aImg.mlLayers };
			blit.dstOffsets[1] = { std::max(1, aImg.mvSize.x >> i), std::max(1, aImg.mvSize.y >> i), std::max(1, aImg.mvSize.z >> i) };
			vkCmdBlitImage(cmd, aImg.mImage, VK_IMAGE_LAYOUT_GENERAL, aImg.mImage, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_LINEAR);
			FullBarrier(cmd);
		}
	}

	void cLowLevelGraphicsVulkan::ReadImage(cVulkanImage& aImg, int alLevel, int alLayer, const cVector2l& avPos, const cVector2l& avSize,
											std::vector<unsigned char>& avOut, size_t alPixelSize)
	{
		size_t lSize = (size_t)avSize.x * avSize.y * alPixelSize;
		VkBuffer buffer;
		VmaAllocation alloc;
		void *pMapped;
		CreateBuffer(lSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, buffer, alloc, &pMapped);
		VmaAllocationCreateInfo dummy{};
		(void)dummy;
		EndRendering();
		FullBarrier(Cmd());
		VkBufferImageCopy region{};
		region.imageSubresource = { (VkImageAspectFlags)(aImg.mAspect & VK_IMAGE_ASPECT_STENCIL_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT : aImg.mAspect), (uint32_t)alLevel, (uint32_t)alLayer, 1 };
		region.imageOffset = { avPos.x, avPos.y, 0 };
		region.imageExtent = { (uint32_t)avSize.x, (uint32_t)avSize.y, 1 };
		vkCmdCopyImageToBuffer(Cmd(), aImg.mImage, VK_IMAGE_LAYOUT_GENERAL, buffer, 1, &region);
		SubmitAndWait();
		vmaInvalidateAllocation(mpAllocator, alloc, 0, VK_WHOLE_SIZE);
		avOut.assign((unsigned char*)pMapped, (unsigned char*)pMapped + lSize);
		vmaDestroyBuffer(mpAllocator, buffer, alloc);
	}

	bool cLowLevelGraphicsVulkan::CreateShaders(const std::vector<unsigned int>& avVert, const std::vector<unsigned int>& avFrag, VkShaderEXT *apOut)
	{
		VkShaderCreateInfoEXT info[2] = {};
		for(int i=0; i<2; ++i)
		{
			const std::vector<unsigned int>& code = i==0 ? avVert : avFrag;
			info[i].sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
			info[i].flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
			info[i].stage = i==0 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
			info[i].nextStage = i==0 ? VK_SHADER_STAGE_FRAGMENT_BIT : 0;
			info[i].codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
			info[i].codeSize = code.size() * sizeof(unsigned int);
			info[i].pCode = code.data();
			info[i].pName = "main";
			info[i].setLayoutCount = 1;
			info[i].pSetLayouts = &mSetLayout;
		}
		return VkCheck(vkCreateShadersEXT_(mDevice, 2, info, NULL, apOut), "vkCreateShadersEXT");
	}

	//////////////////////////////////////////////////////////////////////////
	// FRAME
	//////////////////////////////////////////////////////////////////////////

	VkCommandBuffer cLowLevelGraphicsVulkan::Cmd(){ return mvFrames[mlFrame].mCmd; }

	void cLowLevelGraphicsVulkan::WaitFrame(cFrame& aFrame)
	{
		vkWaitForFences(mDevice, 1, &aFrame.mFence, VK_TRUE, UINT64_MAX);
		mlCompletedSerial = std::max(mlCompletedSerial, aFrame.mlSerial);
		for(auto& d : aFrame.mvDestroy) d();
		aFrame.mvDestroy.clear();
	}

	void cLowLevelGraphicsVulkan::BeginCommandBuffer()
	{
		cFrame& f = mvFrames[mlFrame];
		WaitFrame(f);
		for(auto& d : mvDestroyNow) f.mvDestroy.push_back(d);
		mvDestroyNow.clear();
		vkResetFences(mDevice, 1, &f.mFence);
		mvRing[mlFrame].mlUsed = 0;
		f.mlSerial = ++mlSubmitSerial;
		vkResetCommandBuffer(f.mCmd, 0);
		VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(f.mCmd, &bi);
		mbStateDirty = true;
	}

	void cLowLevelGraphicsVulkan::Submit(bool abPresent)
	{
		cFrame& f = mvFrames[mlFrame];
		EndRendering();

		uint32_t lImage = 0;
		bool bPresent = abPresent && mSwapchain != VK_NULL_HANDLE;
		if(bPresent)
		{
			VkResult res = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX, f.mAcquired, VK_NULL_HANDLE, &lImage);
			if(res == VK_ERROR_OUT_OF_DATE_KHR) { bPresent = false; mbSwapchainDirty = true; }
			else if(res == VK_SUBOPTIMAL_KHR) mbSwapchainDirty = true;
			else if(res != VK_SUCCESS) bPresent = false;
		}
		if(bPresent)
		{
			mlPresentImage = lImage;
			Present();
		}

		vkEndCommandBuffer(f.mCmd);
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		si.commandBufferCount = 1;
		si.pCommandBuffers = &f.mCmd;
		if(bPresent)
		{
			si.waitSemaphoreCount = 1;
			si.pWaitSemaphores = &f.mAcquired;
			si.pWaitDstStageMask = &waitStage;
			si.signalSemaphoreCount = 1;
			si.pSignalSemaphores = &mvRenderDone[lImage];
		}
		VkCheck(vkQueueSubmit(mQueue, 1, &si, f.mFence), "vkQueueSubmit");
		if(bPresent)
		{
			VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
			pi.waitSemaphoreCount = 1;
			pi.pWaitSemaphores = &mvRenderDone[lImage];
			pi.swapchainCount = 1;
			pi.pSwapchains = &mSwapchain;
			pi.pImageIndices = &lImage;
			VkResult res = vkQueuePresentKHR(mQueue, &pi);
			if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) mbSwapchainDirty = true;
		}
	}

	void cLowLevelGraphicsVulkan::SubmitAndWait()
	{
		Submit(false);
		WaitFrame(mvFrames[mlFrame]);
		BeginCommandBuffer();
	}

	void cLowLevelGraphicsVulkan::WaitAndFinishRendering(){ SubmitAndWait(); }

	void cLowLevelGraphicsVulkan::SwapBuffers()
	{
		Submit(true);
		mlFrame ^= 1;
		if(mbSwapchainDirty && mSwapchain != VK_NULL_HANDLE)
		{
			vkDeviceWaitIdle(mDevice);
			CreateSwapchain();
		}
		BeginCommandBuffer();
	}

	//////////////////////////////////////////////////////////////////////////
	// RENDERING
	//////////////////////////////////////////////////////////////////////////

	void cLowLevelGraphicsVulkan::BeginRendering()
	{
		if(mbRendering) return;
		VkRenderingAttachmentInfo vColor[kMaxDrawColorBuffers] = {};
		VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
		VkRenderingAttachmentInfo stencil{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
		cVector2l vSize;
		mlRenderColorCount = 0;
		mRenderDepth = VK_NULL_HANDLE;
		mbRenderStencil = false;

		if(mpFrameBuffer == NULL)
		{
			vSize = mvScreenSize;
			mvRenderColor[0] = mScreenColor.mView;
			mvRenderColorImage[0] = mScreenColor.mImage;
			mlRenderColorCount = 1;
			mRenderDepth = mScreenDepth.mView;
			mbRenderStencil = true;
		}
		else
		{
			cVulkanFrameBuffer *pFB = static_cast<cVulkanFrameBuffer*>(mpFrameBuffer);
			vSize = pFB->GetSize();
			int vAttached[kMaxDrawColorBuffers], lAttached = 0;
			for(int i=0; i<kMaxDrawColorBuffers; ++i) if(pFB->GetColorBuffer(i)) vAttached[lAttached++] = i;
			int lCount = mlDrawTargetCount >= 0 ? mlDrawTargetCount : lAttached;
			for(int i=0; i<lCount; ++i)
			{
				int lIdx = mlDrawTargetCount >= 0 ? mvDrawTargets[i] : vAttached[i];
				cVulkanTexture *pTex = static_cast<cVulkanTexture*>(pFB->GetColorBuffer(lIdx));
				if(pTex == NULL || pTex->GetImage() == NULL) continue;
				mvRenderColor[mlRenderColorCount] = pTex->GetAttachView(pFB->mvLevel[lIdx], pFB->mvLayer[lIdx]);
				mvRenderColorImage[mlRenderColorCount] = pTex->GetImage()->mImage;
				++mlRenderColorCount;
			}
			iFrameBufferAttachment *pDepth = pFB->GetDepthBuffer();
			if(pDepth)
			{
				if(pDepth->GetFrameBufferAttachmentType() == eFrameBufferAttachment_RenderBuffer)
				{
					cVulkanDepthStencilBuffer *pBuf = static_cast<cVulkanDepthStencilBuffer*>(pDepth);
					mRenderDepth = pBuf->mImage.mView;
					mbRenderStencil = pBuf->mImage.mAspect & VK_IMAGE_ASPECT_STENCIL_BIT;
				}
				else
				{
					cVulkanTexture *pTex = static_cast<cVulkanTexture*>(pDepth);
					mRenderDepth = pTex->GetAttachView(pFB->mvLevel[kMaxDrawColorBuffers], pFB->mvLayer[kMaxDrawColorBuffers]);
				}
			}
			else if(iFrameBufferAttachment *pStencil = pFB->GetStencilBuffer())
			{
				mRenderDepth = static_cast<cVulkanDepthStencilBuffer*>(pStencil)->mImage.mView;
				mbRenderStencil = true;
			}
		}

		for(int i=0; i<mlRenderColorCount; ++i)
		{
			vColor[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			vColor[i].imageView = mvRenderColor[i];
			vColor[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			vColor[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			vColor[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		}
		depth.imageView = mRenderDepth;
		depth.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		stencil = depth;

		VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
		ri.renderArea = { {0, 0}, { (uint32_t)vSize.x, (uint32_t)vSize.y } };
		ri.layerCount = 1;
		ri.colorAttachmentCount = mlRenderColorCount;
		ri.pColorAttachments = vColor;
		ri.pDepthAttachment = mRenderDepth ? &depth : NULL;
		ri.pStencilAttachment = mRenderDepth && mbRenderStencil ? &stencil : NULL;
		vkCmdBeginRendering(Cmd(), &ri);
		mbRendering = true;
		mbStateDirty = true;
	}

	void cLowLevelGraphicsVulkan::BeginQuery(uint32_t alIndex)
	{
		BeginRendering();
		// Renderer compares sample counts against a threshold; imprecise queries may return 1
		vkCmdBeginQuery(Cmd(), mQueryPool, alIndex, VK_QUERY_CONTROL_PRECISE_BIT);
		mlActiveQuery = alIndex;
	}

	void cLowLevelGraphicsVulkan::EndQuery(uint32_t alIndex)
	{
		if(mlActiveQuery != alIndex) return;
		vkCmdEndQuery(Cmd(), mQueryPool, alIndex);
		mlActiveQuery = UINT32_MAX;
	}

	void cLowLevelGraphicsVulkan::EndRendering()
	{
		if(!mbRendering) return;
		// Queries can't span render passes; the result covers what was drawn so far
		if(mlActiveQuery != UINT32_MAX) EndQuery(mlActiveQuery);
		vkCmdEndRendering(Cmd());
		FullBarrier(Cmd());
		mbRendering = false;
	}

	void cLowLevelGraphicsVulkan::ApplyState(int alColorTargets)
	{
		VkCommandBuffer cmd = Cmd();
		cVector2l vTotal = mpFrameBuffer ? mpFrameBuffer->GetSize() : mvScreenSize;

		VkViewport vp;
		vp.x = (float)mvFrameBufferPos.x;
		vp.y = (float)((vTotal.y - mvFrameBufferSize.y) - mvFrameBufferPos.y);
		vp.width = (float)mvFrameBufferSize.x;
		vp.height = (float)mvFrameBufferSize.y;
		vp.minDepth = 0;
		vp.maxDepth = 1;
		vkCmdSetViewportWithCount(cmd, 1, &vp);

		VkRect2D sc = { {0, 0}, { (uint32_t)vTotal.x, (uint32_t)vTotal.y } };
		if(mState.mbScissor)
		{
			int x0 = std::max(0, mState.mvScissorPos.x), y0 = std::max(0, mState.mvScissorPos.y);
			int x1 = std::max(x0, mState.mvScissorPos.x + mState.mvScissorSize.x), y1 = std::max(y0, mState.mvScissorPos.y + mState.mvScissorSize.y);
			sc = { {x0, y0}, { (uint32_t)(x1 - x0), (uint32_t)(y1 - y0) } };
		}
		vkCmdSetScissorWithCount(cmd, 1, &sc);

		vkCmdSetRasterizerDiscardEnable(cmd, VK_FALSE);
		vkCmdSetPolygonModeEXT_(cmd, VK_POLYGON_MODE_FILL);
		vkCmdSetRasterizationSamplesEXT_(cmd, VK_SAMPLE_COUNT_1_BIT);
		VkSampleMask lMask = 0xFFFFFFFF;
		vkCmdSetSampleMaskEXT_(cmd, VK_SAMPLE_COUNT_1_BIT, &lMask);
		vkCmdSetAlphaToCoverageEnableEXT_(cmd, VK_FALSE);
		vkCmdSetCullMode(cmd, mState.mbCull ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
		// Same row order as GL but Vulkan's winding formula is negated, so the GL front face flips
		vkCmdSetFrontFace(cmd, mState.mCullMode == eCullMode_Clockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE);
		bool bDepth = mRenderDepth != VK_NULL_HANDLE;
		vkCmdSetDepthTestEnable(cmd, bDepth && mState.mbDepthTest);
		vkCmdSetDepthWriteEnable(cmd, bDepth && mState.mbDepthTest && mState.mbDepthWrite);
		vkCmdSetDepthCompareOp(cmd, ToVkCompare(mState.mDepthFunc));
		vkCmdSetDepthBoundsTestEnable(cmd, VK_FALSE);
		vkCmdSetDepthBiasEnable(cmd, mState.mbDepthBias);
		vkCmdSetDepthBias(cmd, mState.mfDepthBias, 0.0f, mState.mfDepthSlope);
		vkCmdSetDepthClampEnableEXT_(cmd, mState.mbDepthClamp);
		bool bStencil = mbRenderStencil && mState.mbStencil;
		vkCmdSetStencilTestEnable(cmd, bStencil);
		const VkStencilOpState& f = mState.mStencilFront;
		const VkStencilOpState& b = mState.mStencilBack;
		vkCmdSetStencilOp(cmd, VK_STENCIL_FACE_FRONT_BIT, f.failOp, f.passOp, f.depthFailOp, f.compareOp);
		vkCmdSetStencilOp(cmd, VK_STENCIL_FACE_BACK_BIT, b.failOp, b.passOp, b.depthFailOp, b.compareOp);
		vkCmdSetStencilCompareMask(cmd, VK_STENCIL_FACE_FRONT_BIT, f.compareMask);
		vkCmdSetStencilCompareMask(cmd, VK_STENCIL_FACE_BACK_BIT, b.compareMask);
		vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_BIT, f.reference);
		vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_BACK_BIT, b.reference);
		vkCmdSetStencilWriteMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, mState.mlStencilWriteMask);
		vkCmdSetPrimitiveRestartEnable(cmd, VK_FALSE);
		vkCmdSetLineWidth(cmd, 1.0f);
		if(alColorTargets > 0)
		{
			VkBool32 vEnable[kMaxDrawColorBuffers];
			VkColorBlendEquationEXT vEq[kMaxDrawColorBuffers];
			VkColorComponentFlags vMask[kMaxDrawColorBuffers];
			for(int i=0; i<alColorTargets; ++i) { vEnable[i] = mState.mbBlend; vEq[i] = mState.mBlend; vMask[i] = mState.mColorMask; }
			vkCmdSetColorBlendEnableEXT_(cmd, 0, alColorTargets, vEnable);
			vkCmdSetColorBlendEquationEXT_(cmd, 0, alColorTargets, vEq);
			vkCmdSetColorWriteMaskEXT_(cmd, 0, alColorTargets, vMask);
		}
		mbStateDirty = false;
	}

	cVulkanImage* cLowLevelGraphicsVulkan::GetDummyImage(const std::string& asSamplerType)
	{
		auto it = mmapDummyImages.find(asSamplerType);
		if(it != mmapDummyImages.end()) return &it->second;
		cVulkanImage& img = mmapDummyImages[asSamplerType];
		bool bShadow = asSamplerType.find("Shadow") != std::string::npos;
		VkFormat fmt = bShadow ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		if(asSamplerType == "samplerCube") CreateImage(img, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_CUBE, fmt, cVector3l(1,1,1), 1, 6, usage);
		else if(asSamplerType == "sampler3D") CreateImage(img, VK_IMAGE_TYPE_3D, VK_IMAGE_VIEW_TYPE_3D, fmt, cVector3l(1,1,1), 1, 1, usage);
		else if(asSamplerType == "sampler1D") CreateImage(img, VK_IMAGE_TYPE_1D, VK_IMAGE_VIEW_TYPE_1D, fmt, cVector3l(1,1,1), 1, 1, usage);
		else if(asSamplerType == "sampler2DArray") CreateImage(img, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY, fmt, cVector3l(1,1,1), 1, 1, usage);
		else CreateImage(img, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, fmt, cVector3l(1,1,1), 1, 1, usage);
		// GL samples an unbound unit as black, alpha 1
		if(!bShadow)
		{
			VkClearColorValue black = {{0, 0, 0, 1}};
			VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS };
			vkCmdClearColorImage(Cmd(), img.mImage, VK_IMAGE_LAYOUT_GENERAL, &black, 1, &range);
			FullBarrier(Cmd());
		}
		return &img;
	}

	static bool SamplerMatches(const std::string& asType, iTexture *apTex, cVulkanTexture *apVkTex)
	{
		eTextureType type = apTex->GetType();
		bool bDepthCompare = apVkTex->IsDepth() && apTex->GetCompareMode() != eTextureCompareMode_None;
		if(asType == "sampler2D") return (type == eTextureType_2D || type == eTextureType_Rect) && !bDepthCompare;
		if(asType == "sampler2DShadow") return (type == eTextureType_2D || type == eTextureType_Rect) && apVkTex->IsDepth();
		if(asType == "samplerCube") return type == eTextureType_CubeMap;
		if(asType == "sampler3D") return type == eTextureType_3D;
		if(asType == "sampler1D") return type == eTextureType_1D;
		return false;
	}

	void cLowLevelGraphicsVulkan::Draw(const cAttribStream *apStreams, ePrim aPrim, uint32_t alCount, VkBuffer aIndexBuffer, VkDeviceSize alIndexOffset)
	{
		if(alCount == 0) return;
		cVulkanProgram *pProg = mpProgram ? mpProgram : GetFixedFunctionProgram();
		if(pProg == NULL || pProg->mvShaders[0] == VK_NULL_HANDLE) return;

		// Uniforms
		unsigned char *pUni = pProg->mvUniformData.data();
		float vAlpha[4] = { mbAlphaTest ? 1.0f : 0.0f, (float)mAlphaFunc, mfAlphaRef, 0 };
		if(pProg->mvBuiltinOffset[0] >= 0) memcpy(pUni + pProg->mvBuiltinOffset[0], vAlpha, 16);
		const cMatrixf& mtxMV = mvMatrix[eMatrix_ModelView];
		const cMatrixf& mtxP = mvMatrix[eMatrix_Projection];
		if(pProg->mvBuiltinOffset[1] >= 0) WriteMatrix(pUni + pProg->mvBuiltinOffset[1], cMath::MatrixMul(mtxP, mtxMV), 4);
		if(pProg->mvBuiltinOffset[2] >= 0) WriteMatrix(pUni + pProg->mvBuiltinOffset[2], mtxMV, 4);
		if(pProg->mvBuiltinOffset[3] >= 0) WriteMatrix(pUni + pProg->mvBuiltinOffset[3], mtxP, 4);
		if(pProg->mvBuiltinOffset[4] >= 0) WriteMatrix(pUni + pProg->mvBuiltinOffset[4], cMath::MatrixInverse(mtxMV).GetTranspose(), 3);
		if(pProg->mvBuiltinOffset[5] >= 0)
		{
			float *pPlanes = (float*)(pUni + pProg->mvBuiltinOffset[5]);
			for(int i=0; i<kMaxClipPlanes; ++i)
			{
				const cPlanef& p = mvClipPlanesEye[i];
				bool bOn = mvClipPlaneActive[i];
				pPlanes[i*4+0] = bOn ? p.a : 0; pPlanes[i*4+1] = bOn ? p.b : 0; pPlanes[i*4+2] = bOn ? p.c : 0; pPlanes[i*4+3] = bOn ? p.d : 1;
			}
		}
		VkBuffer uboBuffer;
		VkDeviceSize uboOffset;
		size_t lUniSize = std::max<size_t>(pProg->mvUniformData.size(), 16);
		memcpy(AllocTransient(lUniSize, uboBuffer, uboOffset, 64), pUni, pProg->mvUniformData.size());

		VkDescriptorBufferInfo uboInfo = { uboBuffer, uboOffset, lUniSize };
		VkDescriptorImageInfo vImages[kSamplerBindings];
		VkWriteDescriptorSet vWrites[1 + kSamplerBindings];
		uint32_t lWrites = 0;
		vWrites[lWrites] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		vWrites[lWrites].dstBinding = 0;
		vWrites[lWrites].descriptorCount = 1;
		vWrites[lWrites].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vWrites[lWrites].pBufferInfo = &uboInfo;
		++lWrites;
		for(size_t i=0; i<pProg->mvSamplers.size() && i<(size_t)kSamplerBindings; ++i)
		{
			const cGlslVkSampler& smp = pProg->mvSamplers[i];
			int lUnit = pProg->mvSamplerUnit[i];
			iTexture *pTex = lUnit >= 0 && lUnit < kMaxTextureUnits ? mvTextures[lUnit] : NULL;
			cVulkanTexture *pVkTex = static_cast<cVulkanTexture*>(pTex);
			cVulkanImage *pImg = pVkTex ? pVkTex->GetImage() : NULL;
			VkDescriptorImageInfo& info = vImages[i];
			if(pImg && pImg->mView && SamplerMatches(smp.msType, pTex, pVkTex))
			{
				info.imageView = pImg->mView;
				info.sampler = pVkTex->GetSampler(smp.msType == "sampler2DShadow");
			}
			else
			{
				info.imageView = GetDummyImage(smp.msType)->mView;
				VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
				sci.compareEnable = smp.msType.find("Shadow") != std::string::npos;
				sci.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
				info.sampler = GetSampler(sci);
			}
			info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			vWrites[lWrites] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			vWrites[lWrites].dstBinding = smp.mlBinding;
			vWrites[lWrites].descriptorCount = 1;
			vWrites[lWrites].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			vWrites[lWrites].pImageInfo = &info;
			++lWrites;
		}
		BeginRendering();
		VkCommandBuffer cmd = Cmd();
		if(mbStateDirty) ApplyState(mlRenderColorCount);

		VkPrimitiveTopology topo = aPrim == ePrim_Lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : aPrim == ePrim_LineStrip ? VK_PRIMITIVE_TOPOLOGY_LINE_STRIP :
								   aPrim == ePrim_TriStrip ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : aPrim == ePrim_TriFan ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN :
								   aPrim == ePrim_Points ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		vkCmdSetPrimitiveTopology(cmd, topo);

		// Vertex input: attributes the shader reads but the caller doesn't supply get GL's current value
		VkVertexInputBindingDescription2EXT vBindings[eGlslVkAttrib_LastEnum];
		VkVertexInputAttributeDescription2EXT vAttribs[eGlslVkAttrib_LastEnum];
		VkBuffer vBuffers[eGlslVkAttrib_LastEnum];
		VkDeviceSize vOffsets[eGlslVkAttrib_LastEnum];
		uint32_t lAttribs = 0;
		for(int loc=0; loc<eGlslVkAttrib_LastEnum; ++loc)
		{
			if(!(pProg->mlAttribMask & (1u << loc))) continue;
			VkVertexInputBindingDescription2EXT& bd = vBindings[lAttribs];
			VkVertexInputAttributeDescription2EXT& ad = vAttribs[lAttribs];
			bd = {VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT};
			ad = {VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT};
			bd.binding = lAttribs;
			bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			bd.divisor = 1;
			ad.location = loc;
			ad.binding = lAttribs;
			if(apStreams[loc].mBuffer)
			{
				bd.stride = apStreams[loc].mStride;
				ad.format = apStreams[loc].mFormat;
				vBuffers[lAttribs] = apStreams[loc].mBuffer;
				vOffsets[lAttribs] = apStreams[loc].mOffset;
			}
			else
			{
				float *pVal = (float*)AllocTransient(16, vBuffers[lAttribs], vOffsets[lAttribs], 16);
				if(loc == eGlslVkAttrib_Color) { pVal[0] = mCurrentColor.r; pVal[1] = mCurrentColor.g; pVal[2] = mCurrentColor.b; pVal[3] = mCurrentColor.a; }
				else if(loc == eGlslVkAttrib_Normal) { pVal[0] = 0; pVal[1] = 0; pVal[2] = 1; pVal[3] = 1; }
				else { pVal[0] = 0; pVal[1] = 0; pVal[2] = 0; pVal[3] = 1; }
				bd.stride = 0;
				ad.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			}
			++lAttribs;
		}
		vkCmdSetVertexInputEXT_(cmd, lAttribs, vBindings, lAttribs, vAttribs);
		if(lAttribs) vkCmdBindVertexBuffers(cmd, 0, lAttribs, vBuffers, vOffsets);

		vkCmdPushDescriptorSetKHR_(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, lWrites, vWrites);

		VkShaderStageFlagBits vStages[2] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
		vkCmdBindShadersEXT_(cmd, 2, vStages, pProg->mvShaders);

		if(aIndexBuffer)
		{
			vkCmdBindIndexBuffer(cmd, aIndexBuffer, alIndexOffset, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmd, alCount, 1, 0, 0, 0);
		}
		else
			vkCmdDraw(cmd, alCount, 1, 0, 0);
	}

	//////////////////////////////////////////////////////////////////////////
	// FIXED FUNCTION EMULATION
	//////////////////////////////////////////////////////////////////////////

	cVulkanProgram* cLowLevelGraphicsVulkan::GetFixedFunctionProgram()
	{
		// GL modulates vertex colour with every enabled unit; units 0-3 cover what the engine draws without shaders
		unsigned int lKey = 0;
		for(int i=0; i<4; ++i)
		{
			iTexture *pTex = mvTextures[i];
			unsigned int lType = 0;
			if(pTex && static_cast<cVulkanTexture*>(pTex)->GetImage())
			{
				switch(pTex->GetType())
				{
				case eTextureType_2D: lType = 1; break;
				case eTextureType_Rect: lType = 2; break;
				case eTextureType_CubeMap: lType = 3; break;
				case eTextureType_3D: lType = 4; break;
				case eTextureType_1D: lType = 5; break;
				default: break;
				}
			}
			lKey |= lType << (i*3);
		}
		auto it = mmapFixedFunction.find(lKey);
		if(it != mmapFixedFunction.end()) return it->second;

		tString sVtx = "#version 130\nuniform vec4 hpl_ClipPlane[6];\nout float gl_ClipDistance[6];\n"
					   "void main(){\n\tvec4 vEye = gl_ModelViewMatrix * gl_Vertex;\n\tgl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
					   "\tgl_FrontColor = gl_Color;\n\tfor(int i=0; i<6; ++i) gl_ClipDistance[i] = dot(vEye, hpl_ClipPlane[i]);\n";
		tString sFrag = "#version 120\n";
		tString sFragMain = "void main(){\n\tvec4 c = gl_Color;\n";
		static const char *vSampler[] = {"", "sampler2D", "sampler2DRect", "samplerCube", "sampler3D", "sampler1D"};
		static const char *vLookup[] = {"", "texture2D(t%d, gl_TexCoord[%d].xy)", "texture2DRect(t%d, gl_TexCoord[%d].xy)",
										"textureCube(t%d, gl_TexCoord[%d].xyz)", "texture3D(t%d, gl_TexCoord[%d].xyz)", "texture1D(t%d, gl_TexCoord[%d].x)"};
		for(int i=0; i<4; ++i)
		{
			unsigned int lType = (lKey >> (i*3)) & 7;
			if(lType == 0) continue;
			char sBuf[128];
			snprintf(sBuf, sizeof(sBuf), vLookup[lType], i, i);
			sFrag += tString("uniform ") + vSampler[lType] + " t" + cString::ToString(i) + ";\n";
			sFragMain += tString("\tc *= ") + sBuf + ";\n";
			sVtx += "\tgl_TexCoord[" + cString::ToString(i) + "] = gl_MultiTexCoord" + cString::ToString(i) + ";\n";
		}
		sVtx += "}\n";
		sFrag += sFragMain + "\tgl_FragColor = c;\n}\n";

		cVulkanProgram *pProg = hplNew(cVulkanProgram, ("fixed_function", this));
		if(!pProg->LinkFromSource(sVtx, sFrag)) { hplDelete(pProg); pProg = NULL; }
		else
			for(int i=0; i<4; ++i) pProg->SetSamplerToUnit("t" + cString::ToString(i), i);
		mmapFixedFunction[lKey] = pProg;
		return pProg;
	}

	//////////////////////////////////////////////////////////////////////////
	// PRESENT (HDR / WCG encode)
	//////////////////////////////////////////////////////////////////////////

	void cLowLevelGraphicsVulkan::Present()
	{
		if(mpPresentProgram == NULL)
		{
			const char *pVtx = "#version 120\nvoid main(){ gl_TexCoord[0] = gl_MultiTexCoord0; gl_Position = gl_Vertex; }\n";
			// x = 1/gamma, y = mode, z = paper white nits, w = peak nits
			const char *pFrag =
				"#version 120\nuniform sampler2D screen;\nuniform vec4 params;\n"
				"vec3 Pq(vec3 n){ vec3 p = pow(clamp(n / 10000.0, 0.0, 1.0), vec3(0.1593017578125));\n"
				"  return pow((0.8359375 + 18.8515625 * p) / (1.0 + 18.6875 * p), vec3(78.84375)); }\n"
				"vec3 SrgbEncode(vec3 c){ c = clamp(c, 0.0, 1.0); return mix(c * 12.92, 1.055 * pow(c, vec3(1.0/2.4)) - 0.055, step(0.0031308, c)); }\n"
				"void main(){\n"
				"  vec3 c = pow(max(texture2D(screen, gl_TexCoord[0].xy).rgb, 0.0), vec3(params.x));\n"
				"  int mode = int(params.y + 0.5);\n"
				"  if(mode == 0) { gl_FragColor = vec4(c, 1.0); return; }\n"
				"  vec3 lin = pow(c, vec3(2.2));\n" // SDR content is display-referred gamma 2.2
				"  if(mode == 1) {\n"
				"    mat3 toBt2020 = mat3(0.6274, 0.0691, 0.0164, 0.3293, 0.9195, 0.0880, 0.0433, 0.0114, 0.8956);\n"
				"    gl_FragColor = vec4(Pq(min(toBt2020 * lin * params.z, vec3(params.w))), 1.0);\n"
				"  } else if(mode == 2) {\n"
				"    gl_FragColor = vec4(min(lin * params.z, vec3(params.w)) / 80.0, 1.0);\n"
				"  } else {\n"
				"    mat3 toP3 = mat3(0.8225, 0.0332, 0.0171, 0.1774, 0.9669, 0.0724, 0.0, 0.0, 0.9108);\n"
				"    gl_FragColor = vec4(SrgbEncode(toP3 * lin), 1.0);\n"
				"  }\n}\n";
			mpPresentProgram = hplNew(cVulkanProgram, ("present", this));
			if(!mpPresentProgram->LinkFromSource(pVtx, pFrag)) return;
			mpPresentProgram->SetSamplerToUnit("screen", 0);
		}
		cVulkanProgram *pProg = mpPresentProgram;
		VkCommandBuffer cmd = Cmd();
		VkImage swapImage = mvSwapchainImages[mlPresentImage];

		VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
		b.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		b.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		b.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		b.image = swapImage;
		b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dep.imageMemoryBarrierCount = 1;
		dep.pImageMemoryBarriers = &b;
		vkCmdPipelineBarrier2(cmd, &dep);

		VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
		color.imageView = mvSwapchainViews[mlPresentImage];
		color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
		ri.renderArea = { {0, 0}, mSwapchainExtent };
		ri.layerCount = 1;
		ri.colorAttachmentCount = 1;
		ri.pColorAttachments = &color;
		vkCmdBeginRendering(cmd, &ri);

		// Draw() state, but on the swapchain and with everything off
		cRenderState savedState = mState;
		mState = cRenderState();
		mState.mbCull = false;
		mState.mbDepthTest = false;
		mState.mBlend = { VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD };
		VkViewport vp = { 0, 0, (float)mSwapchainExtent.width, (float)mSwapchainExtent.height, 0, 1 };
		VkRect2D sc = { {0, 0}, mSwapchainExtent };
		VkImageView savedDepth = mRenderDepth;
		mRenderDepth = VK_NULL_HANDLE;
		bool bSavedStencil = mbRenderStencil;
		mbRenderStencil = false;
		ApplyState(1);
		vkCmdSetViewportWithCount(cmd, 1, &vp);
		vkCmdSetScissorWithCount(cmd, 1, &sc);
		mState = savedState;
		mRenderDepth = savedDepth;
		mbRenderStencil = bSavedStencil;
		mbStateDirty = true;
		vkCmdSetPrimitiveTopology(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

		// Our rows are bottom-up; the swapchain is top-down
		struct { float x, y, z, w, u, v, s, t; } vQuad[4] = {
			{-1,-1,0,1, 0,1,0,1}, {1,-1,0,1, 1,1,0,1}, {-1,1,0,1, 0,0,0,1}, {1,1,0,1, 1,0,0,1} };
		VkBuffer vb;
		VkDeviceSize vbOffset;
		memcpy(AllocTransient(sizeof(vQuad), vb, vbOffset), vQuad, sizeof(vQuad));
		VkVertexInputBindingDescription2EXT vBind[2] = {};
		VkVertexInputAttributeDescription2EXT vAttr[2] = {};
		for(int i=0; i<2; ++i)
		{
			vBind[i].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
			vBind[i].binding = i;
			vBind[i].stride = 32;
			vBind[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			vBind[i].divisor = 1;
			vAttr[i].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
			vAttr[i].binding = i;
			vAttr[i].location = i == 0 ? eGlslVkAttrib_Vertex : eGlslVkAttrib_MultiTexCoord0;
			vAttr[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		vkCmdSetVertexInputEXT_(cmd, 2, vBind, 2, vAttr);
		VkBuffer vBuffers[2] = { vb, vb };
		VkDeviceSize vOffsets[2] = { vbOffset, vbOffset + 16 };
		vkCmdBindVertexBuffers(cmd, 0, 2, vBuffers, vOffsets);

		int lParams = pProg->GetVariableId("params");
		pProg->SetVec4f(lParams, 1.0f / std::max(0.01f, mfGammaCorrection), (float)mlOutputMode, mfPaperWhite, mfPeakNits);
		VkBuffer ubo;
		VkDeviceSize uboOffset;
		memcpy(AllocTransient(pProg->mvUniformData.size(), ubo, uboOffset, 64), pProg->mvUniformData.data(), pProg->mvUniformData.size());
		VkDescriptorBufferInfo uboInfo = { ubo, uboOffset, pProg->mvUniformData.size() };
		VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
		sci.magFilter = sci.minFilter = mSwapchainExtent.width == (uint32_t)mvScreenSize.x ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
		sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		VkDescriptorImageInfo imgInfo = { GetSampler(sci), mScreenColor.mView, VK_IMAGE_LAYOUT_GENERAL };
		VkWriteDescriptorSet vWrites[2] = {};
		vWrites[0].sType = vWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vWrites[0].dstBinding = 0;
		vWrites[0].descriptorCount = 1;
		vWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vWrites[0].pBufferInfo = &uboInfo;
		vWrites[1].dstBinding = pProg->mvSamplers[0].mlBinding;
		vWrites[1].descriptorCount = 1;
		vWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		vWrites[1].pImageInfo = &imgInfo;
		vkCmdPushDescriptorSetKHR_(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 2, vWrites);
		VkShaderStageFlagBits vStages[2] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
		vkCmdBindShadersEXT_(cmd, 2, vStages, pProg->mvShaders);
		vkCmdDraw(cmd, 4, 1, 0, 0);
		vkCmdEndRendering(cmd);

		b.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		b.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		b.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
		b.dstAccessMask = 0;
		b.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		vkCmdPipelineBarrier2(cmd, &dep);
	}

	//////////////////////////////////////////////////////////////////////////
	// FRAME BUFFER OPERATIONS
	//////////////////////////////////////////////////////////////////////////

	void cLowLevelGraphicsVulkan::ClearFrameBuffer(tClearFrameBufferFlag aFlags)
	{
		BeginRendering();
		cVector2l vTotal = mpFrameBuffer ? mpFrameBuffer->GetSize() : mvScreenSize;
		VkClearRect rect = { { {0, 0}, { (uint32_t)vTotal.x, (uint32_t)vTotal.y } }, 0, 1 };
		if(mState.mbScissor)
		{
			int x0 = std::max(0, mState.mvScissorPos.x), y0 = std::max(0, mState.mvScissorPos.y);
			int x1 = std::min(vTotal.x, mState.mvScissorPos.x + mState.mvScissorSize.x), y1 = std::min(vTotal.y, mState.mvScissorPos.y + mState.mvScissorSize.y);
			if(x1 <= x0 || y1 <= y0) return;
			rect.rect = { {x0, y0}, { (uint32_t)(x1 - x0), (uint32_t)(y1 - y0) } };
		}
		VkClearAttachment vClear[kMaxDrawColorBuffers + 1];
		uint32_t lCount = 0;
		if((aFlags & eClearFrameBufferFlag_Color) && mState.mColorMask)
		{
			for(int i=0; i<mlRenderColorCount; ++i)
			{
				vClear[lCount].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				vClear[lCount].colorAttachment = i;
				vClear[lCount].clearValue.color = {{ mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a }};
				++lCount;
			}
		}
		VkImageAspectFlags lDS = 0;
		if((aFlags & eClearFrameBufferFlag_Depth) && mRenderDepth && mState.mbDepthWrite) lDS |= VK_IMAGE_ASPECT_DEPTH_BIT;
		if((aFlags & eClearFrameBufferFlag_Stencil) && mbRenderStencil) lDS |= VK_IMAGE_ASPECT_STENCIL_BIT;
		if(lDS)
		{
			vClear[lCount].aspectMask = lDS;
			vClear[lCount].colorAttachment = 0;
			vClear[lCount].clearValue.depthStencil = { mfClearDepth, (uint32_t)mlClearStencil };
			++lCount;
		}
		if(lCount) vkCmdClearAttachments(Cmd(), lCount, vClear, 1, &rect);
	}

	static cVulkanImage* GetColorImage(iFrameBuffer *apFB, cVulkanImage *apScreen, int& alLevel, int& alLayer)
	{
		alLevel = alLayer = 0;
		if(apFB == NULL) return apScreen;
		cVulkanFrameBuffer *pFB = static_cast<cVulkanFrameBuffer*>(apFB);
		iFrameBufferAttachment *pAtt = pFB->GetColorBuffer(0);
		if(pAtt == NULL || pAtt->GetFrameBufferAttachmentType() != eFrameBufferAttachment_Texture) return NULL;
		alLevel = pFB->mvLevel[0];
		alLayer = pFB->mvLayer[0];
		return static_cast<cVulkanTexture*>(pAtt)->GetImage();
	}

	void cLowLevelGraphicsVulkan::CopyFrameBufferToTexure(iTexture* apTex, const cVector2l &avPos, const cVector2l &avSize, const cVector2l &avTexOffset)
	{
		if(apTex == NULL) return;
		cVulkanImage *pDst = static_cast<cVulkanTexture*>(apTex)->GetImage();
		int lLevel, lLayer;
		cVulkanImage *pSrc = GetColorImage(mpFrameBuffer, &mScreenColor, lLevel, lLayer);
		if(pSrc == NULL || pDst == NULL) return;

		cVector2l vSize = avSize;
		if(vSize.x <= 0) vSize.x = mvFrameBufferSize.x;
		if(vSize.y <= 0) vSize.y = mvFrameBufferSize.y;
		cVector2l vPos = mvFrameBufferPos + avPos;
		vPos.y = (mvFrameBufferTotalSize.y - vSize.y) - vPos.y;
		cVector2l vTexPos = avTexOffset;
		vTexPos.y = (apTex->GetHeight() - vSize.y) - vTexPos.y;

		EndRendering();
		VkImageBlit blit{};
		blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)lLevel, (uint32_t)lLayer, 1 };
		blit.srcOffsets[0] = { vPos.x, vPos.y, 0 };
		blit.srcOffsets[1] = { vPos.x + vSize.x, vPos.y + vSize.y, 1 };
		blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		blit.dstOffsets[0] = { vTexPos.x, vTexPos.y, 0 };
		blit.dstOffsets[1] = { vTexPos.x + vSize.x, vTexPos.y + vSize.y, 1 };
		vkCmdBlitImage(Cmd(), pSrc->mImage, VK_IMAGE_LAYOUT_GENERAL, pDst->mImage, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_NEAREST);
		FullBarrier(Cmd());
	}

	static void UnpackPixel(VkFormat aFormat, const unsigned char *p, float *apOut)
	{
		auto Half = [](uint16_t h) -> float {
			uint32_t s = (h >> 15) & 1, e = (h >> 10) & 0x1F, m = h & 0x3FF;
			float f = e == 0 ? std::ldexp((float)m, -24) : e == 31 ? (m ? NAN : INFINITY) : std::ldexp((float)(m | 0x400), (int)e - 25);
			return s ? -f : f;
		};
		switch(aFormat)
		{
		case VK_FORMAT_B8G8R8A8_UNORM: case VK_FORMAT_B8G8R8A8_SRGB:
			apOut[0] = p[2]/255.0f; apOut[1] = p[1]/255.0f; apOut[2] = p[0]/255.0f; apOut[3] = p[3]/255.0f; break;
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			for(int i=0; i<4; ++i) apOut[i] = Half(((const uint16_t*)p)[i]); break;
		case VK_FORMAT_R16G16_SFLOAT:
			apOut[0] = Half(((const uint16_t*)p)[0]); apOut[1] = Half(((const uint16_t*)p)[1]); apOut[2] = 0; apOut[3] = 1; break;
		case VK_FORMAT_R16_SFLOAT:
			apOut[0] = apOut[1] = apOut[2] = Half(*(const uint16_t*)p); apOut[3] = 1; break;
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			memcpy(apOut, p, 16); break;
		case VK_FORMAT_R32_SFLOAT: case VK_FORMAT_D32_SFLOAT: case VK_FORMAT_D32_SFLOAT_S8_UINT:
			apOut[0] = apOut[1] = apOut[2] = *(const float*)p; apOut[3] = 1; break;
		case VK_FORMAT_D24_UNORM_S8_UINT: case VK_FORMAT_X8_D24_UNORM_PACK32:
			apOut[0] = apOut[1] = apOut[2] = (*(const uint32_t*)p & 0xFFFFFF) / 16777215.0f; apOut[3] = 1; break;
		case VK_FORMAT_D16_UNORM:
			apOut[0] = apOut[1] = apOut[2] = *(const uint16_t*)p / 65535.0f; apOut[3] = 1; break;
		case VK_FORMAT_R8_UNORM:
			apOut[0] = apOut[1] = apOut[2] = p[0]/255.0f; apOut[3] = 1; break;
		case VK_FORMAT_R8G8_UNORM:
			apOut[0] = p[0]/255.0f; apOut[1] = p[1]/255.0f; apOut[2] = 0; apOut[3] = 1; break;
		default:
			for(int i=0; i<4; ++i) apOut[i] = p[i]/255.0f; break;
		}
	}

	size_t VulkanFormatSize(VkFormat aFormat)
	{
		switch(aFormat)
		{
		case VK_FORMAT_R8_UNORM: return 1;
		case VK_FORMAT_R8G8_UNORM: case VK_FORMAT_R16_SFLOAT: case VK_FORMAT_D16_UNORM: return 2;
		case VK_FORMAT_R16G16B16A16_SFLOAT: return 8;
		case VK_FORMAT_R32G32B32A32_SFLOAT: return 16;
		case VK_FORMAT_R16G16_SFLOAT: return 4;
		default: return 4;
		}
	}

	bool VulkanReadPixelsFloat(cLowLevelGraphicsVulkan *apGfx, cVulkanImage& aImg, int alLevel, int alLayer, const cVector2l& avPos, const cVector2l& avSize, std::vector<float>& avOut)
	{
		std::vector<unsigned char> vRaw;
		size_t lPixel = VulkanFormatSize(aImg.mFormat);
		apGfx->ReadImage(aImg, alLevel, alLayer, avPos, avSize, vRaw, lPixel);
		size_t lCount = (size_t)avSize.x * avSize.y;
		avOut.resize(lCount * 4);
		for(size_t i=0; i<lCount; ++i) UnpackPixel(aImg.mFormat, &vRaw[i*lPixel], &avOut[i*4]);
		return true;
	}

	cBitmap* cLowLevelGraphicsVulkan::CopyFrameBufferToBitmap(const cVector2l &avScreenPos, const cVector2l &avScreenSize)
	{
		cVector2l vSize = avScreenSize;
		if(vSize.x <= 0) vSize.x = mvFrameBufferSize.x;
		if(vSize.y <= 0) vSize.y = mvFrameBufferSize.y;
		cVector2l vPos = mvFrameBufferPos + avScreenPos;
		vPos.y = (mvFrameBufferTotalSize.y - vSize.y) - vPos.y;

		ePixelFormat pixelFormat = ePixelFormat_RGBA;
		if(mpFrameBuffer)
		{
			iFrameBufferAttachment *pColor = mpFrameBuffer->GetColorBuffer(0);
			if(pColor && pColor->GetFrameBufferAttachmentType() == eFrameBufferAttachment_Texture)
				pixelFormat = static_cast<iTexture*>(pColor)->GetPixelFormat();
		}
		cBitmap *pBitmap = hplNew(cBitmap, ());
		pBitmap->CreateData(cVector3l(vSize.x, vSize.y, 1), pixelFormat, 0, 0);

		int lLevel, lLayer;
		cVulkanImage *pSrc = GetColorImage(mpFrameBuffer, &mScreenColor, lLevel, lLayer);
		if(pSrc == NULL) return pBitmap;
		vPos.x = cMath::Clamp(vPos.x, 0, pSrc->mvSize.x - 1);
		vPos.y = cMath::Clamp(vPos.y, 0, pSrc->mvSize.y - 1);
		vSize.x = std::min(vSize.x, pSrc->mvSize.x - vPos.x);
		vSize.y = std::min(vSize.y, pSrc->mvSize.y - vPos.y);
		std::vector<float> vPixels;
		VulkanReadPixelsFloat(this, *pSrc, lLevel, lLayer, vPos, vSize, vPixels);

		unsigned char *pDst = pBitmap->GetData(0,0)->mpData;
		int lBpp = pBitmap->GetBytesPerPixel();
		int lDstW = pBitmap->GetWidth();
		for(int y=0; y<vSize.y; ++y)
			for(int x=0; x<vSize.x; ++x)
			{
				const float *c = &vPixels[((size_t)y*vSize.x + x)*4];
				unsigned char *d = pDst + ((size_t)y*lDstW + x)*lBpp;
				auto B = [](float f){ return (unsigned char)(cMath::Clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f); };
				switch(pixelFormat)
				{
				case ePixelFormat_BGRA: d[0] = B(c[2]); d[1] = B(c[1]); d[2] = B(c[0]); d[3] = B(c[3]); break;
				case ePixelFormat_RGB: d[0] = B(c[0]); d[1] = B(c[1]); d[2] = B(c[2]); break;
				case ePixelFormat_BGR: d[0] = B(c[2]); d[1] = B(c[1]); d[2] = B(c[0]); break;
				default: for(int i=0; i<lBpp && i<4; ++i) d[i] = B(c[i]); break;
				}
			}
		return pBitmap;
	}

	void cLowLevelGraphicsVulkan::SetCurrentFrameBuffer(iFrameBuffer* apFrameBuffer, const cVector2l &avPos, const cVector2l& avSize)
	{
		iFrameBuffer *pPrev = mpFrameBuffer;
		EndRendering();
		mpFrameBuffer = apFrameBuffer;
		mlDrawTargetCount = -1;
		if(pPrev) pPrev->PostBindUpdate();

		mvFrameBufferTotalSize = mpFrameBuffer ? mpFrameBuffer->GetSize() : mvScreenSize;
		mvFrameBufferSize = avSize.x < 0 || avSize.y < 0 ? mvFrameBufferTotalSize : avSize;
		mvFrameBufferPos = avPos;
		mbStateDirty = true;
	}

	void cLowLevelGraphicsVulkan::SetFrameBufferDrawTargets(int *apTargets, int alNumOfTargets)
	{
		EndRendering();
		mlDrawTargetCount = std::min(alNumOfTargets, kMaxDrawColorBuffers);
		for(int i=0; i<mlDrawTargetCount; ++i) mvDrawTargets[i] = apTargets[i];
	}

	//////////////////////////////////////////////////////////////////////////
	// RENDER STATE
	//////////////////////////////////////////////////////////////////////////

	void cLowLevelGraphicsVulkan::SetColorWriteActive(bool abR, bool abG, bool abB, bool abA)
	{
		mState.mColorMask = (abR ? VK_COLOR_COMPONENT_R_BIT : 0) | (abG ? VK_COLOR_COMPONENT_G_BIT : 0) |
							(abB ? VK_COLOR_COMPONENT_B_BIT : 0) | (abA ? VK_COLOR_COMPONENT_A_BIT : 0);
		mbStateDirty = true;
	}

	void cLowLevelGraphicsVulkan::SetStencil(eStencilFunc aFunc, int alRef, unsigned int aMask, eStencilOp aFailOp, eStencilOp aZFailOp, eStencilOp aZPassOp)
	{
		VkStencilOpState s = { ToVkStencilOp(aFailOp), ToVkStencilOp(aZPassOp), ToVkStencilOp(aZFailOp), ToVkCompare(aFunc), aMask, 0xFF, (uint32_t)alRef };
		mState.mStencilFront = mState.mStencilBack = s;
		mbStateDirty = true;
	}

	void cLowLevelGraphicsVulkan::SetStencilTwoSide(eStencilFunc aFrontFunc, eStencilFunc aBackFunc, int alRef, unsigned int aMask,
													eStencilOp aFrontFailOp, eStencilOp aFrontZFailOp, eStencilOp aFrontZPassOp,
													eStencilOp aBackFailOp, eStencilOp aBackZFailOp, eStencilOp aBackZPassOp)
	{
		mState.mStencilFront = { ToVkStencilOp(aFrontFailOp), ToVkStencilOp(aFrontZPassOp), ToVkStencilOp(aFrontZFailOp), ToVkCompare(aFrontFunc), aMask, 0xFF, (uint32_t)alRef };
		mState.mStencilBack = { ToVkStencilOp(aBackFailOp), ToVkStencilOp(aBackZPassOp), ToVkStencilOp(aBackZFailOp), ToVkCompare(aBackFunc), aMask, 0xFF, (uint32_t)alRef };
		mbStateDirty = true;
	}

	void cLowLevelGraphicsVulkan::SetScissorRect(const cVector2l& avPos, const cVector2l& avSize)
	{
		cVector2l vTotal = mpFrameBuffer ? mpFrameBuffer->GetSize() : mvScreenSize;
		mState.mvScissorPos = cVector2l(avPos.x, (vTotal.y - avPos.y) - avSize.y);
		mState.mvScissorSize = avSize;
		mbStateDirty = true;
	}

	void cLowLevelGraphicsVulkan::SetClipPlane(int alIdx, const cPlanef& aPlane)
	{
		// GL stores user clip planes in eye space using the modelview current at the call
		mvClipPlanes[alIdx] = aPlane;
		cMatrixf mtxInv = cMath::MatrixInverse(mvMatrix[eMatrix_ModelView]);
		const float p[4] = { aPlane.a, aPlane.b, aPlane.c, aPlane.d };
		float e[4];
		for(int c=0; c<4; ++c) e[c] = p[0]*mtxInv.m[0][c] + p[1]*mtxInv.m[1][c] + p[2]*mtxInv.m[2][c] + p[3]*mtxInv.m[3][c];
		mvClipPlanesEye[alIdx] = cPlanef(e[0], e[1], e[2], e[3]);
	}

	void cLowLevelGraphicsVulkan::SetBlendFuncSeparate(eBlendFunc aSrcFactorColor, eBlendFunc aDestFactorColor, eBlendFunc aSrcFactorAlpha, eBlendFunc aDestFactorAlpha)
	{
		mState.mBlend = { ToVkBlend(aSrcFactorColor), ToVkBlend(aDestFactorColor), VK_BLEND_OP_ADD, ToVkBlend(aSrcFactorAlpha), ToVkBlend(aDestFactorAlpha), VK_BLEND_OP_ADD };
		mbStateDirty = true;
	}

	//////////////////////////////////////////////////////////////////////////
	// MATRIX
	//////////////////////////////////////////////////////////////////////////

	void cLowLevelGraphicsVulkan::PushMatrix(eMatrix aMtxType){ mvMatrixStack[aMtxType].push_back(mvMatrix[aMtxType]); }

	void cLowLevelGraphicsVulkan::PopMatrix(eMatrix aMtxType)
	{
		if(mvMatrixStack[aMtxType].empty()) return;
		mvMatrix[aMtxType] = mvMatrixStack[aMtxType].back();
		mvMatrixStack[aMtxType].pop_back();
	}

	static cMatrixf GlOrtho(float l, float r, float b, float t, float n, float f)
	{
		return cMatrixf(2/(r-l), 0, 0, -(r+l)/(r-l),
						0, 2/(t-b), 0, -(t+b)/(t-b),
						0, 0, -2/(f-n), -(f+n)/(f-n),
						0, 0, 0, 1);
	}

	void cLowLevelGraphicsVulkan::SetOrthoProjection(const cVector2f& avSize, float afMin, float afMax)
	{
		mvMatrix[eMatrix_Projection] = GlOrtho(0, avSize.x, avSize.y, 0, afMin, afMax);
	}

	void cLowLevelGraphicsVulkan::SetOrthoProjection(const cVector3f& avMin, const cVector3f& avMax)
	{
		mvMatrix[eMatrix_Projection] = GlOrtho(avMin.x, avMax.x, avMax.y, avMin.y, avMin.z, avMax.z);
	}

	//////////////////////////////////////////////////////////////////////////
	// IMMEDIATE MODE
	//////////////////////////////////////////////////////////////////////////

	enum { eIm_Quads, eIm_Lines, eIm_LineStrip, eIm_Triangles };

	void cLowLevelGraphicsVulkan::ImBegin(int alMode)
	{
		mlImMode = alMode;
		mvImVertices.clear();
	}

	void cLowLevelGraphicsVulkan::ImTexCoord(int alUnit, float s, float t, float r)
	{
		if(alUnit < 0 || alUnit >= 4) return;
		mImCurrent.mvTex[alUnit][0] = s; mImCurrent.mvTex[alUnit][1] = t; mImCurrent.mvTex[alUnit][2] = r; mImCurrent.mvTex[alUnit][3] = 1;
	}

	void cLowLevelGraphicsVulkan::ImVertex(float x, float y, float z)
	{
		mImCurrent.mvPos[0] = x; mImCurrent.mvPos[1] = y; mImCurrent.mvPos[2] = z;
		mImCurrent.mvColor[0] = mCurrentColor.r; mImCurrent.mvColor[1] = mCurrentColor.g;
		mImCurrent.mvColor[2] = mCurrentColor.b; mImCurrent.mvColor[3] = mCurrentColor.a;
		mvImVertices.push_back(mImCurrent);
	}

	void cLowLevelGraphicsVulkan::ImEnd()
	{
		if(mvImVertices.empty()) return;
		uint32_t lCount = (uint32_t)mvImVertices.size();
		VkBuffer vb;
		VkDeviceSize vbOffset;
		memcpy(AllocTransient(lCount * sizeof(cImVertex), vb, vbOffset), mvImVertices.data(), lCount * sizeof(cImVertex));

		cAttribStream vStreams[eGlslVkAttrib_LastEnum] = {};
		vStreams[eGlslVkAttrib_Vertex] = { vb, vbOffset + offsetof(cImVertex, mvPos), sizeof(cImVertex), VK_FORMAT_R32G32B32_SFLOAT };
		vStreams[eGlslVkAttrib_Color] = { vb, vbOffset + offsetof(cImVertex, mvColor), sizeof(cImVertex), VK_FORMAT_R32G32B32A32_SFLOAT };
		for(int i=0; i<4; ++i)
			vStreams[eGlslVkAttrib_MultiTexCoord0 + i] = { vb, vbOffset + offsetof(cImVertex, mvTex) + i*16, sizeof(cImVertex), VK_FORMAT_R32G32B32A32_SFLOAT };

		if(mlImMode == eIm_Quads)
		{
			uint32_t lQuads = lCount / 4;
			VkBuffer ib;
			VkDeviceSize ibOffset;
			uint32_t *pIdx = (uint32_t*)AllocTransient(lQuads * 6 * 4, ib, ibOffset, 4);
			for(uint32_t q=0; q<lQuads; ++q)
			{
				uint32_t b = q*4;
				pIdx[q*6+0] = b; pIdx[q*6+1] = b+1; pIdx[q*6+2] = b+2;
				pIdx[q*6+3] = b; pIdx[q*6+4] = b+2; pIdx[q*6+5] = b+3;
			}
			Draw(vStreams, ePrim_Triangles, lQuads * 6, ib, ibOffset);
		}
		else
			Draw(vStreams, mlImMode == eIm_Lines ? ePrim_Lines : mlImMode == eIm_LineStrip ? ePrim_LineStrip : ePrim_Triangles, lCount, VK_NULL_HANDLE, 0);
		mvImVertices.clear();
	}

	#define IM_CLASS cLowLevelGraphicsVulkan
	#define IM_BEGIN ImBegin
	#define IM_END ImEnd
	#define IM_VERTEX3 ImVertex
	#define IM_COLOR4(r,g,b,a) mCurrentColor = cColor(r,g,b,a)
	#define IM_TEXCOORD2(s,t) ImTexCoord(0, s, t)
	#define IM_TEXCOORD3(s,t,r) ImTexCoord(0, s, t, r)
	#define IM_MULTITEXCOORD2(u,s,t) ImTexCoord(u, s, t)
	#define IM_MULTITEXCOORD3(u,s,t,r) ImTexCoord(u, s, t, r)
	#define IM_QUADS eIm_Quads
	#define IM_LINES eIm_Lines
	#define IM_LINE_STRIP eIm_LineStrip
	#define IM_TRIANGLES eIm_Triangles
	#define IM_TEXTURE0 0
	#define IM_TEXTURE1 1
	#include "LowLevelGraphicsImmediate.inl"

	//////////////////////////////////////////////////////////////////////////
	// BATCHES
	//////////////////////////////////////////////////////////////////////////

	void cLowLevelGraphicsVulkan::FlushBatch(tVtxBatchFlag aTypeFlags, bool abQuads, bool abAutoClear)
	{
		uint32_t lVertices = mlVertexCount / mlBatchStride;
		if(lVertices > 0 && mlIndexCount > 0)
		{
			VkBuffer vb;
			VkDeviceSize vbOffset;
			memcpy(AllocTransient(mlVertexCount * sizeof(float), vb, vbOffset), mpVertexArray, mlVertexCount * sizeof(float));
			uint32_t lStride = mlBatchStride * sizeof(float);

			cAttribStream vStreams[eGlslVkAttrib_LastEnum] = {};
			if(aTypeFlags & eVtxBatchFlag_Position) vStreams[eGlslVkAttrib_Vertex] = { vb, vbOffset, lStride, VK_FORMAT_R32G32B32_SFLOAT };
			if(aTypeFlags & eVtxBatchFlag_Color0) vStreams[eGlslVkAttrib_Color] = { vb, vbOffset + 3*4, lStride, VK_FORMAT_R32G32B32A32_SFLOAT };
			if(aTypeFlags & eVtxBatchFlag_Normal) vStreams[eGlslVkAttrib_Normal] = { vb, vbOffset + 10*4, lStride, VK_FORMAT_R32G32B32_SFLOAT };
			static const tVtxBatchFlag vTexFlags[3] = { eVtxBatchFlag_Texture0, eVtxBatchFlag_Texture1, eVtxBatchFlag_Texture2 };
			for(int i=0; i<3; ++i)
			{
				if(!(aTypeFlags & vTexFlags[i])) continue;
				if(mbBatchUnitOwnCoords[i] && mlTexCoordArrayCount[i] > 0)
				{
					VkBuffer tb;
					VkDeviceSize tbOffset;
					memcpy(AllocTransient(mlTexCoordArrayCount[i] * sizeof(float), tb, tbOffset), mpTexCoordArray[i], mlTexCoordArrayCount[i] * sizeof(float));
					vStreams[eGlslVkAttrib_MultiTexCoord0 + i] = { tb, tbOffset, 12, VK_FORMAT_R32G32B32_SFLOAT };
				}
				else
					vStreams[eGlslVkAttrib_MultiTexCoord0 + i] = { vb, vbOffset + 7*4, lStride, VK_FORMAT_R32G32B32_SFLOAT };
			}

			VkBuffer ib;
			VkDeviceSize ibOffset;
			uint32_t lIndices = mlIndexCount;
			if(abQuads)
			{
				uint32_t lQuads = mlIndexCount / 4;
				lIndices = lQuads * 6;
				uint32_t *pIdx = (uint32_t*)AllocTransient(lIndices * 4, ib, ibOffset, 4);
				for(uint32_t q=0; q<lQuads; ++q)
				{
					const unsigned int *s = &mpIndexArray[q*4];
					pIdx[q*6+0] = s[0]; pIdx[q*6+1] = s[1]; pIdx[q*6+2] = s[2];
					pIdx[q*6+3] = s[0]; pIdx[q*6+4] = s[2]; pIdx[q*6+5] = s[3];
				}
			}
			else
				memcpy(AllocTransient(lIndices * 4, ib, ibOffset, 4), mpIndexArray, lIndices * 4);
			Draw(vStreams, ePrim_Triangles, lIndices, ib, ibOffset);
		}
		if(abAutoClear)
		{
			mlIndexCount = 0;
			mlVertexCount = 0;
			for(int i=0; i<kMaxTextureUnits; ++i) mlTexCoordArrayCount[i] = 0;
		}
	}

	void cLowLevelGraphicsVulkan::FlushTriBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear){ FlushBatch(aTypeFlags, false, abAutoClear); }
	void cLowLevelGraphicsVulkan::FlushQuadBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear){ FlushBatch(aTypeFlags, true, abAutoClear); }

	void cLowLevelGraphicsVulkan::ClearBatch()
	{
		mlIndexCount = 0;
		mlVertexCount = 0;
	}
}
