#ifndef HPL_RUST_NEWTON_BACKEND_H
#define HPL_RUST_NEWTON_BACKEND_H

// C ABI of rust/src/backend/newton (OHPL_IMPL=rust). Handles are Rust arena tokens, never addresses.

#include "impl/RustBackends.h"
#include "impl/RustNewton.h"
#include "math/MathTypes.h"
#include <Newton.h>
#include <cstddef>
#include <cstdint>

namespace hpl {

	inline bool OhplL4UseRust()
	{
		return OhplBackendRustRequested();
	}

	struct OhplL4BodyState
	{
		bool active, inUpdateList, buoyancyActive, hpl3;
		int32_t freezeCount;
		float densityMul, density, linearViscosity, angularViscosity;
		float surface[4];
		cVector3f gravity;
	};

	struct OhplL4JointOps
	{
		void (*limitEvent)(size_t j, int32_t kind);
		void (*checkAutoSleep)(size_t j, float minV, float maxV, float value);
		void (*noLimit)(size_t j, float prev, float minV, float maxV, bool skip);
		void (*matrices)(size_t j, cMatrixf* child, cMatrixf* parent);
		void (*positionCheck)(size_t j, float distSqr);
	};

	struct OhplL4Host
	{
		void (*bodyState)(size_t cpp, OhplL4BodyState* out);
		void (*bodyAddToUpdateList)(size_t cpp);
		void (*bodySetFreezeCount)(size_t cpp, int32_t n);
		bool (*bodyHpl3Buoyancy)(size_t cpp, const cVector3f* gravity, float timeStep, cVector3f* force, cVector3f* torque);
		void (*bodySetTransform)(size_t cpp, const float* m);
		void (*bodyGate)(size_t cpp, OhplNewtonGateBody* gate, uint32_t* flags);
		bool (*collideFlagsMatch)(uint32_t a, uint32_t b);
		bool (*bodyOnAABBCollision)(size_t cpp, size_t other);
		bool (*bodyIsActive)(size_t cpp);
		void (*bodyAABB)(size_t cpp, cVector3f* minV, cVector3f* maxV);
		bool (*contactSaveEnabled)(size_t cpp);
		void (*contactPushPoint)(size_t cpp, const cVector3f* position, const cVector3f* normal);
		void (*contactDispatch)(size_t cpp1, size_t cpp2, const OhplNewtonContactAgg* agg, int32_t count);
		bool (*rayBeforeIntersect)(size_t cb, size_t body);
		bool (*rayOnIntersect)(size_t cb, size_t body, float t, const float* dist, const cVector3f* normal, const cVector3f* point);
		void (*bodyVecPush)(size_t vec, size_t cpp);
		void (*drawLine)(size_t low, const cVector3f* a, const cVector3f* b, size_t color);
		void (*bufAdd)(size_t buf, const void* data, size_t count);
		void (*bufGet)(size_t buf, void* data, size_t count);
	};

	struct OhplL4CollPoint { cVector3f point, normal; float depth; };
	static_assert(sizeof(OhplL4CollPoint) == 28, "Rust layout");

	void OhplL4InstallHost();

	extern "C" {
		void ohpl_nt_set_host(const OhplL4Host* host);

		uint64_t ohpl_ntw_create(NewtonWorld** outWorld);
		void ohpl_ntw_destroy(uint64_t w);
		void ohpl_ntw_simulate(uint64_t w, float timeStep);
		void ohpl_ntw_set_max_step(uint64_t w, float step);
		float ohpl_ntw_max_step(uint64_t w);
		void ohpl_ntw_set_size(uint64_t w, const float* minV, const float* maxV);
		void ohpl_ntw_set_accuracy(uint64_t w, int32_t accuracy);
		void ohpl_ntw_set_threads(uint64_t w, int32_t n);
		int32_t ohpl_ntw_threads(uint64_t w);
		void ohpl_ntw_bodies_in_aabb(uint64_t w, const float* minV, const float* maxV, size_t vec);
		void ohpl_ntw_cast_ray(uint64_t w, size_t cb, const cVector3f* origin, const cVector3f* end,
								bool calcDist, bool calcNormal, bool calcPoint, bool prefilter);
		float ohpl_ntw_cast_ray_on_shape(const NewtonCollision* coll, const cMatrixf* transform, const cVector3f* origin,
								const cVector3f* end, cVector3f* normal, bool localPositions);
		bool ohpl_ntw_check_collision(uint64_t w, NewtonCollision* const* subsA, size_t na, NewtonCollision* const* subsB, size_t nb,
								bool compound, const cMatrixf* mtxA, const cMatrixf* mtxB, int32_t maxPoints, bool correct,
								bool aIsMesh, OhplL4CollPoint* out, size_t cap, int32_t* outCount);
		void ohpl_ntw_debug_shape(const NewtonCollision* coll, const cMatrixf* transform, size_t low, size_t color);

		NewtonCollision* ohpl_nts_create(uint64_t w, int32_t kind, const cVector3f* size, const cMatrixf* offset);
		void ohpl_nts_release(uint64_t w, NewtonCollision* coll);
		NewtonCollision* ohpl_nts_compound(uint64_t w, NewtonCollision* const* colls, size_t n);
		NewtonCollision* ohpl_nts_static_scene(uint64_t w, NewtonCollision* const* colls, size_t n);
		NewtonCollision* ohpl_nts_tree(uint64_t w, const unsigned int* indices, int32_t indexNum, const float* vertices, int32_t stride, int32_t vertexNum);
		NewtonCollision* ohpl_nts_height_field(uint64_t w, int32_t size, const unsigned short* elevation, float unit, float vertical);
		void ohpl_nts_serialize(uint64_t w, const NewtonCollision* coll, size_t buf);
		NewtonCollision* ohpl_nts_deserialize(uint64_t w, size_t buf);

		uint64_t ohpl_ntb_create(uint64_t world, NewtonCollision* coll, size_t cpp, NewtonBody** outBody);
		void ohpl_ntb_destroy(uint64_t b);
		void ohpl_ntb_set_matrix(uint64_t b, const cMatrixf* m);
		void ohpl_ntb_set_material(uint64_t b, int32_t id);
		void ohpl_ntb_set_velocity(uint64_t b, const cVector3f* v);
		void ohpl_ntb_velocity(uint64_t b, cVector3f* out);
		void ohpl_ntb_set_omega(uint64_t b, const cVector3f* v);
		void ohpl_ntb_omega(uint64_t b, cVector3f* out);
		void ohpl_ntb_set_linear_damping(uint64_t b, float d);
		float ohpl_ntb_linear_damping(uint64_t b);
		void ohpl_ntb_set_angular_damping(uint64_t b, float d);
		float ohpl_ntb_angular_damping(uint64_t b);
		void ohpl_ntb_set_max_linear(uint64_t b, float s);
		float ohpl_ntb_max_linear(uint64_t b);
		void ohpl_ntb_set_max_angular(uint64_t b, float s);
		float ohpl_ntb_max_angular(uint64_t b);
		void ohpl_ntb_set_gravity(uint64_t b, bool on);
		bool ohpl_ntb_gravity(uint64_t b);
		void ohpl_ntb_inertia(uint64_t b, cVector3f* out);
		void ohpl_ntb_set_mass(uint64_t b, const NewtonCollision* coll, float mass);
		float ohpl_ntb_mass(uint64_t b);
		void ohpl_ntb_set_mass_centre(uint64_t b, const cVector3f* v);
		void ohpl_ntb_mass_centre(uint64_t b, cVector3f* out);
		void ohpl_ntb_add_force(uint64_t b, const cVector3f* f);
		void ohpl_ntb_add_force_at_position(uint64_t b, const cVector3f* f, const cVector3f* pos, const cMatrixf* local);
		void ohpl_ntb_add_torque(uint64_t b, const cVector3f* t);
		void ohpl_ntb_add_impulse(uint64_t b, const cVector3f* impulse, const cMatrixf* world);
		void ohpl_ntb_add_impulse_at(uint64_t b, const cVector3f* impulse, const cVector3f* pos);
		void ohpl_ntb_set_freeze(uint64_t b, int32_t state);
		bool ohpl_ntb_enabled(uint64_t b);
		void ohpl_ntb_set_auto_sleep(uint64_t b, bool on);
		bool ohpl_ntb_auto_sleep(uint64_t b);
		void ohpl_ntb_set_continuous(uint64_t b, bool on);
		bool ohpl_ntb_continuous(uint64_t b);

		int32_t ohpl_ntm_create_group(uint64_t w);
		void ohpl_ntm_update_pair(uint64_t w, int32_t idA, int32_t idB, const OhplNewtonMatProps* a, const OhplNewtonMatProps* b, bool same);

		uint64_t ohpl_ntj_create_ball(uint64_t w, const cVector3f* pivot, NewtonBody* child, NewtonBody* parent, NewtonJoint** outJoint);
		uint64_t ohpl_ntj_create_hinge(uint64_t w, NewtonBody* child, NewtonBody* parent, size_t cpp, const OhplL4JointOps* ops,
								const cMatrixf* local0, const cMatrixf* local1, NewtonJoint** outJoint);
		uint64_t ohpl_ntj_create_slider(uint64_t w, const cVector3f* pivot, const cVector3f* pin, NewtonBody* child, NewtonBody* parent,
								size_t cpp, const OhplL4JointOps* ops, NewtonJoint** outJoint);
		uint64_t ohpl_ntj_create_screw(uint64_t w, const cVector3f* pivot, const cVector3f* pin, NewtonBody* child, NewtonBody* parent,
								size_t cpp, const OhplL4JointOps* ops, NewtonJoint** outJoint);
		void ohpl_ntj_destroy(uint64_t w, uint64_t j);
		void ohpl_ntj_set_collide(uint64_t j, bool on);
		bool ohpl_ntj_collide(uint64_t j);
		void ohpl_ntj_set_stiffness(uint64_t j, float s);
		float ohpl_ntj_stiffness(uint64_t j);
		void ohpl_ntj_set_min(uint64_t j, float v);
		void ohpl_ntj_set_max(uint64_t j, float v);
		float ohpl_ntj_min(uint64_t j);
		float ohpl_ntj_max(uint64_t j);
		void ohpl_ntj_ball_cone_limits(uint64_t j, const cVector3f* pin, float cone, float twist);
		void ohpl_ntj_ball_angles(uint64_t j, cVector3f* out);
		void ohpl_ntj_ball_omega(uint64_t j, cVector3f* out);
		void ohpl_ntj_ball_force(uint64_t j, cVector3f* out);
		float ohpl_ntj_hinge_force_size(uint64_t j);
		float ohpl_ntj_axis(uint64_t j, int32_t what);
		void ohpl_ntj_force(uint64_t j, cVector3f* out);
	}
}

#endif
