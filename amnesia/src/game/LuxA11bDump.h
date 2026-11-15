#ifndef LUX_A11B_DUMP_H
#define LUX_A11B_DUMP_H

#include "LuxBase.h"

bool OhplA11Enabled();
void OhplA11Log(const char *fmt, ...);
void OhplA11LogFont(const cVector3f &avPos, const cVector2f &avSize, const cColor &aCol, int alAlign, int alMat, const tWString &asText);
void OhplA11LogGfx(const cVector3f &avPos, const cVector2f &avSize, const cColor &aCol, int alMat);
void OhplA11Tick();
bool OhplA11Answers();
void OhplA11Ans(const char *op, const std::string &asIn, const std::string &asOut);
std::string OhplA11Esc(const unsigned *apText, size_t alLen);
std::string OhplA11Esc(const std::string &asText);
std::string OhplA11Num(float afX);
inline float OhplA11Dt() { return 1.0f / 60.0f; }

#endif
