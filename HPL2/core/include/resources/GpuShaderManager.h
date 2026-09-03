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

#ifndef HPL_GPU_SHADER_MANAGER_H
#define HPL_GPU_SHADER_MANAGER_H

#include "resources/ResourceManager.h"

#include "graphics/GPUShader.h"

namespace hpl {

	//------------------------------------

	class iLowLevelGraphics;
	class cParserVarContainer;
	class cPreprocessParser;

	//------------------------------------

	/**
	 * Optional fallback hook so a .glsl lookup that finds nothing can still
	 * succeed off a same-named .hpsl file (SOMA/Rebirth/Bunker's HPL3 shader
	 * source format) - see cGpuShaderManager::SetHpslTranspileCallback().
	 * HPL2/core cannot link soma/src/game/HpslTranspiler.cpp directly (it's
	 * game-module code, and the only game module that needs it), so the
	 * signature here matches TranspileHpslToGlsl() exactly and whichever
	 * game module owns a transpiler registers it at startup; Dark
	 * Descent/AMFP never call SetHpslTranspileCallback(), so this stays
	 * NULL and the fallback path is dead code for them.
	 * \param asHpslSource HPSL source, already run through cPreprocessParser::Parse().
	 * \param aType eGpuShaderType_Vertex or eGpuShaderType_Fragment.
	 * \param asGlslOut receives the transpiled GLSL source on success.
	 * \param asErrorOut receives a human-readable reason on failure.
	 */
	typedef bool (*tHpslTranspileCallback)(const tString& asHpslSource, eGpuShaderType aType,
											tString& asGlslOut, tString& asErrorOut);

	class cGpuShaderManager : public iResourceManager
	{
	public:
		cGpuShaderManager(cFileSearcher *apFileSearcher, iLowLevelGraphics *apLowLevelGraphics,
							iLowLevelResources *apLowLevelResources,iLowLevelSystem *apLowLevelSystem);
		~cGpuShaderManager();

		void CheckFeatureSupport();
						
		/**
		 * Creates a new GPU program
		 * \param asName name of the program 
		 * \param asEntry the entry point of the program (usually "main")
		 * \param aType type of the program
		 * \return 
		 */
		iGpuShader* CreateShader(const tString& asName,eGpuShaderType aType, cParserVarContainer *apVarContainer);

		void Destroy(iResourceBase* apResource);
		void Unload(iResourceBase* apResource);

		/**
		 * Registers a game-module-supplied HPSL->GLSL transpiler used as a
		 * fallback when a .glsl file can't be found but a same-named .hpsl
		 * one can. NULL (no fallback) by default - only SOMA/Rebirth/
		 * Bunker's game module calls this; Dark Descent/AMFP never do, so
		 * their .glsl-only lookups are completely unaffected.
		 */
		static void SetHpslTranspileCallback(tHpslTranspileCallback aCallback) { mpHpslTranspileCallback = aCallback; }

	private:
		bool IsShaderSupported(const tString& asName, eGpuShaderType aType);

		iLowLevelGraphics *mpLowLevelGraphics;
		cPreprocessParser* mpPreprocessParser;

		static tHpslTranspileCallback mpHpslTranspileCallback;
	};

};
#endif // HPL_GPU_SHADER_MANAGER_H
