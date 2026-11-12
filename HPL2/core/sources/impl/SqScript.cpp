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

#include "impl/SqScript.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"
#include "math/Math.h"
#include <stdio.h>
#include "impl/scripthelper.h"
#include "resources/BinaryBuffer.h"
#include "resources/Resources.h"
#include "impl/RustL7.h"
#include "impl/scriptarray.h"
#include "impl/scriptstring.h"
#include "../../../../cshim/as_shim.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC DATA
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	#define kEncryptKey 0x4516FFDD

	static void AssignSink(void *apCtx, const char *apData, size_t alLen)
	{
		((tString*)apCtx)->assign(apData, alLen);
	}

	static void ChunkSink(void *apCtx, const char *apData, size_t alLen)
	{
		((std::vector<tString>*)apCtx)->push_back(tString(apData, alLen));
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSqScript::cSqScript(const tString& asName,asIScriptEngine *apScriptEngine,
							cScriptOutput *apScriptOutput, int alHandle, OhplL7Engine *apRustEngine)
		: iScript(asName, _W(""))
	{
		mpScriptEngine = apScriptEngine;
		mpScriptOutput = apScriptOutput;
		mlHandle = alHandle;

		mpRs = NULL;
		mpContext = NULL;
		mpModule = NULL;
		if(apRustEngine==NULL) mpContext = mpScriptEngine->CreateContext();

		//Create a unique module name
		int lRand = cMath::RandRectl(0,1000000);
		msModuleName = "Module_"+cString::ToString(lRand)+
						"_"+cString::ToString(mlHandle);
		if(apRustEngine) mpRs = ohpl_l7_script_new(apRustEngine, mlHandle, lRand);

	}

	cSqScript::~cSqScript()
	{
		if(mpRs)
		{
			ohpl_l7_script_free(mpRs);
			return;
		}
		mpScriptEngine->DiscardModule(msModuleName.c_str());
		mpContext->Release();
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cSqScript::CreateFromFile(const tWString& asFileName, tString *apCompileMessages)
	{
		SetFullPath(asFileName);

		tWString sExt = cString::ToLowerCaseW(cString::GetFileExtW(asFileName));

		/////////////////////////////////////////
		// Load file
		int lLength;
		char *pCharBuffer = NULL;
		
		/////////////////////////////////////
		// Normal load
		if(sExt == _W("hps"))
		{
			pCharBuffer = LoadCharBuffer(asFileName,lLength);
			if(pCharBuffer==NULL){
				Error("Couldn't load script '%s'!\n",asFileName.c_str());
				return false;
			}
		}
		/////////////////////////////////////
		// Compressed load
		else if(cResources::GetCreateAndLoadCompressedMaps())
		{
			cBinaryBuffer compBuffer;
			if(compBuffer.Load(asFileName)==false)
			{
				//Log("Could not load compressed map!\n");
				return false;
			}

			int lKey = kEncryptKey;
			compBuffer.XorTransform((char*)&lKey, sizeof(lKey));

			cBinaryBuffer textBuff;
			if(textBuff.DecompressAndAddFromBuffer(&compBuffer, false)==false)
			{
				//Log("Could not decompress map!\n");
				return false;
			}

			textBuff.SetPos(0);
			lLength = textBuff.GetSize();
			pCharBuffer = hplNewArray(char,lLength);
			textBuff.GetCharArray(pCharBuffer, lLength);
		}

		/////////////////////////////////////////
		// Save compressed
		if(sExt == _W("hps") && cResources::GetCreateAndLoadCompressedMaps())
		{
			tWString sCompFile = cString::SetFileExtW(asFileName,_W("chps"));

			//Only recreate if file does not exist or if out of date.
			if(	cPlatform::FileExists(sCompFile)==false || 
				cPlatform::FileModifiedDate(sCompFile) < cPlatform::FileModifiedDate(asFileName))
			{
				cBinaryBuffer textBuff;
				textBuff.AddCharArray(pCharBuffer, lLength);

				cBinaryBuffer compBuff;
				compBuff.CompressAndAdd(textBuff.GetDataPointer(), textBuff.GetSize());

				int lKey = kEncryptKey;
				compBuff.XorTransform((char*)&lKey, sizeof(lKey));

				compBuff.Save(sCompFile);
			}
		}
		
		/////////////////////////////////////////
		// Create module
		if(mpRs)
		{
			std::vector<tString> vChunks;
			int lRes = ohpl_l7_script_build(mpRs, pCharBuffer, lLength, apCompileMessages ? AssignSink : NULL, apCompileMessages,
											ChunkSink, &vChunks);
			if(lRes==1)
			{
				Error("Couldn't add script '%s'!\n",asFileName.c_str());
			}
			else if(lRes==2)
			{
				Error("Couldn't build script '%s'!\n",cString::To8Char(asFileName).c_str());
				Log("------- SCRIPT OUTPUT BEGIN --------------------------\n");
				for(size_t i=0; i<vChunks.size(); ++i) Log(vChunks[i].c_str());
				Log("------- SCRIPT OUTPUT END ----------------------------\n");
			}
			hplDeleteArray(pCharBuffer);
			return lRes==0;
		}
		mpModule = mpScriptEngine->GetModule(msModuleName.c_str(), asGM_ALWAYS_CREATE);
		if(mpModule->AddScriptSection("main", pCharBuffer, lLength)<0)
		{
			Error("Couldn't add script '%s'!\n",asFileName.c_str());
			hplDeleteArray(pCharBuffer);
			return false;
		}

		int lBuildOutput = mpModule->Build();
		if(apCompileMessages) *apCompileMessages = mpScriptOutput->GetMessage();

		if(lBuildOutput<0)
		{
			Error("Couldn't build script '%s'!\n",cString::To8Char(asFileName).c_str());
			Log("------- SCRIPT OUTPUT BEGIN --------------------------\n");
			mpScriptOutput->Display();
			mpScriptOutput->Clear();
			Log("------- SCRIPT OUTPUT END ----------------------------\n");
			
			hplDeleteArray(pCharBuffer);
			return false;
		}
		mpScriptOutput->Clear();

		hplDeleteArray(pCharBuffer);
		return true;
	}

	//-----------------------------------------------------------------------

	int cSqScript::GetFuncHandle(const tString& asFunc)
	{
		if(mpRs) return ohpl_l7_script_func_handle(mpRs, asFunc.c_str());
		asIScriptFunction *pFunc = mpModule->GetFunctionByName(asFunc.c_str());
		if(pFunc==NULL) return -1;

		mvFuncHandles.push_back(pFunc);
		return (int)mvFuncHandles.size()-1;
	}

	//-----------------------------------------------------------------------

	void cSqScript::AddArg(const tString& asArg)
	{

	}

	//-----------------------------------------------------------------------

	bool cSqScript::Run(const tString& asFuncLine)
	{
		if(mpRs) return ohpl_l7_script_run_line(mpRs, asFuncLine.c_str());
		ExecuteString(mpScriptEngine, asFuncLine.c_str(), mpModule);

		return true;
	}

	//-----------------------------------------------------------------------

	bool cSqScript::Run(int alHandle)
	{
		if(mpRs) return ohpl_l7_script_run_handle(mpRs, alHandle);
		if(alHandle<0 || alHandle>=(int)mvFuncHandles.size()) return false;

		mpContext->Prepare(mvFuncHandles[alHandle]);

		/* Set all the args here */

		mpContext->Execute();

		return true;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	char* cSqScript::LoadCharBuffer(const tWString& asFileName, int& alLength)
	{
		FILE *pFile = cPlatform::OpenFile(asFileName, _W("rb"));
		if(pFile==NULL){
			return NULL;
		}

		fseek(pFile,0,SEEK_END);
		int lLength = (int)ftell(pFile);
		rewind(pFile);
		
		alLength = lLength;

		char *pBuffer = hplNewArray(char,lLength);
		fread(pBuffer, lLength, 1, pFile);

		fclose(pFile);

		return pBuffer;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// STATIC PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

}

// AngelScript add-ons reached from the Rust side through the shim (cshim/as_shim.h)
static void AddonsRegister(void* e)
{
	asIScriptEngine* pEngine = static_cast<asIScriptEngine*>(e);
	RegisterScriptString(pEngine);
	RegisterScriptArray(pEngine, true);
}

static int AddonsExecuteString(void* e, const char* code, void* module)
{
	return ExecuteString(static_cast<asIScriptEngine*>(e), code, static_cast<asIScriptModule*>(module));
}

static void* AddonsStrNew(const char* p, size_t n) { return new CScriptString(p, (unsigned)n); }
static unsigned AddonsArraySize(const void* a) { return static_cast<const CScriptArray*>(a)->GetSize(); }
static void* AddonsArrayAt(void* a, unsigned i) { return static_cast<CScriptArray*>(a)->At(i); }

void OhplL7InstallAddons()
{
	static const OhplAsAddons kAddons = {AddonsRegister, AddonsExecuteString, AddonsStrNew, AddonsArraySize, AddonsArrayAt};
	ohpl_as_set_addons(&kAddons);
}
