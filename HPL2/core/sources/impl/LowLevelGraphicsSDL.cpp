/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef WIN32
#pragma comment(lib, "OpenGL32.lib")
#pragma comment(lib, "GLu32.lib")
//#pragma comment(lib, "GLaux.lib")
#pragma comment(lib, "Cg.lib")
#pragma comment(lib, "CgGL.lib")
//#pragma comment(lib, "SDL_ttf.lib")
#pragma comment(lib, "TaskKeyHook.lib")
#endif

#include <assert.h>
#include <stdlib.h>

#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/Mutex.h"
#include "system/String.h"
#include "math/Math.h"

#include "impl/LowLevelGraphicsSDL.h"
#include "impl/SDLFontData.h"
#include "impl/SDLTexture.h"
#include "impl/GlOps.h"
//#include "impl/CGShader.h"
//#include "impl/CGProgram.h"
#include "impl/GLSLShader.h"
#include "impl/GLSLProgram.h"
#include "impl/VertexBufferOGL_Array.h"
#include "impl/VertexBufferOGL_VBO.h"
#include "impl/FrameBufferGL.h"
#include "impl/OcclusionQueryOGL.h"
#include "impl/RustGlCtx.h"

#include "graphics/Bitmap.h"

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#endif

#if USE_SDL2
#include "SDL2/SDL_syswm.h"
#else
#include "SDL/SDL_syswm.h"
#endif

#ifdef WIN32
#include "impl/TaskKeyHook.h"
#endif

#ifndef WIN32
	#if defined __ppc__ || defined(__LP64__)
		#define CALLBACK
	#else
		#define CALLBACK __attribute__ ((__stdcall__))
	#endif
#endif

namespace hpl {

	#ifdef HPL_OGL_THREADSAFE
		iMutex *gpLowlevelGfxMutex=NULL;

		cLowlevelGfxMutex::cLowlevelGfxMutex(){ if(gpLowlevelGfxMutex) gpLowlevelGfxMutex->Lock(); }
		cLowlevelGfxMutex::~cLowlevelGfxMutex(){ if(gpLowlevelGfxMutex) gpLowlevelGfxMutex->Unlock(); }
	#endif

	static void OhplGlCtxHostLog(int alKind, const char *apMsg)
	{
		switch(alKind)
		{
		case kOhplGlLogInfo:	Log("%s", apMsg); break;
		case kOhplGlLogError:	Error("%s", apMsg); break;
		case kOhplGlLogFatal:	FatalError("%s", apMsg); break;
		case kOhplGlLogWarnBox:	cPlatform::CreateMessageBox(_W("Warning!"),_W("Could not set displaymode and 640x480 is used instead!\n")); break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cLowLevelGraphicsSDL::cLowLevelGraphicsSDL()
	{
		ohpl_glctx_set_host_log(&OhplGlCtxHostLog);
		mpRs = ohpl_glctx_new();

		mlBatchArraySize = 20000;
		mlVertexCount = 0;
		mlIndexCount =0;
		mlMultisampling =0;
        mpScreen = 0;
        mbGrab = false;

		mbDoubleSidedStencilIsSet = false;

#if defined(WIN32) && !SDL_VERSION_ATLEAST(2,0,0)
		mhKeyTrapper = NULL;
#endif

		mpFrameBuffer = NULL;

		//Create the batch arrays:
		mlBatchStride = 13;
		//3 Pos floats, 4 color floats, 3 Tex coord floats .
		mpVertexArray = (float*)hplMalloc(sizeof(float) * mlBatchStride * mlBatchArraySize); 
		mpIndexArray = (unsigned int*)hplMalloc(sizeof(unsigned int) * mlBatchArraySize); //Index is one int.

		for(int i=0;i<kMaxTextureUnits;i++)
		{
			mpTexCoordArray[i] = (float*)hplMalloc(sizeof(float) * 3 * mlBatchArraySize);
			mbTexCoordArrayActive[i] = false;
			mlTexCoordArrayCount[i]=0;
		}

		mbInitHasBeenRun = false;

		//Init extra stuff
#ifdef WITH_CG
		if(mGpuProgramFormat == eGpuProgramFormat_CG)
		{
			InitCG();
		}
#endif

		//TTF_Init();
	}

	//-----------------------------------------------------------------------

	cLowLevelGraphicsSDL::~cLowLevelGraphicsSDL()
	{
		//#ifdef WIN32
		//	if(mhKeyTrapper) FreeLibrary(mhKeyTrapper);
		//#endif

		if(mbInitHasBeenRun)
		{
#if !SDL_VERSION_ATLEAST(2, 0, 0)
			SDL_SetGammaRamp(mvStartGammaArray[0],mvStartGammaArray[1],mvStartGammaArray[2]);
#endif
		}

		hplFree(mpVertexArray);
		hplFree(mpIndexArray);
		for(int i=0;i<kMaxTextureUnits;i++)	hplFree(mpTexCoordArray[i]);

		//Exit extra stuff
#ifdef WITH_CG
		ExitCG();
#endif
		//TTF_Quit();
		ohpl_glctx_free(mpRs);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// GENERAL SETUP
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void CALLBACK OGLDebugOutputCallback(GLenum alSource, GLenum alType, GLuint alID, GLenum alSeverity, GLsizei alLength, const GLchar* apMessage, GLvoid* apUserParam)
	{
		if(alType == GL_DEBUG_TYPE_ERROR || apUserParam)
			Error("GL: %s\n", apMessage);
	}

	//-----------------------------------------------------------------------

	bool cLowLevelGraphicsSDL::Init(int alWidth, int alHeight, int alDisplay, int alBpp, int abFullscreen, 
		int alMultisampling, eGpuProgramFormat aGpuProgramFormat,const tString& asWindowCaption,
		const cVector2l &avWindowPos)
	{
		mvScreenSize.x = alWidth;
		mvScreenSize.y = alHeight;
        mlDisplay = alDisplay;
		mlBpp = alBpp;
		mbFullscreen = abFullscreen;

		mlMultisampling = alMultisampling;
		
		mGpuProgramFormat = aGpuProgramFormat;
		if(mGpuProgramFormat == eGpuProgramFormat_LastEnum) mGpuProgramFormat = eGpuProgramFormat_GLSL;

		OhplGlCtxCfg cfg = {alWidth, alHeight, alDisplay, alBpp, abFullscreen != 0, alMultisampling,
							getenv("OPENHPL_HEADLESS_SOCKET") != NULL, asWindowCaption.c_str(), 0, 0, false};
		if(!ohpl_glctx_create_window(mpRs, &cfg)) return false;
		mpScreen = (SDL_Window*)ohpl_glctx_window(mpRs);
		mvScreenSize = cVector2l(cfg.outW, cfg.outH);
        if (mbGrab) {
            SetWindowGrab(true);
        }

		//Trap Alt tab if in fullscreen
#if defined(WIN32) && !SDL_VERSION_ATLEAST(2,0,0)
		if(abFullscreen)
		{
			//mhKeyTrapper = LoadLibrary( "keyhook.dll" );
			//::DisableTaskKeys(true,false);
		}
#endif //WIN32

		// OHPL_NO_GLEW=1: the remaining C++ GL users (textures, buffers, programs) need GLEW
		if(getenv("OHPL_NO_GLEW") != NULL)
		{
			Log(" Glew skipped\n");
		}
		else
		{
		Log(" Init Glew...");
		GLenum lGlewErr = glewInit();
		// Wayland/EGL: no GLX display, but core GL entry points are loaded
		if(lGlewErr == GLEW_OK || lGlewErr == GLEW_ERROR_NO_GLX_DISPLAY)
		{
			Log("OK\n");
		}
		else
		{
			Error(" Couldn't init glew!\n");
		}
		}
		ohpl_glctx_load(mpRs);

		///Setup up windows specifc context:
#if defined(WIN32) && !SDL_VERSION_ATLEAST(2,0,0)
		mGLContext = wglGetCurrentContext();
		mDeviceContext = wglGetCurrentDC();
#endif

		//Check Multisample properties
		CheckMultisampleCaps();

		//Turn off cursor as default
		ShowCursor(false);

		//Gamma
		mfGammaCorrection = 1.0f;
#if !SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_GetGammaRamp(mvStartGammaArray[0],mvStartGammaArray[1],mvStartGammaArray[2]);

		SDL_SetGamma(mfGammaCorrection,mfGammaCorrection,mfGammaCorrection);
#endif

		//GL
		Log(" Setting up OpenGL\n");
		SetupGL();

		ohpl_glctx_swap(mpRs, 1.0f);

		mbInitHasBeenRun = true;

		ohpl_glctx_debug_from_env(mpRs);

		return true;
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::CheckMultisampleCaps()
	{

	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetupGL()
	{
		///////////////////////////////
		// Setup variables
		mvScissorPos =0;
		mvScissorSize = mvScreenSize;


		mpFrameBuffer = NULL;
		mvFrameBufferPos =0;
		mvFrameBufferSize = mvScreenSize;
		mvFrameBufferTotalSize = mvScreenSize;


		ohpl_glctx_setup_gl(mpRs);

		//Show some info
		char vCaps[1024];
		ohpl_glctx_caps_line(mpRs, vCaps, sizeof(vCaps));
		Log("  %s\n", vCaps);
		Log("  Vendor: %s\n", GlString(GL_VENDOR));
		Log("  Renderer: %s\n", GlString(GL_RENDERER));
		Log("  Version: %s\n", GlString(GL_VERSION));
		Log("  Max texture image units: %d\n",GetCaps(eGraphicCaps_MaxTextureImageUnits));
		Log("  Max texture coord units: %d\n",GetCaps(eGraphicCaps_MaxTextureCoordUnits));
		Log("  Max user clip planes: %d\n",GetCaps(eGraphicCaps_MaxUserClipPlanes));
		Log("  Two sided stencil: %d\n",GetCaps(eGraphicCaps_TwoSideStencil));
		Log("  Vertex Buffer Object: %d\n",GetCaps(eGraphicCaps_VertexBufferObject));

		Log("  Anisotropic filtering: %d\n",GetCaps(eGraphicCaps_AnisotropicFiltering));
		if(GetCaps(eGraphicCaps_AnisotropicFiltering))
			Log("  Max Anisotropic degree: %d\n",GetCaps(eGraphicCaps_MaxAnisotropicFiltering));

		Log("  Multisampling: %d\n",GetCaps(eGraphicCaps_Multisampling));

		Log("  Texture compression: %d\n",GetCaps(eGraphicCaps_TextureCompression));
		Log("  Texture compression S3TC: %d\n",GetCaps(eGraphicCaps_TextureCompression_DXTC));

		Log("  Auto generate MipMaps: %d\n",GetCaps(eGraphicCaps_AutoGenerateMipMaps));

		Log("  Render to texture: %d\n",GetCaps(eGraphicCaps_RenderToTexture));
		Log("  Max draw buffers: %d\n",GetCaps(eGraphicCaps_MaxDrawBuffers));
		Log("  Max color render targets: %d\n",GetCaps(eGraphicCaps_MaxColorRenderTargets));

		Log("  Packed depth-stencil: %d\n",GetCaps(eGraphicCaps_PackedDepthStencil));

		Log("  Texture float: %d\n",GetCaps(eGraphicCaps_TextureFloat));

		Log("  GLSL Version: %s\n", GlString(GL_SHADING_LANGUAGE_VERSION));
		Log("  ShaderModel 2: %d\n",GetCaps(eGraphicCaps_ShaderModel_2));
		Log("  ShaderModel 3: %d\n",GetCaps(eGraphicCaps_ShaderModel_3));
		Log("  ShaderModel 4: %d\n",GetCaps(eGraphicCaps_ShaderModel_4));

		Log("  OGL ATIFragmentShader: %d\n",GetCaps(eGraphicCaps_OGL_ATIFragmentShader));

	}
	//-----------------------------------------------------------------------

	const char* cLowLevelGraphicsSDL::GlString(unsigned int alName)
	{
		return (const char*)ohpl_glctx_get_string(mpRs, alName);
	}

	tString cLowLevelGraphicsSDL::GetRendererDescription()
	{
		return tString(GlString(GL_RENDERER)) + "  |  OpenGL " + GlString(GL_VERSION);
	}

	int cLowLevelGraphicsSDL::GetCaps(eGraphicCaps aType)
	{
		return ohpl_glctx_get_caps(mpRs, aType, mbForceShaderModel3And4Off);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::ShowCursor(bool abX)
	{
		ohpl_glctx_show_cursor(mpRs, abX);
	}
    
	//-----------------------------------------------------------------------

    void cLowLevelGraphicsSDL::SetWindowGrab(bool abX)
    {
        mbGrab = abX;
		ohpl_glctx_window_grab(mpRs, abX, getenv("OPENHPL_HEADLESS_SOCKET") != NULL);
    }

	void cLowLevelGraphicsSDL::SetRelativeMouse(bool abX)
	{
		ohpl_glctx_relative_mouse(mpRs, abX, getenv("OPENHPL_HEADLESS_SOCKET") != NULL);
	}

    void cLowLevelGraphicsSDL::SetWindowCaption(const tString &asName)
    {
		ohpl_glctx_set_caption(mpRs, asName.c_str());
    }

    bool cLowLevelGraphicsSDL::GetWindowMouseFocus()
    {
        return ohpl_glctx_mouse_focus(mpRs);
    }

    bool cLowLevelGraphicsSDL::GetWindowInputFocus()
    {
        return ohpl_glctx_input_focus(mpRs);
    }

    bool cLowLevelGraphicsSDL::GetWindowIsVisible()
    {
        return ohpl_glctx_is_visible(mpRs);
    }

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetVsyncActive(bool abX, bool abAdaptive)
	{
		ohpl_glctx_set_vsync(mpRs, abX, abAdaptive);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetMultisamplingActive(bool abX)
	{	
		ohpl_glctx_set_multisampling_active(mpRs, abX);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetGammaCorrection(float afX)
	{
		;

		mfGammaCorrection = afX;
#if !SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_SetGamma(mfGammaCorrection,mfGammaCorrection,mfGammaCorrection);
#endif
	}

	float cLowLevelGraphicsSDL::GetGammaCorrection()
	{
		;

		return mfGammaCorrection;
	}

	//-----------------------------------------------------------------------

	cVector2f cLowLevelGraphicsSDL::GetScreenSizeFloat()
	{
		;

		return cVector2f((float)mvScreenSize.x, (float)mvScreenSize.y);
	}

	const cVector2l& cLowLevelGraphicsSDL::GetScreenSizeInt()
	{
		;

		return mvScreenSize;
	}

	//-----------------------------------------------------------------------

	bool cLowLevelGraphicsSDL::ForceWindowSize(int alWidth, int alHeight)
	{
		return ohpl_glctx_force_window_size(mpRs, alWidth, alHeight);
	}

	bool cLowLevelGraphicsSDL::CheckAndUpdateScreenSize()
	{
		OhplGlV2 vSize;
		if(!ohpl_glctx_check_screen(mpRs, &vSize)) return false;
		mvScreenSize = cVector2l(vSize.x, vSize.y);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// DATA CREATION
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iFontData* cLowLevelGraphicsSDL::CreateFontData(const tString &asName)
	{
		;

		return hplNew( cSDLFontData, (asName, this) );
	}

	//-----------------------------------------------------------------------

	iGpuProgram* cLowLevelGraphicsSDL::CreateGpuProgram(const tString& asName)
	{
		;

		return hplNew( cGLSLProgram, (asName) );
		//return hplNew( cCGProgram, () ); 
	}

	iGpuShader* cLowLevelGraphicsSDL::CreateGpuShader(const tString& asName, eGpuShaderType aType)
	{
		;

		return hplNew( cGLSLShader, (asName,aType, this) );
		//return hplNew( cCGShader, (asName,mCG_Context, aType) );
	}

	//-----------------------------------------------------------------------

	iTexture* cLowLevelGraphicsSDL::CreateTexture(const tString &asName,eTextureType aType,   eTextureUsage aUsage)
	{
		;

		cSDLTexture *pTexture = hplNew( cSDLTexture, (asName,aType, aUsage, this) );

		return pTexture;
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* cLowLevelGraphicsSDL::CreateVertexBuffer(eVertexBufferType aType,
															eVertexBufferDrawType aDrawType,
															eVertexBufferUsageType aUsageType, 
															int alReserveVtxSize,int alReserveIdxSize)
	{
		;

		//return hplNew( cVertexBufferVBO,(this, aFlags,aDrawType,aUsageType,alReserveVtxSize,alReserveIdxSize) );
		//return hplNew( cVertexBufferOGL, (this, aFlags,aDrawType,aUsageType,alReserveVtxSize,alReserveIdxSize) );

		if(GetCaps(eGraphicCaps_VertexBufferObject) && aType == eVertexBufferType_Hardware)
		{
			return hplNew( cVertexBufferOGL_VBO, (this, aDrawType,aUsageType,alReserveVtxSize,alReserveIdxSize) );
		}
		else
		{
			//Error("VBO is not supported, using Vertex array!\n");
			return hplNew( cVertexBufferOGL_Array, (this, aDrawType,aUsageType,alReserveVtxSize,alReserveIdxSize) );
		}
	}

	//-----------------------------------------------------------------------

	iFrameBuffer* cLowLevelGraphicsSDL::CreateFrameBuffer(const tString& asName)
	{
		;

		if(GetCaps(eGraphicCaps_RenderToTexture)==0) return NULL;

		return hplNew(cFrameBufferGL,(asName, this));
	}

	//-----------------------------------------------------------------------

	iDepthStencilBuffer* cLowLevelGraphicsSDL::CreateDepthStencilBuffer(const cVector2l& avSize, int alDepthBits, int alStencilBits)
	{
		;

		if(GetCaps(eGraphicCaps_RenderToTexture)==0) return NULL;

		return hplNew(cDepthStencilBufferGL,(avSize, alDepthBits,alStencilBits));
	}

	//-----------------------------------------------------------------------

	iOcclusionQuery* cLowLevelGraphicsSDL::CreateOcclusionQuery()
	{
		;

		return hplNew(cOcclusionQueryOGL, () );
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// FRAME BUFFER OPERATIONS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::ClearFrameBuffer(tClearFrameBufferFlag aFlags)
	{
		ohpl_glctx_clear(mpRs, aFlags);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetClearColor(const cColor& aCol){
		ohpl_glctx_set_clear_color(mpRs, aCol.r, aCol.g, aCol.b, aCol.a);
	}
	void cLowLevelGraphicsSDL::SetClearDepth(float afDepth){
		ohpl_glctx_set_clear_depth(mpRs, afDepth);
	}
	void cLowLevelGraphicsSDL::SetClearStencil(int alVal){
		ohpl_glctx_set_clear_stencil(mpRs, alVal);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::CopyFrameBufferToTexure(iTexture* apTex, const cVector2l &avPos,
													const cVector2l &avSize, const cVector2l &avTexOffset)
	{
		if(apTex==NULL)return;

		SetTexture(0, apTex);
		ohpl_glctx_copy_to_texture(mpRs, OhplGl(avPos), OhplGl(avSize), OhplGl(avTexOffset), apTex->GetType(), apTex->GetHeight());
	}

	//-----------------------------------------------------------------------

	cBitmap* cLowLevelGraphicsSDL::CopyFrameBufferToBitmap(	const cVector2l &avScreenPos,const cVector2l &avScreenSize)
	{
		;

		int vRect[4];
		ohpl_glctx_read_rect(mpRs, OhplGl(avScreenPos), OhplGl(avScreenSize), &vRect);
		cVector2l vSize(vRect[2], vRect[3]);
		cVector2l vPos(vRect[0], vRect[1]);

		cBitmap *pBitmap = hplNew(cBitmap, () );

		///////////////////////////////////
		//Get the correct pixel format
        ePixelFormat pixelFormat = ePixelFormat_RGBA;
		if(mpFrameBuffer)
		{
			iFrameBufferAttachment *pColorBuffer = mpFrameBuffer->GetColorBuffer(0);
			if(pColorBuffer && pColorBuffer->GetFrameBufferAttachmentType() == eFrameBufferAttachment_Texture)
			{
				iTexture *pTexture = static_cast<iTexture*>(pColorBuffer);
				pixelFormat = pTexture->GetPixelFormat();
			}
		}

		//////////////////////////////
		//Get create the bitmap data
		pBitmap->CreateData(cVector3l(vSize.x, vSize.y,1),pixelFormat,0,0);

		//////////////////////////////
		//Copy pixels
		ohpl_glctx_read_pixels(mpRs, &vRect, pixelFormat, pBitmap->GetData(0,0)->mpData);

		//TODO: Flip, not needed?

		return pBitmap;
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetCurrentFrameBuffer(iFrameBuffer* apFrameBuffer, const cVector2l &avPos, const cVector2l& avSize)
	{
		;

		///////////////////////////////////////////////////////
		//Set frame buffer 
		if(true)//mpFrameBuffer != apFrameBuffer)
		{
			iFrameBuffer *pPrevFameBuffer = mpFrameBuffer;
			mpFrameBuffer = apFrameBuffer;

			ohpl_glctx_bind_framebuffer(mpRs, mpFrameBuffer ? static_cast<cFrameBufferGL*>(mpFrameBuffer)->GetHandle() : 0);

			// Post tasks like generating mipmaps
			if(pPrevFameBuffer)
			{
				pPrevFameBuffer->PostBindUpdate();
			}
		}

		/////////////////////////////////////
		//Get framebuffer size and set viewport
		const cVector2l vTarget = mpFrameBuffer ? mpFrameBuffer->GetSize() : mvScreenSize;
		ohpl_glctx_set_viewport(mpRs, OhplGl(vTarget), OhplGl(avPos), OhplGl(avSize));
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetFrameBufferDrawTargets(int *apTargets, int alNumOfTargets)
	{
		ohpl_glctx_set_draw_targets(mpRs, apTargets, alNumOfTargets);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::WaitAndFinishRendering()
	{
		ohpl_glctx_finish(mpRs);
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::FlushRendering()
	{
		ohpl_glctx_flush(mpRs);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SwapBuffers()
	{
		ohpl_glctx_swap(mpRs, mfGammaCorrection);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// RENDER STATE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetColorWriteActive(bool abR,bool abG,bool abB,bool abA)
	{
		ohpl_glctx_set_color_write(mpRs, abR, abG, abB, abA);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetDepthWriteActive(bool abX)
	{
		ohpl_glctx_set_depth_write(mpRs, abX);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetDepthTestActive(bool abX)
	{
		ohpl_glctx_set_depth_test(mpRs, abX);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetDepthTestFunc(eDepthTestFunc aFunc)
	{
		ohpl_glctx_set_depth_func(mpRs, aFunc);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetAlphaTestActive(bool abX)
	{
		ohpl_glctx_set_alpha_test(mpRs, abX);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetAlphaTestFunc(eAlphaTestFunc aFunc,float afRef)
	{
		ohpl_glctx_set_alpha_func(mpRs, aFunc, afRef);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencilActive(bool abX)
	{
		ohpl_glctx_set_stencil_active(mpRs, abX);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencilWriteMask(unsigned int alMask)
	{
		ohpl_glctx_set_stencil_write_mask(mpRs, alMask);
	}
	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencil(eStencilFunc aFunc,int alRef, unsigned int aMask,
		eStencilOp aFailOp,eStencilOp aZFailOp,eStencilOp aZPassOp)
	{
		ohpl_glctx_set_stencil(mpRs, aFunc, alRef, aMask, aFailOp, aZFailOp, aZPassOp);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencilTwoSide(eStencilFunc aFrontFunc,eStencilFunc aBackFunc,
		int alRef, unsigned int aMask,
		eStencilOp aFrontFailOp,eStencilOp aFrontZFailOp,eStencilOp aFrontZPassOp,
		eStencilOp aBackFailOp,eStencilOp aBackZFailOp,eStencilOp aBackZPassOp)
	{
		const int vOps[6] = {aFrontFailOp, aFrontZFailOp, aFrontZPassOp, aBackFailOp, aBackZFailOp, aBackZPassOp};
		ohpl_glctx_set_stencil_two_side(mpRs, aFrontFunc, aBackFunc, alRef, aMask, &vOps);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetCullActive(bool abX)
	{
		ohpl_glctx_set_cull_active(mpRs, abX);
	}
	void cLowLevelGraphicsSDL::SetCullMode(eCullMode aMode)
	{
		ohpl_glctx_set_cull_mode(mpRs, aMode);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetScissorActive(bool abX)
	{
		ohpl_glctx_set_scissor_active(mpRs, abX);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetScissorRect(const cVector2l& avPos, const cVector2l& avSize)
	{
		ohpl_glctx_set_scissor_rect(mpRs, mpFrameBuffer ? mpFrameBuffer->GetSize().y : mvScreenSize.y, OhplGl(avPos), OhplGl(avSize));
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetClipPlane(int alIdx, const cPlanef& aPlane)
	{
		mvClipPlanes[alIdx] = aPlane;
		ohpl_glctx_set_clip_plane(mpRs, alIdx, aPlane.a, aPlane.b, aPlane.c, aPlane.d);
	}
	cPlanef cLowLevelGraphicsSDL::GetClipPlane(int alIdx)
	{
		;

		return mvClipPlanes[alIdx];
	}
	void cLowLevelGraphicsSDL::SetClipPlaneActive(int alIdx, bool abX)
	{
		ohpl_glctx_set_clip_plane_active(mpRs, alIdx, abX);
	}


	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetBlendActive(bool abX)
	{
		ohpl_glctx_set_blend_active(mpRs, abX);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetBlendFunc(eBlendFunc aSrcFactor, eBlendFunc aDestFactor)
	{
		ohpl_glctx_set_blend_func(mpRs, aSrcFactor, aDestFactor);
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetBlendFuncSeparate(eBlendFunc aSrcFactorColor, eBlendFunc aDestFactorColor,
		eBlendFunc aSrcFactorAlpha, eBlendFunc aDestFactorAlpha)
	{
		ohpl_glctx_set_blend_func_separate(mpRs, aSrcFactorColor, aDestFactorColor, aSrcFactorAlpha, aDestFactorAlpha);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetDepthClampActive(bool abX)
	{
		ohpl_glctx_set_depth_clamp_active(mpRs, abX);
	}

	void cLowLevelGraphicsSDL::SetPolygonOffsetActive(bool abX)
	{
		ohpl_glctx_set_polygon_offset_active(mpRs, abX);
	}

	void cLowLevelGraphicsSDL::SetPolygonOffset(float afBias, float afSlopeScaleBias)
	{
		ohpl_glctx_set_polygon_offset(mpRs, afBias, afSlopeScaleBias);
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// MATRIX
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::PushMatrix(eMatrix aMtxType)
	{
		ohpl_glctx_push_matrix(mpRs, aMtxType);
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::PopMatrix(eMatrix aMtxType)
	{
		ohpl_glctx_pop_matrix(mpRs, aMtxType);
	}
	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetMatrix(eMatrix aMtxType, const cMatrixf& a_mtxA)
	{
		ohpl_glctx_set_matrix(mpRs, aMtxType, &a_mtxA.v);
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetIdentityMatrix(eMatrix aMtxType)
	{
		ohpl_glctx_set_identity_matrix(mpRs, aMtxType);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetOrthoProjection(const cVector2f& avSize, float afMin, float afMax)
	{
		ohpl_glctx_set_ortho(mpRs, 0, avSize.x, avSize.y, 0, afMin, afMax);
	}

	void cLowLevelGraphicsSDL::SetOrthoProjection(const cVector3f& avMin, const cVector3f& avMax)
	{
		ohpl_glctx_set_ortho(mpRs, avMin.x, avMax.x, avMax.y, avMin.y, avMin.z, avMax.z);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// TEXTURE OPERATIONS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetTexture(unsigned int alUnit,iTexture* apTex)
	{
		ohpl_glctx_set_texture(mpRs, alUnit, apTex ? apTex->GetType() : 0, apTex != NULL, apTex ? static_cast<cSDLTexture*>(apTex)->GetTextureHandle() : 0);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetActiveTextureUnit(unsigned int alUnit)
	{
		ohpl_glctx_set_active_texture_unit(mpRs, alUnit);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetTextureEnv(eTextureParam aParam, int alVal)
	{
		ohpl_glctx_set_texture_env(mpRs, aParam, alVal);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetTextureConstantColor(const cColor &aColor)
	{
		ohpl_glctx_set_texture_constant_color(mpRs, aColor.r, aColor.g, aColor.b, aColor.a);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetColor(const cColor &aColor)
	{
		ohpl_glctx_set_color(mpRs, aColor.r, aColor.g, aColor.b, aColor.a);
	}

	//-----------------------------------------------------------------------



	//////////////////////////////////////////////////////////////////////////
	// DRAWING
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

#define IM_CLASS cLowLevelGraphicsSDL
#define IM_BEGIN(m) (ohpl_glctx_begin(mpRs, m))
#define IM_END() (ohpl_glctx_end(mpRs))
#define IM_VERTEX3(x,y,z) (ohpl_glctx_vertex3f(mpRs, x, y, z))
#define IM_COLOR4(r,g,b,a) (ohpl_glctx_color4f(mpRs, r, g, b, a))
#define IM_TEXCOORD2(s,t) (ohpl_glctx_texcoord2f(mpRs, s, t))
#define IM_TEXCOORD3(s,t,r) (ohpl_glctx_texcoord3f(mpRs, s, t, r))
#define IM_MULTITEXCOORD2(u,s,t) (ohpl_glctx_multitexcoord2f(mpRs, u, s, t))
#define IM_MULTITEXCOORD3(u,s,t,r) (ohpl_glctx_multitexcoord3f(mpRs, u, s, t, r))
#define IM_QUADS GL_QUADS
#define IM_LINES GL_LINES
#define IM_LINE_STRIP GL_LINE_STRIP
#define IM_TRIANGLES GL_TRIANGLES
#define IM_TEXTURE0 GL_TEXTURE0_ARB
#define IM_TEXTURE1 GL_TEXTURE1_ARB
#include "LowLevelGraphicsImmediate.inl"


	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetBatchTextureUnitActive(unsigned int alUnit,bool abActive)
	{
		ohpl_glctx_batch_texture_unit(mpRs, alUnit, abActive, sizeof(float)*mlBatchStride, mpVertexArray, mpTexCoordArray);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::FlushTriBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear)
	{
		ohpl_glctx_flush_batch(mpRs, false, aTypeFlags, sizeof(float)*mlBatchStride, mlIndexCount, mpIndexArray, mpVertexArray, mpTexCoordArray);
		if(abAutoClear){
			mlIndexCount = 0;
			mlVertexCount = 0;
			for(int i=0;i<kMaxTextureUnits;i++)
				mlTexCoordArrayCount[i]=0;
		}
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::FlushQuadBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear)
	{
		ohpl_glctx_flush_batch(mpRs, true, aTypeFlags, sizeof(float)*mlBatchStride, mlIndexCount, mpIndexArray, mpVertexArray, mpTexCoordArray);
		if(abAutoClear){
			mlIndexCount = 0;
			mlVertexCount = 0;
			for(int i=0;i<kMaxTextureUnits;i++)
				mlTexCoordArrayCount[i]=0;
		}
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::ClearBatch()
	{
		mlIndexCount = 0;
		mlVertexCount = 0;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// IMPLEMENTAION SPECIFICS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

#ifdef WITH_CG
	void cLowLevelGraphicsSDL::InitCG()
	{
		mCG_Context = cgCreateContext();
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::ExitCG()
	{
		cgDestroyContext(mCG_Context);
	}
#endif

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// GLOBAL FUNCTIONS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	GLenum GetGLWrapEnum(eTextureWrap aMode)
	{
		return ohpl_gl_wrap(aMode);
	}


	//-----------------------------------------------------------------------

	GLenum PixelFormatToGLFormat(ePixelFormat aFormat)
	{
		return ohpl_gl_pixel_format(aFormat);
	}

	//-------------------------------------------------

	GLenum PixelFormatToGLInternalFormat(ePixelFormat aFormat)
	{
		return ohpl_gl_pixel_internal_format(aFormat);
	}

	//-------------------------------------------------

	GLenum GetGLCompressionFormatFromPixelFormat(ePixelFormat aFormat)
	{
		return ohpl_gl_compression_format(aFormat);
	}

	//-------------------------------------------------

	GLenum TextureTypeToGLTarget(eTextureType aType)
	{
		return ohpl_gl_texture_target(aType);
	}

	//-----------------------------------------------------------------------

	GLenum GetGLBlendEnum(eBlendFunc aType)
	{
		return ohpl_gl_blend(aType);
	}

	//-----------------------------------------------------------------------

	GLenum GetGLTextureParamEnum(eTextureParam aType)
	{
		return ohpl_gl_texture_param(aType);
	}

	//-----------------------------------------------------------------------

	GLenum GetGLTextureOpEnum(eTextureOp aType)
	{
		return ohpl_gl_texture_op(aType);
	}

	//-----------------------------------------------------------------------

	GLenum GetGLTextureSourceEnum(eTextureSource aType)
	{
		return ohpl_gl_texture_source(aType);
	}
	//-----------------------------------------------------------------------

	GLenum GetGLTextureTargetEnum(eTextureType aType)
	{
		return ohpl_gl_texture_target(aType);
	}

	//-----------------------------------------------------------------------

	GLenum GetGLTextureCompareMode(eTextureCompareMode aMode)
	{
		return ohpl_gl_compare_mode(aMode);
	}
	
	GLenum GetGLTextureCompareFunc(eTextureCompareFunc aFunc)
	{
		return ohpl_gl_compare_func(aFunc);
	}

	//-----------------------------------------------------------------------

	GLenum GetGLTextureFuncEnum(eTextureFunc aType)
	{
		return ohpl_gl_texture_func(aType);
	}

	//-----------------------------------------------------------------------
	GLenum GetGLDepthTestFuncEnum(eDepthTestFunc aType)
	{
		return ohpl_gl_compare(aType);
	}

	//-----------------------------------------------------------------------

	GLenum GetGLAlphaTestFuncEnum(eAlphaTestFunc aType)
	{
		return ohpl_gl_compare(aType);
	}

	//-----------------------------------------------------------------------

	GLenum GetGLStencilFuncEnum(eStencilFunc aType)
	{
		return ohpl_gl_compare(aType);
	}

	//-----------------------------------------------------------------------

	GLenum GetGLStencilOpEnum(eStencilOp aType)
	{
		return ohpl_gl_stencil_op(aType);
	}

	//-----------------------------------------------------------------------



}
