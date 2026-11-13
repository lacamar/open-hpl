#ifndef LUX_RUST_LUX_SCRIPT_A1_H
#define LUX_RUST_LUX_SCRIPT_A1_H

// C ABI of rust/src/luxscript/a1*.rs: script-function registration and the host op table.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

struct OhplA1Arg
{
	uint64_t u;
	const void *p;
	size_t n;

	float F() const { uint32_t b = (uint32_t)u; float f; std::memcpy(&f, &b, 4); return f; }
	int I() const { return (int)(uint32_t)u; }
	bool B() const { return (u & 0xFF) != 0; }
	void SetF(float f) { uint32_t b; std::memcpy(&b, &f, 4); u = b; }
	void SetI(int i) { u = (uint32_t)i; }
	void SetB(bool b) { u = b ? 1 : 0; }
};

inline std::string OhplA1Str(const OhplA1Arg &a) { return std::string((const char *)a.p, a.n); }

enum { kOhplA1OpWarning = 0xFFFF };

typedef void (*OhplA1AddFn)(void *, const char *, const void *);
typedef void (*OhplA1HostFn)(unsigned, const OhplA1Arg *, size_t, OhplA1Arg *);

extern "C" {
	size_t ohpl_a1_register(OhplA1AddFn add, void *ctx);
	void ohpl_a1_set_host(OhplA1HostFn host);
	void OhplA1HostCall(unsigned op, const OhplA1Arg *args, size_t n, OhplA1Arg *ret);
}

#endif
