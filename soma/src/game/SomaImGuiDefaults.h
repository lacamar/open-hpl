/*
 * Default field values of script value structs (cImGui*Data...), recovered from the official
 * binary's default factories by scripts/soma-re-struct-defaults.py. String members are slots
 * holding an interned tString pointer.
 */

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
