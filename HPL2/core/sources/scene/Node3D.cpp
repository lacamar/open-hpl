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

#include "scene/Node3D.h"
#include "scene/Entity3D.h"
#include "system/LowLevelSystem.h"
#include "math/Math.h"
#include "scene/RustNode.h"
#include "scene/RustSceneH10.h"

namespace hpl {

	cNode3D::cNode3D(const tString &asName, bool abAutoDeleteChildren)
	{
		msName = asName;
		mpN = ohpl_node_new((uintptr_t)this, abAutoDeleteChildren);
	}

	cNode3D::~cNode3D()
	{
		ohpl_node_free(OhplH10GetHost(), mpN);
	}

	bool cNode3D::AddEntity(iEntity3D* apEntity){ return ohpl_node_add_entity(mpN, apEntity->mpE);}
	bool cNode3D::RemoveEntity(iEntity3D* apEntity){ return ohpl_node_remove_entity(mpN, apEntity->mpE);}

	cNode3DIterator cNode3D::GetChildIterator()
	{
		std::vector<cNode3D*> vItems;
		for(size_t i=0, lCount = ohpl_node_child_count(mpN); i<lCount; ++i) vItems.push_back((cNode3D*)ohpl_node_child_at(mpN, i));
		return cNode3DIterator(vItems);
	}

	cEntity3DIterator cNode3D::GetEntityIterator()
	{
		std::vector<iEntity3D*> vItems;
		for(size_t i=0, lCount = ohpl_node_entity_count(mpN); i<lCount; ++i) vItems.push_back((iEntity3D*)ohpl_node_entity_at(mpN, i));
		return cEntity3DIterator(vItems);
	}

	cNode3D* cNode3D::CreateChild(const tString &asName, bool abAutoDeleteChildren)
	{
		cNode3D *pNode = hplNew( cNode3D, (asName,abAutoDeleteChildren) );
		ohpl_node_set_parent(pNode->mpN, mpN);
		return pNode;
	}
}
