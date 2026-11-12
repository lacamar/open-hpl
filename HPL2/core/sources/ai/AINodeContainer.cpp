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

#include "ai/AINodeContainer.h"

#include "scene/World.h"
#include "physics/PhysicsBody.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"

#include "math/Math.h"
#include "impl/RustBackends.h"


#include "impl/tinyXML/tinyxml.h"

#include <algorithm>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// AI NODE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cAINode::cAINode()
	{
		mlID = 0;
		mlIdx = -1;
		mpGraph = NULL;
		mpUserData = NULL;
	}

	//-----------------------------------------------------------------------

	cAINode::~cAINode()
	{
	}

	//-----------------------------------------------------------------------

	void cAINode::AddEdge(cAINode *pNode)
	{
		cAINodeEdge Edge;
		
		Edge.mpNode = pNode;
		Edge.mfDistance = cMath::Vector3Dist(mvPosition, pNode->mvPosition);
		Edge.mfSqrDistance = cMath::Vector3DistSqr(mvPosition, pNode->mvPosition);
		
		mvEdges.push_back(Edge);

		if(mpGraph && pNode->mlIdx >= 0) ohpl_ai_graph_connect(mpGraph, (uint32_t)mlIdx, (uint32_t)pNode->mlIdx);
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// RAY INTERSECT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cAINodeRayCallback::Reset()
	{
		mbIntersected = false;
		mpCallback = NULL;
	}
	
	//-----------------------------------------------------------------------

	bool cAINodeRayCallback::Intersected()
	{
		return mbIntersected;
	}

	//-----------------------------------------------------------------------
	
	bool cAINodeRayCallback::BeforeIntersect(iPhysicsBody *pBody)
	{
		if(pBody->GetCollideCharacter()==false) return false;

		if( (mFlags & eAIFreePathFlag_SkipStatic) && pBody->GetMass() == 0) return false;

		if( (mFlags & eAIFreePathFlag_SkipDynamic) && 
			(pBody->GetMass() > 0 || pBody->IsCharacter()) ) return false;

		if( (mFlags & eAIFreePathFlag_SkipVolatile) && pBody->IsVolatile()) return false;

		return true;
	}

	//-----------------------------------------------------------------------

	bool cAINodeRayCallback::OnIntersect(iPhysicsBody *pBody,cPhysicsRayParams *apParams)
	{
		if(mpCallback)
		{
			if(mpCallback->Intersects(pBody,apParams))
			{
				mbIntersected = true;
				return false;
			}
			else
			{
				return true;
			}
		}
		else
		{
			mbIntersected = true;
			return false;
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cAINodeIterator::cAINodeIterator(cAINodeContainer *apContainer, const cVector3f &avPos, float afRadius)
	{
		mlPos = 0;

		size_t lCount = ohpl_ai_graph_query(apContainer->mpGraph, &avPos, afRadius);
		std::vector<uint32_t> vIdx(lCount);
		ohpl_ai_graph_result(apContainer->mpGraph, vIdx.data(), lCount);

		mvNodes.reserve(lCount);
		for(size_t i=0; i<lCount; ++i)
		{
			mvNodes.push_back(apContainer->mvNodes[vIdx[i]]);
		}
	}

	//-----------------------------------------------------------------------

	bool cAINodeIterator::HasNext()
	{
		return mlPos < mvNodes.size();
	}

	//-----------------------------------------------------------------------

	cAINode *cAINodeIterator::Next()
	{
		if(mlPos >= mvNodes.size()) return NULL;

		return mvNodes[mlPos++];
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool (*cAINodeContainer::mpTestCast)(unsigned int, const cVector3f*, const cVector3f*) = NULL;

	cAINodeContainer::cAINodeContainer(	const tString& asName, const tString &asNodeName, 
										cWorld *apWorld, const cVector3f &avCollideSize)
	{
		mpWorld = apWorld;
		mvSize = avCollideSize;
		msName = asName;
		msNodeName = asNodeName;

		mpRayCallback = hplNew( cAINodeRayCallback, () );
		mpFreePathCallback = NULL;
		mpGraph = ohpl_ai_graph_new(CastRay, this, &mvSize);
		mbRsMaps = OhplBackendRustRequested();

		mlMaxNodeEnds = 5;
		mlMinNodeEnds = 2;
		mfMaxEndDistance = 3.0f;
		mfMaxHeight = 0.1f;

		mbNodeIsAtCenter = true;

		SyncParams();
	}

	//-----------------------------------------------------------------------

	cAINodeContainer::~cAINodeContainer()
	{
		hplDelete(mpRayCallback);
		ohpl_ai_graph_free(mpGraph);

		STLDeleteAll(mvNodes);
	}	

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cAINodeContainer::ReserveSpace(size_t alReserveSpace)
	{
		mvNodes.reserve(alReserveSpace);
	}

	//-----------------------------------------------------------------------
	
	void cAINodeContainer::AddNode(const tString &asName, int alID, const cVector3f &avPosition, void *apUserData)
	{
		cAINode *pNode = hplNew( cAINode, () );
		pNode->msName = asName;
		pNode->mlID = alID;
		pNode->mvPosition = avPosition;
		pNode->mpUserData = apUserData;
		pNode->mlIdx = (int)mvNodes.size();
		pNode->mpGraph = mpGraph;
		ohpl_ai_graph_add_node(mpGraph, &avPosition, (uint64_t)(uintptr_t)pNode);

		mvNodes.push_back(pNode);
		if(mbRsMaps) ohpl_ai_graph_name_node(mpGraph, (uint32_t)pNode->mlIdx, (const unsigned char*)asName.data(), asName.size(), alID);
		else
		{
			m_mapNodesByName.insert(tAINodeNameMap::value_type(asName,pNode));
			m_mapNodesByID.insert(tAINodeIDMap::value_type(alID,pNode));
		}
	}

	//-----------------------------------------------------------------------

	int cAINodeContainer::GetNodeNum() const
	{
		return (int)mvNodes.size();
	}

	//-----------------------------------------------------------------------

	cAINode* cAINodeContainer::GetNodeFromName(const tString &asName)
	{
		if(mbRsMaps)
		{
			int64_t lIdx = ohpl_ai_graph_find_name(mpGraph, (const unsigned char*)asName.data(), asName.size());
			return lIdx < 0 ? NULL : mvNodes[lIdx];
		}
		tAINodeNameMapIt it = m_mapNodesByName.find(asName);
		if(it == m_mapNodesByName.end())
		{
			return NULL;
		}
		return it->second;
	}
	
	//-----------------------------------------------------------------------

	cAINode* cAINodeContainer::GetNodeFromID(int alID)
	{
		if(mbRsMaps)
		{
			int64_t lIdx = ohpl_ai_graph_find_id(mpGraph, alID);
			return lIdx < 0 ? NULL : mvNodes[lIdx];
		}
		tAINodeIDMapIt it = m_mapNodesByID.find(alID);
		if(it == m_mapNodesByID.end())
		{
			return NULL;
		}
		return it->second;
	}

	//-----------------------------------------------------------------------

	void cAINodeContainer::Compile()
	{
		ohpl_ai_graph_compile(mpGraph);

		for(size_t i=0; i<mvNodes.size(); ++i)
		{
			cAINode *pNode = mvNodes[i];
			uint32_t lCount = ohpl_ai_graph_edge_count(mpGraph, (uint32_t)i);
			pNode->mvEdges.clear();
			for(uint32_t e=0; e<lCount; ++e)
			{
				uint32_t lTo;
				cAINodeEdge Edge;
				ohpl_ai_graph_edge(mpGraph, (uint32_t)i, e, &lTo, &Edge.mfDistance, &Edge.mfSqrDistance);
				Edge.mpNode = mvNodes[lTo];
				pNode->mvEdges.push_back(Edge);
			}
		}
	}

	//-----------------------------------------------------------------------

	void cAINodeContainer::BuildNodeGridMap()
	{
		ohpl_ai_graph_build_grid(mpGraph);
	}

	//-----------------------------------------------------------------------

	cAINodeIterator cAINodeContainer::GetNodeIterator(const cVector3f &avPosition, float afRadius)
	{
		return cAINodeIterator(this,avPosition, afRadius);
	}

	//-----------------------------------------------------------------------

	bool cAINodeContainer::CastRay(void *apContainer, unsigned int alFlags, const cVector3f *apStart, const cVector3f *apEnd)
	{
		cAINodeContainer *pContainer = (cAINodeContainer*)apContainer;
		if(pContainer->mpWorld==NULL) return mpTestCast ? mpTestCast(alFlags, apStart, apEnd) : false;
		iPhysicsWorld *pPhysicsWorld = pContainer->mpWorld->GetPhysicsWorld();
		if(pPhysicsWorld==NULL) return false;

		cAINodeRayCallback *pRayCallback = pContainer->mpRayCallback;
		pRayCallback->SetFlags(alFlags);
		pRayCallback->Reset();
		pRayCallback->mpCallback = pContainer->mpFreePathCallback;

		pPhysicsWorld->CastRay(pRayCallback,*apStart,*apEnd,false,false,false,true);

		return pRayCallback->Intersected();
	}

	//-----------------------------------------------------------------------

	bool cAINodeContainer::FreePath(const cVector3f &avStart, const cVector3f &avEnd, int alRayNum,
									tAIFreePathFlag aFlags,iAIFreePathCallback *apCallback)
	{
		iAIFreePathCallback *pPrevCallback = mpFreePathCallback;
		mpFreePathCallback = apCallback;
		bool bFree = ohpl_ai_graph_free_path(mpGraph, &avStart, &avEnd, alRayNum, aFlags);
		mpFreePathCallback = pPrevCallback;

		return bFree;
	}

	//-----------------------------------------------------------------------

	void cAINodeContainer::SyncParams()
	{
		ohpl_ai_graph_set_params(mpGraph, mlMaxNodeEnds, mlMinNodeEnds, mfMaxEndDistance, mfMaxHeight, mbNodeIsAtCenter);
	}

	//-----------------------------------------------------------------------
	
	void cAINodeContainer::SaveToFile(const tWString &asFile)
	{
		TiXmlDocument* pXmlDoc = hplNew( TiXmlDocument,() );
		
		TiXmlElement *pRootElem = static_cast<TiXmlElement*>(pXmlDoc->InsertEndChild(TiXmlElement("AINodes")));

		for(size_t i=0; i< mvNodes.size(); ++i)
		{
			cAINode * pNode = mvNodes[i];
			TiXmlElement *pNodeElem = static_cast<TiXmlElement*>(pRootElem->InsertEndChild(TiXmlElement("Node")));

			pNodeElem->SetAttribute("Name", pNode->GetName().c_str());
			pNodeElem->SetAttribute("ID", cString::ToString(pNode->GetID()).c_str());

			for(int edge =0; edge < pNode->GetEdgeNum(); ++edge)
			{
				cAINodeEdge *pEdge = pNode->GetEdge(edge);
				TiXmlElement *pEdgeElem = static_cast<TiXmlElement*>(pNodeElem->InsertEndChild(TiXmlElement("Edge")));

				pEdgeElem->SetAttribute("Node", pEdge->mpNode->GetName().c_str());
				tString sDistance = cString::ToString(pEdge->mfDistance);
				pEdgeElem->SetAttribute("Distance", sDistance.c_str());
			}
		}

		FILE *pFile = cPlatform::OpenFile(asFile, _W("w+"));
		if(pFile==NULL || pXmlDoc->SaveFile(pFile)==false)
		{
			Error("Couldn't save XML file %s\n",asFile.c_str());
			hplDelete(pXmlDoc);
			return;
		}
		fclose(pFile);
		hplDelete(pXmlDoc);
	}
	
	//-----------------------------------------------------------------------
	
	void cAINodeContainer::LoadFromFile(const tWString &asFile)
	{
		BuildNodeGridMap();

		FILE *pFile = cPlatform::OpenFile(asFile, _W("rb"));
		if(pFile==NULL) return;
		
		TiXmlDocument* pXmlDoc = hplNew( TiXmlDocument, () );
		if(pXmlDoc->LoadFile(pFile)==false)
		{
			Warning("Couldn't open XML file %s\n",cString::To8Char(asFile).c_str());
			fclose(pFile);
			hplDelete(pXmlDoc);
			return;
		}
		fclose(pFile);

		TiXmlElement *pRootElem = pXmlDoc->RootElement();

		TiXmlElement *pNodeElem = pRootElem->FirstChildElement("Node");
		for(; pNodeElem != NULL; pNodeElem = pNodeElem->NextSiblingElement("Node"))
		{
			tString sName = cString::ToString(pNodeElem->Attribute("Name"),"");
			int alID = cString::ToInt(pNodeElem->Attribute("ID"),-1);

			cAINode *pNode = GetNodeFromID(alID);
			if(pNode==NULL) continue;
            
			TiXmlElement *pEdgeElem = pNodeElem->FirstChildElement("Edge");
			for(; pEdgeElem != NULL; pEdgeElem = pEdgeElem->NextSiblingElement("Edge"))
			{
				tString sNodeName = cString::ToString(pEdgeElem->Attribute("Node"),"");			
				cAINode *pEdgeNode = GetNodeFromName(sNodeName);
				if(pEdgeNode==NULL) continue;

				cAINodeEdge Edge;
				Edge.mpNode = pEdgeNode;
				Edge.mfDistance = cString::ToFloat(pEdgeElem->Attribute("Distance"),0);
				Edge.mfSqrDistance = Edge.mfDistance*Edge.mfDistance;

				pNode->mvEdges.push_back(Edge);
				ohpl_ai_graph_add_edge(mpGraph, (uint32_t)pNode->mlIdx, (uint32_t)pEdgeNode->mlIdx, Edge.mfDistance, Edge.mfSqrDistance);
			}
		}

		hplDelete(pXmlDoc);
	}
	//-----------------------------------------------------------------------
	
}
