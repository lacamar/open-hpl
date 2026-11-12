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

#include "LuxHandObject_Melee.h"

#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxPlayerHands.h"
#include "LuxMapHelper.h"
#include "LuxHelpFuncs.h"

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxHandObject_Melee::cLuxHandObject_Melee(const tString& asName, cLuxPlayerHands *apHands) : iLuxHandObject(asName, apHands)
{
	mpCollideShape = NULL;
}

cLuxHandObject_Melee::~cLuxHandObject_Melee()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxHandObject_Melee::RenderSolid(cRendererCallbackFunctions* apFunctions)
{
	/*cCamera *pCam = gpBase->mpPlayer->GetCamera();

	cVector3f vRotation(pCam->GetPitch(), pCam->GetYaw(), pCam->GetRoll());
	cMatrixf mtxHands = cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ);
	mtxHands.SetTranslation(pCam->GetPosition());
	
	cBoundingVolume shapeBV =  mpCollideShape->GetBoundingVolume();
	shapeBV.SetTransform(cMath::MatrixMul(mtxHands, shapeBV.GetTransform()));
	
	apFunctions->GetLowLevelGfx()->DrawBoxMinMax(shapeBV.GetMin(), shapeBV.GetMax(), cColor(1,1));*/
}

//-----------------------------------------------------------------------

void cLuxHandObject_Melee::RenderTrans(cRendererCallbackFunctions* apFunctions)
{

}

//-----------------------------------------------------------------------

void cLuxHandObject_Melee::LoadImplementedVars(cXmlElement *apVarsElem)
{
	msHandsAnim_Swing = apVarsElem->GetAttributeString("HandsAnim_Swing", "");
	msHandsAnim_SwingCharge = apVarsElem->GetAttributeString("HandsAnim_SwingCharge", "");
	msHandsAnim_Charge = apVarsElem->GetAttributeString("HandsAnim_Charge", "");
	msSwingSound = apVarsElem->GetAttributeString("SwingSound", "");
	msSwingChargeSound = apVarsElem->GetAttributeString("SwingChargeSound", "");
	msChargeSound = apVarsElem->GetAttributeString("ChargeSound", "");
	msChargeDoneSound = apVarsElem->GetAttributeString("ChargeDoneSound", "");
	msHitSound = apVarsElem->GetAttributeString("HitSound", "");

	mvCollideShapeSize = apVarsElem->GetAttributeVector3f("ColliderSize", cVector3f(1));
	mvCollideShapeOffset = apVarsElem->GetAttributeVector3f("ColliderOffset", cVector3f(0));

	mfChargePlayerSpeedMul = apVarsElem->GetAttributeFloat("ChargePlayerSpeedMul", 1.0f);
	mfChargeTime = apVarsElem->GetAttributeFloat("ChargeTime", 2.0f);
	mfRelativeDamageTime = apVarsElem->GetAttributeFloat("RelativeDamageTime", 0.5f);

	mfAttackForce = apVarsElem->GetAttributeFloat("AttackForce", 0);
	mfMaxAttackForce = apVarsElem->GetAttributeFloat("MaxAttackForce", 0);

	mfAttackMinDamage = apVarsElem->GetAttributeFloat("AttackMinDamage", 0);
	mfAttackMaxDamage = apVarsElem->GetAttributeFloat("AttackMaxDamage", 0);
	mfAttackHitSpeed = apVarsElem->GetAttributeFloat("AttackHitSpeed", 0);

	mlAttackStrength = apVarsElem->GetAttributeInt("AttackHitSpeed", 0);
}

//-----------------------------------------------------------------------

void cLuxHandObject_Melee::ImplementedCreateEntity(cLuxMap *apMap)
{
	if(mpCollideShape) return;

	cMatrixf mtxOffset = cMath::MatrixTranslate(mvCollideShapeOffset);
	mpCollideShape = apMap->GetPhysicsWorld()->CreateBoxShape(mvCollideShapeSize, &mtxOffset);
}

//-----------------------------------------------------------------------

void cLuxHandObject_Melee::ImplementedDestroyEntity(cLuxMap *apMap)
{
	if(mpCollideShape==NULL) return;

	apMap->GetPhysicsWorld()->DestroyShape(mpCollideShape);
	mpCollideShape = NULL;
}

//-----------------------------------------------------------------------

void cLuxHandObject_Melee::ImplementedReset()
{
	mpCollideShape = NULL;

}

//-----------------------------------------------------------------------

void cLuxHandObject_Melee::Update(float afTimeStep)
{
	if(mpHands->mlHandObjectState ==1)
	{
		UpdateCharge(afTimeStep);
	}
	else if(mpHands->mlHandObjectState ==2)
	{
		UpdateCheckDamageEvent(afTimeStep);
	}
}

//-----------------------------------------------------------------------

bool cLuxHandObject_Melee::DoAction(eLuxPlayerAction aAction, bool abPressed)
{
	if(aAction == eLuxPlayerAction_Attack)
	{
		OhplGa4bHands hands = GetHands();
		OhplGa4bMeleeAct act;
		ohpl_ga4b_hand_melee_do_action((int)mpHands->GetState(), abPressed, &hands, &act);
		SetHands(hands);
		if(act.charge) Charge();
		if(act.swing) Swing();
		return act.ret;
	}

	return false;
}

OhplGa4bHands cLuxHandObject_Melee::GetHands()
{
	OhplGa4bHands hands;
	hands.obj_state = mpHands->mlHandObjectState;
	hands.attack_down = mpHands->mbHandObjectAttackDown;
	hands.charge_count = mpHands->mfHandObjectChargeCount;
	return hands;
}

void cLuxHandObject_Melee::SetHands(const OhplGa4bHands &aHands)
{
	mpHands->mlHandObjectState = aHands.obj_state;
	mpHands->mbHandObjectAttackDown = aHands.attack_down;
	mpHands->mfHandObjectChargeCount = aHands.charge_count;
}

//-----------------------------------------------------------------------

bool cLuxHandObject_Melee::AnimationIsOver()
{
	OhplGa4bHands hands = GetHands();
	OhplGa4bMeleeAct act;
	ohpl_ga4b_hand_melee_anim_over(&hands, &act);
	SetHands(hands);
	if(act.swing) Swing();
	if(act.charge_speed) gpBase->mpPlayer->SetInteractionMoveSpeedMul(mfChargePlayerSpeedMul);
	return act.ret;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxHandObject_Melee::UpdateCharge(float afTimeStep)
{
	OhplGa4bHands hands = GetHands();
	bool bDone = ohpl_ga4b_hand_melee_update_charge(&hands, mfChargeTime, afTimeStep);
	SetHands(hands);
	if(bDone)
	{
		gpBase->mpPlayer->FadeAspectMulTo(0.9f, 4);
		gpBase->mpPlayer->FadeFOVMulTo(0.95f, 2);
		gpBase->mpHelpFuncs->PlayGuiSoundData(msChargeDoneSound,eSoundEntryType_World);
	}
}

//-----------------------------------------------------------------------

void cLuxHandObject_Melee::UpdateCheckDamageEvent(float afTimeStep)
{
	if(mpHands->CheckAnimationEvent(mfRelativeDamageTime))
	{
		cCamera *pCam = gpBase->mpPlayer->GetCamera();

		cVector3f vRotation(pCam->GetPitch(), pCam->GetYaw(), pCam->GetRoll());
		cMatrixf mtxHands = cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ);
		mtxHands.SetTranslation(pCam->GetPosition());

		OhplGa4bHands hands = GetHands();
		float fDamageMul = ohpl_ga4b_hand_melee_damage_mul(&hands);

		bool bHit = gpBase->mpMapHelper->ShapeDamage(	mpCollideShape, mtxHands, pCam->GetPosition(),
											mfAttackMinDamage*fDamageMul, mfAttackMaxDamage*fDamageMul,
											mfAttackForce, mfMaxAttackForce,
											mlAttackStrength, mfAttackHitSpeed,
											eLuxDamageType_BloodSplat, eLuxWeaponHitType_Sword,
											true,false, true, false);

		if(bHit)
		{
			gpBase->mpHelpFuncs->PlayGuiSoundData(msHitSound,eSoundEntryType_World);
		}
	}
}

//-----------------------------------------------------------------------

void cLuxHandObject_Melee::Swing()
{
	gpBase->mpPlayer->SetInteractionMoveSpeedMul(1.0f);
	gpBase->mpPlayer->FadeAspectMulTo(1.0f, 7.0f);
	gpBase->mpPlayer->FadeFOVMulTo(1.0f, 3.5f);

	OhplGa4bHands hands = GetHands();
	bool bCharged = ohpl_ga4b_hand_melee_swing(&hands);
	SetHands(hands);

	if(bCharged)
	{
		mpHands->PlayAnim(msHandsAnim_SwingCharge, false);
		gpBase->mpHelpFuncs->PlayGuiSoundData(msSwingChargeSound,eSoundEntryType_World);
	}
	else
	{
		mpHands->PlayAnim(msHandsAnim_Swing, false);
		gpBase->mpHelpFuncs->PlayGuiSoundData(msSwingSound,eSoundEntryType_World);
	}
}

//-----------------------------------------------------------------------

void cLuxHandObject_Melee::Charge()
{
	OhplGa4bHands hands = GetHands();
	ohpl_ga4b_hand_melee_charge(&hands);
	SetHands(hands);

	mpHands->PlayAnim(msHandsAnim_Charge, false);
	gpBase->mpHelpFuncs->PlayGuiSoundData(msChargeSound,eSoundEntryType_World);
}

//-----------------------------------------------------------------------

