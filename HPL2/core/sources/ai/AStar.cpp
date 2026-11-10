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

#include "ai/AStar.h"

#include "ai/AINodeContainer.h"

#include "math/Math.h"

#include "system/LowLevelSystem.h"


namespace hpl {

	//-----------------------------------------------------------------------

	cAStarHandler::cAStarHandler(cAINodeContainer *apContainer)
	{
		mlMaxIterations = -1;

		mpContainer = apContainer;

		mpCallback = NULL;
	}

	//-----------------------------------------------------------------------

	cAStarHandler::~cAStarHandler()
	{
	}

	//-----------------------------------------------------------------------

	bool cAStarHandler::CanAddNode(void *apHandler, unsigned int alParent, unsigned int alChild)
	{
		cAStarHandler *pHandler = (cAStarHandler*)apHandler;

		return pHandler->mpCallback->CanAddNode(	pHandler->mpContainer->mvNodes[alParent],
													pHandler->mpContainer->mvNodes[alChild]);
	}

	//-----------------------------------------------------------------------

	bool cAStarHandler::GetPath(const cVector3f& avStart, const cVector3f& avGoal,tAINodeList *apNodeList)
	{
		OhplAiGraph *pGraph = mpContainer->mpGraph;

		if(ohpl_ai_graph_astar(pGraph, &avStart, &avGoal, mlMaxIterations, mpCallback ? CanAddNode : NULL, this)==false)
		{
			return false;
		}

		if(apNodeList)
		{
			size_t lCount = ohpl_ai_graph_result(pGraph, NULL, 0);
			std::vector<uint32_t> vIdx(lCount);
			ohpl_ai_graph_result(pGraph, vIdx.data(), lCount);

			for(size_t i=0; i<lCount; ++i)
			{
				apNodeList->push_back(mpContainer->mvNodes[vIdx[i]]);
			}
		}

		return true;
	}

	//-----------------------------------------------------------------------
}
