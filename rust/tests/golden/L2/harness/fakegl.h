#ifndef L2_FAKEGL_H
#define L2_FAKEGL_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>

struct FakeEntry { const char *name; void *fn; };
extern const FakeEntry gFakeGlTable[];

std::string A(unsigned);
std::string A(int);
std::string A(unsigned char);
std::string A(long);
std::string A(float);
std::string A(double);
std::string A(const void *);

void Rec(const char *name, std::initializer_list<std::string> args, const std::string &extra);
const char *FakeExtensions();
int FakeExtCount();
uint64_t Fnv(const char *);
void AddRange(const void *base, size_t bytes, const char *label);
void ClearRanges();

extern int gGen, gShaders, gPrograms;
extern std::string gTrace;
extern int gWinW, gWinH;
extern int gGlErrors, gFailCompile, gFailLink, gFbStatus;

#endif
