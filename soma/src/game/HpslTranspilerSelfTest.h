/*
 * One-shot startup self-test for HpslTranspiler.cpp - see that file's
 * header comment for scope. Loads SOMA's real clear_vtx.hpsl/clear_frag.hpsl
 * via the engine's resource file searcher, transpiles them, and attempts a
 * real GLSL compile against the live GL context, logging PASS/FAIL (with
 * the GLSL compiler's own error text on failure, via iGpuShader's existing
 * info-log printing) through the engine's normal Log()/Error() so the
 * result shows up in hpl.log like any other engine diagnostic.
 *
 * Deliberately NOT wired into the material/resource system - this only
 * proves "does the transpiled output compile", not "does SOMA render with
 * it". See PORTING_NOTES.md for the next step.
 */

#ifndef SOMA_HPSL_TRANSPILER_SELF_TEST_H
#define SOMA_HPSL_TRANSPILER_SELF_TEST_H

#include "hpl.h"

using namespace hpl;

void RunHpslTranspilerSelfTest(cEngine *apEngine);

#endif // SOMA_HPSL_TRANSPILER_SELF_TEST_H
