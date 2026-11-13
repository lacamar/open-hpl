#include "physics/RustPhysics.h"

#include "physics/Physics.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsController.h"
#include "physics/PhysicsJoint.h"
#include "physics/PhysicsWorld.h"
#include "physics/SurfaceData.h"
#include "scene/ParticleSystem.h"
#include "scene/SoundEntity.h"
#include "scene/World.h"
#include "sound/Sound.h"
#include "sound/SoundChannel.h"
#include "sound/SoundHandler.h"
#include "system/LowLevelSystem.h"
#include "math/Math.h"

namespace hpl {

	struct cRustPhysicsAccess
	{
		static iPhysicsBody* B(size_t h) { return reinterpret_cast<iPhysicsBody*>(h); }
		static iPhysicsJoint* J(size_t h) { return reinterpret_cast<iPhysicsJoint*>(h); }
		static iPhysicsController* C(size_t h) { return reinterpret_cast<iPhysicsController*>(h); }
		static cSoundEntity* E(size_t h) { return reinterpret_cast<cSoundEntity*>(h); }
		static cSoundEntry* Y(size_t h) { return reinterpret_cast<cSoundEntry*>(h); }
		static cWorld* W(size_t h) { return reinterpret_cast<cWorld*>(h); }
		static iPhysicsWorld* PW(size_t h) { return reinterpret_cast<iPhysicsWorld*>(h); }
		static cSurfaceData* S(size_t h) { return reinterpret_cast<cSurfaceData*>(h); }
		static size_t H(const void* p) { return reinterpret_cast<size_t>(p); }
		static tString T(OhplStr s) { return tString(s.p, s.n); }
		static OhplStr S_(const tString& s) { return OhplS(s); }
		static cVector3f V(OhplV3 v) { return cVector3f(v.x, v.y, v.z); }
		static OhplV3 V(const cVector3f& v) { OhplV3 r = { v.x, v.y, v.z }; return r; }

		static OhplPhysVt Make()
		{
			OhplPhysVt vt;
			vt.can_play_impact = [](size_t p) { return reinterpret_cast<cPhysics*>(p)->CanPlayImpact(); };
			vt.add_impact = [](size_t p) { reinterpret_cast<cPhysics*>(p)->AddImpact(); };
			vt.pworld_world = [](size_t pw) { return H(PW(pw)->GetWorld()); };
			vt.joint_world_world = [](size_t j) { return H(J(j)->mpWorld->GetWorld()); };
			vt.body_world_world = [](size_t b) { return H(B(b)->GetWorld()->GetWorld()); };
			vt.world_silent = [](size_t w) { return W(w)->GetSound()->GetSoundHandler()->GetSilent(); };
			vt.world_sound_exists = [](size_t w, size_t e, int32_t id) { return W(w)->SoundEntityExists(E(e), id); };
			vt.world_create_sound = [](size_t w, OhplStr name, OhplStr file) {
				return H(W(w)->CreateSoundEntity(T(name), T(file), true));
			};
			vt.world_create_impact_ps = [](size_t w, OhplStr ps, OhplV3 pos) {
				cMatrixf mtxPos = cMath::MatrixTranslate(V(pos));
				cParticleSystem *pPS = W(w)->CreateParticleSystem("ImpactPS", T(ps), 1);
				if(pPS) pPS->SetMatrix(mtxPos);
			};
			vt.ent_set_saved_false = [](size_t e) { E(e)->SetIsSaved(false); };
			vt.ent_set_position = [](size_t e, OhplV3 p) { E(e)->SetPosition(V(p)); };
			vt.ent_set_param_named = [](size_t e, OhplStr n, float v) { E(e)->SetParam(T(n), v); };
			vt.ent_set_param_index = [](size_t e, int32_t i, float v) { E(e)->SetParam((int)i, v); };
			vt.ent_fade_in = [](size_t e, float t) { E(e)->FadeIn(t); };
			vt.ent_fade_out = [](size_t e, float t) { E(e)->FadeOut(t); };
			vt.ent_main_entry = [](size_t e) { return H(E(e)->GetSoundEntry(eSoundEntityType_Main, true)); };
			vt.ent_volume = [](size_t e) { return E(e)->GetVolume(); };
			vt.ent_set_volume = [](size_t e, float v) { E(e)->SetVolume(v); };
			vt.ent_creation_id = [](size_t e) { return (int32_t)E(e)->GetCreationID(); };
			vt.entry_set_speed_mul = [](size_t y, float v) { Y(y)->SetSpeedMul(v); };
			vt.entry_set_volume_mul = [](size_t y, float v) { Y(y)->SetVolumeMul(v); };
			vt.entry_fade_volume_mul_to = [](size_t y, float v, float t) { Y(y)->FadeVolumeMulTo(v, t); };

			vt.surf_impact_count = [](size_t s) { return (int32_t)S(s)->GetImpactDataNum(); };
			vt.surf_impact_min_speed = [](size_t s, int32_t i) { return S(s)->GetImpactData(i)->GetMinSpeed(); };
			vt.surf_impact_sound = [](size_t s, int32_t i) { return S_(S(s)->GetImpactData(i)->GetSoundName()); };
			vt.surf_impact_ps = [](size_t s, int32_t i) { return S_(S(s)->GetImpactData(i)->GetPSName()); };
			vt.surf_impact_ps_prio = [](size_t s, int32_t i) { return (int32_t)S(s)->GetImpactData(i)->GetPSPrio(); };

			vt.body_mass = [](size_t b) { return B(b)->GetMass(); };
			vt.body_enabled = [](size_t b) { return B(b)->GetEnabled(); };
			vt.body_linear_velocity = [](size_t b) { return V(B(b)->GetLinearVelocity()); };
			vt.body_angular_velocity = [](size_t b) { return V(B(b)->GetAngularVelocity()); };
			vt.body_set_linear_velocity = [](size_t b, OhplV3 v) { B(b)->SetLinearVelocity(V(v)); };
			vt.body_set_angular_velocity = [](size_t b, OhplV3 v) { B(b)->SetAngularVelocity(V(v)); };
			vt.body_world_position = [](size_t b) { return V(B(b)->GetWorldPosition()); };
			vt.body_local_position = [](size_t b) { return V(B(b)->GetLocalPosition()); };
			vt.body_mass_centre = [](size_t b) { return V(B(b)->GetMassCentre()); };
			vt.body_set_position = [](size_t b, OhplV3 p) { B(b)->SetPosition(V(p)); };
			vt.body_add_force = [](size_t b, OhplV3 v) { B(b)->AddForce(V(v)); };
			vt.body_add_torque = [](size_t b, OhplV3 v) { B(b)->AddTorque(V(v)); };
			vt.body_use_surface_effects = [](size_t b) { return B(b)->GetUseSurfaceEffects(); };
			vt.body_scrape_body = [](size_t b) { return H(B(b)->GetScrapeBody()); };
			vt.body_set_scrape_body = [](size_t b, size_t o) { B(b)->SetScrapeBody(B(o)); };
			vt.body_scrape_entity = [](size_t b) { return H(B(b)->GetScrapeSoundEntity()); };
			vt.body_scrape_entity_id = [](size_t b) { return (int32_t)B(b)->GetScrapeSoundEntityID(); };
			vt.body_set_scrape_entity = [](size_t b, size_t e) { B(b)->SetScrapeSoundEntity(E(e)); };
			vt.body_roll_entity = [](size_t b) { return H(B(b)->GetRollSoundEntity()); };
			vt.body_roll_entity_id = [](size_t b) { return (int32_t)B(b)->GetRollSoundEntityID(); };
			vt.body_set_roll_entity = [](size_t b, size_t e) { B(b)->SetRollSoundEntity(E(e)); };
			vt.body_has_slide = [](size_t b) { return B(b)->HasSlide(); };
			vt.body_set_has_slide = [](size_t b, bool v) { B(b)->SetHasSlide(v); };
			vt.body_has_collision = [](size_t b) { return B(b)->HasCollision(); };
			vt.body_set_has_collision = [](size_t b, bool v) { if(B(b)->mRsBook) ohpl_h15_body_flag_set(B(b)->mRsBook, OhplH15_HasCollision, v); else B(b)->mbHasCollision = v; };
			vt.body_in_update_list = [](size_t b) { return B(b)->IsInUpdateList(); };
			vt.body_add_to_update_list = [](size_t b) { B(b)->GetWorld()->AddBodyToUpdateList(B(b)); };
			vt.body_static_movement = [](size_t b) { return B(b)->mRsBook ? ohpl_h15_body_flag_get(B(b)->mRsBook, OhplH15_StaticMovement) : B(b)->mbStaticMovement; };
			vt.body_set_static_movement = [](size_t b, bool v) { if(B(b)->mRsBook) ohpl_h15_body_flag_set(B(b)->mRsBook, OhplH15_StaticMovement, v); else B(b)->mbStaticMovement = v; };
			vt.body_total_static_linear = [](size_t b) { return V(B(b)->mvTotalStaticLinearVel); };
			vt.body_total_static_angular = [](size_t b) { return V(B(b)->mvTotalStaticAngularVel); };
			vt.body_set_total_static_linear = [](size_t b, OhplV3 v) { B(b)->mvTotalStaticLinearVel = V(v); };
			vt.body_set_total_static_angular = [](size_t b, OhplV3 v) { B(b)->mvTotalStaticAngularVel = V(v); };
			vt.body_slide_count = [](size_t b) { return (int32_t)B(b)->mlSlideCount; };
			vt.body_set_slide_count = [](size_t b, int32_t v) { B(b)->mlSlideCount = v; };
			vt.body_disable_after_simulation = [](size_t b) { return B(b)->mRsBook ? ohpl_h15_body_flag_get(B(b)->mRsBook, OhplH15_DisableAfterSim) : B(b)->mbDisableAfterSimulation; };
			vt.body_set_disable_after_simulation = [](size_t b, bool v) { if(B(b)->mRsBook) ohpl_h15_body_flag_set(B(b)->mRsBook, OhplH15_DisableAfterSim, v); else B(b)->mbDisableAfterSimulation = v; };
			vt.body_enable_bodies_around = [](size_t b) {
				iPhysicsBody *pBody = B(b);
				cBoundingVolume tempBV;
				tempBV.SetSize(pBody->GetBoundingVolume()->GetSize() * 1.2f);
				tempBV.SetPosition(pBody->GetBoundingVolume()->GetWorldCenter());
				pBody->mpWorld->EnableBodiesInBV(&tempBV, true);
			};
			vt.body_update_roll_effect = [](size_t b) {
				iPhysicsBody *pBody = B(b);
				if(pBody->mpMaterial && pBody->mpMaterial->GetSurfaceData())
				{
					pBody->mpMaterial->GetSurfaceData()->UpdateRollEffect(pBody);
				}
			};

			vt.joint_force_size = [](size_t j) { return J(j)->GetForceSize(); };
			vt.joint_angle = [](size_t j) { return J(j)->GetAngle(); };
			vt.joint_distance = [](size_t j) { return J(j)->GetDistance(); };
			vt.joint_parent = [](size_t j) { return H(J(j)->GetParentBody()); };
			vt.joint_child = [](size_t j) { return H(J(j)->GetChildBody()); };
			vt.joint_has_collided = [](size_t j) { return J(j)->mbHasCollided; };
			vt.joint_set_has_collided = [](size_t j, bool v) { J(j)->mbHasCollided = v; };
			vt.joint_callback_max = [](size_t j) { if(J(j)->mpCallback) J(j)->mpCallback->OnMaxLimit(J(j)); };
			vt.joint_callback_min = [](size_t j) { if(J(j)->mpCallback) J(j)->mpCallback->OnMinLimit(J(j)); };
			vt.joint_get_controller = [](size_t j, OhplStr name) { return H(J(j)->GetController(T(name))); };
			vt.joint_controller_count = [](size_t j) { return (int32_t)J(j)->ControllerCount(); };
			vt.joint_controller_at = [](size_t j, int32_t i) {
				return H(J(j)->ControllerAt(i));
			};
			vt.joint_warn_missing_controller = [](size_t j, size_t c) {
				Warning("Controller '%s' does not exist in joint '%s'\n", C(c)->GetNextController().c_str(), J(j)->msName.c_str());
			};
			vt.joint_pivot = [](size_t j) { return V(J(j)->GetPivotPoint()); };

			vt.ctrl_is_active = [](size_t c) { return C(c)->IsActive(); };
			vt.ctrl_set_active = [](size_t c, bool v) { C(c)->SetActive(v); };
			vt.ctrl_deactivate = [](size_t c) { C(c)->mbActive = false; };
			vt.ctrl_end_type = [](size_t c) { return (int32_t)C(c)->GetEndType(); };
			vt.ctrl_next_name = [](size_t c) { return S_(C(c)->GetNextController()); };
			vt.ctrl_log = [](size_t c, float value, float dest, float error, float output) {
				Log("%s | Input: %f Dest: %f Error: %f OutPut: %f\n", C(c)->GetName().c_str(), value, dest, error, output);
			};

			vt.body_local_matrix = [](size_t b, cMatrixf *m) { *m = B(b)->GetLocalMatrix(); };
			vt.body_prev_scrape_matrix = [](size_t b, cMatrixf *m) { *m = B(b)->GetPreveScrapeMatrix(); };
			vt.body_set_prev_scrape_matrix = [](size_t b, const cMatrixf *m) { B(b)->SetPreveScrapeMatrix(*m); };
			vt.body_set_matrix = [](size_t b, const cMatrixf *m) { B(b)->SetMatrix(*m); };
			return vt;
		}
	};

	const OhplPhysVt* OhplPhysHost()
	{
		static const OhplPhysVt vt = cRustPhysicsAccess::Make();
		return &vt;
	}
}
