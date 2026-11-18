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

#ifndef HPL_PARTICLE_SYSTEM_H
#define HPL_PARTICLE_SYSTEM_H

#include "system/SystemTypes.h"

#include "scene/Entity3D.h"
#include "scene/RustSP.h"
#include "resources/ResourceBase.h"

namespace hpl {

	class cGraphics;
	class cResources;
	class iParticleEmitter;
	class iParticleEmitterData;
	class cParticleManager;
	class cXmlElement;
	class iLight;

	class cParticleSystem;

	class cParticleSystemData : public iResourceBase
	{
	public:
		cParticleSystemData(const tString &asName,cResources* apResources,cGraphics *apGraphics)
			: iResourceBase(asName, _W(""),0), mpResources(apResources), mpGraphics(apGraphics), mpRust(ohpl_spd_new()) {}
		virtual ~cParticleSystemData();

		cParticleSystem* Create(tString asName, cVector3f avSize);

		bool LoadFromFile(const tWString &asFile);
		bool LoadFromElement(cXmlElement* apElement);

		void AddEmitterData(iParticleEmitterData *apData){ ohpl_spd_add(mpRust, apData); }

		bool Reload(){ return false;}
		void Unload(){}
		void Destroy(){}

		iParticleEmitterData* GetEmitterData(int alIdx) const { return static_cast<iParticleEmitterData*>(ohpl_spd_get(mpRust, alIdx)); }

	private:
		cResources* mpResources;
		cGraphics *mpGraphics;
		OhplSPData *mpRust;
	};

	class cParticleSystem : public iEntity3D
	{
	friend class iParticleEmitter;
	friend struct OhplH12PsHostImpl;
	public:
		cParticleSystem(const tString asName,cParticleSystemData *apData, cResources *apResources, cGraphics *apGraphics)
			: iEntity3D(asName), mpRust(ohpl_sp_new(apData)), mpS(reinterpret_cast<OhplSPPub*>(mpRust)) {}
		~cParticleSystem();

		bool IsVisible(){ return mpS->mbIsVisible;}
		void SetVisible(bool abVisible);
		void SetCulledByDistance(bool abX);
		void SetCulledByFog(bool abX);

		void UpdateLogic(float afTimeStep);

		bool IsDead();
		bool IsDying();
		bool GetRemoveWhenDead() { return mpS->mbRemoveWhenDead; }
		void SetRemoveWhenDead(bool abX) { mpS->mbRemoveWhenDead = abX; }

		void Kill();
		void KillInstantly();

		void AddEmitter(iParticleEmitter* apEmitter);
		iParticleEmitter* GetEmitter(int alIdx){ return static_cast<iParticleEmitter*>(ohpl_sp_emitter(mpRust, alIdx)); }
		int GetEmitterNum(){ return ohpl_sp_emitter_count(mpRust); }

		tString GetEntityType(){ return "ParticleSystem3D"; }

		void SetDataName(const tString &asName) { ohpl_sp_set_data_name(mpRust, reinterpret_cast<const uint8_t*>(asName.data()), asName.size()); }
		void SetDataSize(const cVector3f &avSize) { mpS->mvDataSize = avSize; }

		tString GetDataName() { size_t n; const char *p = reinterpret_cast<const char*>(ohpl_sp_data_name(mpRust, &n)); return tString(p, n); }
		const cVector3f& GetDataSize() { return mpS->mvDataSize;}

		void SetColor(const cColor& aColor){ mpS->mColor = aColor;}
		void SetFadeAtDistance(bool abX){ mpS->mbFadeAtDistance = abX;}
		void SetMinFadeDistanceStart(float afX){ mpS->mfMinFadeDistanceStart = afX;}
		void SetMinFadeDistanceEnd(float afX){ mpS->mfMinFadeDistanceEnd = afX;}
		void SetMaxFadeDistanceStart(float afX){ mpS->mfMaxFadeDistanceStart = afX;}
		void SetMaxFadeDistanceEnd(float afX){ mpS->mfMaxFadeDistanceEnd = afX;}

		void SetBrightness(float afX){ mpS->mfBrightness = afX;}
		float GetBrightness(){ return mpS->mfBrightness;}
		static void SetHpl3Color(bool abX){ ohpl_sp_set_hpl3_color(abX);}
		static bool GetHpl3Color(){ return ohpl_sp_hpl3_color();}

		cColor GetColor(){ return mpS->mColor;}
		bool GetFadeAtDistance(){ return mpS->mbFadeAtDistance;}
		float GetMinFadeDistanceStart(){ return mpS->mfMinFadeDistanceStart;}
		float GetMinFadeDistanceEnd(){ return mpS->mfMinFadeDistanceEnd;}
		float GetMaxFadeDistanceStart(){ return mpS->mfMaxFadeDistanceStart;}
		float GetMaxFadeDistanceEnd(){ return mpS->mfMaxFadeDistanceEnd;}

		void SetLightColor(const cColor& aColor){ mpS->mLightColor = aColor;}
		void SetLightBrightness(float afX){ mpS->mfLightBrightness = afX;}
		iLight* GetLight(){ return static_cast<iLight*>(mpS->mpLight);}
		void SetLight(iLight *apLight){ mpS->mpLight = apLight;}

		void SetParticleManager(cParticleManager *apParticleManager){ mpS->mpManager = apParticleManager;}

		cParticleSystemData *GetData(){ return static_cast<cParticleSystemData*>(mpS->mpData);}

		const OhplSPPub& Pub() const { return *mpS;}
		bool FirstUpdate() const { return mpS->mbFirstUpdate;}

	private:
		OhplSPSys *mpRust;
		OhplSPPub *mpS;
	};

};
#endif // HPL_PARTICLE_SYSTEM_H
