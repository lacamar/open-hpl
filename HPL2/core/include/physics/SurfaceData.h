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

#ifndef HPL_SURFACE_DATA_H
#define HPL_SURFACE_DATA_H

#include "math/MathTypes.h"
#include "physics/PhysicsMaterial.h"
#include "physics/RustPhysBook.h"

namespace hpl {

	#define eRollAxisFlag_X		1
	#define eRollAxisFlag_Y		2
	#define eRollAxisFlag_Z		4

    class cPhysics;
	struct OhplSurfaceParams;
	class iPhysicsWorld;
	class iPhysicsBody;
	class cResources;

	class cSurfaceImpactData
	{
	friend class cSurfaceData;
	public:

		float GetMinSpeed(){ return mfMinSpeed;}

		const tString& GetSoundName(){ return msSoundName;}
		void SetSoundName(const tString& asName){ msSoundName =asName;}

		const tString& GetPSName(){ return msPSName;}
		void SetPSName(const tString& asName){ msPSName =asName;}

		int GetPSPrio(){ return mlPSPrio;}
		void SetPSPrio(int alPrio){ mlPSPrio = alPrio;}

	private:
		float mfMinSpeed;
		tString msSoundName;
		tString msPSName;
		int mlPSPrio;
	};

	class cSurfaceData
	{
	public:
		cSurfaceData(const tString &asName, cPhysics *apPhysics,cResources *apResources);
		~cSurfaceData();

		const tString& GetName() const{ return msName;}
		const tString& GetUserVar() const{ return msUserVar;}
		void SetUserVar(const tString& asX){ msUserVar = asX;}

		void OnImpact(float afSpeed,const cVector3f &avPos,int alContacts,  iPhysicsBody *apBody);
		void OnSlide(float afSpeed,const cVector3f &avPos,int alContacts, iPhysicsBody *apBody,
						iPhysicsBody *apSlideAgainstBody);
		void CreateImpactEffect(float afSpeed,const cVector3f &avPos,int alContacts,
								cSurfaceData *apSecondSurface, iPhysicsWorld *apPhysicsWorld);

		void UpdateRollEffect(iPhysicsBody *apBody);

		void SetElasticity(float afElasticity);
		float GetElasticity() const;
		void SetStaticFriction(float afElasticity);
		float GetStaticFriction() const;
		void SetKineticFriction(float afElasticity);
		float GetKineticFriction() const;

		void SetPriority(int alPriority);
		int GetPriority() const;

		void SetFrictionCombMode(ePhysicsMaterialCombMode aMode);
		ePhysicsMaterialCombMode GetFrictionCombMode() const;
		void SetElasticityCombMode(ePhysicsMaterialCombMode aMode);
		ePhysicsMaterialCombMode GetElasticityCombMode() const;

		const tString& GetStepType(){ return msStepType;}
		void GetStepType(const tString& asX){ msStepType = asX;}

		void SetMinScrapeSpeed(float afX){mS->minScrapeSpeed = afX;}
		void SetMinScrapeFreq(float afX){mS->minScrapeFreq = afX;}
		void SetMinScrapeFreqSpeed(float afX){mS->minScrapeFreqSpeed = afX;}
		void SetMaxScrapeFreq(float afX){mS->maxScrapeFreq = afX;}
		void SetMaxScrapeFreqSpeed(float afX){mS->maxScrapeFreqSpeed = afX;}
		void SetMiddleScrapeSpeed(float afX){mS->middleScrapeSpeed = afX;}
		void SetMinScrapeContacts(int alX){mS->minScrapeContacts = alX;}
		void SetScrapeSoundName(const tString &asName){msScrapeSoundName = asName;}

		void SetMinRollSpeed(float afX){mS->minRollSpeed = afX;}
		void SetMinRollFreq(float afX){mS->minRollFreq = afX;}
		void SetMinRollVolume(float afX){mS->minRollVolume = afX;}
		void SetMinRollFreqSpeed(float afX){mS->minRollFreqSpeed = afX;}
		void SetMaxRollFreq(float afX){mS->maxRollFreq = afX;}
		void SetMaxRollVolume(float afX){mS->maxRollVolume = afX;}
		void SetMaxRollFreqSpeed(float afX){mS->maxRollFreqSpeed = afX;}
		void SetMiddleRollSpeed(float afX){mS->middleRollSpeed = afX;}
		void SetRollSoundName(const tString &asName){msRollSoundName = asName;}
		void SetRollAxisFlags(tFlag aAxisFlags){mS->rollAxisFlags = aAxisFlags;}

        void PreloadData();
        OhplSurfaceState* RustState(){ return mS;}
        static size_t HostOp(int32_t alOp, OhplH15Wa *a);

		iPhysicsMaterial *ToMaterial(iPhysicsWorld *apWorld);

		/**
		 * This must be added with the largest speed first.
		**/
		cSurfaceImpactData* CreateImpactData(float afMinSpeed);
		cSurfaceImpactData* GetImpactData(int alIdx);
		int GetImpactDataNum();
		/**
		* Gets the the appropriate impact data depending on speed. It gets the data with highest speed not higher than afSpeed
		* \param afSpeed The speed value.
		* \return
		*/
		cSurfaceImpactData* GetImpactDataFromSpeed(float afSpeed);

		/**
		* This must be added with the largest speed first.
		**/
		cSurfaceImpactData* CreateHitData(float afMinSpeed);
		cSurfaceImpactData* GetHitData(int alIdx);
		int GetHitDataNum();
		/**
		 * Gets the the appropriate hit data depending on speed. It gets the data with highest speed not higher than afSpeed
		 * \param afSpeed The speed value.
		 * \return
		 */
		cSurfaceImpactData* GetHitDataFromSpeed(float afSpeed);

	protected:
		void RustParams(OhplSurfaceParams &aParams) const;

		cResources *mpResources;
		cPhysics *mpPhysics;

		tString msName;
		tString msUserVar;

		OhplSurfaceState *mS;

		//Properties (references into mS, owned by rust/src/physics/surf.rs)
		tString msScrapeSoundName;
		tString msRollSoundName;

        tString msStepType;
	};
};
#endif // HPL_SURFACE_DATA_H
