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
 * Constant buffers (`cBuffer NAME [: N] { members... };`, HPSL's HLSL-
 * derived syntax - see helper_type_arguments.hpsl and deferred_base_vtx.hpsl's
 * legacy `cBuffer cVertexArguments` block) ARE now supported: each block is
 * flattened into a sequence of plain top-level `uniform TYPE NAME;`
 * declarations (dropping the `cBuffer NAME [: N] { ... };` wrapper), since
 * GLSL 120 has no named-uniform-block syntax at all (`layout(std140) uniform
 * Name {...}` is a GLSL 140+ feature) and HPSL's own cBuffer members are
 * always referenced unqualified anyway - see FlattenConstantBuffers() in
 * HpslTranspiler.cpp for the full reasoning, and PORTING_NOTES.md's "SOMA"
 * section for how this was verified against real files.
 *
 * Still NOT supported: `cTextureBuffer` (GPU-instancing via a texture-buffer
 * object - `uniform cTextureBuffer aInstanceBuffer : 15;` in
 * deferred_base_vtx.hpsl) - rejected with a clear "no known GLSL built-in
 * mapping" error via the same unmapped-type path as any other unrecognised
 * type, rather than guessed at. GLSL 120 has no equivalent type
 * (`samplerBuffer` needs GLSL 140+/`GL_EXT_texture_buffer_object`).
 * PORTING_NOTES.md documents why this is not expected to actually block real
 * rendering: every real material shader that uses `cTextureBuffer` gates it
 * (along with the whole `helper_type_arguments.hpsl` MaterialType-buffer
 * indirection and GPU instancing) behind `@ifdef UseTextureBuffer`, with a
 * simpler "TEMP BACKWARD COMPATIBILITY" `@else` branch that avoids it
 * entirely (a single flat `cBuffer`, no MaterialType branching, and either
 * no instance buffer at all or a plain `cTexture2D` one gated behind its own
 * separate `UseMeshInstancing`/`UseStaticMeshInstancing` flags) - so the
 * chosen strategy is for whichever engine code selects HPSL combo-variables
 * to simply never define `UseTextureBuffer`/`UseMeshInstancing`/
 * `UseStaticMeshInstancing`, steering every material onto that already-
 * transpilable legacy branch and accepting no GPU instancing (one uniform
 * set per draw call, same as this engine's own hand-written GLSL shaders
 * already do) rather than attempting a real texture-buffer/instancing port.
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
