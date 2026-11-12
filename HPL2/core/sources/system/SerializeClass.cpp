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

#include "system/SerializeClass.h"

#include "system/LowLevelSystem.h"

#include "system/String.h"
#include "system/Platform.h"
#include "system/RustSystem.h"
#include "system/RustH1.h"
#include "impl/RustBackends.h"

#include "graphics/GraphicsTypes.h"
#include "math/MathTypes.h"
#include "system/Container.h"

#include "resources/BinaryBuffer.h"

#define ZLIB_WINAPI
#include <zlib.h>
#include <stdio.h>
#include <new>
#include <cstddef>

namespace hpl {

	#define kSavedDataCRCKey (0x12AD11A1)

	static_assert(sizeof(cSerializeMemberField) == 96 && offsetof(cSerializeMemberField, msClassName) == 32 &&
				  offsetof(cSerializeMemberField, mlOffset) == 64 && offsetof(cSerializeMemberField, mType) == 80 &&
				  offsetof(cSerializeMemberField, mlArraySize) == 88,
				  "cSerializeMemberField is mirrored by CppMemberField in rust/src/system/h1.rs");

	static_assert(sizeof(cVector2l) == 8 && sizeof(cVector2f) == 8 && sizeof(cVector3l) == 12 &&
				  sizeof(cVector3f) == 12 && sizeof(cMatrixf) == 64 && sizeof(cColor) == 16 &&
				  sizeof(cRect2l) == 16 && sizeof(cRect2f) == 16 && sizeof(cPlanef) == 16 &&
				  sizeof(wchar_t) == 4 && sizeof(bool) == 1 && sizeof(int) == 4,
				  "serialized value layouts are mirrored in rust/src/system/serialize.rs");

	//-----------------------------------------------------------------------

	cSerializeSavedClass::cSerializeSavedClass(const char* asName,const char *asParent,
							cSerializeMemberField* apMemberFields, size_t alSize,
							iSerializable* (*apCreateFunc)())
	{
		msName = asName;
		msParentName = asParent;
		mpMemberFields = apMemberFields;
		mlSize = alSize;
		mpCreateFunc = apCreateFunc;
	}

	//-----------------------------------------------------------------------

	cSerializeSavedClass gvSerializeTempClasses[kMaxSerializeClasses];
	int glSerializeTempClassesNum =0;

	bool cSerializeClass::mbDataSetup = false;

	cSerializeClass::cSerializeClass(const char* asName,const char* asParent,
									cSerializeMemberField* apMemberFields,size_t alSize,
									iSerializable* (*apCreateFunc)())
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h1_ser_defer(asName, asParent, apMemberFields, alSize, reinterpret_cast<void*(*)()>(apCreateFunc));
			return;
		}
		gvSerializeTempClasses[glSerializeTempClassesNum] = cSerializeSavedClass(asName,asParent,
													apMemberFields,alSize,apCreateFunc);

		glSerializeTempClassesNum++;
	}

	//-----------------------------------------------------------------------

	void cSerializeClass::SetUpData()
	{
		if(mbDataSetup) return;

		mbDataSetup = true;

		if(OhplBackendRustRequested())
		{
			ohpl_h1_ser_flush();
			return;
		}

		for(int i=0; i< glSerializeTempClassesNum; i++)
		{
			cSerializeSavedClass &saved = gvSerializeTempClasses[i];
			std::vector<OhplSerField> vFields;
			for(cSerializeMemberField *pField = saved.mpMemberFields; ; ++pField)
			{
				OhplSerField field = { pField->msName.c_str(), pField->msClassName.c_str(), pField->mlOffset,
										pField->mlSize, pField->mType, pField->mMainType, pField->mlArraySize };
				vFields.push_back(field);
				if(pField->mType == eSerializeType_NULL) break;
			}
			ohpl_ser_register(saved.msName, saved.msParentName, vFields.data(), vFields.size(), saved.mlSize,
							  reinterpret_cast<void*(*)()>(saved.mpCreateFunc));
		}
	}

	//-----------------------------------------------------------------------

	const OhplSerHost* cSerializeClass::GetHost()
	{
		static const OhplSerHost host = {
			[](int alLevel, const uint8_t *apMsg, size_t alLen) {
				tString sMsg((const char*)apMsg, alLen);
				if(alLevel == 0) Log("%s", sMsg.c_str());
				else if(alLevel == 1) Warning("%s", sMsg.c_str());
				else Error("%s", sMsg.c_str());
			},
			[](void *apObj, void (*apSink)(void*, const uint8_t*, size_t), void *apCtx) {
				tString sName = static_cast<iSerializable*>(apObj)->Serialize_GetTopClass();
				apSink(apCtx, (const uint8_t*)sName.data(), sName.size());
			},
			[](void *apObj) { iSerializable *pObj = static_cast<iSerializable*>(apObj); hplDelete(pObj); },
			[](unsigned alType, size_t alSize) -> void* {
				void *pData = hplMalloc(alSize);
				if(alType == eSerializeType_String) new(pData) tString();
				else if(alType == eSerializeType_WString) new(pData) tWString();
				return pData;
			},
			[](unsigned alType, void *apData) {
				if(alType == eSerializeType_String) static_cast<tString*>(apData)->~tString();
				else if(alType == eSerializeType_WString) static_cast<tWString*>(apData)->~tWString();
				hplFree(apData);
			},
			[](void *apStr, size_t *apLen) -> const uint8_t* {
				const tString &sStr = *static_cast<tString*>(apStr);
				*apLen = sStr.size();
				return (const uint8_t*)sStr.data();
			},
			[](void *apStr, const uint8_t *apData, size_t alLen) {
				static_cast<tString*>(apStr)->assign((const char*)apData, alLen);
			},
			[](void *apStr, size_t *apLen) -> const uint32_t* {
				const tWString &sStr = *static_cast<tWString*>(apStr);
				*apLen = sStr.size();
				return (const uint32_t*)sStr.data();
			},
			[](void *apStr, const uint32_t *apData, size_t alLen) {
				static_cast<tWString*>(apStr)->assign((const wchar_t*)apData, alLen);
			},
			sizeof(tString),
			sizeof(tWString),
			[](void *apCont) -> size_t { return static_cast<iContainer*>(apCont)->Size(); },
			[](void *apCont) { static_cast<iContainer*>(apCont)->Clear(); },
			[](void *apCont, void *apPtr) { static_cast<iContainer*>(apCont)->AddVoidPtr(&apPtr); },
			[](void *apCont, void *apElem) { static_cast<iContainer*>(apCont)->AddVoidClass(apElem); },
			[](void *apCont) -> void* { return static_cast<iContainer*>(apCont)->CreateIteratorPtr(); },
			[](void *apIt) -> bool { return static_cast<iContainerIterator*>(apIt)->HasNext(); },
			[](void *apIt) -> void* { return static_cast<iContainerIterator*>(apIt)->NextPtr(); },
			[](void *apIt) { iContainerIterator *pIt = static_cast<iContainerIterator*>(apIt); hplDelete(pIt); },
		};
		return &host;
	}

	//-----------------------------------------------------------------------

	bool cSerializeClass::SaveToFile(iSerializable* apData, const tWString &asFile,const tString &asRoot, bool abCompressAndCrc)
	{
		SetUpData();

		OhplXmlDocPtr xmlDoc;
		ohpl_ser_save(GetHost(), xmlDoc.p, apData, asRoot.c_str());

		///////////////////////////////
		//Normal Save
		if(abCompressAndCrc==false)
		{
			FILE *pFile = cPlatform::OpenFile(asFile, _W("w+"));
			if(pFile==NULL)
			{
				Error("Unable to open serialized file '%s' as w+! Invalid filepointer returned!\n", cString::To8Char(asFile).c_str());
				return false;
			}

			// Enable buffered writing to disk
			char buffer[8*1024];
			setvbuf(pFile, buffer, _IOFBF, 8*1024);

			bool bRet = OhplXmlSaveFile(xmlDoc.p, pFile);
			if(bRet==false)
				Error("Couldn't save class to '%s'\n", asFile.c_str());

			if(pFile) fclose(pFile);

			return bRet;
		}
		///////////////////////////////
		//Compressed Save
		else
		{
			/////////////////////////////
			// Get the data
			tString sData;
			ohpl_xml_doc_print_stream(xmlDoc.p, OhplXmlAssign, &sData);

			/////////////////////////////
			// Compress the data
			cBinaryBuffer destBuffer;

			destBuffer.AddCRC_Begin();

			if(destBuffer.CompressAndAdd(&sData[0], sData.size()+1)==false)
			{
				Error("Unable to compress data for serialized data '%s'!\n", cString::To8Char(asFile).c_str());
				return false;
			}

			destBuffer.AddCRC_End(kSavedDataCRCKey);

			/////////////////////////////
			// Save the file
			if(destBuffer.Save(asFile)==false)
			{
				Error("Unable to save serialized file '%s'!\n", cString::To8Char(asFile).c_str());
				return false;
			}

			return true;
		}
	}

	//-----------------------------------------------------------------------

	bool cSerializeClass::LoadFromFile(iSerializable* apData, const tWString &asFile, bool abCompressedAndCrc)
	{
		SetUpData();

		OhplXmlDocPtr xmlDoc;

		///////////////////////////////
		//Normal Load
		if(abCompressedAndCrc==false)
		{
			#ifdef WIN32
				FILE *pFile = _wfopen(asFile.c_str(),_W("rb"));
			#else
				FILE *pFile = fopen(cString::To8Char(asFile).c_str(),"rb");
			#endif

			if(pFile==NULL)
			{
				Error("Unable to open serialized file '%s' as rb! Invalid filepointer returned!\n", cString::To8Char(asFile).c_str());
				return false;
			}
			if(OhplXmlLoadFile(xmlDoc.p, pFile)==false)
			{
				tString sDesc;
				int lRow, lCol;
				ohpl_xml_doc_error_info(xmlDoc.p, &lRow, &lCol, OhplXmlAssign, &sDesc);
				Error("Couldn't load saved class file '%s' from %s!\n",
										cString::To8Char(asFile).c_str(),
										sDesc.c_str());
				if(pFile) fclose(pFile);
				return false;
			}
			if(pFile) fclose(pFile);
		}
		///////////////////////////////
		//Compressed Load
		else
		{
			////////////////////////////////
			// Get compressed data
			cBinaryBuffer compBuffer;
			if(compBuffer.Load(asFile)==false)
			{
				Error("Unable to open serialized file '%s'!\n", cString::To8Char(asFile).c_str());
				return false;
			}

			////////////////////////////////
			// Check CRC
			if(compBuffer.CheckInternalCRC(kSavedDataCRCKey)==false)
			{
				Error("CRC check for serialized file '%s' failed!\n", cString::To8Char(asFile).c_str());
				return false;
			}

			////////////////////////////////
			// Uncompress data
			cBinaryBuffer textBuffer;
			if(textBuffer.DecompressAndAddFromBuffer(&compBuffer, false)==false)
			{
				Error("Unable to decompress serialized data '%s'!\n", cString::To8Char(asFile).c_str());
				return false;
			}

			////////////////////
			//Load xml
			ohpl_xml_doc_parse(xmlDoc.p, textBuffer.GetDataPointer());
		}

		ohpl_ser_load(GetHost(), xmlDoc.p, apData);

		return true;
	}

	//-----------------------------------------------------------------------

}
