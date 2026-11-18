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

#ifndef HPL_PARTICLE_SYSTEM_MANAGER_H
#define HPL_PARTICLE_SYSTEM_MANAGER_H

#include "resources/ResourceManager.h"
#include "resources/Resources.h"
#include "resources/RustResMgr.h"
#include "scene/ParticleSystem.h"

#include "math/MathTypes.h"

namespace hpl {

	class cGraphics;
	class cXmlElement;

	class cParticleManager : public iResourceManager
	{
	public:
		cParticleManager(cGraphics* apGraphics,cResources *apResources)
			: iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(), apResources->GetLowLevelSystem()),
			  mpGraphics(apGraphics), mpResources(apResources) {}
		~cParticleManager(){ ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_PARTICLE); }

		cParticleSystem* CreatePS(const tString& asName,const tString& asType, cVector3f avSize)
		{
			return static_cast<cParticleSystem*>(ohpl_h3f_particle_by_type(mpRs, H3Ops(), RM_STR(asName), RM_STR(asType), avSize.x, avSize.y, avSize.z));
		}

		cParticleSystem* CreatePS(const tString& asName, const tString& asDataName, cXmlElement* apElement, cVector3f avSize)
		{
			return static_cast<cParticleSystem*>(ohpl_h3f_particle_by_element(mpRs, H3Ops(), RM_STR(asName), RM_STR(asDataName), apElement, avSize.x, avSize.y, avSize.z));
		}

		void AddData(cParticleSystemData *apData){ ohpl_h3f_particle_add_data(mpRs, H3Ops(), static_cast<iResourceBase*>(apData)); }
		void Preload(const tString& asFile){ ohpl_h3f_particle_preload(mpRs, H3Ops(), RM_STR(asFile)); }
		void Destroy(iResourceBase* apResource){ ohpl_h3f_particle_destroy(mpRs, H3Ops(), apResource); }
		void Unload(iResourceBase* apResource){}
	protected:
		void* H3Create(OhplH3Create &aCreate) override
		{
			if(aCreate.kind == OHPL_H3_PS_CREATE)
			{
				cVector3f vSize(aCreate.f[0], aCreate.f[1], aCreate.f[2]);
				cParticleSystemData *pData = H3Res<cParticleSystemData>(aCreate.ptr);
				cParticleSystem *pPS = pData->Create(H3Name(aCreate.name), vSize);
				pPS->SetDataName(H3Name(aCreate.name2));
				pPS->SetDataSize(vSize);
				pPS->SetParticleManager(this);
				return pPS;
			}
			cParticleSystemData *pPSData = hplNew( cParticleSystemData, (H3Name(aCreate.name), mpResources, mpGraphics) );
			bool bOk = aCreate.kind == OHPL_H3_PS_DATA_FILE ? pPSData->LoadFromFile(H3Wide(aCreate.path)) : pPSData->LoadFromElement(static_cast<cXmlElement*>(aCreate.ptr));
			if(bOk==false)
			{
				hplDelete(pPSData);
				return NULL;
			}
			return static_cast<iResourceBase*>(pPSData);
		}
	private:
		cGraphics* mpGraphics;
		cResources *mpResources;
	};

};
#endif // HPL_PARTICLE_SYSTEM_MANAGER_H
