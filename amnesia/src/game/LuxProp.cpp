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

#include "LuxProp.h"

#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxInteractConnections.h"

#include <cstring>



//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxPropLoader::iLuxPropLoader(const tString& asName) : cEntityLoader_Object(asName)
{
	mfDefaultMaxFocusDistance = gpBase->mpGameCfg->GetFloat("Player_Interaction", "Default_DefaultMaxFocusDist",0);

	mbForceFullGameSave = false;
}

//-----------------------------------------------------------------------

void iLuxPropLoader::BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{

}

//-----------------------------------------------------------------------

void iLuxPropLoader::AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	cLuxMap *pMap = gpBase->mpCurrentMapLoading;
	if(pMap==NULL)	return;

	iLuxProp *pProp = CreateProp(mpEntity->GetName(), mlID,pMap);

	//////////////////////////////
	// Set data
	pProp->mpWorld = apWorld;
	pProp->msFileName = msFileName;
	pProp->m_mtxOnLoadTransform = a_mtxTransform;
	pProp->mvOnLoadScale = mvScale;

	pProp->mvBodies = mvBodies;
	pProp->mvJoints = mvJoints;

	pProp->mvBodyExtraData = mvBodyExtraData;

	pProp->mvLights = mvLights;
	pProp->mvParticleSystems = mvParticleSystems;
	pProp->mvBillboards = mvBillboards;
	pProp->mvBeams = mvBeams;
	pProp->mvSoundEntities = mvSoundEntities;

	pProp->mpMeshEntity = mpEntity;

	//Set body userdata
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		iPhysicsBody *pBody = mvBodies[i];
		iLuxEntity *pEnt = pProp;
		pBody->SetUserData(pEnt);
	}

	//////////////////////////////
	// Load base properties
	OhplGa4BaseParams bp;
	{
		OhplGa4VarsAdapter vars(this);
		vars.Slot(kOhplGa4Slot_MainBody, pProp->msMainBodyName)
			.Slot(kOhplGa4Slot_EffectsOnSound, pProp->msEffectsOnSound)
			.Slot(kOhplGa4Slot_EffectsOffSound, pProp->msEffectsOffSound)
			.Slot(kOhplGa4Slot_MoveStartSound, pProp->msMoveStartSound)
			.Slot(kOhplGa4Slot_MoveStopSound, pProp->msMoveStopSound)
			.Slot(kOhplGa4Slot_MoveLoopSound, pProp->msMoveLoopSound);
		ohpl_ga4_base_load(vars.Vars(), &bp);
	}
	pProp->mfHealth = bp.health;
	pProp->mlToughness = bp.toughness;
	pProp->mfMaxFocusDistance = bp.max_focus_distance;
	pProp->mbDissolveOnDestruction = bp.dissolve_on_destruction;
	pProp->mfDissolveTime = bp.dissolve_time;
	pProp->mbShowMesh = bp.show_mesh;
	if(mpEntity) mpEntity->SetVisible(pProp->mbShowMesh);
	pProp->mbShowHints = bp.show_hints;
	pProp->mfEffectsOnTime = bp.effects_on_time;
	pProp->mfEffectsOffTime = bp.effects_off_time;
	pProp->mEffectsOffLightColor = cColor(bp.effects_off_light_color[0], bp.effects_off_light_color[1], bp.effects_off_light_color[2], bp.effects_off_light_color[3]);
	pProp->mfEffectsOffLightRadius = bp.effects_off_light_radius;
	pProp->mbMoveCheckCollision = bp.move_check_collision;

	gpBase->PreloadSound(pProp->msMoveStartSound);
	gpBase->PreloadSound(pProp->msMoveStopSound);
	gpBase->PreloadSound(pProp->msMoveLoopSound);

	//////////////////////////////
	// Load misc properties
	if(pProp->mpMeshEntity && pProp->mpMeshEntity->GetAnimationStateNum()>0 && bp.randomize_anim_start)
	{
		cAnimationState *pAnim = pProp->mpMeshEntity->GetAnimationState(0);
		float fLength = pAnim->GetLength();
		float fStartTime = cMath::RandRectf(0, fLength);

		pAnim->SetTimePosition(fStartTime);
	}

	//////////////////////////////
	// Load type specific properties
	LoadVariables(pProp, apRootElem);

	//////////////////////////////
	// Extra setup

	//If focus distance is still 0 or below, set it as default.
	if(pProp->mfMaxFocusDistance<=0) pProp->mfMaxFocusDistance = mfDefaultMaxFocusDistance;

	pProp->SetupAfterLoad(apWorld);

	//Add to the current map
	pMap->AddEntity(pProp);

	pProp->SetActive(mbActive);

	///////////////////////////////
	// Instance vars
	if(apInstanceVars)
	{
		OhplGa4BaseInstance bi;
		{
			OhplGa4VarsAdapter inst(apInstanceVars);
			inst.Slots(kOhplGa4Slot_ConnectedProp, pProp->mvConnectedProps)
				.Slot(kOhplGa4Slot_CallbackFunc, pProp->msCallbackFunc)
				.Slot(kOhplGa4Slot_ConnChange, pProp->msConnectionStateChangeCallback);
			tString sLookAt, sInteract;
			inst.Slot(kOhplGa4Slot_LookAt, sLookAt).Slot(kOhplGa4Slot_Interact, sInteract);
			ohpl_ga4_base_load_instance(inst.Vars(), mbForceFullGameSave, &bi);

			if(pProp->mpMeshEntity)
			{
				pProp->mpMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, bi.cast_shadows);
			}

			pProp->mbFullGameSave = bi.full_game_save;

			pProp->SetPlayerLookAtCallback(sLookAt, bi.look_at_auto_remove);
			pProp->SetPlayerInteractCallback(sInteract, bi.interact_auto_remove);

			pProp->SetStaticPhysics(bi.static_physics);
		}

		LoadInstanceVariables(pProp, apInstanceVars);
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxProp::iLuxProp(const tString &asName, int alID, cLuxMap *apMap, eLuxPropType aPropType) : iLuxEntity(asName,alID,apMap, eLuxEntityType_Prop)
{
	mPropType = aPropType;

	mpAttachmentParent = NULL;

	mbIsInteractedWith = false;

	ohpl_ga4_base_init(&mBase);

	mbHasFlickering = false;

	mbInteractionDisabled = false;

	mbStaticPhysics = false;

	mpMainBody = NULL;

	mpMoveLoopSound = NULL;

	mlCurrentNonLoopAnimIndex = -1;

    mpParentBone = NULL;
	mfBodyMassBackup = 1;

	msParentBoneName = "";
	msParentEntityName = "";
}

//-----------------------------------------------------------------------

iLuxProp::~iLuxProp()
{
	cWorld *pWorld = mpWorld;
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

	////////////////////
	// Attachment
	if(mpAttachmentParent && mpMap && mpMap->IsDeletingAllWorldEntities()==false)
	{
		mpAttachmentParent->RemoveAttachedProp(this);
		mpAttachmentParent = NULL;
	}
	for(tLuxProp_AttachedPropListIt it = mlstAttachedProps.begin(); it != mlstAttachedProps.end(); ++it)
	{
		//Map is currently deleting all 
		if(mpMap->IsDeletingAllWorldEntities())
		{
			hplDelete(*it);
		}
		else
		{
			DestroyAttachedProp(*it);
		}
	}
	mlstAttachedProps.clear();


	////////////////////
	// Connections
	STLDeleteAll(mvInteractConnections);


	////////////////////
	// Move stuff
	if(mpMoveLoopSound && pWorld->SoundEntityExists(mpMoveLoopSound,mlMoveLoopSoundID))
	{
		pWorld->DestroySoundEntity(mpMoveLoopSound);
	}
	

	////////////////////
	// Check if entity is used
	// TODO?

	////////////////////
	// Destroy physics
	{
		//Joints
		for(size_t i=0; i<mvJoints.size(); ++i)
		{
			iPhysicsJoint *pJoint = mvJoints[i];

			if(pJoint && pPhysicsWorld->JointExists(pJoint))
			{
				pPhysicsWorld->DestroyJoint(pJoint);
			}
		}

		//Bodies
		for(size_t i=0; i<mvBodies.size(); ++i)
		{
			iPhysicsBody *pBody = mvBodies[i];
				
			pWorld->GetPhysicsWorld()->DestroyBody(pBody);
		}
	}

	////////////////////
	// Destroy graphics
	{
		//Mesh entity
		if(mpMeshEntity) pWorld->DestroyMeshEntity(mpMeshEntity);

		//Lights
		for(size_t i=0; i<mvLights.size(); ++i) pWorld->DestroyLight(mvLights[i]);
		
		//Particle systems
		for(size_t i=0; i<mvParticleSystems.size(); ++i) 
		{
			cParticleSystem *pPS = mvParticleSystems[i];
			if(pPS && pWorld->ParticleSystemExists(pPS)) pPS->Kill();
		}

		//Billboards
		for(size_t i=0; i<mvBillboards.size(); ++i)	pWorld->DestroyBillboard(mvBillboards[i]);
		
		//Beams
		for(size_t i=0; i<mvBeams.size(); ++i)		pWorld->DestroyBeam(mvBeams[i]);
		
		//Sound entities
		for(size_t i=0; i<mvSoundEntities.size(); ++i) 	pWorld->DestroySoundEntity(mvSoundEntities[i]);
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void iLuxProp::SetupAfterLoad(cWorld *apWorld)
{
	//////////////////////
	// Normal setup
	mvDefaultBodySettings.resize(mvBodies.size());
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		iPhysicsBody *pBody = mvBodies[i];
        
		mvDefaultBodySettings[i].mbCollideCharacter = pBody->GetCollideCharacter();
	}

	//Get the main body if any
	if(msMainBodyName != "")
	{
		mpMainBody = (iPhysicsBody*)STLFindByName(mvBodies,msName + "_"+ msMainBodyName);
        if(mpMainBody == NULL)
		{
			Warning("Could not find main physics body '%s'\n", msName.c_str());
		}
	}
	
	///////////////////////
	// Set body data
	mvBodyData.resize(mvBodies.size());
	for(size_t i=0; i<mvBodyData.size(); ++i)
	{
		mvBodyData[i].mfMass = mvBodies[i]->GetMass();
	}

	///////////////////////
	// Effects stuff
	SetupEffectData();
	
	///////////////////////
	// Setup implemented 
	OnSetupAfterLoad(apWorld);
}

//-----------------------------------------------------------------------

void iLuxProp::SetParentBone(cBoneState * apParentBone, const cMatrixf & apParentBoneOffsetMatrix, tString asParentName, tString asParentBoneName)
{
    mpParentBone = apParentBone;
    mpParentBoneOffsetMatrix = apParentBoneOffsetMatrix;

	msParentEntityName = asParentName;
	msParentBoneName = asParentBoneName;

    if ( mvBodies.size() > 0 )
    {
        iPhysicsBody *pBody = mvBodies[0];

        mfBodyMassBackup = pBody->GetMass();
        mbBodyCollideBackup = pBody->GetCollide();
        mbBodyCollideCharacterBackup = pBody->GetCollideCharacter();
        mbBodyActiveBackup = pBody->IsActive();
        mBodyMatrixBackup = pBody->GetLocalMatrix();

		pBody->SetMass(0);
		pBody->SetCollide(false);
		pBody->SetCollideCharacter(false);
		pBody->SetActive(false);
		pBody->SetMatrix(cMath::MatrixMul(mpParentBone->GetWorldMatrix(),mpParentBoneOffsetMatrix));
    }
}

//-----------------------------------------------------------------------

void iLuxProp::DetachFromParentBone()
{
    if ( mpParentBone != NULL && mvBodies.size() > 0 )
    {
        mpParentBone = NULL;

        iPhysicsBody *pBody = mvBodies[0];
        pBody->SetMass(mfBodyMassBackup);
		pBody->SetCollide(mbBodyCollideBackup);
		pBody->SetCollideCharacter(mbBodyCollideCharacterBackup);
		pBody->SetActive(mbBodyActiveBackup);
		//pBody->SetMatrix(mBodyMatrixBackup);
    }

	msParentBoneName = "";
	msParentEntityName = "";
}

//-----------------------------------------------------------------------

void iLuxProp::UpdateParentBone(float afTimeStep)
{
    if ( mpParentBone && mvBodies.size() > 0)
	{
        //this->GetMeshEntity()->SetWorldMatrix(cMath::MatrixMul(mpParentBone->GetWorldMatrix(),mpParentBoneOffsetMatrix));
        iPhysicsBody *pBody = mvBodies[0];

    	pBody->SetMatrix(cMath::MatrixMul(mpParentBone->GetWorldMatrix(),mpParentBoneOffsetMatrix));
	}
}

//-----------------------------------------------------------------------

void iLuxProp::OnUpdate(float afTimeStep)
{
	UpdateParentBone(afTimeStep);

	///////////////////////
	// Prop update
	UpdateMoving(afTimeStep);
	UpdateCheckIfOutsidePlayer(afTimeStep);
	UpdateEffectFading(afTimeStep);
	UpdateMeshFading(afTimeStep);
	UpdateAnimation(afTimeStep);
	UpdateAttachedProps(afTimeStep, false);

	///////////////////////
	// Connections
	for(size_t i=0; i<mvInteractConnections.size(); ++i)
	{
		mvInteractConnections[i]->Update(afTimeStep);
		mvInteractConnections[i]->UpdateProp(afTimeStep);
	}

	//////////////////////
	// Specific update
	UpdatePropSpecific(afTimeStep);

	//if(mvLights.size() > 0)
	//	Log("End Color: %s\n", mvLights[0]->GetDiffuseColor().ToString().c_str());
}

//-----------------------------------------------------------------------

void iLuxProp::BeforeEntityDestruction()
{
	if(mbDissolveOnDestruction && mpMeshEntity)
	{
		mpMap->AddDissolveEntity(mpMeshEntity, mfDissolveTime);
	}

	BeforePropDestruction();
}

//-----------------------------------------------------------------------

void iLuxProp::OnRenderSolid(cRendererCallbackFunctions* apFunctions)
{
	return;
	if(mvBodies.size()<=0) return;
	
	cBoundingVolume* pBV = mvBodies[0]->GetBoundingVolume();
	apFunctions->GetLowLevelGfx()->DrawBoxMinMax(pBV->GetMin(), pBV->GetMax(),cColor(1,1,1,1));
}

//-----------------------------------------------------------------------

OhplGa4EffectsIn iLuxProp::Ga4EffectsEnv(bool abActive, bool abFade)
{
	OhplGa4EffectsIn env;
	env.active = abActive;
	env.fade = abFade;
	env.n_ps = (int)mvParticleSystems.size();
	env.n_sounds = (int)mvSoundEntities.size();
	env.on_time = mfEffectsOnTime;
	env.off_time = mfEffectsOffTime;
	env.off_radius = mfEffectsOffLightRadius;
	env.on_sound = msEffectsOnSound != "";
	env.off_sound = msEffectsOffSound != "";
	return env;
}

void iLuxProp::SetEffectsActive(bool abActive, bool abFadeAndPlaySounds)
{
	OhplGa4EffectsIn env = Ga4EffectsEnv(abActive, abFadeAndPlaySounds);
	OhplGa4Run run(this);
	ohpl_ga4_effects_set_active(&mBase, &env, Ga4Lights(), Ga4LightNum(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void iLuxProp::GiveDamage(float afAmount, int alStrength)
{
	float fAmount;
	float fHealth = ohpl_ga4_give_damage(mfHealth, mlToughness, afAmount, alStrength, &fAmount);

	SetHealth(fHealth);

	OnDamage(fAmount, alStrength);
}

//-----------------------------------------------------------------------

void iLuxProp::SetDisableCollisionUntilOutSidePlayer(bool abX)
{
	mBase.check_outside_player = abX;

	if(mBase.check_outside_player)
	{
		for(size_t i=0; i<mvBodies.size(); ++i)
		{
			iPhysicsBody *pBody = mvBodies[i];
			pBody->SetCollideCharacter(false);
		}
	}
}

//-------------------------------------------------------------------

void iLuxProp::MoveLinearTo(const cVector3f& avGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed)
{
	OhplGa4Run run(this);
	ohpl_ga4_move_linear_to(&mBase, avGoal.v, afAcc, afMaxSpeed, afSlowdownDist, abResetSpeed, run.Sink());
	run.Finish();
}

//-------------------------------------------------------------------

void iLuxProp::MoveAngularTo(const cMatrixf& a_mtxGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed,
							 bool abUseOffset, const cVector3f &avWorldOffset, const cVector3f &avLocalOffset)
{
	OhplGa4Run run(this);
	ohpl_ga4_move_angular_to(&mBase, a_mtxGoal.v, afAcc, afMaxSpeed, afSlowdownDist, abResetSpeed, abUseOffset, avWorldOffset.v, avLocalOffset.v, run.Sink());
	run.Finish();
}

//-------------------------------------------------------------------

void iLuxProp::RotateAtSpeed(	float afAcc, float afGoalSpeed, const cVector3f& avAxis, bool abResetSpeed,
								 bool abUseOffset, const cVector3f &avWorldOffset, const cVector3f &avLocalOffset)
{
	OhplGa4Run run(this);
	ohpl_ga4_rotate_at_speed(&mBase, afAcc, afGoalSpeed, avAxis.v, abResetSpeed, abUseOffset, avWorldOffset.v, avLocalOffset.v, run.Sink());
	run.Finish();
}

//-------------------------------------------------------------------

void iLuxProp::StopMove()
{
	OhplGa4Run run(this);
	ohpl_ga4_stop_move(&mBase, run.Sink());
	run.Finish();
}

//-------------------------------------------------------------------

void iLuxProp::FadeInMeshEntity(float afTime)
{
	OhplGa4Run run(this);
	ohpl_ga4_fade_in_mesh(&mBase, afTime, mpMeshEntity!=NULL, run.Sink());
	run.Finish();
}

//-------------------------------------------------------------------

void iLuxProp::ResetProperties()
{
	///////////////////////////////////
	// Reset bodies
    for(size_t i=0; i<mvBodies.size(); ++i)
	{
		cMatrixf mtxBody = cMath::MatrixMul(m_mtxOnLoadTransform, mvBodyExtraData[i].m_mtxLocalTransform);
        iPhysicsBody *pBody = mvBodies[i];

		pBody->SetMatrix(mtxBody);
		pBody->SetLinearVelocity(0);
		pBody->SetAngularVelocity(0);
	}
	
	///////////////////////////////////
	// Reset variables
	ohpl_ga4_reset_move(&mBase);
	mpMoveLoopSound = NULL;

	mfHealth = 100;

	///////////////////////////////////
	// Reset type specific
	OnResetProperties();
}

//-------------------------------------------------------------------

void iLuxProp::PlayAnimation(const tString& asName, float afFadeTime, bool abLoop, const tString& asCallback)
{
	if(mpMeshEntity==NULL) return;

	int lIdx = mpMeshEntity->GetAnimationStateIndex(asName);
	if(lIdx<0)
	{
		Error("Could not find animation '%s' for entity '%s'\n", asName.c_str(), msName.c_str());
		return;
	}

	mpMeshEntity->PlayFadeTo(lIdx, abLoop, afFadeTime);
	if(abLoop==false)	mlCurrentNonLoopAnimIndex = lIdx;
	else				mlCurrentNonLoopAnimIndex = -1;
	
	if(abLoop==false)	msAnimCallback = asCallback;
	else				msAnimCallback = "";
}

//-------------------------------------------------------------------

void iLuxProp::StopAnimation()
{
	if(mpMeshEntity==NULL) return;

	mpMeshEntity->Stop();
}

//-------------------------------------------------------------------

void iLuxProp::PlayCurrentAnimation(float afFadeTime, bool abLoop)
{
	if(mpMeshEntity==NULL) return;

	for(int i = 0; i < mpMeshEntity->GetAnimationStateNum(); ++i)
	{
		cAnimationState *pState = mpMeshEntity->GetAnimationState(i);
		if(pState && pState->IsActive())
		{
			pState->SetLoop(abLoop);
			pState->SetSpeed(pState->GetBaseSpeed());
		}
	}
}
	
void iLuxProp::PauseCurrentAnimation(float afFadeTime)
{
	if(mpMeshEntity==NULL) return;

	for(int i = 0; i < mpMeshEntity->GetAnimationStateNum(); ++i)
	{
		cAnimationState *pState = mpMeshEntity->GetAnimationState(i);
		if(pState && pState->IsActive()) pState->SetSpeed(0);
	}
}

//-------------------------------------------------------------------

void iLuxProp::SetAnimationSpeed(float afSpeed)
{
	if(mpMeshEntity==NULL) return;

	//////////////
	// Set the animation speed of all states
	for(int i = 0; i < mpMeshEntity->GetAnimationStateNum(); ++i)
	{
		cAnimationState *pState = mpMeshEntity->GetAnimationState(i);

		if(pState)
		{
			////////////
			// Set the speed
			pState->SetSpeed(afSpeed);
		}
	}
}

void iLuxProp::SetAnimationPosition(float afPosition)
{
	if(mpMeshEntity==NULL) return;

	//////////////
	// Set the animation speed of all states
	for(int i = 0; i < mpMeshEntity->GetAnimationStateNum(); ++i)
	{
		cAnimationState *pState = mpMeshEntity->GetAnimationState(i);

		if(pState->IsActive())
		{
			////////////
			// Set the speed
			pState->SetTimePosition(afPosition);
		}
	}
}

//-------------------------------------------------------------------

void iLuxProp::SetHealth(float afX)
{
	mfHealth = afX;

	OnHealthChange();
}

//-------------------------------------------------------------------

iEntity3D* iLuxProp::GetAttachEntity()
{
	if(mvBodies.empty())
	{
		return mpMeshEntity;
	}
	else
	{
		return mpMainBody ? mpMainBody : mvBodies[0];
	}
}

//-------------------------------------------------------------------

void iLuxProp::SetStaticPhysics(bool abX)
{
	if(mbStaticPhysics == abX) return;

	mbStaticPhysics = abX;

    for(size_t i=0; i<mvBodies.size(); ++i)
	{
		iPhysicsBody *pBody = mvBodies[i];

		pBody->SetMass(mbStaticPhysics ? 0 : mvBodyData[i].mfMass);
		pBody->SetLinearVelocity(0);
		pBody->SetAngularVelocity(0);
	}
}

//-------------------------------------------------------------------

void iLuxProp::AddAndAttachProp(const tString& asName, const tString& asFileName, const cMatrixf& a_mtxOffset)
{
	if(GetMainBody()==NULL)
	{
		Error("Could not attach prop %s to %s because it does not have a main body!\n", msName.c_str(), asFileName.c_str());
		return;
	}

	/////////////////////////////////
	// Create the entity
	mpMap->ResetLatestEntity();
	mpMap->CreateEntity(asName, asFileName, cMatrixf::Identity,1);
    
	iLuxEntity *pEntity = mpMap->GetLatestEntity();
	if(pEntity==NULL || pEntity->GetEntityType() != eLuxEntityType_Prop || pEntity->GetName() != asName)
	{
		Error("Could not create attach entity '%s' for prop '%s'\n", asFileName.c_str(), msName.c_str());
		return;
	}

	iLuxProp* pProp =  static_cast<iLuxProp*>(pEntity);

	/////////////////////////////////
	// This prop shall not be saved!
	pProp->SetIsSaved(false);

	/////////////////////////////////
	// Turn off collision on all bodies and set mass to 0
	for(size_t i=0; i<pProp->mvBodies.size(); ++i)
	{
		iPhysicsBody *pBody = pProp->mvBodies[i];
		pBody->SetMass(0);
		pBody->SetCollide(false);
		pBody->SetCollideCharacter(false);
		pBody->SetActive(false);
	}
	
	/////////////////////////////////
	// Create attachment data
	cLuxProp_AttachedProp *pAttachProp = hplNew(cLuxProp_AttachedProp, ());
	pAttachProp->msName = asName;
	pAttachProp->msFileName = asFileName;
	pAttachProp->m_mtxOffset = a_mtxOffset;
	pAttachProp->mpProp = pProp;

	mlstAttachedProps.push_back(pAttachProp);
	pProp->SetAttachmentParent(this);

	//////////////////////////////////
	// Update the attached prop matrix so it starts out properly!
	UpdateAttachedProps(0, true);
}

//-------------------------------------------------------------------

bool iLuxProp::DestroyAttachedProp(const tString& asName)
{
	tLuxProp_AttachedPropListIt it = mlstAttachedProps.begin(); 
	for(; it != mlstAttachedProps.end(); ++it)
	{
		cLuxProp_AttachedProp *pAttachedProp = *it;
        if(pAttachedProp->msName == asName)
		{
			DestroyAttachedProp(pAttachedProp);
			mlstAttachedProps.erase(it);
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------

void iLuxProp::DestroyAttachedProp(cLuxProp_AttachedProp *apAttachedProp)
{
	apAttachedProp->mpProp->mpAttachmentParent = NULL; //Must set this null, else it will try and erase itself when delted!
    mpMap->DestroyEntity(apAttachedProp->mpProp);
	hplDelete(apAttachedProp);
}

//-------------------------------------------------------------------

bool iLuxProp::RemoveAttachedProp(iLuxProp *apProp)
{
	tLuxProp_AttachedPropListIt it = mlstAttachedProps.begin(); 
	for(; it != mlstAttachedProps.end(); ++it)
	{
		cLuxProp_AttachedProp *pAttachedProp = *it;
		if(pAttachedProp->mpProp == apProp)
		{
			apProp->SetAttachmentParent(NULL);
			hplDelete(pAttachedProp);

			mlstAttachedProps.erase(it);
			return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------

int iLuxProp::AddInteractConnection(iLuxInteractConnection *apConnection)
{
	mvInteractConnections.push_back(apConnection);
	return (int) mvInteractConnections.size()-1;
}

iLuxInteractConnection* iLuxProp::GetInteractionConnectionFromIndex(int alIndex)
{
	return mvInteractConnections[alIndex];
}

iLuxInteractConnection* iLuxProp::GetInteractionConnectionFromName(const tString& asName)
{
	return (iLuxInteractConnection*)STLFindByName(mvInteractConnections, asName);
}

void iLuxProp::DestroyInteractConnection(iLuxInteractConnection *apConnection)
{
	STLFindAndDelete(mvInteractConnections, apConnection);
}

//-----------------------------------------------------------------------

void iLuxProp::InteractConnectionLimit(int alState)
{
	for(size_t i=0; i<mvInteractConnections.size(); ++i)
	{
		iLuxInteractConnection *pConnection = mvInteractConnections[i];
        
		if(pConnection->GetInteractionOnly() && mbIsInteractedWith==false) continue;

		mvInteractConnections[i]->OnLimit(alState);	
	}
}

void iLuxProp::InteractConnectionTurn(float afAngle, float afPrevAngle, float afMinAngle, float afMaxAngle)
{
	//if(abs(afAngle - afPrevAngle) < 0.001f) return;

	float fT = ohpl_ga4_connection_turn(afAngle, afMinAngle, afMaxAngle);

	for(size_t i=0; i<mvInteractConnections.size(); ++i)
	{
		iLuxInteractConnection *pConnection = mvInteractConnections[i];

		if(pConnection->GetInteractionOnly() && mbIsInteractedWith==false) continue;

		pConnection->OnTurn(afAngle - afPrevAngle, fT);
	}
}

//-----------------------------------------------------------------------

iPhysicsBody* iLuxProp::GetBodyFromID(int alID)
{
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		iPhysicsBody *pBody = mvBodies[i];
		if(pBody->GetUniqueID() == alID) return pBody;
	}

	return NULL;
}

//-----------------------------------------------------------------------

int iLuxProp::GetBodyIndexFromName(const tString& asName)
{
	tString sFinalName = msName + '_'+asName;
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		iPhysicsBody *pBody = mvBodies[i];
		if(pBody->GetName() == sFinalName) return (int)i;
	}
	return -1;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void iLuxProp::OnSetActive(bool abX)
{	
	///////////////
	//Bodies
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		mvBodies[i]->SetActive(abX);
	}

	///////////////////
	//Mesh entity
	if(mpMeshEntity && mbShowMesh)
	{
		mpMeshEntity->SetActive(abX);
		mpMeshEntity->SetVisible(abX);
	}

	///////////////////
	//Lights
	for(size_t i=0; i<mvLights.size(); ++i)
	{
		mvLights[i]->SetVisible(abX);
		mvLights[i]->SetActive(abX);
	}

	///////////////////
	//Particle systems
	for(size_t i=0; i<mvParticleSystems.size(); ++i) 
	{
		cParticleSystem *pPS = mvParticleSystems[i];
		if(pPS)
		{
			pPS->SetVisible(abX);
			pPS->SetActive(abX);
		}
	}

	///////////////////
	//Billboards
	for(size_t i=0; i<mvBillboards.size(); ++i)
	{
		mvBillboards[i]->SetActive(abX);
		mvBillboards[i]->SetVisible(abX);
	}

	///////////////////
	//Beams
	for(size_t i=0; i<mvBeams.size(); ++i)
	{
		mvBeams[i]->SetActive(abX);
		mvBeams[i]->SetVisible(abX);
	}

	///////////////////
	//Sound entities
	for(size_t i=0; i<mvSoundEntities.size(); ++i) 
	{
		cSoundEntity *pSoundEntity = mvSoundEntities[i];

		if(abX)	pSoundEntity->Play(false);
		else	pSoundEntity->Stop(false);	
	}
	
	ImplementedOnSetActive(abX);
}



//-----------------------------------------------------------------------

void iLuxProp::SetupEffectData()
{
	/////////////////////
	// Get particle data
	for(size_t i=0; i<mvParticleSystems.size(); ++i)
	{
		cParticleSystem *pPS = mvParticleSystems[i];
		cLuxProp_PSData psData;

		psData.msName = pPS->GetName().c_str();
		psData.msDataName = pPS->GetDataName();
		psData.m_mtxLocalTransform = pPS->GetLocalMatrix();
		psData.mpParent = pPS->GetEntityParent();

		mvEffectPSData.push_back(psData);
	}

	/////////////////////
	// Get light data
	for(size_t i=0; i<mvLights.size(); ++i)
	{
		iLight *pLight = mvLights[i];
		cLuxProp_LightData lightData;

		lightData.mOnColor = pLight->GetDiffuseColor();
		lightData.mfOnRadius = pLight->GetRadius();
		lightData.mbFlickering = pLight->GetFlickerActive();

		//TODO: Check all lights?
		if(i==0 && pLight->GetFlickerActive()) mbHasFlickering = true;

		mvEffectLightData.push_back(lightData);
	}

	/////////////////////
	// Get billboard data
	for(size_t i=0; i<mvBillboards.size(); ++i)
	{
		cBillboard *pBillboard = mvBillboards[i];
		cLuxProp_BillboardData bbData;

		bbData.mbConnectedToLight = BillboardConnectedToLight(pBillboard);
		bbData.mOnColor = pBillboard->GetColor();

		mvEffectBillboardData.push_back(bbData);	
	}
}

//-----------------------------------------------------------------------

bool iLuxProp::BillboardConnectedToLight(cBillboard *apBB)
{
	for(size_t light=0; light<mvLights.size(); ++light)
	{
		iLight *pLight = mvLights[light];
		std::vector<cLightBillboardConnection> *pBBVEc = pLight->GetBillboardVec();
		if(pBBVEc->empty()) continue;
        
		for(size_t i=0; i<pBBVEc->size(); ++i)
		{
			if( (*pBBVEc)[i].mpBillboard == apBB) return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------

void iLuxProp::UpdateEffectFading(float afTimeStep)
{
	OhplGa4Run run(this);
	ohpl_ga4_effects_update(&mBase, afTimeStep, mfEffectsOnTime, mfEffectsOffTime, Ga4Lights(), Ga4LightNum(), mpMeshEntity!=NULL, run.Sink());
	run.Finish();
}


//-----------------------------------------------------------------------

void iLuxProp::UpdateMeshFading(float afTimeStep)
{
	OhplGa4Run run(this);
	ohpl_ga4_mesh_fading(&mBase, afTimeStep, mpMeshEntity!=NULL, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void iLuxProp::UpdateAttachedProps(float afTimeStep, bool abForceUpdate)
{
	if(mlstAttachedProps.empty()) return;
	if(abForceUpdate==false && GetMainBody()->GetEnabled()==false) return;

	tLuxProp_AttachedPropListIt it = mlstAttachedProps.begin(); 
	for(; it != mlstAttachedProps.end(); ++it)
	{
		cLuxProp_AttachedProp *pAttachProp = *it;
		iLuxProp *pProp = pAttachProp->mpProp;
		if(pProp->GetBodyNum()<=0) continue;

		iPhysicsBody *pBody = pProp->GetBody(0);
		
		cMatrixf mtxProp = cMath::MatrixMul(GetMainBody()->GetLocalMatrix(), 
											cMath::MatrixMul(pAttachProp->m_mtxOffset, pProp->mvBodyExtraData[0].m_mtxLocalTransform));
		pBody->SetMatrix(mtxProp);
	}
}

//-----------------------------------------------------------------------

void iLuxProp::UpdateAnimation(float afTimeStep)
{
	bool bOver = false;
	if(mlCurrentNonLoopAnimIndex>=0)
	{
		cAnimationState *pAnimState = mpMeshEntity->GetAnimationState(mlCurrentNonLoopAnimIndex);
		bOver = pAnimState->IsOver();
	}

	OhplGa4Run run(this);
	ohpl_ga4_anim_update(&mlCurrentNonLoopAnimIndex, bOver, msAnimCallback != "", run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void iLuxProp::UpdateMoveSoundVolume()
{
	if(mpMoveLoopSound==NULL) return;

	cSoundEntry *pEntry = mpMoveLoopSound->GetSoundEntry(eSoundEntityType_Main, true);
	if(pEntry) pEntry->SetVolumeMul(mBase.moving_volume);
}

//-----------------------------------------------------------------------

void iLuxProp::UpdateMoving(float afTimeStep)
{
	OhplGa4MoveIn in;
	memset(&in, 0, sizeof(in));
	in.dt = afTimeStep;
	if(mBase.moving) memcpy(in.body, GetMainBody()->GetLocalMatrix().v, sizeof(in.body));
	in.check_collision = mbMoveCheckCollision;
	in.loop_present = mpMoveLoopSound != NULL;
	in.loop_alive = mpMoveLoopSound && mpMap->GetWorld()->SoundEntityExists(mpMoveLoopSound, mlMoveLoopSoundID);
	in.loop_sound = msMoveLoopSound != "";

	OhplGa4Run run(this);
	ohpl_ga4_update_moving(&mBase, &in, run.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void iLuxProp::UpdateCheckIfOutsidePlayer(float afTimeStep)
{
	if(mBase.check_outside_player==false) return;

	iPhysicsBody *pPlayerBody = gpBase->mpPlayer->GetCharacterBody()->GetCurrentBody();

	std::vector<OhplGa4OutsideBody> vBodies(mvBodies.size());
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		iPhysicsBody *pBody = mvBodies[i];
		OhplGa4OutsideBody &b = vBodies[i];

		b.collide_character = pBody->GetCollideCharacter();
		b.default_collide_character = mvDefaultBodySettings[i].mbCollideCharacter;
		cVector3f vPos = pBody->GetLocalPosition();
		cVector3f vVel = pBody->GetLinearVelocity();
		memcpy(b.pos, vPos.v, sizeof(b.pos));
		memcpy(b.vel, vVel.v, sizeof(b.vel));
	}
	cVector3f vPlayerPos = pPlayerBody->GetLocalPosition();

	OhplGa4Run run(this);
	bool bDone = ohpl_ga4_outside_player(vBodies.empty() ? NULL : &vBodies[0], (int)vBodies.size(), vPlayerPos.v, run.Host(), run.Sink());
	run.Finish();

	if(bDone)
	{
		mBase.check_outside_player = false;
	}
}

//-----------------------------------------------------------------------

void iLuxProp::Ga4MovePush(const float *apAdd, float *apOut)
{
	iPhysicsWorld *pPhysicsWorld = mpMap->GetPhysicsWorld();
	iPhysicsBody *pBody = GetMainBody();

	cMatrixf mtxNew = pBody->GetLocalMatrix();
	cVector3f vAdd(apAdd[0], apAdd[1], apAdd[2]);
	mtxNew.SetTranslation(mtxNew.GetTranslation() + vAdd);

	cVector3f vPushVec(0);
	pPhysicsWorld->CheckShapeWorldCollision(&vPushVec,pBody->GetShape(),mtxNew, NULL, true, false);
	apOut[0] = vPushVec.x; apOut[1] = vPushVec.y; apOut[2] = vPushVec.z;
}

//-----------------------------------------------------------------------

bool iLuxProp::Ga4OutsideCollides(int alIdx)
{
	iPhysicsWorld *pPhysicsWorld = mpWorld->GetPhysicsWorld();
	iPhysicsBody *pPlayerBody = gpBase->mpPlayer->GetCharacterBody()->GetCurrentBody();
	iPhysicsBody *pBody = mvBodies[alIdx];

	cCollideData collideData;
	collideData.SetMaxSize(1);

	return pPhysicsWorld->CheckShapeCollision(	pBody->GetShape(),pBody->GetLocalMatrix(),
												pPlayerBody->GetShape(), pPlayerBody->GetLocalMatrix(),
												collideData,1,false);
}

//-----------------------------------------------------------------------

void iLuxProp::Ga4ApplyOp(const OhplGa4Op &aOp)
{
	switch(aOp.code)
	{
	case kOhplGa4Op_FxPsCreateAll:
		for(size_t i=0; i<mvParticleSystems.size(); ++i)
		{
			cLuxProp_PSData *pPSData = &mvEffectPSData[i];
			cParticleSystem *pPS = mpWorld->CreateParticleSystem(pPSData->msName, pPSData->msDataName,1);
			if(pPS)
			{
				pPS->SetMatrix(pPSData->m_mtxLocalTransform);
				if(pPSData->mpParent) pPSData->mpParent->AddChild(pPS);
			}

			mvParticleSystems[i] = pPS;
		}
		break;
	case kOhplGa4Op_FxLightFadeOn:
		mvLights[aOp.i]->FadeTo(mvEffectLightData[aOp.i].mOnColor, mvEffectLightData[aOp.i].mfOnRadius, aOp.f[0]);
		break;
	case kOhplGa4Op_FxLightSetOn:
		mvLights[aOp.i]->SetDiffuseColor(mvEffectLightData[aOp.i].mOnColor);
		mvLights[aOp.i]->SetRadius(mvEffectLightData[aOp.i].mfOnRadius);
		mvLights[aOp.i]->SetFlickerActive(mvEffectLightData[aOp.i].mbFlickering);
		break;
	case kOhplGa4Op_FxSoundFadeIn:
		mvSoundEntities[aOp.i]->FadeIn(aOp.f[0]);
		break;
	case kOhplGa4Op_FxSoundPlay:
		mvSoundEntities[aOp.i]->Play(false);
		break;
	case kOhplGa4Op_FxPsKillAll:
		for(size_t i=0; i<mvParticleSystems.size(); ++i)
		{
			cParticleSystem *pPS = mvParticleSystems[i];
			mvParticleSystems[i] = NULL;

			if(aOp.j)	pPS->Kill();
			else		mpWorld->DestroyParticleSystem(pPS);
		}
		break;
	case kOhplGa4Op_FxLightOff:
		{
			iLight *pLight = mvLights[aOp.i];
			pLight->StopFading();
			pLight->SetFlickerActive(false);
			if(aOp.j)
			{
				pLight->FadeTo(mEffectsOffLightColor, aOp.f[0], aOp.f[1]);
			}
			else
			{
				pLight->SetDiffuseColor(mEffectsOffLightColor);
				pLight->SetRadius(aOp.f[0]);
			}
		}
		break;
	case kOhplGa4Op_FxSoundFadeOut:
		mvSoundEntities[aOp.i]->FadeOut(aOp.f[0]);
		break;
	case kOhplGa4Op_FxSoundStop:
		mvSoundEntities[aOp.i]->Stop(false);
		break;
	case kOhplGa4Op_FxChangeSound:
		{
			tString sSound = aOp.i ? msEffectsOnSound : msEffectsOffSound;

			if(sSound != "")
			{
				cSoundEntity *pSound = mpWorld->CreateSoundEntity(msName + "_lightlitchange",sSound, true);
				if(pSound)
				{
					pSound->SetPosition(GetWorldCenterPos());
				}
			}
		}
		break;
	case kOhplGa4Op_FxLightFlicker:
		mvLights[aOp.i]->SetFlickerActive(mvEffectLightData[aOp.i].mbFlickering);
		break;
	case kOhplGa4Op_FxBillboards:
		for(size_t i=0; i<mvBillboards.size(); ++i)
		{
			cBillboard *pBillboard = mvBillboards[i];
			if(mvEffectBillboardData[i].mbConnectedToLight) continue;

			pBillboard->SetColor(mvEffectBillboardData[i].mOnColor * aOp.f[0]);
		}
		break;
	case kOhplGa4Op_FxMeshIllum:
		mpMeshEntity->SetIlluminationAmount(aOp.f[0]);
		break;
	case kOhplGa4Op_Coverage:
		mpMeshEntity->SetCoverageAmount(aOp.f[0]);
		break;
	case kOhplGa4Op_SndMoveStart:
		PlaySound("MoveStart", msMoveStartSound,true, true);
		break;
	case kOhplGa4Op_SndMoveStop:
		PlaySound("MoveStop", msMoveStopSound,true, true);
		break;
	case kOhplGa4Op_MoveLoopStart:
		mpMoveLoopSound = PlaySound("MoveLoop",msMoveLoopSound,false, true);
		if(mpMoveLoopSound)
		{
			mlMoveLoopSoundID = mpMoveLoopSound->GetCreationID();
			mpMoveLoopSound->Play(false);

			cSoundEntry* pEntry = mpMoveLoopSound->GetSoundEntry(eSoundEntityType_Main, true);
			if(pEntry) pEntry->SetVolumeMul(0);
		}
		break;
	case kOhplGa4Op_MoveLoopVolume:
		UpdateMoveSoundVolume();
		break;
	case kOhplGa4Op_MoveLoopKill:
		mpMoveLoopSound->Stop(false);
		mpMap->GetWorld()->DestroySoundEntity(mpMoveLoopSound);

		mpMoveLoopSound = NULL;
		if(aOp.j) mlMoveLoopSoundID = -1;
		break;
	case kOhplGa4Op_StaticLinearMove:
		GetMainBody()->StaticLinearMove(cVector3f(aOp.v[0], aOp.v[1], aOp.v[2]));
		break;
	case kOhplGa4Op_StaticAngularMove:
		GetMainBody()->StaticAngularMove(cVector3f(aOp.v[0], aOp.v[1], aOp.v[2]));
		break;
	case kOhplGa4Op_OnStartMove:
		OnStartMove();
		break;
	case kOhplGa4Op_OutsideImpulse:
		mvBodies[aOp.i]->AddImpulse(cVector3f(aOp.v[0], aOp.v[1], aOp.v[2]));
		break;
	case kOhplGa4Op_OutsideCollideChar:
		mvBodies[aOp.i]->SetCollideCharacter(true);
		break;
	case kOhplGa4Op_ConnLimit:
		InteractConnectionLimit(aOp.i);
		break;
	case kOhplGa4Op_ConnTurn:
		InteractConnectionTurn(aOp.f[0], aOp.f[1], aOp.f[2], aOp.f[3]);
		break;
	case kOhplGa4Op_ConnState:
		ConnectionStateChange(aOp.i);
		break;
	case kOhplGa4Op_RunAnimCallback:
		mpMap->RunScript(msAnimCallback + "(\""+ msName + "\")");
		break;
	}
}

//-----------------------------------------------------------------------

cVector3f iLuxProp::GetWorldCenterPos()
{
	cVector3f vCenterPos = 0;
	if(mvBodies.empty()==false)
	{
		iPhysicsBody *pBody = mpMainBody ? mpMainBody : mvBodies[0];
		vCenterPos = pBody->GetWorldPosition();
	}
	else
	{
		vCenterPos = mpMeshEntity->GetBoundingVolume()->GetWorldCenter();
	}
	return vCenterPos;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxPropConnectedProp)
kSerializeVar(msName, eSerializeType_String)
kEndSerialize()


//-----------------------------------------------------------------------

kBeginSerializeVirtual(iLuxProp_SaveData, iLuxEntity_SaveData)

kSerializeVar(msFileName, eSerializeType_String)
kSerializeVar(m_mtxOnLoadTransform, eSerializeType_Matrixf)
kSerializeVar(mvOnLoadScale, eSerializeType_Vector3f)

kSerializeVar(mfHealth, eSerializeType_Float32)

kSerializeVar(mbCheckOutsidePlayer, eSerializeType_Bool)

kSerializeVar(mbEffectsActive, eSerializeType_Bool)
kSerializeVar(mfEffectsAlpha, eSerializeType_Float32)

kSerializeVar(mbStaticPhysics, eSerializeType_Bool)

kSerializeVar(mbMoving, eSerializeType_Bool)

kSerializeVar(mbMovingLinear, eSerializeType_Bool)
kSerializeVar(mfMoveLinearMaxSpeed, eSerializeType_Float32)
kSerializeVar(mfMoveLinearAcc, eSerializeType_Float32)
kSerializeVar(mfMoveLinearSpeed, eSerializeType_Float32)
kSerializeVar(mfMoveLinearSlowdownDist, eSerializeType_Float32)
kSerializeVar(mvMoveLinearGoal, eSerializeType_Vector3f)

kSerializeVar(mbMovingAngular, eSerializeType_Bool)
kSerializeVar(mfMoveAngularMaxSpeed, eSerializeType_Float32)
kSerializeVar(mfMoveAngularAcc, eSerializeType_Float32)
kSerializeVar(mfMoveAngularSpeed, eSerializeType_Float32)
kSerializeVar(mfMoveAngularSlowdownDist, eSerializeType_Float32)
kSerializeVar(m_mtxMoveAngularGoal, eSerializeType_Matrixf)
kSerializeVar(mbMoveAngularUseOffset, eSerializeType_Bool)
kSerializeVar(mvMoveAngularWorldOffset, eSerializeType_Vector3f)
kSerializeVar(mvMoveAngularLocalOffset, eSerializeType_Vector3f)

kSerializeVar(mbMoveAngularNoGoal, eSerializeType_Bool)
kSerializeVar(mvMoveAngularNoGoalDir, eSerializeType_Vector3f)

kSerializeVar(mlCurrentNonLoopAnimIndex, eSerializeType_Int32)
kSerializeVar(msAnimCallback, eSerializeType_String)

kSerializeVar(msParentEntityName, eSerializeType_String)
kSerializeVar(msParentBoneName, eSerializeType_String)
kSerializeVar(mpParentBoneOffsetMatrix, eSerializeType_Matrixf)

kSerializeVar(mfBodyMassBackup, eSerializeType_Float32)
kSerializeVar(mbBodyCollideBackup, eSerializeType_Bool)
kSerializeVar(mbBodyCollideCharacterBackup, eSerializeType_Bool)
kSerializeVar(mbBodyActiveBackup, eSerializeType_Bool)
kSerializeVar(mBodyMatrixBackup, eSerializeType_Matrixf)

kSerializeClassContainer(mvAttachedProps, cLuxProp_AttachedProp, eSerializeType_Class)

kSerializeClassContainer(mvConnectedProps, cLuxPropConnectedProp, eSerializeType_Class)

kSerializeVar(mMeshEntity, eSerializeType_Class)
kSerializeClassContainer(mvBodies, cEngineBody_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvJoints, cEngineJoint_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvPS, cEnginePS_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvBeams, cEngineBeam_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvLights, cEngineLight_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvSounds, cEngineSound_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvBillboards, cEngineBillboard_SaveData, eSerializeType_Class)

kSerializeClassContainer(mvInteractConnections, iLuxInteractConnection_SaveData, eSerializeType_ClassPointer)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxProp_SaveData::~iLuxProp_SaveData()
{
	for(size_t i=0; i<mvInteractConnections.Size(); ++i)
	{
		iLuxInteractConnection_SaveData *pSaveConn = mvInteractConnections[i];
		hplDelete(pSaveConn);
	}
}

//-----------------------------------------------------------------------

cEnginePS_SaveData* iLuxProp_SaveData::GetParticleSystem(cParticleSystem* apPS)
{
	if(apPS==NULL) return NULL;

	for(size_t i=0; i<mvPS.Size();++i)
		if(mvPS[i].msName == apPS->GetName()) return &mvPS[i];

	return NULL;
}
cEngineSound_SaveData* iLuxProp_SaveData::GetSoundEntity(cSoundEntity* apSound)
{
	for(size_t i=0; i<mvSounds.Size();++i)
		if(mvSounds[i].msName == apSound->GetName()) return &mvSounds[i];

	return NULL;
}
cEngineJoint_SaveData* iLuxProp_SaveData::GetJoint(iPhysicsJoint* apJoint)
{
	for(size_t i=0; i<mvJoints.Size();++i)
		if(mvJoints[i].msName == apJoint->GetName()) return &mvJoints[i];

	return NULL;
}

//-----------------------------------------------------------------------

iLuxEntity* iLuxProp_SaveData::CreateEntity(cLuxMap *apMap)
{
	cWorld *pWorld = apMap->GetWorld();
	apMap->ResetLatestEntity();
	pWorld->CreateEntity(msName,m_mtxOnLoadTransform, msFileName, mlID, true, mvOnLoadScale);

	return apMap->GetLatestEntity();
}

//-----------------------------------------------------------------------

void iLuxProp::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::SaveToSaveData(apSaveData);
	iLuxProp_SaveData *pData = static_cast<iLuxProp_SaveData*>(apSaveData);

    cWorld *pWorld = mpWorld;
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

	///////////////////////
	//Properties
	kCopyToVar(pData, msFileName);
	kCopyToVar(pData, m_mtxOnLoadTransform);
	kCopyToVar(pData, mvOnLoadScale);

	kCopyToVar(pData, mfHealth);

	pData->mbCheckOutsidePlayer = mBase.check_outside_player;

	pData->mbEffectsActive = mBase.effects_active;
	pData->mfEffectsAlpha = mBase.effects_alpha;

	kCopyToVar(pData, mbStaticPhysics);

	pData->mbMoving = mBase.moving;

	pData->mbMovingLinear = mBase.moving_linear;
	pData->mfMoveLinearMaxSpeed = mBase.linear.max_speed;
	pData->mfMoveLinearAcc = mBase.linear.acc;
	pData->mfMoveLinearSpeed = mBase.linear.speed;
	pData->mfMoveLinearSlowdownDist = mBase.linear.slowdown;
	pData->mvMoveLinearGoal = cVector3f(mBase.linear.goal[0], mBase.linear.goal[1], mBase.linear.goal[2]);

	pData->mbMovingAngular = mBase.moving_angular;
	pData->mfMoveAngularMaxSpeed = mBase.angular.max_speed;
	pData->mfMoveAngularAcc = mBase.angular.acc;
	pData->mfMoveAngularSpeed = mBase.angular.speed;
	pData->mfMoveAngularSlowdownDist = mBase.angular.slowdown;
	memcpy(pData->m_mtxMoveAngularGoal.v, mBase.angular.goal, sizeof(mBase.angular.goal));

	
	pData->mbMoveAngularNoGoal = mBase.angular.no_goal;
	pData->mvMoveAngularNoGoalDir = cVector3f(mBase.angular.no_goal_dir[0], mBase.angular.no_goal_dir[1], mBase.angular.no_goal_dir[2]);

	pData->mbMoveAngularUseOffset = mBase.angular.use_offset;
	pData->mvMoveAngularWorldOffset = cVector3f(mBase.angular.world_offset[0], mBase.angular.world_offset[1], mBase.angular.world_offset[2]);
	pData->mvMoveAngularLocalOffset = cVector3f(mBase.angular.local_offset[0], mBase.angular.local_offset[1], mBase.angular.local_offset[2]);

	kCopyToVar(pData, mlCurrentNonLoopAnimIndex);
	kCopyToVar(pData, msAnimCallback);

    kCopyToVar(pData, mpParentBoneOffsetMatrix);
    kCopyToVar(pData, msParentEntityName);
	kCopyToVar(pData, msParentBoneName);

	kCopyToVar(pData, mfBodyMassBackup);
	kCopyToVar(pData, mbBodyCollideBackup);
	kCopyToVar(pData, mbBodyCollideCharacterBackup);
	kCopyToVar(pData, mbBodyActiveBackup);
	kCopyToVar(pData, mBodyMatrixBackup);

	///////////////////////
	//Attached props
    tLuxProp_AttachedPropListIt attachIt = mlstAttachedProps.begin();
	for(; attachIt != mlstAttachedProps.end(); ++attachIt)
	{
		cLuxProp_AttachedProp attachProp = *(*attachIt);
		attachProp.mpProp = NULL;
        pData->mvAttachedProps.Add(attachProp);
	}

	///////////////////////
	//Connection
	pData->mvConnectedProps.Resize(mvConnectedProps.size());
	for(size_t i=0; i<mvConnectedProps.size(); ++i)
		pData->mvConnectedProps[i].msName = mvConnectedProps[i];

	///////////////////////
	//Mesh Entity
	if(mpMeshEntity)
		pData->mMeshEntity.FromMeshEntity(mpMeshEntity);

	///////////////////////
	//Bodies
	pData->mvBodies.Resize(mvBodies.size());
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		pData->mvBodies[i].FromBody(mvBodies[i]);
	}

	///////////////////////
	//Joints
	pData->mvJoints.Resize(mvJoints.size());
	for(size_t i=0; i<mvJoints.size(); ++i)
	{
		pData->mvJoints[i].FromJoint(mvJoints[i], pPhysicsWorld);
	}

	///////////////////////
	//Particle Systems
	pData->mvPS.Resize(mvParticleSystems.size());
	for(size_t i=0; i<mvParticleSystems.size(); ++i)
	{
		if(pWorld->ParticleSystemExists(mvParticleSystems[i])==false)
		{
			mvParticleSystems[i] = NULL;
			Warning("particle system %d in %s does not exist anymore!\n",i,GetName().c_str());
		}

		pData->mvPS[i].FromPS(mvParticleSystems[i]);
	}
	
	///////////////////////
	//Lights
	pData->mvLights.Resize(mvLights.size());
	for(size_t i=0; i<mvLights.size(); ++i)
	{
		pData->mvLights[i].FromLight(mvLights[i]);
	}
	
	///////////////////////
	//Sounds
	pData->mvSounds.Resize(mvSoundEntities.size());
	for(size_t i=0; i<mvSoundEntities.size(); ++i)
	{
		pData->mvSounds[i].FromSound(mvSoundEntities[i]);
	}

	///////////////////////
	//Billboards
	pData->mvBillboards.Resize(mvBillboards.size());
	for(size_t i=0; i<mvBillboards.size(); ++i)
	{
		pData->mvBillboards[i].FromBillboard(mvBillboards[i]);
	}

	///////////////////////
	//Interact connections
	for(size_t i=0; i<mvInteractConnections.size(); ++i)
	{
		iLuxInteractConnection *pConn = mvInteractConnections[i];

        iLuxInteractConnection_SaveData *pSaveConn = pConn->CreateSaveData();
		pSaveConn->FromConnection(pConn);
		pData->mvInteractConnections.Add(pSaveConn);
	}
}

//-----------------------------------------------------------------------

void iLuxProp::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveData(apSaveData);
	iLuxProp_SaveData *pData = static_cast<iLuxProp_SaveData*>(apSaveData);

	cWorld *pWorld = mpWorld;
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

	///////////////////////
	//Properties
	kCopyFromVar(pData, m_mtxOnLoadTransform);
	kCopyFromVar(pData, mvOnLoadScale);
	
	kCopyFromVar(pData, mfHealth);

	mBase.check_outside_player = pData->mbCheckOutsidePlayer;

	SetEffectsActive(pData->mbEffectsActive, false);
	mBase.effects_alpha = pData->mfEffectsAlpha;

	SetStaticPhysics(pData->mbStaticPhysics);

	mBase.moving = pData->mbMoving;
	mBase.moving_linear = pData->mbMovingLinear;
	mBase.linear.max_speed = pData->mfMoveLinearMaxSpeed;
	mBase.linear.acc = pData->mfMoveLinearAcc;
	mBase.linear.speed = pData->mfMoveLinearSpeed;
	mBase.linear.slowdown = pData->mfMoveLinearSlowdownDist;
	mBase.linear.goal[0] = pData->mvMoveLinearGoal.x; mBase.linear.goal[1] = pData->mvMoveLinearGoal.y; mBase.linear.goal[2] = pData->mvMoveLinearGoal.z;

	mBase.moving_angular = pData->mbMovingAngular;
	mBase.angular.max_speed = pData->mfMoveAngularMaxSpeed;
	mBase.angular.acc = pData->mfMoveAngularAcc;
	mBase.angular.speed = pData->mfMoveAngularSpeed;
	mBase.angular.slowdown = pData->mfMoveAngularSlowdownDist;
	memcpy(mBase.angular.goal, pData->m_mtxMoveAngularGoal.v, sizeof(mBase.angular.goal));
	
	mBase.angular.no_goal = pData->mbMoveAngularNoGoal;
	mBase.angular.no_goal_dir[0] = pData->mvMoveAngularNoGoalDir.x; mBase.angular.no_goal_dir[1] = pData->mvMoveAngularNoGoalDir.y; mBase.angular.no_goal_dir[2] = pData->mvMoveAngularNoGoalDir.z;
	
	mBase.angular.use_offset = pData->mbMoveAngularUseOffset;
	mBase.angular.world_offset[0] = pData->mvMoveAngularWorldOffset.x; mBase.angular.world_offset[1] = pData->mvMoveAngularWorldOffset.y; mBase.angular.world_offset[2] = pData->mvMoveAngularWorldOffset.z;
	mBase.angular.local_offset[0] = pData->mvMoveAngularLocalOffset.x; mBase.angular.local_offset[1] = pData->mvMoveAngularLocalOffset.y; mBase.angular.local_offset[2] = pData->mvMoveAngularLocalOffset.z;

	kCopyFromVar(pData, mlCurrentNonLoopAnimIndex);
	kCopyFromVar(pData, msAnimCallback);

    kCopyFromVar(pData, mpParentBoneOffsetMatrix);
    kCopyFromVar(pData, msParentEntityName);
	kCopyFromVar(pData, msParentBoneName);

	kCopyFromVar(pData, mfBodyMassBackup);
	kCopyFromVar(pData, mbBodyCollideBackup);
	kCopyFromVar(pData, mbBodyCollideCharacterBackup);
	kCopyFromVar(pData, mbBodyActiveBackup);
	kCopyFromVar(pData, mBodyMatrixBackup);

	if(mfBodyMassBackup == 0 || mfBodyMassBackup < 0 || (mfBodyMassBackup == mfBodyMassBackup) == false) mfBodyMassBackup = 1;
	
	///////////////////////
	//Connections
	mvConnectedProps.resize(pData->mvConnectedProps.Size());
	for(size_t i=0; i<mvConnectedProps.size(); ++i)
		mvConnectedProps[i] = pData->mvConnectedProps[i].msName;

	///////////////////////
	//Mesh Entity
	if(mpMeshEntity)
		pData->mMeshEntity.ToMeshEntity(mpMeshEntity);

	///////////////////////
	//Bodies
	for(size_t i=0; i<mvBodies.size(); ++i)
	{
		pData->mvBodies[i].ToBody(mvBodies[i]);
	}

	///////////////////////
	//Joints
	for(size_t i=0; i<mvJoints.size(); ++i)
	{
		if(pData->mvJoints[i].mbBroken)
		{
			pPhysicsWorld->DestroyJoint(mvJoints[i]);
			mvJoints[i] = NULL;
		}
		else
		{
			pData->mvJoints[i].ToJoint(mvJoints[i]);
		}
	}

	///////////////////////
	//Lights
	for(size_t i=0; i<mvLights.size(); ++i)
	{
		pData->mvLights[i].ToLight(mvLights[i]);
	}
	
	///////////////////////
	//Particle Systems
	int lCount=0;
	for(std::vector<cParticleSystem*>::iterator it = mvParticleSystems.begin(); it != mvParticleSystems.end();)
	{
		cParticleSystem *pPS = *it;

		cEnginePS_SaveData *pSavePS = pData->GetParticleSystem(pPS);
		if(pSavePS)
		{
			pSavePS->ToPS(pPS);
			++it;
		}
		else
		{
			//check if a null was previously saved
			if(pData->mvPS.Size() == mvParticleSystems.size() && pData->mvPS[lCount].msType == "")
			{
				++it;
			}
			//a particle system has been removed.
			else
			{
				pWorld->DestroyParticleSystem(pPS);
				it = mvParticleSystems.erase(it);
			}
		}

		++lCount;
	}

	///////////////////////
	//Sounds
	for(std::vector<cSoundEntity*>::iterator it = mvSoundEntities.begin(); it != mvSoundEntities.end(); )
	{
		cSoundEntity *pSound = *it;
		cEngineSound_SaveData *pSaveSound = pData->GetSoundEntity(pSound);
		if(pSaveSound)
		{
			pSaveSound->ToSound(pSound);
			++it;
		}
		else
		{
			pWorld->DestroySoundEntity(pSound);
			it = mvSoundEntities.erase(it);
		}
	}

	///////////////////////
	//Billboards
	for(size_t i=0; i<mvBillboards.size(); ++i)
	{
		pData->mvBillboards[i].ToBillboard(mvBillboards[i]);
	}
}

//-----------------------------------------------------------------------

void iLuxProp::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
	iLuxProp_SaveData *pData = static_cast<iLuxProp_SaveData*>(apSaveData);

	///////////////////////
	//Attached props (want it in setup so it does not mess with set)
	for(size_t i=0; i<pData->mvAttachedProps.Size(); ++i)
	{
		cLuxProp_AttachedProp &attachProp = pData->mvAttachedProps[i];

		AddAndAttachProp(attachProp.msName, attachProp.msFileName, attachProp.m_mtxOffset);
	}

	///////////////////////
	//Interact connections
	for(size_t i=0; i<pData->mvInteractConnections.Size(); ++i)
	{
		iLuxInteractConnection_SaveData *pSaveConn = pData->mvInteractConnections[i];
		iLuxInteractConnection *pConn = pSaveConn->CreateConnection(mpMap);

		if(pConn) mvInteractConnections.push_back(pConn);
	}
	

	

	///////////////////
	// Bone attachment
	if(msParentBoneName != "" && msParentEntityName != "")
	{
		iLuxEntity * pParentEntity = mpMap->GetEntityByName(msParentEntityName);

		if(pParentEntity)
		{	
			cMeshEntity * pParentMeshEntity = pParentEntity->GetMeshEntity();

			if ( pParentMeshEntity )
			{
				mpParentBone = pParentMeshEntity->GetBoneStateFromName( msParentBoneName );
			}
		}
	}
}

//-----------------------------------------------------------------------

