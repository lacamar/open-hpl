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

#ifndef HPL_AI_NODE_CONTAINER_H
#define HPL_AI_NODE_CONTAINER_H

#include "system/SystemTypes.h"
#include "math/MathTypes.h"

#include "physics/PhysicsWorld.h"
#include "ai/RustAI.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "system/RustXml.h"

namespace hpl {

	class cWorld;

	//--------------------------------
	
	typedef tFlag tAIFreePathFlag;

	#define eAIFreePathFlag_SkipStatic	 (0x00000001)
	#define eAIFreePathFlag_SkipDynamic	 (0x00000002)
	#define eAIFreePathFlag_SkipVolatile (0x00000004)

	
	//--------------------------------
	class cAINode;

	// Layout twins of rust/src/ai CEdge and CNode; Rust owns the objects.
	class cAINodeEdge
	{
	public:
		float mfDistance;
		float mfSqrDistance;
		cAINode *mpNode;
	};

	class cAINode
	{
	public:
		int GetIdx() const { return mlIdx;}

		int GetEdgeNum() const { return (int)mlEdgeNum;}
		inline cAINodeEdge* GetEdge(int alIdx) { return &mpEdges[alIdx];}

		const cVector3f& GetPosition(){ return mvPosition;}

		tString GetName(){ return tString(mpName, mlNameLen);}
		int GetID(){ return mlID; }

	private:
		cAINode();
		int mlID;
		int mlIdx;
		cVector3f mvPosition;
		const char *mpName;
		size_t mlNameLen;
		cAINodeEdge *mpEdges;
		size_t mlEdgeNum;
	};
	static_assert(sizeof(cAINode) == 56 && sizeof(cAINodeEdge) == 16, "rust/src/ai CNode layout");

	typedef std::vector<cAINode*> tAINodeVec;
	typedef tAINodeVec::iterator tAINodeVecIt;

	typedef std::list<cAINode*> tAINodeList;
	typedef tAINodeList::iterator tAINodeListIt;

	class iAIFreePathCallback
	{
	public:
		virtual ~iAIFreePathCallback() { }
		virtual bool Intersects(iPhysicsBody *pBody,cPhysicsRayParams *apParams)=0;
	};

	//--------------------------------

	class cAINodeRayCallback : public iPhysicsRayCallback
	{
	public:
		virtual ~cAINodeRayCallback(){}

		void Reset();
		void SetFlags(tAIFreePathFlag aFlags){ mFlags = aFlags;}
		
		bool BeforeIntersect(iPhysicsBody *pBody);
		bool OnIntersect(iPhysicsBody *pBody,cPhysicsRayParams *apParams);
		
		bool Intersected();

		iAIFreePathCallback *mpCallback;
	
	private:
		bool mbIntersected;
		tAIFreePathFlag mFlags;
	};

	//--------------------------------
	
	//--------------------------------
	class cAINodeContainer;

	class cAINodeIterator
	{
	public:
		cAINodeIterator(cAINodeContainer *apContainer, const cVector3f &avPos, float afRadius);

		bool HasNext(){ return mlPos < mvNodes.size();}
		cAINode *Next(){ return mlPos < mvNodes.size() ? mvNodes[mlPos++] : NULL;}

	private:
		std::vector<cAINode*> mvNodes;
		size_t mlPos;
	};

		
	class cAINodeContainer
	{
	friend class cAINodeIterator;
	friend class cAStarHandler;
	public:
		cAINodeContainer(	const tString& asName,const tString &asNodeName,
							cWorld *apWorld, const cVector3f &avCollideSize);
		~cAINodeContainer();

		const tString& GetNodeName(){ return msNodeName;}
		const tString& GetName(){ return msName;}

		const cVector3f& GetCollideSize(){ return mvSize;}
		
		/**
		 * Reserves spaces for nodes.
		 * \param alReserveSpace Number of nodes to reserve space for.
		 */
		void ReserveSpace(size_t alReserveSpace){}

		/**
		* Adds a new node to the container.
		* \param &asName Name of the node
		* \param &avPosition Position of the node.
		* \param *apUserData Data supplied by user.
         */
        void AddNode(const tString &asName, int alID, const cVector3f &avPosition, void *apUserData=NULL)
		{
			ohpl_ai_graph_add(mpGraph, &avPosition, (const unsigned char*)asName.data(), asName.size(), alID);
		}

		/**
		 * Get the number of nodes.
		 */
		int GetNodeNum() const { return (int)ohpl_ai_graph_node_count(mpGraph);}

		/**
		 * Get a node.
		 * \param alIdx index of node.
		 */
		inline cAINode* GetNode(int alIdx){ return (cAINode*)ohpl_ai_graph_node_at(mpGraph, (uint32_t)alIdx);}

		/**
		 * Gets a node based on the name.
		 * \param &asName Name of the node.
		 */
		cAINode* GetNodeFromName(const tString &asName)
		{
			return (cAINode*)ohpl_ai_graph_node_by_name(mpGraph, (const unsigned char*)asName.data(), asName.size());
		}
		cAINode* GetNodeFromID(int alID){ return (cAINode*)ohpl_ai_graph_node_by_id(mpGraph, alID);}


		/**
		 * Compile the added nodes.
		 */
		void Compile(){ ohpl_ai_graph_compile(mpGraph);}

		/**
		 * Build a grid map for nodes. (Used internally mostly)
		 */
		void BuildNodeGridMap(){ ohpl_ai_graph_build_grid(mpGraph);}

		/**
		 * Returns a node iterator. Note that the radius is not checked, some nodes may lie outside.
		 * \param &avPosition 
		 * \param afRadius 
		 * \return 
		 */
		cAINodeIterator GetNodeIterator(const cVector3f &avPosition, float afRadius){ return cAINodeIterator(this,avPosition, afRadius);}

		/**
		 * Checks for a free path using the containers collide size.
		 * \param &avStart 
		 * \param &avEnd 
		 * \param alRayNum The max number of rays cast, -1 = maximum
		 * \param alFlags Set Flags for the ray casting.
		 * \param apCallback Check for every body and overrides alFlags.
		 * \return 
		 */
		bool FreePath(const cVector3f &avStart, const cVector3f &avEnd, int alRayNum=-1, 
						tAIFreePathFlag aFlags=0, iAIFreePathCallback *apCallback=NULL)
		{
			iAIFreePathCallback *pPrevCallback = mpFreePathCallback;
			mpFreePathCallback = apCallback;
			bool bFree = ohpl_ai_graph_free_path(mpGraph, &avStart, &avEnd, alRayNum, aFlags);
			mpFreePathCallback = pPrevCallback;
			return bFree;
		}


		/**
		 * Sets the max number of end node added to a node.
		 * \param alX The max number, -1 = unlimited
		 */
		void SetMaxEdges(int alX){ mlMaxNodeEnds = alX; SyncParams();}

		/**
		* Sets the min number of end node added to a node. This overrides max distance when needed.
		*/
		void SetMinEdges(int alX){ mlMinNodeEnds = alX; SyncParams();}
		
		/**
		 * Sets the max distance for an end node.
		 * \param afX 
		 */
		void SetMaxEdgeDistance(float afX){ mfMaxEndDistance = afX; SyncParams();}

		float GetMaxEdgeDistance() const { return mfMaxEndDistance;}

		static bool (*mpTestCast)(unsigned int, const cVector3f*, const cVector3f*);

		void SetMaxHeight(float afX){ mfMaxHeight = afX; SyncParams();}
		float GetMaxHeight()const{ return mfMaxHeight;}

		/**
		 * When calculating if there is a free path between two nodes. Is the node postion the center of the collider.
		 * If not the position is the feet postion.
		 */
		void SetNodeIsAtCenter(bool abX){ mbNodeIsAtCenter = abX; SyncParams();}
		bool GetNodeIsAtCenter(){ return mbNodeIsAtCenter;}


		/**
		 * Saves all the node connections to file.
		 */
		void SaveToFile(const tWString &asFile)
		{
			tString sFile = cString::To8Char(asFile);
			if(ohpl_ai_graph_save(mpGraph, (const unsigned char*)sFile.data(), sFile.size())==false)
				Error("Couldn't save XML file %s\n",asFile.c_str());
		}
		/**
		* Loads all node connections from file. Only to be done after all nodes are loaded.
		*/
		void LoadFromFile(const tWString &asFile)
		{
			tString sFile = cString::To8Char(asFile), sLog;
			if(ohpl_ai_graph_load(mpGraph, (const unsigned char*)sFile.data(), sFile.size(), OhplXmlAssign, &sLog))
			{
				Log("%s", sLog.c_str());
				Warning("Couldn't open XML file %s\n", sFile.c_str());
			}
		}

	private:
		void SyncParams(){ ohpl_ai_graph_set_params(mpGraph, mlMaxNodeEnds, mlMinNodeEnds, mfMaxEndDistance, mfMaxHeight, mbNodeIsAtCenter);}
		static bool CastRay(void *apContainer, unsigned int alFlags, const cVector3f *apStart, const cVector3f *apEnd);

		tString msName;
		tString msNodeName;

		cWorld *mpWorld;
		cVector3f mvSize;

		cAINodeRayCallback *mpRayCallback;

		bool mbNodeIsAtCenter;

		OhplAiGraph *mpGraph;
		iAIFreePathCallback *mpFreePathCallback;

		//properties
		int mlMaxNodeEnds;
		int mlMinNodeEnds;
		float mfMaxEndDistance;
		float mfMaxHeight;
	};

	inline cAINodeIterator::cAINodeIterator(cAINodeContainer *apContainer, const cVector3f &avPos, float afRadius)
		: mlPos(0)
	{
		mvNodes.resize(ohpl_ai_graph_query(apContainer->mpGraph, &avPos, afRadius));
		ohpl_ai_graph_result_nodes(apContainer->mpGraph, (OhplAiNode**)mvNodes.data(), mvNodes.size());
	}

};
#endif // HPL_AI_NODE_CONTAINER_H
