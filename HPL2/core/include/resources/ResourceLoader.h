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

#ifndef HPL_RESOURCE_LOADER_H
#define HPL_RESOURCE_LOADER_H

#include "system/SystemTypes.h"
#include "resources/ResourcesTypes.h"
#include "resources/RustResources.h"

namespace hpl {

	class iResourceLoader
	{
	friend class iResourceLoaderHandler;
	public:
		iResourceLoader();
		virtual ~iResourceLoader();
		iResourceLoader(const iResourceLoader&) = delete;
		iResourceLoader& operator=(const iResourceLoader&) = delete;

		bool IsSupported(const tString& asFileExt);

		void AddSupportedExtension(const tString& asExt);

	protected:
		OhplExtList *mpExtensions;
	};

};
#endif // HPL_RESOURCE_LOADER_H
