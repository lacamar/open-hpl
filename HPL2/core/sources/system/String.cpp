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

#include "system/String.h"

#include <stdexcept>
#include <stdlib.h>
#include <string.h>

#include "system/RustString.h"

namespace hpl {

	namespace {
		template<class S> void Assign(void* apCtx, const typename S::value_type* apData, size_t alLen)
		{
			static_cast<S*>(apCtx)->assign(apData, alLen);
		}

		template<class S> void PushString(void* apCtx, const typename S::value_type* apData, size_t alLen)
		{
			static_cast<std::vector<S>*>(apCtx)->emplace_back(apData, alLen);
		}

		template<class S, class F> S Out(F aCall)
		{
			S sOut;
			aCall(Assign<S>, &sOut);
			return sOut;
		}

		const char* SepData(const tString* apSeparators) { return apSeparators ? apSeparators->data() : NULL; }
		size_t SepSize(const tString* apSeparators) { return apSeparators ? apSeparators->size() : 0; }
		const wchar_t* SepData(const tWString* apSeparators) { return apSeparators ? apSeparators->data() : NULL; }
		size_t SepSize(const tWString* apSeparators) { return apSeparators ? apSeparators->size() : 0; }
	}

	//-----------------------------------------------------------------------

	tWString cString::To16Char(const tString &asString)
	{
		if ( asString == "" ) return L"";

		tWString wsTemp;

		size_t needed = mbstowcs(NULL,&asString[0],0);
		wsTemp.resize(needed);
		mbstowcs(&wsTemp[0],&asString[0],needed);

		return wsTemp;
	}

	tString cString::To8Char(const tWString &awsString)
	{
		tString sTemp;

		size_t needed = wcstombs(NULL,&awsString[0],0);
		if (needed == (size_t)-1) {
			return "";
		}
		sTemp.resize(needed);
		wcstombs(&sTemp[0],&awsString[0],needed);

		return sTemp;
	}

	tWString cString::UTF8ToWChar(const tString& asString)
	{
		return Out<tWString>([&](auto k, void* c){ ohpl_str_utf8_to_wide(asString.data(), asString.size(), k, c); });
	}

	tString cString::S16BitToUTF8(const tWString& awsString)
	{
		return Out<tString>([&](auto k, void* c){ ohpl_str_wide_to_utf8(awsString.data(), awsString.size(), k, c); });
	}

	tWString cString::Get16BitFromArray(const tString &asArray)
	{
		return Out<tWString>([&](auto k, void* c){ ohpl_str_wide_from_int_list(asArray.data(), asArray.size(), k, c); });
	}

	//-----------------------------------------------------------------------

	tString cString::Sub(const tString& asString,int alStart,int alCount)
	{
		tString sOut;
		if(!ohpl_str_sub(asString.data(), asString.size(), alStart, alCount, Assign<tString>, &sOut))
			throw std::out_of_range("basic_string::substr");
		return sOut;
	}
	tWString cString::SubW(const tWString& asString,int alStart,int alCount)
	{
		tWString sOut;
		if(!ohpl_str_sub_w(asString.data(), asString.size(), alStart, alCount, Assign<tWString>, &sOut))
			throw std::out_of_range("basic_string::substr");
		return sOut;
	}

	//-----------------------------------------------------------------------

#define HPL_STR1(Type, Name, Fn) \
	Type cString::Name(const Type& aString) \
	{ return Out<Type>([&](auto k, void* c){ Fn(aString.data(), aString.size(), k, c); }); }

#define HPL_STR2(Type, Name, Fn) \
	Type cString::Name(const Type& aString, const Type& aOther) \
	{ return Out<Type>([&](auto k, void* c){ Fn(aString.data(), aString.size(), aOther.data(), aOther.size(), k, c); }); }

	HPL_STR1(tString, GetFileExt, ohpl_str_file_ext)
	HPL_STR1(tWString, GetFileExtW, ohpl_str_file_ext_w)
	HPL_STR1(tString, GetFileName, ohpl_str_file_name)
	HPL_STR1(tWString, GetFileNameW, ohpl_str_file_name_w)
	HPL_STR1(tString, GetFilePath, ohpl_str_file_path)
	HPL_STR1(tWString, GetFilePathW, ohpl_str_file_path_w)
	HPL_STR1(tString, RemoveSlashAtEnd, ohpl_str_remove_slash_at_end)
	HPL_STR1(tWString, RemoveSlashAtEndW, ohpl_str_remove_slash_at_end_w)
	HPL_STR1(tString, GetLastChar, ohpl_str_last_char)
	HPL_STR1(tWString, GetLastCharW, ohpl_str_last_char_w)
	HPL_STR1(tString, ToLowerCase, ohpl_str_to_lower)
	HPL_STR1(tWString, ToLowerCaseW, ohpl_str_to_lower_w)
	HPL_STR1(tString, ToUpperCase, ohpl_str_to_upper)
	HPL_STR1(tWString, ToUpperCaseW, ohpl_str_to_upper_w)

	HPL_STR2(tString, SetFileExt, ohpl_str_set_file_ext)
	HPL_STR2(tWString, SetFileExtW, ohpl_str_set_file_ext_w)
	HPL_STR2(tString, SetFilePath, ohpl_str_set_file_path)
	HPL_STR2(tWString, SetFilePathW, ohpl_str_set_file_path_w)
	HPL_STR2(tString, GetRelativePath, ohpl_str_relative_path)
	HPL_STR2(tWString, GetRelativePathW, ohpl_str_relative_path_w)

#undef HPL_STR1
#undef HPL_STR2

	tString cString::AddSlashAtEnd(const tString& asPath,char alSlash)
	{
		return Out<tString>([&](auto k, void* c){ ohpl_str_add_slash_at_end(asPath.data(), asPath.size(), alSlash, k, c); });
	}
	tWString cString::AddSlashAtEndW(const tWString& asPath,wchar_t alSlash)
	{
		return Out<tWString>([&](auto k, void* c){ ohpl_str_add_slash_at_end_w(asPath.data(), asPath.size(), alSlash, k, c); });
	}

	//-----------------------------------------------------------------------

	tString cString::ReplaceCharTo(const tString& aString, const tString& asOldChar,const tString& asNewChar)
	{
		return Out<tString>([&](auto k, void* c){
			ohpl_str_replace_char_to(aString.data(), aString.size(), asOldChar.data(), asOldChar.size(), asNewChar.data(), asNewChar.size(), k, c); });
	}
	tWString cString::ReplaceCharToW(const tWString& aString, const tWString& asOldChar,const tWString& asNewChar)
	{
		return Out<tWString>([&](auto k, void* c){
			ohpl_str_replace_char_to_w(aString.data(), aString.size(), asOldChar.data(), asOldChar.size(), asNewChar.data(), asNewChar.size(), k, c); });
	}

	tString cString::ReplaceStringTo(const tString& asString, const tString& asOldString,const tString& asNewString)
	{
		return Out<tString>([&](auto k, void* c){
			ohpl_str_replace_string_to(asString.data(), asString.size(), asOldString.data(), asOldString.size(), asNewString.data(), asNewString.size(), k, c); });
	}

	//-----------------------------------------------------------------------

	tString cString::ToString(const char* asString,tString asDefault)
	{
		if(asString==NULL)return asDefault;
		return asString;
	}

	int cString::ToInt(const char* asString,int alDefault)
	{
		if(asString==NULL)return alDefault;
		return ohpl_str_to_int(asString);
	}

	float cString::ToFloat(const char* asString,float afDefault)
	{
		if(asString==NULL)return afDefault;
		return ohpl_str_to_float(asString);
	}

	bool cString::ToBool(const char* asString, bool abDefault)
	{
		if(asString==NULL)return abDefault;
		return ohpl_str_to_bool(asString);
	}

	cColor cString::ToColor(const char* asString, const cColor& aDefault)
	{
		float v[4];
		if(asString==NULL || !ohpl_str_to_floats_exact(asString,4,v)) return aDefault;
		return cColor(v[0],v[1],v[2],v[3]);
	}

	cVector2f cString::ToVector2f(const char* asString, const cVector2f& avDefault)
	{
		float v[2];
		if(asString==NULL || !ohpl_str_to_floats_exact(asString,2,v)) return avDefault;
		return cVector2f(v[0],v[1]);
	}

	cVector3f cString::ToVector3f(const char* asString, const cVector3f& avDefault)
	{
		float v[3];
		if(asString==NULL || !ohpl_str_to_floats_exact(asString,3,v)) return avDefault;
		return cVector3f(v[0],v[1],v[2]);
	}

	cVector2l cString::ToVector2l(const char* asString, const cVector2l& avDefault)
	{
		int v[2];
		if(asString==NULL || !ohpl_str_to_ints_exact(asString,2,v)) return avDefault;
		return cVector2l(v[0],v[1]);
	}

	cVector3l cString::ToVector3l(const char* asString, const cVector3l& avDefault)
	{
		int v[3];
		if(asString==NULL || !ohpl_str_to_ints_exact(asString,3,v)) return avDefault;
		return cVector3l(v[0],v[1],v[2]);
	}

	cMatrixf cString::ToMatrixf(const char* asString, const cMatrixf& a_mtxDefault)
	{
		float v[16];
		if(asString==NULL || !ohpl_str_to_floats_exact(asString,16,v)) return a_mtxDefault;
		return cMatrixf(v[0],v[1],v[2],v[3],
						v[4],v[5],v[6],v[7],
						v[8],v[9],v[10],v[11],
						v[12],v[13],v[14],v[15]);
	}

	//-----------------------------------------------------------------------

	tIntVec& cString::GetIntVec(const tString &asData, tIntVec& avVec,tString *apSeparators)
	{
		ohpl_str_get_int_vec(asData.data(), asData.size(), SepData(apSeparators), SepSize(apSeparators),
			[](void* c, int x){ static_cast<tIntVec*>(c)->push_back(x); }, &avVec);
		return avVec;
	}

	tUIntVec& cString::GetUIntVec(const tString &asData, tUIntVec& avVec,tString *apSeparators)
	{
		ohpl_str_get_int_vec(asData.data(), asData.size(), SepData(apSeparators), SepSize(apSeparators),
			[](void* c, int x){ static_cast<tUIntVec*>(c)->push_back(x); }, &avVec);
		return avVec;
	}

	tFloatVec& cString::GetFloatVec(const tString &asData, tFloatVec& avVec,tString *apSeparators)
	{
		ohpl_str_get_float_vec(asData.data(), asData.size(), SepData(apSeparators), SepSize(apSeparators),
			[](void* c, float x){ static_cast<tFloatVec*>(c)->push_back(x); }, &avVec);
		return avVec;
	}

	tStringVec& cString::GetStringVec(const tString &asData, tStringVec& avVec,tString *apSeparators)
	{
		ohpl_str_split(asData.data(), asData.size(), SepData(apSeparators), SepSize(apSeparators), true, PushString<tString>, &avVec);
		return avVec;
	}

	tWStringVec& cString::GetStringVecW(const tWString &asData, tWStringVec& avVec,tWString *apSeparators)
	{
		ohpl_str_split_w(asData.data(), asData.size(), SepData(apSeparators), SepSize(apSeparators), true, PushString<tWString>, &avVec);
		return avVec;
	}

	tWStringVec& cString::GetStringWVec(const tWString &asData, tWStringVec& avVec,tWString *apSeparators)
	{
		ohpl_str_split_w(asData.data(), asData.size(), SepData(apSeparators), SepSize(apSeparators), false, PushString<tWString>, &avVec);
		return avVec;
	}

	//-----------------------------------------------------------------------

	tString cString::ToString(int alX, int alPaddingZeros)
	{
		return Out<tString>([&](auto k, void* c){ ohpl_str_int_to_string(alX, alPaddingZeros, k, c); });
	}
	tString cString::ToString(unsigned int alX, int alPaddingZeros)
	{
		return Out<tString>([&](auto k, void* c){ ohpl_str_uint_to_string(alX, alPaddingZeros, k, c); });
	}
	tString cString::ToString(unsigned long alX, int alPaddingZeros)
	{
		return Out<tString>([&](auto k, void* c){ ohpl_str_ulong_to_string(alX, alPaddingZeros, k, c); });
	}
	tString cString::ToString(float afX, int alNumDecimals, bool abRemoveTrailingZeros)
	{
		return Out<tString>([&](auto k, void* c){ ohpl_str_float_to_string(afX, alNumDecimals, abRemoveTrailingZeros, k, c); });
	}

	tWString cString::ToStringW(int alX, int alPaddingZeros)
	{
		return Out<tWString>([&](auto k, void* c){ ohpl_str_int_to_string_w(alX, alPaddingZeros, k, c); });
	}
	tWString cString::ToStringW(unsigned long alX, int alPaddingZeros)
	{
		return Out<tWString>([&](auto k, void* c){ ohpl_str_ulong_to_string_w(alX, alPaddingZeros, k, c); });
	}
	tWString cString::ToStringW(float afX, int alNumDecimals, bool abRemoveTrailingZeros)
	{
		return Out<tWString>([&](auto k, void* c){ ohpl_str_float_to_string_w(afX, alNumDecimals, abRemoveTrailingZeros, k, c); });
	}

	//-----------------------------------------------------------------------

	bool cString::MatchesWildcard(const tString& asPattern, const tString& asName)
	{
		return ohpl_str_matches_wildcard(asPattern.data(), asPattern.size(), asName.data(), asName.size());
	}

	int cString::GetFirstStringPos(const tString& aString, const tString&  aChar)
	{
		return ohpl_str_first_pos(aString.data(), aString.size(), aChar.data(), aChar.size());
	}
	int cString::GetFirstStringPosW(const tWString& aString, const tWString&  aChar)
	{
		return ohpl_str_first_pos_w(aString.data(), aString.size(), aChar.data(), aChar.size());
	}
	int cString::GetLastStringPos(const tString& aString, const tString&  aChar)
	{
		return ohpl_str_last_pos(aString.data(), aString.size(), aChar.data(), aChar.size());
	}
	int cString::GetLastStringPosW(const tWString& aString, const tWString&  aChar)
	{
		return ohpl_str_last_pos_w(aString.data(), aString.size(), aChar.data(), aChar.size());
	}
	int cString::CountCharsInString(const tString& aString, const tString& aChar)
	{
		return ohpl_str_count_chars(aString.data(), aString.size(), aChar.data(), aChar.size());
	}
	int cString::CountCharsInStringW(const tWString& aString, const tWString& aChar)
	{
		return ohpl_str_count_chars_w(aString.data(), aString.size(), aChar.data(), aChar.size());
	}

	unsigned int cString::GetHashW(const tWString& asStr)
	{
		return ohpl_str_hash_w(asStr.data(), asStr.size());
	}

	tString cString::GetNumericSuffix(const tString& aString, int* apIndex)
	{
		int lIndex;
		tString sOut = Out<tString>([&](auto k, void* c){ ohpl_str_numeric_suffix(aString.data(), aString.size(), &lIndex, k, c); });
		if(apIndex) *apIndex = lIndex;
		return sOut;
	}
	tWString cString::GetNumericSuffixW(const tWString& aString, int* apIndex)
	{
		int lIndex;
		tWString sOut = Out<tWString>([&](auto k, void* c){ ohpl_str_numeric_suffix_w(aString.data(), aString.size(), &lIndex, k, c); });
		if(apIndex) *apIndex = lIndex;
		return sOut;
	}

	void cString::EncodeDataToTextString(const void *apData, size_t alSize, tString &asOutput)
	{
		tString sOut;
		if(ohpl_str_encode_data(apData, alSize, Assign<tString>, &sOut))
			asOutput = sOut;
	}

	void cString::DecodeDataFromTextString(const tString &asStr, void *apDest, size_t alSize)
	{
		ohpl_str_decode_data(asStr.data(), asStr.size(), apDest, alSize);
	}

	//-----------------------------------------------------------------------
	// Left in C++: raw C-string array fillers (unbounded reads) and the narrow hash (reads out of bounds)

	void cString::UIntStringToArray(unsigned int *apArray, const char* apString,int alSize)
	{
		char vTempChar[20];
		int lTempCharCount=0;

		int lArrayCount=0;
		int lStringCount =0;

		while(lArrayCount < alSize)
		{
			char c= apString[lStringCount];

			//if a space is found, convert the previous characters to a float.
			if(c == ' ' || c == 0)
			{
				if(lTempCharCount>0)
				{
					vTempChar[lTempCharCount] =0;
					apArray[lArrayCount] = (unsigned int) atoi(vTempChar);
					
					lTempCharCount=0;
					lArrayCount++;
				}
			}
			//If character is not a space, add to temp char.
			else
			{
				vTempChar[lTempCharCount] = c;
				lTempCharCount++;
			}

			lStringCount++;
		}
	}

	//-----------------------------------------------------------------------

	void cString::FloatStringToArray(float *apArray, const char* apString,int alSize)
	{
		char vTempChar[20];
		int lTempCharCount=0;

		int lArrayCount=0;
		int lStringCount =0;

		while(lArrayCount < alSize)
		{
			char c= apString[lStringCount];
			//if a space is found, convert the previous characters to a float.
			if(c == ' ' || c == 0)
			{
				if(lTempCharCount>0)
				{
					vTempChar[lTempCharCount] =0;
					apArray[lArrayCount] = (float)atof(vTempChar);

					lTempCharCount=0;
					lArrayCount++;
				}
			}
			//If character is not a space, add to temp char.
			else
			{
				vTempChar[lTempCharCount] = apString[lStringCount];
				lTempCharCount++;
			}

			lStringCount++;
		}
	}
	
	//-----------------------------------------------------------------------

	//ALGO FROM: http://www.azillionmonkeys.com/qed/lHash.html
	// by Paul Hsieh
	
	#define Get16Bits(data) (*((const wchar_t *) (data)))

	unsigned int cString::GetHash(const tString& asStr)
	{
		unsigned int lLen = (unsigned int)asStr.size();
		const char *pData = asStr.c_str();
		unsigned int lHash = lLen, lTemp;
		int lRem;

		if (asStr.empty()) return 0;

		lRem = lLen & 3;
		lLen >>= 2;

		//////////////////////////////////
		//Main loop
		for (;lLen > 0; lLen--)
		{
			lHash  += Get16Bits(pData);
			lTemp    = ( Get16Bits(pData+2) << 11) ^ lHash;
			lHash   = (lHash << 16) ^ lTemp;
			pData  += 2*sizeof (wchar_t);
			lHash  += lHash >> 11;
		}

		//////////////////////////////////
		// Handle end cases
		switch (lRem) 
		{
		case 3: 
			lHash += Get16Bits(pData);
			lHash ^= lHash << 16;
			lHash ^= pData[sizeof (wchar_t)] << 18;
			lHash += lHash >> 11;
			break;
		case 2: 
			lHash += Get16Bits(pData);
			lHash ^= lHash << 11;
			lHash += lHash >> 17;
			break;
		case 1: 
			lHash += *pData;
			lHash ^= lHash << 10;
			lHash += lHash >> 1;
		}

		//////////////////////////////////
		// Force "avalanching" of final 127 bits
		lHash ^= lHash << 3;
		lHash += lHash >> 5;
		lHash ^= lHash << 4;
		lHash += lHash >> 17;
		lHash ^= lHash << 25;
		lHash += lHash >> 6;

		return lHash;
	}
	
	//-----------------------------------------------------------------------
	
	tString cString::GetFormatOptions(const tString& asDataType, int alZeroesOnLeft, int alNumDecimals)
	{
		tString sFormat = "%";
		if(alZeroesOnLeft)
			sFormat += "0" + cString::ToString(alZeroesOnLeft);
		if(alNumDecimals>=0)
			sFormat += "." + cString::ToString(alNumDecimals);
		sFormat += asDataType;

		return sFormat;
	}

	tWString cString::GetFormatOptionsW(const tString& asDataType, int alZeroesOnLeft, int alNumDecimals)
	{
		tString sTemp = GetFormatOptions(asDataType, alZeroesOnLeft, alNumDecimals);

		return cString::To16Char(sTemp);
	}

	//-----------------------------------------------------------------------

}
