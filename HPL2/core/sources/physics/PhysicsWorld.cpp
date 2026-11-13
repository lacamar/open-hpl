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

#include "physics/PhysicsWorld.h"

#include "physics/CollideShape.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsMaterial.h"
#include "physics/CharacterBody.h"
#include "physics/PhysicsJoint.h"
#include "physics/PhysicsController.h"
#include "physics/PhysicsRope.h"
#include "physics/SurfaceData.h"
#include "system/LowLevelSystem.h"
#include "system/System.h"
#include "math/Math.h"
#include "graphics/LowLevelGraphics.h"
#include "scene/World.h"
#include "system/Platform.h"
#include "scene/SoundEntity.h"
#include "physics/RustPhysics.h"
#include "physics/RustPhysBook.h"
#include "math/BoundingVolume.h"
#include "physics/CollideData.h"

namespace hpl {

	namespace {
		template<class T> T* P(size_t a){ return reinterpret_cast<T*>(a); }

		// Host side of the Rust world registry (rust/src/physics/wreg.rs).
		size_t WorldOp(int32_t alOp, OhplH15Wa *a)
		{
			iPhysicsWorld *pW = P<iPhysicsWorld>(a->p[0]);
			switch(alOp)
			{
			case 1:
			{
				const tString *ps = NULL;
				switch(a->p[0])
				{
				case OhplKind_Body: ps = &P<iPhysicsBody>(a->p[1])->GetName(); break;
				case OhplKind_Char: ps = &P<iCharacterBody>(a->p[1])->GetName(); break;
				case OhplKind_Joint: ps = &P<iPhysicsJoint>(a->p[1])->GetName(); break;
				case OhplKind_Ctrl: ps = &P<iPhysicsController>(a->p[1])->GetName(); break;
				case OhplKind_Rope: ps = &P<iPhysicsRope>(a->p[1])->GetName(); break;
				}
				if(ps){ a->s = ps->c_str(); a->n = ps->size(); }
				return 0;
			}
			case 2:
				switch(a->p[0])
				{
				case OhplKind_Shape: hplDelete(P<iCollideShape>(a->p[1])); break;
				case OhplKind_Body: hplDelete(P<iPhysicsBody>(a->p[1])); break;
				case OhplKind_Char: hplDelete(P<iCharacterBody>(a->p[1])); break;
				case OhplKind_Joint: hplDelete(P<iPhysicsJoint>(a->p[1])); break;
				case OhplKind_Ctrl: hplDelete(P<iPhysicsController>(a->p[1])); break;
				case OhplKind_Rope: hplDelete(P<iPhysicsRope>(a->p[1])); break;
				case OhplKind_Material: hplDelete(P<iPhysicsMaterial>(a->p[1])); break;
				}
				return 0;
			case 3: { iPhysicsBody *pB = P<iPhysicsBody>(a->p[0]); pB->Destroy(); hplDelete(pB); return 0; }
			case 4: return P<iPhysicsBody>(a->p[0])->IsInUpdateList();
			case 5: P<iPhysicsBody>(a->p[0])->SetInUpdateList(a->i[0] != 0); return 0;
			case 6: return P<iPhysicsBody>(a->p[0])->UpdateBeforeSimulate(a->f[0]);
			case 7: P<iPhysicsBody>(a->p[0])->UpdateAfterSimulate(a->f[0]); return 0;
			case 8: pW->GetContactPoints()->clear(); return 0;
			case 9: P<iPhysicsController>(a->p[0])->Update(a->f[0]); return 0;
			case 10: return P<iPhysicsRope>(a->p[0])->GetActive();
			case 11: P<iPhysicsRope>(a->p[0])->UpdateBeforeSimulate(a->f[0]); return 0;
			case 12: P<iPhysicsRope>(a->p[0])->UpdateAfterSimulate(a->f[0]); return 0;
			case 13: P<iCharacterBody>(a->p[0])->Update(a->f[0]); return 0;
			case 14: pW->Simulate(a->f[0]); return 0;
			case 15: return P<iPhysicsJoint>(a->p[0])->OnPhysicsUpdate();
			case 16: return P<iPhysicsJoint>(a->p[0])->CheckBreakage();
			case 17: P<iCollideShape>(a->p[0])->DecUserCount(); return 0;
			case 18: return P<iCollideShape>(a->p[0])->HasUsers();
			case 19: return (size_t)(uint32_t)P<iPhysicsRope>(a->p[0])->GetUniqueID();
			case 20: { iPhysicsMaterial *pM = P<iPhysicsMaterial>(a->p[0]); return pM->IsPreloaded()==false && pM->GetSurfaceData(); }
			case 21: { iPhysicsMaterial *pM = P<iPhysicsMaterial>(a->p[0]); pM->SetPreloaded(true); pM->GetSurfaceData()->PreloadData(); return 0; }
			case 22: pW->GetTempBodies().resize(0); pW->GetBodiesInBV(P<cBoundingVolume>(a->p[1]), &pW->GetTempBodies()); return 0;
			case 23: return pW->GetTempBodies().size();
			case 24: return reinterpret_cast<size_t>(pW->GetTempBodies()[a->p[1]]);
			case 25: a->rf = P<iPhysicsBody>(a->p[0])->GetMass(); return 0;
			case 26:
			{
				cBoundingVolume *pBV = new cBoundingVolume(P<iCollideShape>(a->p[1])->GetBoundingVolume());
				pBV->SetTransform(cMath::MatrixMul(*P<const cMatrixf>(a->p[2]), pBV->GetTransform()));
				return reinterpret_cast<size_t>(pBV);
			}
			case 27: delete P<cBoundingVolume>(a->p[0]); return 0;
			case 28: return cMath::CheckBVIntersection(*P<cBoundingVolume>(a->p[0]), *P<iPhysicsBody>(a->p[1])->GetBoundingVolume());
			case 29: P<iPhysicsBody>(a->p[0])->Enable(); return 0;
			case 30: pW->FadeoutBodySounds(P<iPhysicsBody>(a->p[1]), a->f[0]); return 0;
			case 31: pW->FadeoutJointSound(P<iPhysicsJoint>(a->p[1]), a->f[0]); return 0;
			case 32: return pW->GetContactPoints()->size();
			case 33:
			{
				const cColor *pCol = P<const cColor>(a->p[3]);
				pW->RenderContactPoint(P<iLowLevelGraphics>(a->p[1]), a->p[2], pCol[0], pCol[1]);
				return 0;
			}
			case 34:
			{
				iPhysicsBody *pB = P<iPhysicsBody>(a->p[0]);
				const OhplH15Query *q = P<const OhplH15Query>(a->p[1]);
				*P<OhplBodyFilter>(a->p[2]) = OhplBodyFilter{ pB->IsActive(), pB->IsCharacter(), reinterpret_cast<size_t>(pB) == q->skipBody, pB->GetMass(),
					pB->GetCollideCharacter(), pB->GetCollide(), (int32_t)pB->GetPushStrength(),
					iPhysicsBody::CollideFlagsMatch(q->flags, pB->GetCollideFlags()),
					q->collideCharacter, q->isCharacter, q->skipStatic, q->minPush };
				return 0;
			}
			case 35: return reinterpret_cast<size_t>(new cCollideData());
			case 36: delete P<cCollideData>(a->p[0]); return 0;
			case 37:
			{
				cCollideData *pCD = P<cCollideData>(a->p[0]);
				iPhysicsBody *pB = P<iPhysicsBody>(a->p[4]);
				pCD->SetMaxSize(32);
				return P<iPhysicsWorld>(a->p[1])->CheckShapeCollision(P<iCollideShape>(a->p[2]), *P<const cMatrixf>(a->p[3]), pB->GetShape(), pB->GetLocalMatrix(), *pCD, 32, true);
			}
			case 38: return (size_t)P<cCollideData>(a->p[0])->mlNumOfPoints;
			case 39:
			{
				const cCollidePoint &pt = P<cCollideData>(a->p[0])->mvContactPoints[a->p[1]];
				a->rv[0] = pt.mvNormal.x; a->rv[1] = pt.mvNormal.y; a->rv[2] = pt.mvNormal.z; a->rf = pt.mfDepth;
				return 0;
			}
			case 40: P<iPhysicsWorldCollisionCallback>(a->p[0])->OnCollision(P<iPhysicsBody>(a->p[1]), P<cCollideData>(a->p[2])); return 0;
			}
			return 0;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iPhysicsWorld::iPhysicsWorld()
	{
		mbLogDebug = false;
		mpWorld = NULL;
		mRsBook = ohpl_h15w_new(WorldOp, reinterpret_cast<size_t>(this));
	}

	//-----------------------------------------------------------------------

	iPhysicsWorld::~iPhysicsWorld()
	{
		ohpl_h15_world_free(mRsBook);
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::RegisterShape(iCollideShape *apShape){ ohpl_h15w_add(mRsBook, OhplKind_Shape, reinterpret_cast<size_t>(apShape)); }
	void iPhysicsWorld::RegisterBody(iPhysicsBody *apBody){ ohpl_h15w_add(mRsBook, OhplKind_Body, reinterpret_cast<size_t>(apBody)); }
	void iPhysicsWorld::RegisterCharBody(iCharacterBody *apBody){ ohpl_h15w_add(mRsBook, OhplKind_Char, reinterpret_cast<size_t>(apBody)); }
	void iPhysicsWorld::RegisterJoint(iPhysicsJoint *apJoint){ ohpl_h15w_add(mRsBook, OhplKind_Joint, reinterpret_cast<size_t>(apJoint)); }
	void iPhysicsWorld::RegisterController(iPhysicsController *apController){ ohpl_h15w_add(mRsBook, OhplKind_Ctrl, reinterpret_cast<size_t>(apController)); }
	void iPhysicsWorld::RegisterRope(iPhysicsRope *apRope){ ohpl_h15w_add(mRsBook, OhplKind_Rope, reinterpret_cast<size_t>(apRope)); }
	void iPhysicsWorld::RegisterMaterial(const tString &asName, iPhysicsMaterial *apMaterial)
	{
		ohpl_h15w_add_material(mRsBook, asName.c_str(), asName.size(), reinterpret_cast<size_t>(apMaterial));
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void iPhysicsWorld::Update(float afTimeStep)
	{
		ohpl_h15w_update(mRsBook, afTimeStep);
	}
	
	//-----------------------------------------------------------------------

	void iPhysicsWorld::DestroyShape(iCollideShape *apShape)
	{
		ohpl_h15w_destroy_shape(mRsBook, reinterpret_cast<size_t>(apShape));
	}

	//-----------------------------------------------------------------------
	
	void iPhysicsWorld::DestroyBody(iPhysicsBody* apBody)
	{
		ohpl_h15w_destroy_body(mRsBook, reinterpret_cast<size_t>(apBody));
	}

	iPhysicsBody *iPhysicsWorld::GetBody(const tString &asName)
	{
		return reinterpret_cast<iPhysicsBody*>(ohpl_h15w_find_by_name(mRsBook, OhplKind_Body, asName.c_str(), asName.size()));
	}

	tPhysicsBodyList& iPhysicsWorld::BodyList()
	{
		mlstBodies.clear();
		for(size_t i=0, n=ohpl_h15w_len(mRsBook, OhplKind_Body); i<n; ++i) mlstBodies.push_back(reinterpret_cast<iPhysicsBody*>(ohpl_h15w_at(mRsBook, OhplKind_Body, i)));
		return mlstBodies;
	}

	cPhysicsBodyIterator iPhysicsWorld::GetBodyIterator()
	{
		return cPhysicsBodyIterator(&BodyList());
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::EnableBodiesInBV(cBoundingVolume *apBV, bool abEnabled)
	{
		ohpl_h15w_enable_bodies_in_bv(mRsBook, reinterpret_cast<size_t>(apBV));
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::AddBodyToUpdateList(iPhysicsBody *apBody)
	{
		ohpl_h15w_update_add(mRsBook, reinterpret_cast<size_t>(apBody));
	}

	//NOTE: Skipping removing dynamic bodies for now since they will need to be checked if enabled 
	//		Still want to remove when destroying though.
	void iPhysicsWorld::RemoveBodyFromUpdateList(iPhysicsBody *apBody, bool abDestroyingBody)
	{
		ohpl_h15w_update_remove(mRsBook, reinterpret_cast<size_t>(apBody));
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::DestroyJoint(iPhysicsJoint* apJoint)
	{
		ohpl_h15w_find_and_delete(mRsBook, OhplKind_Joint, reinterpret_cast<size_t>(apJoint));
	}

	iPhysicsJoint *iPhysicsWorld::GetJoint(const tString &asName)
	{
		return reinterpret_cast<iPhysicsJoint*>(ohpl_h15w_find_by_name(mRsBook, OhplKind_Joint, asName.c_str(), asName.size()));
	}

	bool iPhysicsWorld::JointExists(iPhysicsJoint* apJoint)
	{
		return ohpl_h15w_contains(mRsBook, OhplKind_Joint, reinterpret_cast<size_t>(apJoint));
	}

	cPhysicsJointIterator iPhysicsWorld::GetJointIterator()
	{
		mlstJoints.clear();
		for(size_t i=0, n=ohpl_h15w_len(mRsBook, OhplKind_Joint); i<n; ++i) mlstJoints.push_back(reinterpret_cast<iPhysicsJoint*>(ohpl_h15w_at(mRsBook, OhplKind_Joint, i)));
		return cPhysicsJointIterator(&mlstJoints);
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::DestroyCharacterBody(iCharacterBody* apBody)
	{
		ohpl_h15w_find_and_delete(mRsBook, OhplKind_Char, reinterpret_cast<size_t>(apBody));
	}
		
	iPhysicsBody *iPhysicsWorld::GetCharacterBody(const tString &asName)
	{
		return reinterpret_cast<iPhysicsBody*>(ohpl_h15w_find_by_name(mRsBook, OhplKind_Char, asName.c_str(), asName.size()));
	}

	//-----------------------------------------------------------------------
	
	iPhysicsMaterial* iPhysicsWorld::GetMaterialFromName(const tString &asName)
	{
		return reinterpret_cast<iPhysicsMaterial*>(ohpl_h15w_get_material(mRsBook, asName.c_str(), asName.size()));
	}
	
	//-----------------------------------------------------------------------

	cPhysicsMaterialIterator iPhysicsWorld::GetMaterialIterator()
	{
		m_mapMaterials.clear();
		for(size_t i=0, n=ohpl_h15w_material_len(mRsBook); i<n; ++i)
		{
			OhplStr name; size_t obj;
			if(ohpl_h15w_material_at(mRsBook, i, &name, &obj)) m_mapMaterials[tString(name.p, name.n)] = reinterpret_cast<iPhysicsMaterial*>(obj);
		}
		return cPhysicsMaterialIterator(&m_mapMaterials);
	}

	//-----------------------------------------------------------------------

	iPhysicsRope* iPhysicsWorld::GetRope(const tString &asName)
	{
		return reinterpret_cast<iPhysicsRope*>(ohpl_h15w_find_by_name(mRsBook, OhplKind_Rope, asName.c_str(), asName.size()));
	}

	iPhysicsRope* iPhysicsWorld::GetRopeFromUniqueID(int alID)
	{
		return reinterpret_cast<iPhysicsRope*>(ohpl_h15w_rope_from_unique_id(mRsBook, alID));
	}

	void iPhysicsWorld::DestroyRope(iPhysicsRope* apRope)
	{
		ohpl_h15w_find_and_delete(mRsBook, OhplKind_Rope, reinterpret_cast<size_t>(apRope));
	}

	//-----------------------------------------------------------------------
	
	void iPhysicsWorld::DestroyAll()
	{
		ohpl_h15w_destroy_all(mRsBook);
	}

	//-----------------------------------------------------------------------

	
	void iPhysicsWorld::DestroyController(iPhysicsController *apController)
	{
		ohpl_h15w_find_and_delete(mRsBook, OhplKind_Ctrl, reinterpret_cast<size_t>(apController));
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::FadeoutBodySounds(iPhysicsBody *pBody, float afFadeSpeed)
	{
		cSoundEntity *pRollSound = pBody->GetRollSoundEntity();
		if(pRollSound && mpWorld->SoundEntityExists(pRollSound,pBody->GetRollSoundEntityID())) pRollSound->FadeOut(afFadeSpeed);
		pBody->SetRollSoundEntity(NULL);

		cSoundEntity *pScrapeSound = pBody->GetScrapeSoundEntity();
		if(pScrapeSound && mpWorld->SoundEntityExists(pScrapeSound, pBody->GetScrapeSoundEntityID())) pScrapeSound->FadeOut(afFadeSpeed);
		pBody->SetScrapeSoundEntity(NULL);
	}

	void iPhysicsWorld::FadeoutJointSound(iPhysicsJoint *pJoint, float afFadeSpeed)
	{
		cSoundEntity *pLoopSound = pJoint->GetSound();
		if(pLoopSound && mpWorld->SoundEntityExists(pLoopSound, pJoint->GetSoundID())) pLoopSound->FadeOut(afFadeSpeed);
		pJoint->SetSound(NULL);
	}

	void iPhysicsWorld::FadeoutAllLoopSounds(float afFadeSpeed)
	{
		ohpl_h15w_fadeout_all_loop_sounds(mRsBook, afFadeSpeed);
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::RenderContactPoint(iLowLevelGraphics *apLowLevel, size_t i, const cColor& aPointColor, const cColor& aLineColor)
	{
		apLowLevel->DrawSphere(mvContactPoints[i].mvPoint,0.2f, aPointColor);
		apLowLevel->DrawLine(mvContactPoints[i].mvPoint,
							mvContactPoints[i].mvNormal * mvContactPoints[i].mfDepth *0.2f,
							aLineColor);
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::RenderContactPoints(iLowLevelGraphics *apLowLevel, const cColor& aPointColor,
											const cColor& aLineColor)
	{
		const cColor colors[2] = { aPointColor, aLineColor };
		ohpl_h15w_render_contacts(mRsBook, reinterpret_cast<size_t>(apLowLevel), reinterpret_cast<size_t>(colors));
	}

	//-----------------------------------------------------------------------
	
	bool iPhysicsWorld::CheckShapeWorldCollision(cVector3f *apPushVector,
							iCollideShape* apShape, const cMatrixf& a_mtxTransform,
							iPhysicsBody *apSkipBody, bool abSkipStatic, bool abIsCharacter,
							iPhysicsWorldCollisionCallback *apCallback,
							bool abCollideCharacter,
							int alMinPushStrength,
							tFlag alCollideFlags,
							bool abDebug)
	{
		const OhplH15Query query = { reinterpret_cast<size_t>(apSkipBody), (uint32_t)alCollideFlags, abCollideCharacter, abIsCharacter, abSkipStatic, (int32_t)alMinPushStrength };
		return ohpl_h15w_shape_world_collision(mRsBook, apPushVector, reinterpret_cast<size_t>(apShape), &a_mtxTransform, &query, reinterpret_cast<size_t>(apCallback));
	}

	//-----------------------------------------------------------------------

}
