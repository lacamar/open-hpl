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

#ifndef HPL_RESOURCEBASE_H
#define HPL_RESOURCEBASE_H

#include <time.h>
#include "system/SystemTypes.h"
#include "system/LowLevelSystem.h"
#include "resources/RustResources.h"
#include "resources/RustResourcesH3.h"

namespace hpl {

	class iResourceBase
	{
	public:
		
		iResourceBase(const tString& asName, const tWString& asFullPath,unsigned long alPrio);

		virtual ~iResourceBase();
				
		/**
		 * virtual bool Reload()=0; 
		 * \return true is reload was succesful, else false.
		 */
		virtual bool Reload()=0;

		/**
		 * Free most the memory, save info to get started again. 
		 */
		virtual void Unload()=0;

        /**
         * Free all memory.
         */
        virtual void Destroy()=0;

		const tString& GetName(){return msName;}
		unsigned long GetHandle(){return mMeta.handle;}
		void SetHandle(unsigned long alHandle){mMeta.handle=alHandle;}
		
		void SetFullPath(const tWString& asPath);
		const tWString& GetFullPath(){return msFullPath;}
		
		unsigned long GetTime(){return mMeta.time;}
		unsigned long GetPrio(){return mMeta.prio;}
		unsigned long GetSize(){return mMeta.size;}

		void SetLogDestruction(bool abX){ mMeta.log_destruction = abX;}
		
		unsigned int GetUserCount(){return mMeta.user_count;}
		void IncUserCount();
		void DecUserCount(){ohpl_resmeta_dec_user(&mMeta);}
		bool HasUsers(){ return mMeta.user_count>0;}

		OhplResMeta* GetMeta(){ return &mMeta; }

		static bool GetLogCreateAndDelete(){ return ohpl_h3_log_create_get();}
		static void SetLogCreateAndDelete(bool abX){ ohpl_h3_log_create_set(abX);}

	protected:
		
		tString msName;
		
		OhplResMeta mMeta;
	
	private:
		tWString msFullPath;
	};

};
#endif // HPL_RESOURCEBASE_H
