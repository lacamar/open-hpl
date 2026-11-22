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

void OhplA5PropDump(iLuxProp *apProp);
void OhplA5OnLoad();
void OhplA5Freeze(iLuxProp *apProp);
void OhplA5Seed(const std::string &asName);

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


void iLuxPropLoader::AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	cLuxMap *pMap = gpBase->mpCurrentMapLoading;
	if(pMap==NULL)	return;

	iLuxProp *pProp = CreateProp(mpEntity->GetName(), mlID,pMap);

	//////////////////////////////
	// Set data
	pProp->mpWorld = apWorld;
	pProp->Str(kOhplA5eStr_FileName) = msFileName;
	pProp->OnLoadTransformRef() = a_mtxTransform;
	pProp->OnLoadScaleRef() = mvScale;

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
		ohpl_a5e_load_base(pProp->mhState, vars.Vars(), &bp);
	}
	pProp->SetMaxFocusDistance(bp.max_focus_distance);
	if(mpEntity) mpEntity->SetVisible(pProp->S().show_mesh);

	gpBase->PreloadSound(tString(pProp->Str(kOhplA5eStr_MoveStartSound)));
	gpBase->PreloadSound(tString(pProp->Str(kOhplA5eStr_MoveStopSound)));
	gpBase->PreloadSound(tString(pProp->Str(kOhplA5eStr_MoveLoopSound)));

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
	if(pProp->GetMaxFocusDistance()<=0) pProp->SetMaxFocusDistance(mfDefaultMaxFocusDistance);

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
			tString sCallback = pProp->GetCallbackFunc(), sConnChange = pProp->GetConnectionStateChangeCallback();
			inst.Slots(kOhplGa4Slot_ConnectedProp, pProp->mvConnectedProps)
				.Slot(kOhplGa4Slot_CallbackFunc, sCallback)
				.Slot(kOhplGa4Slot_ConnChange, sConnChange);
			tString sLookAt, sInteract;
			inst.Slot(kOhplGa4Slot_LookAt, sLookAt).Slot(kOhplGa4Slot_Interact, sInteract);
			ohpl_ga4_base_load_instance(inst.Vars(), mbForceFullGameSave, &bi);
			pProp->SetCallbackFunc(sCallback);
			pProp->SetConnectionStateChangeCallback(sConnChange);

			if(pProp->mpMeshEntity)
			{
				pProp->mpMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, bi.cast_shadows);
			}

			pProp->SetFullGameSave(bi.full_game_save);

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
	OhplA5Seed(asName);
	mPropType = aPropType;

	mhState = ohpl_a5e_new(&mpState);

	mpAttachmentParent = NULL;

	SetInteractionDisabled(false);

	mpMainBody = NULL;

	mpMoveLoopSound = NULL;

    mpParentBone = NULL;
}

//-----------------------------------------------------------------------

iLuxProp::~iLuxProp()
{
	cWorld *pWorld = mpWorld;
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

	////////////////////
	// Attachment
	if(mpAttachmentParent && GetMap() && GetMap()->IsDeletingAllWorldEntities()==false)
	{
		mpAttachmentParent->RemoveAttachedProp(this);
		mpAttachmentParent = NULL;
	}
	for(tLuxProp_AttachedPropListIt it = mlstAttachedProps.begin(); it != mlstAttachedProps.end(); ++it)
	{
		//Map is currently deleting all 
		if(GetMap()->IsDeletingAllWorldEntities())
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
	if(mpMoveLoopSound && pWorld->SoundEntityExists(mpMoveLoopSound,S().move_loop_sound_id))
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

	ohpl_a5e_free(mhState);
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
	if(Str(kOhplA5eStr_MainBodyName) != "")
	{
		mpMainBody = (iPhysicsBody*)STLFindByName(mvBodies,GetName() + "_"+ Str(kOhplA5eStr_MainBodyName));
        if(mpMainBody == NULL)
		{
			Warning("Could not find main physics body '%s'\n", GetName().c_str());
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

	OhplA5Freeze(this);
}

//-----------------------------------------------------------------------

void iLuxProp::SetParentBone(cBoneState * apParentBone, const cMatrixf & apParentBoneOffsetMatrix, tString asParentName, tString asParentBoneName)
{
    mpParentBone = apParentBone;
    ParentBoneOffsetMatrix() = apParentBoneOffsetMatrix;

	Str(kOhplA5eStr_ParentEntityName) = asParentName;
	Str(kOhplA5eStr_ParentBoneName) = asParentBoneName;

    if ( mvBodies.size() > 0 )
    {
        iPhysicsBody *pBody = mvBodies[0];

        S().body_mass_backup = pBody->GetMass();
        S().body_collide_backup = pBody->GetCollide();
        S().body_collide_character_backup = pBody->GetCollideCharacter();
        S().body_active_backup = pBody->IsActive();
        BodyMatrixBackup() = pBody->GetLocalMatrix();

		pBody->SetMass(0);
		pBody->SetCollide(false);
		pBody->SetCollideCharacter(false);
		pBody->SetActive(false);
		pBody->SetMatrix(cMath::MatrixMul(mpParentBone->GetWorldMatrix(),ParentBoneOffsetMatrix()));
    }
}

//-----------------------------------------------------------------------

void iLuxProp::DetachFromParentBone()
{
    if ( mpParentBone != NULL && mvBodies.size() > 0 )
    {
        mpParentBone = NULL;

        iPhysicsBody *pBody = mvBodies[0];
        pBody->SetMass(S().body_mass_backup);
		pBody->SetCollide(S().body_collide_backup);
		pBody->SetCollideCharacter(S().body_collide_character_backup);
		pBody->SetActive(S().body_active_backup);
		//pBody->SetMatrix(BodyMatrixBackup());
    }

	Str(kOhplA5eStr_ParentBoneName) = "";
	Str(kOhplA5eStr_ParentEntityName) = "";
}

//-----------------------------------------------------------------------

void iLuxProp::UpdateParentBone(float afTimeStep)
{
    if ( mpParentBone && mvBodies.size() > 0)
	{
        //this->GetMeshEntity()->SetWorldMatrix(cMath::MatrixMul(mpParentBone->GetWorldMatrix(),ParentBoneOffsetMatrix()));
        iPhysicsBody *pBody = mvBodies[0];

		pBody->SetMatrix(cMath::MatrixMul(mpParentBone->GetWorldMatrix(),ParentBoneOffsetMatrix()));
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

	OhplA5PropDump(this);

	//if(mvLights.size() > 0)
	//	Log("End Color: %s\n", mvLights[0]->GetDiffuseColor().ToString().c_str());
}

//-----------------------------------------------------------------------

void iLuxProp::BeforeEntityDestruction()
{
	if(S().dissolve_on_destruction && mpMeshEntity)
	{
		GetMap()->AddDissolveEntity(mpMeshEntity, S().dissolve_time);
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
	ohpl_a5e_effects_env(mhState, abActive, abFade, (int)mvParticleSystems.size(), (int)mvSoundEntities.size(), &env);
	return env;
}

void iLuxProp::SetEffectsActive(bool abActive, bool abFadeAndPlaySounds)
{
	OhplGa4Run run(this);
	ohpl_a5e_effects_set_active(mhState, abActive, abFadeAndPlaySounds, (int)mvParticleSystems.size(), (int)mvSoundEntities.size(), Ga4Lights(), Ga4LightNum(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void iLuxProp::GiveDamage(float afAmount, int alStrength)
{
	float fAmount;
	float fHealth = ohpl_a5e_give_damage(mhState, afAmount, alStrength, &fAmount);

	SetHealth(fHealth);

	OnDamage(fAmount, alStrength);
}

//-----------------------------------------------------------------------

void iLuxProp::SetDisableCollisionUntilOutSidePlayer(bool abX)
{
	Base().check_outside_player = abX;

	if(Base().check_outside_player)
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
	ohpl_ga4_move_linear_to(&Base(), avGoal.v, afAcc, afMaxSpeed, afSlowdownDist, abResetSpeed, run.Sink());
	run.Finish();
}

//-------------------------------------------------------------------

void iLuxProp::MoveAngularTo(const cMatrixf& a_mtxGoal, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed,
							 bool abUseOffset, const cVector3f &avWorldOffset, const cVector3f &avLocalOffset)
{
	OhplGa4Run run(this);
	ohpl_ga4_move_angular_to(&Base(), a_mtxGoal.v, afAcc, afMaxSpeed, afSlowdownDist, abResetSpeed, abUseOffset, avWorldOffset.v, avLocalOffset.v, run.Sink());
	run.Finish();
}

//-------------------------------------------------------------------

void iLuxProp::RotateAtSpeed(	float afAcc, float afGoalSpeed, const cVector3f& avAxis, bool abResetSpeed,
								 bool abUseOffset, const cVector3f &avWorldOffset, const cVector3f &avLocalOffset)
{
	OhplGa4Run run(this);
	ohpl_ga4_rotate_at_speed(&Base(), afAcc, afGoalSpeed, avAxis.v, abResetSpeed, abUseOffset, avWorldOffset.v, avLocalOffset.v, run.Sink());
	run.Finish();
}

//-------------------------------------------------------------------

void iLuxProp::StopMove()
{
	OhplGa4Run run(this);
	ohpl_ga4_stop_move(&Base(), run.Sink());
	run.Finish();
}

//-------------------------------------------------------------------

void iLuxProp::FadeInMeshEntity(float afTime)
{
	OhplGa4Run run(this);
	ohpl_ga4_fade_in_mesh(&Base(), afTime, mpMeshEntity!=NULL, run.Sink());
	run.Finish();
}

//-------------------------------------------------------------------

void iLuxProp::ResetProperties()
{
	///////////////////////////////////
	// Reset bodies
    for(size_t i=0; i<mvBodies.size(); ++i)
	{
		cMatrixf mtxBody = cMath::MatrixMul(OnLoadTransformRef(), mvBodyExtraData[i].m_mtxLocalTransform);
        iPhysicsBody *pBody = mvBodies[i];

		pBody->SetMatrix(mtxBody);
		pBody->SetLinearVelocity(0);
		pBody->SetAngularVelocity(0);
	}
	
	///////////////////////////////////
	// Reset variables
	ohpl_ga4_reset_move(&Base());
	mpMoveLoopSound = NULL;

	S().health = 100;

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
		Error("Could not find animation '%s' for entity '%s'\n", asName.c_str(), GetName().c_str());
		return;
	}

	mpMeshEntity->PlayFadeTo(lIdx, abLoop, afFadeTime);
	if(abLoop==false)	S().anim_index = lIdx;
	else				S().anim_index = -1;
	
	if(abLoop==false)	Str(kOhplA5eStr_AnimCallback) = asCallback;
	else				Str(kOhplA5eStr_AnimCallback) = "";
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
	S().health = afX;

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
	if(S().static_physics == abX) return;

	S().static_physics = abX;

    for(size_t i=0; i<mvBodies.size(); ++i)
	{
		iPhysicsBody *pBody = mvBodies[i];

		pBody->SetMass(S().static_physics ? 0 : mvBodyData[i].mfMass);
		pBody->SetLinearVelocity(0);
		pBody->SetAngularVelocity(0);
	}
}

//-------------------------------------------------------------------

void iLuxProp::AddAndAttachProp(const tString& asName, const tString& asFileName, const cMatrixf& a_mtxOffset)
{
	if(GetMainBody()==NULL)
	{
		Error("Could not attach prop %s to %s because it does not have a main body!\n", GetName().c_str(), asFileName.c_str());
		return;
	}

	/////////////////////////////////
	// Create the entity
	GetMap()->ResetLatestEntity();
	GetMap()->CreateEntity(asName, asFileName, cMatrixf::Identity,1);
    
	iLuxEntity *pEntity = GetMap()->GetLatestEntity();
	if(pEntity==NULL || pEntity->GetEntityType() != eLuxEntityType_Prop || pEntity->GetName() != asName)
	{
		Error("Could not create attach entity '%s' for prop '%s'\n", asFileName.c_str(), GetName().c_str());
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
    GetMap()->DestroyEntity(apAttachedProp->mpProp);
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
        
		if(pConnection->GetInteractionOnly() && S().interacted_with==false) continue;

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

		if(pConnection->GetInteractionOnly() && S().interacted_with==false) continue;

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
	tString sFinalName = GetName() + '_'+asName;
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
	if(mpMeshEntity && S().show_mesh)
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
		if(i==0 && pLight->GetFlickerActive()) S().has_flickering = true;

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
		if(pLight->HasBillboard(apBB)) return true;
	}
	return false;
}

//-----------------------------------------------------------------------

void iLuxProp::UpdateEffectFading(float afTimeStep)
{
	OhplGa4Run run(this);
	ohpl_a5e_effects_update(mhState, afTimeStep, Ga4Lights(), Ga4LightNum(), mpMeshEntity!=NULL, run.Sink());
	run.Finish();
}


//-----------------------------------------------------------------------

void iLuxProp::UpdateMeshFading(float afTimeStep)
{
	OhplGa4Run run(this);
	ohpl_ga4_mesh_fading(&Base(), afTimeStep, mpMeshEntity!=NULL, run.Sink());
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
	if(S().anim_index>=0)
	{
		cAnimationState *pAnimState = mpMeshEntity->GetAnimationState(S().anim_index);
		bOver = pAnimState->IsOver();
	}

	OhplGa4Run run(this);
	ohpl_a5e_anim_update(mhState, bOver, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void iLuxProp::UpdateMoveSoundVolume()
{
	if(mpMoveLoopSound==NULL) return;

	cSoundEntry *pEntry = mpMoveLoopSound->GetSoundEntry(eSoundEntityType_Main, true);
	if(pEntry) pEntry->SetVolumeMul(Base().moving_volume);
}

//-----------------------------------------------------------------------

void iLuxProp::UpdateMoving(float afTimeStep)
{
	bool bLoopAlive = mpMoveLoopSound && GetMap()->GetWorld()->SoundEntityExists(mpMoveLoopSound, S().move_loop_sound_id);

	OhplGa4Run run(this);
	ohpl_a5e_update_moving(mhState, afTimeStep, Base().moving ? GetMainBody()->GetLocalMatrix().v : NULL, mpMoveLoopSound != NULL, bLoopAlive, run.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void iLuxProp::UpdateCheckIfOutsidePlayer(float afTimeStep)
{
	if(Base().check_outside_player==false) return;

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
		Base().check_outside_player = false;
	}
}

//-----------------------------------------------------------------------

void iLuxProp::Ga4MovePush(const float *apAdd, float *apOut)
{
	iPhysicsWorld *pPhysicsWorld = GetMap()->GetPhysicsWorld();
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
				pLight->FadeTo(EffectsOffLightColor(), aOp.f[0], aOp.f[1]);
			}
			else
			{
				pLight->SetDiffuseColor(EffectsOffLightColor());
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
			tString sSound = aOp.i ? Str(kOhplA5eStr_EffectsOnSound) : Str(kOhplA5eStr_EffectsOffSound);

			if(sSound != "")
			{
				cSoundEntity *pSound = mpWorld->CreateSoundEntity(GetName() + "_lightlitchange",sSound, true);
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
		PlaySound("MoveStart", Str(kOhplA5eStr_MoveStartSound),true, true);
		break;
	case kOhplGa4Op_SndMoveStop:
		PlaySound("MoveStop", Str(kOhplA5eStr_MoveStopSound),true, true);
		break;
	case kOhplGa4Op_MoveLoopStart:
		mpMoveLoopSound = PlaySound("MoveLoop",Str(kOhplA5eStr_MoveLoopSound),false, true);
		if(mpMoveLoopSound)
		{
			S().move_loop_sound_id = mpMoveLoopSound->GetCreationID();
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
		GetMap()->GetWorld()->DestroySoundEntity(mpMoveLoopSound);

		mpMoveLoopSound = NULL;
		if(aOp.j) S().move_loop_sound_id = -1;
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
		GetMap()->RunScript(Str(kOhplA5eStr_AnimCallback) + "(\""+ GetName() + "\")");
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

kSerializeRustClass(cLuxPropConnectedProp)


//-----------------------------------------------------------------------

kSerializeRustClassV(iLuxProp_SaveData, iLuxEntity_SaveData)

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
	pData->msFileName = Str(kOhplA5eStr_FileName);
	pData->m_mtxOnLoadTransform = OnLoadTransformRef();
	pData->mvOnLoadScale = OnLoadScaleRef();

	pData->mfHealth = S().health;

	pData->mbCheckOutsidePlayer = Base().check_outside_player;

	pData->mbEffectsActive = Base().effects_active;
	pData->mfEffectsAlpha = Base().effects_alpha;

	pData->mbStaticPhysics = S().static_physics;

	pData->mbMoving = Base().moving;

	pData->mbMovingLinear = Base().moving_linear;
	pData->mfMoveLinearMaxSpeed = Base().linear.max_speed;
	pData->mfMoveLinearAcc = Base().linear.acc;
	pData->mfMoveLinearSpeed = Base().linear.speed;
	pData->mfMoveLinearSlowdownDist = Base().linear.slowdown;
	pData->mvMoveLinearGoal = cVector3f(Base().linear.goal[0], Base().linear.goal[1], Base().linear.goal[2]);

	pData->mbMovingAngular = Base().moving_angular;
	pData->mfMoveAngularMaxSpeed = Base().angular.max_speed;
	pData->mfMoveAngularAcc = Base().angular.acc;
	pData->mfMoveAngularSpeed = Base().angular.speed;
	pData->mfMoveAngularSlowdownDist = Base().angular.slowdown;
	memcpy(pData->m_mtxMoveAngularGoal.v, Base().angular.goal, sizeof(Base().angular.goal));

	
	pData->mbMoveAngularNoGoal = Base().angular.no_goal;
	pData->mvMoveAngularNoGoalDir = cVector3f(Base().angular.no_goal_dir[0], Base().angular.no_goal_dir[1], Base().angular.no_goal_dir[2]);

	pData->mbMoveAngularUseOffset = Base().angular.use_offset;
	pData->mvMoveAngularWorldOffset = cVector3f(Base().angular.world_offset[0], Base().angular.world_offset[1], Base().angular.world_offset[2]);
	pData->mvMoveAngularLocalOffset = cVector3f(Base().angular.local_offset[0], Base().angular.local_offset[1], Base().angular.local_offset[2]);

	pData->mlCurrentNonLoopAnimIndex = S().anim_index;
	pData->msAnimCallback = Str(kOhplA5eStr_AnimCallback);

    pData->mpParentBoneOffsetMatrix = ParentBoneOffsetMatrix();
    pData->msParentEntityName = Str(kOhplA5eStr_ParentEntityName);
	pData->msParentBoneName = Str(kOhplA5eStr_ParentBoneName);

	pData->mfBodyMassBackup = S().body_mass_backup;
	pData->mbBodyCollideBackup = S().body_collide_backup;
	pData->mbBodyCollideCharacterBackup = S().body_collide_character_backup;
	pData->mbBodyActiveBackup = S().body_active_backup;
	pData->mBodyMatrixBackup = BodyMatrixBackup();

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
	OhplA5OnLoad();
	super_class::LoadFromSaveData(apSaveData);
	iLuxProp_SaveData *pData = static_cast<iLuxProp_SaveData*>(apSaveData);

	cWorld *pWorld = mpWorld;
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

	///////////////////////
	//Properties
	OnLoadTransformRef() = pData->m_mtxOnLoadTransform;
	OnLoadScaleRef() = pData->mvOnLoadScale;
	
	S().health = pData->mfHealth;

	Base().check_outside_player = pData->mbCheckOutsidePlayer;

	SetEffectsActive(pData->mbEffectsActive, false);
	Base().effects_alpha = pData->mfEffectsAlpha;

	SetStaticPhysics(pData->mbStaticPhysics);

	Base().moving = pData->mbMoving;
	Base().moving_linear = pData->mbMovingLinear;
	Base().linear.max_speed = pData->mfMoveLinearMaxSpeed;
	Base().linear.acc = pData->mfMoveLinearAcc;
	Base().linear.speed = pData->mfMoveLinearSpeed;
	Base().linear.slowdown = pData->mfMoveLinearSlowdownDist;
	Base().linear.goal[0] = pData->mvMoveLinearGoal.x; Base().linear.goal[1] = pData->mvMoveLinearGoal.y; Base().linear.goal[2] = pData->mvMoveLinearGoal.z;

	Base().moving_angular = pData->mbMovingAngular;
	Base().angular.max_speed = pData->mfMoveAngularMaxSpeed;
	Base().angular.acc = pData->mfMoveAngularAcc;
	Base().angular.speed = pData->mfMoveAngularSpeed;
	Base().angular.slowdown = pData->mfMoveAngularSlowdownDist;
	memcpy(Base().angular.goal, pData->m_mtxMoveAngularGoal.v, sizeof(Base().angular.goal));
	
	Base().angular.no_goal = pData->mbMoveAngularNoGoal;
	Base().angular.no_goal_dir[0] = pData->mvMoveAngularNoGoalDir.x; Base().angular.no_goal_dir[1] = pData->mvMoveAngularNoGoalDir.y; Base().angular.no_goal_dir[2] = pData->mvMoveAngularNoGoalDir.z;
	
	Base().angular.use_offset = pData->mbMoveAngularUseOffset;
	Base().angular.world_offset[0] = pData->mvMoveAngularWorldOffset.x; Base().angular.world_offset[1] = pData->mvMoveAngularWorldOffset.y; Base().angular.world_offset[2] = pData->mvMoveAngularWorldOffset.z;
	Base().angular.local_offset[0] = pData->mvMoveAngularLocalOffset.x; Base().angular.local_offset[1] = pData->mvMoveAngularLocalOffset.y; Base().angular.local_offset[2] = pData->mvMoveAngularLocalOffset.z;

	S().anim_index = pData->mlCurrentNonLoopAnimIndex;
	Str(kOhplA5eStr_AnimCallback) = pData->msAnimCallback;

    ParentBoneOffsetMatrix() = pData->mpParentBoneOffsetMatrix;
    Str(kOhplA5eStr_ParentEntityName) = pData->msParentEntityName;
	Str(kOhplA5eStr_ParentBoneName) = pData->msParentBoneName;

	S().body_mass_backup = pData->mfBodyMassBackup;
	S().body_collide_backup = pData->mbBodyCollideBackup;
	S().body_collide_character_backup = pData->mbBodyCollideCharacterBackup;
	S().body_active_backup = pData->mbBodyActiveBackup;
	BodyMatrixBackup() = pData->mBodyMatrixBackup;

	if(S().body_mass_backup == 0 || S().body_mass_backup < 0 || (S().body_mass_backup == S().body_mass_backup) == false) S().body_mass_backup = 1;
	
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

	OhplA5Freeze(this);
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
		iLuxInteractConnection *pConn = pSaveConn->CreateConnection(GetMap());

		if(pConn) mvInteractConnections.push_back(pConn);
	}
	

	

	///////////////////
	// Bone attachment
	if(Str(kOhplA5eStr_ParentBoneName) != "" && Str(kOhplA5eStr_ParentEntityName) != "")
	{
		iLuxEntity * pParentEntity = GetMap()->GetEntityByName(Str(kOhplA5eStr_ParentEntityName));

		if(pParentEntity)
		{	
			cMeshEntity * pParentMeshEntity = pParentEntity->GetMeshEntity();

			if ( pParentMeshEntity )
			{
				mpParentBone = pParentMeshEntity->GetBoneStateFromName( Str(kOhplA5eStr_ParentBoneName) );
			}
		}
	}
}

//-----------------------------------------------------------------------

