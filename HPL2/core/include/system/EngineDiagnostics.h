/*
 * Text diagnostics backing the headless control commands
 * (shader_report, render_stats, world_stats, frame_stats, ...).
 * Everything returns pre-encoded JSON for cHeadlessResponse::SetRaw().
 */

#ifndef HPL_ENGINE_DIAGNOSTICS_H
#define HPL_ENGINE_DIAGNOSTICS_H

#include "system/SystemTypes.h"
#include "math/MathTypes.h"

namespace hpl {

	class cWorld;
	class cViewport;
	class cGraphics;
	class iTexture;
	class cRenderList;

	class cEngineDiagnostics
	{
	public:
		static tString JsonEscape(const tString &asIn);

		// Called from cGLSLShader::CreateFromString() / cGLSLProgram::Link().
		static void ReportShader(const tString &asName, const char *apStage, bool abOk, const tString &asInfoLog);
		static tString GetShaderReportJson(bool abFailedOnly);
		static int GetShaderFailCount() { return mlShaderFailCount; }

		static void CountDrawCall() { ++mlDrawCalls; }
		// Called by cEngine after each rendered frame, latches the running counters.
		static void EndFrame();
		static unsigned int GetRenderedFrameCount() { return mlRenderedFrames; }
		static int GetLastFrameDrawCalls() { return mlLastFrameDrawCalls; }

		// Drains glGetError() into a running per-code tally.
		static tString PollGLErrorsJson(bool abReset);

		static tString GetWorldStatsJson(cWorld *apWorld);
		static tString GetRenderStatsJson(cViewport *apViewport, cGraphics *apGraphics);
		// The alMax lights nearest to avPos.
		static tString GetLightsJson(cWorld *apWorld, const cVector3f &avPos, int alMax, cRenderList *apRenderList=NULL);
		static tString GetEntityInfoJson(cWorld *apWorld, const tString &asName);

		// Per-channel min/max/mean + NaN/zero counts of RGBA float pixels.
		static tString GetPixelStatsJson(const float *apPixels, int alWidth, int alHeight);
		// Luminance histogram + black/white/magenta fractions of 8-bit RGBA.
		static tString GetFrameStatsJson(const unsigned char *apPixels, int alWidth, int alHeight, int alBytesPerPixel);

	private:
		static int mlDrawCalls;
		static int mlLastFrameDrawCalls;
		static int mlShaderFailCount;
		static unsigned int mlRenderedFrames;
	};

}
#endif // HPL_ENGINE_DIAGNOSTICS_H
