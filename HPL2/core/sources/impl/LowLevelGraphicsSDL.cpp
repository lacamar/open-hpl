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

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cLowLevelGraphicsSDL::cLowLevelGraphicsSDL()
	{
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

		ohpl_gl_state_new(&mGl);

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
#if SDL_VERSION_ATLEAST(2, 0, 0)
        SDL_DestroyWindow(mpScreen);
#endif
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

		//Set some GL Attributes
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

		// Multisampling
		if(mlMultisampling > 0)
		{
			if(SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1)==-1)
			{
				Error("Multisample buffers not supported!\n");
			}
			else
			{
				if(SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, mlMultisampling)==-1)
				{
					Error("Couldn't set multisampling samples to %d\n",mlMultisampling);
				}
			}
		}

#if SDL_VERSION_ATLEAST(2, 0, 0)
        unsigned int mlFlags = SDL_WINDOW_OPENGL;
        if (alWidth == 0 && alHeight == 0) {
            mvScreenSize = cVector2l(800,600);
            mlFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        } else if (abFullscreen) {
            const char *pDriver = SDL_GetCurrentVideoDriver();
            if(pDriver != NULL && cString::ToLowerCase(pDriver) == "wayland")
            {
                mlFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            }
            else
            {
                mlFlags |= SDL_WINDOW_FULLSCREEN;
            }
        }

        if (getenv("OPENHPL_HEADLESS_SOCKET") != NULL) {
            mlFlags |= SDL_WINDOW_HIDDEN;
        }


        Log(" Setting video mode: %d x %d - %d bpp\n",alWidth, alHeight, alBpp);
        mpScreen = SDL_CreateWindow(asWindowCaption.c_str(),
                                    SDL_WINDOWPOS_CENTERED_DISPLAY(mlDisplay), SDL_WINDOWPOS_CENTERED_DISPLAY(mlDisplay),
                                    mvScreenSize.x, mvScreenSize.y, mlFlags);
		if(mpScreen==NULL)
        {
            // try disabling FSAA
			Error("Could not set display mode setting a lower one! %s\n", SDL_GetError());
			mvScreenSize = cVector2l(640,480);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
            mpScreen = SDL_CreateWindow(asWindowCaption.c_str(),
                                        SDL_WINDOWPOS_CENTERED_DISPLAY(mlDisplay), SDL_WINDOWPOS_CENTERED_DISPLAY(mlDisplay),
                                        mvScreenSize.x, mvScreenSize.y, mlFlags);
            if(mpScreen==NULL)
            {
                FatalError("Unable to initialize display! %s\n", SDL_GetError());
                return false;
            }
            else
            {
                cPlatform::CreateMessageBox(_W("Warning!"),_W("Could not set displaymode and 640x480 is used instead!\n"));
            }
        }
        {
            // update with the screen size ACTUALLY obtained
            int w,h;
            SDL_GetWindowSize(mpScreen, &w, &h);
            mvScreenSize = cVector2l(w, h);
        }
        mGLContext = SDL_GL_CreateContext(mpScreen);
#else
		unsigned int mlFlags = SDL_OPENGL;

		if(abFullscreen) mlFlags |= SDL_FULLSCREEN;

		// If caption set before engine creation, no chance for the "SDL_App" to appear for even a msec
		SetWindowCaption(asWindowCaption);

		Log(" Setting video mode: %d x %d - %d bpp\n",alWidth, alHeight, alBpp);
		mpScreen = SDL_SetVideoMode( alWidth, alHeight, alBpp, mlFlags);
		if(mpScreen==NULL){
			Error("Could not set display mode setting a lower one!\n");
			mvScreenSize = cVector2l(640,480);

			mpScreen = SDL_SetVideoMode( mvScreenSize.x, mvScreenSize.y, alBpp, mlFlags);
			if(mpScreen==NULL)
			{
				FatalError("Unable to initialize display!\n");
				return false;
			}
			else
			{
				//SetWindowCaption(asWindowCaption);
				cPlatform::CreateMessageBox(_W("Warning!"),_W("Could not set displaymode and 640x480 is used instead!\n"));
			}
		}
		else
		{
			//SetWindowCaption(asWindowCaption);
		}
        // update with the screen size ACTUALLY obtained
        mvScreenSize = cVector2l(mpScreen->w, mpScreen->h);
#   ifdef WIN32
		//////////////////////////////
		// Set up window position
		if(abFullscreen==false)
		{
			SDL_SysWMinfo pInfo;
			SDL_VERSION(&pInfo.version);
			SDL_GetWMInfo(&pInfo);

			RECT r;
			GetWindowRect(pInfo.window, &r);
			
			if(avWindowPos.x >=0 && avWindowPos.y >=0)
			{
				SetWindowPos(pInfo.window, HWND_TOP, avWindowPos.x, avWindowPos.y, 0, 0,  SWP_NOSIZE);
			}
		}
#   endif
#endif
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
		mlGammaProgram = 0;
		mlGammaTexture = 0;
		mvGammaTextureSize = 0;
#if !SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_GetGammaRamp(mvStartGammaArray[0],mvStartGammaArray[1],mvStartGammaArray[2]);

		SDL_SetGamma(mfGammaCorrection,mfGammaCorrection,mfGammaCorrection);
#endif

		//GL
		Log(" Setting up OpenGL\n");
		SetupGL();

#if SDL_VERSION_ATLEAST(2, 0, 0)
        SDL_GL_SwapWindow(mpScreen);
#else
		//Set the clear color
		SDL_GL_SwapBuffers();
#endif

		mbInitHasBeenRun = true;


		if(getenv("OPENHPL_GL_DEBUG") && glDebugMessageCallback)
		{
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback((GLDEBUGPROC)&OGLDebugOutputCallback, strcmp(getenv("OPENHPL_GL_DEBUG"), "all") ? NULL : this);
		}


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
		ohpl_gl_state_setup(&mGl);
		mvScissorPos =0;
		mvScissorSize = mvScreenSize;


		mpFrameBuffer = NULL;
		mvFrameBufferPos =0;
		mvFrameBufferSize = mvScreenSize;
		mvFrameBufferTotalSize = mvScreenSize;


		///////////////////////////////
		//Inits GL stuff
		//Set Shade model and clear color.
		glShadeModel(GL_SMOOTH);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

		///////////////////////////////
		//Depth Test setup
		glClearDepth(1.0f);//VAlues buffer is cleared with
		glEnable(GL_DEPTH_TEST); //enable depth testing
		glDepthFunc(GL_LEQUAL); //function to do depth test with
		glDisable(GL_ALPHA_TEST);
		glDepthMask(true);

		///////////////////////////////
		//Render Settings
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GetGLDepthTestFuncEnum((eDepthTestFunc)mGl.depthFunc));
		glAlphaFunc(GetGLAlphaTestFuncEnum((eAlphaTestFunc)mGl.alphaFunc),mGl.alphaRef);

		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CW);

		glDisable(GL_SCISSOR_TEST);
		glScissor(mvScissorPos.x, (mvScissorSize.y - mvScissorPos.y - 1)-mvScissorSize.y, mvScissorSize.x, mvScissorSize.y);
		glDisable(GL_BLEND);

		///////////////////////////////
		//Set best perspective correction
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

		//int lStencilBits=-1;
		//glGetIntegerv(GL_STENCIL_BITS,&lStencilBits);
		//Log(" Stencil bits: %d\n",lStencilBits);

		///////////////////////////////
		//Stencil setup
		glClearStencil(0);

		///////////////////////////////
		//Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		//glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);


		/////  BEGIN BATCH ARRAY STUFF ///////////////

		//Enable all the vertex arrays that are used:
		glEnableClientState(GL_VERTEX_ARRAY ); //The positions
		glEnableClientState(GL_COLOR_ARRAY ); //The color
		glEnableClientState(GL_TEXTURE_COORD_ARRAY); //Tex coords
		glDisableClientState(GL_NORMAL_ARRAY);
		//Disable the once not used.
		glDisableClientState(GL_INDEX_ARRAY); //color index
		glDisableClientState(GL_EDGE_FLAG_ARRAY);

		///// END BATCH ARRAY STUFF ///////////////


		//Show some info
		Log("  Vendor: %s\n", glGetString(GL_VENDOR));
		Log("  Renderer: %s\n", glGetString(GL_RENDERER));
		Log("  Version: %s\n", glGetString(GL_VERSION));
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

		Log("  GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
		Log("  ShaderModel 2: %d\n",GetCaps(eGraphicCaps_ShaderModel_2));
		Log("  ShaderModel 3: %d\n",GetCaps(eGraphicCaps_ShaderModel_3));
		Log("  ShaderModel 4: %d\n",GetCaps(eGraphicCaps_ShaderModel_4));

		Log("  OGL ATIFragmentShader: %d\n",GetCaps(eGraphicCaps_OGL_ATIFragmentShader));

	}
	//-----------------------------------------------------------------------

	tString cLowLevelGraphicsSDL::GetRendererDescription()
	{
		return tString((const char*)glGetString(GL_RENDERER)) + "  |  OpenGL " + (const char*)glGetString(GL_VERSION);
	}

	int cLowLevelGraphicsSDL::GetCaps(eGraphicCaps aType)
	{
		;

		switch(aType)
		{
		case eGraphicCaps_TextureTargetRectangle:	return 1;//GLEW_ARB_texture_rectangle?1:0;
		
		case eGraphicCaps_VertexBufferObject:		return GLEW_ARB_vertex_buffer_object?1:0;
		case eGraphicCaps_TwoSideStencil:			
			{
				if(GLEW_EXT_stencil_two_side) return 1;
				else if(GLEW_ATI_separate_stencil) return 1;
				else return 0;
			}

		case eGraphicCaps_MaxTextureImageUnits:
			{
				int lUnits;
				glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB,(GLint *)&lUnits);
				return lUnits;
			}

		case eGraphicCaps_MaxTextureCoordUnits:
			{
				int lUnits;
				glGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB,(GLint *)&lUnits);
				return lUnits;
			}
		case eGraphicCaps_MaxUserClipPlanes:
			{
				int lClipPlanes;
				glGetIntegerv( GL_MAX_CLIP_PLANES,(GLint *)&lClipPlanes);
				return lClipPlanes;	
			}

		case eGraphicCaps_AnisotropicFiltering:		return GLEW_EXT_texture_filter_anisotropic ? 1 : 0;
		
		case eGraphicCaps_MaxAnisotropicFiltering:
			{
				if(!GLEW_EXT_texture_filter_anisotropic) return 0;

				float fMax;
				glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&fMax);
				return (int)fMax;
			}

		case eGraphicCaps_Multisampling: return GLEW_ARB_multisample ? 1: 0;
		
		case eGraphicCaps_TextureCompression:		return GLEW_ARB_texture_compression  ? 1 : 0;
		case eGraphicCaps_TextureCompression_DXTC:	return GLEW_EXT_texture_compression_s3tc ? 1 : 0;
		
		case eGraphicCaps_AutoGenerateMipMaps:		return GLEW_SGIS_generate_mipmap ? 1 : 0;
	
		case eGraphicCaps_RenderToTexture:			return GLEW_EXT_framebuffer_object ? 1: 0;
		
		case eGraphicCaps_MaxDrawBuffers:
			{
				GLint lMaxbuffers;
				glGetIntegerv(GL_MAX_DRAW_BUFFERS, &lMaxbuffers);
				return lMaxbuffers;
			}
		case eGraphicCaps_PackedDepthStencil:	return GLEW_EXT_packed_depth_stencil ? 1: 0;		
		case eGraphicCaps_TextureFloat:			return GLEW_ARB_texture_float ? 1: 0;

		case eGraphicCaps_PolygonOffset:		return 1;	//OpenGL always support it!

		case eGraphicCaps_ShaderModel_2:		return (GLEW_ARB_fragment_program || GLEW_ARB_fragment_shader) ? 1 : 0;	//Mac always support this, so not a good test.
#ifdef __APPLE__
		case eGraphicCaps_ShaderModel_3:		return 0; // Force return false for OS X as dynamic branching doesn't work well (it's slow)
		case eGraphicCaps_ShaderModel_4:		return 0;
#else
		case eGraphicCaps_ShaderModel_3:
			{
				if(mbForceShaderModel3And4Off)
					return 0;
				else
					return  (GLEW_NV_vertex_program3 || GLEW_ATI_shader_texture_lod) ? 1 : 0;
			}
		case eGraphicCaps_ShaderModel_4:
			{
				if(mbForceShaderModel3And4Off)
					return 0;
				else
					return  GLEW_EXT_gpu_shader4 ? 1 : 0;
			}
#endif
		
		case eGraphicCaps_OGL_ATIFragmentShader: return  GLEW_ATI_fragment_shader ? 1 : 0;

		case eGraphicCaps_MaxColorRenderTargets:
			{
				GLint lMax;
				glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &lMax);
				return lMax;
			}
		}
		return 0;
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::ShowCursor(bool abX)
	{
		;

		if(abX)
			SDL_ShowCursor(SDL_ENABLE);
		else
			SDL_ShowCursor(SDL_DISABLE);
	}
    
	//-----------------------------------------------------------------------

    void cLowLevelGraphicsSDL::SetWindowGrab(bool abX)
    {
        mbGrab = abX;
        if(getenv("OPENHPL_HEADLESS_SOCKET") != NULL) return;
#if SDL_VERSION_ATLEAST(2, 0, 0)
        if (mpScreen) {
            SDL_SetWindowGrab(mpScreen, abX ? SDL_TRUE : SDL_FALSE);
        }
#else
		SDL_WM_GrabInput(abX ? SDL_GRAB_ON : SDL_GRAB_OFF);
#endif
    }

	void cLowLevelGraphicsSDL::SetRelativeMouse(bool abX)
	{
		if(getenv("OPENHPL_HEADLESS_SOCKET") != NULL) return;
#if SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_SetRelativeMouseMode(abX ? SDL_TRUE : SDL_FALSE);
#endif
	}

    void cLowLevelGraphicsSDL::SetWindowCaption(const tString &asName)
    {
#if SDL_VERSION_ATLEAST(2, 0, 0)
        SDL_SetWindowTitle(mpScreen, asName.c_str());
#else
        SDL_WM_SetCaption(asName.c_str(), "");
#endif
    }

    bool cLowLevelGraphicsSDL::GetWindowMouseFocus()
    {
#if SDL_VERSION_ATLEAST(2, 0, 0)
        return (SDL_GetWindowFlags(mpScreen) & SDL_WINDOW_MOUSE_FOCUS) != 0;
#else
        return (SDL_GetAppState() & SDL_APPMOUSEFOCUS) !=0;
#endif
    }

    bool cLowLevelGraphicsSDL::GetWindowInputFocus()
    {
#if SDL_VERSION_ATLEAST(2, 0, 0)
        return (SDL_GetWindowFlags(mpScreen) & SDL_WINDOW_INPUT_FOCUS) != 0;
#else
        return (SDL_GetAppState() & SDL_APPINPUTFOCUS) !=0;
#endif
    }

    bool cLowLevelGraphicsSDL::GetWindowIsVisible()
    {
#if SDL_VERSION_ATLEAST(2, 0, 0)
        return (SDL_GetWindowFlags(mpScreen) & SDL_WINDOW_SHOWN) != 0;
#else
        return (SDL_GetAppState() & SDL_APPACTIVE) !=0;
#endif
    }

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetVsyncActive(bool abX, bool abAdaptive)
	{
        ;
#if SDL_VERSION_ATLEAST(2, 0, 0)
        SDL_GL_SetSwapInterval(abX ? (abAdaptive ? -1 : 1) : 0);
#elif defined(WIN32)
		if(WGLEW_EXT_swap_control)
		{
			wglSwapIntervalEXT(abX ? (abAdaptive ? -1 : 1) : 0);
		}
#elif defined(__linux__)
		if (GLX_SGI_swap_control)
		{
			GLXSWAPINTERVALPROC glXSwapInterval = (GLXSWAPINTERVALPROC)glXGetProcAddress((GLubyte*)"glXSwapIntervalSGI");
			glXSwapInterval(abX ? (abAdaptive ? -1 : 1) : 0);
		}
		else if (GLX_MESA_swap_control)
		{
			GLXSWAPINTERVALPROC glXSwapInterval = (GLXSWAPINTERVALPROC)glXGetProcAddress((GLubyte*)"glXSwapIntervalMESA");
			glXSwapInterval(abX ? (abAdaptive ? -1 : 1) : 0);
		}
#elif defined(__APPLE__)
		CGLContextObj ctx = CGLGetCurrentContext();
		GLint swap = abX ? 1 : 0;
		CGLSetParameter(ctx, kCGLCPSwapInterval, &swap);
#endif
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetMultisamplingActive(bool abX)
	{	
		;

		if(!GLEW_ARB_multisample || mlMultisampling<=0) return;

		if(abX)
			glEnable(GL_MULTISAMPLE_ARB);
		else
			glDisable(GL_MULTISAMPLE_ARB);
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
#if SDL_VERSION_ATLEAST(2, 0, 0)
		if(mpScreen == NULL) return false;
		SDL_SetWindowFullscreen(mpScreen, 0);
		SDL_SetWindowSize(mpScreen, alWidth, alHeight);
		return true;
#else
		return false;
#endif
	}

	bool cLowLevelGraphicsSDL::CheckAndUpdateScreenSize()
	{
#if SDL_VERSION_ATLEAST(2, 0, 0)
		if(mpScreen == NULL) return false;

		int lW = 0, lH = 0;
		SDL_GetWindowSize(mpScreen, &lW, &lH);
		if(lW <= 0 || lH <= 0) return false;

		if(lW == mvScreenSize.x && lH == mvScreenSize.y) return false;

		mvScreenSize = cVector2l(lW, lH);
		return true;
#else
		return false;
#endif
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
		;

		GLbitfield bitmask=0;

		if(aFlags & eClearFrameBufferFlag_Color)	bitmask |= GL_COLOR_BUFFER_BIT;
		if(aFlags & eClearFrameBufferFlag_Depth)	bitmask |= GL_DEPTH_BUFFER_BIT;
		if(aFlags & eClearFrameBufferFlag_Stencil)	bitmask |= GL_STENCIL_BUFFER_BIT;

		glClear(bitmask);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetClearColor(const cColor& aCol){
		;

		glClearColor(aCol.r, aCol.g, aCol.b, aCol.a);
	}
	void cLowLevelGraphicsSDL::SetClearDepth(float afDepth){
		;

		glClearDepth(afDepth);
	}
	void cLowLevelGraphicsSDL::SetClearStencil(int alVal){
		;

		glClearStencil(alVal);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::CopyFrameBufferToTexure(iTexture* apTex, const cVector2l &avPos,
													const cVector2l &avSize, const cVector2l &avTexOffset)
	{
		if(apTex==NULL)return;

		int vRect[6];
		ohpl_gl_copy_to_texture(OhplGl(mvFrameBufferPos), OhplGl(mvFrameBufferSize), OhplGl(mvFrameBufferTotalSize), OhplGl(avPos), OhplGl(avSize), OhplGl(avTexOffset), apTex->GetHeight(), vRect);

		SetTexture(0, apTex);
		glCopyTexSubImage2D(GetGLTextureTargetEnum(apTex->GetType()),0,	vRect[0], vRect[1],
							vRect[2], vRect[3], vRect[4], vRect[5]);
	}

	//-----------------------------------------------------------------------

	cBitmap* cLowLevelGraphicsSDL::CopyFrameBufferToBitmap(	const cVector2l &avScreenPos,const cVector2l &avScreenSize)
	{
		;

		int vRect[4];
		ohpl_gl_read_rect(OhplGl(mvFrameBufferPos), OhplGl(mvFrameBufferSize), OhplGl(mvFrameBufferTotalSize), OhplGl(avScreenPos), OhplGl(avScreenSize), vRect);
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
		glReadPixels(vPos.x, vPos.y, vSize.x, vSize.y,PixelFormatToGLFormat(pixelFormat), GL_UNSIGNED_BYTE, pBitmap->GetData(0,0)->mpData);

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

			if(mpFrameBuffer)
			{
				cFrameBufferGL *pFrameBufferGL = static_cast<cFrameBufferGL*>(mpFrameBuffer);

				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,pFrameBufferGL->GetHandle());
			}
			else
			{
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
			}

			// Post tasks like generating mipmaps
			if(pPrevFameBuffer)
			{
				pPrevFameBuffer->PostBindUpdate();
			}
		}

		/////////////////////////////////////
		//Get framebuffer size and set viewport
		OhplGlViewport vp;
		ohpl_gl_viewport(OhplGl(mpFrameBuffer ? mpFrameBuffer->GetSize() : mvScreenSize), OhplGl(avPos), OhplGl(avSize), &vp);
		mvFrameBufferTotalSize = vp.total;
		mvFrameBufferSize = vp.size;
		mvFrameBufferPos = vp.pos;
		glViewport(vp.gl[0], vp.gl[1], vp.gl[2], vp.gl[3]);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetFrameBufferDrawTargets(int *apTargets, int alNumOfTargets)
	{
		unsigned int vAttachments[64];
		ohpl_gl_draw_targets(apTargets, alNumOfTargets > 0 ? (size_t)alNumOfTargets : 0, vAttachments, 64);
		int lNum = alNumOfTargets > 64 ? 64 : alNumOfTargets;
		glDrawBuffers(lNum, lNum > 0 ? vAttachments : NULL);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::WaitAndFinishRendering()
	{
		;

		glFinish();
		//dont use this any more, SwapBuffers() takes care of it
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::FlushRendering()
	{
		;

		glFlush();
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::ApplyShaderGamma()
	{
		if(mlGammaProgram == 0)
		{
			const char *pVtx = "#version 120\nvarying vec2 uv;\nvoid main(){ uv = gl_Vertex.xy*0.5+0.5; gl_Position = gl_Vertex; }\n";
			const char *pFrag = "#version 120\nuniform sampler2D tex; uniform float invGamma; varying vec2 uv;\n"
								"void main(){ vec4 c = texture2D(tex, uv); gl_FragColor = vec4(pow(c.rgb, vec3(invGamma)), c.a); }\n";
			GLuint lVtx = glCreateShader(GL_VERTEX_SHADER), lFrag = glCreateShader(GL_FRAGMENT_SHADER);
			glShaderSource(lVtx, 1, &pVtx, NULL); glCompileShader(lVtx);
			glShaderSource(lFrag, 1, &pFrag, NULL); glCompileShader(lFrag);
			mlGammaProgram = glCreateProgram();
			glAttachShader(mlGammaProgram, lVtx); glAttachShader(mlGammaProgram, lFrag);
			glLinkProgram(mlGammaProgram);
			glDeleteShader(lVtx); glDeleteShader(lFrag);
			glGenTextures(1, &mlGammaTexture);
		}

		GLint lPrevProgram, lPrevFrameBuffer, lPrevActiveTex, lPrevTex, lPrevArrayBuffer;
		glGetIntegerv(GL_CURRENT_PROGRAM, &lPrevProgram);
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &lPrevFrameBuffer);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &lPrevActiveTex);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &lPrevArrayBuffer);
		glActiveTexture(GL_TEXTURE0);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &lPrevTex);
		glPushAttrib(GL_ALL_ATTRIB_BITS);

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, mlGammaTexture);
		if(mvGammaTextureSize != mvScreenSize)
		{
			mvGammaTextureSize = mvScreenSize;
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mvScreenSize.x, mvScreenSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, mvScreenSize.x, mvScreenSize.y);

		glViewport(0, 0, mvScreenSize.x, mvScreenSize.y);
		glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
		glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_ALPHA_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glUseProgram(mlGammaProgram);
		glUniform1i(glGetUniformLocation(mlGammaProgram, "tex"), 0);
		glUniform1f(glGetUniformLocation(mlGammaProgram, "invGamma"), 1.0f / mfGammaCorrection);
		glBegin(GL_TRIANGLE_STRIP);
		glVertex2f(-1, -1); glVertex2f(1, -1); glVertex2f(-1, 1); glVertex2f(1, 1);
		glEnd();

		glPopAttrib();
		glUseProgram(lPrevProgram);
		glBindTexture(GL_TEXTURE_2D, lPrevTex);
		glActiveTexture(lPrevActiveTex);
		glBindBuffer(GL_ARRAY_BUFFER, lPrevArrayBuffer);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, lPrevFrameBuffer);
	}

	void cLowLevelGraphicsSDL::SwapBuffers()
	{
		;
		if(cMath::Abs(mfGammaCorrection - 1.0f) > 0.001f && mfGammaCorrection > 0)
			ApplyShaderGamma();
#if SDL_VERSION_ATLEAST(2, 0, 0)
        SDL_GL_SwapWindow(mpScreen);
#else
		SDL_GL_SwapBuffers();
#endif
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// RENDER STATE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetColorWriteActive(bool abR,bool abG,bool abB,bool abA)
	{
		if(ohpl_gl_state_color_write(&mGl, abR, abG, abB, abA)) glColorMask(abR,abG,abB,abA);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetDepthWriteActive(bool abX)
	{
		if(ohpl_gl_state_depth_write(&mGl, abX)) glDepthMask(abX);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetDepthTestActive(bool abX)
	{
		if(!ohpl_gl_state_depth_test(&mGl, abX)) return;
		if(abX) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetDepthTestFunc(eDepthTestFunc aFunc)
	{
		if(ohpl_gl_state_depth_func(&mGl, aFunc)) glDepthFunc(GetGLDepthTestFuncEnum(aFunc));
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetAlphaTestActive(bool abX)
	{
		if(!ohpl_gl_state_alpha_test(&mGl, abX)) return;
		if(abX) glEnable(GL_ALPHA_TEST);
		else glDisable(GL_ALPHA_TEST);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetAlphaTestFunc(eAlphaTestFunc aFunc,float afRef)
	{
		if(ohpl_gl_state_alpha_func(&mGl, aFunc, afRef)) glAlphaFunc(GetGLAlphaTestFuncEnum(aFunc),afRef);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencilActive(bool abX)
	{
		;

		if(abX)
		{
			//DO this check, so you can setup stencil and then turn on / off afterwards
			//and separate will still remain.
			if(mbDoubleSidedStencilIsSet && GLEW_EXT_stencil_two_side)
			{
				glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
			}
			glEnable(GL_STENCIL_TEST);
		}
		else
		{
			glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
			glDisable(GL_STENCIL_TEST);
		}
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencilWriteMask(unsigned int alMask)
	{
		;

		glStencilMask(alMask);
	}
	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencil(eStencilFunc aFunc,int alRef, unsigned int aMask,
		eStencilOp aFailOp,eStencilOp aZFailOp,eStencilOp aZPassOp)
	{
		;

		mbDoubleSidedStencilIsSet = false;
		if(GLEW_EXT_stencil_two_side)
		{
			glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
			glActiveStencilFaceEXT(GL_FRONT);
		}
		glStencilFunc(GetGLStencilFuncEnum(aFunc), alRef, aMask);

		glStencilOp(GetGLStencilOpEnum(aFailOp), GetGLStencilOpEnum(aZFailOp),
			GetGLStencilOpEnum(aZPassOp));
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetStencilTwoSide(eStencilFunc aFrontFunc,eStencilFunc aBackFunc,
		int alRef, unsigned int aMask,
		eStencilOp aFrontFailOp,eStencilOp aFrontZFailOp,eStencilOp aFrontZPassOp,
		eStencilOp aBackFailOp,eStencilOp aBackZFailOp,eStencilOp aBackZPassOp)
	{
		;

		mbDoubleSidedStencilIsSet = true;

		//Nvidia implementation
		if(GLEW_EXT_stencil_two_side)
		{
			glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);

			//Front
			glActiveStencilFaceEXT(GL_FRONT);
			glStencilFunc(GetGLStencilFuncEnum(aFrontFunc), alRef, aMask);

			glStencilOp(GetGLStencilOpEnum(aFrontFailOp), GetGLStencilOpEnum(aFrontZFailOp),
				GetGLStencilOpEnum(aFrontZPassOp));
			//Back
			glActiveStencilFaceEXT(GL_BACK);
			glStencilFunc(GetGLStencilFuncEnum(aBackFunc), alRef, aMask);

			glStencilOp(GetGLStencilOpEnum(aBackFailOp), GetGLStencilOpEnum(aBackZFailOp),
				GetGLStencilOpEnum(aBackZPassOp));
		}
		//Ati implementation
		else if(GLEW_ATI_separate_stencil)
		{
			//Front
			glStencilOpSeparateATI( GL_FRONT, GetGLStencilOpEnum(aFrontFailOp), 
				GetGLStencilOpEnum(aFrontZFailOp), 
				GetGLStencilOpEnum(aFrontZPassOp));
			//Back
			glStencilOpSeparateATI( GL_BACK, GetGLStencilOpEnum(aBackFailOp), 
				GetGLStencilOpEnum(aBackZFailOp), 
				GetGLStencilOpEnum(aBackZPassOp));

			//Front and Back function
			glStencilFuncSeparateATI(GetGLStencilFuncEnum(aFrontFunc),
				GetGLStencilFuncEnum(aBackFunc), 
				alRef, aMask);
		}
		else
		{
			FatalError("Only single sided stencil supported!\n");
		}
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetCullActive(bool abX)
	{
		ohpl_gl_state_cull_active(&mGl, abX);
		if(abX) glEnable(GL_CULL_FACE);
		else	glDisable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}
	void cLowLevelGraphicsSDL::SetCullMode(eCullMode aMode)
	{
		ohpl_gl_state_cull_mode(&mGl, aMode);
		glCullFace(GL_BACK);
		if(aMode == eCullMode_Clockwise)	glFrontFace(GL_CCW);
		else								glFrontFace(GL_CW);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetScissorActive(bool abX)
	{
		if(!ohpl_gl_state_scissor(&mGl, abX)) return;
		if(abX) glEnable(GL_SCISSOR_TEST);
		else glDisable(GL_SCISSOR_TEST);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetScissorRect(const cVector2l& avPos, const cVector2l& avSize)
	{
		int vRect[4];
		ohpl_gl_scissor_rect(mpFrameBuffer ? mpFrameBuffer->GetSize().y : mvScreenSize.y, OhplGl(avPos), OhplGl(avSize), vRect);
		glScissor(vRect[0], vRect[1], vRect[2], vRect[3]);
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetClipPlane(int alIdx, const cPlanef& aPlane)
	{
		;

		mvClipPlanes[alIdx] = aPlane;

		double vPlane[4];
		vPlane[0] = aPlane.a;
		vPlane[1] = aPlane.b;
		vPlane[2] = aPlane.c;
		vPlane[3] = aPlane.d;
		glClipPlane(GL_CLIP_PLANE0 + alIdx,vPlane);
	}
	cPlanef cLowLevelGraphicsSDL::GetClipPlane(int alIdx)
	{
		;

		return mvClipPlanes[alIdx];
	}
	void cLowLevelGraphicsSDL::SetClipPlaneActive(int alIdx, bool abX)
	{
		;

		if(abX) glEnable(GL_CLIP_PLANE0 + alIdx);
		else	glDisable(GL_CLIP_PLANE0 + alIdx);
	}


	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetBlendActive(bool abX)
	{
		if(!ohpl_gl_state_blend(&mGl, abX)) return;
		if(abX)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetBlendFunc(eBlendFunc aSrcFactor, eBlendFunc aDestFactor)
	{
		;

		glBlendFunc(GetGLBlendEnum(aSrcFactor),GetGLBlendEnum(aDestFactor));
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetBlendFuncSeparate(eBlendFunc aSrcFactorColor, eBlendFunc aDestFactorColor,
		eBlendFunc aSrcFactorAlpha, eBlendFunc aDestFactorAlpha)
	{
		;

		if(GLEW_EXT_blend_func_separate)
		{
			glBlendFuncSeparateEXT(GetGLBlendEnum(aSrcFactorColor),
				GetGLBlendEnum(aDestFactorColor),
				GetGLBlendEnum(aSrcFactorAlpha),
				GetGLBlendEnum(aDestFactorAlpha));
		}
		else
		{
			glBlendFunc(GetGLBlendEnum(aSrcFactorColor),GetGLBlendEnum(aDestFactorColor));
		}
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetDepthClampActive(bool abX)
	{
		if(abX)	glEnable(GL_DEPTH_CLAMP);
		else	glDisable(GL_DEPTH_CLAMP);
	}

	void cLowLevelGraphicsSDL::SetPolygonOffsetActive(bool abX)
	{
		;

		if(abX)	glEnable(GL_POLYGON_OFFSET_FILL);
		else	glDisable(GL_POLYGON_OFFSET_FILL);

	}

	void cLowLevelGraphicsSDL::SetPolygonOffset(float afBias, float afSlopeScaleBias)
	{
		;

		glPolygonOffset(afSlopeScaleBias, afBias);
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// MATRIX
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::PushMatrix(eMatrix aMtxType)
	{
		;

		SetMatrixMode(aMtxType);
		glPushMatrix();
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::PopMatrix(eMatrix aMtxType)
	{
		;

		SetMatrixMode(aMtxType);
		glPopMatrix();
	}
	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetMatrix(eMatrix aMtxType, const cMatrixf& a_mtxA)
	{
		;

		SetMatrixMode(aMtxType);
		glLoadIdentity();
		cMatrixf mtxTranpose = a_mtxA.GetTranspose();
		glLoadMatrixf(mtxTranpose.v);
	}

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetIdentityMatrix(eMatrix aMtxType)
	{
		;

		SetMatrixMode(aMtxType);
		glLoadIdentity();
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetOrthoProjection(const cVector2f& avSize, float afMin, float afMax)
	{
		;

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,avSize.x,avSize.y,0,afMin,afMax);
	}

	void cLowLevelGraphicsSDL::SetOrthoProjection(const cVector3f& avMin, const cVector3f& avMax)
	{
		;

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(avMin.x,avMax.x,avMax.y,avMin.y,avMin.z,avMax.z);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// TEXTURE OPERATIONS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	void cLowLevelGraphicsSDL::SetTexture(unsigned int alUnit,iTexture* apTex)
	{
		GLenum NewTarget=0;
		if(apTex)	NewTarget = GetGLTextureTargetEnum(apTex->GetType());

		OhplGlTexPlan plan;
		ohpl_gl_state_set_texture(&mGl, alUnit, NewTarget, apTex != NULL, GLEW_ARB_multitexture, &plan);

		if(plan.activeUnit >= 0) glActiveTextureARB(GL_TEXTURE0_ARB + plan.activeUnit);
		if(plan.disable) glDisable(plan.disable);
		if(plan.bind) glBindTexture(NewTarget, static_cast<cSDLTexture*>(apTex)->GetTextureHandle());
		if(plan.enable) glEnable(plan.enable);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetActiveTextureUnit(unsigned int alUnit)
	{
		if(!ohpl_gl_state_active_unit(&mGl, alUnit)) return;
		glActiveTextureARB(GL_TEXTURE0_ARB + alUnit);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetTextureEnv(eTextureParam aParam, int alVal)
	{
		;

		GLenum lParam = GetGLTextureParamEnum(aParam);

		glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE_ARB);

		if(aParam==eTextureParam_ColorFunc || aParam==eTextureParam_AlphaFunc){
			glTexEnvi(GL_TEXTURE_ENV,lParam,GetGLTextureFuncEnum((eTextureFunc)alVal));
		}
		else if(aParam>=eTextureParam_ColorSource0 && aParam<=eTextureParam_AlphaSource2){
			glTexEnvi(GL_TEXTURE_ENV,lParam,GetGLTextureSourceEnum((eTextureSource)alVal));
		}
		else if(aParam>=eTextureParam_ColorOp0 && aParam<=eTextureParam_AlphaOp2){
			glTexEnvi(GL_TEXTURE_ENV,lParam,GetGLTextureOpEnum((eTextureOp)alVal));
		}
		else {
			glTexEnvi(GL_TEXTURE_ENV,lParam,alVal);
		}
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetTextureConstantColor(const cColor &aColor)
	{
		;

		float vColor[4] = {	aColor.r, aColor.g, aColor.b, aColor.a	};

		glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, &vColor[0]);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetColor(const cColor &aColor)
	{
		;

		glColor4f(aColor.r, aColor.g, aColor.b, aColor.a);
	}

	//-----------------------------------------------------------------------



	//////////////////////////////////////////////////////////////////////////
	// DRAWING
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

#define IM_CLASS cLowLevelGraphicsSDL
#define IM_BEGIN glBegin
#define IM_END glEnd
#define IM_VERTEX3 glVertex3f
#define IM_COLOR4 glColor4f
#define IM_TEXCOORD2 glTexCoord2f
#define IM_TEXCOORD3 glTexCoord3f
#define IM_MULTITEXCOORD2 glMultiTexCoord2fARB
#define IM_MULTITEXCOORD3 glMultiTexCoord3fARB
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
		OhplGlOp vOps[kOhplGlMaxOps];
		size_t lCount = ohpl_gl_batch_texture_unit(alUnit, abActive, sizeof(float)*mlBatchStride, vOps);
		OhplGlOpSources src = {NULL, mpVertexArray, mpTexCoordArray};
		RunGlOps(vOps, lCount, src);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::FlushTriBatch(tVtxBatchFlag aTypeFlags, bool abAutoClear)
	{
		;

		SetVtxBatchStates(aTypeFlags);
		SetUpBatchArrays();

		glDrawElements(GL_TRIANGLES,mlIndexCount,GL_UNSIGNED_INT, mpIndexArray);

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
		;

		SetVtxBatchStates(aTypeFlags);
		SetUpBatchArrays();

		glDrawElements(GL_QUADS,mlIndexCount,GL_UNSIGNED_INT, mpIndexArray);

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

	void cLowLevelGraphicsSDL::SetUpBatchArrays()
	{
		OhplGlOp vOps[kOhplGlMaxOps];
		size_t lCount = ohpl_gl_batch_arrays(sizeof(float)*mlBatchStride, vOps);
		OhplGlOpSources src = {NULL, mpVertexArray, mpTexCoordArray};
		RunGlOps(vOps, lCount, src);
	}

	//-----------------------------------------------------------------------

	void cLowLevelGraphicsSDL::SetVtxBatchStates(tVtxBatchFlag aFlags)
	{
		OhplGlOp vOps[kOhplGlMaxOps];
		size_t lCount = ohpl_gl_batch_states(aFlags, vOps);
		OhplGlOpSources src = {NULL, mpVertexArray, mpTexCoordArray};
		RunGlOps(vOps, lCount, src);
	}
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

	void cLowLevelGraphicsSDL::SetMatrixMode(eMatrix mType)
	{
		;

		switch(mType)
		{
		case eMatrix_ModelView: glMatrixMode(GL_MODELVIEW);break;
		case eMatrix_Projection: glMatrixMode(GL_PROJECTION); break;
		case eMatrix_Texture: glMatrixMode(GL_TEXTURE); break;
		}
	}

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
