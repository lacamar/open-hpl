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

#ifndef LUX_MAP_HANDLER_H
#define LUX_MAP_HANDLER_H

#include "LuxBase.h"
#include "RustLuxMap.h"
#include "RustLuxMapA7.h"
#include "RustLuxMapA13.h"

class cLuxMap;
class cLuxSavedGameMapCollection;
class cLuxModelCache;

typedef std::list<cLuxMap*> tLuxMapList;

class cMapHandlerSoundCallback : public iSoundEntityGlobalCallback
{
public:
	cMapHandlerSoundCallback(OhplA13H *apHandler);
	virtual ~cMapHandlerSoundCallback(){}

	void OnStart(cSoundEntity *apSoundEntity);

private:
	OhplA13H *mpHandler;
};

class cLuxDebugRenderCallback : public iRendererCallback
{
public:
	cLuxDebugRenderCallback();

	void OnPostSolidDraw(cRendererCallbackFunctions* apFunctions);
	void OnPostTranslucentDraw(cRendererCallbackFunctions* apFunctions);

	iPhysicsWorld* mpPhysicsWorld;
	iLowLevelGraphics* mpLowLevelGfx;
};

// Shell: the map list, the current map and the map change live in rust/src/luxmap/mhandler.rs; this class
// keeps the engine objects (viewport, post effects, save collection) the host ops reach.
class cLuxMapHandler : public iLuxUpdateable
{
friend class cMapHandlerSoundCallback;
public:
	cLuxMapHandler();
	~cLuxMapHandler();

	void OnStart();
	void Update(float afTimeStep){ ohpl_a13_handler_update(mpR, afTimeStep);}
	void Reset(){ ohpl_a13_handler_reset(mpR);}
    void OnQuit(){ ohpl_a13_handler_on_quit(mpR);}

	void LoadUserConfig();
	void SaveUserConfig();

	void CreateDataCache();
	void DestroyDataCache();

	void UpdateViewportRenderProperties();

	void SetUpdateActive(bool abX){ ohpl_a13_handler_set_update_active(mpR, abX);}

	void RenderSolid(cRendererCallbackFunctions* apFunctions){ ohpl_a13_handler_render_solid(mpR, apFunctions);}

	void OnEnterContainer(const tString& asOldContainer){ ohpl_a13_handler_container(mpR, true);}
	void OnLeaveContainer(const tString& asNewContainer){ ohpl_a13_handler_container(mpR, false);}

	void ChangeMap(const tString& asMapName, const tString& asStartPos, const tString& asStartSound, const tString& asEndSound)
	{
		ohpl_a13_handler_change_map(mpR, OHPL_A7_S(asMapName), OHPL_A7_S(asStartPos), OHPL_A7_S(asStartSound), OHPL_A7_S(asEndSound));
	}

	bool MapIsLoaded(){ return mpCurrentMap != NULL;}

	cLuxMap* LoadMap(const tString& asName, bool abLoadEntities){ return static_cast<cLuxMap*>(ohpl_a13_handler_load_map(mpR, OHPL_A7_S(asName), abLoadEntities));}
	void DestroyMap(cLuxMap* apMap, bool abRunScript){ ohpl_a13_handler_destroy_map(mpR, apMap, abRunScript);}
	void SetCurrentMap(cLuxMap* apMap, bool abRunScript, bool abFirstTime, const tString& asPlayerPos)
	{
		ohpl_a13_handler_set_current(mpR, apMap, abRunScript, abFirstTime, OHPL_A7_S(asPlayerPos));
	}
	cLuxMap* GetCurrentMap(){ return mpCurrentMap;}

	cViewport* GetViewport(){ return mpViewport;}

	const tString& GetMapFolder(){ return msMapFolder;}
	void SetMapFolder(const tString& asFolder){ msMapFolder = asFolder;}

	void PauseSoundsAndMusic(){ ohpl_a13_handler_pause(mpR, true);}
	void ResumeSoundsAndMusic(){ ohpl_a13_handler_pause(mpR, false);}

	iPostEffect *GetPostEffect_Bloom(){ return mpPostEffect_Bloom;}
	iPostEffect *GetPostEffect_ImageTrail(){ return mpPostEffect_ImageTrail;}
	iPostEffect *GetPostEffect_Sepia(){ return mpPostEffect_Sepia;}
	iPostEffect *GetPostEffect_RadialBlur(){ return mpPostEffect_RadialBlur;}

	void ClearSaveMapCollection();
	cLuxSavedGameMapCollection *GetSavedMapCollection(){ return mpSavedGame;}
	void SetSavedMapCollection(cLuxSavedGameMapCollection *apMaps);

	tString FileToMapName(const tString& asFile)
	{
		tString sName;
		ohpl_ga5_map_name(asFile.data(), asFile.size(), ohpl_ga5::Collect, &sName);
		return sName;
	}

	void SetShowCommentary(bool abX){ ohpl_a13_handler_set_show_commentary(mpR, abX, true);}
	bool GetShowCommentary(){ return ohpl_a13_handler_show_commentary(mpR);}

	void AppLostInputFocus(){ PauseSoundsAndMusic();}
	void AppGotInputFocus(){ ResumeSoundsAndMusic();}

	// Host ops (LuxMapHost.cpp): the state the Rust handler mirrors into the shell.
	void HostSetCurrent(cLuxMap *apMap){ mpCurrentMap = apMap;}
	void HostSetView(cWorld *apWorld, iPhysicsWorld *apPhysicsWorld);

	//////////////////////////////////
	// Used to lock the SavedMapCollection
    iMutex *mpSavedGameMutex;

private:
	void LoadMainConfig();
	void SaveMainConfig();

	cLuxDebugRenderCallback mRenderCallback;

	tString msMapFolder;

	cLuxModelCache *mpDataCache;

	cLuxMap* mpCurrentMap;
	OhplA13H *mpR;

	cViewport *mpViewport;

	cMapHandlerSoundCallback* mpSoundCallback;

	iPostEffect *mpPostEffect_Bloom;
	iPostEffect *mpPostEffect_ImageTrail;
	iPostEffect *mpPostEffect_Sepia;
	iPostEffect *mpPostEffect_RadialBlur;

	cLuxSavedGameMapCollection *mpSavedGame;
};

#endif // LUX_MAP_HANDLER_H
