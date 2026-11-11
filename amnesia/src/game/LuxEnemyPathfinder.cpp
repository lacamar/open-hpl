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

#include "LuxEnemyPathfinder.h"

#include "LuxEnemy.h"
#include "LuxEnemyMover.h"
#include "LuxMap.h"

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEnemyPathfinder::cLuxEnemyPathfinder(iLuxEnemy *apEnemy, cLuxEnemyMover *apMover)
{
	mpEnemy = apEnemy;
	mpMover = apMover;

	mbMoving = false;

	mpAStar = NULL;
	mpNodeContainer = NULL;
	mpGa3ExplicitSkip = NULL;
	mlGa3SkipMode = 2;
	mWin.len = 0;
}

//-----------------------------------------------------------------------

cLuxEnemyPathfinder::~cLuxEnemyPathfinder()
{
	cWorld *pWorld = mpEnemy->mpMap->GetWorld();
	if(mpAStar) pWorld->DestroyAStarHandler(mpAStar);	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEnemyPathfinder::SetupAfterLoad(cWorld *apWorld)
{
	
}

//-----------------------------------------------------------------------

void cLuxEnemyPathfinder::AfterWorldLoad()
{
	cWorld *pWorld = mpEnemy->mpMap->GetWorld();

	tString sType = cString::SetFileExt(cString::GetFileName(mpEnemy->msFileName),"");
	mpNodeContainer = pWorld->CreateAINodeContainer( sType, "Default", mpEnemy->mpCharBody->GetSize(), false, 2 , 6 , 5.0f,0.41f);
	if(mpNodeContainer==NULL)
		Error("No node container found for enemy '%s'\n", mpEnemy->GetName().c_str());

	if(mpNodeContainer)
		mpAStar = pWorld->CreateAStarHandler(mpNodeContainer);
	else
		mpAStar = NULL;
}

//-----------------------------------------------------------------------

bool cLuxEnemyPathfinder::MoveTo(const cVector3f& avPos)
{
	///////////////////////
	// Set up data
	iCharacterBody *pCharBody = mpEnemy->mpCharBody;

	mWin.len = 0;
	mlstPathNodes.clear();
	mbMoving = true;
	
	/////////////////////////////////////
	//Get the start and goal position
	cVector3f vStartPos = pCharBody->GetPosition();
	mvMoveGoalPos = avPos;

	/////////////////////////////////////
	//No path finding just go straight to goal.
	if(mpAStar==NULL)
	{
		return false;
	}
	

	//If node is not at center, the nodes are assumed to be at feet, adjust for this!
	if(mpNodeContainer==NULL || mpNodeContainer->GetNodeIsAtCenter()==false)
	{
		vStartPos -= cVector3f(0,pCharBody->GetSize().y/2.0f,0);
	}

	vStartPos.y += 0.01f;

	/////////////////////////////////
	//Get the nodes of the path
	bool bRet = mpAStar->GetPath(vStartPos,mvMoveGoalPos,&mlstPathNodes);

	if(bRet==false)
	{
		//Log("Could not find path!\n");
		//TODO: Debug output
	}

	
	return bRet;
}

//-----------------------------------------------------------------------

void cLuxEnemyPathfinder::Stop()
{
	mbMoving = false;
	mlstPathNodes.clear();
	mWin.len = 0;
}

cAINode* cLuxEnemyPathfinder::GetNodeAtPos(const cVector3f &avPos,float afMinDistance,float afMaxDistance,bool abGetClosest, 
											bool abPosToNodeFreePathCheck,bool abEnemyToNodeFreePathCheck,
											cAINode *apSkipNode,
											int alFreePathRayNum, tAIFreePathFlag alFreePathFlags,
											bool abSkipUsedNodes)
{
	if(mpNodeContainer==NULL) return NULL;	

	mpGa3ExplicitSkip = apSkipNode;
	OhplGa3Host host = mpEnemy->Ga3Host();
	cVector3f vNodePos;
	int lIdx = ohpl_ga3_node_at_pos(&host, &avPos.x, afMinDistance, afMaxDistance, abGetClosest, abPosToNodeFreePathCheck, abEnemyToNodeFreePathCheck,
									2, alFreePathRayNum, abSkipUsedNodes, &vNodePos.x);
	if(lIdx < 0) return NULL;
	return mvGa3Nodes[lIdx];
}

//-----------------------------------------------------------------------

void cLuxEnemyPathfinder::OnUpdate(float afTimeStep)
{
	//TODO: Check if goal position is reached!
	//TODO: Check if Path end is reached. If so notify higher level
	UpdateMoving(afTimeStep);
}


//-----------------------------------------------------------------------

void cLuxEnemyPathfinder::OnRenderSolid(cRendererCallbackFunctions* apFunctions)
{
	if(mpNodeContainer==NULL) return;

	///////////////////////
	//Render all nodes
	for(int i=0; i<mpNodeContainer->GetNodeNum(); ++i)
	{
		cAINode *pNode = mpNodeContainer->GetNode(i);

		apFunctions->GetLowLevelGfx()->DrawSphere(pNode->GetPosition(), 0.3f,cColor(0.4f,1));

		for(int j=0; j < pNode->GetEdgeNum(); ++j)
		{
			cAINodeEdge *pEdge = pNode->GetEdge(j);

			apFunctions->GetLowLevelGfx()->DrawLine(pNode->GetPosition(),pEdge->mpNode->GetPosition(),cColor(0.4f,0.4f,0.4f,1));
		}
	}

	///////////////////////
	//Render current goal position
	if(mbMoving==false) return;

	//////////////////////////////
	//GoalPos
	cVector3f vGoalPos = mvMoveGoalPos;
	if(mpNodeContainer->GetNodeIsAtCenter()==false)
	{
		vGoalPos += cVector3f(0,mpNodeContainer->GetCollideSize().y/2, 0);
	}

	apFunctions->GetLowLevelGfx()->DrawSphere(vGoalPos,0.2f, cColor(1,0,1));
	cVector3f vLastVec = vGoalPos;

	//////////////////////////////
	//Nodes
	tAINodeListIt it = mlstPathNodes.begin();
	for(; it != mlstPathNodes.end(); ++it)
	{
		cAINode *pNode = *it;

		cVector3f vNodePos = pNode->GetPosition();
		if(mpNodeContainer->GetNodeIsAtCenter()==false)
		{
			vNodePos += cVector3f(0,mpNodeContainer->GetCollideSize().y/2, 0);
		}

		apFunctions->GetLowLevelGfx()->DrawSphere(vNodePos,0.2f, cColor(1,0,1));
		apFunctions->GetLowLevelGfx()->DrawLine(vLastVec, vNodePos,cColor(1,0,1));

		vLastVec = vNodePos;
	}

	//////////////////////////////
	//Start pos
	cVector3f vStartPos = mpEnemy->mpCharBody->GetPosition();
	//if(mpContainer->GetNodeIsAtCenter()==false){
	//vStartPos += cVector3f(0,mpContainer->GetCollideSize().y/2, 0);
	//}
	apFunctions->GetLowLevelGfx()->DrawSphere(vStartPos,0.2f,cColor(1,0,1));
	apFunctions->GetLowLevelGfx()->DrawLine(vLastVec, vStartPos,cColor(1,0,1));

	
}

cVector3f  cLuxEnemyPathfinder::GetNextGoalPos()
{
	OhplGa3Host host = mpEnemy->Ga3Host();
	cVector3f vPos;
	ohpl_ga3_next_goal_pos(&host, &mvMoveGoalPos.x, &vPos.x);
	return vPos;
}

const cVector3f& cLuxEnemyPathfinder::GetFinalGoalPos()
{
	return mvMoveGoalPos;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEnemyPathfinder::UpdateMoving(float afTimeStep)
{
	OhplGa3Host host = mpEnemy->Ga3Host();
	ohpl_ga3_update_moving(&mpEnemy->mGa3Ctx, &host);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxEnemyPathfinder_SaveData)

kSerializeVar(mbMoving, eSerializeType_Bool)
kSerializeVar(mvMoveGoalPos, eSerializeType_Vector3f)

kSerializeVarContainer(mvPathNodeIds, eSerializeType_Int32)

kEndSerialize()

//-----------------------------------------------------------------------

void cLuxEnemyPathfinder_SaveData::FromPathfinder(cLuxEnemyPathfinder *apPathfinder)
{
	mbMoving = apPathfinder->mbMoving;
	mvMoveGoalPos = apPathfinder->mvMoveGoalPos;

	for(tAINodeListIt it = apPathfinder->mlstPathNodes.begin(); it != apPathfinder->mlstPathNodes.end(); ++it)
	{
		cAINode *pNode = *it;

        mvPathNodeIds.Add(pNode->GetID());
	}
}

//-----------------------------------------------------------------------

void cLuxEnemyPathfinder_SaveData::ToPathfinder(cLuxEnemyPathfinder *apPathfinder)
{
	apPathfinder->mbMoving = mbMoving;
	apPathfinder->mvMoveGoalPos = mvMoveGoalPos;
}

//-----------------------------------------------------------------------

void cLuxEnemyPathfinder_SaveData::SetupPathfinder(cLuxEnemyPathfinder *apPathfinder)
{
	if(apPathfinder->mpNodeContainer==NULL) return;

	for(size_t i=0; i<mvPathNodeIds.Size(); ++i)
	{
		cAINode *pNode = apPathfinder->mpNodeContainer->GetNodeFromID(mvPathNodeIds[i]);
		if(pNode) apPathfinder->mlstPathNodes.push_back(pNode);
	}	
}


//-----------------------------------------------------------------------
