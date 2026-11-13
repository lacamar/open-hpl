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

#include "impl/GLSLProgram.h"
#include "system/EngineDiagnostics.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"

#include "impl/SDLTexture.h"
#include "impl/GLSLShader.h"
#include "impl/LowLevelGraphicsSDL.h"




namespace hpl{

	//-----------------------------------------------------------------------

	int cGLSLProgram::mlCurrentProgram =0;

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cGLSLProgram::cGLSLProgram(const tString& asName) : iGpuProgram(asName,eGpuProgramFormat_GLSL)
	{
		mlRs = ohpl_glprog_new(&mlHandle);
	}

	//-----------------------------------------------------------------------

	cGLSLProgram::~cGLSLProgram()
	{
		cGLSLShader* pRsShader0 = static_cast<cGLSLShader*>(mpShader[0]);
		cGLSLShader* pRsShader1 = static_cast<cGLSLShader*>(mpShader[1]);
		ohpl_glprog_free(mlRs, pRsShader0 ? (int64_t)pRsShader0->GetHandle() : -1, pRsShader1 ? (int64_t)pRsShader1->GetHandle() : -1);
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	bool cGLSLProgram::Link()
	{
		for(int i=0; i<2; ++i)
		{
			cGLSLShader* pRsShader = static_cast<cGLSLShader*>(mpShader[i]);
			if(pRsShader) ohpl_glprog_attach(mlRs, pRsShader->GetHandle());
		}

		unsigned long lRsLinkStart = cPlatform::GetApplicationTime();
		bool bRsLinked = ohpl_glprog_link(mlRs);
		cEngineDiagnostics::AddLinkTime((double)(cPlatform::GetApplicationTime() - lRsLinkStart));

		{
			tString sInfoLog;
			size_t lLogLen = 0;
			const unsigned char *pLog = ohpl_glprog_log(mlRs, &lLogLen);
			if(lLogLen) sInfoLog.assign((const char*)pLog, lLogLen);
			cEngineDiagnostics::ReportShader(msName, "link", bRsLinked, sInfoLog);
		}
		if(bRsLinked == false)
		{
			Error("Failed to link GLSL program %s\n",msName.c_str());
			LogProgramInfoLog();
			return false;
		}

		for(eGpuShaderType type : {eGpuShaderType_Vertex, eGpuShaderType_Fragment})
		{
			iGpuShader* pShader = mpShader[type];
			if(pShader == NULL || pShader->GetSamplerUnitNum() == 0) continue;
			ohpl_glprog_use(mlRs);

			for(int i=0; i<pShader->GetSamplerUnitNum(); ++i)
			{
				tString &sSamplerName = pShader->GetSamplerUnit(i)->msName;
				ohpl_glprog_link_sampler(mlRs, sSamplerName.c_str(), pShader->GetSamplerUnit(i)->mlUnit);
			}

			ohpl_glprog_use_current(mlRs);
		}

		return true;
	}

	//-----------------------------------------------------------------------

	void cGLSLProgram::Bind()
	{
		ohpl_glprog_bind(mlRs);
	}

	//-----------------------------------------------------------------------

	void cGLSLProgram::UnBind()
	{
		ohpl_glprog_unbind();
	}


	//-----------------------------------------------------------------------

	tString cGLSLProgram::DumpUniforms()
	{
		tString sOut;
		int lRsCount = ohpl_glprog_uniform_count(mlRs);
		for(int i=0; i<lRsCount; ++i)
		{
			char sName[128]; float vVal[16];
			int lN = ohpl_glprog_uniform_at(mlRs, i, &sName, &vVal);
			sOut += sName;
			for(int j=0; j<lN; ++j) sOut += " " + cString::ToString(vVal[j]);
			sOut += "\n";
		}
		return sOut;
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetSamplerToUnit(const tString& asSamplerName, int alUnit)
	{
		return ohpl_glprog_set_sampler(mlRs, asSamplerName.c_str(), alUnit);
	}

	//-----------------------------------------------------------------------

	int cGLSLProgram::GetVariableId(const tString& asName)
	{
		return ohpl_glprog_variable_id(mlRs, asName.c_str(), asName.size());
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::GetVariableAsId(const tString& asName, int alId)
	{
		return ohpl_glprog_variable_as_id(mlRs, asName.c_str(), asName.size(), alId);
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetInt(int alVarId, int alX)
	{
		return ohpl_glprog_set_int(mlRs, alVarId, alX);
	}

	//-----------------------------------------------------------------------


	bool cGLSLProgram::SetFloat(int alVarId, float afX)
	{
		return ohpl_glprog_set_float(mlRs, alVarId, afX);
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetFloatArray(int alVarId, const float *apX, int alCount)
	{
		return ohpl_glprog_set_float_array(mlRs, alVarId, apX, alCount);
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetVec2f(int alVarId, float afX,float afY)
	{
		return ohpl_glprog_set_vec2(mlRs, alVarId, afX, afY);
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetVec3f(int alVarId, float afX,float afY,float afZ)
	{
		return ohpl_glprog_set_vec3(mlRs, alVarId, afX, afY, afZ);
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetVec4f(int alVarId, float afX,float afY,float afZ, float afW)
	{
		return ohpl_glprog_set_vec4(mlRs, alVarId, afX, afY, afZ, afW);
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetMatrixf(int alVarId, const cMatrixf& aMtx)
	{
		return ohpl_glprog_set_matrix(mlRs, alVarId, aMtx.v);
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetMatrixf(int alVarId, eGpuShaderMatrix aType, 
									eGpuShaderMatrixOp aOp)
	{
		//Not needed for GLSL
		return false;
	}

		
	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cGLSLProgram::LogProgramInfoLog()
	{
		size_t lRsLen = 0;
		const unsigned char *pRsLog = ohpl_glprog_dump_log(mlRs, &lRsLen);
		if(pRsLog)
		{
			Log("-------------\n");
			Log("%s\n",(const char*)pRsLog);
			Log("-------------\n");
		}
	}
	
	//-----------------------------------------------------------------------

}
