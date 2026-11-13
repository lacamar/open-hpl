#include "LuxEnemy.h"

#include "LuxEnemyMover.h"
#include "LuxEnemyPathfinder.h"

#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxEffectRenderer.h"
#include "LuxDebugHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxProp.h"
#include "LuxProp_Object.h"

// Host table of the Rust enemy logic: one engine call per op, no decisions.

static cVector3f A4Get(const float *apSrc)
{
	return cVector3f(apSrc[0], apSrc[1], apSrc[2]);
}

static void A4Put(float *apDest, const cVector3f &avV)
{
	apDest[0] = avV.x; apDest[1] = avV.y; apDest[2] = avV.z;
}

OhplA4Host iLuxEnemy::A4Host()
{
	OhplA4Host host;
	host.ctx = this;
	host.call = &iLuxEnemy::A4Call;
	return host;
}

int iLuxEnemy::A4Call(void *apEnemy, unsigned alOp, OhplA4Io *apIo)
{
	iLuxEnemy *pEnemy = static_cast<iLuxEnemy*>(apEnemy);
	cLuxPlayer *pPlayer = gpBase->mpPlayer;
	cAnimationState *pAnim = static_cast<cAnimationState*>(apIo->p[0]);
	cSoundEntity *pSound = static_cast<cSoundEntity*>(apIo->p[0]);
	float *f = apIo->f;

	switch(alOp)
	{
	case kOhplA4Op_GA3:				return Ga3Call(pEnemy, apIo->i[0], static_cast<OhplGa3Io*>(apIo->p[0]));
	case kOhplA4Op_STEP_SIZE:		f[0] = gpBase->mpEngine->GetStepSize(); return 0;
	case kOhplA4Op_RAND_RECT:		f[0] = cMath::RandRectf(f[0], f[1]); return 0;
	case kOhplA4Op_UPDATE_STEP:
		switch(apIo->i[0])
		{
		case 0:		pEnemy->mpPathfinder->OnUpdate(f[0]); break;
		case 1:		pEnemy->mpMover->OnUpdate(f[0]); break;
		case 2:		pEnemy->UpdateCanSeePlayer(f[0]); break;
		case 3:		pEnemy->UpdatePlayerDetected(f[0]); break;
		case 4:		pEnemy->UpdatePlayerInRange(f[0]); break;
		case 5:		pEnemy->UpdateCheckStuckAtDoor(f[0]); break;
		case 6:		pEnemy->UpdateCheckLastPlayerPos(f[0]); break;
		case 7:		pEnemy->UpdateStateMachine(f[0]); break;
		case 8:		pEnemy->UpdateEnemySpecific(f[0]); break;
		case 9:		pEnemy->UpdateCharBody(f[0]); break;
		case 10:	pEnemy->UpdateRegenHealth(f[0]); break;
		}
		return 0;

	case kOhplA4Op_ANIM_NAME_EQ:	return pAnim->GetName() == static_cast<const char*>(apIo->p[1]) ? 1 : 0;
	case kOhplA4Op_ANIM_IS_ACTIVE:	return pAnim->IsActive() ? 1 : 0;
	case kOhplA4Op_ANIM_IS_OVER:	return pAnim->IsOver() ? 1 : 0;
	case kOhplA4Op_ANIM_FIND:		apIo->p[1] = pEnemy->mpMeshEntity->GetAnimationStateFromName(static_cast<const char*>(apIo->p[0])); return 0;
	case kOhplA4Op_WARN_NO_ANIM:	Warning("Animation '%s' does not exist!\n", static_cast<const char*>(apIo->p[0])); return 0;
	case kOhplA4Op_ANIM_SET_ACTIVE:	pAnim->SetActive(apIo->i[0] != 0); return 0;
	case kOhplA4Op_ANIM_FADE_OUT:	pAnim->FadeOut(f[0]); return 0;
	case kOhplA4Op_ANIM_IS_FADING:	return pAnim->IsFading() ? 1 : 0;
	case kOhplA4Op_ANIM_SET_WEIGHT:	pAnim->SetWeight(f[0]); return 0;
	case kOhplA4Op_ANIM_FADE_IN:	pAnim->FadeIn(f[0]); return 0;
	case kOhplA4Op_ANIM_SET_LOOP:	pAnim->SetLoop(apIo->i[0] != 0); return 0;
	case kOhplA4Op_ANIM_REL_TIME:	f[0] = pAnim->GetRelativeTimePosition(); return 0;
	case kOhplA4Op_ANIM_SET_REL_TIME:	pAnim->SetRelativeTimePosition(f[0]); return 0;
	case kOhplA4Op_ANIM_SET_TIME:	pAnim->SetTimePosition(f[0]); return 0;
	case kOhplA4Op_ANIM_IS_LOOPING:	return pAnim->IsLooping() ? 1 : 0;
	case kOhplA4Op_ANIM_PREV_TIME:	f[0] = pAnim->GetPreviousTimePosition(); return 0;
	case kOhplA4Op_ANIM_EVENT_TIME:	f[0] = pAnim->GetSpecialEventTime(); return 0;
	case kOhplA4Op_ANIM_TIME:		f[0] = pAnim->GetTimePosition(); return 0;
	case kOhplA4Op_ANIM_SET_SPEED:	pAnim->SetSpeed(f[0]); return 0;
	case kOhplA4Op_ANIM_LENGTH:		f[0] = pAnim->GetLength(); return 0;

	case kOhplA4Op_SOUND_EXISTS:	return pEnemy->mpMap->GetWorld()->SoundEntityExists(pSound, apIo->i[0]) ? 1 : 0;
	case kOhplA4Op_SOUND_FADE_OUT:	pSound->FadeOut(f[0]); return 0;
	case kOhplA4Op_SOUND_CREATE:	apIo->p[1] = pEnemy->PlaySound(pEnemy->msAmbientSound[apIo->i[0]]); return 0;
	case kOhplA4Op_SOUND_ID:		return pSound->GetCreationID();
	case kOhplA4Op_AMBIENT_EMPTY:	return pEnemy->msAmbientSound[apIo->i[0]] == "" ? 1 : 0;

	case kOhplA4Op_CAM_COLLIDE_MESH:
		return pPlayer->GetCamera()->GetFrustum()->CollideBoundingVolume(pEnemy->mpMeshEntity->GetBoundingVolume()) != eCollision_Outside ? 1 : 0;
	case kOhplA4Op_CAM_POS:			A4Put(f, pPlayer->GetCamera()->GetPosition()); return 0;
	case kOhplA4Op_PLAYER_SANITY:	f[0] = pPlayer->GetSanity(); return 0;
	case kOhplA4Op_SUBMESH_NUM:		return pEnemy->mpMeshEntity->GetSubMeshEntityNum();
	case kOhplA4Op_ADD_GLOW:		gpBase->mpEffectRenderer->AddEnemyGlow(pEnemy->mpMeshEntity->GetSubMeshEntity(apIo->i[0]), f[0]); return 0;
	case kOhplA4Op_LIGHT_LEVEL_AT:	f[0] = gpBase->mpMapHelper->GetLightLevelAtPos(A4Get(f)); return 0;
	case kOhplA4Op_MESH_VISIBLE:	return pEnemy->mpMeshEntity->IsVisible() ? 1 : 0;

	case kOhplA4Op_FADE_TO_SMOKE:	pEnemy->FadeToSmoke(apIo->i[0] != 0); return 0;
	case kOhplA4Op_GROUND_RAY:
	{
		float fDist = 0;
		cVector3f vNormal;
		bool bHit = gpBase->mpMapHelper->GetClosestCharCollider(A4Get(f), cVector3f(0,-1,0), 0.5f, &fDist, &vNormal, NULL);
		f[3] = fDist;
		return bHit ? 1 : 0;
	}
	case kOhplA4Op_SET_ENTITY_OFFSET:
	{
		cMatrixf mtxOffset = pEnemy->m_mtxCharMeshOffset;
		mtxOffset.SetTranslation(mtxOffset.GetTranslation() + cVector3f(0,-f[0],0));
		pEnemy->mpCharBody->SetEntityOffset(mtxOffset);
		return 0;
	}

	case kOhplA4Op_DEBUG_DAMAGE:	gpBase->mpDebugHandler->AddMessage(_W("Enemy damage ") + cString::ToStringW(f[0]), false); return 0;
	case kOhplA4Op_DAMAGE_MUL:		f[0] = pEnemy->GetDamageMul(f[0], apIo->i[0]); return 0;
	case kOhplA4Op_ON_DAMAGE:		pEnemy->OnDamage(f[0], apIo->i[0]); return 0;
	case kOhplA4Op_CALLBACK_DEATH:	pEnemy->RunCallbackFunc("OnDeath"); return 0;
	case kOhplA4Op_ATTACK_SHAPE:
	{
		const cEnemyAttackSizeData &aSize = *static_cast<const cEnemyAttackSizeData*>(apIo->p[0]);
		const cEnemyAttackDamageData &aDamage = *static_cast<const cEnemyAttackDamageData*>(apIo->p[1]);
		bool bHitPlayer = false;
		bool bHit = gpBase->mpMapHelper->ShapeDamage(pEnemy->GetAttackShape(aSize.mlShapeIdx), pEnemy->GetDamageShapeMatrix(aSize.mvOffset), pEnemy->mpCharBody->GetPosition(),
													aDamage.mfMinDamage*f[0], aDamage.mfMaxDamage*f[0],
													aDamage.mfForce, aDamage.mfMaxImpulse,
													aDamage.mlStrength, aDamage.mfHitSpeed,
													aDamage.mDamageType, aDamage.mWeaponHitType,
													false, aDamage.mbCheckPlayer, aDamage.mbCheckProps, true,
													&bHitPlayer);
		return (bHit ? 1 : 0) | (bHitPlayer ? 2 : 0);
	}
	case kOhplA4Op_ATTACK_HIT_SOUND:	pEnemy->PlaySound(static_cast<const cEnemyAttackDamageData*>(apIo->p[0])->msHitSound); return 0;

	case kOhplA4Op_ON_SET_ACTIVE_SPECIFIC:	pEnemy->OnSetActiveEnemySpecific(apIo->i[0] != 0); return 0;
	case kOhplA4Op_BODY_EXISTS:		return pEnemy->mpCharBody ? 1 : 0;
	case kOhplA4Op_MESH_EXISTS:		return pEnemy->mpMeshEntity ? 1 : 0;
	case kOhplA4Op_MESH_SET_ACTIVE:	pEnemy->mpMeshEntity->SetActive(apIo->i[0] != 0); pEnemy->mpMeshEntity->SetVisible(apIo->i[0] != 0); return 0;
	case kOhplA4Op_ON_RESET_PROPERTIES:	pEnemy->OnResetProperties(); return 0;
	case kOhplA4Op_BODY_SET_FEET:	pEnemy->mpCharBody->SetFeetPosition(A4Get(f)); return 0;

	case kOhplA4Op_FRUSTUM_BODY:
		return pPlayer->GetCamera()->GetFrustum()->CollideBoundingVolume(pEnemy->mpCharBody->GetCurrentBody()->GetBoundingVolume()) != eCollision_Outside ? 1 : 0;
	case kOhplA4Op_FRUSTUM_BODY_AT:
	{
		cBoundingVolume bv;
		bv = *pEnemy->mpCharBody->GetCurrentBody()->GetBoundingVolume();
		bv.SetPosition(A4Get(f));
		return pPlayer->GetCamera()->GetFrustum()->CollideBoundingVolume(&bv) != eCollision_Outside ? 1 : 0;
	}

	case kOhplA4Op_FOOD_BEGIN:
	{
		pEnemy->mvA4Entities.clear();
		cLuxEntityIterator it = pEnemy->mpMap->GetEntityIterator();
		while(it.HasNext()) pEnemy->mvA4Entities.push_back(it.Next());
		return (int)pEnemy->mvA4Entities.size();
	}
	case kOhplA4Op_FOOD_IS_OBJECT:
	{
		iLuxEntity *pEntity = pEnemy->mvA4Entities[apIo->i[0]];
		if(pEntity->GetEntityType() != eLuxEntityType_Prop) return 0;
		return static_cast<iLuxProp*>(pEntity)->GetPropType() == eLuxPropType_Object ? 1 : 0;
	}
	case kOhplA4Op_FOOD_IS_FOOD:
	{
		cLuxProp_Object *pObject = static_cast<cLuxProp_Object*>(pEnemy->mvA4Entities[apIo->i[0]]);
		apIo->p[1] = pObject;
		return pObject->IsFood() ? 1 : 0;
	}
	case kOhplA4Op_FOOD_LOWEST_Y:	f[0] = static_cast<cLuxProp_Object*>(pEnemy->mvA4Entities[apIo->i[0]])->GetBody(0)->GetBoundingVolume()->GetMin().y; return 0;
	case kOhplA4Op_FOOD_POS:		A4Put(f, static_cast<cLuxProp_Object*>(pEnemy->mvA4Entities[apIo->i[0]])->GetBody(0)->GetLocalPosition()); return 0;

	case kOhplA4Op_BODY_RIGHT_UP_FWD:
		A4Put(f, pEnemy->mpCharBody->GetRight());
		A4Put(f+3, pEnemy->mpCharBody->GetUp());
		A4Put(f+6, pEnemy->mpCharBody->GetForward());
		return 0;
	}
	return 0;
}
