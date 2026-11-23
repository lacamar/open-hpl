// Host ops of the Rust area updates (rust/src/luxmap/aupd.rs): the engine objects liquid, sticky, slime, insanity and flashback areas reach.

#include "LuxArea.h"
#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxPlayerHelpers.h"
#include "LuxEffectHandler.h"
#include "LuxInsanityHandler.h"
#include "LuxEnemy.h"
#include "RustLuxMapA13.h"

struct OhplAreaHost
{
	static OhplA13S Str(const tString &asStr)
	{
		OhplA13S s = { reinterpret_cast<const unsigned char*>(asStr.data()), asStr.size() };
		return s;
	}
	static void Splash(iLuxArea *apArea, iPhysicsMaterial *apMaterial, iPhysicsBody *apBody, float afSurfaceY)
	{
		if(apMaterial==NULL) return;
		cSurfaceData *pSurface = apMaterial->GetSurfaceData();
		float fSpeed;
		if(apBody->IsCharacter())
			fSpeed = apBody->GetCharacterBody()->GetForceVelocity().Length();
		else
			fSpeed = apBody->GetLinearVelocity().Length();
		cSurfaceImpactData *pImpact = pSurface->GetImpactDataFromSpeed(fSpeed);
		if(pImpact == NULL) return;
		cVector3f vEffectPos = cMath::MatrixMul(apBody->GetLocalMatrix(),apBody->GetMassCentre());
		vEffectPos.y = afSurfaceY+0.01f;
		cWorld *pWorld = apArea->GetMap()->GetWorld();
		if(pImpact->GetPSName() != "")
		{
			cParticleSystem *pPS = pWorld->CreateParticleSystem("Splash", pImpact->GetPSName(),1);
			if(pPS) pPS->SetPosition(vEffectPos);
		}
		if(pImpact->GetSoundName() != "")
		{
			cSoundEntity *pSound = pWorld->CreateSoundEntity("Splash",pImpact->GetSoundName(),true);
			if(pSound) pSound->SetPosition(vEffectPos);
		}
	}
	static void Op(int32_t alOp, OhplA13A *a)
	{
		static std::vector<iPhysicsBody*> vBodies;
		static tString sTmp;
		switch(alOp)
		{
		case 100:
			a->f[0] = gpBase->mpEngine->GetStepSize();
			break;
		case 101:
		{
			iLuxArea *pArea = (iLuxArea*)a->p[0];
			a->p[1] = (uintptr_t)pArea->mpBody;
			a->p[2] = (uintptr_t)pArea->GetMap()->GetPhysicsWorld();
			cVector3f vPos = pArea->mpBody->GetWorldPosition();
			cVector3f vSize = pArea->mpBody->GetShape()->GetSize();
			a->f[0] = vPos.x; a->f[1] = vPos.y; a->f[2] = vPos.z; a->f[3] = vSize.x; a->f[4] = vSize.y; a->f[5] = vSize.z;
			memcpy(a->m, pArea->mpBody->GetWorldMatrix().v, sizeof(a->m));
			break;
		}
		case 102:
			vBodies.clear();
			((iPhysicsWorld*)a->p[0])->GetBodiesInBV(((iPhysicsBody*)a->p[1])->GetBoundingVolume(), &vBodies);
			a->i[0] = (int32_t)vBodies.size();
			a->r = (uintptr_t)vBodies.data();
			break;
		case 103:
		{
			iPhysicsBody *pBody = (iPhysicsBody*)a->p[0];
			bool bAllowed = false;
			if(a->i[0]) bAllowed = gpBase->mpPlayer->GetCurrentStateData()->AllowBuoyancy(pBody);
			bool bActive = false;
			float fDistSqr = 0;
			if(a->i[0] && bAllowed)
			{
				bActive = pBody->GetBuoyancyActive();
				fDistSqr = cMath::Vector3DistSqr(gpBase->mpPlayer->GetCamera()->GetPosition(), pBody->GetLocalPosition());
			}
			a->i[1] = bAllowed;
			a->i[2] = bActive;
			a->f[0] = fDistSqr;
			break;
		}
		case 104:
		{
			iPhysicsBody *pBody = (iPhysicsBody*)a->p[0];
			cPlanef plane;
			memcpy(&plane, a->f, sizeof(plane));
			if(a->i[0] == 0)
			{
				pBody->SetBuoyancyActive(false);
			}
			else if(a->i[0] == 1)
			{
				pBody->SetBuoyancySurface(plane);
				pBody->SetBuoyancyDensity(a->f[4]);
				pBody->SetBuoyancyLinearViscosity(a->f[5]);
				pBody->SetBuoyancyAngularViscosity(a->f[6]);
				Splash((iLuxArea*)a->p[1], (iPhysicsMaterial*)a->p[2], pBody, a->f[7]);
				pBody->SetBuoyancyActive(true);
			}
			else
			{
				pBody->SetBuoyancySurface(plane);
				pBody->Enable();
			}
			break;
		}
		case 105:
		{
			iPhysicsBody *pBody = (iPhysicsBody*)a->p[0];
			cVector3f vPos = cMath::MatrixMul(pBody->GetLocalMatrix(),pBody->GetMassCentre());
			a->f[0] = vPos.x; a->f[1] = vPos.y; a->f[2] = vPos.z;
			break;
		}
		case 106:
		{
			cLuxPlayer *pPlayer = gpBase->mpPlayer;
			iCharacterBody *pChar = ((iPhysicsBody*)a->p[0])->GetCharacterBody();
			bool bIsPlayer = pChar == pPlayer->GetCharacterBody();
			iLuxEnemy *pEnemy = bIsPlayer ? NULL : (iLuxEnemy*)pChar->GetUserData();
			a->i[0] = bIsPlayer;
			a->i[1] = pEnemy != NULL;
			a->i[2] = bIsPlayer ? pPlayer->IsInWater() : (pEnemy ? pEnemy->IsInWater() : false);
			break;
		}
		case 107:
		{
			iLuxArea *pArea = (iLuxArea*)a->p[0];
			iPhysicsMaterial *pMaterial = (iPhysicsMaterial*)a->p[2];
			iCharacterBody *pChar = ((iPhysicsBody*)a->p[1])->GetCharacterBody();
			bool bEnter = a->i[1] == 1;
			if(a->i[0])
			{
				cLuxPlayer *pPlayer = gpBase->mpPlayer;
				if(bEnter)
				{
					pPlayer->SetIsInWater(true);
					pPlayer->SetWaterSpeedMul(a->f[1]);
					if(pMaterial) pPlayer->SetWaterStepSound(pMaterial->GetSurfaceData()->GetStepType());
					Splash(pArea, pMaterial, pChar->GetCurrentBody(), a->f[0]);
				}
				else pPlayer->SetIsInWater(false);
			}
			else
			{
				iLuxEnemy *pEnemy = (iLuxEnemy*)pChar->GetUserData();
				if(bEnter)
				{
					pEnemy->SetInWater(true);
					pEnemy->SetWaterSurfaceY(a->f[0]);
					pEnemy->SetWaterSurfaceData(pMaterial->GetSurfaceData());
				}
				else pEnemy->SetInWater(false);
			}
			break;
		}
		case 108:
			sTmp = ((iPhysicsBody*)a->p[0])->GetName();
			a->rs[0] = Str(sTmp);
			break;
		case 109:
		{
			iPhysicsBody *pBody = (iPhysicsBody*)a->p[0];
			iPhysicsBody *pAreaBody = (iPhysicsBody*)a->p[1];
			if(a->i[0])
				a->i[0] = cMath::CheckPointInBVIntersection(cMath::MatrixMul(pBody->GetLocalMatrix(),pBody->GetMassCentre()), *pAreaBody->GetBoundingVolume());
			else
				a->i[0] = cMath::CheckBVIntersection(*pBody->GetBoundingVolume(), *pAreaBody->GetBoundingVolume());
			break;
		}
		case 111:
		{
			iPhysicsBody *pBody = (iPhysicsBody*)a->p[0];
			gpBase->mpPlayer->GetCurrentStateData()->OnAttachBodyToStickyArea(pBody);
			memcpy(a->m, pBody->GetLocalMatrix().v, sizeof(a->m));
			a->i[0] = pBody->GetGravity();
			a->f[0] = pBody->GetMass();
			iLuxEntity *pEntity = (iLuxEntity*)pBody->GetUserData();
			a->i[1] = pEntity ? (int)pEntity->GetFullGameSave() : -1;
			break;
		}
		case 112:
		{
			iPhysicsBody *pBody = (iPhysicsBody*)a->p[0];
			pBody->SetLinearVelocity(0);
			pBody->SetAngularVelocity(0);
			pBody->SetGravity(false);
			pBody->SetMass(0);
			iLuxEntity *pEntity = (iLuxEntity*)pBody->GetUserData();
			if(pEntity) pEntity->SetFullGameSave(true);
			break;
		}
		case 113:
		{
			iPhysicsBody *pBody = (iPhysicsBody*)a->p[0];
			pBody->SetGravity(a->i[0] != 0);
			pBody->SetMass(a->f[0]);
			pBody->Enable();
			iLuxEntity *pEntity = (iLuxEntity*)pBody->GetUserData();
			if(pEntity) pEntity->SetFullGameSave(a->i[1] != 0);
			break;
		}
		case 114:
		{
			iLuxArea *pArea = (iLuxArea*)a->p[0];
			cWorld *pWorld = pArea->GetMap()->GetWorld();
			tString sName((const char*)a->s[0].p, a->s[0].n);
			if(a->i[0] == 0)
			{
				cSoundEntity *pSound = pWorld->CreateSoundEntity(a->i[1] ? "DetachSound" : "AttachSound", sName, true);
				if(pSound) pSound->SetPosition(pArea->mpBody->GetWorldPosition());
			}
			else
			{
				cParticleSystem *pPS = pWorld->CreateParticleSystem(a->i[1] ? "DetachPS" : "AttachPS", sName, 1);
				if(pPS) pPS->SetMatrix(pArea->mpBody->GetWorldMatrix());
			}
			break;
		}
		case 115:
		{
			iPhysicsBody *pBody = (iPhysicsBody*)a->p[0];
			cVector3f vCentre = pBody->GetMassCentre();
			cVector3f vPos = pBody->GetWorldPosition();
			a->f[0] = vCentre.x; a->f[1] = vCentre.y; a->f[2] = vCentre.z;
			a->f[3] = vPos.x; a->f[4] = vPos.y; a->f[5] = vPos.z;
			break;
		}
		case 116:
		{
			iPhysicsBody *pBody = (iPhysicsBody*)a->p[0];
			if(a->i[0] == 1)
			{
				cMatrixf mtx;
				memcpy(mtx.v, a->m, sizeof(a->m));
				pBody->SetMatrix(mtx);
			}
			else pBody->SetPosition(cVector3f(a->f[0], a->f[1], a->f[2]));
			break;
		}
		case 117:
			a->i[0] = ((iLuxArea*)a->p[0])->CollidesWithPlayer();
			break;
		case 118:
			((iLuxEntity*)a->p[0])->SetActive(a->i[0] != 0);
			break;
		case 119:
			gpBase->mpPlayer->GiveDamage(a->f[0], 1, eLuxDamageType_BloodSplat, true, false);
			break;
		case 120:
			((iLuxEntity*)a->p[0])->PlaySound("SlimeAttack", tString((const char*)a->s[0].p, a->s[0].n), true, false);
			break;
		case 121:
		{
			iLuxArea *pArea = (iLuxArea*)a->p[0];
			cParticleSystem *pPS = pArea->GetMap()->GetWorld()->CreateParticleSystem("SlimeAttack", tString((const char*)a->s[0].p, a->s[0].n), 1);
			if(pPS) pPS->SetPosition(pArea->mpBody->GetLocalPosition());
			break;
		}
		case 122:
			gpBase->mpEffectHandler->GetScreenShake()->Start(a->f[0], 0.5, 0.1, 0.3f);
			break;
		case 123:
			if(a->i[0] == 0) a->i[0] = gpBase->mpInsanityHandler->NewEventIsPossible();
			else gpBase->mpInsanityHandler->StartEvent();
			break;
		case 124:
		{
			gpBase->mpPlayer->GetHelperFlashback()->Start(tString((const char*)a->s[0].p, a->s[0].n), tString((const char*)a->s[1].p, a->s[1].n));
			break;
		}
		}
	}
};
void OhplAreaHostOp(int32_t alOp, OhplA13A *a)
{
	OhplAreaHost::Op(alOp, a);
}