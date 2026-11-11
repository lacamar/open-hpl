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
		;

		mlHandle = glCreateProgram();
		mpParams = ohpl_gl_params_new();

	}

	//-----------------------------------------------------------------------

	cGLSLProgram::~cGLSLProgram()
	{
		;

		for(int i=0; i<2; ++i)
		{
			cGLSLShader* pGLSLShader = static_cast<cGLSLShader*>(mpShader[i]);
			if(pGLSLShader)
			{
				glDetachShader(mlHandle,pGLSLShader->GetHandle());
			}
		}
		ohpl_gl_prog_forget(&mlCurrentProgram, mlHandle);
		glDeleteProgram(mlHandle);
		ohpl_gl_params_free(mpParams);
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	bool cGLSLProgram::Link()
	{
		;

		///////////////////////////////////////
		//Attach shaders
		for(int i=0; i<2; ++i)
		{
			cGLSLShader* pGLSLShader = static_cast<cGLSLShader*>(mpShader[i]);
			if(pGLSLShader)
			{
				glAttachShader(mlHandle,pGLSLShader->GetHandle());
			}
		}

		///////////////////////////////////////
		//Link 
		unsigned long lLinkStart = cPlatform::GetApplicationTime();
		glLinkProgram(mlHandle);
		cEngineDiagnostics::AddLinkTime((double)(cPlatform::GetApplicationTime() - lLinkStart));

		///////////////////////////////////////
		//Check for errors
		GLint lStatus;
		glGetProgramiv(mlHandle, GL_LINK_STATUS, &lStatus);
		{
			tString sInfoLog;
			GLint lLogLen = 0;
			glGetProgramiv(mlHandle, GL_INFO_LOG_LENGTH, &lLogLen);
			if(lStatus == GL_FALSE && lLogLen > 1)
			{
				sInfoLog.resize(lLogLen);
				glGetProgramInfoLog(mlHandle, lLogLen, NULL, &sInfoLog[0]);
			}
			cEngineDiagnostics::ReportShader(msName, "link", lStatus != GL_FALSE, sInfoLog);
		}
		if(lStatus == GL_FALSE)
		{
			Error("Failed to link GLSL program %s\n",msName.c_str());
			LogProgramInfoLog();
			return false;
		}

		///////////////////////////////////////
		//Set up sampler units
		for(eGpuShaderType type : {eGpuShaderType_Vertex, eGpuShaderType_Fragment})
		{
			iGpuShader* pShader = mpShader[type];
			if(pShader == NULL || pShader->GetSamplerUnitNum() == 0) continue;
			glUseProgram(mlHandle);
			
			for(int i=0; i<pShader->GetSamplerUnitNum(); ++i)
			{
				tString &sSamplerName = pShader->GetSamplerUnit(i)->msName;
				int lUnit = pShader->GetSamplerUnit(i)->mlUnit;
				
				GLint lVarHandle = glGetUniformLocation(mlHandle,sSamplerName.c_str());
				if(lVarHandle<0) continue; // inactive, optimised out by the driver
			
				glUniform1i(lVarHandle, lUnit);
			}

			glUseProgram(mlCurrentProgram);
		}
		
		return true;
	}

	//-----------------------------------------------------------------------

	void cGLSLProgram::Bind()
	{
		if(!ohpl_gl_prog_bind(&mlCurrentProgram, mlHandle)) return;
		glUseProgram(mlHandle);
	}

	//-----------------------------------------------------------------------

	void cGLSLProgram::UnBind()
	{
		if(!ohpl_gl_prog_unbind(&mlCurrentProgram)) return;
		glUseProgram(0);
	}


	//-----------------------------------------------------------------------

	tString cGLSLProgram::DumpUniforms()
	{
		tString sOut;
		GLint lCount = 0;
		glGetProgramiv(mlHandle, GL_ACTIVE_UNIFORMS, &lCount);
		for(GLint i=0; i<lCount; ++i)
		{
			char sName[128]; GLint lSize; GLenum type;
			glGetActiveUniform(mlHandle, i, sizeof(sName), NULL, &lSize, &type, sName);
			float vVal[16] = {0};
			glGetUniformfv(mlHandle, glGetUniformLocation(mlHandle, sName), vVal);
			int lN = ohpl_gl_uniform_components(type);
			sOut += sName;
			for(int j=0; j<lN; ++j) sOut += " " + cString::ToString(vVal[j]);
			sOut += "\n";
		}
		return sOut;
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetSamplerToUnit(const tString& asSamplerName, int alUnit)
	{
		;

		GLint lVarHandle = glGetUniformLocation(mlHandle,asSamplerName.c_str());
		if(lVarHandle <0) return false;

		Bind();
		glUniform1i(lVarHandle, alUnit);
		UnBind();

		return true;
	}

	//-----------------------------------------------------------------------

	int cGLSLProgram::GetVariableId(const tString& asName)
	{
		int lId = ohpl_gl_params_find(mpParams, asName.data(), asName.size());
		if(lId >= 0) return lId;

		GLint lLocation = glGetUniformLocation(mlHandle,asName.c_str());
		if(lLocation <0) return -1;

		return ohpl_gl_params_add(mpParams, asName.data(), asName.size(), lLocation);
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::GetVariableAsId(const tString& asName, int alId)
	{
		int lState = ohpl_gl_params_as_id_begin(mpParams, asName.data(), asName.size(), alId);
		if(lState != 2) return lState == 1;

		GLint lHandle = glGetUniformLocation(mlHandle,asName.c_str());
		return ohpl_gl_params_as_id_finish(mpParams, asName.data(), asName.size(), alId, lHandle);
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetInt(int alVarId, int alX)
	{
		int lLocation;
		if(!ohpl_gl_params_location(mpParams, alVarId, &lLocation)) return false;

		if(mlCurrentProgram != mlHandle) Bind();
		glUniform1i(lLocation, alX);
		return true;
	}

	//-----------------------------------------------------------------------


	bool cGLSLProgram::SetFloat(int alVarId, float afX)
	{
		int lLocation;
		if(!ohpl_gl_params_location(mpParams, alVarId, &lLocation)) return false;

		if(mlCurrentProgram != mlHandle) Bind();
		glUniform1f(lLocation, afX);
		return true;
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetFloatArray(int alVarId, const float *apX, int alCount)
	{
		int lLocation;
		if(!ohpl_gl_params_location(mpParams, alVarId, &lLocation)) return false;

		if(mlCurrentProgram != mlHandle) Bind();
		glUniform1fv(lLocation, alCount, apX);
		return true;
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetVec2f(int alVarId, float afX,float afY)
	{
		int lLocation;
		if(!ohpl_gl_params_location(mpParams, alVarId, &lLocation)) return false;

		if(mlCurrentProgram != mlHandle) Bind();
		glUniform2f(lLocation, afX, afY);
		return true;
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetVec3f(int alVarId, float afX,float afY,float afZ)
	{
		int lLocation;
		if(!ohpl_gl_params_location(mpParams, alVarId, &lLocation)) return false;

		if(mlCurrentProgram != mlHandle) Bind();
		glUniform3f(lLocation, afX, afY, afZ);
		return true;
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetVec4f(int alVarId, float afX,float afY,float afZ, float afW)
	{
		int lLocation;
		if(!ohpl_gl_params_location(mpParams, alVarId, &lLocation)) return false;

		if(mlCurrentProgram != mlHandle) Bind();
		glUniform4f(lLocation, afX, afY, afZ,afW);
		return true;
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetMatrixf(int alVarId, const cMatrixf& aMtx)
	{
		int lLocation;
		if(!ohpl_gl_params_location(mpParams, alVarId, &lLocation)) return false;

		if(mlCurrentProgram != mlHandle) Bind();
		glUniformMatrix4fv(lLocation, 1, true, aMtx.v);
		return true;
	}

	//-----------------------------------------------------------------------

	bool cGLSLProgram::SetMatrixf(int alVarId, eGpuShaderMatrix aType, 
									eGpuShaderMatrixOp aOp)
	{
		;

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
		;

		GLint infologLength = 0;
		GLsizei charsWritten  = 0;
		char *infoLog;

		glGetProgramiv(mlHandle, GL_INFO_LOG_LENGTH,&infologLength);

		if (infologLength > 0)
		{
			infoLog = (char *)hplMalloc(infologLength);
			glGetProgramInfoLog(mlHandle, infologLength, &charsWritten, infoLog);
			Log("-------------\n");
			Log("%s\n",infoLog);
			Log("-------------\n");
			hplFree(infoLog);
		}
	}
	
	//-----------------------------------------------------------------------

}
