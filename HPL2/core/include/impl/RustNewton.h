#ifndef HPL_RUST_NEWTON_H
#define HPL_RUST_NEWTON_H

// C ABI of rust/src/physics/newton_ffi.rs.

#include "math/MathTypes.h"
#include <cstddef>
#include <cstdint>

namespace hpl {

	struct OhplNewtonMatProps { int32_t frictionMode, elasticityMode; float elasticity, staticFriction, kineticFriction; };
	struct OhplNewtonGateBody { bool collide, active, isRagDoll, collideRagDoll, isCharacter, collideCharacter; };
	struct OhplNewtonContact { float normalSpeed, tangentSpeed0, tangentSpeed1; cVector3f force, position, normal; };
	struct OhplNewtonContactAgg { float maxNormalSpeed, maxTangentSpeed; cVector3f force, normal, position; };
	struct OhplNewtonSubShape { const cVector3f* points; size_t count; float radius, volume; };
	struct OhplNewtonBuoyIn
	{
		float mass, density, densityMul, dragDensityMul, linearViscosity, angularViscosity;
		float maxLinearSpeed, maxAngularSpeed, shapeVolume, timeStep;
		bool noPrevVel;
		cPlanef surface;
		cMatrixf matrix, inverse, inertia;
		cVector3f massCentre, gravity, linearVelocity, angularVelocity, prevLinearVel, prevAngularVel;
	};
	struct OhplNewtonBuoyOut
	{
		cVector3f force, torque, prevLinearVel, prevAngularVel;
		float dragDensityMul;
		bool noPrevVel, dragUpdated;
	};

	extern "C" {
		bool ohpl_newton_buoyancy(const OhplNewtonBuoyIn* in, const OhplNewtonSubShape* subs, size_t n, OhplNewtonBuoyOut* out);
		size_t ohpl_newton_buoyancy_points(int32_t shape, const cVector3f* size, cVector3f* out, size_t cap, float* radius);
	}

	static_assert(sizeof(OhplNewtonContact) == 48 && sizeof(OhplNewtonContactAgg) == 44, "Rust layout");
	static_assert(sizeof(OhplNewtonBuoyOut) == 56, "Rust layout");
}

#endif
