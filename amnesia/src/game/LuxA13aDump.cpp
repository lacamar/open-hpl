// OHPL_A13A_DUMP: map helper answers (line of sight, light level, closest entity, shape damage) around the player at fixed updates.

#include "LuxMap.h"
#include "LuxEntity.h"
#include "LuxMapHelper.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxBase.h"

#include <stdio.h>
#include <stdlib.h>

static void Pos(FILE *f, const char *asKey, const cVector3f &v)
{
	fprintf(f, " %s=%.4f,%.4f,%.4f", asKey, v.x, v.y, v.z);
}

void OhplA13aTick(cLuxMap *apMap)
{
	static const char *sPath = getenv("OHPL_A13A_DUMP");
	if(sPath == NULL || *sPath == 0) return;
	if(apMap != gpBase->mpMapHandler->GetCurrentMap()) return;
	static int lFrame = 0;
	++lFrame;
	if(lFrame != 3 && lFrame != 40 && lFrame != 100) return;

	FILE *f = fopen(sPath, "a");
	if(f == NULL) return;
	cVector3f vPos = gpBase->mpPlayer->GetCharacterBody()->GetPosition();
	fprintf(f, "== %s frame %d", apMap->GetName().c_str(), lFrame);
	Pos(f, "player", vPos);
	fprintf(f, "\n");

	const cVector3f vDirs[6] = { cVector3f(1,0,0), cVector3f(-1,0,0), cVector3f(0,0,1), cVector3f(0,0,-1), cVector3f(0,1,0), cVector3f(0,-1,0) };
	for(int i=0; i<6; ++i)
	{
		cVector3f vEnd = vPos + vDirs[i]*4.0f;
		fprintf(f, "los %d %d %d", i, gpBase->mpMapHelper->CheckLineOfSight(vPos, vEnd, false), gpBase->mpMapHelper->CheckLineOfSight(vPos, vEnd, true));
		fprintf(f, " light %.5f %.5f", gpBase->mpMapHelper->GetLightLevelAtPos(vEnd), gpBase->mpMapHelper->GetLightLevelAtPos(vEnd, NULL, 1.5f));

		float fDist = -1;
		iPhysicsBody *pBody = NULL;
		iLuxEntity *pEntity = NULL;
		bool bEnt = gpBase->mpMapHelper->GetClosestEntity(vPos, vDirs[i], 8.0f, &fDist, &pBody, &pEntity);
		fprintf(f, " ent %d %.5f %s %s", bEnt, fDist, pBody ? pBody->GetName().c_str() : "-", pEntity ? pEntity->GetName().c_str() : "-");

		cVector3f vNormal;
		fDist = -1;
		pBody = NULL;
		bool bChar = gpBase->mpMapHelper->GetClosestCharCollider(vPos, vDirs[i], 8.0f, &fDist, &vNormal, &pBody);
		fprintf(f, " chr %d %.5f %s", bChar, fDist, pBody ? pBody->GetName().c_str() : "-");
		Pos(f, "n", vNormal);
		fprintf(f, "\n");
	}

	iPhysicsWorld *pPhysics = apMap->GetPhysicsWorld();
	cMatrixf mtxOffset = cMatrixf::Identity;
	iCollideShape *pShape = pPhysics->CreateBoxShape(cVector3f(4,4,4), &mtxOffset);
	cMatrixf mtxShape = cMatrixf::Identity;
	mtxShape.SetTranslation(vPos);
	bool bHitPlayer = true;
	bool bHit = gpBase->mpMapHelper->ShapeDamage(pShape, mtxShape, vPos, 0, 0, 0, 0, 1, 1.0f, eLuxDamageType_BloodSplat, eLuxWeaponHitType_Sword, true, false, true, false, &bHitPlayer);
	fprintf(f, "damage %d %d\n", bHit, bHitPlayer);
	pPhysics->DestroyShape(pShape);
	fclose(f);
}
