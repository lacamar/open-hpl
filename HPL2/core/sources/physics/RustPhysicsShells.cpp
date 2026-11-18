#include "physics/PhysicsController.h"
#include "physics/PhysicsJointBall.h"
#include "physics/PhysicsJointHinge.h"
#include "physics/PhysicsJointScrew.h"
#include "physics/PhysicsJointSlider.h"

#include "math/Math.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsWorld.h"

namespace hpl {

	iPhysicsController::iPhysicsController(const tString &asName, iPhysicsWorld *apWorld)
		: mS(ohpl_h15c_new(reinterpret_cast<size_t>(this))), msName(asName)
	{
	}

	iPhysicsController::~iPhysicsController()
	{
		ohpl_h15c_free(mS);
	}

	iPhysicsJoint::iPhysicsJoint(const tString &asName, iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
		iPhysicsWorld *apWorld,const cVector3f &avPivotPoint, const cVector3f &avPinDir)
		: msName(asName)
	{
		mS = ohpl_h15j_new(HostOp, reinterpret_cast<size_t>(this), reinterpret_cast<size_t>(OhplPhysHost()), reinterpret_cast<size_t>(apParentBody),
			reinterpret_cast<size_t>(apChildBody), reinterpret_cast<size_t>(apWorld), OhplV3Pod{avPivotPoint.x, avPivotPoint.y, avPivotPoint.z},
			OhplV3Pod{avPinDir.x, avPinDir.y, avPinDir.z});
		mhRustWorld = apWorld->RustHandle();
	}

	iPhysicsJoint::~iPhysicsJoint()
	{
		if(mS->child || mS->parent)
		{
			ohpl_ntj_destroy(mhRustWorld, mhRust);
		}
		ohpl_h15j_drop(mS->shell, reinterpret_cast<size_t>(this));
	}

	static NewtonBody* NB(iPhysicsBody *apBody){ return apBody ? apBody->GetNewtonBody() : NULL;}

	iPhysicsJointBall::iPhysicsJointBall(const tString &asName, iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
		iPhysicsWorld *apWorld, const cVector3f &avPivotPoint, const cVector3f &avPinDir)
		: iPhysicsJoint(asName,apParentBody,apChildBody,apWorld,avPivotPoint,avPinDir)
	{
		mhRust = ohpl_ntj_create_ball(mhRustWorld, &avPivotPoint, NB(apChildBody), NB(apParentBody), NULL);
	}

	iPhysicsJointBall::~iPhysicsJointBall()
	{
	}

	iPhysicsJointHinge::iPhysicsJointHinge(const tString &asName, iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
		iPhysicsWorld *apWorld, const cVector3f &avPivotPoint, const cVector3f &avPinDir)
		: iPhysicsJoint(asName,apParentBody,apChildBody,apWorld,avPivotPoint,avPinDir)
	{
		mhRust = ohpl_ntj_create_hinge(mhRustWorld, NB(apChildBody), NB(apParentBody), (size_t)static_cast<iPhysicsJoint*>(this), RustOps(),
									&mS->pivot, &mS->pinDir, NULL);
	}

	iPhysicsJointHinge::~iPhysicsJointHinge()
	{
	}

	cVector3f iPhysicsJointHinge::GetAngularVelocity()
	{
		if(mS->parent)
		{
			cVector3f vAngularVel0 = mS->child->GetAngularVelocity();
			cVector3f vAngularVel1 = mS->parent->GetAngularVelocity();

			return GetPinDir() * cMath::Vector3Dot(vAngularVel0 - vAngularVel1, GetPinDir());
		}
		return GetPinDir() * cMath::Vector3Dot(mS->child->GetAngularVelocity(), GetPinDir());
	}

	void iPhysicsJointHinge::ResetPosition()
	{
		cMatrixf mtxChild;
		ohpl_ntj_hinge_reset_matrix(mhRust, &mtxChild);
		mS->child->SetMatrix(mtxChild);
		mS->child->SetLinearVelocity(0);
		mS->child->SetAngularVelocity(0);
	}

	iPhysicsJointSlider::iPhysicsJointSlider(const tString &asName, iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
		iPhysicsWorld *apWorld, const cVector3f &avPivotPoint, const cVector3f &avPinDir)
		: iPhysicsJoint(asName,apParentBody,apChildBody,apWorld,avPivotPoint,avPinDir)
	{
		mhRust = ohpl_ntj_create_slider(mhRustWorld, &avPivotPoint, &avPinDir, NB(apChildBody), NB(apParentBody),
									(size_t)static_cast<iPhysicsJoint*>(this), RustOps(), NULL);
	}

	iPhysicsJointSlider::~iPhysicsJointSlider()
	{
	}

	iPhysicsJointScrew::iPhysicsJointScrew(const tString &asName, iPhysicsBody *apParentBody, iPhysicsBody *apChildBody,
		iPhysicsWorld *apWorld, const cVector3f &avPivotPoint, const cVector3f &avPinDir)
		: iPhysicsJoint(asName,apParentBody,apChildBody,apWorld,avPivotPoint,avPinDir)
	{
		mhRust = ohpl_ntj_create_screw(mhRustWorld, &avPivotPoint, &avPinDir, NB(apChildBody), NB(apParentBody),
									(size_t)static_cast<iPhysicsJoint*>(this), RustOps(), NULL);
	}

	iPhysicsJointScrew::~iPhysicsJointScrew()
	{
	}
}
