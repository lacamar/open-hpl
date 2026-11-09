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

#include "math/Quaternion.h"

#include "math/Math.h"
#include "math/RustMath.h"
#include "system/LowLevelSystem.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cQuaternion::cQuaternion()
	{
	}

	//-----------------------------------------------------------------------

	cQuaternion::cQuaternion(float afAngle, const cVector3f & avAxis)
	{
		FromAngleAxis(afAngle,avAxis);
	}

	//-----------------------------------------------------------------------

	cQuaternion::cQuaternion(float afW, float afX, float afY, float afZ)
	{
		w = afW;
		v.x = afX;
		v.y = afY;
		v.z = afZ;
	}

	//-----------------------------------------------------------------------
	cQuaternion::cQuaternion(const cMatrix<float> &a_mtxRot)
	{
		FromRotationMatrix(a_mtxRot);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////


	const cQuaternion cQuaternion::Identity = cQuaternion(1.0f,0.0f,0.0f,0.0f);

	//-----------------------------------------------------------------------

	void cQuaternion::Normalize() { ohpl_quaternion_normalize(this); }

	void cQuaternion::ToRotationMatrix(cMatrixf &a_mtxDest) const { ohpl_quaternion_to_rotation_matrix(this, &a_mtxDest); }

	void cQuaternion::FromRotationMatrix(const cMatrix<float> &a_mtxRot) { ohpl_quaternion_from_rotation_matrix(this, &a_mtxRot); }

	void cQuaternion::FromAngleAxis(float afAngle, const cVector3f &avAxis) { ohpl_quaternion_from_angle_axis(this, afAngle, &avAxis); }

	cQuaternion cQuaternion::operator+(const cQuaternion &aqB) const
	{
		cQuaternion qOut;
		ohpl_quaternion_add(this, &aqB, &qOut);
		return qOut;
	}

	cQuaternion cQuaternion::operator-(const cQuaternion &aqB) const
	{
		cQuaternion qOut;
		ohpl_quaternion_sub(this, &aqB, &qOut);
		return qOut;
	}

	cQuaternion cQuaternion::operator*(const cQuaternion &aqB) const
	{
		cQuaternion qOut;
		ohpl_quaternion_mul(this, &aqB, &qOut);
		return qOut;
	}

	cQuaternion cQuaternion::operator/(const cQuaternion &aqB) const
	{
		cQuaternion qOut;
		ohpl_quaternion_div(this, &aqB, &qOut);
		return qOut;
	}

	cQuaternion cQuaternion::operator*(float afScalar) const
	{
		cQuaternion qOut;
		ohpl_quaternion_scale(this, afScalar, &qOut);
		return qOut;
	}
}
