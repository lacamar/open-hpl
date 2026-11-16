// Host of the Rust map family (rust/src/luxmap/mhost.rs): one op entry reaching the engine objects.

#include "LuxMapHost.h"
#include "LuxMapHelper.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxEntity.h"
#include "LuxEnemy.h"
#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxPlayerState.h"
#include "LuxAreaNodes.h"
#include "LuxArea_Sticky.h"
#include "LuxProp_Item.h"
#include "LuxProp_SwingDoor.h"
#include "LuxProp_Lamp.h"
#include "LuxConfigHandler.h"
#include "LuxDebugHandler.h"
#include "LuxCompletionCountHandler.h"
#include "LuxMusicHandler.h"
#include "LuxProgressLogHandler.h"
#include "LuxEffectHandler.h"
#include "LuxEffectRenderer.h"
#include "LuxHelpFuncs.h"
#include "LuxSaveHandler.h"
#include "LuxSavedGame.h"
#include "LuxLoadScreenHandler.h"
#include "LuxMainMenu.h"
#include "LuxAchievementHandler.h"

void OhplA4bTick(cLuxMap *apMap);
void OhplA13aTick(cLuxMap *apMap);

namespace
{

enum
{
	kOp_CUR_MAP = 1, kOp_RAY_CAST, kOp_BODY_INFO, kOp_BODY_SUBMESHES, kOp_ENT_INFO, kOp_ENT_CAN_INTERACT, kOp_BODY_POS,
	kOp_SHAPE_BODIES, kOp_SHAPE_COLLIDE, kOp_BODY_IMPULSE, kOp_BODY_PUSH, kOp_ENEMY_HIT, kOp_BODY_HIT, kOp_FX_AT,
	kOp_ENT_DAMAGE, kOp_PLAYER_DAMAGE, kOp_LIGHT_ROOT, kOp_NODE_INFO, kOp_LIGHT_TEST, kOp_LIGHT_PROPS, kOp_PLAYER_AMB_LIGHT,
	kOp_SET_LOADING, kOp_LOAD_CFG, kOp_FILE_EXISTS, kOp_SCRIPT_CREATE, kOp_SCRIPT_DESTROY, kOp_SCRIPT_RUN, kOp_WORLD_LOAD, kOp_WORLD_DESTROY,
	kOp_LOG, kOp_HARD_MODE, kOp_ENT_KIND, kOp_ENT_CALL, kOp_ENT_DELETE, kOp_ENEMY, kOp_DOOR, kOp_STICKY, kOp_PROP_BODY, kOp_COLLIDER,
	kOp_BODY_PAIR, kOp_PLAYER, kOp_FADE_IN, kOp_MUSIC, kOp_COMPLETION_SHOW, kOp_QUEST_VALUE, kOp_DISSOLVE_NEW, kOp_DISSOLVE_ALPHA,
	kOp_LAMP_NEW, kOp_LAMP_ADD, kOp_LAMP_UPDATE, kOp_WORLD_CREATE_ENTITY, kOp_LOADING_MAP, kOp_IS_CURRENT_MAP, kOp_UPDATE_HOOKS,
	kOp_SHOW_ENTITY_INFO, kOp_SHOW_COMMENTARY,
	kOp_MAP_NEW = 57, kOp_MAP_NAME, kOp_MAP_CTX, kOp_SET_CURRENT, kOp_MODULE_MSG, kOp_VIEW, kOp_PLAY_GUI_SOUND, kOp_FADE, kOp_PLAYER_ACTIVE,
	kOp_SAVE_PENDING, kOp_CLEANUP_DATA, kOp_LOCK, kOp_LOADSCREEN, kOp_SAVED, kOp_WORLD_SOUNDS_FADE, kOp_ACHIEVEMENT, kOp_PLAYER_SANITY,
	kOp_TIME, kOp_SOUND_ALL, kOp_WORLD_ACTIVE, kOp_VIEWPORT_ACTIVE, kOp_STOP_COMMENTARY, kOp_FOLDER, kOp_SAVED_RESET, kOp_DATACACHE_DESTROY,
	kOp_QUIT, kOp_CLEAR_RENDER_LISTS, kOp_MAP_LOAD
};

enum { kEnt_AFTER_WORLD_LOAD, kEnt_ON_MAP_ENTER, kEnt_UPDATE_LOGIC, kEnt_RENDER_SOLID, kEnt_BEFORE_DESTRUCTION, kEnt_DESTROY_ME, kEnt_SET_ACTIVE,
	kEnt_PLAYER_ON_DESTROY, kEnt_IS_ACTIVE, kEnt_GET_DESTROY_ME, kEnt_IS_COMMENTARY, kEnt_HARD_ITEM };

class cLuxDissolveEntity
{
public:
	cLuxDissolveEntity(cLuxMap *apMap, cMeshEntity *apEntity) : mpMap(apMap), mpEntity(apEntity){}
	~cLuxDissolveEntity(){ mpMap->GetWorld()->DestroyMeshEntity(mpEntity);}

	cLuxMap *mpMap;
	cMeshEntity *mpEntity;
};



class cRayShim : public iPhysicsRayCallback
{
public:
	cRayShim(void *apCtx) : mpCtx(apCtx){}
	bool BeforeIntersect(iPhysicsBody *apBody){ return ohpl_a13_ray_before(mpCtx, apBody);}
	bool OnIntersect(iPhysicsBody *apBody, cPhysicsRayParams *apParams){ return ohpl_a13_ray_hit(mpCtx, apBody, apParams->mfDist, &apParams->mvNormal.x);}
private:
	void *mpCtx;
};

OhplA13S Str(const tString &asStr)
{
	OhplA13S s = { reinterpret_cast<const unsigned char*>(asStr.data()), asStr.size() };
	return s;
}

cMatrixf Mtx(const float *apM)
{
	cMatrixf m;
	memcpy(m.v, apM, sizeof(float)*16);
	return m;
}

cVector3f Vec(const float *apV){ return cVector3f(apV[0], apV[1], apV[2]);}

void Host(int32_t alOp, OhplA13A *a)
{
	static std::vector<OhplGa5SubMesh> vSubMeshes;
	static std::vector<iPhysicsBody*> vBodies;
	static cCollideData collideData;
	static std::vector<float> vPoints;
	static std::vector<uintptr_t> vNodeItems;
	static tString sSound, sPS;

	switch(alOp)
	{
	case kOp_CUR_MAP:
	{
		cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
		a->r = (uintptr_t)pMap;
		if(pMap)
		{
			a->p[0] = (uintptr_t)pMap->GetPhysicsWorld();
			a->p[1] = (uintptr_t)pMap->GetWorld();
		}
		break;
	}
	case kOp_RAY_CAST:
	{
		cRayShim shim((void*)a->p[1]);
		((iPhysicsWorld*)a->p[0])->CastRay(&shim, Vec(&a->f[0]), Vec(&a->f[3]), a->i[1]&1, a->i[1]&2, a->i[1]&4, a->i[1]&8);
		break;
	}
	case kOp_BODY_INFO:
	{
		iPhysicsBody *pBody = (iPhysicsBody*)a->p[0];
		bool bChar = pBody->IsCharacter();
		a->i[0] = (bChar ? 1 : 0) | (pBody->GetCollide() ? 2 : 0) | (pBody->GetBlocksLight() ? 4 : 0) | (pBody->GetCollideCharacter() ? 8 : 0) |
					(pBody->IsActive() ? 16 : 0) | (pBody == gpBase->mpPlayer->GetCharacterBody()->GetCurrentBody() ? 32 : 0);
		a->f[0] = pBody->GetMass();
		a->r = (uintptr_t)pBody->GetUserData();
		a->p[1] = bChar ? (uintptr_t)pBody->GetCharacterBody()->GetUserData() : 0;
		break;
	}
	case kOp_BODY_SUBMESHES:
	{
		iPhysicsBody *pBody = (iPhysicsBody*)a->p[0];
		iLuxEntity *pEntity = (iLuxEntity*)pBody->GetUserData();
		vSubMeshes.clear();
		a->i[1] = 0;
		if(pEntity && pEntity->GetMeshEntity())
		{
			cMeshEntity *pMeshEntity = pEntity->GetMeshEntity();
			a->i[1] = 1;
			for(int i=0; i< pMeshEntity->GetSubMeshEntityNum(); ++i)
			{
				cSubMeshEntity *pSubMeshEnt = pMeshEntity->GetSubMeshEntity(i);
				cMaterial *pMaterial = pSubMeshEnt->GetMaterial();

				OhplGa5SubMesh subMesh;
				subMesh.parentIsBody = pSubMeshEnt->GetEntityParent() == (iEntity3D*)pBody;
				subMesh.hasMaterial = pMaterial != NULL;
				subMesh.translucent = pMaterial ? pMaterial->GetType()->IsTranslucent() : false;
				subMesh.shadowCaster = pSubMeshEnt->GetRenderFlagBit(eRenderableFlag_ShadowCaster);
				vSubMeshes.push_back(subMesh);
			}
		}
		a->i[0] = (int32_t)vSubMeshes.size();
		a->r = (uintptr_t)vSubMeshes.data();
		break;
	}
	case kOp_ENT_INFO:
	{
		iLuxEntity *pEntity = (iLuxEntity*)a->p[0];
		a->i[0] = pEntity->IsActive();
		a->i[1] = pEntity->GetEntityType() == eLuxEntityType_Enemy;
		break;
	}
	case kOp_ENT_CAN_INTERACT:
		a->i[0] = ((iLuxEntity*)a->p[0])->CanInteract((iPhysicsBody*)a->p[1]);
		break;
	case kOp_BODY_POS:
	{
		cVector3f v = ((iPhysicsBody*)a->p[0])->GetWorldPosition();
		a->f[0] = v.x; a->f[1] = v.y; a->f[2] = v.z;
		break;
	}
	case kOp_SHAPE_BODIES:
	{
		iCollideShape *pShape = (iCollideShape*)a->p[1];
		cBoundingVolume shapeBV = pShape->GetBoundingVolume();
		shapeBV.SetTransform(cMath::MatrixMul(Mtx(a->m), shapeBV.GetTransform()));
		vBodies.clear();
		((iPhysicsWorld*)a->p[0])->GetBodiesInBV(&shapeBV, &vBodies);
		a->i[0] = (int32_t)vBodies.size();
		a->r = (uintptr_t)vBodies.data();
		break;
	}
	case kOp_SHAPE_COLLIDE:
	{
		iPhysicsWorld *pWorld = (iPhysicsWorld*)a->p[0];
		iCollideShape *pShape = (iCollideShape*)a->p[1];
		iPhysicsBody *pBody = (iPhysicsBody*)a->p[2];
		cMatrixf mtx = Mtx(a->m);
		cBoundingVolume shapeBV = pShape->GetBoundingVolume();
		shapeBV.SetTransform(cMath::MatrixMul(mtx, shapeBV.GetTransform()));
		a->i[0] = cMath::CheckBVIntersection(shapeBV, *pBody->GetBoundingVolume());
		a->i[1] = 0;
		a->i[2] = 0;
		if(a->i[0] == 0) break;
		collideData.SetMaxSize(4);
		a->i[1] = pWorld->CheckShapeCollision(pShape, mtx, pBody->GetShape(), pBody->GetLocalMatrix(), collideData, 4, false);
		if(a->i[1] == 0) break;
		vPoints.clear();
		for(int i=0; i < collideData.mlNumOfPoints; ++i)
		{
			const cVector3f &v = collideData.mvContactPoints[i].mvPoint;
			vPoints.push_back(v.x); vPoints.push_back(v.y); vPoints.push_back(v.z);
		}
		a->i[2] = collideData.mlNumOfPoints;
		a->r = (uintptr_t)vPoints.data();
		break;
	}
	case kOp_BODY_IMPULSE:
		((iPhysicsBody*)a->p[0])->AddImpulseAtPosition(Vec(&a->f[0]), Vec(&a->f[3]));
		break;
	case kOp_BODY_PUSH:
		((iPhysicsBody*)a->p[0])->GetCharacterBody()->AddForceVelocity(Vec(&a->f[0]));
		break;
	case kOp_ENEMY_HIT:
	{
		iLuxEnemy *pEnemy = static_cast<iLuxEnemy*>((iLuxEntity*)a->p[0]);
		sSound = pEnemy->GetHitSound((eLuxWeaponHitType)a->i[0]);
		sPS = pEnemy->GetHitPS((eLuxWeaponHitType)a->i[0]);
		a->rs[0] = Str(sSound);
		a->rs[1] = Str(sPS);
		break;
	}
	case kOp_BODY_HIT:
	{
		iPhysicsBody *pBody = (iPhysicsBody*)a->p[0];
		cSurfaceData *pSurface = pBody->GetMaterial()->GetSurfaceData();
		cSurfaceImpactData *pImpact = pSurface ? pSurface->GetHitDataFromSpeed(a->f[0]) : NULL;
		a->i[0] = pImpact != NULL;
		if(pImpact)
		{
			sSound = pImpact->GetSoundName();
			sPS = pImpact->GetPSName();
			a->rs[0] = Str(sSound);
			a->rs[1] = Str(sPS);
		}
		break;
	}
	case kOp_FX_AT:
	{
		cWorld *pWorld = (cWorld*)a->p[0];
		tString sName((const char*)a->s[0].p, a->s[0].n);
		if(a->i[0] == 0)
		{
			cSoundEntity *pSound = pWorld->CreateSoundEntity("HitSound", sName, true);
			if(pSound) pSound->SetPosition(Vec(&a->f[0]));
		}
		else
		{
			cParticleSystem *pPS = pWorld->CreateParticleSystem("HitPS", sName, 1);
			if(pPS) pPS->SetPosition(Vec(&a->f[0]));
		}
		break;
	}
	case kOp_ENT_DAMAGE:
		((iLuxEntity*)a->p[0])->GiveDamage(a->f[0], a->i[0]);
		break;
	case kOp_PLAYER_DAMAGE:
		gpBase->mpPlayer->GiveDamage(a->f[0], a->i[0], (eLuxDamageType)a->i[1], true, a->i[2] != 0);
		break;
	case kOp_LIGHT_ROOT:
		a->r = (uintptr_t)((cWorld*)a->p[0])->GetRenderableContainer(a->i[0] == 0 ? eWorldContainerType_Static : eWorldContainerType_Dynamic)->GetRoot();
		break;
	case kOp_NODE_INFO:
	{
		iRenderableContainerNode *pNode = (iRenderableContainerNode*)a->p[0];
		pNode->UpdateBeforeUse();
		vNodeItems.clear();
		a->i[0] = a->i[1] = 0;
		if(pNode->HasChildNodes())
		{
			for(tRenderableContainerNodeListIt it = pNode->GetChildNodeList()->begin(); it != pNode->GetChildNodeList()->end(); ++it)
			{
				vNodeItems.push_back((uintptr_t)*it);
				++a->i[0];
			}
		}
		if(pNode->HasObjects())
		{
			for(tRenderableListIt it = pNode->GetObjectList()->begin(); it != pNode->GetObjectList()->end(); ++it)
			{
				vNodeItems.push_back((uintptr_t)*it);
				++a->i[1];
			}
		}
		const cVector3f &vMin = pNode->GetMin(), &vMax = pNode->GetMax();
		a->f[0] = vMin.x; a->f[1] = vMin.y; a->f[2] = vMin.z;
		a->f[3] = vMax.x; a->f[4] = vMax.y; a->f[5] = vMax.z;
		a->r = (uintptr_t)vNodeItems.data();
		break;
	}
	case kOp_LIGHT_TEST:
	{
		iRenderable *pObject = (iRenderable*)a->p[0];
		a->i[0] = a->i[1] = 0;
		if(pObject->IsVisible()==false || pObject->GetRenderType() != eRenderableType_Light) break;
		a->i[0] = 1;
		cVector3f vPos = Vec(&a->f[0]);
		iLight *pLight = static_cast<iLight*>(pObject);
		switch(pLight->GetLightType())
		{
		case eLightType_Box:
			a->i[1] = cMath::CheckPointInBVIntersection(vPos, *pLight->GetBoundingVolume());
			break;
		case eLightType_Point:
			a->i[1] = cMath::CheckPointInSphereIntersection(vPos, pLight->GetWorldPosition(), pLight->GetRadius());
			break;
		case eLightType_Spot:
			a->i[1] = static_cast<cLightSpot*>(pLight)->GetFrustum()->CollidePoint(vPos);
			break;
		}
		break;
	}
	case kOp_LIGHT_PROPS:
	{
		iLight *pLight = (iLight*)a->p[0];
		cColor c = pLight->GetDiffuseColor();
		a->f[0] = c.r; a->f[1] = c.g; a->f[2] = c.b;
		cVector3f v = pLight->GetWorldPosition();
		a->f[3] = v.x; a->f[4] = v.y; a->f[5] = v.z;
		a->f[6] = pLight->GetRadius();
		a->i[0] = pLight->GetLightType() == eLightType_Box;
		a->i[1] = pLight->GetLightType() == eLightType_Spot;
		a->i[2] = pLight->GetCastShadows();
		break;
	}
	case kOp_PLAYER_AMB_LIGHT:
		a->r = (uintptr_t)gpBase->mpPlayer->GetHelperInDarkness()->GetAmbientLight();
		break;
	case kOp_SET_LOADING:
		gpBase->mpCurrentMapLoading = (cLuxMap*)a->p[0];
		break;
	case kOp_LOAD_CFG:
		a->i[0] = gpBase->mpConfigHandler->mbFastPhysicsLoad;
		a->i[1] = gpBase->mpConfigHandler->mbFastStaticLoad;
		a->i[2] = gpBase->mpConfigHandler->mbFastEntityLoad;
		a->i[3] = cResources::GetCreateAndLoadCompressedMaps();
		a->p[0] = eWorldLoadFlag_NoDynamicGameEntities;
		a->p[1] = eWorldLoadFlag_FastPhysicsLoad;
		a->p[2] = eWorldLoadFlag_FastStaticLoad;
		a->p[3] = eWorldLoadFlag_FastEntityLoad;
		break;
	case kOp_FILE_EXISTS:
		a->i[0] = gpBase->mpEngine->GetResources()->GetFileSearcher()->GetFilePath(tString((const char*)a->s[0].p, a->s[0].n))!=_W("");
		break;
	case kOp_SCRIPT_CREATE:
	{
		sSound.clear();
		a->r = (uintptr_t)gpBase->mpEngine->GetResources()->GetScriptManager()->CreateScript(tString((const char*)a->s[0].p, a->s[0].n), &sSound);
		a->rs[0] = Str(sSound);
		break;
	}
	case kOp_SCRIPT_DESTROY:
		gpBase->mpEngine->GetResources()->GetScriptManager()->Destroy((iScript*)a->p[0]);
		break;
	case kOp_SCRIPT_RUN:
		((iScript*)a->p[0])->Run(tString((const char*)a->s[0].p, a->s[0].n));
		break;
	case kOp_WORLD_LOAD:
	{
		cWorld *pWorld = gpBase->mpEngine->GetScene()->LoadWorld(tString((const char*)a->s[0].p, a->s[0].n), (tWorldLoadFlag)a->i[0]);
		a->r = (uintptr_t)pWorld;
		a->p[1] = pWorld ? (uintptr_t)pWorld->GetPhysicsWorld() : 0;
		break;
	}
	case kOp_WORLD_DESTROY:
		if(a->p[0]) gpBase->mpEngine->GetScene()->DestroyWorld((cWorld*)a->p[0]);
		break;
	case kOp_LOG:
	{
		tString sMsg((const char*)a->s[0].p, a->s[0].n);
		switch(a->i[0])
		{
		case 0: Error("%s", sMsg.c_str()); break;
		case 1: Warning("%s", sMsg.c_str()); break;
		case 2: FatalError("%s", sMsg.c_str()); break;
		case 3: gpBase->mpDebugHandler->AddMessage(cString::To16Char(sMsg), false); break;
		default: gpBase->mpProgressLogHandler->AddLog(eLuxProgressLogLevel_High, sMsg); break;
		}
		break;
	}
	case kOp_HARD_MODE:
		a->i[0] = gpBase->mbHardMode;
		a->i[1] = gpBase->mpPlayer->GetTinderboxes();
		break;
	case kOp_ENT_KIND:
	{
		iLuxEntity *pEntity = (iLuxEntity*)a->p[0];
		bool bEnemy = pEntity->GetEntityType() == eLuxEntityType_Enemy;
		bool bArea = pEntity->GetEntityType() == eLuxEntityType_Area;
		bool bSticky = bArea && static_cast<iLuxArea*>(pEntity)->GetAreaType() == eLuxAreaType_Sticky;
		a->i[0] = (int32_t)pEntity->GetEntityType();
		a->i[1] = bEnemy;
		a->i[2] = bArea;
		a->i[3] = bSticky;
		a->r = 0;
		if(bEnemy) a->r = (uintptr_t)static_cast<iLuxEnemy*>(pEntity);
		else if(bSticky) a->r = (uintptr_t)static_cast<cLuxArea_Sticky*>(static_cast<iLuxArea*>(pEntity));
		a->p[2] = (uintptr_t)(uint32_t)pEntity->GetID();
		a->rs[0] = Str(pEntity->GetName());
		break;
	}
	case kOp_ENT_CALL:
	{
		iLuxEntity *pEntity = (iLuxEntity*)a->p[0];
		switch(a->i[0])
		{
		case kEnt_AFTER_WORLD_LOAD: pEntity->AfterWorldLoad(); break;
		case kEnt_ON_MAP_ENTER: pEntity->OnMapEnter(); break;
		case kEnt_UPDATE_LOGIC: pEntity->UpdateLogic(a->f[0]); break;
		case kEnt_RENDER_SOLID: pEntity->OnRenderSolid((cRendererCallbackFunctions*)a->p[1]); break;
		case kEnt_BEFORE_DESTRUCTION: cLuxMap::HostEntityCall(pEntity, 0); break;
		case kEnt_DESTROY_ME: cLuxMap::HostEntityCall(pEntity, 1); break;
		case kEnt_SET_ACTIVE: pEntity->SetActive(a->i[1] != 0); break;
		case kEnt_PLAYER_ON_DESTROY: gpBase->mpPlayer->GetCurrentStateData()->OnDestroyEntity(pEntity); break;
		case kEnt_IS_ACTIVE: a->i[0] = pEntity->IsActive(); break;
		case kEnt_GET_DESTROY_ME: a->i[0] = pEntity->GetDestroyMe(); break;
		case kEnt_IS_COMMENTARY: a->i[0] = pEntity->GetEntityType() == eLuxEntityType_CommentaryIcon; break;
		case kEnt_HARD_ITEM:
		{
			a->i[0] = 0;
			if(pEntity->GetEntityType() != eLuxEntityType_Prop) break;
			iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
			if(pProp->GetPropType() != eLuxPropType_Item) break;
			eLuxItemType itemType = static_cast<cLuxProp_Item*>(pProp)->GetItemType();
			if(itemType == eLuxItemType_Tinderbox) a->i[0] = 1;
			else if(itemType == eLuxItemType_LampOil) a->i[0] = 2;
			break;
		}
		}
		break;
	}
	case kOp_ENT_DELETE:
		hplDelete((iLuxEntity*)a->p[0]);
		break;
	case kOp_ENEMY:
	{
		iLuxEnemy *pEnemy = static_cast<iLuxEnemy*>((iLuxEntity*)a->p[0]);
		switch(a->i[0])
		{
		case 0:
		{
			cBoundingVolume *pBv = pEnemy->GetCharacterBody()->GetCurrentBody()->GetBoundingVolume();
			cVector3f vCentre = pBv->GetWorldCenter();
			a->i[0] = pEnemy->IsActive();
			a->i[1] = pEnemy->GetDeaf();
			a->f[0] = vCentre.x; a->f[1] = vCentre.y; a->f[2] = vCentre.z;
			a->f[3] = pBv->GetRadius();
			break;
		}
		case 1: pEnemy->SendMessage((eLuxEnemyMessage)a->i[1], a->f[0], a->i[2] != 0, Vec(&a->f[1]), a->f[4], a->i[3]); break;
		case 2: pEnemy->SendMessage(eLuxEnemyMessage_SoundHeard, 0, false, Vec(&a->f[0]), a->f[3], 0); break;
		case 3: a->r = (uintptr_t)pEnemy->GetPathFront(); break;
		case 4: pEnemy->SetActive(false); pEnemy->ResetProperties(); break;
		}
		break;
	}
	case kOp_DOOR:
	{
		iLuxEntity *pEntity = (iLuxEntity*)a->p[0];
		a->i[0] = a->i[1] = a->i[2] = 0;
		if(pEntity->GetEntityType() != eLuxEntityType_Prop || static_cast<iLuxProp*>(pEntity)->GetPropType() != eLuxPropType_SwingDoor) break;
		cLuxProp_SwingDoor *pDoor = static_cast<cLuxProp_SwingDoor*>(static_cast<iLuxProp*>(pEntity));
		a->i[0] = 1;
		a->i[1] = pDoor->IsBroken();
		a->i[2] = pDoor->GetDoorState();
		break;
	}
	case kOp_STICKY:
	{
		cLuxArea_Sticky *pSticky = (cLuxArea_Sticky*)a->p[0];
		if(a->i[0] == 0)
		{
			a->r = (uintptr_t)pSticky->GetAttachedBody();
			a->i[0] = pSticky->CanDetach();
		}
		else pSticky->DetachBody();
		break;
	}
	case kOp_PROP_BODY:
	{
		iLuxEntity *pEntity = (iLuxEntity*)a->p[0];
		a->r = pEntity->GetEntityType() == eLuxEntityType_Prop ? (uintptr_t)static_cast<iLuxProp*>(pEntity)->GetBodyFromID(a->i[0]) : 0;
		break;
	}
	case kOp_COLLIDER:
	{
		iLuxCollideCallbackContainer *pCont = (iLuxCollideCallbackContainer*)a->p[0];
		if(a->i[0] == 0) a->i[0] = pCont->GetBodyNum();
		else a->r = (uintptr_t)pCont->GetBody(a->i[1]);
		break;
	}
	case kOp_BODY_PAIR:
	{
		cCollideData data;
		data.SetMaxSize(1);
		iPhysicsBody *pBody1 = (iPhysicsBody*)a->p[1], *pBody2 = (iPhysicsBody*)a->p[2];
		a->i[0] = cMath::CheckBVIntersection(*pBody1->GetBoundingVolume(), *pBody2->GetBoundingVolume()) &&
					((iPhysicsWorld*)a->p[0])->CheckShapeCollision(pBody1->GetShape(), pBody1->GetLocalMatrix(), pBody2->GetShape(), pBody2->GetLocalMatrix(), data, 1, false);
		break;
	}
	case kOp_PLAYER:
		if(a->i[0] == 0) gpBase->mpPlayer->PlaceAtStartNode((cLuxNode_PlayerStart*)a->p[0]);
		else gpBase->mpPlayer->ClearTerrorEnemies();
		break;
	case kOp_FADE_IN:
		gpBase->mpEffectHandler->GetFade()->FadeIn(a->f[0]);
		break;
	case kOp_MUSIC:
	{
		cLuxMusicHandler *pMusic = gpBase->mpMusicHandler;
		switch(a->i[0])
		{
		case 0: a->i[0] = pMusic->GetMaxPrio(); break;
		case 1:
		{
			cLuxMusic music = pMusic->GetMusic(a->i[1]);
			sSound = music.msFile;
			a->i[0] = music.mbLoop; a->i[1] = music.mbSpecialEffect; a->i[2] = music.mbResume;
			a->f[0] = music.mfVolume;
			a->rs[0] = Str(sSound);
			break;
		}
		case 2: a->f[0] = pMusic->GetVolumeMul(); break;
		case 3: pMusic->Reset(); break;
		default: pMusic->Play(tString((const char*)a->s[0].p, a->s[0].n), true, a->f[0], 1, a->i[1], a->i[2] != 0, false); break;
		}
		break;
	}
	case kOp_COMPLETION_SHOW:
		gpBase->mpCompletionCountHandler->ShowCompletionIncrease(a->f[0], a->f[1], a->f[2]);
		break;
	case kOp_QUEST_VALUE:
		a->i[0] = gpBase->mpCompletionCountHandler->mlQuestCompletionValue;
		break;
	case kOp_DISSOLVE_NEW:
	{
		cLuxMap *pMap = (cLuxMap*)a->p[0];
		cMeshEntity *pOld = (cMeshEntity*)a->p[1];
		cWorld *pWorld = (cWorld*)a->p[2];
		cMesh *pMesh = pOld->GetMesh();
		pMesh->IncUserCount();
		cMeshEntity *pNew = pWorld->CreateMeshEntity(pOld->GetName()+"_Dissolve", pMesh);
		pNew->SetIlluminationAmount(pOld->GetIlluminationAmount());
		pNew->Stop();
		a->r = (uintptr_t)hplNew(cLuxDissolveEntity, (pMap, pNew));
		if(pMesh->GetSkeleton())
		{
			pNew->SetMatrix(pOld->GetWorldMatrix());
			for(int i=0; i<pNew->GetBoneStateNum(); ++i)
			{
				cBoneState *pOldBone = pOld->GetBoneState(i);
				cBoneState *pNewBone = pNew->GetBoneState(i);
				pNewBone->SetActive(false);
				pNewBone->SetMatrix(pOldBone->GetLocalMatrix());
			}
		}
		else
		{
			for(int i=0; i<pNew->GetSubMeshEntityNum(); ++i)
				pNew->GetSubMeshEntity(i)->SetMatrix(pOld->GetSubMeshEntity(i)->GetWorldMatrix());
		}
		break;
	}
	case kOp_DISSOLVE_ALPHA:
		((cLuxDissolveEntity*)a->p[0])->mpEntity->SetCoverageAmount(a->f[0]);
		break;
	case kOp_LAMP_NEW:
		a->r = (uintptr_t)hplNew(cLuxLampLightConnection, ((iLight*)a->p[0]));
		break;
	case kOp_LAMP_ADD:
		((cLuxLampLightConnection*)a->p[0])->AddLamp((cLuxProp_Lamp*)a->p[1], a->f[0], a->i[0] != 0, a->i[1] != 0);
		break;
	case kOp_LAMP_UPDATE:
		((cLuxLampLightConnection*)a->p[0])->Update(a->f[0]);
		break;
	case kOp_WORLD_CREATE_ENTITY:
		((cWorld*)a->p[0])->CreateEntity(tString((const char*)a->s[0].p, a->s[0].n), Mtx(a->m), tString((const char*)a->s[1].p, a->s[1].n), a->i[0], true, Vec(&a->f[0]));
		break;
	case kOp_LOADING_MAP:
		a->r = (uintptr_t)gpBase->mpCurrentMapLoading;
		break;
	case kOp_IS_CURRENT_MAP:
		a->i[0] = (cLuxMap*)a->p[0] == gpBase->mpMapHandler->GetCurrentMap();
		break;
	case kOp_UPDATE_HOOKS:
	{
		cLuxMap *pMap = (cLuxMap*)a->p[0];
		OhplA4bTick(pMap);
		OhplA13aTick(pMap);
		static const char *sDumpFile = getenv("OHPL_A7_MAPDUMP");
		if(sDumpFile)
		{
			static const char *sDumpFrames = getenv("OHPL_A7_MAPDUMP_FRAMES");
			if(a->i[0] == 0) pMap->DumpA7(sDumpFile, "load");
			if(a->i[0] + 1 == (sDumpFrames ? atoi(sDumpFrames) : 120)) pMap->DumpA7(sDumpFile, "frames");
		}
		break;
	}
	case kOp_SHOW_ENTITY_INFO:
		a->i[0] = gpBase->mpDebugHandler->GetShowEntityInfo();
		break;
	case kOp_SHOW_COMMENTARY:
		a->i[0] = gpBase->mpMapHandler->GetShowCommentary();
		break;
	case kOp_MAP_NEW:
		a->r = (uintptr_t)hplNew(cLuxMap, (tString((const char*)a->s[0].p, a->s[0].n)));
		break;
	case kOp_MAP_LOAD:
		((cLuxMap*)a->p[0])->LoadFromFile(tString((const char*)a->s[0].p, a->s[0].n), a->i[0] != 0);
		break;
	case kOp_MAP_NAME:
		a->rs[0] = Str(((cLuxMap*)a->p[0])->GetName());
		break;
	case kOp_MAP_CTX:
		a->r = (uintptr_t)((cLuxMap*)a->p[0])->GetA7();
		break;
	case kOp_SET_CURRENT:
		((cLuxMapHandler*)a->p[5])->HostSetCurrent((cLuxMap*)a->p[0]);
		break;
	case kOp_MODULE_MSG:
	{
		static const eLuxUpdateableMessage vMsgs[4] = { eLuxUpdateableMessage_DestroyWorldEntities, eLuxUpdateableMessage_OnMapLeave,
														eLuxUpdateableMessage_CreateWorldEntities, eLuxUpdateableMessage_OnMapEnter };
		gpBase->RunModuleMessage(vMsgs[a->i[0] & 3], (cLuxMap*)a->p[0]);
		break;
	}
	case kOp_VIEW:
		if(a->i[0] == 0) ((cLuxMapHandler*)a->p[5])->HostSetView((cWorld*)a->p[0], (iPhysicsWorld*)a->p[1]);
		else ((cLuxMapHandler*)a->p[5])->HostSetView(NULL, NULL);
		break;
	case kOp_PLAY_GUI_SOUND:
		gpBase->mpHelpFuncs->PlayGuiSoundData(tString((const char*)a->s[0].p, a->s[0].n), eSoundEntryType_Gui);
		break;
	case kOp_FADE:
		if(a->i[0] == 0) gpBase->mpEffectHandler->GetFade()->FadeOut(a->f[0]);
		else if(a->i[0] == 1) gpBase->mpEffectHandler->GetFade()->FadeIn(a->f[0]);
		else a->i[0] = gpBase->mpEffectHandler->GetFade()->IsFading();
		break;
	case kOp_PLAYER_ACTIVE:
		gpBase->mpPlayer->SetActive(a->i[0] != 0);
		break;
	case kOp_SAVE_PENDING:
	{
		cLuxSaveHandlerThreadClass* pThreadClass = gpBase->mpSaveHandler->GetThreadClass();
		if(pThreadClass->IsRunning()) pThreadClass->ProcessPendingSaves();
		break;
	}
	case kOp_CLEANUP_DATA:
		gpBase->mpHelpFuncs->CleanupData();
		break;
	case kOp_LOCK:
		if(a->i[0]) ((cLuxMapHandler*)a->p[5])->mpSavedGameMutex->Lock();
		else ((cLuxMapHandler*)a->p[5])->mpSavedGameMutex->Unlock();
		break;
	case kOp_LOADSCREEN:
		if(a->i[0] == 0) gpBase->mpLoadScreenHandler->DrawGameScreen();
		else gpBase->mpLoadScreenHandler->GameScreenLoadDone(tString((const char*)a->s[0].p, a->s[0].n), a->f[0]);
		break;
	case kOp_SAVED:
	{
		cLuxSavedGameMapCollection *pSaved = ((cLuxMapHandler*)a->p[5])->GetSavedMapCollection();
		if(a->i[0] == 0) pSaved->SaveMap((cLuxMap*)a->p[0]);
		else if(a->i[0] == 1) pSaved->LoadMap((cLuxMap*)a->p[0]);
		else a->i[0] = pSaved->MapExists(tString((const char*)a->s[0].p, a->s[0].n));
		break;
	}
	case kOp_WORLD_SOUNDS_FADE:
		gpBase->mpEngine->GetSound()->GetSoundHandler()->FadeOutAll(eSoundEntryType_World, 0.5f, true);
		break;
	case kOp_ACHIEVEMENT:
		gpBase->mpAchievementHandler->UnlockAchievement(a->i[0] == 0 ? eLuxAchievement_EscapeArtist : eLuxAchievement_Descendant);
		break;
	case kOp_PLAYER_SANITY:
		gpBase->mpPlayer->AddSanity(a->f[0], true);
		break;
	case kOp_TIME:
		a->r = (uintptr_t)cPlatform::GetApplicationTime();
		break;
	case kOp_SOUND_ALL:
	{
		cSound *pSound = gpBase->mpEngine->GetSound();
		if(a->i[0] == 0)
		{
			pSound->GetSoundHandler()->PauseAll(eSoundEntryType_All);
			pSound->GetMusicHandler()->Pause();
		}
		else if(a->i[0] == 1)
		{
			pSound->GetSoundHandler()->ResumeAll(eSoundEntryType_All);
			pSound->GetMusicHandler()->Resume();
		}
		else pSound->GetSoundHandler()->StopAll(eSoundEntryType_All);
		break;
	}
	case kOp_WORLD_ACTIVE:
		((cWorld*)a->p[0])->SetActive(a->i[0] != 0);
		break;
	case kOp_VIEWPORT_ACTIVE:
		((cLuxMapHandler*)a->p[5])->GetViewport()->SetActive(a->i[0] != 0);
		((cLuxMapHandler*)a->p[5])->GetViewport()->SetVisible(a->i[0] != 0);
		break;
	case kOp_STOP_COMMENTARY:
		gpBase->mpEffectHandler->GetPlayCommentary()->Stop();
		break;
	case kOp_FOLDER:
		if(a->i[0] == 0) a->rs[0] = Str(((cLuxMapHandler*)a->p[5])->GetMapFolder());
		else ((cLuxMapHandler*)a->p[5])->SetMapFolder("");
		break;
	case kOp_SAVED_RESET:
		((cLuxMapHandler*)a->p[5])->GetSavedMapCollection()->Reset();
		break;
	case kOp_DATACACHE_DESTROY:
		((cLuxMapHandler*)a->p[5])->DestroyDataCache();
		break;
	case kOp_QUIT:
		switch(a->i[0])
		{
		case 0:
			gpBase->mpEngine->GetUpdater()->SetContainer("MainMenu");
			gpBase->mpLoadScreenHandler->DrawMenuScreen();
			break;
		case 1:
			gpBase->mpSaveHandler->AutoSave();
			break;
		case 2:
			gpBase->mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);
			gpBase->SetCustomStory(NULL);
			break;
		default:
			gpBase->mpMainMenu->OnLeaveContainer("");
			gpBase->mpMainMenu->OnEnterContainer("");
			break;
		}
		break;
	case kOp_CLEAR_RENDER_LISTS:
		gpBase->mpEffectRenderer->ClearRenderLists();
		break;
	}
}

void A7DropEntity(void *p){ hplDelete(static_cast<iLuxEntity*>(p)); }
void A7DropDissolve(void *p){ hplDelete(static_cast<cLuxDissolveEntity*>(p)); }
void A7DropLamp(void *p){ hplDelete(static_cast<cLuxLampLightConnection*>(p)); }
void A7DropStart(void *p){ hplDelete(static_cast<cLuxNode_PlayerStart*>(p)); }
void A7DropPos(void *p){ hplDelete(static_cast<cLuxNode_Pos*>(p)); }
void A7DropUse(void *p){ hplDelete(static_cast<cLuxUseItemCallback*>(p)); }

struct cRegister
{
	cRegister(){ ohpl_a13_set_host(Host);}
};
cRegister gRegister;

}

cLuxMap::cLuxMap(const tString& asName)
{
	msName = asName;
	mpA7 = ohpl_a7_new();
	ohpl_a7_set_drop(mpA7, kOhplA7Entity, A7DropEntity);
	ohpl_a7_set_drop(mpA7, kOhplA7Dissolve, A7DropDissolve);
	ohpl_a7_set_drop(mpA7, kOhplA7Lamp, A7DropLamp);
	ohpl_a7_set_drop(mpA7, kOhplA7Start, A7DropStart);
	ohpl_a7_set_drop(mpA7, kOhplA7Pos, A7DropPos);
	ohpl_a7_set_drop(mpA7, kOhplA7UseItem, A7DropUse);
	ohpl_a13_map_init(mpA7, (uintptr_t)this);
}

cLuxMap::~cLuxMap()
{
	ohpl_a13_map_teardown(mpA7);
	ohpl_a7_free(mpA7);
}
