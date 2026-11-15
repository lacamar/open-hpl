#ifndef SOMA_S8_TRACE_H
#define SOMA_S8_TRACE_H

#include "hpl.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <vector>

// OHPL_S8_DUMP=<file>: text log of the splash and gamma screen engine calls, fixed 1/60 step,
// deterministic clock, forced Return on the gamma screen after OHPL_S8_AUTOKEY updates.
// Line: `op args [= results]`; pointers are numbered by first appearance.
namespace s8
{
	struct A
	{
		char k;
		unsigned long long u;
		std::string s;
		A(const void *p) : k('p'), u((unsigned long long)p) {}
		A(int v) : k('i'), u((unsigned long long)(long long)v) {}
		A(unsigned long v) : k('i'), u(v) {}
		A(bool v) : k('b'), u(v) {}
		A(float v) : k('f') { unsigned b; memcpy(&b, &v, 4); u = b; }
		A(const char *v) : k('s'), u(0), s(v) {}
		A(const hpl::tString &v) : k('s'), u(0), s(v) {}
		A(const hpl::tWString &v) : k('s'), u(0) { for (wchar_t c : v) s += c < 0x80 ? (char)c : '?'; }
	};

	inline FILE *File()
	{
		static FILE *f = getenv("OHPL_S8_DUMP") ? fopen(getenv("OHPL_S8_DUMP"), "w") : NULL;
		return f;
	}
	inline bool On() { return File() != NULL; }

	inline unsigned long Now(unsigned long alReal)
	{
		static unsigned long t = 0;
		return On() ? (t += 16) : alReal;
	}

	inline void Put(FILE *f, const A &a)
	{
		static std::map<unsigned long long, int> ids;
		switch (a.k)
		{
		case 'p':
			if (a.u == 0)
				fputs(" p0", f);
			else
				fprintf(f, " p%d", ids.emplace(a.u, (int)ids.size() + 1).first->second);
			break;
		case 's': fprintf(f, " \"%s\"", a.s.c_str()); break;
		case 'f': fprintf(f, " %08llx", a.u); break;
		case 'i': fprintf(f, " %lld", (long long)a.u); break;
		default: fprintf(f, " %llu", a.u); break;
		}
	}

	inline void Tr(const char *asOp, std::initializer_list<A> avArgs, const std::vector<A> &avRes = {})
	{
		FILE *f = File();
		if (f == NULL)
			return;
		fputs(asOp, f);
		for (const A &a : avArgs)
			Put(f, a);
		if (avRes.size())
			fputs(" =", f);
		for (const A &a : avRes)
			Put(f, a);
		fputc('\n', f);
		fflush(f);
	}

	// Traced runs do one Update, then one OnDraw per class, whatever the engine loop does
	inline bool Pass(int alClass, bool abUpdate, bool abBypass = false)
	{
		static bool vNextUpdate[2] = {true, true};
		if (On() == false || abBypass)
			return true;
		if (vNextUpdate[alClass] != abUpdate)
			return false;
		vNextUpdate[alClass] = !abUpdate;
		return true;
	}
	inline bool &Bypass()
	{
		static bool b = false;
		return b;
	}

	inline bool AutoKey()
	{
		static int n = 0;
		static int lim = getenv("OHPL_S8_AUTOKEY") ? atoi(getenv("OHPL_S8_AUTOKEY")) : -1;
		return lim >= 0 && ++n > lim;
	}
}

#endif
