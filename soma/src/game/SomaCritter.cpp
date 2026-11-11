#include "SomaCritter.h"
#include "RustSomaUi.h"
#include "SomaAgent.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxPlayer.h"
#include "SomaScriptApi.h"
#include "SomaScriptBind.h"

#include <angelscript.h>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace
{
	struct cCritterState
	{
		OhplGs7CritterState mRust;
		tString msGroup;
		cCritterState() { ohpl_gs7_critter_state_default(&mRust); }
	};
	std::unordered_map<cSomaLuxEntity *, cCritterState> gmapStates;

	cCritterState &State(cSomaLuxEntity *apEnt)
	{
		cCritterState &s = gmapStates[apEnt];
		if (s.mRust.init == false)
		{
			s.msGroup = apEnt->mInstanceVars.GetVarString("GroupEntityName", "");
			cVector3f vFwd = apEnt->GetMatrix().GetForward();
			ohpl_gs7_critter_state_init(&s.mRust, vFwd.x, vFwd.z);
		}
		return s;
	}

	template <class T> T &Prop(cSomaLuxEntity *apEnt, const char *asName)
	{
		return *(T *)(apEnt->mpCritterProps + SomaIndirectPropOffset("cLuxCritter", asName));
	}

	cVector3f Out3(void (*apFn)(const float *, float *), const cVector3f &avIn)
	{
		cVector3f vOut;
		apFn(avIn.v, vOut.v);
		return vOut;
	}

	cVector3f SafeNormalize(const cVector3f &v) { return Out3(ohpl_gs7_critter_safe_normalize, v); }

	cVector3f PlayerPos()
	{
		cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
		return pPlayer && pPlayer->GetCharacterBody() ? pPlayer->GetCharacterBody()->GetPosition() : cVector3f(0);
	}

	class cFirstHit : public iPhysicsRayCallback
	{
	public:
		cSomaLuxEntity *mpIgnore;
		float mfDist = -1;
		cVector3f mvNormal = 0;
		bool OnIntersect(iPhysicsBody *apBody, cPhysicsRayParams *apParams) override
		{
			if (apBody->IsCharacter() || apBody->GetCollide() == false)
				return true;
			for (iPhysicsBody *pBody : mpIgnore->mvBodies)
				if (pBody == apBody)
					return true;
			if (mfDist < 0 || apParams->mfDist < mfDist)
			{
				mfDist = apParams->mfDist;
				mvNormal = apParams->mvNormal;
			}
			return true;
		}
	};

	std::unordered_map<tString, std::vector<cSomaLuxEntity *>> gmapGroups;

	const std::vector<cSomaLuxEntity *> &GroupMembers(cSomaLuxEntity *apEnt)
	{
		const tString &sGroup = State(apEnt).msGroup;
		auto it = gmapGroups.find(sGroup);
		if (it != gmapGroups.end())
			return it->second;
		std::vector<cSomaLuxEntity *> &vOut = gmapGroups[sGroup];
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		if (pMap && sGroup != "")
			for (cSomaLuxEntity *pEnt : pMap->GetEntities())
				if (pEnt->meType == eSomaLuxEntityType_Critter && pEnt->mbActive && State(pEnt).msGroup == sGroup)
					vOut.push_back(pEnt);
		return vOut;
	}

	typedef cSomaLuxEntity E;

	bool Cast(void *apEnt, const float *avStart, const float *avEnd, float *afDist, float *avNormal)
	{
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		if (pMap == NULL)
			return false;
		cFirstHit hit;
		hit.mpIgnore = (cSomaLuxEntity *)apEnt;
		pMap->GetWorld()->GetPhysicsWorld()->CastRay(&hit, cVector3f(avStart[0], avStart[1], avStart[2]), cVector3f(avEnd[0], avEnd[1], avEnd[2]), true, true, false);
		*afDist = hit.mfDist;
		memcpy(avNormal, hit.mvNormal.v, sizeof(float) * 3);
		return true;
	}

	OhplGs7CritterHost Host(E *p) { return {p, Cast}; }

	void LoadProps(E *p, OhplGs7CritterProps &r)
	{
		memcpy(r.wanted_vel, Prop<cVector3f>(p, "mvWantedVel").v, sizeof(float) * 3);
		memcpy(r.gravity_vel, Prop<cVector3f>(p, "mvGravityVel").v, sizeof(float) * 3);
		memcpy(r.ground_normal, Prop<cVector3f>(p, "mvGroundNormal").v, sizeof(float) * 3);
		memcpy(r.wall_avoid_normal, Prop<cVector3f>(p, "mvWallAvoidNormal").v, sizeof(float) * 3);
		memcpy(r.wall_avoid_position, Prop<cVector3f>(p, "mvWallAvoidPosition").v, sizeof(float) * 3);
		r.max_velocity = Prop<float>(p, "mfMaxVelocity");
		r.max_turn_speed = Prop<float>(p, "mfMaxTurnSpeed");
		r.turn_speed_mul = Prop<float>(p, "mfTurnSpeedMul");
		r.max_gravity_velocity = Prop<float>(p, "mfMaxGravityVelocity");
		r.forward_rotation_y_offset = Prop<float>(p, "mfForwardRotationYOffset");
		r.anim_state = Prop<int>(p, "mlAnimState");
		r.wall_avoid_detected = Prop<bool>(p, "mbWallAvoidDetected");
	}

	void StoreProps(E *p, const OhplGs7CritterProps &r)
	{
		memcpy(Prop<cVector3f>(p, "mvWantedVel").v, r.wanted_vel, sizeof(float) * 3);
		memcpy(Prop<cVector3f>(p, "mvGravityVel").v, r.gravity_vel, sizeof(float) * 3);
		memcpy(Prop<cVector3f>(p, "mvGroundNormal").v, r.ground_normal, sizeof(float) * 3);
		memcpy(Prop<cVector3f>(p, "mvWallAvoidNormal").v, r.wall_avoid_normal, sizeof(float) * 3);
		memcpy(Prop<cVector3f>(p, "mvWallAvoidPosition").v, r.wall_avoid_position, sizeof(float) * 3);
		Prop<bool>(p, "mbWallAvoidDetected") = r.wall_avoid_detected;
	}

	cVector3f FlockingAdd(E *p, float afCenterMul, float afCenterYMul, float afSep, float afAlign, float afCoh, int alMaxChecks, float afTimeStep)
	{
		cVector3f vPos = p->GetPosition();
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		cSomaLuxEntity *pCenter = pMap ? pMap->GetEntity(State(p).msGroup) : NULL;
		cVector3f vCenter = pCenter ? pCenter->GetPosition() : cVector3f(0);
		std::vector<OhplGs7FlockMember> vMembers;
		for (cSomaLuxEntity *pOther : GroupMembers(p))
		{
			OhplGs7FlockMember m;
			memcpy(m.pos, pOther->GetPosition().v, sizeof(float) * 3);
			memcpy(m.vel, State(pOther).mRust.vel, sizeof(float) * 3);
			m.is_self = pOther == p;
			vMembers.push_back(m);
		}
		OhplGs7Flock f = {afCenterMul, afCenterYMul, afSep, afAlign, afCoh, alMaxChecks, afTimeStep};
		cVector3f vOut;
		ohpl_gs7_critter_flocking_add(vPos.v, State(p).mRust.vel, pCenter != NULL, vCenter.v, vMembers.data(), vMembers.size(), &f, vOut.v);
		return vOut;
	}

	cVector3f WanderAdd(E *p, float afLength, float afRadius, float afTimeStep, bool ab3D)
	{
		cCritterState &s = State(p);
		cVector3f vWanted = Prop<cVector3f>(p, "mvWantedVel"), vOut;
		ohpl_gs7_critter_wander_add(&s.mRust, vWanted.v, afLength, afRadius, afTimeStep, ab3D, vOut.v);
		return vOut;
	}

	bool FarExceeded(E *p)
	{
		return ohpl_gs7_critter_far_exceeded(PlayerPos().v, p->GetPosition().v, p->mVars.GetVarFloat("MaxEnabledDistance", 60));
	}
}

bool SomaRaycast(cSomaLuxEntity *apEnt, const cVector3f &avStart, const cVector3f &avEnd, float &afDist, cVector3f &avNormal)
{
	if ((avEnd - avStart).SqrLength() < 1e-8f)
		return false;
	float fDist;
	cVector3f vNormal;
	if (Cast(apEnt, avStart.v, avEnd.v, &fDist, vNormal.v) == false)
		return false;
	afDist = fDist;
	avNormal = vNormal;
	return fDist >= 0;
}

void SomaUpdateCritter(cSomaLuxEntity *apEnt, float afTimeStep)
{
	if (apEnt->mpCritterProps == NULL || afTimeStep <= 0)
		return;
	cCritterState &s = State(apEnt);
	OhplGs7CritterProps props;
	LoadProps(apEnt, props);
	OhplGs7CritterHost host = Host(apEnt);
	OhplGs7CritterStep step;
	ohpl_gs7_critter_update(&s.mRust, &props, &host, apEnt->GetPosition().v, afTimeStep, &step);
	StoreProps(apEnt, props);

	cMatrixf mtx = cMath::MatrixMul(cMath::MatrixRotateY(step.yaw), cMath::MatrixRotateX(step.pitch));
	mtx.SetTranslation(cVector3f(step.pos[0], step.pos[1], step.pos[2]));
	apEnt->SetMatrix(mtx);

	if (step.play_anim)
	{
		const tString &sAnim = Prop<tString>(apEnt, step.play_anim == 1 ? "msMoveAnim" : "msIdleAnim");
		// official skips missing anims silently (most critter .ents lack Move/Idle)
		if (apEnt->mpMesh && apEnt->mpMesh->GetAnimationStateIndex(sAnim) >= 0)
			apEnt->PlayAnimation(sAnim, 0.3f, true, "");
	}
}

void SomaInitCritterProps(cSomaLuxEntity *apEnt)
{
	Prop<float>(apEnt, "mfMaxVelocity") = 1;
	Prop<float>(apEnt, "mfMaxTurnSpeed") = 6;
	Prop<float>(apEnt, "mfTurnSpeedMul") = 10;
	Prop<float>(apEnt, "mfMaxGravityVelocity") = 5;
	Prop<cVector3f>(apEnt, "mvGroundNormal") = cVector3f(0, 1, 0);
	Prop<tString>(apEnt, "msMoveAnim") = "Move";
	Prop<tString>(apEnt, "msIdleAnim") = "Idle";
}

void SomaForgetCritter(cSomaLuxEntity *apEnt) { gmapStates.erase(apEnt); gmapGroups.clear(); }
void SomaBeginCritterFrame() { gmapGroups.clear(); }

void SomaRegisterCritterNatives(asIScriptEngine *e)
{
	const char *T = "cLuxCritter";
	if (e->GetTypeInfoByName(T) == NULL)
		return;
	SOMA_METHOD(e, T, "cVector3f Move_Normalize(const cVector3f &in avVec)", +[](E *, const cVector3f &v) { return SafeNormalize(v); });
	SOMA_METHOD(e, T, "void Move_ChangeMaxSpeed(float afGoal, float afAcc, float afTimeStep)", +[](E *p, float g, float a, float t) {
		float &f = Prop<float>(p, "mfMaxVelocity");
		f = ohpl_gs7_critter_change_max_speed(f, g, a, t);
	});
	SOMA_METHOD(e, T, "cVector3f GetPlayerHeadPos()", +[](E *) { cSomaLuxPlayer *pl = cSomaLuxPlayer::Get(); return pl && pl->GetCamera() ? pl->GetCamera()->GetPosition() : PlayerPos(); });
	SOMA_METHOD(e, T, "float GetDistanceToPlayer()", +[](E *p) { return (PlayerPos() - p->GetPosition()).Length(); });
	SOMA_METHOD(e, T, "float GetDistanceToPlayer2D()", +[](E *p) { cVector3f d = PlayerPos() - p->GetPosition(); return cVector2f(d.x, d.z).Length(); });
	SOMA_METHOD(e, T, "float GetDistanceToPos(const cVector3f&in avPos)", +[](E *p, const cVector3f &v) { return (v - p->GetPosition()).Length(); });
	SOMA_METHOD(e, T, "float GetDistanceToPos2D(const cVector3f&in avPos)", +[](E *p, const cVector3f &v) { cVector3f d = v - p->GetPosition(); return cVector2f(d.x, d.z).Length(); });
	SOMA_METHOD(e, T, "cVector3f Move_GetTowardPosAdd(const cVector3f&in avPos, bool abNormalize, float afTimeStep)", +[](E *p, const cVector3f &v, bool n, float t) {
		cVector3f vOut;
		ohpl_gs7_critter_toward_add(p->GetPosition().v, v.v, n, t, vOut.v);
		return vOut;
	});
	SOMA_METHOD(e, T, "cVector3f Move_GetTowardPlayerAdd(bool abNormalize, float afTimeStep)", +[](E *p, bool n, float t) {
		cVector3f vOut;
		ohpl_gs7_critter_toward_add(p->GetPosition().v, PlayerPos().v, n, t, vOut.v);
		return vOut;
	});
	SOMA_METHOD(e, T, "cVector3f Move_GetStopAdd(float afAmount, float afTimeStep)", +[](E *p, float a, float t) { cVector3f vOut; ohpl_gs7_critter_stop_add(Prop<cVector3f>(p, "mvWantedVel").v, a, t, vOut.v); return vOut; });
	SOMA_METHOD(e, T, "cVector3f Move_GetTowardCenterAdd(float afTimeStep)", +[](E *p, float t) {
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		cSomaLuxEntity *pCenter = pMap ? pMap->GetEntity(State(p).msGroup) : NULL;
		cVector3f vCenter = pCenter ? pCenter->GetPosition() : cVector3f(0), vOut;
		ohpl_gs7_critter_toward_center_add(p->GetPosition().v, pCenter != NULL, vCenter.v, t, vOut.v);
		return vOut;
	});
	SOMA_METHOD(e, T, "cVector3f Move_GetFlockingAdd(float afCenterMul, float afCenterYMul, float afSeparationMul, float afAlignmentMul, float afCohesionMul,int alMaxMemberChecks, float afTimeStep)",
				+[](E *p, float c, float cy, float s, float a, float h, int n, float t) { return FlockingAdd(p, c, cy, s, a, h, n, t); });
	SOMA_METHOD(e, T, "cVector3f Move_GetWanderAdd2D(float afLength, float afRadius, float afTimeStep)", +[](E *p, float l, float r, float t) { return WanderAdd(p, l, r, t, false); });
	SOMA_METHOD(e, T, "cVector3f Move_GetWanderAdd3D(float afLength, float afRadius, float afTimeStep)", +[](E *p, float l, float r, float t) { return WanderAdd(p, l, r, t, true); });
	SOMA_METHOD(e, T, "cVector3f Move_GetTowardsGroundAdd(float afMaxHeight, float afTimeStep)", +[](E *p, float h, float t) {
		cCritterState &s = State(p);
		OhplGs7CritterHost host = Host(p);
		cVector3f vOut;
		ohpl_gs7_critter_towards_ground_add(&s.mRust, &host, p->GetPosition().v, h, p->mpMesh != NULL, p->mpMesh ? p->mpMesh->GetBoundingVolume()->GetSize().y : 0, t, FarExceeded(p), vOut.v);
		return vOut;
	});
	SOMA_METHOD(e, T, "cVector3f Move_GetWallAvoidAdd(float afDistanceForward, float afTimeStep)", +[](E *p, float d, float t) {
		OhplGs7CritterProps props;
		LoadProps(p, props);
		OhplGs7CritterHost host = Host(p);
		cVector3f vOut;
		ohpl_gs7_critter_wall_avoid_add(&State(p).mRust, &props, &host, p->GetPosition().v, d, t, FarExceeded(p), vOut.v);
		StoreProps(p, props);
		return vOut;
	});
	SOMA_METHOD(e, T, "void SetGroup(const tString&in asEntityName)", +[](E *p, const tString &s) { State(p).msGroup = s; });
	SOMA_METHOD(e, T, "void SetGroup(iLuxEntity @apEntity)", +[](E *p, E *g) { State(p).msGroup = g ? g->msName : tString(); });
	SOMA_METHOD(e, T, "void SetIsFlying(bool abX)", +[](E *p, bool b) { State(p).mRust.flying = b; });
	SOMA_METHOD(e, T, "bool IsFlying()", +[](E *p) { return State(p).mRust.flying; });
	SOMA_METHOD(e, T, "void SetTestCollision(bool abX)", +[](E *p, bool b) { State(p).mRust.test_collision = b; });
	SOMA_METHOD(e, T, "bool GetTestCollision()", +[](E *p) { return State(p).mRust.test_collision; });
	SOMA_METHOD(e, T, "void SetAlignToGround(bool abX)", +[](E *p, bool b) { State(p).mRust.align_to_ground = b; });
	SOMA_METHOD(e, T, "bool GetAlignToGround()", +[](E *p) { return State(p).mRust.align_to_ground; });
	SOMA_METHOD(e, T, "void SetCanRunOnWalls(bool abX)", +[](E *p, bool b) { State(p).mRust.can_run_on_walls = b; });
	SOMA_METHOD(e, T, "bool GetCanRunOnWalls()", +[](E *p) { return State(p).mRust.can_run_on_walls; });
	SOMA_METHOD(e, T, "void SetUseRayCollision(bool abX)", +[](E *p, bool b) { State(p).mRust.use_ray_collision = b; });
	SOMA_METHOD(e, T, "bool GetUseRayCollision()", +[](E *p) { return State(p).mRust.use_ray_collision; });
	SOMA_METHOD(e, T, "void SetHealth(float afX)", +[](E *p, float x) { p->SetHealth(x); });
	SOMA_METHOD(e, T, "float GetHealth()", +[](E *p) { return p->mfHealth; });
}
