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

#ifndef HPL_SOUND_ENTITY_DATA_H
#define HPL_SOUND_ENTITY_DATA_H

#include "system/SystemTypes.h"
#include "resources/ResourceBase.h"
#include "sound/SoundTypes.h"
#include "sound/RustSound.h"

namespace hpl {

	class cResources;
	class cSound;

	/** A .snt sound entity file; the record is a Rust object, the class is the resource shell. */
	class cSoundEntityData : public iResourceBase
	{
	public:
		cSoundEntityData(const tString& asName, cResources *apResources, cSound *apSound) : iResourceBase(asName, _W(""),0), mpResources(apResources), mpRs(ohpl_sndd_new(this)) {}
		~cSoundEntityData(){ ohpl_sndd_free(mpRs); }

		OhplSndData* Rs(){ return mpRs; }

		tString GetSoundName(eSoundEntityType aType, int alIdx){ return Name(aType, alIdx, false, false); }
		tString GetRandomSoundName(eSoundEntityType aType, bool abSkipPrevious){ return Name(aType, 0, true, abSkipPrevious); }
		int GetSoundNum(eSoundEntityType aType){ return (int)ohpl_sndd_num(mpRs, aType); }
		void SetSoundName(const tString& asName, eSoundEntityType aType, int alIdx){ if(alIdx >= 0) ohpl_sndd_put_name(mpRs, aType, alIdx, (const unsigned char*)asName.data(), asName.size()); }
		void AddSoundName(const tString& asName, eSoundEntityType aType){ ohpl_sndd_put_name(mpRs, aType, -1, (const unsigned char*)asName.data(), asName.size()); }
		bool HasSound(eSoundEntityType aType){ return ohpl_sndd_num(mpRs, aType) > 0; }

		void PreloadSounds(){ ohpl_sndd_preload(mpRs, OhplSndHostTable(), mpResources); }

		bool CreateFromFile(const tWString &asFile);

		void SetFadeStart(bool abX){ ohpl_sndd_set(mpRs, 7, abX); }
		bool GetFadeStart(){ return ohpl_sndd_get(mpRs, 7) != 0; }
		void SetFadeStop(bool abX){ ohpl_sndd_set(mpRs, 8, abX); }
		bool GetFadeStop(){ return ohpl_sndd_get(mpRs, 8) != 0; }

		void SetVolume(float afX){ ohpl_sndd_set(mpRs, 0, afX); }
		float GetVolume(){ return ohpl_sndd_get(mpRs, 0); }
		void SetMaxDistance(float afX){ ohpl_sndd_set(mpRs, 1, afX); }
		float GetMaxDistance(){ return ohpl_sndd_get(mpRs, 1); }
		void SetMinDistance(float afX){ ohpl_sndd_set(mpRs, 2, afX); }
		float GetMinDistance(){ return ohpl_sndd_get(mpRs, 2); }

		void SetStream(bool abX){ ohpl_sndd_set(mpRs, 9, abX); }
		bool GetStream(){ return ohpl_sndd_get(mpRs, 9) != 0; }

		static void SetLazyLoad(bool abX){ ohpl_sndd_set_lazy(abX); }
		void SetLoop(bool abX){ ohpl_sndd_set(mpRs, 10, abX); }
		bool GetLoop(){ return ohpl_sndd_get(mpRs, 10) != 0; }
		void SetUse3D(bool abX){ ohpl_sndd_set(mpRs, 11, abX); }
		bool GetUse3D(){ return ohpl_sndd_get(mpRs, 11) != 0; }

		void SetBlockable(bool abX){ ohpl_sndd_set(mpRs, 12, abX); }
		bool GetBlockable(){ return ohpl_sndd_get(mpRs, 12) != 0; }
		void SetBlockVolumeMul(float afX){ ohpl_sndd_set(mpRs, 3, afX); }
		float GetBlockVolumeMul(){ return ohpl_sndd_get(mpRs, 3); }

		void SetRandom(float afX){ ohpl_sndd_set(mpRs, 4, afX); }
		float GetRandom(){ return ohpl_sndd_get(mpRs, 4); }
		void SetInterval(float afX){ ohpl_sndd_set(mpRs, 5, afX); }
		float GetInterval(){ return ohpl_sndd_get(mpRs, 5); }

		void SetPriority(int alX){ ohpl_sndd_set(mpRs, 6, (float)alX); }
		int GetPriority(){ return (int)ohpl_sndd_get(mpRs, 6); }

		//Resource implementation
		bool Reload(){ return false;}
		void Unload(){}
		void Destroy(){}

	private:
		tString Name(eSoundEntityType aType, int alIdx, bool abRandom, bool abSkipPrevious)
		{
			size_t n;
			const unsigned char *p = ohpl_sndd_name(mpRs, aType, alIdx, abRandom, abSkipPrevious, &n);
			return tString((const char*)p, n);
		}

		cResources *mpResources;
		OhplSndData *mpRs;
	};
};
#endif // HPL_SOUND_DATA_H
