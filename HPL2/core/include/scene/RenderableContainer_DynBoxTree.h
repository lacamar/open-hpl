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

#ifndef HPL_RENDERABLE_CONTAINER_DYNBOXTREE_H
#define HPL_RENDERABLE_CONTAINER_DYNBOXTREE_H

#include "scene/RenderableContainer.h"
#include "scene/RustRenderableContainer.h"

namespace hpl {

	//-------------------------------------------

	class cBoundingVolume;

	class cRCNode_DynBoxTree;
	class cRenderableContainer_DynBoxTree;

	//-------------------------------------------


	class cDynBoxTreeObjectCallback : public cRenderableContainerObjectCallback, public iEntityCallback
	{
	public:
		cDynBoxTreeObjectCallback(cRenderableContainer_DynBoxTree *apContainer);
		virtual ~cDynBoxTreeObjectCallback(){}

		void OnTransformUpdate(iEntity3D * apEntity);

	private:
		cRenderableContainer_DynBoxTree *mpContainer;
	};

	
	//-------------------------------------------

	class cRCNode_DynBoxTree : public iRenderableContainerNode
	{
	friend class cRenderableContainer_DynBoxTree;
	friend class cDynBoxTreeObjectCallback;
	public:
		cRCNode_DynBoxTree();
		~cRCNode_DynBoxTree();

		void UpdateBeforeUse();

		void RemoveObject(iRenderable *apObject);

		void RecalculateSplit();
		void RecalculateAABB();

		inline OhplBtDynState& DynState(){ return mD;}
		inline void SetContainer(cRenderableContainer_DynBoxTree *apContainer){ mpContainer = apContainer;}

	private:
		cRenderableContainer_DynBoxTree *mpContainer;

		OhplBtDynState mD;
	};

	//-------------------------------------------

	class cRenderableContainer_DynBoxTree : public iRenderableContainer
	{
	friend class cRCNode_DynBoxTree;
	friend class cDynBoxTreeObjectCallback;
	public:
		cRenderableContainer_DynBoxTree();
		~cRenderableContainer_DynBoxTree();

		void Add(iRenderable *apRenderable);
		void Remove(iRenderable *apRenderable);

		iRenderableContainerNode* GetRoot();

        void Compile();

		void RebuildNodes();

		void RenderDebug(cRendererCallbackFunctions *apFunctions);

	private:
		void SpecificUpdateBeforeRendering();

		void RenderDebugNode(cRendererCallbackFunctions *apFunctions, cRCNode_DynBoxTree *apNode, int alLevel);

		cRCNode_DynBoxTree mRoot;

		OhplBtTree mTree;

		tRenderableSet m_setObjectsToUpdate;

		cDynBoxTreeObjectCallback *mpObjectCalllback;
	};

	//-------------------------------------------
};
#endif // HPL_RENDERABLE_CONTAINER_DYNBOXTREE_H
