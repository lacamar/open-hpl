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

#ifndef HPL_BONE_STATE_H
#define HPL_BONE_STATE_H

#include "scene/Node3D.h"
#include "math/Math.h"

namespace hpl {

	class iPhysicsBody;

	class cBoneState : public cNode3D
	{
	public:
		cBoneState(const tString &asName, bool abAutoDeleteChildren) : cNode3D(asName,abAutoDeleteChildren){}

		void SetBody(iPhysicsBody *apBody){ mpBody = apBody;}
		iPhysicsBody* GetBody(){ return mpBody;}

		void SetColliderBody(iPhysicsBody *apBody){ mpColliderBody = apBody;}
		iPhysicsBody* GetColliderBody(){ return mpColliderBody;}

		void SetBodyMatrix(const cMatrixf &a_mtxBody){ m_mtxBody = a_mtxBody; m_mtxInvBody = cMath::MatrixInverse(a_mtxBody);}
		const cMatrixf& GetBodyMatrix(){ return m_mtxBody;}
		const cMatrixf& GetInvBodyMatrix(){ return m_mtxInvBody;}

	private:
		iPhysicsBody *mpBody = NULL;
		cMatrixf m_mtxBody;
		cMatrixf m_mtxInvBody;
		iPhysicsBody *mpColliderBody = NULL;
	};
};
#endif // HPL_BONE_STATE_H
