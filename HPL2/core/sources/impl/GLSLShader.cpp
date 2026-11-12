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

#include "impl/GLSLShader.h"
#include "impl/RustGl.h"
#include "impl/RustBackends.h"

#include "system/EngineDiagnostics.h"
#include "impl/SDLTexture.h"
#include "impl/LowLevelGraphicsSDL.h"
#include "system/LowLevelSystem.h"

#include "system/Platform.h"
#include "system/String.h"

#ifdef WIN32
#include <io.h>
#endif

namespace hpl{

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cGLSLShader::cGLSLShader(const tString& asName,eGpuShaderType aType, iLowLevelGraphics *apLowLevelGraphics) 
				: iGpuShader(asName, _W(""), aType, eGpuProgramFormat_GLSL)
	{
		;

		mpLowLevelGraphics = apLowLevelGraphics;
		
		mlRs = OhplBackendRustRequested() ? ohpl_glsh_new(aType, &mlHandle) : 0;
		if(mlRs == 0) mlHandle = glCreateShader(GetGLShaderType(aType));
	}

	cGLSLShader::~cGLSLShader()
	{
		if(mlRs)
		{
			ohpl_glsh_free(mlRs);
			return;
		}

		glDeleteShader(mlHandle);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	bool cGLSLShader::Reload(){return false;}
	void cGLSLShader::Unload(){}
	void cGLSLShader::Destroy(){}
	
	//-----------------------------------------------------------------------

	bool cGLSLShader::CreateFromFile(const tWString &asFile, const tString &asEntry, bool abPrintInfoIfFail)
	{
		///////////////////////////////////////
		//Load file
		FILE *pFile = cPlatform::OpenFile(asFile, _W("rb"));
		if(pFile == NULL)
		{
			Error("Could not open file %ls\n", asFile.c_str());
			return false;
		}
		SetFullPath(asFile);
		
		fseek(pFile,0,SEEK_END);
		int lFileSize = ftell(pFile);
		rewind(pFile);

		char *pBuffer = (char*) hplMalloc(sizeof(char)* lFileSize+1);

		fread(pBuffer, sizeof(GLchar), lFileSize, pFile);
		pBuffer[lFileSize] =0; //Zero at end so it is a proper string.
		
		fclose(pFile);

        bool bRet = CreateFromString(pBuffer, asEntry, abPrintInfoIfFail);

		hplFree(pBuffer);
		
		return bRet;
	}

	//-----------------------------------------------------------------------


	bool cGLSLShader::CreateFromString(const char *apStringData, const tString& asEntry, bool abPrintInfoIfFail)
	{
		;

		///////////////////////////////////////
		//Compile

		//const char *pConstBuffer = apStringData;
		//glShaderSource(mlHandle, 1, &pConstBuffer, NULL);
		if(const char *pDumpDir = getenv("OPENHPL_DUMP_SHADERS_DIR"))
		{
			static int lDumpCount = 0;
			tString sPath = tString(pDumpDir) + "/";
			ohpl_gl_shader_dump_name(lDumpCount++, msName.data(), msName.size(), mShaderType == eGpuShaderType_Vertex,
				[](void *apOut, const unsigned char *apData, size_t alLen){ static_cast<tString*>(apOut)->append((const char*)apData, alLen); }, &sPath);
			if(FILE *pFile = fopen(sPath.c_str(), "wb")) { fputs(apStringData, pFile); fclose(pFile); }
		}
		GLint lStatus;
		if(mlRs)
		{
			tString sInfoLog;
			lStatus = ohpl_glsh_compile(mlRs, apStringData) ? GL_TRUE : GL_FALSE;
			size_t lLogLen = 0;
			const unsigned char *pLog = ohpl_glsh_log(mlRs, &lLogLen);
			if(lLogLen) sInfoLog.assign((const char*)pLog, lLogLen);
			cEngineDiagnostics::ReportShader(msName, "compile", lStatus != GL_FALSE, sInfoLog);
		}
		else
		{
		glShaderSource(mlHandle, 1, &apStringData, NULL);

		glCompileShader(mlHandle);

		///////////////////////////////////////
		//Check for errors.
		glGetShaderiv(mlHandle,GL_COMPILE_STATUS,&lStatus);
		{
			tString sInfoLog;
			GLint lLogLen = 0;
			glGetShaderiv(mlHandle, GL_INFO_LOG_LENGTH, &lLogLen);
			if(lStatus == GL_FALSE && lLogLen > 1)
			{
				sInfoLog.resize(lLogLen);
				glGetShaderInfoLog(mlHandle, lLogLen, NULL, &sInfoLog[0]);
			}
			cEngineDiagnostics::ReportShader(msName, "compile", lStatus != GL_FALSE, sInfoLog);
		}
		}
		if(lStatus == GL_FALSE)
		{
			if(abPrintInfoIfFail)
			{
				Error("Failed to compile GLSL shader '%s'!\n",cString::To8Char(GetFullPath()).c_str());
				Log("Shader code:\n-------------------\n");
				
				LogShaderCode(apStringData);
	            
				
				Log("---------------------\n");
				Log("Compile log:\n");
				LogShaderInfoLog();
			}
			return false;
		}
		
		/////////////////////////////
		//If debug is on, then show code + compile log
		if(mbDebugInfo)
		{
			Log("Shader '%s' code:\n-------------------\n", msName.c_str());

			//LogShaderCode(apStringData);

			//Log("---------------------\n");
			Log("Compile log:\n");
			LogShaderInfoLog();
		}


		return true;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cGLSLShader::LogShaderInfoLog()
	{
		if(mlRs)
		{
			size_t lLen = 0;
			const unsigned char *pLog = ohpl_glsh_dump_log(mlRs, &lLen);
			if(pLog)
			{
				Log("---------------------\n");
				Log("%s\n",(const char*)pLog);
				Log("---------------------\n");
			}
			return;
		}

		;

		GLint infologLength = 0;
		GLsizei charsWritten  = 0;
		char *infoLog;

		glGetShaderiv(mlHandle, GL_INFO_LOG_LENGTH,&infologLength);

		if (infologLength > 0)
		{
			infoLog = (char *)hplMalloc(infologLength);
			glGetShaderInfoLog(mlHandle, infologLength, &charsWritten, infoLog);
			Log("---------------------\n");
			Log("%s\n",infoLog);
			Log("---------------------\n");
			hplFree(infoLog);
		}
	}

	//-----------------------------------------------------------------------

	static void LogShaderLine(void *apCtx, int alRow, const char *apLine, size_t alLen)
	{
		Log("[%04d] %s\n",alRow, std::string(apLine, alLen).c_str());
	}

	void cGLSLShader::LogShaderCode(const char *apStringData)
	{
		ohpl_gl_shader_code_lines(apStringData, LogShaderLine, NULL);
	}

	//-----------------------------------------------------------------------

	GLenum cGLSLShader::GetGLShaderType(eGpuShaderType aType)
	{
		return ohpl_gl_shader_type(aType);
	}
	
	//-----------------------------------------------------------------------

}
