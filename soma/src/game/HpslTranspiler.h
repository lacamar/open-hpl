#ifndef SOMA_HPSL_TRANSPILER_H
#define SOMA_HPSL_TRANSPILER_H

#include "hpl.h"

using namespace hpl;

bool TranspileHpslToGlsl(const tString& asPreprocessedHpsl, eGpuShaderType aType,
						  tString& asGlslOut, tString& asErrorOut);

#endif // SOMA_HPSL_TRANSPILER_H
