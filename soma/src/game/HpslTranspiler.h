#ifndef SOMA_HPSL_TRANSPILER_H
#define SOMA_HPSL_TRANSPILER_H

#include "hpl.h"

using namespace hpl;

extern "C" {
	struct OhplBytes { uint8_t *ptr; size_t len; };
	bool ohpl_hpsl_transpile(const uint8_t *apSrc, size_t alLen, int alShaderType, OhplBytes *apOut);
	void ohpl_bytes_free(OhplBytes aBytes);
}

inline bool TranspileHpslToGlsl(const tString& asPreprocessedHpsl, eGpuShaderType aType,
								tString& asGlslOut, tString& asErrorOut)
{
	OhplBytes out;
	bool bOk = ohpl_hpsl_transpile((const uint8_t*)asPreprocessedHpsl.data(), asPreprocessedHpsl.size(), (int)aType, &out);
	(bOk ? asGlslOut : asErrorOut).assign((const char*)out.ptr, out.len);
	ohpl_bytes_free(out);
	return bOk;
}

#endif // SOMA_HPSL_TRANSPILER_H
