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

#include "physics/SurfaceData.h"

#include "physics/Physics.h"
#include "physics/RustPhysics.h"
#include "physics/RustPhysBook.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
#include "system/LowLevelSystem.h"
#include "math/Math.h"

#include "scene/ParticleSystem.h"

#include "scene/World.h"
#include "scene/SoundEntity.h"
#include "sound/Sound.h"
#include "sound/SoundChannel.h"
#include "sound/SoundHandler.h"

#include "resources/Resources.h"
#include "resources/SoundEntityManager.h"
#include "resources/ParticleManager.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	size_t cSurfaceData::HostOp(int32_t alOp, OhplH15Wa *a)
	{
		cSurfaceData *pS = reinterpret_cast<cSurfaceData*>(a->p[0]);
		tString sArg;
		if(alOp == 206 || alOp == 207 || alOp == 210 || alOp == 211) sArg = tString(reinterpret_cast<const char*>(a->p[alOp >= 210 ? 2 : 1]), a->p[alOp >= 210 ? 3 : 2]);
		cSurfaceImpactData *pI = reinterpret_cast<cSurfaceImpactData*>(a->p[1]);
		const tString *ps = NULL;
		switch(alOp)
		{
		case 201: hplDelete(pI); return 0;
		case 202: { cSurfaceImpactData *pNew = hplNew( cSurfaceImpactData, () ); pNew->mfMinSpeed = a->f[0]; return reinterpret_cast<size_t>(pNew); }
		case 203: pI->msSoundName = tString(reinterpret_cast<const char*>(a->p[2]), a->p[3]); return 0;
		case 204: pI->msPSName = tString(reinterpret_cast<const char*>(a->p[2]), a->p[3]); return 0;
		case 205: pI->mlPSPrio = a->i[0]; return 0;
		case 206: pS->mpResources->GetSoundEntityManager()->Preload(sArg); return 0;
		case 207: pS->mpResources->GetParticleManager()->Preload(sArg); return 0;
		case 208: ps = a->i[0] == 0 ? &pS->msUserVar : a->i[0] == 1 ? &pS->msStepType : a->i[0] == 2 ? &pS->msScrapeSoundName : &pS->msRollSoundName; break;
		case 209: ps = a->i[0] == 0 ? &pI->msSoundName : &pI->msPSName; break;
		case 210: return reinterpret_cast<size_t>(reinterpret_cast<iPhysicsWorld*>(a->p[1])->GetMaterialFromName(sArg));
		case 211: return reinterpret_cast<size_t>(reinterpret_cast<iPhysicsWorld*>(a->p[1])->CreateMaterial(sArg));
		case 212:
		{
			iPhysicsMaterial *pM = reinterpret_cast<iPhysicsMaterial*>(a->p[1]);
			if(a->i[0] == 0) pM->SetElasticity(a->f[0]); else if(a->i[0] == 1) pM->SetKineticFriction(a->f[0]); else pM->SetStaticFriction(a->f[0]);
			return 0;
		}
		case 213:
		{
			iPhysicsMaterial *pM = reinterpret_cast<iPhysicsMaterial*>(a->p[1]);
			ePhysicsMaterialCombMode mode = (ePhysicsMaterialCombMode)(a->i[0] >> 8);
			if((a->i[0] & 0xff) == 0) pM->SetElasticityCombMode(mode); else pM->SetFrictionCombMode(mode);
			return 0;
		}
		case 214: reinterpret_cast<iPhysicsMaterial*>(a->p[1])->SetSurfaceData(pS); return 0;
		}
		if(ps){ a->s = ps->c_str(); a->n = ps->size(); }
		return 0;
	}

	cSurfaceData::cSurfaceData(const tString &asName, cPhysics *apPhysics,cResources *apResources)
	{
		msName = asName;
		mpPhysics = apPhysics;
		mpResources = apResources;

		ohpl_h15s_new(HostOp, reinterpret_cast<size_t>(this), &mS);
	}

	//-----------------------------------------------------------------------

	cSurfaceData::~cSurfaceData()
	{
		ohpl_h15s_free(mS.rs);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cSurfaceData::RustParams(OhplSurfaceParams &aParams) const
	{
		ohpl_h15s_params(&mS, reinterpret_cast<size_t>(this), reinterpret_cast<size_t>(mpPhysics), OhplS(msScrapeSoundName), OhplS(msRollSoundName), &aParams);
	}

	void cSurfaceData::OnImpact(float afSpeed,const cVector3f &avPos,int alContacts, iPhysicsBody *apBody)
	{
		OhplSurfaceParams params;
		RustParams(params);
		ohpl_phys_surface_on_impact(OhplPhysHost(), &params, afSpeed, &avPos, reinterpret_cast<size_t>(apBody));
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::OnSlide(float afSpeed,const cVector3f &avPos,int alContacts, iPhysicsBody *apBody,
								iPhysicsBody *apSlideAgainstBody)
	{
		OhplSurfaceParams params;
		RustParams(params);
		ohpl_phys_surface_on_slide(OhplPhysHost(), &params, afSpeed, &avPos, alContacts,
									reinterpret_cast<size_t>(apBody), reinterpret_cast<size_t>(apSlideAgainstBody));
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::CreateImpactEffect(	float afSpeed,const cVector3f &avPos,int alContacts,
											cSurfaceData *apSecondSurface, iPhysicsWorld *apPhysicsWorld)
	{
		OhplSurfaceParams params;
		RustParams(params);
		ohpl_phys_surface_create_impact_effect(OhplPhysHost(), &params, afSpeed, &avPos,
												reinterpret_cast<size_t>(apSecondSurface), reinterpret_cast<size_t>(apPhysicsWorld));
	}


	//-----------------------------------------------------------------------

	void cSurfaceData::UpdateRollEffect(iPhysicsBody *apBody)
	{
		OhplSurfaceParams params;
		RustParams(params);
		ohpl_phys_surface_update_roll_effect(OhplPhysHost(), &params, reinterpret_cast<size_t>(apBody));
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetElasticity(float afElasticity)
	{
		ohpl_h15s_set_f32(&mS, 0, afElasticity);
	}

	float cSurfaceData::GetElasticity() const
	{
		return ohpl_h15s_get_f32(const_cast<OhplSurfaceState*>(&mS), 0);
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetStaticFriction(float afElasticity)
	{
		ohpl_h15s_set_f32(&mS, 1, afElasticity);
	}

	float cSurfaceData::GetStaticFriction() const
	{
		return ohpl_h15s_get_f32(const_cast<OhplSurfaceState*>(&mS), 1);
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetKineticFriction(float afElasticity)
	{
		ohpl_h15s_set_f32(&mS, 2, afElasticity);
	}

	float cSurfaceData::GetKineticFriction() const
	{
		return ohpl_h15s_get_f32(const_cast<OhplSurfaceState*>(&mS), 2);
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetPriority(int alPriority)
	{
		ohpl_h15s_set_i32(&mS, 0, alPriority);
	}

	int cSurfaceData::GetPriority() const
	{
		return ohpl_h15s_get_i32(&mS, 0);
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetFrictionCombMode(ePhysicsMaterialCombMode aMode)
	{
		ohpl_h15s_set_i32(&mS, 1, aMode);
	}

	ePhysicsMaterialCombMode cSurfaceData::GetFrictionCombMode() const
	{
		return (ePhysicsMaterialCombMode)ohpl_h15s_get_i32(&mS, 1);
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetElasticityCombMode(ePhysicsMaterialCombMode aMode)
	{
		ohpl_h15s_set_i32(&mS, 2, aMode);
	}

	//-----------------------------------------------------------------------

	ePhysicsMaterialCombMode cSurfaceData::GetElasticityCombMode() const
	{
		return (ePhysicsMaterialCombMode)ohpl_h15s_get_i32(&mS, 2);
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::PreloadData()
	{
		ohpl_h15s_preload(mS.rs);
	}

	//-----------------------------------------------------------------------

	iPhysicsMaterial* cSurfaceData::ToMaterial(iPhysicsWorld *apWorld)
	{
		return reinterpret_cast<iPhysicsMaterial*>(ohpl_h15s_to_material(mS.rs, &mS, msName.c_str(), msName.size(), reinterpret_cast<size_t>(apWorld)));
	}

	//-----------------------------------------------------------------------

	cSurfaceImpactData* cSurfaceData::CreateImpactData(float afMinSpeed)
	{
		return reinterpret_cast<cSurfaceImpactData*>(ohpl_h15s_create_impact(mS.rs, afMinSpeed, false));
	}

	cSurfaceImpactData* cSurfaceData::GetImpactData(int alIdx)
	{
		return reinterpret_cast<cSurfaceImpactData*>(ohpl_h15s_impact_at(mS.rs, false, alIdx));
	}

	int cSurfaceData::GetImpactDataNum()
	{
		return ohpl_h15s_impact_num(mS.rs, false);
	}

	cSurfaceImpactData* cSurfaceData::GetImpactDataFromSpeed(float afSpeed)
	{
		return reinterpret_cast<cSurfaceImpactData*>(ohpl_h15s_impact_from_speed(mS.rs, false, afSpeed));
	}

	//-----------------------------------------------------------------------

	cSurfaceImpactData* cSurfaceData::CreateHitData(float afMinSpeed)
	{
		return reinterpret_cast<cSurfaceImpactData*>(ohpl_h15s_create_impact(mS.rs, afMinSpeed, true));
	}

	cSurfaceImpactData* cSurfaceData::GetHitData(int alIdx)
	{
		return reinterpret_cast<cSurfaceImpactData*>(ohpl_h15s_impact_at(mS.rs, true, alIdx));
	}

	int cSurfaceData::GetHitDataNum()
	{
		return ohpl_h15s_impact_num(mS.rs, true);
	}

	cSurfaceImpactData* cSurfaceData::GetHitDataFromSpeed(float afSpeed)
	{
		return reinterpret_cast<cSurfaceImpactData*>(ohpl_h15s_impact_from_speed(mS.rs, true, afSpeed));
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
}
