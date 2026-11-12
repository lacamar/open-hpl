#include "impl/RustNewtonBackend.h"

#include "impl/PhysicsBodyNewton.h"
#include "impl/PhysicsMaterialNewton.h"
#include "impl/PhysicsWorldNewton.h"
#include "graphics/LowLevelGraphics.h"
#include "physics/PhysicsWorld.h"
#include "resources/BinaryBuffer.h"

namespace hpl {

	static_assert(sizeof(cCollidePoint) == sizeof(OhplL4CollPoint) && offsetof(cCollidePoint, mfDepth) == offsetof(OhplL4CollPoint, depth),
				"Rust layout");

	static cPhysicsBodyNewton* Body(size_t cpp) { return reinterpret_cast<cPhysicsBodyNewton*>(cpp); }

	struct OhplL4HostImpl
	{
		static void BodyState(size_t cpp, OhplL4BodyState* out)
		{
			cPhysicsBodyNewton* b = Body(cpp);
			out->active = b->IsActive();
			out->inUpdateList = b->IsInUpdateList();
			out->buoyancyActive = b->mBuoyancy.mbActive;
			out->hpl3 = cPhysicsBodyNewton::mbHpl3Buoyancy;
			out->freezeCount = b->mlFreezeCount;
			out->densityMul = b->mfBuoyancyDensityMul;
			out->density = b->mBuoyancy.mfDensity;
			out->linearViscosity = b->mBuoyancy.mfLinearViscosity;
			out->angularViscosity = b->mBuoyancy.mfAngularViscosity;
			out->surface[0] = b->mBuoyancy.mSurface.a;
			out->surface[1] = b->mBuoyancy.mSurface.b;
			out->surface[2] = b->mBuoyancy.mSurface.c;
			out->surface[3] = b->mBuoyancy.mSurface.d;
			out->gravity = b->mpWorld->GetGravity();
		}

		static void BodyAddToUpdateList(size_t cpp)
		{
			cPhysicsBodyNewton* b = Body(cpp);
			b->GetWorld()->AddBodyToUpdateList(b);
		}

		static void BodySetFreezeCount(size_t cpp, int32_t n) { Body(cpp)->mlFreezeCount = n; }

		static bool BodyHpl3Buoyancy(size_t cpp, const cVector3f* gravity, float timeStep, cVector3f* force, cVector3f* torque)
		{
			cPhysicsBodyNewton* b = Body(cpp);
			if(b->CalcHpl3Buoyancy(*gravity, timeStep) == false) return false;
			*force = b->mvBuoyancyForce;
			*torque = b->mvBuoyancyTorque;
			return true;
		}

		static void BodySetTransform(size_t cpp, const float* m)
		{
			cPhysicsBodyNewton* b = Body(cpp);
			b->m_mtxLocalTransform.FromTranspose(m);
			cPhysicsBodyNewton::mbUseCallback = false;
			b->SetTransformUpdated(true);
			cPhysicsBodyNewton::mbUseCallback = true;
		}

		static void BodyGate(size_t cpp, OhplNewtonGateBody* gate, uint32_t* flags)
		{
			cPhysicsBodyNewton* b = Body(cpp);
			*gate = OhplNewtonGateBody{b->GetCollide(), b->IsActive(), b->IsRagDoll(), b->GetCollideRagDoll(),
										b->IsCharacter(), b->GetCollideCharacter()};
			*flags = b->GetCollideFlags();
		}

		static bool CollideFlagsMatch(uint32_t a, uint32_t b) { return iPhysicsBody::CollideFlagsMatch(a, b); }

		static bool BodyOnAABBCollision(size_t cpp, size_t other) { return Body(cpp)->OnAABBCollision(Body(other)); }

		static bool BodyIsActive(size_t cpp) { return Body(cpp)->IsActive(); }

		static void BodyAABB(size_t cpp, cVector3f* minV, cVector3f* maxV)
		{
			cBoundingVolume* bv = Body(cpp)->GetBoundingVolume();
			*minV = bv->GetMin();
			*maxV = bv->GetMax();
		}

		static bool ContactSaveEnabled(size_t cpp) { return Body(cpp)->GetWorld()->GetSaveContactPoints(); }

		static void ContactPushPoint(size_t cpp, const cVector3f* position, const cVector3f* normal)
		{
			cCollidePoint point;
			point.mfDepth = 1;
			point.mvPoint = *position;
			point.mvNormal = *normal;
			Body(cpp)->GetWorld()->GetContactPoints()->push_back(point);
		}

		static void ContactDispatch(size_t cpp1, size_t cpp2, const OhplNewtonContactAgg* agg, int32_t count)
		{
			cPhysicsContactData contactData;
			contactData.mfMaxContactNormalSpeed = agg->maxNormalSpeed;
			contactData.mfMaxContactTangentSpeed = agg->maxTangentSpeed;
			contactData.mvForce = agg->force;
			contactData.mvContactNormal = agg->normal;
			contactData.mvContactPosition = agg->position;
			cPhysicsMaterialNewton::DispatchContacts(Body(cpp1), Body(cpp2), contactData, count);
		}

		static bool RayBeforeIntersect(size_t cb, size_t body)
		{
			return reinterpret_cast<iPhysicsRayCallback*>(cb)->BeforeIntersect(Body(body));
		}

		// Persists across casts like the C++ path, so fields the caller did not ask for keep their last value.
		static bool RayOnIntersect(size_t cb, size_t body, float t, const float* dist, const cVector3f* normal, const cVector3f* point)
		{
			static cPhysicsRayParams params;
			params.mfT = t;
			if(dist) params.mfDist = *dist;
			if(normal) params.mvNormal = *normal;
			if(point) params.mvPoint = *point;
			return reinterpret_cast<iPhysicsRayCallback*>(cb)->OnIntersect(Body(body), &params);
		}

		static void BodyVecPush(size_t vec, size_t cpp)
		{
			reinterpret_cast<std::vector<iPhysicsBody*>*>(vec)->push_back(Body(cpp));
		}

		static void DrawLine(size_t low, const cVector3f* a, const cVector3f* b, size_t color)
		{
			reinterpret_cast<iLowLevelGraphics*>(low)->DrawLine(*a, *b, *reinterpret_cast<const cColor*>(color));
		}

		static void BufAdd(size_t buf, const void* data, size_t count)
		{
			reinterpret_cast<cBinaryBuffer*>(buf)->AddInt32Array((int*)data, count);
		}

		static void BufGet(size_t buf, void* data, size_t count)
		{
			reinterpret_cast<cBinaryBuffer*>(buf)->GetInt32Array((int*)data, count);
		}
	};

	void OhplL4InstallHost()
	{
		static const OhplL4Host host = {
			OhplL4HostImpl::BodyState,
			OhplL4HostImpl::BodyAddToUpdateList,
			OhplL4HostImpl::BodySetFreezeCount,
			OhplL4HostImpl::BodyHpl3Buoyancy,
			OhplL4HostImpl::BodySetTransform,
			OhplL4HostImpl::BodyGate,
			OhplL4HostImpl::CollideFlagsMatch,
			OhplL4HostImpl::BodyOnAABBCollision,
			OhplL4HostImpl::BodyIsActive,
			OhplL4HostImpl::BodyAABB,
			OhplL4HostImpl::ContactSaveEnabled,
			OhplL4HostImpl::ContactPushPoint,
			OhplL4HostImpl::ContactDispatch,
			OhplL4HostImpl::RayBeforeIntersect,
			OhplL4HostImpl::RayOnIntersect,
			OhplL4HostImpl::BodyVecPush,
			OhplL4HostImpl::DrawLine,
			OhplL4HostImpl::BufAdd,
			OhplL4HostImpl::BufGet,
		};
		ohpl_nt_set_host(&host);
	}

}
