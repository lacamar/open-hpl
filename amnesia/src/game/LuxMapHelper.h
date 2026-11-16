#ifndef LUX_MAP_HELPER_H
#define LUX_MAP_HELPER_H

#include "LuxBase.h"
#include "RustLuxMapA13.h"

class iLuxEntity;

// Shell: the queries live in rust/src/luxmap/mhelper.rs.
class cLuxMapHelper : public iLuxUpdateable
{
public:
	cLuxMapHelper() : iLuxUpdateable("LuxMapHelper"){}

	void OnStart(){}
	void Update(float afTimeStep){}
	void Reset(){}

	bool ShapeDamage(	iCollideShape *apShape, const cMatrixf& a_mtxTransform, const cVector3f &avOrigin,
						float afMinDamage, float afMaxDamage, float afForce, float afMaxImpulse,
						int alStrength, float afHitSpeed, eLuxDamageType aDamageType,eLuxWeaponHitType aWeaponHitType,
						bool abCheckEnemies, bool abCheckPlayer, bool abCheckProps, bool abLethalForPlayer,
						bool *apHitPlayer=NULL)
	{
		OhplA13Damage d = { apShape, {}, { avOrigin.x, avOrigin.y, avOrigin.z }, afMinDamage, afMaxDamage, afForce, afMaxImpulse,
							alStrength, afHitSpeed, (int32_t)aDamageType, (int32_t)aWeaponHitType,
							abCheckEnemies, abCheckPlayer, abCheckProps, abLethalForPlayer };
		memcpy(d.transform, a_mtxTransform.v, sizeof(d.transform));
		bool bHitPlayer = false;
		bool bHit = ohpl_a13_shape_damage(&d, &bHitPlayer);
		if(apHitPlayer) *apHitPlayer = bHitPlayer;
		return bHit;
	}

	bool CheckLineOfSight(const cVector3f& avStart, const cVector3f& avEnd, bool abCheckShadows)
	{
		return ohpl_a13_check_los(&avStart.x, &avEnd.x, abCheckShadows);
	}

	bool GetClosestEntity(	const cVector3f& avStart,const cVector3f& avDir, float afRayLength,
							float *afDistance, iPhysicsBody** apBody, iLuxEntity **apEntity)
	{
		float fDist = afDistance ? *afDistance : 0;
		void *pBody = apBody ? *apBody : NULL, *pEntity = apEntity ? *apEntity : NULL;
		bool bFound = ohpl_a13_closest_entity(&avStart.x, &avDir.x, afRayLength, &fDist, &pBody, &pEntity);
		if(afDistance)	*afDistance = fDist;
		if(apBody)		*apBody = (iPhysicsBody*)pBody;
		if(apEntity)	*apEntity = (iLuxEntity*)pEntity;
		return bFound;
	}

	bool GetClosestCharCollider(const cVector3f& avStart,const cVector3f& avDir, float afRayLength,
								float *afDistance, cVector3f *avNormal, iPhysicsBody** apBody)
	{
		float fDist = afDistance ? *afDistance : 0;
		cVector3f vNormal = avNormal ? *avNormal : cVector3f(0,0,0);
		void *pBody = apBody ? *apBody : NULL;
		bool bFound = ohpl_a13_closest_char(&avStart.x, &avDir.x, afRayLength, &fDist, &vNormal.x, &pBody);
		if(afDistance)	*afDistance = fDist;
		if(avNormal)	*avNormal = vNormal;
		if(apBody)		*apBody = (iPhysicsBody*)pBody;
		return bFound;
	}

	float GetLightLevelAtPos(const cVector3f& avPos, std::vector<iLight*>* apSkipLightsVec=NULL, float afRadiusAdd=0)
	{
		return ohpl_a13_light_level(&avPos.x, apSkipLightsVec ? (const void *const*)apSkipLightsVec->data() : NULL,
									apSkipLightsVec ? apSkipLightsVec->size() : 0, apSkipLightsVec != NULL, afRadiusAdd);
	}
};

#endif // LUX_MAP_HELPER_H
