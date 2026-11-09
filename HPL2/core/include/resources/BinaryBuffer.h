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

#ifndef HPL_BINARY_BUFFER_H
#define HPL_BINARY_BUFFER_H

#include "system/SystemTypes.h"
#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"

#include <cstddef>
#include <cstdint>

extern "C" {
	struct OhplBinaryBuffer;
	OhplBinaryBuffer* ohpl_bb_new();
	void ohpl_bb_free(OhplBinaryBuffer* bb);
	void ohpl_bb_clear(OhplBinaryBuffer* bb);
	void ohpl_bb_assign(OhplBinaryBuffer* bb, const uint8_t* data, size_t len);
	uint8_t* ohpl_bb_data(OhplBinaryBuffer* bb);
	size_t ohpl_bb_size(const OhplBinaryBuffer* bb);
	size_t ohpl_bb_reserved(const OhplBinaryBuffer* bb);
	size_t ohpl_bb_pos(const OhplBinaryBuffer* bb);
	bool ohpl_bb_reserve(OhplBinaryBuffer* bb, size_t size);
	bool ohpl_bb_set_pos(OhplBinaryBuffer* bb, size_t pos);
	bool ohpl_bb_add_pos(OhplBinaryBuffer* bb, size_t n);
	void ohpl_bb_add_array(OhplBinaryBuffer* bb, const void* data, size_t elem, size_t count, bool per_elem);
	void ohpl_bb_get_array(OhplBinaryBuffer* bb, void* out, size_t elem, size_t count);
	bool ohpl_bb_get_bytes(OhplBinaryBuffer* bb, void* out, size_t len);
	const uint8_t* ohpl_bb_get_string(OhplBinaryBuffer* bb, size_t* len);
	bool ohpl_bb_compress_and_add(OhplBinaryBuffer* bb, const void* src, size_t len, int level, bool write_size);
	bool ohpl_bb_decompress_and_add(OhplBinaryBuffer* bb, const void* src, size_t len);
	bool ohpl_bb_decompress_from(OhplBinaryBuffer* dst, OhplBinaryBuffer* src, bool size_written);
	void ohpl_bb_xor(OhplBinaryBuffer* bb, const void* key, size_t len);
	void ohpl_bb_crc_begin(OhplBinaryBuffer* bb);
	uint32_t ohpl_bb_crc_end(OhplBinaryBuffer* bb, uint32_t key);
	uint32_t ohpl_bb_get_crc(const OhplBinaryBuffer* bb, uint32_t key, int pos, int count);
	bool ohpl_bb_check_internal_crc(OhplBinaryBuffer* bb, uint32_t key, int count);
	bool ohpl_bb_check_crc(const OhplBinaryBuffer* bb, uint32_t crc, uint32_t key, int count);
	void ohpl_bb_set_i32(OhplBinaryBuffer* bb, int32_t v, size_t pos);
}

namespace hpl {
	class cBinaryBuffer
	{
	public:
		cBinaryBuffer() : mpImpl(ohpl_bb_new()) {}
		cBinaryBuffer(const tWString& asFile) : msFile(asFile), mpImpl(ohpl_bb_new()) {}
		~cBinaryBuffer() { ohpl_bb_free(mpImpl); }
		cBinaryBuffer(const cBinaryBuffer&) = delete;
		cBinaryBuffer& operator=(const cBinaryBuffer&) = delete;
		

		////////////////////////////////
		// GENERAL
		////////////////////////////////

		/**
		 * Loads data from set file location. Will not work is location is not set! Loading clears any data set.
		 * \return true if loading ws ok, else false
		 */
		bool Load();
		/**
		 * Loads data from file.
		 * \return 
		 */
		bool Load(const tWString& asFile);
		/**
		 * Saves the data to set file location. Will not work is location is not set! Loading clears any data set.
		 * \return true if loading was ok, else false
		 */
		bool Save();
		/**
		 * Save data to file.
		 */
		bool Save(const tWString& asFile);

		/**
		 * Loads data from a readable text encoded data string.
		 */
		void LoadFromCharEncode(const tString& asInputData, size_t alSize);

		/**
		 * Saves current data to a readable text encoded string
		 */
		void SaveToCharEncode(tString& asOutputData);


		void SetFileLocation(const tWString& asFile){ msFile = asFile;}
		const tWString& GetFileLocation(){ return msFile;}

		/**
		* Reserves memory space, if reserving less than what the file already contains, then nothing happens.'
		* \return true if memory was found, else false
		*/
		bool Reserve(size_t alSize) { return ohpl_bb_reserve(mpImpl, alSize); }
		
		/**
		 * This deletes all data and resets size and pos.
		 */
		void Clear() { ohpl_bb_clear(mpImpl); }

		size_t GetSize(){ return ohpl_bb_size(mpImpl); }
		size_t GetReservedSize(){ return ohpl_bb_reserved(mpImpl); }

		char* GetDataPointer(){ return (char*)ohpl_bb_data(mpImpl); }
		char* GetDataPointerAtPos(size_t alPos){ return (char*)ohpl_bb_data(mpImpl) + alPos; }
		char* GetDataPointerAtCurrentPos(){ return (char*)ohpl_bb_data(mpImpl) + ohpl_bb_pos(mpImpl); }

		////////////////////////////////
		// FILE SEARCHING
		////////////////////////////////

		size_t GetPos()const { return ohpl_bb_pos(mpImpl); }
		bool SetPos(size_t alPos) { return ohpl_bb_set_pos(mpImpl, alPos); }
		bool AddPos(size_t alSize) { return ohpl_bb_add_pos(mpImpl, alSize); }

		/**
		 * If data position has reached end of file.
		 */
		bool IsEOF(){ return ohpl_bb_pos(mpImpl) >= ohpl_bb_size(mpImpl); }

		////////////////////////////////
		// COMPRESSION
		////////////////////////////////

		/**
		* Compresses the data from apSrcData to current pos. Compression level is 0 - 9, where 0 is no compression. -1 = default (recommended basically) compression.
		* If abWriteDataSize is true, then the first 4 bytes, will be a 32bit int with the size of the compressed data.
		*/
		bool CompressAndAdd(char *apSrcData, size_t alSize, int alCompressionLevel=-1, bool abWriteDataSize=false) { return ohpl_bb_compress_and_add(mpImpl, apSrcData, alSize, alCompressionLevel, abWriteDataSize); }

		/**
		* Decompresses the data from apSrcData to current pos.
		*/
		bool DecompressAndAdd(char *apSrcData, size_t alSize) { return ohpl_bb_decompress_and_add(mpImpl, apSrcData, alSize); }

		/**
		* Decompresses the data from a source buffer, beginning at the current position. If abSizeDataWritten is false, it stops at EOF and assumes size has not been writen. If true, it will assume that the first 4 bytes is the size of the data.
		* It also updates the current postion in the data for SrcBuffer
		*/
		bool DecompressAndAddFromBuffer(cBinaryBuffer *apSrcBuffer, bool abSizeDataWritten) { return ohpl_bb_decompress_from(mpImpl, apSrcBuffer->mpImpl, abSizeDataWritten); }

		////////////////////////////////
		// ENCRYPTION
		////////////////////////////////

		/**
		* Runs an XOR the entire buffer using simple XOR
		*/
		void XorTransform(const char* apKeyData, size_t alKeySize) { ohpl_bb_xor(mpImpl, apKeyData, alKeySize); }
		
		////////////////////////////////
		// TESTS
		////////////////////////////////

		/**
		* Do this when the data position is where the CRC should be stored and then any subsequent data (until AddCRC_End is called) will be part of the CRC calculation.
		* This will add a 32 bit int with the CRC that is then set when AddCRC_End is called.
		*/
		void AddCRC_Begin() { ohpl_bb_crc_begin(mpImpl); }
		
		/**
		* This must be called after AddCRC_Begin, and after some data has been added.
		*/
		unsigned int AddCRC_End(unsigned int alKey) { return ohpl_bb_crc_end(mpImpl, alKey); }
				
		/**
		* Calculates the CRC from current position (if alDataPos<0) or alDataPos, to end (if alCount < 0) or according to alCount
		*/
		unsigned int GetCRC(unsigned int alKey, int alDataPos=-1, int alCount=-1) { return ohpl_bb_get_crc(mpImpl, alKey, alDataPos, alCount); }

		/**
		* This checks CRC from current position to end (if alCount<0) else according to alCount (which includes the the first 32bit CRC)
		* It assumes that the first the 32bit int is the CRC and will increase the data position to be past these.
		*/
		bool CheckInternalCRC(unsigned int alKey, int alCount=-1) { return ohpl_bb_check_internal_crc(mpImpl, alKey, alCount); }

		/**
		* This checks CRC from current position to end (if alCount<0) else according to alCount
		*/
		bool CheckCRC(unsigned int alCRC, unsigned int alKey, int alCount=-1) { return ohpl_bb_check_crc(mpImpl, alCRC, alKey, alCount); }
		
		////////////////////////////////
		// DATA INPUT
		////////////////////////////////
		void AddChar(char alX) { ohpl_bb_add_array(mpImpl, &alX, 1, 1, false); }
		void AddUnsignedChar(unsigned char alX) { ohpl_bb_add_array(mpImpl, &alX, 1, 1, false); }
		void AddBool(bool abX) { char c = abX ? 1 : 0; ohpl_bb_add_array(mpImpl, &c, 1, 1, false); }
		void AddShort16(short alX) { ohpl_bb_add_array(mpImpl, &alX, 2, 1, true); }
		void AddUnsignedShort16(unsigned short alX) { ohpl_bb_add_array(mpImpl, &alX, 2, 1, true); }
		void AddInt32(int alX) { ohpl_bb_add_array(mpImpl, &alX, 4, 1, true); }
		void AddFloat32(float afX) { ohpl_bb_add_array(mpImpl, &afX, 4, 1, true); }
		void AddVector2f(const cVector2f& avX) { float t[2] = { avX.x, avX.y }; ohpl_bb_add_array(mpImpl, t, 4, 2, true); }
		void AddVector3f(const cVector3f& avX) { float t[3] = { avX.x, avX.y, avX.z }; ohpl_bb_add_array(mpImpl, t, 4, 3, true); }
		void AddVector2l(const cVector2l& avX) { int t[2] = { avX.v[0], avX.v[1] }; ohpl_bb_add_array(mpImpl, t, 4, 2, false); }
		void AddVector3l(const cVector3l& avX) { int t[3] = { avX.v[0], avX.v[1], avX.v[2] }; ohpl_bb_add_array(mpImpl, t, 4, 3, false); }
		void AddMatrixf(const cMatrixf& a_mtxX) { ohpl_bb_add_array(mpImpl, a_mtxX.v, 4, 16, true); }
		void AddQuaternion(const cQuaternion& aqX) { AddVector3f(aqX.v); AddFloat32(aqX.w); }
		void AddColor(const cColor& avX) { ohpl_bb_add_array(mpImpl, avX.v, 4, 4, true); }
		void AddString(const tString& asStr) { ohpl_bb_add_array(mpImpl, asStr.c_str(), 1, asStr.size() + 1, false); }
//      Must rethink how to store, as Mac/Linux is UTF-32 and Windows is UTF-16.  Better to store as UTF-8 on all platforms.
//      Also, must take care of Endianess for PPC
//		void AddStringW(const tWString& asStr);

		void AddCharArray(const char* apData, size_t alSize) { ohpl_bb_add_array(mpImpl, apData, 1, alSize, false); }
		void AddShort16Array(const short* apData, size_t alSize) { ohpl_bb_add_array(mpImpl, apData, 2, alSize, true); }
		void AddInt32Array(const int* apData, size_t alSize) { ohpl_bb_add_array(mpImpl, apData, 4, alSize, true); }
		void AddFloat32Array(const float* apData, size_t alSize) { ohpl_bb_add_array(mpImpl, apData, 4, alSize, true); }
		
        ////////////////////////////////
		// DATA UPDATE
		////////////////////////////////
        void SetInt32(int alX, size_t alPos) { ohpl_bb_set_i32(mpImpl, alX, alPos); }

		////////////////////////////////
		// DATA OUTPUT
		////////////////////////////////
		char GetChar() { char x = 0; ohpl_bb_get_array(mpImpl, &x, 1, 1); return x; }
		unsigned char GetUnsignedChar() { unsigned char x = 0; ohpl_bb_get_array(mpImpl, &x, 1, 1); return x; }
		bool GetBool() { char c = 0; ohpl_bb_get_array(mpImpl, &c, 1, 1); return c != 0; }
		short GetShort16() { short x = 0; ohpl_bb_get_array(mpImpl, &x, 2, 1); return x; }
		unsigned short GetUnsignedShort16() { unsigned short x = 0; ohpl_bb_get_array(mpImpl, &x, 2, 1); return x; }
		int GetInt32() { int x = 0; ohpl_bb_get_array(mpImpl, &x, 4, 1); return x; }
		float GetFloat32() { float x = 0; ohpl_bb_get_array(mpImpl, &x, 4, 1); return x; }
		void GetVector2f(cVector2f *apX) { float t[2]; ohpl_bb_get_array(mpImpl, t, 4, 2); apX->x = t[0]; apX->y = t[1]; }
		void GetVector3f(cVector3f *apX) { float t[3]; ohpl_bb_get_array(mpImpl, t, 4, 3); apX->x = t[0]; apX->y = t[1]; apX->z = t[2]; }
		void GetVector2l(cVector2l *apX) { int t[2]; if(ohpl_bb_get_bytes(mpImpl, t, sizeof(t))) { apX->v[0] = t[0]; apX->v[1] = t[1]; } }
		void GetVector3l(cVector3l *apX) { int t[3]; if(ohpl_bb_get_bytes(mpImpl, t, sizeof(t))) { apX->v[0] = t[0]; apX->v[1] = t[1]; apX->v[2] = t[2]; } }
		void GetMatrixf(cMatrixf *apX) { ohpl_bb_get_array(mpImpl, apX->v, 4, 16); }
		void GetQuaternion(cQuaternion* apX) { GetVector3f(&apX->v); apX->w = GetFloat32(); }
		void GetColor(cColor *apX) { ohpl_bb_get_array(mpImpl, apX->v, 4, 4); }
		void GetString( tString *apStr) { size_t n; const uint8_t* p = ohpl_bb_get_string(mpImpl, &n); apStr->assign((const char*)p, n); }
//		void GetStringW(tWString *apStr);

		void GetCharArray(char* apData, size_t alSize) { ohpl_bb_get_bytes(mpImpl, apData, alSize); }
		void GetShort16Array(short* apData, size_t alSize) { ohpl_bb_get_array(mpImpl, apData, 2, alSize); }
		void GetInt32Array(int* apData, size_t alSize) { ohpl_bb_get_array(mpImpl, apData, 4, alSize); }
		void GetFloat32Array(float* apData, size_t alSize) { ohpl_bb_get_array(mpImpl, apData, 4, alSize); }
					        		
	private:
		tWString msFile;
		OhplBinaryBuffer* mpImpl;
	};

};
#endif // HPL_BINARY_BUFFER_H
