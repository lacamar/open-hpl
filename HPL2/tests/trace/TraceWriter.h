#ifndef HPL_TRACE_WRITER_H
#define HPL_TRACE_WRITER_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "graphics/Color.h"
#include "graphics/GraphicsTypes.h"
#include "math/MathTypes.h"
#include "system/SystemTypes.h"

namespace hpl {

	// Deterministic text trace. Floats as hex bits, objects as per-kind creation-order labels
	// ("tex#3"), blobs as size + FNV-1a hash. Raw pointers never reach the text.
	class cTraceWriter
	{
	public:
		static uint64_t Hash(const void* apData, size_t alSize)
		{
			const unsigned char* p = static_cast<const unsigned char*>(apData);
			uint64_t h = 1469598103934665603ull;
			for(size_t i = 0; i < alSize; ++i) { h ^= p[i]; h *= 1099511628211ull; }
			return h;
		}

		std::string NewLabel(const void* apObj, const char* asKind)
		{
			std::string s = std::string(asKind) + "#" + std::to_string(mCounters[asKind]++);
			mLabels[apObj] = s;
			return s;
		}

		void Forget(const void* apObj) { mLabels.erase(apObj); }

		std::string Label(const void* apObj)
		{
			if(apObj == NULL) return "null";
			auto it = mLabels.find(apObj);
			if(it != mLabels.end()) return it->second;
			return NewLabel(apObj, "ptr");
		}

		template<class... A>
		void Call(const std::string& asFn, const A&... a)
		{
			mText += asFn;
			mText += '(';
			bool first = true;
			int dummy[] = {0, (Sep(first), Put(a), 0)...};
			(void)dummy;
			mText += ")\n";
		}

		template<class R, class... A>
		void CallR(const std::string& asFn, const R& aRet, const A&... a)
		{
			mText += asFn;
			mText += '(';
			bool first = true;
			int dummy[] = {0, (Sep(first), Put(a), 0)...};
			(void)dummy;
			mText += ") -> ";
			Put(aRet);
			mText += '\n';
		}

		const std::string& Text() const { return mText; }
		void Clear() { mText.clear(); }

		bool WriteFile(const char* asPath) const
		{
			FILE* f = fopen(asPath, "wb");
			if(f == NULL) return false;
			size_t n = fwrite(mText.data(), 1, mText.size(), f);
			fclose(f);
			return n == mText.size();
		}

		static std::string FloatBits(float afX)
		{
			uint32_t u;
			memcpy(&u, &afX, 4);
			char b[16];
			snprintf(b, sizeof(b), "f:%08x", u);
			return b;
		}

		void PutHash(const char* asTag, const void* apData, size_t alSize)
		{
			char b[64];
			snprintf(b, sizeof(b), "%s{n=%zu,h=%016llx}", asTag, alSize, (unsigned long long)Hash(apData, alSize));
			mText += b;
		}

		void Put(bool abX) { mText += abX ? "true" : "false"; }
		void Put(int alX) { mText += std::to_string(alX); }
		void Put(unsigned int alX) { mText += std::to_string(alX) + "u"; }
		void Put(long alX) { mText += std::to_string(alX) + "l"; }
		void Put(unsigned long alX) { mText += std::to_string(alX) + "ul"; }
		void Put(unsigned long long alX) { mText += std::to_string(alX) + "ull"; }
		void Put(float afX) { mText += FloatBits(afX); }
		void Put(const char* apStr)
		{
			if(apStr == NULL) { mText += "null"; return; }
			PutHash("cstr", apStr, strlen(apStr));
		}
		void Put(const std::string& asStr) { mText += '"'; mText += asStr; mText += '"'; }
		void Put(const std::wstring& asStr)
		{
			mText += "w\"";
			for(wchar_t c : asStr)
				mText += (c >= 32 && c < 127) ? std::string(1, (char)c) : "\\u" + std::to_string((unsigned)c);
			mText += '"';
		}
		void Put(const cVector2f& v) { mText += "v2{"; Put(v.x); mText += ','; Put(v.y); mText += '}'; }
		void Put(const cVector2l& v) { mText += "v2i{"; Put(v.x); mText += ','; Put(v.y); mText += '}'; }
		void Put(const cVector3f& v) { mText += "v3{"; Put(v.x); mText += ','; Put(v.y); mText += ','; Put(v.z); mText += '}'; }
		void Put(const cVector3l& v) { mText += "v3i{"; Put(v.x); mText += ','; Put(v.y); mText += ','; Put(v.z); mText += '}'; }
		void Put(const cColor& c)
		{
			mText += "c{"; Put(c.r); mText += ','; Put(c.g); mText += ','; Put(c.b); mText += ','; Put(c.a); mText += '}';
		}
		void Put(const cPlanef& p)
		{
			mText += "pl{"; Put(p.a); mText += ','; Put(p.b); mText += ','; Put(p.c); mText += ','; Put(p.d); mText += '}';
		}
		void Put(const cRect2f& r)
		{
			mText += "r{"; Put(r.x); mText += ','; Put(r.y); mText += ','; Put(r.w); mText += ','; Put(r.h); mText += '}';
		}
		void Put(const cMatrixf& m)
		{
			mText += "m{";
			for(int i = 0; i < 16; ++i) { if(i) mText += ','; Put(m.v[i]); }
			mText += '}';
		}
		void Put(const cVertex& v)
		{
			mText += "vtx{"; Put(v.pos); mText += ','; Put(v.tex); mText += ','; Put(v.tan); mText += ','; Put(v.norm);
			mText += ','; Put(v.col); mText += '}';
		}
		void Put(const tVertexVec& v)
		{
			mText += "vtxvec[" + std::to_string(v.size()) + "]{";
			for(size_t i = 0; i < v.size(); ++i) { if(i) mText += ','; Put(v[i]); }
			mText += '}';
		}
		void Put(const tVector3fVec& v)
		{
			mText += "v3vec[" + std::to_string(v.size()) + "]{";
			for(size_t i = 0; i < v.size(); ++i) { if(i) mText += ','; Put(v[i]); }
			mText += '}';
		}
		void Put(const std::vector<int>& v)
		{
			mText += "ints[" + std::to_string(v.size()) + "]{";
			for(size_t i = 0; i < v.size(); ++i) { if(i) mText += ','; Put(v[i]); }
			mText += '}';
		}

		template<class T>
		typename std::enable_if<std::is_enum<T>::value>::type Put(T aEnum) { Put((int)aEnum); }

		template<class T>
		typename std::enable_if<std::is_pointer<T>::value && !std::is_same<T, const char*>::value &&
								!std::is_same<T, char*>::value>::type Put(T apObj)
		{
			mText += Label(static_cast<const void*>(apObj));
		}

	private:
		void Sep(bool& abFirst) { if(!abFirst) mText += ", "; abFirst = false; }

		std::string mText;
		std::map<const void*, std::string> mLabels;
		std::map<std::string, int> mCounters;
	};

}

#endif
