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

#ifndef HPL_ENGINE_FILE_LOADING_H
#define HPL_ENGINE_FILE_LOADING_H

#include "system/SystemTypes.h"
#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "resources/ResourcesTypes.h"
#include "scene/World.h"

namespace hpl {

	//----------------------------

	class cXmlElement;

	class iEntity3D;
	struct OhplWdPod;
	class iLight;
	class cBillboard;
	class cSoundEntity;
	class cParticleSystem;
	class cWorld;
	class cResources;
	class cFogArea;
	class cLensFlare;
	class cGraphics;
	class cMesh;

	//----------------------------

	extern "C" {
	void* ohpl_efl_load_fog_area(void*, const uint8_t*, size_t, OhplWdPod*, bool);
	void* ohpl_efl_load_particle_system(void*, const uint8_t*, size_t, OhplWdPod*);
	void* ohpl_efl_load_sound(void*, const uint8_t*, size_t, OhplWdPod*);
	void* ohpl_efl_load_billboard(void*, const uint8_t*, size_t, OhplWdPod*, bool, void (*)(void*, int32_t, const uint8_t*, size_t), void*);
	void* ohpl_efl_load_lens_flare(void*, const uint8_t*, size_t, OhplWdPod*, bool);
	void* ohpl_efl_load_light(void*, const uint8_t*, size_t, OhplWdPod*, bool);
	}

	class cEngineFileLoading
	{
	public:
		static cFogArea* LoadFogArea(cXmlElement* apElement, const tString& asNamePrefix, cWorld *apWorld, bool abStatic)
		{ return static_cast<cFogArea*>(ohpl_efl_load_fog_area(apElement, Pre(asNamePrefix), asNamePrefix.size(), apWorld->Rs(), abStatic)); }
		static cParticleSystem* LoadParticleSystem(cXmlElement* apElement, const tString& asNamePrefix, cWorld *apWorld)
		{ return static_cast<cParticleSystem*>(ohpl_efl_load_particle_system(apElement, Pre(asNamePrefix), asNamePrefix.size(), apWorld->Rs())); }
		static cSoundEntity* LoadSound(cXmlElement* apElement, const tString& asNamePrefix, cWorld *apWorld)
		{ return static_cast<cSoundEntity*>(ohpl_efl_load_sound(apElement, Pre(asNamePrefix), asNamePrefix.size(), apWorld->Rs())); }
		static cBillboard* LoadBillboard(	cXmlElement* apElement, const tString& asNamePrefix, cWorld *apWorld, cResources *apResources, bool abStatic,
											tEFL_LightBillboardConnectionList *apLightBillboardList=NULL)
		{ return static_cast<cBillboard*>(ohpl_efl_load_billboard(apElement, Pre(asNamePrefix), asNamePrefix.size(), apWorld->Rs(), abStatic, apLightBillboardList ? PushConnection : NULL, apLightBillboardList)); }
		static cLensFlare* LoadLensFlare(cXmlElement* apElement, const tString& asNamePrefix, cWorld *apWorld, cResources *apResources, bool abStatic)
		{ return static_cast<cLensFlare*>(ohpl_efl_load_lens_flare(apElement, Pre(asNamePrefix), asNamePrefix.size(), apWorld->Rs(), abStatic)); }
		static iLight* LoadLight(cXmlElement* apElement, const tString& asNamePrefix, cWorld *apWorld, cResources *apResources, bool abStatic)
		{ return static_cast<iLight*>(ohpl_efl_load_light(apElement, Pre(asNamePrefix), asNamePrefix.size(), apWorld->Rs(), abStatic)); }

		static cMesh* LoadDecalMeshHelper(cXmlElement* apElement, cGraphics* apGraphics, cResources* apResources, const tString& asName, const tString& asMaterial, const cColor& aColor);

	private:
		static const uint8_t* Pre(const tString& asPrefix){ return reinterpret_cast<const uint8_t*>(asPrefix.data()); }
		static void PushConnection(void *apList, int32_t alID, const uint8_t *apLight, size_t alLen)
		{
			cEFL_LightBillboardConnection connection;
			connection.msBillboardID = alID;
			connection.msLightName = tString(reinterpret_cast<const char*>(apLight), alLen);
			static_cast<tEFL_LightBillboardConnectionList*>(apList)->push_back(connection);
		}
	};
};
#endif // HPL_ENGINE_FILE_LOADING_H
