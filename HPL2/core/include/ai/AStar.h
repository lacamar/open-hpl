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

#ifndef HPL_A_STAR_H
#define HPL_A_STAR_H

#include "system/SystemTypes.h"
#include "engine/EngineTypes.h"
#include "math/MathTypes.h"

namespace hpl {

	class cAINodeContainer;
	class cAINode;

	//--------------------------------------

	typedef std::set<cAINode*> tAINodeSet;
	typedef tAINodeSet::iterator tAINodeSetIt;

	//--------------------------------------

	typedef std::list<cAINode*> tAINodeList;
	typedef tAINodeList::iterator tAINodeListIt;

	//--------------------------------------

	class cAStarHandler;

	class iAStarCallback
	{
	public:
		virtual ~iAStarCallback(){}
		
		virtual bool CanAddNode(cAINode *apParentNode,cAINode *apChildNode)=0;
	};

	//--------------------------------------

	class cAStarHandler
	{
	public:
		cAStarHandler(cAINodeContainer *apContainer);
		~cAStarHandler();
		
		bool GetPath(const cVector3f& avStart, const cVector3f& avGoal, tAINodeList *apNodeList);

		/**
		 * Set max number of times the algorithm is iterated.
		 * \param alX -1 = until OpenList is empty
		 */
		void SetMaxIterations(int alX){ mlMaxIterations = alX;}

		void SetCallback(iAStarCallback *apCallback){ mpCallback = apCallback;}

	private:
		static bool CanAddNode(void *apHandler, unsigned int alParent, unsigned int alChild);

		cAINodeContainer *mpContainer;

		int mlMaxIterations;

		iAStarCallback *mpCallback;
	};

};
#endif // HPL_A_STAR_H
