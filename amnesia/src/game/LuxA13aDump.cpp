// OHPL_A13A_DUMP: map helper answers (line of sight, light level, closest entity, shape damage) around the player at fixed updates.

#include "LuxMap.h"
#include "LuxEntity.h"
#include "LuxMapHelper.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxBase.h"

#include "LuxEntity.h"
#include "LuxEnemy.h"
#include "LuxAreaNodes.h"
#include "LuxArea.h"
#include "LuxProp.h"

#include <stdio.h>
#include <stdlib.h>
#include <fstream>

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

// OHPL_A7_MAPDUMP: every entity in list order with its save data, plus the lookups the map answers.
void cLuxMap::DumpA7(const char *asFile, const char *asLabel)
{
	std::ofstream os(asFile, std::ios::app);
	tString sTmp = tString(asFile) + ".tmp";
	os << "== map " << msName << " " << asLabel << "\n";
	Log("A7 map dump '%s' %s\n", msName.c_str(), asLabel);

	int lIdx = 0;
	cLuxEntityIterator it = GetEntityIterator();
	while(it.HasNext())
	{
		iLuxEntity *pEntity = it.Next();
		os << lIdx++ << "|" << (int)pEntity->GetEntityType() << "|" << pEntity->GetName() << "|" << pEntity->GetID()
			<< "|" << pEntity->IsActive() << "|" << pEntity->GetDestroyMe()
			<< "|" << (GetEntityByName(pEntity->GetName()) == pEntity) << "|" << (GetEntityByID(pEntity->GetID()) == pEntity);
		if(pEntity->GetEntityType() == eLuxEntityType_Area) os << "|area" << (int)static_cast<iLuxArea*>(pEntity)->GetAreaType();
		if(pEntity->GetEntityType() == eLuxEntityType_Prop) os << "|prop" << (int)static_cast<iLuxProp*>(pEntity)->GetPropType();
		os << "|";
		iLuxEntity_SaveData *pData = pEntity->GetEntityType() == eLuxEntityType_Area ? pEntity->CreateSaveData() : NULL;
		if(pData)
		{
			pEntity->SaveToSaveData(pData);
			cSerializeClass::SaveToFile(pData, cString::To16Char(sTmp), "D");
			std::ifstream in(sTmp.c_str());
			std::string sLine;
			while(std::getline(in, sLine))
			{
				size_t lPos = sLine.find_first_not_of(" \t\r");
				if(lPos != std::string::npos) os << sLine.substr(lPos);
			}
			hplDelete(pData);
		}
		os << "\n";
	}

	os << "enemies:";
	cLuxEnemyIterator enemyIt = GetEnemyIterator();
	while(enemyIt.HasNext()) os << " " << enemyIt.Next()->GetName();
	os << "\nfreeid " << ohpl_a7_ent_free_id(mpA7) << " latest " << (GetLatestEntity() ? GetLatestEntity()->GetName() : tString("-")) << "\n";

	std::vector<cLuxEventTimer> vTimers;
	CopyTimers(vTimers);
	for(size_t i=0; i<vTimers.size(); ++i) os << "timer " << vTimers[i].msName << " " << vTimers[i].msFunction << " " << vTimers[i].mfCount << " " << vTimers[i].mbDestroyMe << "\n";
	std::vector<cLuxUseItemCallback> vCallbacks;
	CopyUseItemCallbacks(vCallbacks);
	for(size_t i=0; i<vCallbacks.size(); ++i) os << "useitem " << vCallbacks[i].msName << " " << vCallbacks[i].msItem << " " << vCallbacks[i].msEntity << " " << vCallbacks[i].msFunction << "\n";
	for(int i=0; i<GetPlayerStartNodeNum(); ++i) os << "start " << GetPlayerStartNode(i)->GetName() << " " << GetPlayerStartNode(i)->GetPosition().x << "\n";
	cLuxNode_PlayerStart *pFirst = GetFirstPlayerStart();
	os << "firststart " << (pFirst ? pFirst->GetName() : tString("-")) << "\n";
	for(tLuxScriptVarMapIt vit = m_mapVars.begin(); vit != m_mapVars.end(); ++vit) os << "var " << vit->first << "=" << vit->second.msVal << "\n";
	os << "doomed 0 deleting " << IsDeletingAllWorldEntities() << "\n";
}
