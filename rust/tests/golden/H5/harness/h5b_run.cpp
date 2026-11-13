// H5b harness: drives the seeded Gfx scenario (rust/src/graphics/h5b_scn.rs) through the C++ facade into
// cRecordingLowLevelGraphics and writes the C++ trace and the scenario's result log.
// usage: H5B_SEED=<n> H5B_ROUNDS=<n> H5B_OUT=<prefix> h5b_run   -> <prefix>.trace, <prefix>.log
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "graphics/RustGraphicsH5b.h"
#include "resources/GpuShaderManager.h"
#include "system/PreprocessParser.h"
#include "trace/RecordingLowLevelGraphics.h"

using namespace hpl;

extern "C" uint64_t H5bCall(void *ctx, uint32_t op, const uint64_t *a, size_t n, uint64_t *out)
{
	return cRustGfxFacade::Call(ctx, op, a, n, out);
}

static bool Write(const std::string &asPath, const std::string &asText)
{
	FILE *f = fopen(asPath.c_str(), "wb");
	if(f == NULL) return false;
	size_t n = fwrite(asText.data(), 1, asText.size(), f);
	fclose(f);
	return n == asText.size();
}

// Link-time hook of the shared build wrapper; unused here.
extern "C" iGpuShader* __wrap__ZN3hpl17cGpuShaderManager12CreateShaderERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS_14eGpuShaderTypeEPNS_19cParserVarContainerE(
	void*, const tString&, eGpuShaderType, cParserVarContainer*)
{
	return NULL;
}

int hplMain(const tString&)
{
	const char *sSeed = getenv("H5B_SEED"), *sRounds = getenv("H5B_ROUNDS"), *sOut = getenv("H5B_OUT");
	if(!sSeed || !sRounds || !sOut) { fprintf(stderr, "set H5B_SEED, H5B_ROUNDS, H5B_OUT\n"); return 2; }
	unsigned long long seed = strtoull(sSeed, NULL, 10);
	unsigned rounds = (unsigned)atoi(sRounds);
	std::string prefix = sOut;

	cRecordingLowLevelGraphics llg;
	cRustGfxFacade facade(&llg);
	std::vector<char> log(1 << 20);
	size_t n = ohpl_h5b_run_foreign(&facade, &H5bCall, seed, rounds, log.data(), log.size());
	if(n > log.size()) { fprintf(stderr, "log too large (%zu)\n", n); return 1; }
	if(!Write(prefix + ".log", std::string(log.data(), n))) return 1;
	if(!llg.Trace().WriteFile((prefix + ".trace").c_str())) return 1;
	return 0;
}
