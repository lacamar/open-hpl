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

#ifndef HPL_SCENE_H
#define HPL_SCENE_H

#include <list>
#include "system/SystemTypes.h"
#include "engine/EngineTypes.h"
#include "engine/Updateable.h"
#include "scene/Camera.h"
#include "scene/SceneTypes.h"
#include "scene/RustScn.h"
#include "resources/MeshLoader.h"

namespace hpl {

	class cAI;
	class cGraphics;
	class cResources;
	class cSystem;
	class cSound;
	class cPhysics;
	class cGui;
	class cCamera;
	class cFrustum;
	class cUpdater;
	class cWorld;
	class cViewport;

	//--------------------------------------------------------------------

	typedef cOhplPtrView<cViewport> tViewportList;

	//--------------------------------------------------------------------

	#define tSceneRenderFlag_World			0x00000001
	#define tSceneRenderFlag_Gui			0x00000002
	#define tSceneRenderFlag_PostEffects	0x00000004
	#define tSceneRenderFlag_All			0xFFFFFFFF

	//--------------------------------------------------------------------

	// State and logic live in rust/src/scene/scn.rs.
	class cScene : public iUpdateable
	{
	public:
		cScene(cGraphics *apGraphics,cResources *apResources, cSound* apSound, cPhysics *apPhysics,
				cSystem *apSystem, cAI *apAI,cGui *apGui)
			: iUpdateable("HPL_Scene"), mpS(ohpl_scn_new(this, apGraphics, apResources, apSound, apPhysics, apSystem, apAI, apGui)){}
		~cScene(){ ohpl_scn_free(mpS);}

		void Reset(){}

		/**
		 * Called by cEngine
		 */
		void Render(float afFrameTime, tFlag alFlags){ ohpl_scn_render(mpS, afFrameTime, alFlags);}
		void PostUpdate(float afTimeStep){ ohpl_scn_post_update(mpS, afTimeStep);}

		///// VIEW PORT METHODS ////////////////////

		cViewport* CreateViewport(cCamera *apCamera=NULL, cWorld *apWorld=NULL, bool abPushFront = false)
		{
			return ohpl_scn_create_viewport(mpS, apCamera, apWorld, abPushFront);
		}
		void DestroyViewport(cViewport* apViewPort){ ohpl_scn_destroy_viewport(mpS, apViewPort);}
		bool ViewportExists(cViewport* apViewPort){ return ohpl_scn_viewport_exists(mpS, apViewPort);}
		void SetCurrentListener(cViewport* apViewPort){ ohpl_scn_set_current_listener(mpS, apViewPort);}
		const tViewportList& GetViewportList(){ return *static_cast<const tViewportList*>(ohpl_scn_viewports(mpS));}

		///// CAMERA METHODS ////////////////////

		cCamera* CreateCamera(eCameraMoveMode aMoveMode){ return static_cast<cCamera*>(ohpl_scn_create_camera(mpS));}
		void DestroyCamera(cCamera* apCam){ ohpl_scn_destroy_camera(mpS, apCam);}

		///// WORLD METHODS ////////////////////

		cWorld* LoadWorld(const tString& asFile, tWorldLoadFlag aFlags)
		{
			return static_cast<cWorld*>(ohpl_scn_load_world(mpS, asFile.data(), asFile.size(), aFlags));
		}
		cWorld* CreateWorld(const tString& asName){ return static_cast<cWorld*>(ohpl_scn_create_world(mpS, asName.data(), asName.size()));}
		void DestroyWorld(cWorld* apWorld){ ohpl_scn_destroy_world(mpS, apWorld);}
		bool WorldExists(cWorld* apWorld){ return ohpl_scn_world_exists(mpS, apWorld);}

	private:
		OhplScn *mpS;
	};

};
#endif // HPL_SCENE_H
