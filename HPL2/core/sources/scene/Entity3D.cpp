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

#include "scene/Entity3D.h"
#include "scene/Node3D.h"
#include "math/Math.h"
#include "scene/RustNode.h"
#include "scene/RustSceneH10.h"

#include "system/LowLevelSystem.h"

namespace hpl {

	struct OhplH10Hooks
	{
		static iEntity3D* E(uintptr_t alEnt){ return (iEntity3D*)alEnt;}

		static void EntOnUpdated(uintptr_t alEnt){ E(alEnt)->OnTransformUpdated();}
		static void EntCallbacks(uintptr_t alEnt)
		{
			iEntity3D *pEnt = E(alEnt);
			if(pEnt->mlstCallbacks.empty()) return;
			for(tEntityCallbackListIt it = pEnt->mlstCallbacks.begin(); it != pEnt->mlstCallbacks.end(); ++it)
			{
				(*it)->OnTransformUpdate(pEnt);
			}
		}
		static void NodeDelete(uintptr_t alNode){ hplDelete((cNode3D*)alNode);}
	};

	const OhplH10Host* OhplH10GetHost()
	{
		static const OhplH10Host host = {OhplH10Hooks::EntOnUpdated, OhplH10Hooks::EntCallbacks, OhplH10Hooks::NodeDelete};
		return &host;
	}

	//-----------------------------------------------------------------------
	iEntity3D::iEntity3D(tString asName)
	{
		msName = asName;
		mpE = ohpl_ent_new((uintptr_t)this);
		msSourceFile = "";
	}

	iEntity3D::~iEntity3D()
	{
		ohpl_ent_free(mpE);
	}

	//-----------------------------------------------------------------------

	cBoundingVolume* iEntity3D::GetBoundingVolume()
	{
		if(ohpl_h10_ent_bv_dirty(OhplH10GetHost(), mpE)) mBoundingVolume.SetTransform(mpE->world);
		return &mBoundingVolume;
	}

	void iEntity3D::AddCallback(iEntityCallback *apCallback)
	{
		mlstCallbacks.push_back(apCallback);
	}

	void iEntity3D::RemoveCallback(iEntityCallback *apCallback)
	{
		STLFindAndRemove(mlstCallbacks, apCallback);
	}

	cEntity3DIterator iEntity3D::GetChildIterator()
	{
		std::vector<iEntity3D*> vItems;
		for(size_t i=0, lCount = ohpl_ent_child_count(mpE); i<lCount; ++i) vItems.push_back((iEntity3D*)ohpl_ent_child_at(mpE, i));
		return cEntity3DIterator(vItems);
	}

	void iEntity3D::AddNodeChild(cNode3D *apNode){ ohpl_ent_add_node_child(OhplH10GetHost(), mpE, apNode->mpN);}
	void iEntity3D::RemoveNodeChild(cNode3D *apNode){ ohpl_ent_remove_node_child(mpE, apNode->mpN);}
	bool iEntity3D::IsNodeChild(cNode3D *apNode){ return ohpl_ent_is_node_child(mpE, apNode->mpN);}
}
