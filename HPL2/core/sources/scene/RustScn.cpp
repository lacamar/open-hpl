#include "scene/Scene.h"
#include "scene/Viewport.h"
#include "scene/Camera.h"
#include "scene/World.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "resources/Resources.h"
#include "resources/FileSearcher.h"
#include "resources/WorldLoaderHandler.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/LowLevelGraphics.h"
#include "sound/Sound.h"
#include "sound/LowLevelSound.h"
#include "gui/GuiSet.h"

namespace hpl {

namespace {

cGraphics* G(void *p){ return static_cast<cGraphics*>(p);}

void* LoadWorld(void *apResources, const char *asFile, size_t alLen, uint32_t alFlags)
{
	cResources *pResources = static_cast<cResources*>(apResources);
	const tString sFile(asFile, alLen);

	tWString asPath = pResources->GetFileSearcher()->GetFilePath(sFile);
	if(asPath == _W(""))
	{
		if(cResources::GetCreateAndLoadCompressedMaps())
			asPath = pResources->GetFileSearcher()->GetFilePath(cString::SetFileExt(sFile,"cmap"));
		if(asPath == _W(""))
		{
			Error("World '%s' doesn't exist\n",sFile.c_str());
			return NULL;
		}
	}

	cWorld* pWorld = pResources->GetWorldLoaderHandler()->LoadWorld(asPath, alFlags);
	if(pWorld==NULL){
		Error("Couldn't load world from '%s'\n",cString::To8Char(asPath).c_str());
	}
	return pWorld;
}

void* CameraNew(void *apGraphics)
{
	cCamera *pCamera = hplNew( cCamera, () );
	iLowLevelGraphics *pLow = G(apGraphics)->GetLowLevel();
	pCamera->SetAspect(pLow->GetScreenSizeFloat().x / pLow->GetScreenSizeFloat().y);
	return pCamera;
}

void* WorldNew(const OhplScnWorldArgs *apA, const char *asName, size_t alLen)
{
	return hplNew( cWorld, (tString(asName, alLen), G(apA->graphics), static_cast<cResources*>(apA->resources),
							static_cast<cSound*>(apA->sound), static_cast<cPhysics*>(apA->physics), static_cast<cScene*>(apA->scene),
							static_cast<cSystem*>(apA->system), static_cast<cAI*>(apA->ai)) );
}

void RendererRender(void *apR, float afFrameTime, void *apFrustum, void *apWorld, void *apSettings, void *apTarget,
					bool abPost, void *apCallbacks)
{
	static_cast<iRenderer*>(apR)->Render(afFrameTime, static_cast<cFrustum*>(apFrustum), static_cast<cWorld*>(apWorld),
										static_cast<cRenderSettings*>(apSettings), static_cast<cRenderTarget*>(apTarget),
										abPost, static_cast<tRendererCallbackList*>(apCallbacks));
}

void SetFrameBuffer(void *apGraphics, void *apFb, const int *apPos, const int *apSize)
{
	G(apGraphics)->GetLowLevel()->SetCurrentFrameBuffer(static_cast<iFrameBuffer*>(apFb),
														cVector2l(apPos[0], apPos[1]), cVector2l(apSize[0], apSize[1]));
}

void SetListener(void *apSound, const cVector3f *apPos, const cVector3f *apFwd, const cVector3f *apUp)
{
	static_cast<cSound*>(apSound)->GetLowLevel()->SetListenerAttributes(*apPos, cVector3f(0,0,0), *apFwd, *apUp);
}

void LogMsg(bool abError, const char *asMsg, size_t alLen)
{
	if(abError) Error("%.*s", (int)alLen, asMsg);
	else Log("%.*s", (int)alLen, asMsg);
}

}

void OhplScnRegisterHost()
{
	{
		static const OhplScnHost h = {
			[]() -> void* { return hplNew( cRenderSettings, () ); },
			[](void *p){ hplDelete( static_cast<cRenderSettings*>(p) ); },
			[](void *p){ static_cast<cRenderSettings*>(p)->ResetVariables(); },
			CameraNew,
			[](void *p){ hplDelete( static_cast<cCamera*>(p) ); },
			WorldNew,
			[](void *p){ hplDelete( static_cast<cWorld*>(p) ); },
			LoadWorld,
			[](void *p) -> void* { return G(p)->GetRenderer(eRenderer_Main); },
			[](){ iRenderer::IncRenderFrameCount(); },
			RendererRender,
			[](void *p) -> void* { return static_cast<iRenderer*>(p)->GetPostEffectTexture(); },
			[](void *p){ return static_cast<cPostEffectComposite*>(p)->HasActiveEffects(); },
			[](void *p, float afTime, void *apFrustum, void *apTex, void *apTarget){
				static_cast<cPostEffectComposite*>(p)->Render(afTime, static_cast<cFrustum*>(apFrustum),
															static_cast<iTexture*>(apTex), static_cast<cRenderTarget*>(apTarget)); },
			SetFrameBuffer,
			[](void *p){ return static_cast<cGuiSet*>(p)->Is3D(); },
			[](void *p){ return (int)static_cast<cGuiSet*>(p)->GetDrawPriority(); },
			[](void *p, void *apFrustum){ static_cast<cGuiSet*>(p)->Render(static_cast<cFrustum*>(apFrustum)); },
			[](void *p, int alMsg){ static_cast<iViewportCallback*>(p)->RunMessage((eViewportMessage)alMsg); },
			SetListener,
			LogMsg };
		ohpl_scn_set_host(&h);
	}
}

}
