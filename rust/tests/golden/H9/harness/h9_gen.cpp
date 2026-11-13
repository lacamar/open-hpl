// H9 differential harness: OHPL_IMPL=cpp|rust picks the setup path of cRendererDeferred::LoadData's fog and light program combos.
//   h9_gen   prints the cProgramComboManager state (features, default defines, variable ids) per configuration.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#define private public
#define protected public
#include "graphics/ProgramComboManager.h"
#include "graphics/RustRendererH9.h"
#include "impl/RustBackends.h"
#include "system/PreprocessParser.h"
#undef private
#undef protected

using namespace hpl;

int hplMain(const std::string&) { return 0; }

static void Dump(cProgramComboManager &m, int mode)
{
	cProgramComboSettings &s = m.mvSettings[mode];
	printf("combos %d\n", m.mvCombinationNum[mode]);
	printf("mode %s vtx %s frag %s\n", s.msName.c_str(), s.msVtxShader.c_str(), s.msFragShader.c_str());
	for(size_t i = 0; i < s.mvFeatures.size(); ++i)
		printf("f %s %u %u\n", s.mvFeatures[i].msVariable.c_str(), (unsigned)s.mvFeatures[i].mlShaders, (unsigned)s.mvFeatures[i].mlNeededFlags);
	for(size_t i = 0; i < s.mvDefaultVars.size(); ++i)
		printf("d %s=%s\n", s.mvDefaultVars[i].msName.c_str(), s.mvDefaultVars[i].msValue.c_str());
	for(tProgramComboVariableList::iterator it = m.mvVarLists[mode].begin(); it != m.mvVarLists[mode].end(); ++it)
		printf("v %s %d\n", it->msVarName.c_str(), it->mlId);
}

int main()
{
	bool bRust = OhplBackendRustRequested();
	fprintf(stderr, "rs %d\n", bRust ? 1 : 0);
	for(int packed = 0; packed < 2; ++packed)
	{
		cProgramComboManager m("FogArea", NULL, NULL, 1);
		OhplH9SetupFogCombos(&m, packed != 0, bRust);
		printf("fog packed %d\n", packed);
		Dump(m, 0);
	}
	static const int vSizes[] = {0, 1, 2, 4, 8, 16, 64};
	static const int vSamples[] = {0, 1, 7, 8, 16, 33};
	for(int q = 0; q < 3; ++q) for(int g64 = 0; g64 < 2; ++g64) for(int rt = 3; rt <= 4; ++rt)
		for(size_t a = 0; a < sizeof(vSizes) / sizeof(vSizes[0]); ++a) for(size_t b = 0; b < sizeof(vSamples) / sizeof(vSamples[0]); ++b)
		{
			cProgramComboManager m("Deferred", NULL, NULL, 2);
			OhplH9SetupLightCombos(&m, (eShadowMapQuality)q, g64 != 0, rt, vSizes[a], vSamples[b], bRust);
			printf("light q %d g64 %d rt %d jitter %d %d\n", q, g64, rt, vSizes[a], vSamples[b]);
			Dump(m, 0);
		}
	fflush(stdout);
	_exit(0);
}
