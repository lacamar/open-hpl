#include "SomaCritter.h"
#include "SomaAgent.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxPlayer.h"
#include "SomaScriptApi.h"
#include "SomaScriptBind.h"

#include <angelscript.h>
#include <cmath>
#include <unordered_map>

namespace
{
	struct cCritterState
	{
		bool mbFlying = false;
		bool mbTestCollision = true;
		bool mbAlignToGround = false;
		bool mbCanRunOnWalls = false;
		bool mbUseRayCollision = false;
		bool mbInit = false;
		tString msGroup;
		cVector3f mvVel = 0;
		float mfWanderAngle = 0, mfWanderPitch = 0;
		float mfYaw = 0, mfPitch = 0;
		int mlPlayingAnimState = -1;
	};
	std::unordered_map<cSomaLuxEntity *, cCritterState> gmapStates;

	cCritterState &State(cSomaLuxEntity *apEnt)
	{
		cCritterState &s = gmapStates[apEnt];
		if (s.mbInit == false)
		{
			s.mbInit = true;
			s.msGroup = apEnt->mInstanceVars.GetVarString("GroupEntityName", "");
			cVector3f vFwd = apEnt->GetMatrix().GetForward();
			s.mfYaw = std::atan2(vFwd.x, vFwd.z);
		}
		return s;
	}

	template <class T> T &Prop(cSomaLuxEntity *apEnt, const char *asName)
	{
		return *(T *)(apEnt->mpCritterProps + SomaIndirectPropOffset("cLuxCritter", asName));
	}

	cVector3f SafeNormalize(const cVector3f &v)
	{
		float fLen = v.Length();
		return fLen > 1e-6f ? v / fLen : cVector3f(0);
	}

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

	void GroupMembers(cSomaLuxEntity *apEnt, std::vector<cSomaLuxEntity *> &avOut)
	{
		const tString &sGroup = State(apEnt).msGroup;
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		if (pMap == NULL || sGroup == "")
			return;
		for (cSomaLuxEntity *pEnt : pMap->GetEntities())
			if (pEnt->meType == eSomaLuxEntityType_Critter && pEnt->mbActive && State(pEnt).msGroup == sGroup)
				avOut.push_back(pEnt);
	}

	float Wrap(float a)
	{
		while (a > kPif) a -= k2Pif;
		while (a < -kPif) a += k2Pif;
		return a;
	}

	float TurnTowards(float afFrom, float afTo, float afMaxStep)
	{
		float fDiff = Wrap(afTo - afFrom);
		return afFrom + cMath::Clamp(fDiff, -afMaxStep, afMaxStep);
	}

	typedef cSomaLuxEntity E;

	cVector3f FlockingAdd(E *p, float afCenterMul, float afCenterYMul, float afSep, float afAlign, float afCoh, int alMaxChecks, float afTimeStep)
	{
		cVector3f vPos = p->GetPosition();
		cVector3f vAdd = 0;
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		if (cSomaLuxEntity *pCenter = pMap ? pMap->GetEntity(State(p).msGroup) : NULL)
		{
			cVector3f vToCenter = pCenter->GetPosition() - vPos;
			vToCenter.y *= afCenterYMul;
			vAdd += vToCenter * afCenterMul;
		}
		std::vector<cSomaLuxEntity *> vMembers;
		GroupMembers(p, vMembers);
		if (vMembers.empty())
			return vAdd * afTimeStep;
		cVector3f vMean = 0, vSep = 0, vVel = 0;
		int lChecked = 0;
		for (cSomaLuxEntity *pOther : vMembers)
		{
			cVector3f vOther = pOther->GetPosition();
			vMean += vOther;
			if (pOther == p || lChecked >= alMaxChecks)
				continue;
			++lChecked;
			cVector3f vDelta = vPos - vOther;
			float fSqr = vDelta.SqrLength();
			if (fSqr > 1e-6f && fSqr < 4.0f)
				vSep += vDelta / fSqr;
			vVel += State(pOther).mvVel;
		}
		vMean = vMean / (float)vMembers.size();
		vAdd += (vMean - vPos) * afCoh + vSep * afSep;
		if (lChecked > 0)
			vAdd += (vVel / (float)lChecked - State(p).mvVel) * afAlign;
		return vAdd * afTimeStep;
	}

	cVector3f WanderAdd(E *p, float afLength, float afRadius, float afTimeStep, bool ab3D)
	{
		cCritterState &s = State(p);
		s.mfWanderAngle += cMath::RandRectf(-1, 1) * 2.0f * afTimeStep * 10.0f;
		if (ab3D)
			s.mfWanderPitch = cMath::Clamp(s.mfWanderPitch + cMath::RandRectf(-1, 1) * afTimeStep * 10.0f, -1.0f, 1.0f);
		cVector3f vFwd = SafeNormalize(Prop<cVector3f>(p, "mvWantedVel"));
		if (vFwd.SqrLength() == 0)
			vFwd = cVector3f(std::sin(s.mfYaw), 0, std::cos(s.mfYaw));
		cVector3f vCircle(std::sin(s.mfWanderAngle), ab3D ? std::sin(s.mfWanderPitch) : 0, std::cos(s.mfWanderAngle));
		cVector3f vAdd = vFwd * afLength + vCircle * afRadius;
		if (ab3D == false)
			vAdd.y = 0;
		return vAdd * afTimeStep;
	}
}

bool SomaRaycast(cSomaLuxEntity *apEnt, const cVector3f &avStart, const cVector3f &avEnd, float &afDist, cVector3f &avNormal)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap == NULL || (avEnd - avStart).SqrLength() < 1e-8f)
		return false;
	cFirstHit hit;
	hit.mpIgnore = apEnt;
	pMap->GetWorld()->GetPhysicsWorld()->CastRay(&hit, avStart, avEnd, true, true, false);
	afDist = hit.mfDist;
	avNormal = hit.mvNormal;
	return hit.mfDist >= 0;
}

void SomaUpdateCritter(cSomaLuxEntity *apEnt, float afTimeStep)
{
	if (apEnt->mpCritterProps == NULL || afTimeStep <= 0)
		return;
	cCritterState &s = State(apEnt);
	cVector3f &vWanted = Prop<cVector3f>(apEnt, "mvWantedVel");
	float fMax = Prop<float>(apEnt, "mfMaxVelocity");
	if (vWanted.Length() > fMax)
		vWanted = SafeNormalize(vWanted) * fMax;

	cVector3f vVel = vWanted;
	if (s.mbFlying == false)
	{
		cVector3f &vGravity = Prop<cVector3f>(apEnt, "mvGravityVel");
		vGravity.y = std::max(vGravity.y - 9.8f * afTimeStep, -Prop<float>(apEnt, "mfMaxGravityVelocity"));
		vVel += vGravity;
	}

	cVector3f vPos = apEnt->GetPosition();
	cVector3f vNew = vPos + vVel * afTimeStep;
	float fDist;
	cVector3f vNormal;
	if (s.mbFlying == false && SomaRaycast(apEnt, vNew + cVector3f(0, 0.3f, 0), vNew - cVector3f(0, 0.05f, 0), fDist, vNormal))
	{
		vNew.y = vNew.y + 0.3f - fDist;
		Prop<cVector3f>(apEnt, "mvGravityVel") = 0;
		Prop<cVector3f>(apEnt, "mvGroundNormal") = vNormal;
	}
	if (s.mbTestCollision && SomaRaycast(apEnt, vPos, vNew, fDist, vNormal))
	{
		vNew = vPos;
		vWanted = 0;
	}
	s.mvVel = (vNew - vPos) / afTimeStep;

	cVector3f vDir = s.mbFlying ? vVel : cVector3f(vVel.x, 0, vVel.z);
	if (vDir.Length() > 0.01f)
	{
		float fTurn = Prop<float>(apEnt, "mfMaxTurnSpeed") * afTimeStep;
		float fMul = Prop<float>(apEnt, "mfTurnSpeedMul") * afTimeStep;
		float fYaw = std::atan2(vDir.x, vDir.z) + Prop<float>(apEnt, "mfForwardRotationYOffset");
		s.mfYaw = TurnTowards(s.mfYaw, fYaw, std::min(fTurn, std::fabs(Wrap(fYaw - s.mfYaw)) * fMul + 1e-4f));
		if (s.mbFlying)
		{
			float fPitch = -std::atan2(vDir.y, std::sqrt(vDir.x * vDir.x + vDir.z * vDir.z));
			s.mfPitch = TurnTowards(s.mfPitch, fPitch, fTurn);
		}
	}
	cMatrixf mtx = cMath::MatrixMul(cMath::MatrixRotateY(s.mfYaw), cMath::MatrixRotateX(s.mfPitch));
	mtx.SetTranslation(vNew);
	apEnt->SetMatrix(mtx);

	int lAnimState = Prop<int>(apEnt, "mlAnimState");
	if (lAnimState != s.mlPlayingAnimState)
	{
		s.mlPlayingAnimState = lAnimState;
		const tString &sAnim = Prop<tString>(apEnt, lAnimState == 1 ? "msMoveAnim" : "msIdleAnim");
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

void SomaForgetCritter(cSomaLuxEntity *apEnt) { gmapStates.erase(apEnt); }

void SomaRegisterCritterNatives(asIScriptEngine *e)
{
	const char *T = "cLuxCritter";
	if (e->GetTypeInfoByName(T) == NULL)
		return;
	SOMA_METHOD(e, T, "cVector3f Move_Normalize(const cVector3f &in avVec)", +[](E *, const cVector3f &v) { return SafeNormalize(v); });
	SOMA_METHOD(e, T, "void Move_ChangeMaxSpeed(float afGoal, float afAcc, float afTimeStep)", +[](E *p, float g, float a, float t) {
		float &f = Prop<float>(p, "mfMaxVelocity");
		f = f < g ? std::min(f + a * t, g) : std::max(f - a * t, g);
	});
	SOMA_METHOD(e, T, "cVector3f GetPlayerHeadPos()", +[](E *) { cSomaLuxPlayer *pl = cSomaLuxPlayer::Get(); return pl && pl->GetCamera() ? pl->GetCamera()->GetPosition() : PlayerPos(); });
	SOMA_METHOD(e, T, "float GetDistanceToPlayer()", +[](E *p) { return (PlayerPos() - p->GetPosition()).Length(); });
	SOMA_METHOD(e, T, "float GetDistanceToPlayer2D()", +[](E *p) { cVector3f d = PlayerPos() - p->GetPosition(); return cVector2f(d.x, d.z).Length(); });
	SOMA_METHOD(e, T, "float GetDistanceToPos(const cVector3f&in avPos)", +[](E *p, const cVector3f &v) { return (v - p->GetPosition()).Length(); });
	SOMA_METHOD(e, T, "float GetDistanceToPos2D(const cVector3f&in avPos)", +[](E *p, const cVector3f &v) { cVector3f d = v - p->GetPosition(); return cVector2f(d.x, d.z).Length(); });
	SOMA_METHOD(e, T, "cVector3f Move_GetTowardPosAdd(const cVector3f&in avPos, bool abNormalize, float afTimeStep)", +[](E *p, const cVector3f &v, bool n, float t) {
		cVector3f d = v - p->GetPosition();
		return n ? SafeNormalize(d) : d * t;
	});
	SOMA_METHOD(e, T, "cVector3f Move_GetTowardPlayerAdd(bool abNormalize, float afTimeStep)", +[](E *p, bool n, float t) {
		cVector3f d = PlayerPos() - p->GetPosition();
		return n ? SafeNormalize(d) : d * t;
	});
	SOMA_METHOD(e, T, "cVector3f Move_GetStopAdd(float afAmount, float afTimeStep)", +[](E *p, float a, float t) { return Prop<cVector3f>(p, "mvWantedVel") * (-a * t); });
	SOMA_METHOD(e, T, "cVector3f Move_GetTowardCenterAdd(float afTimeStep)", +[](E *p, float t) {
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		cSomaLuxEntity *pCenter = pMap ? pMap->GetEntity(State(p).msGroup) : NULL;
		return pCenter ? (pCenter->GetPosition() - p->GetPosition()) * t : cVector3f(0);
	});
	SOMA_METHOD(e, T, "cVector3f Move_GetFlockingAdd(float afCenterMul, float afCenterYMul, float afSeparationMul, float afAlignmentMul, float afCohesionMul,int alMaxMemberChecks, float afTimeStep)",
				+[](E *p, float c, float cy, float s, float a, float h, int n, float t) { return FlockingAdd(p, c, cy, s, a, h, n, t); });
	SOMA_METHOD(e, T, "cVector3f Move_GetWanderAdd2D(float afLength, float afRadius, float afTimeStep)", +[](E *p, float l, float r, float t) { return WanderAdd(p, l, r, t, false); });
	SOMA_METHOD(e, T, "cVector3f Move_GetWanderAdd3D(float afLength, float afRadius, float afTimeStep)", +[](E *p, float l, float r, float t) { return WanderAdd(p, l, r, t, true); });
	SOMA_METHOD(e, T, "cVector3f Move_GetTowardsGroundAdd(float afMaxHeight, float afTimeStep)", +[](E *p, float h, float t) {
		cVector3f vPos = p->GetPosition(), vNormal;
		float fDist;
		if (SomaRaycast(p, vPos, vPos - cVector3f(0, h, 0), fDist, vNormal))
			return cVector3f(0, -fDist, 0) * t;
		return cVector3f(0);
	});
	SOMA_METHOD(e, T, "cVector3f Move_GetWallAvoidAdd(float afDistanceForward, float afTimeStep)", +[](E *p, float d, float t) {
		bool &bDetected = Prop<bool>(p, "mbWallAvoidDetected");
		bDetected = false;
		cVector3f vDir = SafeNormalize(Prop<cVector3f>(p, "mvWantedVel"));
		if (vDir.SqrLength() == 0 || d <= 0)
			return cVector3f(0);
		cVector3f vPos = p->GetPosition(), vNormal;
		float fDist;
		if (SomaRaycast(p, vPos, vPos + vDir * d, fDist, vNormal) == false)
			return cVector3f(0);
		bDetected = true;
		Prop<cVector3f>(p, "mvWallAvoidNormal") = vNormal;
		Prop<cVector3f>(p, "mvWallAvoidPosition") = vPos + vDir * fDist;
		return vNormal * ((1.0f - fDist / d) * t);
	});
	SOMA_METHOD(e, T, "void SetGroup(const tString&in asEntityName)", +[](E *p, const tString &s) { State(p).msGroup = s; });
	SOMA_METHOD(e, T, "void SetGroup(iLuxEntity @apEntity)", +[](E *p, E *g) { State(p).msGroup = g ? g->msName : tString(); });
	SOMA_METHOD(e, T, "void SetIsFlying(bool abX)", +[](E *p, bool b) { State(p).mbFlying = b; });
	SOMA_METHOD(e, T, "bool IsFlying()", +[](E *p) { return State(p).mbFlying; });
	SOMA_METHOD(e, T, "void SetTestCollision(bool abX)", +[](E *p, bool b) { State(p).mbTestCollision = b; });
	SOMA_METHOD(e, T, "bool GetTestCollision()", +[](E *p) { return State(p).mbTestCollision; });
	SOMA_METHOD(e, T, "void SetAlignToGround(bool abX)", +[](E *p, bool b) { State(p).mbAlignToGround = b; });
	SOMA_METHOD(e, T, "bool GetAlignToGround()", +[](E *p) { return State(p).mbAlignToGround; });
	SOMA_METHOD(e, T, "void SetCanRunOnWalls(bool abX)", +[](E *p, bool b) { State(p).mbCanRunOnWalls = b; });
	SOMA_METHOD(e, T, "bool GetCanRunOnWalls()", +[](E *p) { return State(p).mbCanRunOnWalls; });
	SOMA_METHOD(e, T, "void SetUseRayCollision(bool abX)", +[](E *p, bool b) { State(p).mbUseRayCollision = b; });
	SOMA_METHOD(e, T, "bool GetUseRayCollision()", +[](E *p) { return State(p).mbUseRayCollision; });
	SOMA_METHOD(e, T, "void SetHealth(float afX)", +[](E *p, float x) { p->SetHealth(x); });
	SOMA_METHOD(e, T, "float GetHealth()", +[](E *p) { return p->mfHealth; });
}
