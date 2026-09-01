/*
 * Best-effort HPSL -> GLSL 120 syntax transpiler.
 *
 * Scope note (see PORTING_NOTES.md "SOMA" section for the full writeup):
 * HPL2's own GLSL shaders already use the exact same @ifdef/@else/@endif/
 * @include/@define directive set that HPSL files use (see
 * HPL2/core/include/system/PreprocessParser.h / cPreprocessParser) - that
 * machinery is NOT reimplemented here. Feed the *already-preprocessed*
 * HPSL source (i.e. run it through cPreprocessParser::Parse() first, same
 * as HPL2's own shaders) into TranspileHpslToGlsl(); this only rewrites
 * what's left: HPSL's cVectorNf/cTextureX type names and its
 * main()-parameter-based I/O convention into GLSL 120's
 * attribute/varying/gl_FragData global convention (matching the style
 * HPL2's own hand-written GLSL shaders already use - see e.g.
 * core/shaders/deferred_base_vtx.glsl in a real Dark Descent/SOMA
 * install).
 *
 * Only proven against SOMA's clear_vtx.hpsl/clear_frag.hpsl pair (the
 * smallest HPSL shaders - no @ifdef, no constant buffers). The
 * vertex-input semantic name -> GLSL built-in table below has exactly two
 * verified entries (vtx_vPosition, vtx_vColor); the rest are educated
 * guesses by analogy with HPL2's own fixed-function-attribute shaders,
 * NOT confirmed against any other real .hpsl file. No cTextureBuffer or
 * constant-buffer support at all - unexplored.
 */

#ifndef SOMA_HPSL_TRANSPILER_H
#define SOMA_HPSL_TRANSPILER_H

#include "hpl.h"

using namespace hpl;

/**
 * \param asPreprocessedHpsl HPSL source, already run through cPreprocessParser::Parse().
 * \param aType eGpuShaderType_Vertex or eGpuShaderType_Fragment.
 * \param asGlslOut receives the transpiled GLSL 120 source on success.
 * \param asErrorOut receives a human-readable reason on failure.
 * \return false if the source uses a construct this best-effort transpiler doesn't understand.
 */
bool TranspileHpslToGlsl(const tString& asPreprocessedHpsl, eGpuShaderType aType,
						  tString& asGlslOut, tString& asErrorOut);

#endif // SOMA_HPSL_TRANSPILER_H
