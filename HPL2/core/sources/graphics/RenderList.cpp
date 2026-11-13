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

#include "graphics/RenderList.h"

#include "graphics/Renderable.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/Renderer.h"
#include "graphics/SubMesh.h"

#include "scene/MeshEntity.h"
#include "scene/SubMeshEntity.h"
#include "scene/Light.h"
#include "scene/FogArea.h"
#include "scene/LensFlare.h"

#include "math/Math.h"
#include "math/Frustum.h"

#include "impl/RustBackends.h"

#include <algorithm>
#include <cstring>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cRenderList::cRenderList()
	{
		mfFrameTime =0;
		mpFrustum = NULL;
	}

	//-----------------------------------------------------------------------

	cRenderList::~cRenderList()
	{

	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cRenderList::Setup(float afFrameTime, cFrustum *apFrustum)
	{
		mfFrameTime = afFrameTime;
		mpFrustum = apFrustum;
	}

	//-----------------------------------------------------------------------

	void cRenderList::AddObject(iRenderable *apObject)
	{
		eRenderableType renderType =apObject->GetRenderType();

		if(renderType == eRenderableType_LensFlare)
		{
			cLensFlare *pFlare = static_cast<cLensFlare*>(apObject);
			if(pFlare->CollidesWithFrustum(mpFrustum)==false) return;
			mvOcclusionQueryObjects.push_back(pFlare);
			mvLensFlares.push_back(pFlare);
			for(int i=0; i<eLensFlareType_MultiIris; ++i)
				if(pFlare->IsFlareActive((eLensFlareType)i)) AddObject(pFlare->GetTypeRenderable((eLensFlareType)i));
			if(pFlare->IsFlareActive(eLensFlareType_MultiIris)) mvMultiIrisFlares.push_back(pFlare);
			return;
		}

		if(OhplBackendRustRequested())
		{
			AddObjectRust(apObject, renderType);
			return;
		}

		////////////////////////////////////////
		//Update material, if not already done this frame
		cMaterial *pMaterial = apObject->GetMaterial();
		iMaterialType *pMaterialType = pMaterial ? pMaterial->GetType() : NULL;
		if(pMaterial && pMaterial->GetRenderFrameCount() != iRenderer::GetRenderFrameCount())
		{
			pMaterial->SetRenderFrameCount(iRenderer::GetRenderFrameCount());
			pMaterial->UpdateBeforeRendering(mfFrameTime);
		}

		////////////////////////////////////////
		// Update per frame things, if not done yet.
		if(apObject->GetRenderFrameCount() != iRenderer::GetRenderFrameCount())
		{
			apObject->SetRenderFrameCount(iRenderer::GetRenderFrameCount());
			apObject->UpdateGraphicsForFrame(mfFrameTime);
		}

		////////////////////////////////////////
		//Update per viewport specific and set amtrix point
		//Skip this for non-decal translucent! This is because the water rendering might mess it up otherwise!
		if(pMaterialType==NULL || pMaterialType->IsTranslucent()==false || pMaterialType->IsDecal() )
		{
			//skip rendering if the update return false
			if(apObject->UpdateGraphicsForViewport(mpFrustum,mfFrameTime)==false)
			{
				return;
			}

			apObject->SetModelMatrixPtr(apObject->GetModelMatrix(mpFrustum));
		}
		//Only set a matrix used for sorting. Calculate the proper in the trans rendering!
		else
		{
			apObject->SetModelMatrixPtr(apObject->GetModelMatrix(NULL));
		}

		////////////////////////////////////////
        //Calculate the View Z value
		// For transparent and non decals!
		if(pMaterialType && pMaterialType->IsTranslucent() && pMaterialType->IsDecal()==false)
		{
			cVector3f vIntersectionPos;
			cBoundingVolume *pBV = apObject->GetBoundingVolume();

			//If there is an intersection (which happens unless inside), use that. Else use world center
			if(cMath::CheckAABBLineIntersection(pBV->GetMin(), pBV->GetMax(), mpFrustum->GetOrigin(), pBV->GetWorldCenter(), &vIntersectionPos, NULL)==false)
			{
				vIntersectionPos = pBV->GetWorldCenter();		
			}
			
			vIntersectionPos = cMath::MatrixMul(mpFrustum->GetViewMatrix(), vIntersectionPos);
			apObject->SetViewSpaceZ(vIntersectionPos.z);			
		}
		else
		{
			cVector3f vCameraPos = cMath::MatrixMul(mpFrustum->GetViewMatrix(), apObject->GetBoundingVolume()->GetWorldCenter());
			apObject->SetViewSpaceZ(vCameraPos.z);
		}
		
		

		//////////////////////////////
		// If objects uses occlusion queries, add it as such
		if(apObject->UsesOcclusionQuery())
		{
			mvOcclusionQueryObjects.push_back(apObject);
		}
        
		//////////////////////////////
		// Light, add to special list
		if(renderType == eRenderableType_Light)
		{
			mvLights.push_back( static_cast<iLight*>(apObject) );
		}
		//////////////////////////////
		// Fog area, add to special list
		if(renderType == eRenderableType_FogArea)
		{
			mvFogAreas.push_back( static_cast<cFogArea*>(apObject) );
		}
		//////////////////////////////
		// GuiSet, add to special list
		else if(renderType == eRenderableType_GuiSet)
		{
			//TODO....
		}
		///////////////////////////
		// Normal addition
		else
		{
			if(pMaterial==NULL) return; //Skip if it has no material...

			////////////////////////
			// Transparent
			if(pMaterialType->IsTranslucent())
			{
				if(pMaterialType->IsDecal())
					mvDecalObjects.push_back(apObject);
				else
					mvTransObjects.push_back(apObject);
			}
			////////////////////////
			// Solid
			else
			{
				mvSolidObjects.push_back(apObject);
				if(pMaterial->GetTexture(eMaterialTexture_Illumination) && apObject->GetIlluminationAmount()>0)
				{
					mvIllumObjects.push_back(apObject);
				}
			}
		}
	}

	void cRenderList::AddObjectRust(iRenderable *apObject, eRenderableType aRenderType)
	{
		const int lFrame = iRenderer::GetRenderFrameCount();
		cMaterial *pMaterial = apObject->GetMaterial();
		iMaterialType *pMaterialType = pMaterial ? pMaterial->GetType() : NULL;

		OhplH8GateIn gate;
		gate.hasMaterial = pMaterial != NULL;
		gate.hasType = pMaterialType != NULL;
		gate.translucent = pMaterialType && pMaterialType->IsTranslucent();
		gate.decal = pMaterialType && pMaterialType->IsDecal();
		gate.frame = lFrame;
		gate.materialFrame = pMaterial ? pMaterial->GetRenderFrameCount() : 0;
		gate.objectFrame = apObject->GetRenderFrameCount();
		const unsigned lGates = ohpl_h8_rl_gates(&gate);

		if(lGates & eOhplH8Gate_MaterialUpdate)
		{
			pMaterial->SetRenderFrameCount(lFrame);
			pMaterial->UpdateBeforeRendering(mfFrameTime);
		}
		if(lGates & eOhplH8Gate_ObjectUpdate)
		{
			apObject->SetRenderFrameCount(lFrame);
			apObject->UpdateGraphicsForFrame(mfFrameTime);
		}

		if(lGates & eOhplH8Gate_ViewportUpdate)
		{
			if(apObject->UpdateGraphicsForViewport(mpFrustum,mfFrameTime)==false) return;
			apObject->SetModelMatrixPtr(apObject->GetModelMatrix(mpFrustum));
		}
		else
		{
			apObject->SetModelMatrixPtr(apObject->GetModelMatrix(NULL));
		}

		cBoundingVolume *pBV = apObject->GetBoundingVolume();
		OhplH8PostIn in;
		in.renderType = aRenderType;
		in.hasMaterial = gate.hasMaterial;
		in.hasType = gate.hasType;
		in.translucent = gate.translucent;
		in.decal = gate.decal;
		in.hasIllumTexture = pMaterial && pMaterial->GetTexture(eMaterialTexture_Illumination);
		in.usesOcclusionQuery = apObject->UsesOcclusionQuery();
		in.illumination = apObject->GetIlluminationAmount();
		const cVector3f &vMin = pBV->GetMin(), &vMax = pBV->GetMax(), &vCenter = pBV->GetWorldCenter(), &vOrigin = mpFrustum->GetOrigin();
		in.bvMin.x = vMin.x; in.bvMin.y = vMin.y; in.bvMin.z = vMin.z;
		in.bvMax.x = vMax.x; in.bvMax.y = vMax.y; in.bvMax.z = vMax.z;
		in.bvCenter.x = vCenter.x; in.bvCenter.y = vCenter.y; in.bvCenter.z = vCenter.z;
		in.origin.x = vOrigin.x; in.origin.y = vOrigin.y; in.origin.z = vOrigin.z;
		memcpy(&in.view, &mpFrustum->GetViewMatrix(), sizeof(OhplMat4));

		OhplH8PostOut out;
		ohpl_h8_rl_post(&in, &out);

		apObject->SetViewSpaceZ(out.viewZ);
		if(out.lists & eOhplH8List_Occlusion)	mvOcclusionQueryObjects.push_back(apObject);
		if(out.lists & eOhplH8List_Light)		mvLights.push_back( static_cast<iLight*>(apObject) );
		if(out.lists & eOhplH8List_Fog)			mvFogAreas.push_back( static_cast<cFogArea*>(apObject) );
		if(out.lists & eOhplH8List_Decal)		mvDecalObjects.push_back(apObject);
		if(out.lists & eOhplH8List_Trans)		mvTransObjects.push_back(apObject);
		if(out.lists & eOhplH8List_Solid)		mvSolidObjects.push_back(apObject);
		if(out.lists & eOhplH8List_Illum)		mvIllumObjects.push_back(apObject);
	}

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	void cRenderList::Clear()
	{
		// Use resize instead of clear, because that way capacity is preserved and allocation is never 
		// needed unless there is a need to increase the vector size.

		mvOcclusionQueryObjects.resize(0);
		mvTransObjects.resize(0);
		mvDecalObjects.resize(0);
		mvSolidObjects.resize(0);
		mvIllumObjects.resize(0);
		mvLights.resize(0);
		mvFogAreas.resize(0);
		mvLensFlares.resize(0);
		mvMultiIrisFlares.resize(0);

		for(int i=0; i<eRenderListType_LastEnum; ++i)
		{
			mvSortedArrays[i].resize(0);
		}
	}

	//-----------------------------------------------------------------------

	void cRenderList::PrintAllObjects()
	{
		Log("---------------------------------\n");
		Log("------ RENDER LIST CONTENTS -----\n");

		Log("Trans Objects:\n");
		for(size_t i=0; i<mvTransObjects.size(); ++i)
			Log(" '%s' ViewspaceZ: %f LargeSurfacePlacement: %d Mat: '%s' RenderCount: %d\n", mvTransObjects[i]->GetName().c_str(), 
					mvTransObjects[i]->GetViewSpaceZ(),
					mvTransObjects[i]->GetLargePlaneSurfacePlacement(),
					mvTransObjects[i]->GetMaterial()->GetName().c_str(), 
					mvTransObjects[i]->GetRenderFrameCount());

		Log("Solid Objects:\n");
		for(size_t i=0; i<mvSolidObjects.size(); ++i)
			Log(" '%s' Mat: '%s' RenderCount: %d\n", mvSolidObjects[i]->GetName().c_str(), mvSolidObjects[i]->GetMaterial()->GetName().c_str(), mvSolidObjects[i]->GetRenderFrameCount());
		
		Log("Decal Objects:\n");
		for(size_t i=0; i<mvDecalObjects.size(); ++i)
			Log(" '%s' Mat: '%s' RenderCount: %d\n", mvDecalObjects[i]->GetName().c_str(), mvDecalObjects[i]->GetMaterial()->GetName().c_str(), mvSolidObjects[i]->GetRenderFrameCount());
		
		Log("Illum Objects:\n");
		for(size_t i=0; i<mvIllumObjects.size(); ++i)
			Log(" '%s' Mat: '%s' RenderCount: %d\n", mvIllumObjects[i]->GetName().c_str(), mvIllumObjects[i]->GetMaterial()->GetName().c_str(), mvSolidObjects[i]->GetRenderFrameCount());
	

		Log("---------------------------------\n");
	}

	//-----------------------------------------------------------------------

	bool cRenderList::ArrayHasObjects(eRenderListType aType)
	{
		return mvSortedArrays[aType].empty()==false;
	}

	//-----------------------------------------------------------------------


	cRenderableVecIterator cRenderList::GetArrayIterator(eRenderListType aType)
	{
		return cRenderableVecIterator(&mvSortedArrays[aType]);
	}

	//-----------------------------------------------------------------------

	cRenderableVecIterator cRenderList::GetOcclusionQueryObjectIterator()
	{
		return cRenderableVecIterator(&mvOcclusionQueryObjects);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	static inline uintptr_t PtrId(const void *apPtr){ return (uintptr_t)apPtr; }

	static_assert(sizeof(OhplSolidKey) == 184 && sizeof(OhplTransKey) == 116 && sizeof(OhplDecalKey) == 48 &&
				  sizeof(OhplIllumKey) == 32 && sizeof(OhplCompileIn) == 80 && sizeof(OhplCompileOut) == 40, "Render list key layout");

	void cRenderList::Compile(tRenderListCompileFlag aFlags)
	{
		const bool bZ = (aFlags & eRenderListCompileFlag_Z) != 0;
		const bool bDiffuse = (aFlags & eRenderListCompileFlag_Diffuse) != 0;
		const bool bDecal = (aFlags & eRenderListCompileFlag_Decal) != 0;
		const bool bIllum = (aFlags & eRenderListCompileFlag_Illumination) != 0;
		const bool bTrans = (aFlags & eRenderListCompileFlag_Translucent) != 0;
		if(!(bZ || bDiffuse || bDecal || bIllum || bTrans)) return;

		////////////////////////////////////
		// Gather the sort keys
		const size_t lSolid = (bZ || bDiffuse) ? mvSolidObjects.size() : 0;
		mvSolidKeys.resize(lSolid);
		for(size_t i=0; i<lSolid; ++i)
		{
			iRenderable *pObject = mvSolidObjects[i];
			cMaterial *pMat = pObject->GetMaterial();
			OhplSolidKey &key = mvSolidKeys[i];
			key.alphaMode = pMat->GetAlphaMode();
			key.viewZ = pObject->GetViewSpaceZ();
			if(bZ)
			{
				key.progZ = PtrId(pMat->GetProgram(0,eMaterialRenderMode_Z));
				key.texDiffuse = PtrId(pMat->GetTexture(eMaterialTexture_Diffuse));
			}
			if(bDiffuse)
			{
				key.progDiffuse = PtrId(pMat->GetProgram(0,eMaterialRenderMode_Diffuse));
				for(int u=0; u<kMaxTextureUnits; ++u)
					key.texUnits[u] = PtrId(pMat->GetTextureInUnit(eMaterialRenderMode_Diffuse,u));
				key.vtx = PtrId(pObject->GetVertexBuffer());
				key.mtx = PtrId(pObject->GetModelMatrixPtr());
				key.obj = PtrId(pObject);
			}
		}

		const size_t lTrans = bTrans ? mvTransObjects.size() : 0;
		mvTransKeys.resize(lTrans);
		for(size_t i=0; i<lTrans; ++i)
		{
			iRenderable *pObject = mvTransObjects[i];
			OhplTransKey &key = mvTransKeys[i];
			key.viewZ = pObject->GetViewSpaceZ();
			key.placement = 0;
			key.planeCandidate = 0;
			key.pad = 0;
			cVector3f vCenter = pObject->GetBoundingVolume()->GetWorldCenter();
			key.center.x = vCenter.x; key.center.y = vCenter.y; key.center.z = vCenter.z;

			if(pObject->GetRenderType() != eRenderableType_SubMesh) continue;
			if(pObject->GetMaterial()->GetLargeTransperantSurface()==false) continue;
			cSubMeshEntity *pSubEnt = static_cast<cSubMeshEntity*>(pObject);
			cSubMesh *pSubMesh = pSubEnt->GetSubMesh();
			if(pSubMesh->GetIsOneSided()==false) continue;

			key.planeCandidate = 1;
			const cVector3f &vNormal = pSubMesh->GetOneSidedNormal();
			const cVector3f &vPoint = pSubMesh->GetOneSidedPoint();
			key.normal.x = vNormal.x; key.normal.y = vNormal.y; key.normal.z = vNormal.z;
			key.point.x = vPoint.x; key.point.y = vPoint.y; key.point.z = vPoint.z;
			memcpy(&key.world, &pSubEnt->GetWorldMatrix(), sizeof(OhplMat4));
		}

		const size_t lDecal = bDecal ? mvDecalObjects.size() : 0;
		mvDecalKeys.resize(lDecal);
		for(size_t i=0; i<lDecal; ++i)
		{
			iRenderable *pObject = mvDecalObjects[i];
			cMaterial *pMat = pObject->GetMaterial();
			OhplDecalKey &key = mvDecalKeys[i];
			key.sortOrder = pMat->GetDecalSortOrder();
			key.pad = 0;
			key.texIllum = PtrId(pMat->GetTexture(eMaterialTexture_Illumination));
			key.vtx = PtrId(pObject->GetVertexBuffer());
			key.mtx = PtrId(pObject->GetModelMatrixPtr());
			cVector3f vPos = pObject->GetWorldPosition();
			key.pos.x = vPos.x; key.pos.y = vPos.y; key.pos.z = vPos.z;
			key.pad2 = 0;
		}

		const size_t lIllum = bIllum ? mvIllumObjects.size() : 0;
		mvIllumKeys.resize(lIllum);
		for(size_t i=0; i<lIllum; ++i)
		{
			iRenderable *pObject = mvIllumObjects[i];
			OhplIllumKey &key = mvIllumKeys[i];
			key.illumAmount = pObject->GetIlluminationAmount();
			key.pad = 0;
			key.texIllum = PtrId(pObject->GetMaterial()->GetTexture(eMaterialTexture_Illumination));
			key.vtx = PtrId(pObject->GetVertexBuffer());
			key.mtx = PtrId(pObject->GetModelMatrixPtr());
		}

		////////////////////////////////////
		// Sort in Rust
		for(int i=0; i<5; ++i) mvOrder[i].resize(0);
		if(bZ)			mvOrder[eRenderListType_Z].resize(lSolid);
		if(bDiffuse)	mvOrder[eRenderListType_Diffuse].resize(lSolid);
		if(bTrans)		mvOrder[eRenderListType_Translucent].resize(lTrans);
		if(bDecal)		mvOrder[eRenderListType_Decal].resize(lDecal);
		if(bIllum)		mvOrder[eRenderListType_Illumination].resize(lIllum);

		OhplCompileIn in;
		in.flags = aFlags;
		in.origin.x = in.origin.y = in.origin.z = 0;
		if(bTrans)
		{
			const cVector3f &vOrigin = mpFrustum->GetOrigin();
			in.origin.x = vOrigin.x; in.origin.y = vOrigin.y; in.origin.z = vOrigin.z;
		}
		in.solid = mvSolidKeys.data();		in.solidLen = lSolid;
		in.trans = mvTransKeys.data();		in.transLen = lTrans;
		in.decal = mvDecalKeys.data();		in.decalLen = lDecal;
		in.illum = mvIllumKeys.data();		in.illumLen = lIllum;

		OhplCompileOut out;
		out.z = mvOrder[eRenderListType_Z].data();
		out.diffuse = mvOrder[eRenderListType_Diffuse].data();
		out.trans = mvOrder[eRenderListType_Translucent].data();
		out.decal = mvOrder[eRenderListType_Decal].data();
		out.illum = mvOrder[eRenderListType_Illumination].data();

		ohpl_renderlist_compile(&in, &out);

		////////////////////////////////////
		// Apply
		if(bTrans)
		{
			for(size_t i=0; i<lTrans; ++i) mvTransObjects[i]->SetLargePlaneSurfacePlacement(mvTransKeys[i].placement);
		}

		if(bZ)			ApplyOrder(eRenderListType_Z, mvSolidObjects);
		if(bDiffuse)	ApplyOrder(eRenderListType_Diffuse, mvSolidObjects);
		if(bDecal)		ApplyOrder(eRenderListType_Decal, mvDecalObjects);
		if(bIllum)		ApplyOrder(eRenderListType_Illumination, mvIllumObjects);
		if(bTrans)		ApplyOrder(eRenderListType_Translucent, mvTransObjects);
	}

	//-----------------------------------------------------------------------

	void cRenderList::ApplyOrder(eRenderListType aType, const tRenderableVec& avSource)
	{
		const std::vector<unsigned>& vOrder = mvOrder[aType];
		tRenderableVec& vDest = mvSortedArrays[aType];
		vDest.resize(vOrder.size());
		for(size_t i=0; i<vOrder.size(); ++i) vDest[i] = avSource[vOrder[i]];
	}

	//-----------------------------------------------------------------------

}
