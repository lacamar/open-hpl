/*
 * One-shot startup self-test for HpslTranspiler.cpp - see that file's
 * header comment for scope. Loads a fixed list of real SOMA .hpsl files
 * via the engine's resource file searcher, transpiles them, and attempts a
 * real GLSL compile against the live GL context, logging PASS/FAIL (with
 * the GLSL compiler's own error text on failure, via iGpuShader's existing
 * info-log printing) through the engine's normal Log()/Error() so the
 * result shows up in hpl.log like any other engine diagnostic.
 *
 * Covers nine real files as of this pass: clear_vtx/clear_frag (the
 * original proof-of-concept pair), null_vtx/null_frag, three
 * fragment/vertex shaders with no real pair partner needed to prove
 * compilation (deferred_depthonly_frag, deferred_posteffect_quad_vtx,
 * debug_overdraw_frag) - each exercises real syntax the clear pair didn't
 * (mul(), sample(), a texture uniform with a binding-index suffix, and
 * the px_vPosition->gl_Position/gl_FragCoord convention) - and, new this
 * pass, deferred_base_vtx.hpsl/deferred_base_frag.hpsl (a real material
 * shader pair, compiled with a deliberately minimal combo of defines - see
 * RunHpslTranspilerSelfTest()'s vMinimalMaterialCombo) exercising cBuffer
 * constant-buffer flattening and the custom-attribute fallback for vertex
 * inputs with no fixed-function GLSL 120 built-in (tangent/bone data).
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
