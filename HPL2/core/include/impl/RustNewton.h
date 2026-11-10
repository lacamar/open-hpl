#ifndef HPL_RUST_NEWTON_H
#define HPL_RUST_NEWTON_H

// C ABI of rust/src/physics/newton_ffi.rs.

#include "math/MathTypes.h"
#include <cstddef>
#include <cstdint>

namespace hpl {

	struct OhplNewtonMatProps { int32_t frictionMode, elasticityMode; float elasticity, staticFriction, kineticFriction; };
	struct OhplNewtonPairParams { float elasticity, staticFriction, kineticFriction; };
	struct OhplNewtonGateBody { bool collide, active, isRagDoll, collideRagDoll, isCharacter, collideCharacter; };
	struct OhplNewtonContact { float normalSpeed, tangentSpeed0, tangentSpeed1; cVector3f force, position, normal; };
	struct OhplNewtonContactAgg { float maxNormalSpeed, maxTangentSpeed; cVector3f force, normal, position; };
	struct OhplNewtonSurfaceCalls { bool impact1, slide1, impact2, slide2; };
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
		void ohpl_newton_pair_params(const OhplNewtonMatProps* a, const OhplNewtonMatProps* b, bool same, OhplNewtonPairParams* out);
		bool ohpl_newton_aabb_gate(bool flagsMatch, const OhplNewtonGateBody* a, const OhplNewtonGateBody* b);
		void ohpl_newton_aggregate_contacts(const OhplNewtonContact* contacts, size_t n, OhplNewtonContactAgg* out);
		void ohpl_newton_surface_calls(int32_t prio1, int32_t prio2, bool sameMaterial, float normalSpeed, float tangentSpeed, OhplNewtonSurfaceCalls* out);
		bool ohpl_newton_clamp_speed(const cVector3f* vel, float max, cVector3f* out);
		bool ohpl_newton_buoyancy(const OhplNewtonBuoyIn* in, const OhplNewtonSubShape* subs, size_t n, OhplNewtonBuoyOut* out);
		size_t ohpl_newton_buoyancy_points(int32_t shape, const cVector3f* size, cVector3f* out, size_t cap, float* radius);
	}

	static_assert(sizeof(OhplNewtonContact) == 48 && sizeof(OhplNewtonContactAgg) == 44, "Rust layout");
	static_assert(sizeof(OhplNewtonBuoyOut) == 56, "Rust layout");
}

#endif
