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
 * Proven (real GL compile, see HpslTranspilerSelfTest) against five real
 * SOMA .hpsl files: clear_vtx/clear_frag, null_vtx/null_frag,
 * deferred_depthonly_frag, deferred_posteffect_quad_vtx, and
 * debug_overdraw_frag. The vertex-input semantic name -> GLSL built-in
 * table below has exactly two verified entries (vtx_vPosition,
 * vtx_vColor); the rest are educated guesses by analogy with HPL2's own
 * fixed-function-attribute shaders, NOT confirmed against any other real
 * .hpsl file.
 *
 * Also handles, verified against the same five files:
 *  - HPSL's "px_vPosition" convention name (HLSL SV_Position-equivalent):
 *    a vertex shader's unnumbered "out ... px_vPosition" becomes
 *    gl_Position (not a varying - it's consumed by the fixed-function
 *    clipper, not interpolated by name), and a fragment shader's
 *    "in ... px_vPosition" becomes gl_FragCoord. Confirmed by every
 *    sample vertex/fragment shader pair examined always calling their
 *    final vertex-stage output (and mirrored fragment-stage input)
 *    exactly this name, and never assigning to it in a fragment shader.
 *    NOT verified for exact numerical equivalence (gl_FragCoord's origin
 *    convention/z-range vs whatever SOMA's original D3D-lineage pixel
 *    shader compiler produced) - only that it's the obviously-intended
 *    semantic mapping and that resulting shaders compile.
 *  - The `mul(A, B)` intrinsic (HLSL-style matrix/vector multiply,
 *    ubiquitous - 165+ call sites across all SOMA .hpsl files) rewritten
 *    to GLSL's native `(A * B)` operator syntax. Only the 2-argument form
 *    is supported; 3+ argument forms exist elsewhere in the shader corpus
 *    (unverified what they mean - possibly a different overload, or a
 *    counting artifact of nested constructor commas) and are rejected
 *    with a clear error rather than guessed at.
 *  - The `sample(texture, uv)` intrinsic (197 of 232 total call sites
 *    across the corpus use exactly this 2-argument form) rewritten to the
 *    correct GLSL 120 sampling function (texture2D/textureCube/
 *    texture2DRect) based on the referenced uniform's declared type.
 *    `sampleLod`/`sampleBias`/`sampleGrad`/`sampleCmp` (distinct
 *    identifiers, not matched by this) and 3+ argument `sample(...)`
 *    calls are NOT supported - unexplored, no verified GLSL equivalent
 *    chosen yet.
 *  - `uniform cTextureX aName : N;`'s trailing `: N` texture-unit-binding
 *    suffix (D3D-style register binding, meaningless to GLSL 120) is
 *    stripped from uniform declarations.
 *
 * Still no cTextureBuffer/constant-buffer support at all - unexplored,
 * and known to block every real deferred-rendering shader (see
 * deferred_base_vtx.hpsl's `@include helper_type_arguments.hpsl` and
 * `uniform cTextureBuffer aInstanceBuffer : 15;` - a materially bigger
 * undertaking, not attempted this pass; see PORTING_NOTES.md).
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
