/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "LuxProp_NPC.h"

#include "LuxPlayer.h"
#include "LuxMessageHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxInventory.h"

#include "LuxMap.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_NPC::cLuxPropLoader_NPC(const tString& asName) : iLuxPropLoader(asName)
{
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_NPC::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_NPC, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

class cLuxNpcLoadCtx
{
public:
	cLuxNpcLoadCtx(cLuxProp_NPC *apNPC, cMesh *apMesh, const tString &asFileName) : mpNPC(apNPC), mpMesh(apMesh), msFileName(asFileName)
	{
		mSink.ctx = this; mSink.push = &Push;
		mSkel.ctx = this; mSkel.present = &Present; mSkel.bone_index = &BoneIndex; mSkel.bone_world = &BoneWorld;
	}
	const OhplGa4Sink *Sink() const { return &mSink; }
	const OhplGa4cSkeleton *Skeleton() const { return &mSkel; }

private:
	static cLuxNpcLoadCtx *Me(void *c) { return (cLuxNpcLoadCtx*)c; }
	static bool Present(void *c) { return Me(c)->mpMesh->GetSkeleton() != NULL; }
	static int BoneIndex(void *c, const unsigned char *apName, size_t alLen)
	{
		cLuxNpcLoadCtx *me = Me(c);
		me->mvBoneNames.push_back(tString((const char *)apName, alLen));
		return me->mpMesh->GetSkeleton()->GetBoneIndexByName(me->mvBoneNames.back());
	}
	static void BoneWorld(void *c, int alIdx, float *apOut)
	{
		cBone *pBone = Me(c)->mpMesh->GetSkeleton()->GetBoneByIndex(alIdx);
		cMatrixf mtxWorld = pBone->GetWorldTransform();
		memcpy(apOut, mtxWorld.v, sizeof(mtxWorld.v));
	}
	static void Push(void *c, const OhplGa4Op *apOp)
	{
		cLuxNpcLoadCtx *me = Me(c);
		switch(apOp->code)
		{
		case kOhplGa4cOp_NpcErrorNoSkeleton:
			Error("NPC '%s' is missing skeleton!\n", me->msFileName.c_str());
			break;
		case kOhplGa4cOp_NpcErrorBoneArray:
			Error("MoveHeadBones array is emtpy or do not match size of MoveHeadBoneMuls for NPC '%s'\n", me->msFileName.c_str());
			break;
		case kOhplGa4cOp_NpcBonesResize:
			me->mpNPC->mvHeadMoveBones.resize(apOp->i);
			break;
		case kOhplGa4cOp_NpcErrorBone:
			me->mpNPC->mvHeadMoveBones[apOp->i].mlBoneIdx = apOp->j;
			Error("Move head Bone '%s' does not exist in '%s'!\n", me->mvBoneNames[apOp->i].c_str(), me->msFileName.c_str());
			break;
		case kOhplGa4cOp_NpcBoneSet:
			{
				cLuxProp_NPC_HeadMoveBone& boneData = me->mpNPC->mvHeadMoveBones[apOp->i];
				boneData.mlBoneIdx = apOp->j;
				boneData.mfMul = apOp->f[0];
				boneData.mvRotVec = cVector3f(apOp->v[0], apOp->v[1], apOp->v[2]);
			}
			break;
		}
	}

	cLuxProp_NPC *mpNPC;
	cMesh *mpMesh;
	tString msFileName;
	tStringVec mvBoneNames;
	OhplGa4Sink mSink;
	OhplGa4cSkeleton mSkel;
};

//-----------------------------------------------------------------------

void cLuxPropLoader_NPC::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_NPC  *pNPC = static_cast<cLuxProp_NPC*>(apProp);

	OhplGa4VarsAdapter vars(this);
	cLuxNpcLoadCtx ctx(pNPC, mpMesh, msFileName);
	ohpl_ga4c_npc_load(vars.Vars(), ctx.Skeleton(), &pNPC->mP, ctx.Sink());
}

//-----------------------------------------------------------------------

void cLuxPropLoader_NPC::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
	cLuxProp_NPC  *pNPC = static_cast<cLuxProp_NPC*>(apProp);

	OhplGa4VarsAdapter vars(apInstanceVars);
	vars.Slot(kOhplGa4cSlot_NpcArea, pNPC->msFollowPlayerArea);
	pNPC->SetAwake(ohpl_ga4c_npc_load_instance(vars.Vars()), false);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_NPC::cLuxProp_NPC(const tString &asName, int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_NPC)
{
	 ohpl_ga4c_npc_state_new(&mS);
	 mpFollowPlayerArea = NULL;
}

//-----------------------------------------------------------------------

cLuxProp_NPC::~cLuxProp_NPC()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxProp_NPC::AfterWorldLoad()
{
	if(ohpl_ga4c_npc_after_world_load(msFollowPlayerArea == "") == kOhplGa4cArea_WarnEmpty)
	{
		Warning("NPC '%s' does not have a follow player area!\n", msName.c_str());
		return;
	}

    mpFollowPlayerArea = mpMap->GetEntityByName(msFollowPlayerArea);
	if(ohpl_ga4c_npc_area_missing(mpFollowPlayerArea != NULL))
	{
		Error("Could not find follow area '%s' for NPC '%s'!\n",msFollowPlayerArea.c_str(), msName.c_str());
	}
}

//-----------------------------------------------------------------------

bool cLuxProp_NPC::CanInteract(iPhysicsBody *apBody)
{
	return false;
}

//-----------------------------------------------------------------------

bool cLuxProp_NPC::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return false;
}

//-----------------------------------------------------------------------

void cLuxProp_NPC::OnSetupAfterLoad(cWorld *apWorld)
{
	
}

//-----------------------------------------------------------------------

void cLuxProp_NPC::OnResetProperties()
{

}

//-----------------------------------------------------------------------

void cLuxProp_NPC::UpdatePropSpecific(float afTimeStep)
{
	UpdateWakeState(afTimeStep);
	UpdateCheckPlayerIsInArea(afTimeStep);
	UpdateHeadMovement(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxProp_NPC::BeforePropDestruction()
{

}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_NPC::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return eLuxFocusCrosshair_Default;
}

//-----------------------------------------------------------------------

void cLuxProp_NPC::SetAwake(bool abX, bool abEffects)
{
	OhplGa4Run run(this);
	ohpl_ga4c_npc_set_awake(&mS, abX, abEffects, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_NPC::Ga4ApplyOp(const OhplGa4Op &aOp)
{
	switch(aOp.code)
	{
	case kOhplGa4cOp_NpcAnim:
		{
			const char *sName = "Idle";
			if(aOp.i == kOhplGa4cNpcAnim_Awake)			sName = "Awake";
			else if(aOp.i == kOhplGa4cNpcAnim_Sleep)	sName = "Sleep";
			else if(aOp.i == kOhplGa4cNpcAnim_SleepIdle) sName = "SleepIdle";
			PlayAnimation(sName, aOp.f[0], aOp.j != 0, "");
		}
		break;
	case kOhplGa4cOp_NpcBoneRotate:
		{
			cBoneState *pBoneState = mpMeshEntity->GetBoneState(aOp.i);
			pBoneState->SetUsePreTransform(true);
			cMatrixf mtxRotate;
			memcpy(mtxRotate.v, aOp.m, sizeof(mtxRotate.v));
			pBoneState->SetPreTransform(mtxRotate);
		}
		break;
	default:
		iLuxProp::Ga4ApplyOp(aOp);
	}
}

//-----------------------------------------------------------------------

void cLuxProp_NPC::OnConnectionStateChange(iLuxEntity *apEntity, int alState)
{
	
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

class cLuxNpcHost
{
public:
	cLuxNpcHost(cLuxProp_NPC *apNPC) : mpNPC(apNPC)
	{
		mHost.ctx = this; mHost.check_area = &CheckArea; mHost.mesh_world = &MeshWorld;
		mHost.mesh_position = &MeshPosition; mHost.player_position = &PlayerPosition;
	}
	const OhplGa4cNpcHost *Host() const { return &mHost; }

private:
	static cLuxProp_NPC *Npc(void *c) { return ((cLuxNpcHost*)c)->mpNPC; }
	static bool CheckArea(void *c) { return Npc(c)->mpMap->CheckCollision(Npc(c)->mpFollowPlayerArea, gpBase->mpPlayer); }
	static void MeshWorld(void *c, float *apOut)
	{
		cMatrixf mtxWorld = Npc(c)->mpMeshEntity->GetWorldMatrix();
		memcpy(apOut, mtxWorld.v, sizeof(mtxWorld.v));
	}
	static void MeshPosition(void *c, float *apOut)
	{
		cVector3f vPos = Npc(c)->mpMeshEntity->GetWorldPosition();
		memcpy(apOut, vPos.v, sizeof(vPos.v));
	}
	static void PlayerPosition(void *c, float *apOut)
	{
		cVector3f vPos = gpBase->mpPlayer->GetCharacterBody()->GetPosition();
		memcpy(apOut, vPos.v, sizeof(vPos.v));
	}

	cLuxProp_NPC *mpNPC;
	OhplGa4cNpcHost mHost;
};

//-----------------------------------------------------------------------

void cLuxProp_NPC::UpdateWakeState(float afTimeStep)
{
	OhplGa4Run run(this);
	ohpl_ga4c_npc_update_wake(&mS, mlCurrentNonLoopAnimIndex, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_NPC::UpdateCheckPlayerIsInArea(float afTimeStep)
{
	cLuxNpcHost host(this);
	ohpl_ga4c_npc_update_area(&mS, mpFollowPlayerArea != NULL, afTimeStep, host.Host());
}

//-----------------------------------------------------------------------

void cLuxProp_NPC::UpdateHeadMovement(float afTimeStep)
{
	cLuxNpcHost host(this);
	OhplGa4Run run(this);
	ohpl_ga4c_npc_update_head(&mS, &mP, mvHeadMoveBones.empty() ? NULL : (const OhplGa4cNpcBone*)&mvHeadMoveBones[0], (int)mvHeadMoveBones.size(), afTimeStep, host.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// DEBUG
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

float cLuxProp_NPC::DrawDebug(cGuiSet *apSet,iFontData *apFont,float afStartY)
{
	//apSet->DrawFont(apFont, cVector3f(5,afStartY,1), 13,cColor(1,1),_W("Angle: %f"), cMath::ToDeg(mfHeadAngle));
	//afStartY +=14;

	return afStartY;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxProp_NPC_SaveData, iLuxProp_SaveData)

kSerializeVar(msFollowPlayerArea, eSerializeType_String)
kSerializeVar(mfHeadAngle, eSerializeType_Float32)
kSerializeVar(mbPlayerIsInArea, eSerializeType_Bool)
kSerializeVar(mbAwake, eSerializeType_Bool)
kSerializeVar(mbPlayingWakeAnim, eSerializeType_Bool)
kSerializeVar(mbFollowPlayer, eSerializeType_Bool)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_NPC::CreateSaveData()
{
	return hplNew(cLuxProp_NPC_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_NPC::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_NPC_SaveData *pData = static_cast<cLuxProp_NPC_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	kCopyToVar(pData, msFollowPlayerArea);
	pData->mfHeadAngle = mS.head_angle;
	pData->mbPlayerIsInArea = mS.player_in_area;
	pData->mbAwake = mS.awake;
	pData->mbPlayingWakeAnim = mS.playing_wake_anim;
	pData->mbFollowPlayer = mS.follow_player;
}

//-----------------------------------------------------------------------

void cLuxProp_NPC::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_NPC_SaveData *pData = static_cast<cLuxProp_NPC_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	kCopyFromVar(pData, msFollowPlayerArea);
	mS.head_angle = pData->mfHeadAngle;
	mS.player_in_area = pData->mbPlayerIsInArea;
	mS.awake = pData->mbAwake;
	mS.playing_wake_anim = pData->mbPlayingWakeAnim;
	mS.follow_player = pData->mbFollowPlayer;
}

//-----------------------------------------------------------------------

void cLuxProp_NPC::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
