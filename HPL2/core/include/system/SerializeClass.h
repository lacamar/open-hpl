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

#ifndef HPL_SERIALIZE_CLASS_H
#define HPL_SERIALIZE_CLASS_H

#include "system/SystemTypes.h"
#include "system/MemoryManager.h"
#include "system/RustH1.h"

namespace hpl {

	#define kSerializableClassInit(aClass) \
				public: \
				virtual tString Serialize_GetTopClass(){ return #aClass;}

	class iSerializable
	{
	public:
		virtual ~iSerializable(){}
		virtual tString Serialize_GetTopClass(){ return "";}
	};

	class cSerializeClass
	{
	public:
		cSerializeClass(const char* asName, const char* asParent, size_t alSize, iSerializable* (*apCreateFunc)())
		{
			ohpl_h1_ser_defer(asName, asParent, alSize, reinterpret_cast<void*(*)()>(apCreateFunc));
		}

		static bool SaveToFile(iSerializable* apData, const tWString &asFile, const tString &asRoot, bool abCompressAndCRC=false)
		{
			return ohpl_h1_ser_save_file(GetHost(), apData, asFile.data(), asFile.size(), asRoot.data(), asRoot.size(), abCompressAndCRC);
		}

		static const ::OhplSerHost* GetHost();

		static bool LoadFromFile(iSerializable* apData, const tWString &asFile, bool abCompressedAndCRC=false)
		{
			return ohpl_h1_ser_load_file(GetHost(), apData, asFile.data(), asFile.size(), abCompressedAndCRC);
		}
	};

	// Field tables and layouts are Rust-owned (rust/src/luxsave); C++ supplies the factory.
	#define kSerializeRustClass(aClass) kSerializeRustClassP(aClass, )

	#define kSerializeRustClassP(aClass, aParent) \
		static iSerializable *Create_##aClass() { return hplNew(aClass, ()); } \
		static cSerializeClass gSerializedClass_##aClass(#aClass, #aParent, sizeof(aClass), Create_##aClass);

	#define kSerializeRustClassV(aClass, aParent) \
		static cSerializeClass gSerializedClass_##aClass(#aClass, #aParent, sizeof(aClass), NULL);

};
#endif // HPL_SERIALIZE_CLASS_H
