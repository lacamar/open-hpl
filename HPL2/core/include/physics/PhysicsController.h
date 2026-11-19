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
#include "physics/RustPhysics.h"

namespace hpl {

	enum ePhysicsControllerType
	{
		ePhysicsControllerType_Pid,
		ePhysicsControllerType_Spring,
		ePhysicsControllerType_LastEnum
	};

	enum ePhysicsControllerInput
	{
		ePhysicsControllerInput_JointAngle,
		ePhysicsControllerInput_JointDist,
		ePhysicsControllerInput_LinearSpeed,
		ePhysicsControllerInput_AngularSpeed,
		ePhysicsControllerInput_LastEnum
	};

	enum ePhysicsControllerOutput
	{
		ePhysicsControllerOutput_Force,
		ePhysicsControllerOutput_Torque,
		ePhysicsControllerOutput_LastEnum
	};

	enum ePhysicsControllerAxis
	{
		ePhysicsControllerAxis_X,
		ePhysicsControllerAxis_Y,
		ePhysicsControllerAxis_Z,
		ePhysicsControllerAxis_LastEnum
	};

	enum ePhysicsControllerEnd
	{
		ePhysicsControllerEnd_Null,
		ePhysicsControllerEnd_OnDest,
		ePhysicsControllerEnd_OnMin,
		ePhysicsControllerEnd_OnMax,
		ePhysicsControllerEnd_LastEnum
	};

	class iPhysicsWorld;
	class iPhysicsJoint;
	class iPhysicsBody;

	class iPhysicsController;
	class iPhysicsJoint;
	class iPhysicsBody;
	// Controller state, owned by rust/src/physics/controller.rs.
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
}

extern "C" {
hpl::OhplCtrlState* ohpl_h15c_new(size_t);
void ohpl_h15c_free(hpl::OhplCtrlState*);
void ohpl_h15c_update(const hpl::OhplPhysVt*, hpl::OhplCtrlState*, float);
void ohpl_h15c_set_active(hpl::OhplCtrlState*, bool);
void ohpl_h15c_set_error_num(hpl::OhplCtrlState*, int);
}

namespace hpl {

	class iPhysicsController
	{
	friend struct cRustPhysicsAccess;
	public:
		iPhysicsController(const tString &asName, iPhysicsWorld *apWorld);
		virtual ~iPhysicsController();

		void Update(float afTimeStep){ ohpl_h15c_update(OhplPhysHost(), mS, afTimeStep);}

		const tString& GetName(){ return msName;}

		void SetJoint(iPhysicsJoint *apJoint){ mS->joint = apJoint;}
		iPhysicsJoint* GetJoint(){ return mS->joint;}
		void SetBody(iPhysicsBody *apBody){ mS->body = apBody;}
		iPhysicsBody* GetBody(){ return mS->body;}

		bool IsActive(){ return mS->active;}
		void SetActive(bool abX){ ohpl_h15c_set_active(mS, abX);}

		/*
		 * p in Pid and k in springs
		 */
		void SetA(float afA){ mS->a = afA;}
		/*
		* i in Pid and b in springs
		*/
		void SetB(float afB){ mS->b = afB;}
		/*
		* d in Pid and not used in springs
		*/
		void SetC(float afC){ mS->c = afC;}

		void SetPidIntegralSize(int alSize){ ohpl_h15c_set_error_num(mS, alSize);}

		void SetType(ePhysicsControllerType aType){ mS->kind = aType;}

		void SetDestValue(float afX){ mS->dest = afX;}
		float GetDestValue(){ return mS->dest;}

		void SetMaxOutput(float afX){ mS->maxOutput = afX;}

		void SetInputType(ePhysicsControllerInput aInput, ePhysicsControllerAxis aAxis){
			mS->input = aInput; mS->inputAxis = aAxis;}

		void SetOutputType(ePhysicsControllerOutput aOutput, ePhysicsControllerAxis aAxis){
			mS->output = aOutput; mS->outputAxis = aAxis;}
		void SetMulMassWithOutput(bool abX){ mS->mulMass = abX;}

		void SetEndType(ePhysicsControllerEnd aEnd){ mS->end = aEnd;}
		ePhysicsControllerEnd GetEndType(){ return mS->end;}

		void SetNextController(const tString &asName){ msNextController = asName; mS->nextP = msNextController.c_str(); mS->nextN = msNextController.size();}
		const tString& GetNextController(){ return msNextController;}

		void SetLogInfo(bool abX){mS->log = abX;}

		void SetPaused(bool abX){mS->paused = abX;}

	protected:
		OhplCtrlState *mS;
		tString msName;
		tString msNextController;
	};
};
#endif // HPL_PHYSICS_CONTROLLER_H
