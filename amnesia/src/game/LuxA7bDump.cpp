// OHPL_A7B_DUMP: per map update, every area's state block, overlapping bodies and the player; the player is moved through the areas by a fixed script.

#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxBase.h"
#include "LuxEntity.h"
#include "LuxArea.h"
#include "RustLuxMap.h"

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <string>
#include <vector>

struct OhplA7bDump
{
	static void Hex(FILE *f, const void *apData, size_t alBytes)
	{
		const unsigned char *p = (const unsigned char*)apData;
		for(size_t i=0; i<alBytes; ++i) fprintf(f, "%02x", p[i]);
	}

	static void Area(FILE *f, cLuxMap *apMap, iLuxArea *apArea, int alFrame)
	{
		iPhysicsBody *pBody = apArea->GetBody();
		fprintf(f, "%d %s t=%d act=%d pod=", alFrame, apArea->GetName().c_str(), (int)apArea->GetAreaType(), (int)apArea->IsActive());
		Hex(f, apArea->mpA7Pod, ohpl_a7_area_pod_len(apArea->mpA7Area));
		std::vector<iPhysicsBody*> vBodies;
		apMap->GetPhysicsWorld()->GetBodiesInBV(pBody->GetBoundingVolume(), &vBodies);
		std::vector<std::string> vNames;
		for(size_t i=0; i<vBodies.size(); ++i)
		{
			iPhysicsBody *b = vBodies[i];
			if(b == pBody || b->GetCollide() == false) continue;
			char sBuf[256];
			snprintf(sBuf, sizeof(sBuf), "%s:%d%d%d:%.3f:%d", b->GetName().c_str(), (int)b->IsActive(), (int)b->IsCharacter(),
					(int)b->GetBuoyancyActive(), b->GetMass(), (int)b->GetGravity());
			vNames.push_back(sBuf);
		}
		std::sort(vNames.begin(), vNames.end());
		fprintf(f, " ov=");
		for(size_t i=0; i<vNames.size(); ++i) fprintf(f, "%s,", vNames[i].c_str());
		fprintf(f, " water=%d", (int)gpBase->mpPlayer->IsInWater());
		if(apArea->GetAreaType() == eLuxAreaType_Sticky)
		{
			iPhysicsBody *pAtt = static_cast<cLuxAreaRs*>(apArea)->GetAttachedBody();
			fprintf(f, " att=%s", pAtt ? pAtt->GetName().c_str() : "-");
		}
		fprintf(f, "\n");
	}

	static void Rope(FILE *f, cLuxRope *apRope, int alFrame)
	{
		fprintf(f, "%d rope %s act=%d", alFrame, apRope->GetName().c_str(), (int)apRope->IsActive());
		if(apRope->mpRope)
		{
			cVector3f s = apRope->mpRope->GetStartParticle()->GetPosition(), e = apRope->mpRope->GetEndParticle()->GetPosition();
			fprintf(f, " len=%.3f s=%.3f,%.3f,%.3f e=%.3f,%.3f,%.3f", apRope->mpRope->GetTotalLength(), s.x, s.y, s.z, e.x, e.y, e.z);
		}
		fprintf(f, "\n");
	}
};

void OhplA7bTick(cLuxMap *apMap)
{
	static const char *sPath = getenv("OHPL_A7B_DUMP");
	if(sPath == NULL || *sPath == 0) return;
	if(apMap != gpBase->mpMapHandler->GetCurrentMap()) return;
	static int lFrame = 0;
	static std::vector<iLuxArea*> vTargets;
	++lFrame;

	FILE *f = fopen(sPath, "a");
	if(f == NULL) return;

	if(lFrame == 1)
	{
		fprintf(f, "== %s\n", apMap->GetName().c_str());
		const eLuxAreaType vOrder[] = { eLuxAreaType_Liquid, eLuxAreaType_Ladder, eLuxAreaType_Sticky, eLuxAreaType_SlimeDamage, eLuxAreaType_Insanity, eLuxAreaType_Flashback };
		for(size_t t=0; t<sizeof(vOrder)/sizeof(vOrder[0]); ++t)
		{
			cLuxEntityIterator it = apMap->GetEntityIterator();
			while(it.HasNext())
			{
				iLuxEntity *pEntity = it.Next();
				if(pEntity->GetEntityType() == eLuxEntityType_Area && static_cast<iLuxArea*>(pEntity)->GetAreaType() == vOrder[t])
				{
					vTargets.push_back(static_cast<iLuxArea*>(pEntity));
					break;
				}
			}
		}
	}
	const int lSlot = lFrame / 50 - 1;
	if(lFrame % 50 == 0 && lSlot >= 0 && lSlot < (int)vTargets.size())
	{
		iLuxArea *pTarget = vTargets[lSlot];
		gpBase->mpPlayer->GetCharacterBody()->SetPosition(pTarget->GetBody()->GetWorldPosition());
		fprintf(f, "%d move %s\n", lFrame, pTarget->GetName().c_str());
	}

	if(lFrame <= 400)
	{
		cLuxEntityIterator it = apMap->GetEntityIterator();
		while(it.HasNext())
		{
			iLuxEntity *pEntity = it.Next();
			if(pEntity->GetEntityType() == eLuxEntityType_Area) OhplA7bDump::Area(f, apMap, static_cast<iLuxArea*>(pEntity), lFrame);
			else if(pEntity->GetEntityType() == eLuxEntityType_Rope) OhplA7bDump::Rope(f, static_cast<cLuxRope*>(pEntity), lFrame);
		}
	}
	fclose(f);
}
