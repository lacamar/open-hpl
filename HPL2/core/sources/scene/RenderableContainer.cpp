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

#include "scene/RenderableContainer.h"

#include "system/LowLevelSystem.h"

#include "graphics/Renderable.h"
#include "graphics/OcclusionQuery.h"
#include "math/Math.h"
#include "math/BoundingVolume.h"

#include <cstring>
#include <vector>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// VISIBLE NODE TRACKER
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cVisibleRCNodeTracker::cVisibleRCNodeTracker()
	{
		mpTracker = ohpl_rn_tracker_new();
	}

	//-----------------------------------------------------------------------

	void cVisibleRCNodeTracker::SwitchAndClearVisibleNodeSet()
	{
		ohpl_rn_tracker_switch(mpTracker);
	}

	void cVisibleRCNodeTracker::SetNodeVisible(iRenderableContainerNode *apNode)
	{
		ohpl_rn_tracker_set_visible(mpTracker, (uintptr_t)apNode);
	}

	bool cVisibleRCNodeTracker::WasNodeVisible(iRenderableContainerNode *apNode)
	{
		return ohpl_rn_tracker_was_visible(mpTracker, (uintptr_t)apNode);
	}

	//-----------------------------------------------------------------------

	cVisibleRCNodeTracker::~cVisibleRCNodeTracker()
	{
		Reset();
		ohpl_rn_tracker_free(mpTracker);
	}

	void cVisibleRCNodeTracker::Reset()
	{
		std::vector<uintptr_t> vPending(ohpl_rn_tracker_pending(mpTracker, NULL, 0));
		ohpl_rn_tracker_pending(mpTracker, vPending.empty() ? NULL : &vPending[0], vPending.size());
		for(uintptr_t pQuery : vPending) hplDelete((iOcclusionQuery*)pQuery);

		ohpl_rn_tracker_reset(mpTracker);
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// RENDERABLE CALLBACK
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cRenderableContainerObjectCallback::cRenderableContainerObjectCallback()
	{

	}

	//-----------------------------------------------------------------------

	void cRenderableContainerObjectCallback::OnVisibleChange(iRenderable *apObject)
	{
		ohpl_bt_object_properties_changed(OhplContainerHost(), NULL, (uintptr_t)apObject);
	}
	
	//-----------------------------------------------------------------------

	void cRenderableContainerObjectCallback::OnRenderFlagsChange(iRenderable *apObject)
	{
		ohpl_bt_object_properties_changed(OhplContainerHost(), NULL, (uintptr_t)apObject);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// HOST CALLBACKS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	static inline iRenderableContainerNode* HNode(uintptr_t alNode){ return (iRenderableContainerNode*)alNode;}
	static inline iRenderable* HObj(uintptr_t alObj){ return (iRenderable*)alObj;}

	static OhplRcNodeBase* HBase(void*, uintptr_t alNode){ return &HNode(alNode)->State();}

	static size_t HChildCount(void*, uintptr_t alNode){ return HNode(alNode)->GetChildNodeList()->size();}

	static size_t HChildren(void*, uintptr_t alNode, uintptr_t *apOut, size_t alCap)
	{
		size_t lCount =0;
		for(iRenderableContainerNode *pChild : *HNode(alNode)->GetChildNodeList())
		{
			if(lCount < alCap) apOut[lCount] = (uintptr_t)pChild;
			++lCount;
		}
		return lCount;
	}

	static size_t HObjectCount(void*, uintptr_t alNode){ return HNode(alNode)->GetObjectList()->size();}

	static size_t HObjects(void*, uintptr_t alNode, uintptr_t *apOut, size_t alCap)
	{
		size_t lCount =0;
		for(iRenderable *pObject : *HNode(alNode)->GetObjectList())
		{
			if(lCount < alCap) apOut[lCount] = (uintptr_t)pObject;
			++lCount;
		}
		return lCount;
	}

	static void HPushObject(void*, uintptr_t alNode, uintptr_t alObj){ HNode(alNode)->GetObjectList()->push_back(HObj(alObj));}
	static void HClearObjects(void*, uintptr_t alNode){ HNode(alNode)->GetObjectList()->clear();}
	static void HRemoveFirstObject(void*, uintptr_t alNode, uintptr_t alObj){ STLFindAndRemove(*HNode(alNode)->GetObjectList(), HObj(alObj));}
	static void HRemoveAllObjects(void*, uintptr_t alNode, uintptr_t alObj){ HNode(alNode)->GetObjectList()->remove(HObj(alObj));}

	static void HDeleteChildren(void*, uintptr_t alNode){ STLDeleteAll(*HNode(alNode)->GetChildNodeList());}
	static void HDeleteChild(void*, uintptr_t alParent, uintptr_t alChild){ STLFindAndDelete(*HNode(alParent)->GetChildNodeList(), HNode(alChild));}
	static void HClearChildren(void*, uintptr_t alNode){ HNode(alNode)->GetChildNodeList()->clear();}
	static void HPushChild(void*, uintptr_t alNode, uintptr_t alChild){ HNode(alNode)->GetChildNodeList()->push_back(HNode(alChild));}
	static void HDeleteNode(void*, uintptr_t alNode){ hplDelete(HNode(alNode));}

	static uintptr_t HObjectNode(void*, uintptr_t alObj){ return (uintptr_t)HObj(alObj)->GetRenderContainerNode();}
	static void HSetObjectNode(void*, uintptr_t alObj, uintptr_t alNode){ HObj(alObj)->SetRenderContainerNode(HNode(alNode));}

	static void HObjectBV(void*, uintptr_t alObj, OhplRcBv *apOut)
	{
		cBoundingVolume *pBV = HObj(alObj)->GetBoundingVolume();
		apOut->mvMin = pBV->GetMin();
		apOut->mvMax = pBV->GetMax();
		apOut->mvCenter = pBV->GetWorldCenter();
		apOut->mvPosition = pBV->GetPosition();
		apOut->mfRadius = pBV->GetRadius();
	}

	static bool HObjectVisible(void*, uintptr_t alObj){ return HObj(alObj)->IsVisible();}
	static uint32_t HObjectRenderFlags(void*, uintptr_t alObj){ return HObj(alObj)->GetRenderFlags();}

	void OhplFillContainerHost(OhplRnHostVt &avVt)
	{
		avVt.base = HBase;
		avVt.child_count = HChildCount;
		avVt.children = HChildren;
		avVt.object_count = HObjectCount;
		avVt.objects = HObjects;
		avVt.push_object = HPushObject;
		avVt.clear_objects = HClearObjects;
		avVt.remove_first_object = HRemoveFirstObject;
		avVt.remove_all_objects = HRemoveAllObjects;
		avVt.delete_children = HDeleteChildren;
		avVt.delete_child = HDeleteChild;
		avVt.clear_children = HClearChildren;
		avVt.push_child = HPushChild;
		avVt.delete_node = HDeleteNode;
		avVt.object_node = HObjectNode;
		avVt.set_object_node = HSetObjectNode;
		avVt.object_bv = HObjectBV;
		avVt.object_visible = HObjectVisible;
		avVt.object_render_flags = HObjectRenderFlags;
	}

	const OhplRnHostVt* OhplContainerHost()
	{
		static const OhplRnHostVt vt = []{ OhplRnHostVt v; memset(&v, 0, sizeof(v)); OhplFillContainerHost(v); return v; }();
		return &vt;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// RENDERABLE CONTAINER NODE
	//////////////////////////////////////////////////////////////////////////

	iRenderableContainerNode::iRenderableContainerNode()
	{
		ohpl_bt_node_base_init(&mS, false);
	}

	//-----------------------------------------------------------------------

	void iRenderableContainerNode::PushUpNeedAABBUpdate()
	{
		ohpl_bt_push_up_aabb_update(OhplContainerHost(), NULL, (uintptr_t)this);
	}

	//-----------------------------------------------------------------------

	void iRenderableContainerNode::CalculateMinMaxFromObjects()
	{
		ohpl_bt_calculate_min_max(OhplContainerHost(), NULL, (uintptr_t)this);
	}

	//////////////////////////////////////////////////////////////////////////
	// RENDERABLE CONTAINER
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iRenderableContainer::UpdateBeforeRendering()
	{
		ohpl_bt_update_before_rendering(OhplContainerHost(), NULL, (uintptr_t)GetRoot());

		/////////////////////////
		//Container specific Update
		SpecificUpdateBeforeRendering();
	}

	//-----------------------------------------------------------------------
}
