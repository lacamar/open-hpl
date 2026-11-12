#pragma once
#include <cstdio>
#include <cstdint>
#include <string>

inline uint64_t Fnv(const void *p, size_t n, uint64_t h = 1469598103934665603ull)
{
	const unsigned char *b = static_cast<const unsigned char*>(p);
	for(size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
	return h;
}
inline uint64_t Fnv(const std::string &s, uint64_t h = 1469598103934665603ull) { return Fnv(s.data(), s.size(), h); }
inline std::string Hex(uint64_t v) { char b[20]; snprintf(b, sizeof b, "%016llx", (unsigned long long)v); return b; }
