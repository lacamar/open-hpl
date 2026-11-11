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

#include "scene/RenderableContainer_BoxTree.h"

#include "graphics/Renderable.h"
#include "graphics/Renderer.h"
#include "graphics/LowLevelGraphics.h"

#include "system/LowLevelSystem.h"

#include <algorithm>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// NODE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cRCNode_BoxTree::cRCNode_BoxTree()
	{
		mS.mbUsesFlagsAndVisibility = true;
	}

	//-----------------------------------------------------------------------

	cRCNode_BoxTree::~cRCNode_BoxTree()
	{
		STLDeleteAll(mlstChildNodes);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cRenderableContainer_BoxTree::cRenderableContainer_BoxTree()
	{
		mlMinLeafObjects = 12;		//The minimum number of objects in each leaf.
		mfMinSideLength = 3.0f;		//The smallest largest side allowed before the splitting stop.
		mfMaxSideLength = 30.0f;	//The largest side allowed for splitting to stop (overrides mlMinLeafObjects!)

		mfMaxIntersectionAmount = 0.03f;	//The amount (1=100%) an object can be over the splitting line and still not be an intersecting object
											//Also is the amount by which a new AABB can be expanded.

		mlMaxVolumeCalcObjects = 200;		//The maximum number of objects present if a cpu intense volume calculations should be used.

		//THere are too few objects to create an intersection group, objects that have the relative length (percent of node size)
		//will still remian in intersection.
		mfMinForceIntersectionRelativeSize = 0.8f;

		//Create the root
		mpRoot = hplNew( cRCNode_BoxTree, ());
		mpRoot->mS.mpParent = 0;
		mpRoot->mS.mfViewDistance =0;
		mpRoot->mS.mbInsideView = true;

		mpObjectCalllback = hplNew( cRenderableContainerObjectCallback, () );
	}
	
	cRenderableContainer_BoxTree::~cRenderableContainer_BoxTree()
	{
		if(mpRoot) hplDelete(mpRoot);

		hplDelete( mpObjectCalllback );
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cRenderableContainer_BoxTree::Add(iRenderable *apRenderable)
	{
		m_mlstTempObjects.push_back(apRenderable);
	}

	//-----------------------------------------------------------------------

	void cRenderableContainer_BoxTree::Remove(iRenderable *apRenderable)
	{
		STLFindAndRemove(m_mlstTempObjects, apRenderable);
		if(mpRoot) ohpl_bt_boxtree_remove(OhplContainerHost(), NULL, (uintptr_t)mpRoot, (uintptr_t)apRenderable);
	}

	void cRenderableContainer_BoxTree::RemoveSet(const std::set<iRenderable*>& aSet)
	{
		m_mlstTempObjects.remove_if([&](iRenderable* pR) { return aSet.count(pR) != 0; });
		if(mpRoot)
		{
			std::vector<uintptr_t> vSet;
			for(iRenderable *pR : aSet) vSet.push_back((uintptr_t)pR);
			ohpl_bt_boxtree_remove_set(OhplContainerHost(), NULL, (uintptr_t)mpRoot, vSet.empty() ? NULL : &vSet[0], vSet.size());
		}
	}

	//-----------------------------------------------------------------------

	iRenderableContainerNode* cRenderableContainer_BoxTree::GetRoot()
	{
		return mpRoot;
	}

	//-----------------------------------------------------------------------
	void cRenderableContainer_BoxTree::Compile()
	{
		if(mpRoot) hplDelete(mpRoot);
		mpRoot = hplNew( cRCNode_BoxTree, ());
		mpRoot->mS.mpParent = 0;
		mpRoot->mS.mfViewDistance =0;
		mpRoot->mS.mbInsideView = true;

		std::vector<iRenderable*> vObjects(m_mlstTempObjects.begin(), m_mlstTempObjects.end());
		std::vector<OhplRcObject> vBounds(vObjects.size());
		for(size_t i=0; i<vObjects.size(); ++i)
		{
			cBoundingVolume *pBV = vObjects[i]->GetBoundingVolume();
			vBounds[i].mvMin = pBV->GetMin();
			vBounds[i].mvMax = pBV->GetMax();
			vBounds[i].mvCenter = pBV->GetWorldCenter();
		}

		OhplRcParams params;
		params.mlMinLeafObjects = mlMinLeafObjects;
		params.mfMinSideLength = mfMinSideLength;
		params.mfMaxSideLength = mfMaxSideLength;
		params.mfMaxIntersectionAmount = mfMaxIntersectionAmount;
		params.mlMaxVolumeCalcObjects = mlMaxVolumeCalcObjects;
		params.mfMinForceIntersectionRelativeSize = mfMinForceIntersectionRelativeSize;

		OhplBoxTree *pTree = ohpl_rc_boxtree_compile(vBounds.empty() ? NULL : &vBounds[0], vBounds.size(), &params);
		const OhplRcNode *pNodes = ohpl_rc_boxtree_nodes(pTree);
		const uint32_t *pIndices = ohpl_rc_boxtree_objects(pTree);
		size_t lNodeNum = ohpl_rc_boxtree_node_count(pTree);

		std::vector<cRCNode_BoxTree*> vNodes(lNodeNum, (cRCNode_BoxTree*)NULL);
		for(size_t i=0; i<lNodeNum; ++i)
		{
			const OhplRcNode &src = pNodes[i];
			cRCNode_BoxTree *pNode = i==0 ? mpRoot : hplNew(cRCNode_BoxTree, () );
			vNodes[i] = pNode;
			if(i>0)
			{
				pNode->SetParent(vNodes[src.mlParent]);
				vNodes[src.mlParent]->mlstChildNodes.push_back(pNode);
			}
			pNode->mS.mvMin = src.mvMin;
			pNode->mS.mvMax = src.mvMax;
			pNode->mS.mvCenter = src.mvCenter;
			pNode->mS.mfRadius = src.mfRadius;

			for(uint32_t j=0; j<src.mlObjectCount; ++j)
			{
				iRenderable *pObject = vObjects[pIndices[src.mlFirstObject + j]];
				pNode->mlstObjects.push_back(pObject);
				pObject->SetRenderCallback(mpObjectCalllback);
				pObject->SetRenderContainerNode(pNode);
			}
		}

		ohpl_rc_boxtree_free(pTree);
	}

	int glCount =0;
	int glDrawLevel=0;

	void cRenderableContainer_BoxTree::RenderDebug(cRendererCallbackFunctions *apFunctions)
	{
		glCount++;
		if(glCount > 300)
		{
			glCount =0;
			glDrawLevel++;
			if(glDrawLevel > 5) glDrawLevel =0;
		}
		
		apFunctions->SetDepthTest(true);
		apFunctions->SetDepthWrite(false);
		apFunctions->SetBlendMode(eMaterialBlendMode_None);

		apFunctions->SetProgram(NULL);
		apFunctions->SetTextureRange(NULL, 0);
		apFunctions->SetMatrix(NULL);


		RenderDebugNode(apFunctions, mpRoot,0);
	}
	
	//-----------------------------------------------------------------------
	//-----------------------------------------------------------------------
	static cColor LevelColor[10] = {cColor(1,1,1),cColor(1,0,1),cColor(1,1,0),cColor(0,1,1),cColor(0,0,1),cColor(0,1,0),cColor(1,0,0),cColor(1,0.5f,1),
									cColor(1,1,0.5f), cColor(1,0.5f,0.5f)};
	void cRenderableContainer_BoxTree::RenderDebugNode(cRendererCallbackFunctions *apFunctions, cRCNode_BoxTree *apNode, int alLevel)
	{
		//if(apNode->GetNodeList()->empty())
		if(glDrawLevel==alLevel || (glDrawLevel > alLevel && apNode->GetChildNodeList()->empty()))
			apFunctions->GetLowLevelGfx()->DrawBoxMinMax(apNode->GetMin(),apNode->GetMax(),LevelColor[alLevel % 10]);

		tRenderableContainerNodeListIt childIt = apNode->GetChildNodeList()->begin();
		for(; childIt != apNode->GetChildNodeList()->end(); ++childIt)
		{
			cRCNode_BoxTree *pChildNode = static_cast<cRCNode_BoxTree*>(*childIt);
			RenderDebugNode(apFunctions,pChildNode , alLevel+1);
		}


		tRenderableListIt objIt = apNode->GetObjectList()->begin();
		for(; objIt != apNode->GetObjectList()->end(); ++objIt)
		{
			iRenderable *pObj = *objIt;
			//cBoundingVolume *pBV = pObj->GetBoundingVolume();
			//apFunctions->GetLowLevelGfx()->DrawBoxMinMax(pBV->GetMin(),pBV->GetMax(),LevelColor[alLevel % 10]);	
		}
	}

}
