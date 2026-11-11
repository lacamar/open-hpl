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

#include "scene/RenderableContainer_DynBoxTree.h"

#include "graphics/Renderable.h"
#include "graphics/Renderer.h"
#include "graphics/LowLevelGraphics.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// HOST
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	static OhplBtDynState* HDynState(void*, uintptr_t alNode){ return &static_cast<cRCNode_DynBoxTree*>((iRenderableContainerNode*)alNode)->DynState();}

	static uintptr_t HNewChild(void *apContainer, uintptr_t alParent)
	{
		cRCNode_DynBoxTree *pParent = static_cast<cRCNode_DynBoxTree*>((iRenderableContainerNode*)alParent);
		cRCNode_DynBoxTree *pChild = hplNew( cRCNode_DynBoxTree, () );
		pChild->SetContainer((cRenderableContainer_DynBoxTree*)apContainer);
		pChild->SetParent(pParent);
		pParent->GetChildNodeList()->push_back(pChild);
		return (uintptr_t)pChild;
	}

	static const OhplRnHostVt* DynHost()
	{
		static const OhplRnHostVt vt = []{
			OhplRnHostVt v;
			memset(&v, 0, sizeof(v));
			OhplFillContainerHost(v);
			v.dyn_state = HDynState;
			v.new_child = HNewChild;
			return v;
		}();
		return &vt;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CALLBACK
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cDynBoxTreeObjectCallback::cDynBoxTreeObjectCallback(cRenderableContainer_DynBoxTree *apContainer) : cRenderableContainerObjectCallback()
	{
		mpContainer = apContainer;
	}

	//-----------------------------------------------------------------------

	void cDynBoxTreeObjectCallback::OnTransformUpdate(iEntity3D * apEntity)
	{
		////////////////////////////////////////////
		//Get renderable object
		iRenderable *pObject = static_cast<iRenderable*>(apEntity);

		mpContainer->m_setObjectsToUpdate.insert(pObject);
	}

	//-----------------------------------------------------------------------

	
	//////////////////////////////////////////////////////////////////////////
	// NODE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cRCNode_DynBoxTree::cRCNode_DynBoxTree()
	{
		ohpl_bt_node_dyn_init(&mD);
		mpContainer = NULL;
	}

	//-----------------------------------------------------------------------

	cRCNode_DynBoxTree::~cRCNode_DynBoxTree()
	{
		STLDeleteAll(mlstChildNodes);
	}

	//-----------------------------------------------------------------------

	void cRCNode_DynBoxTree::UpdateBeforeUse()
	{
		ohpl_bt_dyn_update_before_use(DynHost(), mpContainer, &mpContainer->mTree, (uintptr_t)this);
	}

	//-----------------------------------------------------------------------

	void cRCNode_DynBoxTree::RemoveObject(iRenderable *apObject)
	{
		STLFindAndRemove(mlstObjects, apObject);
	}
	
	//-----------------------------------------------------------------------

	void cRCNode_DynBoxTree::RecalculateSplit()
	{
		mD.mbRecalculateSplit = true;
	}

	//-----------------------------------------------------------------------

	void cRCNode_DynBoxTree::RecalculateAABB()
	{
		mD.mbRecalculateAABB = true;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cRenderableContainer_DynBoxTree::cRenderableContainer_DynBoxTree()
	{
		ohpl_bt_tree_init(&mTree);

		mRoot.mpContainer = this;
		mRoot.SetParent(NULL);
		mRoot.State().mfViewDistance =0;
		mRoot.State().mbInsideView = true;

		mpObjectCalllback = hplNew( cDynBoxTreeObjectCallback, (this) );
	}
	
	cRenderableContainer_DynBoxTree::~cRenderableContainer_DynBoxTree()
	{
		hplDelete( mpObjectCalllback );
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cRenderableContainer_DynBoxTree::Add(iRenderable *apRenderable)
	{
		ohpl_bt_dyn_add(DynHost(), this, &mTree, (uintptr_t)&mRoot, (uintptr_t)apRenderable);

		/////////////////////////
		//Add callbacks
		apRenderable->SetRenderCallback(mpObjectCalllback);
		apRenderable->AddCallback(mpObjectCalllback);
	}

	//-----------------------------------------------------------------------

	void cRenderableContainer_DynBoxTree::Remove(iRenderable *apRenderable)
	{
		//////////////////////////////////
		// Remove from to-update list
		if(m_setObjectsToUpdate.empty()==false)
		{
			m_setObjectsToUpdate.erase(apRenderable);
		}

		if(ohpl_bt_dyn_remove(DynHost(), this, &mTree, (uintptr_t)apRenderable)==false) return;

		////////////////////////
		//Remove callbacks
		apRenderable->SetRenderCallback(NULL);
		apRenderable->RemoveCallback(mpObjectCalllback);
	}

	//-----------------------------------------------------------------------

	iRenderableContainerNode* cRenderableContainer_DynBoxTree::GetRoot()
	{
		return &mRoot;
	}

	//-----------------------------------------------------------------------

	void cRenderableContainer_DynBoxTree::Compile()
	{
		//When doing a compile, wait a little while and then do a rebuild. So all particle system and usch have come into place.
		mTree.mlRebuildCount = 10;
	}

	//-----------------------------------------------------------------------

	void cRenderableContainer_DynBoxTree::RebuildNodes()
	{
		mTree.mlRebuildCount = 0;
	}

	//-----------------------------------------------------------------------

	void cRenderableContainer_DynBoxTree::RenderDebug(cRendererCallbackFunctions *apFunctions)
	{
		apFunctions->SetDepthTest(true);
		apFunctions->SetDepthWrite(false);
		apFunctions->SetBlendMode(eMaterialBlendMode_None);

		apFunctions->SetProgram(NULL);
		apFunctions->SetTextureRange(NULL, 0);
		apFunctions->SetMatrix(NULL);


		RenderDebugNode(apFunctions, &mRoot,0);
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cRenderableContainer_DynBoxTree::SpecificUpdateBeforeRendering()
	{
		std::vector<uintptr_t> vMoved;
		for(iRenderable *pR : m_setObjectsToUpdate) vMoved.push_back((uintptr_t)pR);

		ohpl_bt_dyn_specific_update(DynHost(), this, &mTree, (uintptr_t)&mRoot, vMoved.empty() ? NULL : &vMoved[0], vMoved.size());

		m_setObjectsToUpdate.clear();
	}

	//-----------------------------------------------------------------------

	static cColor LevelColor[10] = {cColor(1,1,1),cColor(1,0,1),cColor(1,1,0),cColor(0,1,1),cColor(0,0,1),cColor(0,1,0),cColor(1,0,0),cColor(1,0.5f,1),
									cColor(1,1,0.5f), cColor(1,0.5f,0.5f)};

	void cRenderableContainer_DynBoxTree::RenderDebugNode(cRendererCallbackFunctions *apFunctions, cRCNode_DynBoxTree *apNode, int alLevel)
	{
		//AABB
		apFunctions->GetLowLevelGfx()->DrawBoxMinMax(apNode->GetMin(),apNode->GetMax(),LevelColor[alLevel % 10]);

		//Split plane
		if(apNode->HasChildNodes())
		{
			cVector3f vMin = apNode->GetMin();
			cVector3f vMax = apNode->GetMax();
			vMin.v[apNode->mD.mlSplitAxis] = apNode->mD.mfSplitPlane;
			vMax.v[apNode->mD.mlSplitAxis] = apNode->mD.mfSplitPlane;

			apFunctions->GetLowLevelGfx()->DrawBoxMinMax(vMin, vMax,LevelColor[alLevel % 10]);

		}
				

		tRenderableContainerNodeListIt childIt = apNode->GetChildNodeList()->begin();
		for(; childIt != apNode->GetChildNodeList()->end(); ++childIt)
		{
			cRCNode_DynBoxTree *pChildNode = static_cast<cRCNode_DynBoxTree*>(*childIt);
			RenderDebugNode(apFunctions,pChildNode , alLevel+1);
		}
	}

	//-----------------------------------------------------------------------

}
