#ifndef SOMA_IMGUI_DEFAULTS_H
#define SOMA_IMGUI_DEFAULTS_H

#include <vector>

struct cSomaStructDefaults
{
	const char *mpType;
	int mlSize;
	const char *mpBytes; // offsets 16..mlSize
	std::vector<int> mvStringOffsets;
};

extern const cSomaStructDefaults gvSomaStructDefaults[];
extern const int glSomaStructDefaultsNum;

#endif // SOMA_IMGUI_DEFAULTS_H
