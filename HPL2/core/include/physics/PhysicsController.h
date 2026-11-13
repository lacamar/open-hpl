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

#ifndef HPL_PHYSICS_CONTROLLER_H
#define HPL_PHYSICS_CONTROLLER_H

#include "math/MathTypes.h"
#include "math/PidController.h"
#include "math/Spring.h"

#include "engine/SaveGame.h"

struct OhplPid;

namespace hpl {

	//-------------------------------------------

	enum ePhysicsControllerType
	{
		ePhysicsControllerType_Pid,
		ePhysicsControllerType_Spring,
		ePhysicsControllerType_LastEnum
	};

	//-------------------------------------------

	enum ePhysicsControllerInput
	{
		ePhysicsControllerInput_JointAngle,
		ePhysicsControllerInput_JointDist,
		ePhysicsControllerInput_LinearSpeed,
		ePhysicsControllerInput_AngularSpeed,
		ePhysicsControllerInput_LastEnum
	};

	//-------------------------------------------

	enum ePhysicsControllerOutput
	{
		ePhysicsControllerOutput_Force,
		ePhysicsControllerOutput_Torque,
		ePhysicsControllerOutput_LastEnum
	};

	//-------------------------------------------

	enum ePhysicsControllerAxis
	{
		ePhysicsControllerAxis_X,
		ePhysicsControllerAxis_Y,
		ePhysicsControllerAxis_Z,
		ePhysicsControllerAxis_LastEnum
	};

	//-------------------------------------------

	enum ePhysicsControllerEnd
	{
		ePhysicsControllerEnd_Null,
		ePhysicsControllerEnd_OnDest,
		ePhysicsControllerEnd_OnMin,
		ePhysicsControllerEnd_OnMax,
		ePhysicsControllerEnd_LastEnum
	};

	//-------------------------------------------

	class iPhysicsWorld;
	class iPhysicsJoint;
	class iPhysicsBody;

	class iPhysicsController;
	class iPhysicsJoint;
	class iPhysicsBody;
	// Controller state, owned by rust/src/physics/controller.rs (layout of OhplCtrlParams); the members are references into it.
	struct OhplCtrlState
	{
		iPhysicsController *self;
		iPhysicsBody *body;
		iPhysicsJoint *joint;
		bool active, paused, log, mulMass;
		ePhysicsControllerType kind;
		ePhysicsControllerInput input;
		ePhysicsControllerAxis inputAxis;
		ePhysicsControllerOutput output;
		ePhysicsControllerAxis outputAxis;
		ePhysicsControllerEnd end;
		float a, b, c, dest, maxOutput;
		const char *nextP;
		size_t nextN;
	};
	static_assert(sizeof(OhplCtrlState) == 88, "controller state layout");

	class iPhysicsController
	{
	friend struct cRustPhysicsAccess;
	public:
		iPhysicsController(const tString &asName, iPhysicsWorld *apWorld);
		virtual ~iPhysicsController();

		void Update(float afTimeStep);

		const tString& GetName(){ return msName;}

		void SetJoint(iPhysicsJoint *apJoint){ mpJoint = apJoint;}
		iPhysicsJoint* GetJoint(){ return mpJoint;}
		void SetBody(iPhysicsBody *apBody){ mpBody = apBody;}
		iPhysicsBody* GetBody(){ return mpBody;}

		bool IsActive(){ return mbActive;}
		void SetActive(bool abX);

		/*
		 * p in Pid and k in springs
		 */
		void SetA(float afA){ mfA = afA;}
		/*
		* i in Pid and b in springs
		*/
		void SetB(float afB){ mfB = afB;}
		/*
		* d in Pid and not used in springs
		*/
		void SetC(float afC){ mfC = afC;}

		void SetPidIntegralSize(int alSize);

		void SetType(ePhysicsControllerType aType){ mType = aType;}

		void SetDestValue(float afX){ mfDestValue = afX;}
		float GetDestValue(){ return mfDestValue;}

		void SetMaxOutput(float afX){ mfMaxOutput = afX;}

		void SetInputType(ePhysicsControllerInput aInput, ePhysicsControllerAxis aAxis){
			mInputType = aInput; mInputAxis = aAxis;}

		void SetOutputType(ePhysicsControllerOutput aOutput, ePhysicsControllerAxis aAxis){
			mOutputType = aOutput; mOutputAxis = aAxis;}
		void SetMulMassWithOutput(bool abX){ mbMulMassWithOutput = abX;}

		void SetEndType(ePhysicsControllerEnd aEnd){ mEndType = aEnd;}
		ePhysicsControllerEnd GetEndType(){ return mEndType;}

		void SetNextController(const tString &asName){ msNextController = asName; mS.nextP = msNextController.c_str(); mS.nextN = msNextController.size();}
		const tString& GetNextController(){ return msNextController;}

		void SetLogInfo(bool abX){mbLogInfo = abX;}

		void SetPaused(bool abX){mbPaused = abX;}


	protected:
		OhplCtrlState mS = OhplCtrlState();
		iPhysicsWorld *mpWorld;
		tString msName;
		iPhysicsBody *&mpBody = mS.body;
		iPhysicsJoint *&mpJoint = mS.joint;
		float &mfA = mS.a;
		float &mfB = mS.b;
		float &mfC = mS.c;
		float &mfDestValue = mS.dest;
		float &mfMaxOutput = mS.maxOutput;
		bool &mbMulMassWithOutput = mS.mulMass;
		ePhysicsControllerType &mType = mS.kind;
		ePhysicsControllerInput &mInputType = mS.input;
		ePhysicsControllerAxis &mInputAxis = mS.inputAxis;
		ePhysicsControllerOutput &mOutputType = mS.output;
		ePhysicsControllerAxis &mOutputAxis = mS.outputAxis;
		ePhysicsControllerEnd &mEndType = mS.end;
		tString msNextController;
		OhplPid *mpPid;

		bool &mbActive = mS.active;
		bool &mbPaused = mS.paused;
		bool &mbLogInfo = mS.log;
	};
};
#endif // HPL_PHYSICS_CONTROLLER_H
