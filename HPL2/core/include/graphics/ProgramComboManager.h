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

#ifndef HPL_PROGRAM_COMBO_MANAGER_H
#define HPL_PROGRAM_COMBO_MANAGER_H

#include "system/SystemTypes.h"
#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/RustMt.h"

namespace hpl {

	class cGraphics;
	class cResources;
	class iGpuProgram;
	class iGpuShader;
	class cParserVarContainer;

	#define kPC_VertexBit		eFlagBit_0
	#define kPC_FragmentBit		eFlagBit_1

	// Generated programs and shaders by feature flags; the state lives in Rust (rust/src/material/pcm.rs).
	class cProgramComboManager
	{
	public:
		cProgramComboManager(const tString& asName, cGraphics *apGraphics, cResources *apResources, int alNumOfMainModes) :
			mpRs(ohpl_pcm_new(OhplMtHostGet(), (const unsigned char*)asName.data(), asName.size(), apGraphics, apResources, alNumOfMainModes > 0 ? alNumOfMainModes : 0)){}
		~cProgramComboManager(){ ohpl_pcm_free(mpRs); }

		void SetName(const tString& asName){ ohpl_pcm_set_name(mpRs, (const unsigned char*)asName.data(), asName.size()); }

		iGpuProgram* GenerateProgram(int alMainMode, int alFlags){ return static_cast<iGpuProgram*>(ohpl_pcm_generate_program(mpRs, alMainMode, alFlags)); }
		int GetGenerateCombinationNum(int alMainMode){ return ohpl_pcm_combination_num(mpRs, alMainMode); }

		void SetupGenerateProgramData(int alMainMode, const tString &asModeName,
								const tString &asVtxShaderName, const tString &asFragShaderName,
								cProgramComboFeature *apFeatures, int alFeatureNum,
								cParserVarContainer &avDefaultVars);

		void AddGenerateProgramVariableId(const tString& asVarName, int alId, int alMainMode){ ohpl_pcm_add_variable_id(mpRs, (const unsigned char*)asVarName.data(), asVarName.size(), alId, alMainMode); }

		void DestroyGeneratedProgram(int alMainMode, iGpuProgram* apProgram){ ohpl_pcm_destroy_generated_program(mpRs, alMainMode, apProgram); }
		void DestroyGeneratedShader(int alMainMode, iGpuShader* apShader, eGpuShaderType aType){ ohpl_pcm_destroy_generated_shader(mpRs, alMainMode, apShader, aType); }

		iGpuShader *CreateShader(const tString& asName, eGpuShaderType aType, cParserVarContainer *apVars, bool abAddtoList);
		iGpuProgram *CreateProgram(const tString& asName, bool abAddtoList){ return static_cast<iGpuProgram*>(ohpl_pcm_create_program(mpRs, (const unsigned char*)asName.data(), asName.size(), abAddtoList)); }

		/**
		 * Do NOT use these to destroy program create with SetupProgramModeData
		 */
		void DestroyShader(iGpuShader * apShader){ ohpl_pcm_destroy_shader(mpRs, apShader); }
		void DestroyProgram(iGpuProgram * apProgram){ ohpl_pcm_destroy_program(mpRs, apProgram); }

		iGpuProgram* CreateProgramFromShaders(	const tString &asProgramName,
												const tString &asVtxShaderName, const tString &asFragShaderName,
												cParserVarContainer *apVars,bool abAddtoList);
		iGpuProgram* CreateProgramFromShaders(	const tString &asProgramName, iGpuShader *apVtxShader,iGpuShader *apFragShader,
												bool abAddtoList){ return static_cast<iGpuProgram*>(ohpl_pcm_program_from_shaders(mpRs, (const unsigned char*)asProgramName.data(), asProgramName.size(), apVtxShader, apFragShader, abAddtoList)); }

		void DestroyShadersAndPrograms(){ ohpl_pcm_destroy_all(mpRs); }

	private:
		void *mpRs;
	};

};
#endif // HPL_PROGRAM_COMBO_MANAGER_H
